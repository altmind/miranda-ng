/*

Chat module plugin for Miranda IM

Copyright (C) 2003 J�rgen Persson

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

#include <m_smileyadd.h>
#include <m_popup.h>
#include <m_fontservice.h>

struct MODULEINFO : public GCModuleInfoBase {};
struct SESSION_INFO : public GCSessionInfoBase {};
struct LOGSTREAMDATA : public GCLogStreamDataBase {};

// special service for tweaking performance
#define MS_GC_GETEVENTPTR  "GChat/GetNewEventPtr"
typedef INT_PTR(*GETEVENTFUNC)(WPARAM wParam, LPARAM lParam);
struct GCPTRS
{
	GETEVENTFUNC pfnAddEvent;
};

extern HGENMENU hJoinMenuItem, hLeaveMenuItem;
extern GlobalLogSettingsBase *g_Settings;
extern int g_cbSession, g_cbModuleInfo, g_iFontMode, g_iChatLang;
extern wchar_t *g_szFontGroup;
extern mir_cs cs;

extern char* pLogIconBmpBits[14];

// log.c
void   LoadMsgLogBitmaps(void);
void   FreeMsgLogBitmaps(void);
void   ValidateFilename (wchar_t *filename);
wchar_t* MakeTimeStamp(wchar_t *pszStamp, time_t time);
wchar_t* GetChatLogsFilename(SESSION_INFO *si, time_t tTime);
char*  Log_CreateRtfHeader(MODULEINFO *mi);
char*  Log_CreateRTF(LOGSTREAMDATA *streamData);
char*  Log_SetStyle(int style);

// clist.c
BOOL     AddEvent(MCONTACT hContact, HICON hIcon, MEVENT hEvent, int type, wchar_t* fmt, ...);
MCONTACT AddRoom(const char *pszModule, const wchar_t *pszRoom, const wchar_t *pszDisplayName, int iType);
MCONTACT FindRoom(const char *pszModule, const wchar_t *pszRoom);
BOOL     SetAllOffline(BOOL bHide, const char *pszModule);
BOOL     SetOffline(MCONTACT hContact, BOOL bHide);

int      RoomDoubleclicked(WPARAM wParam,LPARAM lParam);
INT_PTR  EventDoubleclicked(WPARAM wParam,LPARAM lParam);
INT_PTR  JoinChat(WPARAM wParam, LPARAM lParam);
INT_PTR  LeaveChat(WPARAM wParam, LPARAM lParam);
int      PrebuildContactMenu(WPARAM wParam, LPARAM lParam);
INT_PTR  PrebuildContactMenuSvc(WPARAM wParam, LPARAM lParam);

// colorchooser.c
void ColorChooser(SESSION_INFO *si, BOOL bFG, HWND hwndDlg, HWND hwndTarget, HWND hwndChooser);

// options.c
int    OptionsInit(void);
int    OptionsUnInit(void);
void   LoadMsgDlgFont(int i, LOGFONT * lf, COLORREF * colour);
void   LoadGlobalSettings(void);
HICON  LoadIconEx(char* pszIcoLibName, bool big);
void   LoadLogFonts(void);
void   SetIndentSize(void);
void   RegisterFonts(void);

// services.c
void   LoadChatIcons(void);
int    LoadChatModule(void);
void   UnloadChatModule(void);

// tools.c
int    DoRtfToTags(CMStringW &pszText, int iNumColors, COLORREF *pColors);
int    GetTextPixelSize(wchar_t* pszText, HFONT hFont, BOOL bWidth);
wchar_t *RemoveFormatting(const wchar_t* pszText);
BOOL   DoSoundsFlashPopupTrayStuff(SESSION_INFO *si, GCEVENT *gce, BOOL bHighlight, int bManyFix);
int    GetColorIndex(const char *pszModule, COLORREF cr);
void   CheckColorsInModule(const char *pszModule);
int    GetRichTextLength(HWND hwnd);
BOOL   IsHighlighted(SESSION_INFO *si, GCEVENT *pszText);
UINT   CreateGCMenu(HWND hwndDlg, HMENU *hMenu, int iIndex, POINT pt, SESSION_INFO *si, wchar_t* pszUID, wchar_t* pszWordText);
void   DestroyGCMenu(HMENU *hMenu, int iIndex);
BOOL   DoEventHookAsync(HWND hwnd, const wchar_t *pszID, const char *pszModule, int iType, const wchar_t* pszUID, const wchar_t* pszText, INT_PTR dwItem);
BOOL   DoEventHook(const wchar_t *pszID, const char *pszModule, int iType, const wchar_t *pszUID, const wchar_t* pszText, INT_PTR dwItem);
BOOL   IsEventSupported(int eventType);
BOOL   LogToFile(SESSION_INFO *si, GCEVENT *gce);
BOOL   DoTrayIcon(SESSION_INFO *si, GCEVENT *gce);
BOOL   DoPopup(SESSION_INFO *si, GCEVENT *gce);
int    ShowPopup(MCONTACT hContact, SESSION_INFO *si, HICON hIcon, char* pszProtoName, wchar_t* pszRoomName, COLORREF crBkg, const wchar_t* fmt, ...);

const wchar_t*  my_strstri(const wchar_t* s1, const wchar_t* s2);

#pragma comment(lib,"comctl32.lib")
