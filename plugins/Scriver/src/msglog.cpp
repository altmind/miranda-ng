/*
Scriver

Copyright (c) 2000-12 Miranda ICQ/IM project,

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

#define LOGICON_MSG_IN      0
#define LOGICON_MSG_OUT     1
#define LOGICON_MSG_NOTICE  2

extern int RTL_Detect(WCHAR *pszwText);
static int logPixelSY;
static char* pLogIconBmpBits[3];
static HIMAGELIST g_hImageList;

#define STREAMSTAGE_HEADER  0
#define STREAMSTAGE_EVENTS  1
#define STREAMSTAGE_TAIL    2
#define STREAMSTAGE_STOP    3

#define SMF_AFTERMASK (SMF_SHOWTIME | SMF_GROUPMESSAGES | SMF_MARKFOLLOWUPS)

struct EventData
{
	int cbSize;
	int iType;
	DWORD	dwFlags;
	const char *fontName;
	int fontSize;
	int fontStyle;
	COLORREF	color;
	union {
		char *pszNick;     // Nick, usage depends on type of event
		wchar_t *pszNickT;
		wchar_t *pszNickW; // Nick - Unicode
	};
	union {
		char *pszText;     // Text, usage depends on type of event
		wchar_t *pszTextT;
		wchar_t *pszTextW; // Text - Unicode
	};
	union {
		char *pszText2;     // Text, usage depends on type of event
		wchar_t *pszText2T;
		wchar_t *pszText2W; // Text - Unicode
	};
	DWORD	time;
	DWORD	eventType;
	BOOL  custom;
	EventData *next;
};

struct LogStreamData
{
	int      stage;
	MCONTACT hContact;
	MEVENT   hDbEvent, hDbEventLast;
	char    *buffer;
	size_t   bufferOffset, bufferLen;
	int      eventsToInsert;
	int      isFirst;
	SrmmWindowData *dlgDat;
	GlobalMessageData *gdat;
	EventData *events;
};

int DbEventIsCustomForMsgWindow(DBEVENTINFO *dbei)
{
	DBEVENTTYPEDESCR *et = (DBEVENTTYPEDESCR*)CallService(MS_DB_EVENT_GETTYPE, (WPARAM)dbei->szModule, (LPARAM)dbei->eventType);
	return et && (et->flags & DETF_MSGWINDOW);
}

int DbEventIsMessageOrCustom(DBEVENTINFO* dbei)
{
	return dbei->eventType == EVENTTYPE_MESSAGE || DbEventIsCustomForMsgWindow(dbei);
}

int DbEventIsShown(DBEVENTINFO &dbei)
{
	switch (dbei.eventType) {
	case EVENTTYPE_MESSAGE:
		return 1;

	case EVENTTYPE_JABBER_CHATSTATES:
	case EVENTTYPE_JABBER_PRESENCE:
		return 0;

	case EVENTTYPE_FILE:
	case EVENTTYPE_URL:
		return 1;
	}

	return DbEventIsCustomForMsgWindow(&dbei);
}

EventData* getEventFromDB(SrmmWindowData *dat, MCONTACT hContact, MEVENT hDbEvent)
{
	DBEVENTINFO dbei = { sizeof(dbei) };
	dbei.cbBlob = db_event_getBlobSize(hDbEvent);
	if (dbei.cbBlob == -1)
		return NULL;
	dbei.pBlob = (PBYTE)mir_alloc(dbei.cbBlob);
	db_event_get(hDbEvent, &dbei);
	if (!DbEventIsShown(dbei)) {
		mir_free(dbei.pBlob);
		return NULL;
	}

	EventData *evt = (EventData*)mir_calloc(sizeof(EventData));
	evt->custom = DbEventIsCustomForMsgWindow(&dbei);
	if (!(dbei.flags & DBEF_SENT) && (dbei.eventType == EVENTTYPE_MESSAGE || dbei.eventType == EVENTTYPE_URL || evt->custom)) {
		db_event_markRead(hContact, hDbEvent);
		pcli->pfnRemoveEvent(hContact, hDbEvent);
	}
	else if (dbei.eventType == EVENTTYPE_JABBER_CHATSTATES || dbei.eventType == EVENTTYPE_JABBER_PRESENCE)
		db_event_markRead(hContact, hDbEvent);

	evt->eventType = dbei.eventType;
	evt->dwFlags = (dbei.flags & DBEF_READ ? IEEDF_READ : 0) | (dbei.flags & DBEF_SENT ? IEEDF_SENT : 0) | (dbei.flags & DBEF_RTL ? IEEDF_RTL : 0);
	evt->dwFlags |= IEEDF_UNICODE_TEXT | IEEDF_UNICODE_NICK | IEEDF_UNICODE_TEXT2;

	if (dat->flags & SMF_RTL)
		evt->dwFlags |= IEEDF_RTL;

	evt->time = dbei.timestamp;
	evt->pszNick = NULL;
	if (evt->dwFlags & IEEDF_SENT)
		evt->pszNickT = Contact_GetInfo(CNF_DISPLAY, NULL, dat->szProto);
	else
		evt->pszNickT = mir_wstrdup(pcli->pfnGetContactDisplayName(hContact, 0));

	if (evt->eventType == EVENTTYPE_FILE) {
		char *filename = ((char*)dbei.pBlob) + sizeof(DWORD);
		char *descr = filename + mir_strlen(filename) + 1;
		evt->pszTextT = DbGetEventStringT(&dbei, filename);
		if (*descr != 0)
			evt->pszText2T = DbGetEventStringT(&dbei, descr);
	}
	else evt->pszTextT = DbGetEventTextW(&dbei, CP_UTF8);

	if (!(dat->flags & SMF_RTL) && RTL_Detect(evt->pszTextT))
		evt->dwFlags |= IEEDF_RTL;

	mir_free(dbei.pBlob);
	return evt;
}

static EventData* GetTestEvent(DWORD flags)
{
	EventData *evt = (EventData *)mir_calloc(sizeof(EventData));
	evt->eventType = EVENTTYPE_MESSAGE;
	evt->dwFlags = IEEDF_READ | flags;
	evt->dwFlags |= IEEDF_UNICODE_TEXT | IEEDF_UNICODE_NICK | IEEDF_UNICODE_TEXT2;
	evt->time = time(NULL);
	return evt;
}

static EventData* GetTestEvents()
{
	EventData *evt, *firstEvent, *prevEvent;
	firstEvent = prevEvent = evt = GetTestEvent(IEEDF_SENT);
	evt->pszNickT = mir_wstrdup(TranslateT("Me"));
	evt->pszTextT = mir_wstrdup(TranslateT("O Lord, bless this Thy hand grenade that with it Thou mayest blow Thine enemies"));

	evt = GetTestEvent(IEEDF_SENT);
	evt->pszNickT = mir_wstrdup(TranslateT("Me"));
	evt->pszTextT = mir_wstrdup(TranslateT("to tiny bits, in Thy mercy"));
	prevEvent->next = evt;
	prevEvent = evt;

	evt = GetTestEvent(0);
	evt->pszNickT = mir_wstrdup(TranslateT("My contact"));
	evt->pszTextT = mir_wstrdup(TranslateT("Lorem ipsum dolor sit amet,"));
	prevEvent->next = evt;
	prevEvent = evt;

	evt = GetTestEvent(0);
	evt->pszNickT = mir_wstrdup(TranslateT("My contact"));
	evt->pszTextT = mir_wstrdup(TranslateT("consectetur adipisicing elit"));
	prevEvent->next = evt;
	prevEvent = evt;
	return firstEvent;
}

static void freeEvent(EventData *evt)
{
	mir_free(evt->pszNickT);
	mir_free(evt->pszTextT);
	mir_free(evt->pszText2T);
	mir_free(evt);
}

static int AppendUnicodeOrAnsiiToBufferL(CMStringA &buf, const WCHAR *line, size_t maxLen, BOOL isAnsii)
{
	if (maxLen == -1)
		maxLen = mir_wstrlen(line);

	const WCHAR *maxLine = line + maxLen;

	if (isAnsii)
		buf.Append("{");
	else
		buf.Append("{\\uc1 ");

	int wasEOL = 0, textCharsCount = 0;
	for (; line < maxLine; line++, textCharsCount++) {
		wasEOL = 0;
		if (*line == '\r' && line[1] == '\n') {
			buf.Append("\\line ");
			wasEOL = 1;
			line++;
		}
		else if (*line == '\n') {
			buf.Append("\\line ");
			wasEOL = 1;
		}
		else if (*line == '\t') {
			buf.Append("\\tab ");
		}
		else if (*line == '\\' || *line == '{' || *line == '}') {
			buf.AppendChar('\\');
			buf.AppendChar((char)*line);
		}
		else if (*line < 128) {
			buf.AppendChar((char)*line);
		}
		else if (isAnsii) {
			buf.AppendFormat("\\'%02x", (*line) & 0xFF);
		}
		else {
			buf.AppendFormat("\\u%d ?", *line);
		}
	}
	if (wasEOL)
		buf.AppendChar(' ');

	buf.AppendChar('}');

	return textCharsCount;
}

static int AppendAnsiToBuffer(CMStringA &buf, const char *line)
{
	return AppendUnicodeOrAnsiiToBufferL(buf, _A2T(line), -1, true);
}

static int AppendUnicodeToBuffer(CMStringA &buf, const WCHAR *line)
{
	return AppendUnicodeOrAnsiiToBufferL(buf, line, -1, false);
}

// mir_free() the return value
static char* CreateRTFHeader()
{
	HDC hdc = GetDC(NULL);
	logPixelSY = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(NULL, hdc);

	CMStringA buf;

	buf.Append("{\\rtf1\\ansi\\deff0{\\fonttbl");
	for (int i = 0; i < fontOptionsListSize; i++) {
		LOGFONT lf;
		LoadMsgDlgFont(i, &lf, NULL);
		buf.AppendFormat("{\\f%u\\fnil\\fcharset%u %S;}", i, lf.lfCharSet, lf.lfFaceName);
	}
	buf.Append("}{\\colortbl ");

	COLORREF colour;
	for (int i = 0; i < fontOptionsListSize; i++) {
		LoadMsgDlgFont(i, NULL, &colour);
		buf.AppendFormat("\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
	}
	if (GetSysColorBrush(COLOR_HOTLIGHT) == NULL)
		colour = RGB(0, 0, 255);
	else
		colour = GetSysColor(COLOR_HOTLIGHT);
	buf.AppendFormat("\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
	colour = db_get_dw(NULL, SRMMMOD, SRMSGSET_BKGCOLOUR, SRMSGDEFSET_BKGCOLOUR);
	buf.AppendFormat("\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
	colour = db_get_dw(NULL, SRMMMOD, SRMSGSET_INCOMINGBKGCOLOUR, SRMSGDEFSET_INCOMINGBKGCOLOUR);
	buf.AppendFormat("\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
	colour = db_get_dw(NULL, SRMMMOD, SRMSGSET_OUTGOINGBKGCOLOUR, SRMSGDEFSET_OUTGOINGBKGCOLOUR);
	buf.AppendFormat("\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
	colour = db_get_dw(NULL, SRMMMOD, SRMSGSET_LINECOLOUR, SRMSGDEFSET_LINECOLOUR);
	buf.AppendFormat("\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
	buf.Append("}");
	return buf.Detach();
}

// mir_free() the return value
static char* CreateRTFTail()
{
	return mir_strdup("}");
}

// return value is static
static char* SetToStyle(int style)
{
	static char szStyle[128];
	LOGFONT lf;
	LoadMsgDlgFont(style, &lf, NULL);
	mir_snprintf(szStyle, "\\f%u\\cf%u\\b%d\\i%d\\fs%u", style, style, lf.lfWeight >= FW_BOLD ? 1 : 0, lf.lfItalic, 2 * abs(lf.lfHeight) * 74 / logPixelSY);
	return szStyle;
}

// mode: 0 - date & time, 1 - date, 2 - time
wchar_t* TimestampToString(DWORD dwFlags, time_t check, int mode)
{
	static wchar_t szResult[512];
	wchar_t str[80];
	wchar_t format[20];

	szResult[0] = '\0';
	format[0] = '\0';
	if ((mode == 0 || mode == 1) && (dwFlags & SMF_SHOWDATE)) {
		struct tm tm_now, tm_today;
		time_t now = time(NULL);
		time_t today;
		tm_now = *localtime(&now);
		tm_today = tm_now;
		tm_today.tm_hour = tm_today.tm_min = tm_today.tm_sec = 0;
		today = mktime(&tm_today);

		if (dwFlags & SMF_RELATIVEDATE && check >= today) {
			mir_wstrcpy(szResult, TranslateT("Today"));
			if (mode == 0)
				mir_wstrcat(szResult, L",");
		}
		else if (dwFlags & SMF_RELATIVEDATE && check > (today - 86400)) {
			mir_wstrcpy(szResult, TranslateT("Yesterday"));
			if (mode == 0)
				mir_wstrcat(szResult, L",");
		}
		else {
			if (dwFlags & SMF_LONGDATE)
				mir_wstrcpy(format, L"D");
			else
				mir_wstrcpy(format, L"d");
		}
	}
	if (mode == 0 || mode == 2) {
		if (mode == 0 && (dwFlags & SMF_SHOWDATE))
			mir_wstrcat(format, L" ");

		mir_wstrcat(format, (dwFlags & SMF_SHOWSECONDS) ? L"s" : L"t");
	}
	if (format[0] != '\0') {
		TimeZone_PrintTimeStamp(NULL, check, format, str, _countof(str), 0);
		mir_wstrncat(szResult, str, _countof(szResult) - mir_wstrlen(szResult));
	}
	return szResult;
}

int isSameDate(time_t time1, time_t time2)
{
	struct tm tm_t1 = *localtime((time_t *)(&time1));
	struct tm tm_t2 = *localtime((time_t *)(&time2));
	if (tm_t1.tm_year == tm_t2.tm_year && tm_t1.tm_mon == tm_t2.tm_mon
		&& tm_t1.tm_mday == tm_t2.tm_mday) {
		return 1;
	}
	return 0;
}

static int DetectURL(wchar_t *text, BOOL firstChar)
{
	wchar_t c;
	struct prefix_s
	{
		wchar_t *text;
		int length;
	} prefixes[12] = {
		{ L"http:", 5 },
		{ L"file:", 5 },
		{ L"mailto:", 7 },
		{ L"ftp:", 4 },
		{ L"https:", 6 },
		{ L"gopher:", 7 },
		{ L"nntp:", 5 },
		{ L"prospero:", 9 },
		{ L"telnet:", 7 },
		{ L"news:", 5 },
		{ L"wais:", 5 },
		{ L"www.", 4 }
	};
	c = firstChar ? ' ' : text[-1];
	if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
		int found = 0;
		int i, len = 0;
		int prefixlen = _countof(prefixes);
		for (i = 0; i < prefixlen; i++) {
			if (!wcsncmp(text, prefixes[i].text, prefixes[i].length)) {
				len = prefixes[i].length;
				found = 1;
				break;
			}
		}
		if (found) {
			for (; text[len] != '\n' && text[len] != '\r' && text[len] != '\t' && text[len] != ' ' && text[len] != '\0'; len++);
			for (; len > 0; len--)
				if ((text[len - 1] >= '0' && text[len - 1] <= '9') || iswalpha(text[len - 1]))
					break;

			return len;
		}
	}
	return 0;
}

static void AppendWithCustomLinks(EventData *evt, int style, CMStringA &buf)
{
	if (evt->pszText == NULL)
		return;

	BOOL isAnsii = (evt->dwFlags & IEEDF_UNICODE_TEXT) == 0;
	WCHAR *wText;
	int lasttoken = 0;
	size_t len, laststart = 0;
	if (isAnsii) {
		len = mir_strlen(evt->pszText);
		wText = mir_a2u(evt->pszText);
	}
	else {
		wText = evt->pszTextW;
		len = (int)mir_wstrlen(evt->pszTextW);
	}
	for (size_t j = 0; j < len; j++) {
		int newtoken = 0;
		int l = DetectURL(wText + j, j == 0);
		if (l > 0)
			newtoken = 1;

		if (j == 0)
			lasttoken = newtoken;

		if (newtoken != lasttoken) {
			if (lasttoken == 0)
				buf.AppendFormat("%s ", SetToStyle(style));
			else
				buf.AppendFormat("%s ", SetToStyle(evt->dwFlags & IEEDF_SENT ? MSGFONTID_MYURL : MSGFONTID_YOURURL));

			AppendUnicodeOrAnsiiToBufferL(buf, wText + laststart, j - laststart, isAnsii);
			laststart = j;
			lasttoken = newtoken;
		}
	}
	if (len - laststart > 0) {
		if (lasttoken == 0)
			buf.AppendFormat("%s ", SetToStyle(style));
		else
			buf.AppendFormat("%s ", SetToStyle(evt->dwFlags & IEEDF_SENT ? MSGFONTID_MYURL : MSGFONTID_YOURURL));

		AppendUnicodeOrAnsiiToBufferL(buf, wText + laststart, len - laststart, isAnsii);
	}
	if (isAnsii)
		mir_free(wText);
}

//mir_free() the return value
static char* CreateRTFFromEvent(SrmmWindowData *dat, EventData *evt, GlobalMessageData *gdat, LogStreamData *streamData)
{
	int style, showColon = 0;
	int isGroupBreak = TRUE;
	int highlight = 0;

	if ((gdat->flags & SMF_GROUPMESSAGES) && evt->dwFlags == LOWORD(dat->lastEventType) &&
		evt->eventType == EVENTTYPE_MESSAGE && HIWORD(dat->lastEventType) == EVENTTYPE_MESSAGE &&
		(isSameDate(evt->time, dat->lastEventTime)) && ((((int)evt->time < dat->startTime) == (dat->lastEventTime < dat->startTime)) || !(evt->dwFlags & IEEDF_READ))) {
		isGroupBreak = FALSE;
	}

	CMStringA buf;
	if (!streamData->isFirst && !dat->isMixed) {
		if (isGroupBreak || gdat->flags & SMF_MARKFOLLOWUPS)
			buf.Append("\\par");
		else
			buf.Append("\\line");
	}

	if (evt->dwFlags & IEEDF_RTL)
		dat->isMixed = 1;

	if (!streamData->isFirst && isGroupBreak && (gdat->flags & SMF_DRAWLINES))
		buf.AppendFormat("\\sl-1\\slmult0\\highlight%d\\cf%d\\fs1  \\par\\sl0", fontOptionsListSize + 4, fontOptionsListSize + 4);

	buf.Append((evt->dwFlags & IEEDF_RTL) ? "\\rtlpar" : "\\ltrpar");

	if (evt->eventType == EVENTTYPE_MESSAGE)
		highlight = fontOptionsListSize + 2 + ((evt->dwFlags & IEEDF_SENT) ? 1 : 0);
	else
		highlight = fontOptionsListSize + 1;

	buf.AppendFormat("\\highlight%d\\cf%d", highlight, highlight);
	if (!streamData->isFirst && dat->isMixed) {
		if (isGroupBreak)
			buf.Append("\\sl-1 \\par\\sl0");
		else
			buf.Append("\\sl-1 \\line\\sl0");
	}
	streamData->isFirst = FALSE;
	if (dat->isMixed) {
		if (evt->dwFlags & IEEDF_RTL)
			buf.Append("\\ltrch\\rtlch");
		else
			buf.Append("\\rtlch\\ltrch");
	}
	if ((gdat->flags & SMF_SHOWICONS) && isGroupBreak) {
		int i = LOGICON_MSG_NOTICE;

		switch (evt->eventType) {
		case EVENTTYPE_MESSAGE:
			if (evt->dwFlags & IEEDF_SENT)
				i = LOGICON_MSG_OUT;
			else
				i = LOGICON_MSG_IN;
			break;

		default:
			i = LOGICON_MSG_NOTICE;
			break;
		}

		buf.Append("\\fs1  ");
		buf.Append(pLogIconBmpBits[i]);
		buf.AppendChar(' ');
	}

	if (gdat->flags & SMF_SHOWTIME && (evt->eventType != EVENTTYPE_MESSAGE ||
		(gdat->flags & SMF_MARKFOLLOWUPS || isGroupBreak || !(gdat->flags & SMF_GROUPMESSAGES)))) {
		wchar_t *timestampString = NULL;
		if (gdat->flags & SMF_GROUPMESSAGES && evt->eventType == EVENTTYPE_MESSAGE) {
			if (isGroupBreak) {
				if (!(gdat->flags & SMF_MARKFOLLOWUPS))
					timestampString = TimestampToString(gdat->flags, evt->time, 0);
				else if (gdat->flags & SMF_SHOWDATE)
					timestampString = TimestampToString(gdat->flags, evt->time, 1);
			}
			else if (gdat->flags & SMF_MARKFOLLOWUPS)
				timestampString = TimestampToString(gdat->flags, evt->time, 2);
		}
		else timestampString = TimestampToString(gdat->flags, evt->time, 0);

		if (timestampString != NULL) {
			buf.AppendFormat("%s ", SetToStyle(evt->dwFlags & IEEDF_SENT ? MSGFONTID_MYTIME : MSGFONTID_YOURTIME));
			AppendUnicodeToBuffer(buf, timestampString);
		}
		if (evt->eventType != EVENTTYPE_MESSAGE)
			buf.AppendFormat("%s: ", SetToStyle(evt->dwFlags & IEEDF_SENT ? MSGFONTID_MYCOLON : MSGFONTID_YOURCOLON));
		showColon = 1;
	}
	if ((!(gdat->flags & SMF_HIDENAMES) && evt->eventType == EVENTTYPE_MESSAGE && isGroupBreak) || evt->eventType == EVENTTYPE_JABBER_CHATSTATES || evt->eventType == EVENTTYPE_JABBER_PRESENCE) {
		if (evt->eventType == EVENTTYPE_MESSAGE) {
			if (showColon)
				buf.AppendFormat(" %s ", SetToStyle(evt->dwFlags & IEEDF_SENT ? MSGFONTID_MYNAME : MSGFONTID_YOURNAME));
			else
				buf.AppendFormat("%s ", SetToStyle(evt->dwFlags & IEEDF_SENT ? MSGFONTID_MYNAME : MSGFONTID_YOURNAME));
		}
		else buf.AppendFormat("%s ", SetToStyle(MSGFONTID_NOTICE));

		if (evt->dwFlags & IEEDF_UNICODE_NICK)
			AppendUnicodeToBuffer(buf, evt->pszNickW);
		else
			AppendAnsiToBuffer(buf, evt->pszNick);

		showColon = 1;
		if (evt->eventType == EVENTTYPE_MESSAGE && gdat->flags & SMF_GROUPMESSAGES) {
			if (gdat->flags & SMF_MARKFOLLOWUPS)
				buf.Append("\\par");
			else
				buf.Append("\\line");
			showColon = 0;
		}
	}

	if ((gdat->flags & SMF_AFTERMASK) == SMF_AFTERMASK && evt->eventType == EVENTTYPE_MESSAGE && isGroupBreak) {
		buf.AppendFormat(" %s ", SetToStyle(evt->dwFlags & IEEDF_SENT ? MSGFONTID_MYTIME : MSGFONTID_YOURTIME));
		AppendUnicodeToBuffer(buf, TimestampToString(gdat->flags, evt->time, 2));
		showColon = 1;
	}
	if (showColon && evt->eventType == EVENTTYPE_MESSAGE) {
		if (evt->dwFlags & IEEDF_RTL)
			buf.AppendFormat("\\~%s: ", SetToStyle(evt->dwFlags & IEEDF_SENT ? MSGFONTID_MYCOLON : MSGFONTID_YOURCOLON));
		else
			buf.AppendFormat("%s: ", SetToStyle(evt->dwFlags & IEEDF_SENT ? MSGFONTID_MYCOLON : MSGFONTID_YOURCOLON));
	}
	switch (evt->eventType) {
	case EVENTTYPE_JABBER_CHATSTATES:
	case EVENTTYPE_JABBER_PRESENCE:
	case EVENTTYPE_URL:
	case EVENTTYPE_FILE:
		style = MSGFONTID_NOTICE;
		buf.AppendFormat("%s ", SetToStyle(style));
		if (evt->eventType == EVENTTYPE_FILE) {
			if (evt->dwFlags & IEEDF_SENT)
				AppendUnicodeToBuffer(buf, TranslateT("File sent"));
			else
				AppendUnicodeToBuffer(buf, TranslateT("File received"));
			AppendUnicodeToBuffer(buf, L":");
		}
		else if (evt->eventType == EVENTTYPE_URL) {
			if (evt->dwFlags & IEEDF_SENT)
				AppendUnicodeToBuffer(buf, TranslateT("URL sent"));
			else
				AppendUnicodeToBuffer(buf, TranslateT("URL received"));
			AppendUnicodeToBuffer(buf, L":");
		}
		AppendUnicodeToBuffer(buf, L" ");

		if (evt->pszTextW != NULL) {
			if (evt->dwFlags & IEEDF_UNICODE_TEXT)
				AppendUnicodeToBuffer(buf, evt->pszTextW);
			else
				AppendAnsiToBuffer(buf, evt->pszText);
		}

		if (evt->pszText2W != NULL) {
			AppendUnicodeToBuffer(buf, L" (");
			if (evt->dwFlags & IEEDF_UNICODE_TEXT2)
				AppendUnicodeToBuffer(buf, evt->pszText2W);
			else
				AppendAnsiToBuffer(buf, evt->pszText2);
			AppendUnicodeToBuffer(buf, L")");
		}
		break;
	default:
		if (gdat->flags & SMF_MSGONNEWLINE && showColon)
			buf.Append("\\line");

		style = evt->dwFlags & IEEDF_SENT ? MSGFONTID_MYMSG : MSGFONTID_YOURMSG;
		AppendWithCustomLinks(evt, style, buf);
		break;
	}
	if (dat->isMixed)
		buf.Append("\\par");

	dat->lastEventTime = evt->time;
	dat->lastEventType = MAKELONG(evt->dwFlags, evt->eventType);
	return buf.Detach();
}

static DWORD CALLBACK LogStreamInEvents(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
	LogStreamData *dat = (LogStreamData*)dwCookie;

	if (dat->buffer == NULL) {
		dat->bufferOffset = 0;
		switch (dat->stage) {
		case STREAMSTAGE_HEADER:
			dat->buffer = CreateRTFHeader();
			dat->stage = STREAMSTAGE_EVENTS;
			break;
		case STREAMSTAGE_EVENTS:
			if (dat->events != NULL) {
				EventData *evt = dat->events;
				dat->buffer = NULL;
				dat->buffer = CreateRTFFromEvent(dat->dlgDat, evt, dat->gdat, dat);
				dat->events = evt->next;
				freeEvent(evt);
			}
			else if (dat->eventsToInsert) {
				do {
					EventData *evt = getEventFromDB(dat->dlgDat, dat->hContact, dat->hDbEvent);
					dat->buffer = NULL;
					if (evt != NULL) {
						dat->buffer = CreateRTFFromEvent(dat->dlgDat, evt, dat->gdat, dat);
						freeEvent(evt);
					}
					if (dat->buffer)
						dat->hDbEventLast = dat->hDbEvent;
					dat->hDbEvent = db_event_next(dat->hContact, dat->hDbEvent);
					if (--dat->eventsToInsert == 0)
						break;
				} while (dat->buffer == NULL && dat->hDbEvent);
			}
			if (dat->buffer)
				break;

			dat->stage = STREAMSTAGE_TAIL;
			//fall through
		case STREAMSTAGE_TAIL:
			dat->buffer = CreateRTFTail();
			dat->stage = STREAMSTAGE_STOP;
			break;
		case STREAMSTAGE_STOP:
			*pcb = 0;
			return 0;
		}
		dat->bufferLen = mir_strlen(dat->buffer);
	}
	*pcb = min(cb, LONG(dat->bufferLen - dat->bufferOffset));
	memcpy(pbBuff, dat->buffer + dat->bufferOffset, *pcb);
	dat->bufferOffset += *pcb;
	if (dat->bufferOffset == dat->bufferLen) {
		mir_free(dat->buffer);
		dat->buffer = NULL;
	}
	return 0;
}

void StreamInTestEvents(HWND hEditWnd, GlobalMessageData *gdat)
{
	SrmmWindowData dat;
	memset(&dat, 0, sizeof(dat));

	LogStreamData streamData = { 0 };
	streamData.isFirst = TRUE;
	streamData.events = GetTestEvents();
	streamData.dlgDat = &dat;
	streamData.gdat = gdat;

	EDITSTREAM stream = { 0 };
	stream.pfnCallback = LogStreamInEvents;
	stream.dwCookie = (DWORD_PTR)&streamData;
	SendMessage(hEditWnd, EM_STREAMIN, SF_RTF, (LPARAM)&stream);
	SendMessage(hEditWnd, EM_HIDESELECTION, FALSE, 0);
}

void StreamInEvents(HWND hwndDlg, MEVENT hDbEventFirst, int count, int fAppend)
{
	FINDTEXTEXA fi;
	EDITSTREAM stream = { 0 };
	LogStreamData streamData = { 0 };
	SrmmWindowData *dat = (SrmmWindowData *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	CHARRANGE oldSel, sel;

	// IEVIew MOD Begin
	if (dat->hwndLog != NULL) {
		IEVIEWEVENT evt;
		IEVIEWWINDOW ieWindow;
		memset(&evt, 0, sizeof(evt));
		evt.cbSize = sizeof(evt);
		evt.dwFlags = ((dat->flags & SMF_RTL) ? IEEF_RTL : 0);
		evt.hwnd = dat->hwndLog;
		evt.hContact = dat->hContact;
		evt.pszProto = dat->szProto;
		if (!fAppend) {
			evt.iType = IEE_CLEAR_LOG;
			CallService(MS_IEVIEW_EVENT, 0, (LPARAM)&evt);
		}
		evt.iType = IEE_LOG_DB_EVENTS;
		evt.hDbEventFirst = hDbEventFirst;
		evt.count = count;
		CallService(MS_IEVIEW_EVENT, 0, (LPARAM)&evt);
		dat->hDbEventLast = evt.hDbEventFirst != NULL ? evt.hDbEventFirst : dat->hDbEventLast;

		memset(&ieWindow, 0, sizeof(ieWindow));
		ieWindow.cbSize = sizeof(ieWindow);
		ieWindow.iType = IEW_SCROLLBOTTOM;
		ieWindow.hwnd = dat->hwndLog;
		CallService(MS_IEVIEW_WINDOW, 0, (LPARAM)&ieWindow);
		return;
	}

	// IEVIew MOD End
	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_HIDESELECTION, TRUE, 0);
	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_EXGETSEL, 0, (LPARAM)&oldSel);
	streamData.hContact = dat->hContact;
	streamData.hDbEvent = hDbEventFirst;
	streamData.hDbEventLast = dat->hDbEventLast;
	streamData.dlgDat = dat;
	streamData.eventsToInsert = count;
	streamData.isFirst = fAppend ? GetRichTextLength(GetDlgItem(hwndDlg, IDC_LOG), CP_ACP, FALSE) == 0 : 1;
	streamData.gdat = &g_dat;
	stream.pfnCallback = LogStreamInEvents;
	stream.dwCookie = (DWORD_PTR)& streamData;
	sel.cpMin = 0;
	if (fAppend) {
		GETTEXTLENGTHEX gtxl = { 0 };
		gtxl.flags = GTL_DEFAULT | GTL_PRECISE | GTL_NUMCHARS;
		gtxl.codepage = 1200;
		fi.chrg.cpMin = SendDlgItemMessage(hwndDlg, IDC_LOG, EM_GETTEXTLENGTHEX, (WPARAM)&gtxl, 0);
		sel.cpMin = sel.cpMax = GetRichTextLength(GetDlgItem(hwndDlg, IDC_LOG), 1200, FALSE);
		SendDlgItemMessage(hwndDlg, IDC_LOG, EM_EXSETSEL, 0, (LPARAM)&sel);
	}
	else {
		SendDlgItemMessage(hwndDlg, IDC_LOG, WM_SETREDRAW, FALSE, 0);
		SetDlgItemText(hwndDlg, IDC_LOG, L"");
		sel.cpMin = 0;
		sel.cpMax = GetRichTextLength(GetDlgItem(hwndDlg, IDC_LOG), 1200, FALSE);
		SendDlgItemMessage(hwndDlg, IDC_LOG, EM_EXSETSEL, 0, (LPARAM)&sel);
		fi.chrg.cpMin = 0;
		dat->isMixed = 0;
	}

	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_STREAMIN, fAppend ? SFF_SELECTION | SF_RTF : SFF_SELECTION | SF_RTF, (LPARAM)&stream);
	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_EXSETSEL, 0, (LPARAM)&oldSel);
	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_HIDESELECTION, FALSE, 0);
	if (g_dat.smileyAddInstalled) {
		SMADD_RICHEDIT3 smre;
		smre.cbSize = sizeof(SMADD_RICHEDIT3);
		smre.hwndRichEditControl = GetDlgItem(hwndDlg, IDC_LOG);

		MCONTACT hContact = db_mc_getSrmmSub(dat->hContact);
		smre.Protocolname = (hContact != NULL) ? GetContactProto(hContact) : dat->szProto;

		if (fi.chrg.cpMin > 0) {
			sel.cpMin = fi.chrg.cpMin;
			sel.cpMax = -1;
			smre.rangeToReplace = &sel;
		}
		else smre.rangeToReplace = NULL;

		smre.disableRedraw = TRUE;
		smre.hContact = dat->hContact;
		smre.flags = 0;
		CallService(MS_SMILEYADD_REPLACESMILEYS, 0, (LPARAM)&smre);
	}

	int len = GetRichTextLength(GetDlgItem(hwndDlg, IDC_LOG), 1200, FALSE);
	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETSEL, len - 1, len - 1);

	if (!fAppend)
		SendDlgItemMessage(hwndDlg, IDC_LOG, WM_SETREDRAW, TRUE, 0);

	dat->hDbEventLast = streamData.hDbEventLast;
	PostMessage(hwndDlg, DM_SCROLLLOGTOBOTTOM, 0, 0);
}

#define RTFPICTHEADERMAXSIZE   78

void LoadMsgLogIcons(void)
{
	HICON hIcon = NULL;
	RECT rc;

	g_hImageList = ImageList_Create(10, 10, ILC_COLOR32 | ILC_MASK, _countof(pLogIconBmpBits), 0);
	HBRUSH hBkgBrush = CreateSolidBrush(db_get_dw(NULL, SRMMMOD, SRMSGSET_BKGCOLOUR, SRMSGDEFSET_BKGCOLOUR));
	HBRUSH hInBkgBrush = CreateSolidBrush(db_get_dw(NULL, SRMMMOD, SRMSGSET_INCOMINGBKGCOLOUR, SRMSGDEFSET_INCOMINGBKGCOLOUR));
	HBRUSH hOutBkgBrush = CreateSolidBrush(db_get_dw(NULL, SRMMMOD, SRMSGSET_OUTGOINGBKGCOLOUR, SRMSGDEFSET_OUTGOINGBKGCOLOUR));

	BITMAPINFOHEADER bih = { sizeof(bih) };
	bih.biBitCount = 24;
	bih.biCompression = BI_RGB;
	bih.biHeight = 10;
	bih.biPlanes = 1;
	bih.biWidth = 10;
	int widthBytes = ((bih.biWidth * bih.biBitCount + 31) >> 5) * 4;
	rc.top = rc.left = 0;
	rc.right = bih.biWidth;
	rc.bottom = bih.biHeight;
	HDC hdc = GetDC(NULL);
	HBITMAP hBmp = CreateCompatibleBitmap(hdc, bih.biWidth, bih.biHeight);
	HDC hdcMem = CreateCompatibleDC(hdc);
	PBYTE pBmpBits = (PBYTE)mir_alloc(widthBytes * bih.biHeight);
	HBRUSH hBrush = hBkgBrush;
	for (int i = 0; i < _countof(pLogIconBmpBits); i++) {
		switch (i) {
		case LOGICON_MSG_IN:
			ImageList_AddIcon(g_hImageList, GetCachedIcon("scriver_INCOMING"));
			hIcon = ImageList_GetIcon(g_hImageList, LOGICON_MSG_IN, ILD_NORMAL);
			hBrush = hInBkgBrush;
			break;
		case LOGICON_MSG_OUT:
			ImageList_AddIcon(g_hImageList, GetCachedIcon("scriver_OUTGOING"));
			hIcon = ImageList_GetIcon(g_hImageList, LOGICON_MSG_OUT, ILD_NORMAL);
			hBrush = hOutBkgBrush;
			break;
		case LOGICON_MSG_NOTICE:
			ImageList_AddIcon(g_hImageList, GetCachedIcon("scriver_NOTICE"));
			hIcon = ImageList_GetIcon(g_hImageList, LOGICON_MSG_NOTICE, ILD_NORMAL);
			hBrush = hBkgBrush;
			break;
		}

		pLogIconBmpBits[i] = (char*)mir_alloc(RTFPICTHEADERMAXSIZE + (bih.biSize + widthBytes * bih.biHeight) * 2);
		size_t rtfHeaderSize = sprintf(pLogIconBmpBits[i], "{\\pict\\dibitmap0\\wbmbitspixel%u\\wbmplanes1\\wbmwidthbytes%u\\picw%u\\pich%u ", bih.biBitCount, widthBytes, (UINT)bih.biWidth, (UINT)bih.biHeight); //!!!!!!!!!!!

		HBITMAP hoBmp = (HBITMAP)SelectObject(hdcMem, hBmp);
		FillRect(hdcMem, &rc, hBrush);
		DrawIconEx(hdcMem, 0, 0, hIcon, bih.biWidth, bih.biHeight, 0, NULL, DI_NORMAL);
		SelectObject(hdcMem, hoBmp);
		GetDIBits(hdc, hBmp, 0, bih.biHeight, pBmpBits, (BITMAPINFO *)& bih, DIB_RGB_COLORS);
		DestroyIcon(hIcon);

		char *szDest = pLogIconBmpBits[i] + rtfHeaderSize;
		bin2hex(&bih, sizeof(bih), szDest); szDest += sizeof(bih) * 2;
		bin2hex(pBmpBits, widthBytes * bih.biHeight, szDest); szDest += widthBytes * bih.biHeight * 2;
		mir_strcpy(szDest, "}");
	}
	mir_free(pBmpBits);
	DeleteDC(hdcMem);
	DeleteObject(hBmp);
	ReleaseDC(NULL, hdc);
	DeleteObject(hBkgBrush);
	DeleteObject(hInBkgBrush);
	DeleteObject(hOutBkgBrush);
}

void FreeMsgLogIcons(void)
{
	for (int i = 0; i < _countof(pLogIconBmpBits); i++)
		mir_free(pLogIconBmpBits[i]);

	ImageList_RemoveAll(g_hImageList);
	ImageList_Destroy(g_hImageList);
}
