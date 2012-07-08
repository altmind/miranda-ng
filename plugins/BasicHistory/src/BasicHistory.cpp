/*
Basic History plugin
Copyright (C) 2011-2012 Krzysztof Kral

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "version.h"
#include "HistoryWindow.h"
#include "resource.h"
#include "Options.h"

// {E25367A2-51AE-4044-BE28-131BC18B71A4}
#define	MIID_BASICHISTORY { 0xe25367a2, 0x51ae, 0x4044, { 0xbe, 0x28, 0x13, 0x1b, 0xc1, 0x8b, 0x71, 0xa4 } }

#define MS_HISTORY_DELETEALLCONTACTHISTORY       "BasicHistory/DeleteAllContactHistory"
#define MS_HISTORY_EXECUTE_TASK       "BasicHistory/ExecuteTask"

HCURSOR     hCurSplitNS, hCurSplitWE;
HANDLE  g_hMainThread=NULL;

extern HINSTANCE hInst;

HANDLE hServiceShowContactHistory, hServiceDeleteAllContactHistory, hServiceExecuteTask;
HANDLE *hEventIcons = NULL;
int iconsNum;
HANDLE hPlusIcon, hMinusIcon, hFindNextIcon, hFindPrevIcon;
HANDLE hPlusExIcon, hMinusExIcon;
HANDLE hToolbarButton;
HGENMENU hContactMenu, hDeleteContactMenu;
HGENMENU hTaskMainMenu;
std::vector<HGENMENU> taskMenus;
bool g_SmileyAddAvail = false;
char* metaContactProto = NULL;
const IID IID_ITextDocument={0x8CC497C0, 0xA1DF, 0x11ce, {0x80, 0x98, 0x00, 0xAA, 0x00, 0x47, 0xBE, 0x5D}};

#define MODULE "BasicHistory"

PLUGININFOEX pluginInfo = 
{
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__AUTHOREMAIL,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	MIID_BASICHISTORY
};

TIME_API tmi = {0};
int hLangpack = 0;

extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD mirandaVersion)
{
	return &pluginInfo;
}

static const MUUID interfaces[] = {MIID_UIHISTORY, MIID_LAST};
extern "C" __declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}

void InitScheduler();
void DeinitScheduler();
int DoLastTask(WPARAM, LPARAM);
INT_PTR ExecuteTaskService(WPARAM wParam, LPARAM lParam);

int PrebuildContactMenu(WPARAM wParam, LPARAM lParam)
{
	int count = EventList::GetContactMessageNumber((HANDLE)wParam);
	bool isInList = HistoryWindow::IsInList(GetForegroundWindow());
	CLISTMENUITEM mi = {0};
	mi.cbSize = sizeof(mi);
	mi.flags = CMIM_FLAGS;

	if (!count) mi.flags |= CMIF_HIDDEN;
	else mi.flags &= ~CMIF_HIDDEN;
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hContactMenu, (LPARAM)&mi);

	mi.flags = CMIM_FLAGS;
	if (!count || !isInList) mi.flags |= CMIF_HIDDEN;
	else mi.flags &= ~CMIF_HIDDEN;
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hDeleteContactMenu, (LPARAM)&mi);

	return 0;
}

int ToolbarModuleLoaded(WPARAM wParam,LPARAM lParam)
{
	if(ServiceExists(MS_TTB_REMOVEBUTTON)) {
		TTBButton tbb = {0};
		tbb.cbSize = sizeof(tbb);
		tbb.name = LPGEN("Open History");
		tbb.pszService = MS_HISTORY_SHOWCONTACTHISTORY;
		tbb.pszTooltipUp = LPGEN("Open History");
		tbb.pszTooltipDn = LPGEN("Open History");
		tbb.dwFlags = TTBBF_SHOWTOOLTIP;
		tbb.hIconHandleUp = tbb.hIconHandleDn = LoadSkinnedIconHandle(SKINICON_OTHER_HISTORY);
		hToolbarButton = TopToolbar_AddButton(&tbb);
	}
	return 0;
}

void InitMenuItems()
{
	CLISTMENUITEM mi = { 0 };
	mi.cbSize = sizeof(mi);
	mi.position = 1000090000;
	mi.flags = CMIF_ICONFROMICOLIB;
	mi.icolibItem = LoadSkinnedIconHandle(SKINICON_OTHER_HISTORY);
	mi.pszName = LPGEN("View &History");
	mi.pszService = MS_HISTORY_SHOWCONTACTHISTORY;
	hContactMenu = Menu_AddContactMenuItem(&mi);

	mi.position = 500060000;
	mi.pszService = MS_HISTORY_SHOWCONTACTHISTORY;
	Menu_AddMainMenuItem(&mi);

	mi.position = 1000090001;
	mi.flags = CMIF_ICONFROMICOLIB;
	mi.icolibItem = LoadSkinnedIconHandle(SKINICON_OTHER_DELETE);
	mi.pszName = LPGEN("Delete All User History");
	mi.pszService = MS_HISTORY_DELETEALLCONTACTHISTORY;
	hDeleteContactMenu = Menu_AddContactMenuItem(&mi);

	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, PrebuildContactMenu);
}

void InitTaskMenuItems()
{
	if(Options::instance->taskOptions.size() > 0)
	{
		CLISTMENUITEM mi = { 0 };
		mi.cbSize = sizeof(mi);
		if(hTaskMainMenu == NULL)
		{
			mi.position = 500060005;
			mi.flags = CMIF_ROOTPOPUP | CMIF_ICONFROMICOLIB;
			mi.icolibItem = LoadSkinnedIconHandle(SKINICON_OTHER_HISTORY);
			mi.pszName = LPGEN("Execute history task");
			hTaskMainMenu = Menu_AddMainMenuItem(&mi);
		}

		std::vector<TaskOptions>::iterator taskIt = Options::instance->taskOptions.begin();
		std::vector<HGENMENU>::iterator it = taskMenus.begin();
		for(; it != taskMenus.end() && taskIt != Options::instance->taskOptions.end(); ++it, ++taskIt)
		{
			memset(&mi, 0, sizeof(mi));
			mi.cbSize = sizeof(mi);
			mi.flags = CMIM_FLAGS | CMIM_NAME | CMIF_CHILDPOPUP | CMIF_ROOTHANDLE | CMIF_TCHAR | CMIF_KEEPUNTRANSLATED;
			mi.hParentMenu = hTaskMainMenu;
			mi.ptszName = (TCHAR*)taskIt->taskName.c_str();
			CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)(HGENMENU)*it, (LPARAM)&mi);
		}

		for(; it != taskMenus.end(); ++it)
		{
			memset(&mi, 0, sizeof(mi));
			mi.cbSize = sizeof(mi);
			mi.flags = CMIM_FLAGS | CMIF_CHILDPOPUP | CMIF_ROOTHANDLE | CMIF_TCHAR | CMIF_KEEPUNTRANSLATED | CMIF_HIDDEN;
			mi.hParentMenu = hTaskMainMenu;
			CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)(HGENMENU)*it, (LPARAM)&mi);
		}

		int pos = (int)taskMenus.size();
		for(; taskIt != Options::instance->taskOptions.end(); ++taskIt)
		{
			memset(&mi, 0, sizeof(mi));
			mi.cbSize = sizeof(mi);
			mi.flags = CMIF_CHILDPOPUP | CMIF_ROOTHANDLE | CMIF_TCHAR | CMIF_KEEPUNTRANSLATED;
			mi.pszService = MS_HISTORY_EXECUTE_TASK;
			mi.hParentMenu = hTaskMainMenu;
			mi.popupPosition = pos++;
			mi.ptszName = (TCHAR*)taskIt->taskName.c_str();
			HGENMENU menu = Menu_AddMainMenuItem(&mi);
			taskMenus.push_back(menu);
		}
	}
	else if(hTaskMainMenu != NULL)
	{
		CLISTMENUITEM mi = { 0 };
		mi.cbSize = sizeof(mi);
		mi.flags = CMIM_FLAGS | CMIF_ROOTPOPUP | CMIF_HIDDEN;
		CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hTaskMainMenu, (LPARAM)&mi);
	}
}

void InitIcolib()
{
	TCHAR stzFile[MAX_PATH];

	SKINICONDESC sid = {0};
	sid.cbSize = sizeof(sid);
	sid.cx = sid.cy = 16;
	sid.ptszDefaultFile = stzFile;
	sid.ptszSection = LPGENT("History");
	sid.flags = SIDF_ALL_TCHAR;

	GetModuleFileName(hInst, stzFile, MAX_PATH);

	iconsNum = 3;
	hEventIcons = new HANDLE[iconsNum];
	sid.pszName = "BasicHistory_in";
	sid.ptszDescription = LPGENT("Incoming message");
	sid.iDefaultIndex = -IDI_INM;
	hEventIcons[0] = Skin_AddIcon(&sid);

	sid.pszName = "BasicHistory_out";
	sid.ptszDescription = LPGENT("Outgoing message");
	sid.iDefaultIndex = -IDI_OUTM;
	hEventIcons[1] = Skin_AddIcon(&sid);

	sid.pszName = "BasicHistory_status";
	sid.ptszDescription = LPGENT("Statuschange");
	sid.iDefaultIndex = -IDI_STATUS;
	hEventIcons[2] = Skin_AddIcon(&sid);

	sid.pszName = "BasicHistory_show";
	sid.ptszDescription = LPGENT("Show Contacts");
	sid.iDefaultIndex = -IDI_SHOW;
	hPlusIcon = Skin_AddIcon(&sid);

	sid.pszName = "BasicHistory_hide";
	sid.ptszDescription = LPGENT("Hide Contacts");
	sid.iDefaultIndex = -IDI_HIDE;
	hMinusIcon = Skin_AddIcon(&sid);

	sid.pszName = "BasicHistory_findnext";
	sid.ptszDescription = LPGENT("Find Next");
	sid.iDefaultIndex = -IDI_FINDNEXT;
	hFindNextIcon = Skin_AddIcon(&sid);

	sid.pszName = "BasicHistory_findprev";
	sid.ptszDescription = LPGENT("Find Previous");
	sid.iDefaultIndex = -IDI_FINDPREV;
	hFindPrevIcon = Skin_AddIcon(&sid);

	sid.pszName = "BasicHistory_plusex";
	sid.ptszDescription = LPGENT("Plus in export");
	sid.iDefaultIndex = -IDI_PLUSEX;
	hPlusExIcon = Skin_AddIcon(&sid);

	sid.pszName = "BasicHistory_minusex";
	sid.ptszDescription = LPGENT("Minus in export");
	sid.iDefaultIndex = -IDI_MINUSEX;
	hMinusExIcon = Skin_AddIcon(&sid);
}

void InitUpdater()
{
	if (ServiceExists(MS_UPDATE_REGISTER))
	{
		Update update = {0};
		char szVersion[16];
		update.cbSize = sizeof(Update);
		update.szComponentName = pluginInfo.shortName;
		update.pbVersion = (BYTE *)CreateVersionStringPluginEx(&pluginInfo, szVersion);
		update.cpbVersion = (int)strlen((char *)update.pbVersion);

#ifdef _WIN64
		update.szUpdateURL = "http://programista.free.of.pl/miranda/BasicHistory64.zip";
		update.szVersionURL = "http://programista.free.of.pl/miranda/pluginversion.php?plugin=basichistory&x64=yes";
		update.szBetaUpdateURL = "http://programista.free.of.pl/miranda/BasicHistoryBeta64.zip";
		update.szBetaVersionURL = "http://programista.free.of.pl/miranda/pluginversion.php?plugin=basichistory&beta=yes&x64=yes";
		update.szBetaChangelogURL = "http://programista.free.of.pl/miranda/BasicHistoryChangelog.txt";
#else
		update.szUpdateURL = "http://programista.free.of.pl/miranda/BasicHistory.zip";
		update.szVersionURL = "http://programista.free.of.pl/miranda/pluginversion.php?plugin=basichistory";
		update.szBetaUpdateURL = "http://programista.free.of.pl/miranda/BasicHistoryBeta.zip";
		update.szBetaVersionURL = "http://programista.free.of.pl/miranda/pluginversion.php?plugin=basichistory&beta=yes";
		update.szBetaChangelogURL = "http://programista.free.of.pl/miranda/BasicHistoryChangelog.txt";

#endif
		update.pbBetaVersionPrefix = update.pbVersionPrefix = (BYTE *)"Basic History ";
		update.cpbBetaVersionPrefix = update.cpbVersionPrefix = (int)strlen((char *)update.pbVersionPrefix);
		CallService(MS_UPDATE_REGISTER, 0, (WPARAM)&update);
	}
}

INT_PTR ShowContactHistory(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact = (HANDLE)wParam;
	HistoryWindow::Open(hContact);
	return 0;
}

int PreShutdownHistoryModule(WPARAM, LPARAM)
{
	HistoryWindow::Deinit();
	DeinitScheduler();
	return 0;
}

int HistoryContactDelete(WPARAM wParam, LPARAM)
{
	HistoryWindow::Close((HANDLE)wParam);
	return 0;
}

int ModulesLoaded(WPARAM wParam, LPARAM lParam)
{
	InitMenuItems();
	InitUpdater();

	TCHAR ftpExe[MAX_PATH];
	if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, ftpExe)))
	{
		_tcscat_s(ftpExe, _T("\\WinSCP\\WinSCP.exe"));
		DWORD atr = GetFileAttributes(ftpExe);
		if(atr == INVALID_FILE_ATTRIBUTES || atr & FILE_ATTRIBUTE_DIRECTORY)
		{
#ifdef _WIN64
			if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILESX86, NULL, SHGFP_TYPE_CURRENT, ftpExe)))
			{
				_tcscat_s(ftpExe, _T("\\WinSCP\\WinSCP.exe"));
				atr = GetFileAttributes(ftpExe);
				if(!(atr == INVALID_FILE_ATTRIBUTES || atr & FILE_ATTRIBUTE_DIRECTORY))
				{
					Options::instance->ftpExePathDef = ftpExe;
				}
			}
#endif
		}
		else
			Options::instance->ftpExePathDef = ftpExe;
	}

	TCHAR* log = _T("%miranda_logpath%\\BasicHistory\\ftplog.txt");
	TCHAR* logAbsolute = Utils_ReplaceVarsT(log);
	Options::instance->ftpLogPath = logAbsolute;
	mir_free(logAbsolute);
	Options::instance->Load();
	InitTaskMenuItems();

	HookEvent(ME_TTB_MODULELOADED,ToolbarModuleLoaded);
	HookEvent(ME_SYSTEM_PRESHUTDOWN,PreShutdownHistoryModule);
	HookEvent(ME_DB_CONTACT_DELETED,HistoryContactDelete);
	HookEvent(ME_FONT_RELOAD, HistoryWindow::FontsChanged);
	HookEvent(ME_SYSTEM_OKTOEXIT, DoLastTask);
	
	if (ServiceExists(MS_SMILEYADD_REPLACESMILEYS))
		g_SmileyAddAvail = true;

	if (ServiceExists(MS_MC_GETPROTOCOLNAME))
		metaContactProto = (char*)CallService(MS_MC_GETPROTOCOLNAME, 0, 0);

	InitScheduler();
	return 0;
}

extern "C" int __declspec(dllexport) Load(void)
{
	hTaskMainMenu = NULL;
	DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&g_hMainThread,0,FALSE,DUPLICATE_SAME_ACCESS);
	mir_getTMI(&tmi);
	mir_getLP(&pluginInfo);
	hCurSplitNS = LoadCursor(NULL, IDC_SIZENS);
	hCurSplitWE = LoadCursor(NULL, IDC_SIZEWE);
	hServiceShowContactHistory = CreateServiceFunction(MS_HISTORY_SHOWCONTACTHISTORY, ShowContactHistory);
	hServiceDeleteAllContactHistory = CreateServiceFunction(MS_HISTORY_DELETEALLCONTACTHISTORY, HistoryWindow::DeleteAllUserHistory);
	hServiceExecuteTask = CreateServiceFunction(MS_HISTORY_EXECUTE_TASK, ExecuteTaskService);
	Options::instance = new Options();
	HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded);
	HookEvent(ME_OPT_INITIALISE, Options::InitOptions);
	EventList::Init();
	InitIcolib();
	return 0;
}

extern "C" int __declspec(dllexport) Unload(void)
{
	if (g_hMainThread)
		CloseHandle(g_hMainThread);
	g_hMainThread=NULL;
	
	DestroyServiceFunction(hServiceShowContactHistory);
	DestroyServiceFunction(hServiceDeleteAllContactHistory);
	DestroyServiceFunction(hServiceExecuteTask);
	
	HistoryWindow::Deinit();
	
	DestroyCursor(hCurSplitNS);
	DestroyCursor(hCurSplitWE);
	
	EventList::Deinit();
	
	if(Options::instance != NULL)
	{
		Options::instance->Unload();
		delete Options::instance;
		Options::instance = NULL;
	}

	if(hEventIcons != NULL)
	{
		delete [] hEventIcons;
	}

	return 0;
}
