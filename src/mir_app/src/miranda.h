/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (�) 2012-16 Miranda NG project (http://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

typedef HRESULT (STDAPICALLTYPE *pfnDrawThemeTextEx)(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, LPRECT, const struct _DTTOPTS *);
typedef HRESULT (STDAPICALLTYPE *pfnSetWindowThemeAttribute)(HWND, enum WINDOWTHEMEATTRIBUTETYPE, PVOID, DWORD);
typedef HRESULT (STDAPICALLTYPE *pfnBufferedPaintInit)(void);
typedef HRESULT (STDAPICALLTYPE *pfnBufferedPaintUninit)(void);
typedef HANDLE  (STDAPICALLTYPE *pfnBeginBufferedPaint)(HDC, RECT *, BP_BUFFERFORMAT, BP_PAINTPARAMS *, HDC *);
typedef HRESULT (STDAPICALLTYPE *pfnEndBufferedPaint)(HANDLE, BOOL);
typedef HRESULT (STDAPICALLTYPE *pfnGetBufferedPaintBits)(HANDLE, RGBQUAD **, int *);

extern pfnDrawThemeTextEx drawThemeTextEx;
extern pfnSetWindowThemeAttribute setWindowThemeAttribute;
extern pfnBufferedPaintInit bufferedPaintInit;
extern pfnBufferedPaintUninit bufferedPaintUninit;
extern pfnBeginBufferedPaint beginBufferedPaint;
extern pfnEndBufferedPaint endBufferedPaint;
extern pfnGetBufferedPaintBits getBufferedPaintBits;

extern ITaskbarList3 * pTaskbarInterface;

typedef HRESULT (STDAPICALLTYPE *pfnDwmExtendFrameIntoClientArea)(HWND hwnd, const MARGINS *margins);
typedef HRESULT (STDAPICALLTYPE *pfnDwmIsCompositionEnabled)(BOOL *);

extern pfnDwmExtendFrameIntoClientArea dwmExtendFrameIntoClientArea;
extern pfnDwmIsCompositionEnabled dwmIsCompositionEnabled;

/**** chat.cpp *************************************************************************/

extern struct CHAT_MANAGER chatApi;

/**** database.cpp *********************************************************************/

extern MIDatabase* currDb;
extern DATABASELINK* currDblink;
extern LIST<DATABASELINK> arDbPlugins;

int  InitIni(void);
void UninitIni(void);

/**** miranda.cpp **********************************************************************/

extern HINSTANCE g_hInst;
extern DWORD hMainThreadId;
extern HANDLE hOkToExitEvent, hModulesLoadedEvent, hevLoadModule, hevUnloadModule;
extern wchar_t mirandabootini[MAX_PATH];

/**** newplugins.cpp *******************************************************************/

char* GetPluginNameByInstance(HINSTANCE hInstance);
int   LoadStdPlugins(void);

/**** path.cpp *************************************************************************/

void InitPathVar(void);

/**** srmm.cpp *************************************************************************/

void KillModuleSrmmIcons(int hLangpack);

/**** utf.cpp **************************************************************************/

__forceinline char* Utf8DecodeA(const char* src)
{
	char* tmp = mir_strdup(src);
	Utf8Decode(tmp, NULL);
	return tmp;
}

#pragma optimize("", on)

/**** options.cpp **********************************************************************/

HTREEITEM FindNamedTreeItemAtRoot(HWND hwndTree, const wchar_t* name);

/**** skinicons.cpp ********************************************************************/

extern int g_iIconX, g_iIconY, g_iIconSX, g_iIconSY;

HICON LoadIconEx(HINSTANCE hInstance, LPCTSTR lpIconName, BOOL bShared);
int ImageList_AddIcon_NotShared(HIMAGELIST hIml, LPCTSTR szResource);
int ImageList_ReplaceIcon_NotShared(HIMAGELIST hIml, int iIndex, HINSTANCE hInstance, LPCTSTR szResource);

int ImageList_AddIcon_IconLibLoaded(HIMAGELIST hIml, int iconId);
int ImageList_AddIcon_ProtoIconLibLoaded(HIMAGELIST hIml, const char *szProto, int iconId);
int ImageList_ReplaceIcon_IconLibLoaded(HIMAGELIST hIml, int nIndex, HICON hIcon);

#define Safe_DestroyIcon(hIcon) if (hIcon) DestroyIcon(hIcon)

/**** clistmenus.cpp ********************************************************************/

extern CLIST_INTERFACE cli;

extern int hMainMenuObject, hContactMenuObject, hStatusMenuObject;
extern HANDLE hPreBuildMainMenuEvent, hPreBuildContactMenuEvent;
extern HANDLE hShutdownEvent, hPreShutdownEvent;
extern HMENU hMainMenu, hStatusMenu;

extern const int statusModeList[ MAX_STATUS_COUNT ];
extern const int skinIconStatusList[ MAX_STATUS_COUNT ];
extern const int skinIconStatusFlags[ MAX_STATUS_COUNT ];

extern OBJLIST<CListEvent> g_cliEvents;

int TryProcessDoubleClick(MCONTACT hContact);

/**** protocols.cpp *********************************************************************/

#define OFFSET_PROTOPOS 200
#define OFFSET_VISIBLE  400
#define OFFSET_ENABLED  600
#define OFFSET_NAME     800

extern LIST<PROTOACCOUNT> accounts;
extern LIST<PROTOCOLDESCRIPTOR> protos;

INT_PTR ProtoCallService(LPCSTR szModule, const char *szService, WPARAM wParam, LPARAM lParam);

PROTOACCOUNT* Proto_CreateAccount(const char *szModuleName, const char *szBaseProto, const wchar_t *tszAccountName);

PROTOACCOUNT* __fastcall Proto_GetAccount(MCONTACT hContact);

PROTO_INTERFACE* AddDefaultAccount(const char *szProtoName);
int  FreeDefaultAccount(PROTO_INTERFACE* ppi);

BOOL ActivateAccount(PROTOACCOUNT *pa);
void EraseAccount(const char *pszProtoName);
void DeactivateAccount(PROTOACCOUNT *pa, bool bIsDynamic, bool bErase);
void UnloadAccount(PROTOACCOUNT *pa, bool bIsDynamic, bool bErase);
void OpenAccountOptions(PROTOACCOUNT *pa);

void LoadDbAccounts(void);
void WriteDbAccounts(void);

INT_PTR CallProtoServiceInt(MCONTACT hContact, const char* szModule, const char* szService, WPARAM wParam, LPARAM lParam);

INT_PTR stubChainRecv(WPARAM, LPARAM);
#define MS_PROTO_HIDDENSTUB "Proto/stubChainRecv"

/**** utils.cpp ************************************************************************/

void HotkeyToName(wchar_t *buf, int size, BYTE shift, BYTE key);
WORD GetHotkeyValue(INT_PTR idHotkey);

HBITMAP ConvertIconToBitmap(HIMAGELIST hIml, int iconId);

///////////////////////////////////////////////////////////////////////////////

extern "C"
{
	MIR_CORE_DLL(int)  Langpack_MarkPluginLoaded(PLUGININFOEX* pInfo);
	MIR_CORE_DLL(int)  GetSubscribersCount(HANDLE hHook);
	MIR_CORE_DLL(void) db_setCurrent(MIDatabase* _db);
};
