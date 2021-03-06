#include "main.h"

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
	// {34B5A402-1B79-4246-B041-43D0B590AE2C}
	{ 0x34b5a402, 0x1b79, 0x4246, { 0xb0, 0x41, 0x43, 0xd0, 0xb5, 0x90, 0xae, 0x2c } }
};

CLIST_INTERFACE *pcli;
MWindowList hFileList;
HINSTANCE hInst;
int hLangpack;

char *szServiceTitle = SERVICE_TITLE;
char *szServicePrefix = SERVICE_PREFIX;
HANDLE hHookDbSettingChange, hHookContactAdded, hHookSkinIconsChanged;

HICON hIcons[5];

IconItem iconList[] =
{
	{ LPGEN("Play"), "FePlay", IDI_PLAY },
	{ LPGEN("Pause"), "FePause", IDI_PAUSE },
	{ LPGEN("Revive"), "FeRefresh", IDI_REFRESH },
	{ LPGEN("Stop"), "FeStop", IDI_STOP },
	{ LPGEN("Main"), "FeMain", IDI_SMALLICON },
};

int iIconId[5] = { 3, 2, 4, 1, 0 };

//
//  wParam - Section name
//  lParam - Icon ID
//
int OnSkinIconsChanged(WPARAM, LPARAM)
{
	for (int indx = 0; indx < _countof(hIcons); indx++)
		hIcons[indx] = IcoLib_GetIconByHandle(iconList[indx].hIcolib);

	WindowList_Broadcast(hFileList, WM_FE_SKINCHANGE, 0, 0);
	return 0;
}

int OnSettingChanged(WPARAM hContact, LPARAM lParam)
{
	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING*)lParam;

	if (hContact && !strcmp(cws->szSetting, "Status"))
	{
		HWND hwnd = WindowList_Find(hFileList, hContact);
		PostMessage(hwnd, WM_FE_STATUSCHANGE, 0, 0);
	}
	return 0;
}

INT_PTR OnRecvFile(WPARAM, LPARAM lParam)
{
	CLISTEVENT *clev = (CLISTEVENT*)lParam;

	HWND hwnd = WindowList_Find(hFileList, clev->hContact);
	if (IsWindow(hwnd))
	{
		ShowWindow(hwnd, SW_SHOWNORMAL);
		SetForegroundWindow(hwnd);
		SetFocus(hwnd);
	}
	/*
	else
	{
		if(hwnd != 0) WindowList_Remove(hFileList, hwnd);
		FILEECHO *fe = new FILEECHO((HANDLE)clev->hContact);
		fe->inSend = FALSE;
		hwnd = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_MAIN), NULL, DialogProc, (LPARAM)fe);
		if(hwnd == NULL)
		{
			delete fe;
			return 0;
		}
		//SendMessage(hwnd, WM_FE_SERVICE, 0, TRUE);
		ShowWindow(hwnd, SW_SHOWNORMAL);
	}
	*/
	return 1;
}

INT_PTR OnSendFile(WPARAM wParam, LPARAM)
{
	HWND hwnd = WindowList_Find(hFileList, wParam);
	if (IsWindow(hwnd))
	{
		SetForegroundWindow(hwnd);
		SetFocus(hwnd);
	}
	else
	{
		if (hwnd != 0) WindowList_Remove(hFileList, hwnd);
		FILEECHO *fe = new FILEECHO(wParam);
		fe->inSend = TRUE;
		hwnd = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_MAIN), NULL, DialogProc, (LPARAM)fe);
		if (hwnd == NULL)
		{
			delete fe;
			return 0;
		}
		//SendMessage(hwnd, WM_FE_SERVICE, 0, TRUE);
		ShowWindow(hwnd, SW_SHOWNORMAL);
	}
	return 1;
}

INT_PTR OnRecvMessage(WPARAM wParam, LPARAM lParam)
{
	CCSDATA *ccs = (CCSDATA *)lParam;
	PROTORECVEVENT *ppre = (PROTORECVEVENT *)ccs->lParam;

	if (strncmp(ppre->szMessage, szServicePrefix, mir_strlen(szServicePrefix)))
		return Proto_ChainRecv(wParam, ccs);

	HWND hwnd = WindowList_Find(hFileList, ccs->hContact);
	if (!IsWindow(hwnd))
	{
		if (hwnd != 0) WindowList_Remove(hFileList, hwnd);
		FILEECHO *fe = new FILEECHO(ccs->hContact);
		fe->inSend = FALSE;
		hwnd = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_MAIN), NULL, DialogProc, (LPARAM)fe);
		if (hwnd == NULL)
		{
			delete fe;
			return 0;
		}
	}
	char *msg = mir_strdup(ppre->szMessage + mir_strlen(szServicePrefix));
	PostMessage(hwnd, WM_FE_MESSAGE, (WPARAM)ccs->hContact, (LPARAM)msg);

	return 0;
}

int OnOptInitialise(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.hInstance = hInst;
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPTIONS);
	odp.pszTitle = SERVICE_TITLE;
	odp.pszGroup = LPGEN("Events");
	odp.flags = ODPF_BOLDGROUPS;
	odp.pfnDlgProc = OptionsDlgProc;
	Options_AddPage(wParam, &odp);
	return 0;
}

//
// MirandaPluginInfo()
// Called by Miranda to get Version
//
extern "C" __declspec(dllexport) PLUGININFOEX *MirandaPluginInfoEx(DWORD)
{
	return &pluginInfo;
}

//
// Startup initializing
//

static int OnModulesLoaded(WPARAM, LPARAM)
{
	for (int indx = 0; indx < _countof(hIcons); indx++)
		hIcons[indx] = IcoLib_GetIconByHandle(iconList[indx].hIcolib);

	hHookSkinIconsChanged = HookEvent(ME_SKIN2_ICONSCHANGED, OnSkinIconsChanged);

	CMenuItem mi;
	SET_UID(mi, 0xe4a98d2a, 0xa54a, 0x4db1, 0x8d, 0x29, 0xd, 0x5c, 0xf1, 0x10, 0x69, 0x35);
	mi.position = 200011;
	mi.hIcolibItem = iconList[ICON_MAIN].hIcolib;
	mi.name.a = LPGEN("File As Message...");
	mi.pszService = SERVICE_NAME "/FESendFile";
	mi.flags = CMIF_NOTOFFLINE;
	Menu_AddContactMenuItem(&mi);
	return 0;
}

extern "C" __declspec(dllexport) int Load(void)
{
	mir_getLP(&pluginInfo);
	mir_getCLI();

	InitCRC32();

	Icon_Register(hInst, "fileAsMessage", iconList, _countof(iconList));

	hFileList = WindowList_Create();

	CreateServiceFunction(SERVICE_NAME PSR_MESSAGE, OnRecvMessage);
	CreateServiceFunction(SERVICE_NAME "/FESendFile", OnSendFile);
	CreateServiceFunction(SERVICE_NAME "/FERecvFile", OnRecvFile);

	PROTOCOLDESCRIPTOR pd = { 0 };
	pd.cbSize = sizeof(pd);
	pd.szName = SERVICE_NAME;
	pd.type = PROTOTYPE_FILTER;
	Proto_RegisterModule(&pd);

	HookEvent(ME_OPT_INITIALISE, OnOptInitialise);
	HookEvent(ME_SYSTEM_MODULESLOADED, OnModulesLoaded);
	hHookDbSettingChange = HookEvent(ME_DB_CONTACT_SETTINGCHANGED, OnSettingChanged);
	hHookSkinIconsChanged = NULL;

	return 0;
}

//
// Unload()
// Called by Miranda when Plugin is unloaded.
//
extern "C" __declspec(dllexport) int Unload(void)
{
	WindowList_Destroy(hFileList);
	if (hHookSkinIconsChanged != NULL)
		UnhookEvent(hHookSkinIconsChanged);
	UnhookEvent(hHookDbSettingChange);
	UnhookEvent(hHookContactAdded);

	return 0;
}

//
// DllMain()
//
int WINAPI DllMain(HINSTANCE hInstance, DWORD, LPVOID)
{
	hInst = hInstance;
	return TRUE;
}
