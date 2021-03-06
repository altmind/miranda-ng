/*
Chat module plugin for Miranda IM

Copyright 2000-12 Miranda IM, 2012-16 Miranda NG project,
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

#include "stdafx.h"

INT_PTR SvcGetChatManager(WPARAM, LPARAM);

#include "chat.h"

HGENMENU hJoinMenuItem, hLeaveMenuItem;
mir_cs cs;

static HANDLE
   hServiceRegister = NULL,
   hServiceNewChat = NULL,
   hServiceAddEvent = NULL,
   hServiceGetAddEventPtr = NULL,
   hServiceGetInfo = NULL,
   hServiceGetCount = NULL,
   hEventPrebuildMenu = NULL,
   hEventDoubleclicked = NULL,
   hEventJoinChat = NULL,
   hEventLeaveChat = NULL,
   hHookEvent = NULL;

/////////////////////////////////////////////////////////////////////////////////////////
// Post-load event hooks

void LoadChatIcons(void)
{
	chatApi.hIcons[ICON_ACTION] = LoadIconEx("log_action", FALSE);
	chatApi.hIcons[ICON_ADDSTATUS] = LoadIconEx("log_addstatus", FALSE);
	chatApi.hIcons[ICON_HIGHLIGHT] = LoadIconEx("log_highlight", FALSE);
	chatApi.hIcons[ICON_INFO] = LoadIconEx("log_info", FALSE);
	chatApi.hIcons[ICON_JOIN] = LoadIconEx("log_join", FALSE);
	chatApi.hIcons[ICON_KICK] = LoadIconEx("log_kick", FALSE);
	chatApi.hIcons[ICON_MESSAGE] = LoadIconEx("log_message_in", FALSE);
	chatApi.hIcons[ICON_MESSAGEOUT] = LoadIconEx("log_message_out", FALSE);
	chatApi.hIcons[ICON_NICK] = LoadIconEx("log_nick", FALSE);
	chatApi.hIcons[ICON_NOTICE] = LoadIconEx("log_notice", FALSE);
	chatApi.hIcons[ICON_PART] = LoadIconEx("log_part", FALSE);
	chatApi.hIcons[ICON_QUIT] = LoadIconEx("log_quit", FALSE);
	chatApi.hIcons[ICON_REMSTATUS] = LoadIconEx("log_removestatus", FALSE);
	chatApi.hIcons[ICON_TOPIC] = LoadIconEx("log_topic", FALSE);
	chatApi.hIcons[ICON_STATUS0] = LoadIconEx("status0", FALSE);
	chatApi.hIcons[ICON_STATUS1] = LoadIconEx("status1", FALSE);
	chatApi.hIcons[ICON_STATUS2] = LoadIconEx("status2", FALSE);
	chatApi.hIcons[ICON_STATUS3] = LoadIconEx("status3", FALSE);
	chatApi.hIcons[ICON_STATUS4] = LoadIconEx("status4", FALSE);
	chatApi.hIcons[ICON_STATUS5] = LoadIconEx("status5", FALSE);

	FreeMsgLogBitmaps();
	LoadMsgLogBitmaps();
}

static int FontsChanged(WPARAM, LPARAM)
{
	LoadGlobalSettings();
	LoadLogFonts();

	FreeMsgLogBitmaps();
	LoadMsgLogBitmaps();

	SetIndentSize();
	g_Settings->bLogIndentEnabled = (db_get_b(NULL, CHAT_MODULE, "LogIndentEnabled", 1) != 0) ? TRUE : FALSE;

	chatApi.MM_FontsChanged();
	chatApi.MM_FixColors();
	chatApi.SM_BroadcastMessage(NULL, GC_SETWNDPROPS, 0, 0, TRUE);
	return 0;
}

static int IconsChanged(WPARAM, LPARAM)
{
	FreeMsgLogBitmaps();
	LoadMsgLogBitmaps();

	chatApi.MM_IconsChanged();
	chatApi.SM_BroadcastMessage(NULL, GC_SETWNDPROPS, 0, 0, FALSE);
	return 0;
}

static int PreShutdown(WPARAM, LPARAM)
{
	if (g_Settings != NULL) {
		chatApi.SM_BroadcastMessage(NULL, GC_CLOSEWINDOW, 0, 1, FALSE);

		chatApi.SM_RemoveAll();
		chatApi.MM_RemoveAll();

		DeleteObject(chatApi.hListBkgBrush);
		DeleteObject(chatApi.hListSelectedBkgBrush);
	}
	return 0;
}

static int SmileyOptionsChanged(WPARAM, LPARAM)
{
	chatApi.SM_BroadcastMessage(NULL, GC_REDRAWLOG, 0, 1, FALSE);
	return 0;
}

static INT_PTR Service_GetCount(WPARAM, LPARAM lParam)
{
	if (!lParam)
		return -1;

	mir_cslock lck(cs);
	return chatApi.SM_GetCount((char *)lParam);
}

static INT_PTR Service_GetInfo(WPARAM, LPARAM lParam)
{
	GC_INFO *gci = (GC_INFO *)lParam;
	if (!gci || !gci->pszModule)
		return 1;

	mir_cslock lck(cs);

	SESSION_INFO *si;
	if (gci->Flags & GCF_BYINDEX)
		si = chatApi.SM_FindSessionByIndex(gci->pszModule, gci->iItem);
	else
		si = chatApi.SM_FindSession(gci->pszID, gci->pszModule);
	if (si == NULL)
		return 1;

	if (gci->Flags & GCF_DATA)     gci->dwItemData = si->dwItemData;
	if (gci->Flags & GCF_HCONTACT) gci->hContact = si->hContact;
	if (gci->Flags & GCF_TYPE)     gci->iType = si->iType;
	if (gci->Flags & GCF_COUNT)    gci->iCount = si->nUsersInNicklist;
	if (gci->Flags & GCF_USERS)    gci->pszUsers = chatApi.SM_GetUsers(si);
	if (gci->Flags & GCF_ID)       gci->pszID = si->ptszID;
	if (gci->Flags & GCF_NAME)     gci->pszName = si->ptszName;
	return 0;
}

static INT_PTR Service_Register(WPARAM, LPARAM lParam)
{
	GCREGISTER *gcr = (GCREGISTER *)lParam;
	if (gcr == NULL)
		return GC_REGISTER_ERROR;

	if (gcr->cbSize != sizeof(GCREGISTER))
		return GC_REGISTER_WRONGVER;

	mir_cslock lck(cs);
	MODULEINFO *mi = chatApi.MM_AddModule(gcr->pszModule);
	if (mi == NULL)
		return GC_REGISTER_ERROR;

	mi->ptszModDispName = mir_wstrdup(gcr->ptszDispName);
	mi->bBold = (gcr->dwFlags & GC_BOLD) != 0;
	mi->bUnderline = (gcr->dwFlags & GC_UNDERLINE) != 0;
	mi->bItalics = (gcr->dwFlags & GC_ITALICS) != 0;
	mi->bColor = (gcr->dwFlags & GC_COLOR) != 0;
	mi->bBkgColor = (gcr->dwFlags & GC_BKGCOLOR) != 0;
	mi->bAckMsg = (gcr->dwFlags & GC_ACKMSG) != 0;
	mi->bChanMgr = (gcr->dwFlags & GC_CHANMGR) != 0;
	mi->bSingleFormat = (gcr->dwFlags & GC_SINGLEFORMAT) != 0;
	mi->bFontSize = (gcr->dwFlags & GC_FONTSIZE) != 0;
	mi->iMaxText = gcr->iMaxText;
	mi->nColorCount = gcr->nColors;
	if (gcr->nColors > 0) {
		mi->crColors = (COLORREF *)mir_alloc(sizeof(COLORREF)* gcr->nColors);
		memcpy(mi->crColors, gcr->pColors, sizeof(COLORREF)* gcr->nColors);
	}

	mi->pszHeader = chatApi.Log_CreateRtfHeader(mi);

	CheckColorsInModule((char*)gcr->pszModule);
	chatApi.SetAllOffline(TRUE, gcr->pszModule);
	return 0;
}

static INT_PTR Service_NewChat(WPARAM, LPARAM lParam)
{
	GCSESSION *gcw = (GCSESSION *)lParam;
	if (gcw == NULL)
		return GC_NEWSESSION_ERROR;

	if (gcw->cbSize != sizeof(GCSESSION))
		return GC_NEWSESSION_WRONGVER;

	mir_cslock lck(cs);
	MODULEINFO *mi = chatApi.MM_FindModule(gcw->pszModule);
	if (mi == NULL)
		return GC_NEWSESSION_ERROR;


	// try to restart a session first
	SESSION_INFO *si = chatApi.SM_FindSession(gcw->ptszID, gcw->pszModule);
	if (si != NULL) {
		chatApi.UM_RemoveAll(&si->pUsers);
		chatApi.TM_RemoveAll(&si->pStatuses);

		si->iStatusCount = 0;
		si->nUsersInNicklist = 0;
		si->pMe = NULL;

		if (chatApi.OnReplaceSession)
			chatApi.OnReplaceSession(si);
		return 0;
	}

	// create a new session and set the defaults
	if ((si = chatApi.SM_AddSession(gcw->ptszID, gcw->pszModule)) == NULL)
		return GC_NEWSESSION_ERROR;

	si->dwItemData = gcw->dwItemData;
	if (gcw->iType != GCW_SERVER)
		si->wStatus = ID_STATUS_ONLINE;
	si->iType = gcw->iType;
	si->dwFlags = gcw->dwFlags;
	si->ptszName = mir_wstrdup(gcw->ptszName);
	si->ptszStatusbarText = mir_wstrdup(gcw->ptszStatusbarText);
	si->iSplitterX = g_Settings->iSplitterX;
	si->iSplitterY = g_Settings->iSplitterY;
	si->iLogFilterFlags = db_get_dw(NULL, CHAT_MODULE, "FilterFlags", 0x03E0);
	si->bFilterEnabled = db_get_b(NULL, CHAT_MODULE, "FilterEnabled", 0);
	si->bNicklistEnabled = db_get_b(NULL, CHAT_MODULE, "ShowNicklist", 1);

	if (mi->bColor) {
		si->iFG = 4;
		si->bFGSet = TRUE;
	}
	if (mi->bBkgColor) {
		si->iBG = 2;
		si->bBGSet = TRUE;
	}

	wchar_t szTemp[256];
	if (si->iType == GCW_SERVER)
		mir_snwprintf(szTemp, L"Server: %s", si->ptszName);
	else
		wcsncpy_s(szTemp, si->ptszName, _TRUNCATE);
	si->hContact = chatApi.AddRoom(gcw->pszModule, gcw->ptszID, szTemp, si->iType);
	db_set_s(si->hContact, si->pszModule, "Topic", "");
	db_unset(si->hContact, "CList", "StatusMsg");
	if (si->ptszStatusbarText)
		db_set_ws(si->hContact, si->pszModule, "StatusBar", si->ptszStatusbarText);
	else
		db_set_s(si->hContact, si->pszModule, "StatusBar", "");

	if (chatApi.OnCreateSession)
		chatApi.OnCreateSession(si, mi);
	return 0;
}

static void SetInitDone(SESSION_INFO *si)
{
	if (si->bInitDone)
		return;

	si->bInitDone = true;
	for (STATUSINFO *p = si->pStatuses; p; p = p->next)
		if ((UINT_PTR)p->hIcon < STATUSICONCOUNT)
			p->hIcon = HICON(si->iStatusCount - (INT_PTR)p->hIcon - 1);
}

static int DoControl(GCEVENT *gce, WPARAM wp)
{
	SESSION_INFO *si;

	if (gce->pDest->iType == GC_EVENT_CONTROL) {
		switch (wp) {
		case WINDOW_HIDDEN:
			if (si = chatApi.SM_FindSession(gce->pDest->ptszID, gce->pDest->pszModule)) {
				SetInitDone(si);
				chatApi.SetActiveSession(si->ptszID, si->pszModule);
				if (si->hWnd)
					chatApi.ShowRoom(si, wp, FALSE);
			}
			return 0;

		case WINDOW_MINIMIZE:
		case WINDOW_MAXIMIZE:
		case WINDOW_VISIBLE:
		case SESSION_INITDONE:
			if (si = chatApi.SM_FindSession(gce->pDest->ptszID, gce->pDest->pszModule)) {
				SetInitDone(si);
				if (wp != SESSION_INITDONE || db_get_b(NULL, CHAT_MODULE, "PopupOnJoin", 0) == 0)
					chatApi.ShowRoom(si, wp, TRUE);
				return 0;
			}
			break;

		case SESSION_OFFLINE:
			chatApi.SM_SetOffline(gce->pDest->ptszID, gce->pDest->pszModule);
			// fall through

		case SESSION_ONLINE:
			chatApi.SM_SetStatus(gce->pDest->ptszID, gce->pDest->pszModule, wp == SESSION_ONLINE ? ID_STATUS_ONLINE : ID_STATUS_OFFLINE);
			break;

		case WINDOW_CLEARLOG:
			if (si = chatApi.SM_FindSession(gce->pDest->ptszID, gce->pDest->pszModule)) {
				chatApi.LM_RemoveAll(&si->pLog, &si->pLogEnd);
				if (chatApi.OnClearLog)
					chatApi.OnClearLog(si);
				si->iEventCount = 0;
				si->LastTime = 0;
			}
			break;

		case SESSION_TERMINATE:
			return chatApi.SM_RemoveSession(gce->pDest->ptszID, gce->pDest->pszModule, (gce->dwFlags & GCEF_REMOVECONTACT) != 0);
		}
		chatApi.SM_SendMessage(gce->pDest->ptszID, gce->pDest->pszModule, GC_EVENT_CONTROL + WM_USER + 500, wp, 0);
	}

	else if (gce->pDest->iType == GC_EVENT_CHUID && gce->ptszText) {
		chatApi.SM_ChangeUID(gce->pDest->ptszID, gce->pDest->pszModule, gce->ptszNick, gce->ptszText);
	}

	else if (gce->pDest->iType == GC_EVENT_CHANGESESSIONAME && gce->ptszText) {
		if (si = chatApi.SM_FindSession(gce->pDest->ptszID, gce->pDest->pszModule)) {
			replaceStrW(si->ptszName, gce->ptszText);
			if (si->hWnd)
				SendMessage(si->hWnd, GC_UPDATETITLE, 0, 0);
			if (chatApi.OnRenameSession)
				chatApi.OnRenameSession(si);
		}
	}

	else if (gce->pDest->iType == GC_EVENT_SETITEMDATA) {
		if (si = chatApi.SM_FindSession(gce->pDest->ptszID, gce->pDest->pszModule))
			si->dwItemData = gce->dwItemData;
	}

	else if (gce->pDest->iType == GC_EVENT_GETITEMDATA) {
		if (si = chatApi.SM_FindSession(gce->pDest->ptszID, gce->pDest->pszModule)) {
			gce->dwItemData = si->dwItemData;
			return si->dwItemData;
		}
		return 0;
	}
	else if (gce->pDest->iType == GC_EVENT_SETSBTEXT) {
		if (si = chatApi.SM_FindSession(gce->pDest->ptszID, gce->pDest->pszModule)) {
			replaceStrW(si->ptszStatusbarText, gce->ptszText);
			if (si->ptszStatusbarText)
				db_set_ws(si->hContact, si->pszModule, "StatusBar", si->ptszStatusbarText);
			else
				db_set_s(si->hContact, si->pszModule, "StatusBar", "");

			if (chatApi.OnSetStatusBar)
				chatApi.OnSetStatusBar(si);
		}
	}
	else if (gce->pDest->iType == GC_EVENT_ACK) {
		chatApi.SM_SendMessage(gce->pDest->ptszID, gce->pDest->pszModule, GC_ACKMESSAGE, 0, 0);
	}
	else if (gce->pDest->iType == GC_EVENT_SENDMESSAGE && gce->ptszText) {
		chatApi.SM_SendUserMessage(gce->pDest->ptszID, gce->pDest->pszModule, gce->ptszText);
	}
	else if (gce->pDest->iType == GC_EVENT_SETSTATUSEX) {
		chatApi.SM_SetStatusEx(gce->pDest->ptszID, gce->pDest->pszModule, gce->ptszText, gce->dwItemData);
	}
	else return 1;

	return 0;
}

static void AddUser(GCEVENT *gce)
{
	SESSION_INFO *si = chatApi.SM_FindSession(gce->pDest->ptszID, gce->pDest->pszModule);
	if (si == NULL) return;

	WORD status = chatApi.TM_StringToWord(si->pStatuses, gce->ptszStatus);
	USERINFO *ui = chatApi.SM_AddUser(gce->pDest->ptszID, gce->pDest->pszModule, gce->ptszUID, gce->ptszNick, status);
	if (ui == NULL) return;

	ui->pszNick = mir_wstrdup(gce->ptszNick);
	if (gce->bIsMe)
		si->pMe = ui;
	ui->Status = status;
	ui->Status |= si->pStatuses->Status;

	if (chatApi.OnNewUser)
		chatApi.OnNewUser(si, ui);
}

static INT_PTR Service_AddEvent(WPARAM wParam, LPARAM lParam)
{
	GCEVENT *gce = (GCEVENT*)lParam;
	BOOL bIsHighlighted = FALSE;
	BOOL bRemoveFlag = FALSE;

	if (gce == NULL)
		return GC_EVENT_ERROR;

	GCDEST *gcd = gce->pDest;
	if (gcd == NULL)
		return GC_EVENT_ERROR;

	if (gce->cbSize != sizeof(GCEVENT))
		return GC_EVENT_WRONGVER;

	if (!IsEventSupported(gcd->iType))
		return GC_EVENT_ERROR;

	if (NotifyEventHooks(hHookEvent, wParam, lParam))
		return 1;

	mir_cslock lck(cs);

	// Do different things according to type of event
	switch (gcd->iType) {
	case GC_EVENT_ADDGROUP:
		{
			STATUSINFO *si = chatApi.SM_AddStatus(gcd->ptszID, gcd->pszModule, gce->ptszStatus);
			if (si && gce->dwItemData)
				si->hIcon = CopyIcon((HICON)gce->dwItemData);
		}
		return 0;

	case GC_EVENT_CHUID:
	case GC_EVENT_CHANGESESSIONAME:
	case GC_EVENT_SETITEMDATA:
	case GC_EVENT_GETITEMDATA:
	case GC_EVENT_CONTROL:
	case GC_EVENT_SETSBTEXT:
	case GC_EVENT_ACK:
	case GC_EVENT_SENDMESSAGE:
	case GC_EVENT_SETSTATUSEX:
		return DoControl(gce, wParam);

	case GC_EVENT_SETCONTACTSTATUS:
		return chatApi.SM_SetContactStatus(gcd->ptszID, gcd->pszModule, gce->ptszUID, (WORD)gce->dwItemData);

	case GC_EVENT_TOPIC:
		if (SESSION_INFO *si = chatApi.SM_FindSession(gcd->ptszID, gcd->pszModule)) {
			if (gce->ptszText) {
				replaceStrW(si->ptszTopic, RemoveFormatting(gce->ptszText));
				db_set_ws(si->hContact, si->pszModule, "Topic", si->ptszTopic);
				if (chatApi.OnSetTopic)
					chatApi.OnSetTopic(si);
				if (db_get_b(NULL, CHAT_MODULE, "TopicOnClist", 0))
					db_set_ws(si->hContact, "CList", "StatusMsg", si->ptszTopic);
			}
		}
		break;

	case GC_EVENT_ADDSTATUS:
		chatApi.SM_GiveStatus(gcd->ptszID, gcd->pszModule, gce->ptszUID, gce->ptszStatus);
		bIsHighlighted = chatApi.IsHighlighted(NULL, gce);
		break;

	case GC_EVENT_REMOVESTATUS:
		chatApi.SM_TakeStatus(gcd->ptszID, gcd->pszModule, gce->ptszUID, gce->ptszStatus);
		bIsHighlighted = chatApi.IsHighlighted(NULL, gce);
		break;

	case GC_EVENT_MESSAGE:
	case GC_EVENT_ACTION:
		if (!gce->bIsMe && gcd->ptszID && gce->ptszText) {
			SESSION_INFO *si = chatApi.SM_FindSession(gcd->ptszID, gcd->pszModule);
			bIsHighlighted = chatApi.IsHighlighted(si, gce);
		}
		break;

	case GC_EVENT_NICK:
		chatApi.SM_ChangeNick(gcd->ptszID, gcd->pszModule, gce);
		bIsHighlighted = chatApi.IsHighlighted(NULL, gce);
		break;

	case GC_EVENT_JOIN:
		AddUser(gce);
		bIsHighlighted = chatApi.IsHighlighted(NULL, gce);
		break;

	case GC_EVENT_PART:
	case GC_EVENT_QUIT:
	case GC_EVENT_KICK:
		bRemoveFlag = TRUE;
		bIsHighlighted = chatApi.IsHighlighted(NULL, gce);
		break;
	}

	// Decide which window (log) should have the event
	LPCTSTR pWnd = NULL;
	LPCSTR pMod = NULL;
	if (gcd->ptszID) {
		pWnd = gcd->ptszID;
		pMod = gcd->pszModule;
	}
	else if (gcd->iType == GC_EVENT_NOTICE || gcd->iType == GC_EVENT_INFORMATION) {
		SESSION_INFO *si = chatApi.GetActiveSession();
		if (si && !mir_strcmp(si->pszModule, gcd->pszModule)) {
			pWnd = si->ptszID;
			pMod = si->pszModule;
		}
		else return 0;
	}
	else {
		// Send the event to all windows with a user pszUID. Used for broadcasting QUIT etc
		chatApi.SM_AddEventToAllMatchingUID(gce);
		if (!bRemoveFlag)
			return 0;
	}

	// add to log
	if (pWnd) {
		SESSION_INFO *si = chatApi.SM_FindSession(pWnd, pMod);

		// fix for IRC's old stuyle mode notifications. Should not affect any other protocol
		if ((gcd->iType == GC_EVENT_ADDSTATUS || gcd->iType == GC_EVENT_REMOVESTATUS) && !(gce->dwFlags & GCEF_ADDTOLOG))
			return 0;

		if (gcd->iType == GC_EVENT_JOIN && gce->time == 0)
			return 0;

		if (si && (si->bInitDone || gcd->iType == GC_EVENT_TOPIC || (gcd->iType == GC_EVENT_JOIN && gce->bIsMe))) {
			int isOk = chatApi.SM_AddEvent(pWnd, pMod, gce, bIsHighlighted);
			if (chatApi.OnAddLog)
				chatApi.OnAddLog(si, isOk);
			if (!(gce->dwFlags & GCEF_NOTNOTIFY))
				chatApi.DoSoundsFlashPopupTrayStuff(si, gce, bIsHighlighted, 0);
			if ((gce->dwFlags & GCEF_ADDTOLOG) && g_Settings->bLoggingEnabled)
				chatApi.LogToFile(si, gce);
		}

		if (!bRemoveFlag)
			return 0;
	}

	if (bRemoveFlag)
		return chatApi.SM_RemoveUser(gcd->ptszID, gcd->pszModule, gce->ptszUID) == 0;

	return GC_EVENT_ERROR;
}

static INT_PTR Service_GetAddEventPtr(WPARAM, LPARAM lParam)
{
	GCPTRS *gp = (GCPTRS *)lParam;

	mir_cslock lck(cs);
	gp->pfnAddEvent = Service_AddEvent;
	return 0;
}

static int ModulesLoaded(WPARAM, LPARAM)
{
	LoadChatIcons();

	HookEvent(ME_SMILEYADD_OPTIONSCHANGED, SmileyOptionsChanged);
	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, PrebuildContactMenu);

	CMenuItem mi;
	SET_UID(mi, 0x2bb76d5, 0x740d, 0x4fd2, 0x8f, 0xee, 0x7c, 0xa4, 0x5a, 0x74, 0x65, 0xa6);
	mi.position = -2000090001;
	mi.flags = CMIF_DEFAULT;
	mi.hIcolibItem = Skin_GetIconHandle(SKINICON_CHAT_JOIN);
	mi.name.a = LPGEN("&Join chat");
	mi.pszService = "GChat/JoinChat";
	hJoinMenuItem = Menu_AddContactMenuItem(&mi);

	SET_UID(mi, 0x72b7440b, 0xd2db, 0x4e22, 0xa6, 0xb1, 0x2, 0xd0, 0x96, 0xee, 0xad, 0x88);
	mi.position = -2000090000;
	mi.hIcolibItem = Skin_GetIconHandle(SKINICON_CHAT_LEAVE);
	mi.flags = CMIF_NOTOFFLINE;
	mi.name.a = LPGEN("&Leave chat");
	mi.pszService = "GChat/LeaveChat";
	hLeaveMenuItem = Menu_AddContactMenuItem(&mi);

	chatApi.SetAllOffline(TRUE, NULL);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Service creation

static bool bInited = false;

int LoadChatModule(void)
{
	HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, PreShutdown);
	HookEvent(ME_SKIN_ICONSCHANGED, IconsChanged);

	CreateServiceFunction(MS_GC_REGISTER, Service_Register);
	CreateServiceFunction(MS_GC_NEWSESSION, Service_NewChat);
	CreateServiceFunction(MS_GC_EVENT, Service_AddEvent);
	CreateServiceFunction(MS_GC_GETEVENTPTR, Service_GetAddEventPtr);
	CreateServiceFunction(MS_GC_GETINFO, Service_GetInfo);
	CreateServiceFunction(MS_GC_GETSESSIONCOUNT, Service_GetCount);

	CreateServiceFunction("GChat/DblClickEvent", EventDoubleclicked);
	CreateServiceFunction("GChat/PrebuildMenuEvent", PrebuildContactMenuSvc);
	CreateServiceFunction("GChat/JoinChat", JoinChat);
	CreateServiceFunction("GChat/LeaveChat", LeaveChat);
	CreateServiceFunction("GChat/GetInterface", SvcGetChatManager);

	chatApi.hSendEvent = CreateHookableEvent(ME_GC_EVENT);
	chatApi.hBuildMenuEvent = CreateHookableEvent(ME_GC_BUILDMENU);
	hHookEvent = CreateHookableEvent(ME_GC_HOOK_EVENT);

	HookEvent(ME_FONT_RELOAD, FontsChanged);
	HookEvent(ME_SKIN2_ICONSCHANGED, IconsChanged);

	bInited = true;
	return 0;
}

void UnloadChatModule(void)
{
	if (!bInited)
		return;

	mir_free(chatApi.szActiveWndID);
	mir_free(chatApi.szActiveWndModule);

	FreeMsgLogBitmaps();
	OptionsUnInit();

	DestroyHookableEvent(chatApi.hSendEvent);
	DestroyHookableEvent(chatApi.hBuildMenuEvent);
	DestroyHookableEvent(hHookEvent);
}
