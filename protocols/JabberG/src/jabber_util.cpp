/*

Jabber Protocol Plugin for Miranda NG

Copyright (c) 2002-04  Santithorn Bunchua
Copyright (c) 2005-12  George Hazan
Copyright (�) 2012-16 Miranda NG project

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

#include "jabber_caps.h"

int CJabberProto::SerialNext(void)
{
	return ::InterlockedIncrement(&m_nSerial);
}

///////////////////////////////////////////////////////////////////////////////
// JabberChatRoomHContactFromJID - looks for the char room MCONTACT with required JID

MCONTACT CJabberProto::ChatRoomHContactFromJID(const TCHAR *jid)
{
	JABBER_LIST_ITEM *item = ListGetItemPtr(LIST_CHATROOM, jid);
	if (item != NULL && item->hContact)
		return item->hContact;

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// JabberHContactFromJID - looks for the MCONTACT with required JID

MCONTACT CJabberProto::HContactFromJID(const TCHAR *jid, bool bStripResource)
{
	if (jid == NULL)
		return NULL;

	JABBER_LIST_ITEM *item = ListGetItemPtr(LIST_ROSTER, jid);
	if (item != NULL && item->hContact)
		return item->hContact;

	if (bStripResource) {
		TCHAR szJid[JABBER_MAX_JID_LEN];
		JabberStripJid(jid, szJid, _countof(szJid));
		item = ListGetItemPtr(LIST_ROSTER, jid);
		if (item != NULL && item->hContact)
			return item->hContact;
	}

	return NULL;
}

TCHAR* __stdcall JabberNickFromJID(const TCHAR *jid)
{
	if (jid == NULL)
		return mir_tstrdup(_T(""));

	const TCHAR *p = _tcsrchr(jid, '/');
	if (p != NULL)
		return mir_tstrdup(p+1);

	p = _tcschr(jid, '@');
	return (p != NULL) ? mir_tstrndup(jid, p - jid) : mir_tstrdup(jid);
}

pResourceStatus CJabberProto::ResourceInfoFromJID(const TCHAR *jid)
{
	if (jid == NULL)
		return NULL;

	JABBER_LIST_ITEM *item = ListGetItemPtr(LIST_VCARD_TEMP, jid);
	if (item == NULL)
		item = ListGetItemPtr(LIST_ROSTER, jid);
	if (item == NULL)
		return NULL;

	const TCHAR *p = _tcschr(jid, '/');
	if (p == NULL)
		return item->getTemp();

	return item->findResource(p + 1);
}

TCHAR* JabberPrepareJid(LPCTSTR jid)
{
	if (jid == NULL) return NULL;
	TCHAR *szNewJid = mir_tstrdup(jid);
	if (!szNewJid) return NULL;
	TCHAR *pDelimiter = _tcschr(szNewJid, _T('/'));
	if (pDelimiter) *pDelimiter = 0;
	CharLower(szNewJid);
	if (pDelimiter) *pDelimiter = _T('/');
	return szNewJid;
}

void __stdcall JabberUrlDecodeW(WCHAR *str)
{
	if (str == NULL)
		return;

	WCHAR *p, *q;
	for (p = q = str; *p != '\0'; p++, q++) {
		if (*p == '&') {
			if (!wcsncmp(p, L"&amp;", 5)) { *q = '&'; p += 4; }
			else if (!wcsncmp(p, L"&apos;", 6)) { *q = '\''; p += 5; }
			else if (!wcsncmp(p, L"&gt;", 4)) { *q = '>'; p += 3; }
			else if (!wcsncmp(p, L"&lt;", 4)) { *q = '<'; p += 3; }
			else if (!wcsncmp(p, L"&quot;", 6)) { *q = '"'; p += 5; }
			else { *q = *p; }
		}
		else {
			*q = *p;
		}
	}
	*q = '\0';
}

char* __stdcall JabberSha1(const char *str, JabberShaStrBuf buf)
{
	BYTE digest[MIR_SHA1_HASH_SIZE];
	mir_sha1_ctx sha;
	mir_sha1_init(&sha);
	mir_sha1_append(&sha, (BYTE*)str, (int)mir_strlen(str));
	mir_sha1_finish(&sha, digest);

	bin2hex(digest, sizeof(digest), buf);
	return buf;
}

TCHAR* __stdcall JabberStrFixLines(const TCHAR *str)
{
	if (str == NULL)
		return NULL;

	const TCHAR *p;
	int add = 0;
	bool prev_r = false;
	bool prev_n = false;

	for (p = str; p && *p; ++p)
		if (*p == _T('\r') || *p == _T('\n'))
			++add;

	TCHAR *buf = (TCHAR *)mir_alloc((mir_tstrlen(str) + add + 1) * sizeof(TCHAR));
	TCHAR *res = buf;

	for (p = str; p && *p; ++p) {
		if (*p == _T('\n') && !prev_r)
			*res++ = _T('\r');
		if (*p != _T('\r') && *p != _T('\n') && prev_r)
			*res++ = _T('\n');
		*res++ = *p;
		prev_r = *p == _T('\r');
		prev_n = *p == _T('\n');
	}
	*res = 0;

	return buf;
}

void __stdcall JabberHttpUrlDecode(TCHAR *str)
{
	TCHAR *p, *q;
	unsigned int code;

	if (str == NULL) return;
	for (p = q = (TCHAR*)str; *p != '\0'; p++, q++) {
		if (*p == '%' && *(p + 1) != '\0' && isxdigit(*(p + 1)) && *(p + 2) != '\0' && isxdigit(*(p + 2))) {
			_stscanf((TCHAR*)p + 1, _T("%2x"), &code);
			*q = (unsigned char)code;
			p += 2;
		}
		else *q = *p;
	}

	*q = '\0';
}

int __stdcall JabberCombineStatus(int status1, int status2)
{
	// Combine according to the following priority (high to low)
	// ID_STATUS_FREECHAT
	// ID_STATUS_ONLINE
	// ID_STATUS_DND
	// ID_STATUS_AWAY
	// ID_STATUS_NA
	// ID_STATUS_INVISIBLE (valid only for TLEN_PLUGIN)
	// ID_STATUS_OFFLINE
	// other ID_STATUS in random order (actually return status1)
	if (status1 == ID_STATUS_FREECHAT || status2 == ID_STATUS_FREECHAT)
		return ID_STATUS_FREECHAT;
	if (status1 == ID_STATUS_ONLINE || status2 == ID_STATUS_ONLINE)
		return ID_STATUS_ONLINE;
	if (status1 == ID_STATUS_DND || status2 == ID_STATUS_DND)
		return ID_STATUS_DND;
	if (status1 == ID_STATUS_AWAY || status2 == ID_STATUS_AWAY)
		return ID_STATUS_AWAY;
	if (status1 == ID_STATUS_NA || status2 == ID_STATUS_NA)
		return ID_STATUS_NA;
	if (status1 == ID_STATUS_INVISIBLE || status2 == ID_STATUS_INVISIBLE)
		return ID_STATUS_INVISIBLE;
	if (status1 == ID_STATUS_OFFLINE || status2 == ID_STATUS_OFFLINE)
		return ID_STATUS_OFFLINE;
	return status1;
}

struct tagErrorCodeToStr
{
	int code;
	TCHAR *str;
}
static JabberErrorCodeToStrMapping[] = {
		{ JABBER_ERROR_REDIRECT, LPGENT("Redirect") },
		{ JABBER_ERROR_BAD_REQUEST, LPGENT("Bad request") },
		{ JABBER_ERROR_UNAUTHORIZED, LPGENT("Unauthorized") },
		{ JABBER_ERROR_PAYMENT_REQUIRED, LPGENT("Payment required") },
		{ JABBER_ERROR_FORBIDDEN, LPGENT("Forbidden") },
		{ JABBER_ERROR_NOT_FOUND, LPGENT("Not found") },
		{ JABBER_ERROR_NOT_ALLOWED, LPGENT("Not allowed") },
		{ JABBER_ERROR_NOT_ACCEPTABLE, LPGENT("Not acceptable") },
		{ JABBER_ERROR_REGISTRATION_REQUIRED, LPGENT("Registration required") },
		{ JABBER_ERROR_REQUEST_TIMEOUT, LPGENT("Request timeout") },
		{ JABBER_ERROR_CONFLICT, LPGENT("Conflict") },
		{ JABBER_ERROR_INTERNAL_SERVER_ERROR, LPGENT("Internal server error") },
		{ JABBER_ERROR_NOT_IMPLEMENTED, LPGENT("Not implemented") },
		{ JABBER_ERROR_REMOTE_SERVER_ERROR, LPGENT("Remote server error") },
		{ JABBER_ERROR_SERVICE_UNAVAILABLE, LPGENT("Service unavailable") },
		{ JABBER_ERROR_REMOTE_SERVER_TIMEOUT, LPGENT("Remote server timeout") },
		{ -1, LPGENT("Unknown error") }
};

TCHAR* __stdcall JabberErrorStr(int errorCode)
{
	int i;

	for (i = 0; JabberErrorCodeToStrMapping[i].code != -1 && JabberErrorCodeToStrMapping[i].code != errorCode; i++);
	return JabberErrorCodeToStrMapping[i].str;
}

TCHAR* __stdcall JabberErrorMsg(HXML errorNode, int* pErrorCode)
{
	TCHAR *errorStr = (TCHAR*)mir_alloc(256 * sizeof(TCHAR));
	if (errorNode == NULL) {
		if (pErrorCode)
			*pErrorCode = -1;
		mir_sntprintf(errorStr, 256, _T("%s -1: %s"), TranslateT("Error"), TranslateT("Unknown error message"));
		return errorStr;
	}

	int errorCode = -1;
	const TCHAR *str = XmlGetAttrValue(errorNode, _T("code"));
	if (str != NULL)
		errorCode = _ttoi(str);

	str = XmlGetText(errorNode);
	if (str == NULL)
		str = XmlGetText(XmlGetChild(errorNode, _T("text")));
	if (str == NULL) {
		for (int i = 0;; i++) {
			HXML c = XmlGetChild(errorNode, i);
			if (c == NULL) break;
			const TCHAR *attr = XmlGetAttrValue(c, _T("xmlns"));
			if (attr && !mir_tstrcmp(attr, _T("urn:ietf:params:xml:ns:xmpp-stanzas"))) {
				str = XmlGetName(c);
				break;
			}
		}
	}

	if (str != NULL)
		mir_sntprintf(errorStr, 256, _T("%s %d: %s\r\n%s"), TranslateT("Error"), errorCode, TranslateTS(JabberErrorStr(errorCode)), str);
	else
		mir_sntprintf(errorStr, 256, _T("%s %d: %s"), TranslateT("Error"), errorCode, TranslateTS(JabberErrorStr(errorCode)));

	if (pErrorCode)
		*pErrorCode = errorCode;
	return errorStr;
}

void CJabberProto::SendVisibleInvisiblePresence(BOOL invisible)
{
	if (!m_bJabberOnline) return;

	LISTFOREACH(i, this, LIST_ROSTER)
	{
		JABBER_LIST_ITEM *item = ListGetItemPtrFromIndex(i);
		if (item == NULL)
			continue;

		MCONTACT hContact = HContactFromJID(item->jid);
		if (hContact == NULL)
			continue;

		WORD apparentMode = getWord(hContact, "ApparentMode", 0);
		if (invisible == TRUE && apparentMode == ID_STATUS_OFFLINE)
			m_ThreadInfo->send(XmlNode(_T("presence")) << XATTR(_T("to"), item->jid) << XATTR(_T("type"), _T("invisible")));
		else if (invisible == FALSE && apparentMode == ID_STATUS_ONLINE)
			SendPresenceTo(m_iStatus, item->jid, NULL);
	}
}

time_t __stdcall JabberIsoToUnixTime(const TCHAR *stamp)
{
	TCHAR date[9];
	int i, y;

	if (stamp == NULL)
		return 0;

	const TCHAR *p = stamp;

	// Get the date part
	for (i = 0; *p != '\0' && i < 8 && isdigit(*p); p++, i++)
		date[i] = *p;

	// Parse year
	if (i == 6) {
		// 2-digit year (1970-2069)
		y = (date[0] - '0') * 10 + (date[1] - '0');
		if (y < 70) y += 100;
	}
	else if (i == 8) {
		// 4-digit year
		y = (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 + date[3] - '0';
		y -= 1900;
	}
	else return 0;

	struct tm timestamp;
	timestamp.tm_year = y;

	// Parse month
	timestamp.tm_mon = (date[i - 4] - '0') * 10 + date[i - 3] - '0' - 1;

	// Parse date
	timestamp.tm_mday = (date[i - 2] - '0') * 10 + date[i - 1] - '0';

	// Skip any date/time delimiter
	for (; *p != '\0' && !isdigit(*p); p++);

	// Parse time
	if (_stscanf(p, _T("%d:%d:%d"), &timestamp.tm_hour, &timestamp.tm_min, &timestamp.tm_sec) != 3)
		return (time_t)0;

	timestamp.tm_isdst = 0;	// DST is already present in _timezone below
	time_t t = mktime(&timestamp);

	_tzset();
	t -= _timezone;
	return (t >= 0) ? t : 0;
}

void CJabberProto::SendPresenceTo(int status, const TCHAR* to, HXML extra, const TCHAR *msg)
{
	if (!m_bJabberOnline) return;

	// Send <presence/> update for status (we won't handle ID_STATUS_OFFLINE here)
	int iPriority = getDword("Priority", 0);
	UpdatePriorityMenu(iPriority);

	TCHAR szPriority[40];
	_itot(iPriority, szPriority, 10);

	XmlNode p(_T("presence")); p << XCHILD(_T("priority"), szPriority);
	if (to != NULL)
		p << XATTR(_T("to"), to);

	if (extra)
		XmlAddChild(p, extra);

	// XEP-0115:Entity Capabilities
	HXML c = p << XCHILDNS(_T("c"), JABBER_FEAT_ENTITY_CAPS) << XATTR(_T("node"), JABBER_CAPS_MIRANDA_NODE)
		<< XATTR(_T("ver"), szCoreVersion);

	LIST<TCHAR> arrExtCaps(5);
	if (bSecureIM)
		arrExtCaps.insert(JABBER_EXT_SECUREIM);

	if (bMirOTR)
		arrExtCaps.insert(JABBER_EXT_MIROTR);

	if (bNewGPG)
		arrExtCaps.insert(JABBER_EXT_NEWGPG);

	if (bPlatform)
		arrExtCaps.insert(JABBER_EXT_PLATFORMX64);
	else
		arrExtCaps.insert(JABBER_EXT_PLATFORMX86);

	if (m_options.EnableRemoteControl)
		arrExtCaps.insert(JABBER_EXT_COMMANDS);

	if (m_options.EnableUserMood)
		arrExtCaps.insert(JABBER_EXT_USER_MOOD);

	if (m_options.EnableUserTune)
		arrExtCaps.insert(JABBER_EXT_USER_TUNE);

	if (m_options.EnableUserActivity)
		arrExtCaps.insert(JABBER_EXT_USER_ACTIVITY);

	if (m_options.AcceptNotes)
		arrExtCaps.insert(JABBER_EXT_MIR_NOTES);

	NotifyFastHook(hExtListInit, (WPARAM)&arrExtCaps, (LPARAM)(IJabberInterface*)this);

	// add features enabled through IJabberNetInterface::AddFeatures()
	for (int i = 0; i < m_lstJabberFeatCapPairsDynamic.getCount(); i++)
		if (m_uEnabledFeatCapsDynamic & m_lstJabberFeatCapPairsDynamic[i]->jcbCap)
			arrExtCaps.insert(m_lstJabberFeatCapPairsDynamic[i]->szExt);

	if (arrExtCaps.getCount()) {
		CMString szExtCaps = arrExtCaps[0];
		for (int i = 1; i < arrExtCaps.getCount(); i++) {
			szExtCaps.AppendChar(' ');
			szExtCaps += arrExtCaps[i];
		}
		XmlAddAttr(c, _T("ext"), szExtCaps);
	}

	if (m_options.EnableAvatars) {
		HXML x = p << XCHILDNS(_T("x"), _T("vcard-temp:x:update"));

		ptrA hashValue(getStringA("AvatarHash"));
		if (hashValue != NULL) // XEP-0153: vCard-Based Avatars
			x << XCHILD(_T("photo"), _A2T(hashValue));
		else
			x << XCHILD(_T("photo"));
	}
	{
		mir_cslock lck(m_csModeMsgMutex);
		switch (status) {
		case ID_STATUS_ONLINE:
			if (!msg) msg = m_modeMsgs.szOnline;
			break;
		case ID_STATUS_INVISIBLE:
			p << XATTR(_T("type"), _T("invisible"));
			break;
		case ID_STATUS_AWAY:
		case ID_STATUS_ONTHEPHONE:
		case ID_STATUS_OUTTOLUNCH:
			p << XCHILD(_T("show"), _T("away"));
			if (!msg) msg = m_modeMsgs.szAway;
			break;
		case ID_STATUS_NA:
			p << XCHILD(_T("show"), _T("xa"));
			if (!msg) msg = m_modeMsgs.szNa;
			break;
		case ID_STATUS_DND:
		case ID_STATUS_OCCUPIED:
			p << XCHILD(_T("show"), _T("dnd"));
			if (!msg) msg = m_modeMsgs.szDnd;
			break;
		case ID_STATUS_FREECHAT:
			p << XCHILD(_T("show"), _T("chat"));
			if (!msg) msg = m_modeMsgs.szFreechat;
			break;
		default: // Should not reach here
			break;
		}

		if (msg)
			p << XCHILD(_T("status"), msg);
	}

	m_ThreadInfo->send(p);
}

void CJabberProto::SendPresence(int status, bool bSendToAll)
{
	SendPresenceTo(status, NULL, NULL);
	SendVisibleInvisiblePresence(status == ID_STATUS_INVISIBLE);

	// Also update status in all chatrooms
	if (bSendToAll) {
		LISTFOREACH(i, this, LIST_CHATROOM)
		{
			JABBER_LIST_ITEM *item = ListGetItemPtrFromIndex(i);
			if (item != NULL) {
				TCHAR text[1024];
				mir_sntprintf(text, _T("%s/%s"), item->jid, item->nick);
				SendPresenceTo(status == ID_STATUS_INVISIBLE ? ID_STATUS_ONLINE : status, text, NULL);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// JabberGetPacketID - converts the xml id attribute into an integer

int __stdcall JabberGetPacketID(HXML n)
{
	const TCHAR *str = XmlGetAttrValue(n, _T("id"));
	if (str)
		if (!_tcsncmp(str, _T(JABBER_IQID), _countof(JABBER_IQID) - 1))
			return _ttoi(str + _countof(JABBER_IQID) - 1);

	return -1;
}

TCHAR* __stdcall JabberId2string(int id)
{
	TCHAR text[100];
	mir_sntprintf(text, _T(JABBER_IQID) _T("%d"), id);
	return mir_tstrdup(text);
}

///////////////////////////////////////////////////////////////////////////////
// JabberGetClientJID - adds a resource postfix to a JID

TCHAR* CJabberProto::GetClientJID(MCONTACT hContact, TCHAR *dest, size_t destLen)
{
	if (hContact == NULL)
		return NULL;

	ptrT jid(getTStringA(hContact, "jid"));
	return GetClientJID(jid, dest, destLen);
}

TCHAR* CJabberProto::GetClientJID(const TCHAR *jid, TCHAR *dest, size_t destLen)
{
	if (jid == NULL)
		return NULL;

	_tcsncpy_s(dest, destLen, jid, _TRUNCATE);
	TCHAR *p = _tcschr(dest, '/');

	mir_cslock lck(m_csLists);
	JABBER_LIST_ITEM *LI = ListGetItemPtr(LIST_ROSTER, jid);
	if (LI != NULL) {
		if (LI->arResources.getCount() == 1 && !mir_tstrcmp(LI->arResources[0]->m_tszCapsNode, _T("http://talk.google.com/xmpp/bot/caps"))) {
			if (p) *p = 0;
			return dest;
		}

		if (p == NULL) {
			pResourceStatus r(LI->getBestResource());
			if (r != NULL)
				mir_sntprintf(dest, destLen, _T("%s/%s"), jid, r->m_tszResourceName);
		}
	}

	return dest;
}

///////////////////////////////////////////////////////////////////////////////
// JabberStripJid - strips a resource postfix from a JID

TCHAR* __stdcall JabberStripJid(const TCHAR *jid, TCHAR *dest, size_t destLen)
{
	if (jid == NULL)
		*dest = 0;
	else {
		_tcsncpy_s(dest, destLen, jid, _TRUNCATE);

		TCHAR *p = _tcschr(dest, '/');
		if (p != NULL)
			*p = 0;
	}

	return dest;
}

/////////////////////////////////////////////////////////////////////////////////////////
// JabberGetPictureType - tries to autodetect the picture type from the buffer

LPCTSTR __stdcall JabberGetPictureType(HXML node, const char *picBuf)
{
	if (LPCTSTR ptszType = XmlGetText(XmlGetChild(node, "TYPE")))
		if (!mir_tstrcmp(ptszType, _T("image/jpeg")) ||
			 !mir_tstrcmp(ptszType, _T("image/png")) ||
			 !mir_tstrcmp(ptszType, _T("image/gif")) ||
			 !mir_tstrcmp(ptszType, _T("image/bmp")))
			return ptszType;

	switch (ProtoGetBufferFormat(picBuf)) {
		case PA_FORMAT_GIF:	return _T("image/gif");
		case PA_FORMAT_BMP:  return _T("image/bmp");
		case PA_FORMAT_PNG:  return _T("image/png");
		case PA_FORMAT_JPEG: return _T("image/jpeg");
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////
// TStringPairs class members

TStringPairs::TStringPairs(char* buffer) :
	elems(NULL)
{
	TStringPairsElem tempElem[100];

	char* token = strtok(buffer, ",");

	for (numElems = 0; token != NULL; numElems++) {
		char* p = strchr(token, '='), *p1;
		if (p == NULL)
			break;

		while (isspace(*token))
			token++;

		tempElem[numElems].name = rtrim(token);
		*p++ = 0;
		if ((p1 = strchr(p, '\"')) != NULL) {
			*p1 = 0;
			p = p1 + 1;
		}

		if ((p1 = strrchr(p, '\"')) != NULL)
			*p1 = 0;

		tempElem[numElems].value = rtrim(p);
		token = strtok(NULL, ",");
	}

	if (numElems) {
		elems = new TStringPairsElem[numElems];
		memcpy(elems, tempElem, sizeof(tempElem[0]) * numElems);
	}
}

TStringPairs::~TStringPairs()
{
	delete[] elems;
}

const char* TStringPairs::operator[](const char* key) const
{
	for (int i = 0; i < numElems; i++)
		if (!mir_strcmp(elems[i].name, key))
			return elems[i].value;

	return "";
}

////////////////////////////////////////////////////////////////////////
// Manage combo boxes with recent item list

void CJabberProto::ComboLoadRecentStrings(HWND hwndDlg, UINT idcCombo, char *param, int recentCount)
{
	for (int i = 0; i < recentCount; i++) {
		char setting[MAXMODULELABELLENGTH];
		mir_snprintf(setting, "%s%d", param, i);
		ptrT tszRecent(getTStringA(setting));
		if (tszRecent != NULL)
			SendDlgItemMessage(hwndDlg, idcCombo, CB_ADDSTRING, 0, tszRecent);
	}

	if (!SendDlgItemMessage(hwndDlg, idcCombo, CB_GETCOUNT, 0, 0))
		SendDlgItemMessage(hwndDlg, idcCombo, CB_ADDSTRING, 0, (LPARAM)_T(""));
}

void CJabberProto::ComboAddRecentString(HWND hwndDlg, UINT idcCombo, char *param, const TCHAR *string, int recentCount)
{
	if (!string || !*string)
		return;
	if (SendDlgItemMessage(hwndDlg, idcCombo, CB_FINDSTRING, (WPARAM)-1, (LPARAM)string) != CB_ERR)
		return;

	int id;
	SendDlgItemMessage(hwndDlg, idcCombo, CB_ADDSTRING, 0, (LPARAM)string);
	if ((id = SendDlgItemMessage(hwndDlg, idcCombo, CB_FINDSTRING, (WPARAM)-1, (LPARAM)_T(""))) != CB_ERR)
		SendDlgItemMessage(hwndDlg, idcCombo, CB_DELETESTRING, id, 0);

	id = getByte(param, 0);
	char setting[MAXMODULELABELLENGTH];
	mir_snprintf(setting, "%s%d", param, id);
	setTString(setting, string);
	setByte(param, (id + 1) % recentCount);
}

/////////////////////////////////////////////////////////////////////////////////////////
// jabber frame maintenance code

static VOID CALLBACK sttRebuildInfoFrameApcProc(void* param)
{
	CJabberProto *ppro = (CJabberProto *)param;
	if (!ppro->m_pInfoFrame)
		return;

	ppro->m_pInfoFrame->LockUpdates();
	if (!ppro->m_bJabberOnline) {
		ppro->m_pInfoFrame->RemoveInfoItem("$/PEP");
		ppro->m_pInfoFrame->RemoveInfoItem("$/Transports");
		ppro->m_pInfoFrame->UpdateInfoItem("$/JID", Skin_GetIconHandle(SKINICON_OTHER_USERDETAILS), TranslateT("Offline"));
	}
	else {
		ppro->m_pInfoFrame->UpdateInfoItem("$/JID", Skin_GetIconHandle(SKINICON_OTHER_USERDETAILS), ppro->m_szJabberJID);

		if (!ppro->m_bPepSupported)
			ppro->m_pInfoFrame->RemoveInfoItem("$/PEP");
		else {
			ppro->m_pInfoFrame->RemoveInfoItem("$/PEP/");
			ppro->m_pInfoFrame->CreateInfoItem("$/PEP", false);
			ppro->m_pInfoFrame->UpdateInfoItem("$/PEP", ppro->GetIconHandle(IDI_PL_LIST_ANY), TranslateT("Advanced Status"));

			ppro->m_pInfoFrame->CreateInfoItem("$/PEP/mood", true);
			ppro->m_pInfoFrame->SetInfoItemCallback("$/PEP/mood", &CJabberProto::InfoFrame_OnUserMood);
			ppro->m_pInfoFrame->UpdateInfoItem("$/PEP/mood", Skin_GetIconHandle(SKINICON_OTHER_SMALLDOT), TranslateT("Set mood..."));

			ppro->m_pInfoFrame->CreateInfoItem("$/PEP/activity", true);
			ppro->m_pInfoFrame->SetInfoItemCallback("$/PEP/activity", &CJabberProto::InfoFrame_OnUserActivity);
			ppro->m_pInfoFrame->UpdateInfoItem("$/PEP/activity", Skin_GetIconHandle(SKINICON_OTHER_SMALLDOT), TranslateT("Set activity..."));
		}

		ppro->m_pInfoFrame->RemoveInfoItem("$/Transports/");
		ppro->m_pInfoFrame->CreateInfoItem("$/Transports", false);
		ppro->m_pInfoFrame->UpdateInfoItem("$/Transports", ppro->GetIconHandle(IDI_TRANSPORT), TranslateT("Transports"));

		JABBER_LIST_ITEM *item = NULL;
		LISTFOREACH(i, ppro, LIST_ROSTER)
		{
			if ((item = ppro->ListGetItemPtrFromIndex(i)) != NULL) {
				if (_tcschr(item->jid, '@') == NULL && _tcschr(item->jid, '/') == NULL && item->subscription != SUB_NONE) {
					MCONTACT hContact = ppro->HContactFromJID(item->jid);
					if (hContact == NULL) continue;

					char name[128];
					char *jid_copy = mir_t2a(item->jid);
					mir_snprintf(name, "$/Transports/%s", jid_copy);
					ppro->m_pInfoFrame->CreateInfoItem(name, true, hContact);
					ppro->m_pInfoFrame->UpdateInfoItem(name, ppro->GetIconHandle(IDI_TRANSPORTL), (TCHAR *)item->jid);
					ppro->m_pInfoFrame->SetInfoItemCallback(name, &CJabberProto::InfoFrame_OnTransport);
					mir_free(jid_copy);
				}
			}
		}
	}
	ppro->m_pInfoFrame->Update();
}

void CJabberProto::RebuildInfoFrame()
{
	if (!m_bShutdown)
		CallFunctionAsync(sttRebuildInfoFrameApcProc, this);
}

////////////////////////////////////////////////////////////////////////
// time2str & str2time

TCHAR* time2str(time_t _time, TCHAR *buf, size_t bufLen)
{
	struct tm* T = localtime(&_time);
	mir_sntprintf(buf, bufLen, _T("%04d-%02d-%02dT%02d:%02d:%02dZ"),
					  T->tm_year + 1900, T->tm_mon + 1, T->tm_mday, T->tm_hour, T->tm_min, T->tm_sec);
	return buf;
}

time_t str2time(const TCHAR *buf)
{
	struct tm T = { 0 };
	if (_stscanf(buf, _T("%04d-%02d-%02dT%02d:%02d:%02dZ"), &T.tm_year, &T.tm_mon, &T.tm_mday, &T.tm_hour, &T.tm_min, &T.tm_sec) != 6) {
		int boo;
		if (_stscanf(buf, _T("%04d-%02d-%02dT%02d:%02d:%02d.%dZ"), &T.tm_year, &T.tm_mon, &T.tm_mday, &T.tm_hour, &T.tm_min, &T.tm_sec, &boo) != 7)
			return 0;
	}

	T.tm_year -= 1900;
	T.tm_mon--;
	return mktime(&T);
}

////////////////////////////////////////////////////////////////////////
// case-insensitive _tcsstr
const TCHAR *JabberStrIStr(const TCHAR *str, const TCHAR *substr)
{
	TCHAR *str_up = NEWTSTR_ALLOCA(str);
	TCHAR *substr_up = NEWTSTR_ALLOCA(substr);

	CharUpperBuff(str_up, (DWORD)mir_tstrlen(str_up));
	CharUpperBuff(substr_up, (DWORD)mir_tstrlen(substr_up));

	TCHAR *p = _tcsstr(str_up, substr_up);
	return p ? (str + (p - str_up)) : NULL;
}

////////////////////////////////////////////////////////////////////////
// clipboard processing

void JabberCopyText(HWND hwnd, const TCHAR *text)
{
	if (!hwnd || !text) return;

	if (OpenClipboard(hwnd)) {
		EmptyClipboard();
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(TCHAR)*(mir_tstrlen(text) + 1));
		TCHAR *s = (TCHAR *)GlobalLock(hMem);
		mir_tstrcpy(s, text);
		GlobalUnlock(hMem);
		SetClipboardData(CF_UNICODETEXT, hMem);
		CloseClipboard();
	}
}

BOOL CJabberProto::EnterString(CMString &result, LPCTSTR caption, int type, char *windowName, int recentCount, int timeout)
{
	if (caption == NULL) {
		caption = NEWTSTR_ALLOCA(result.GetString());
		result.Empty();
	}

	ENTER_STRING param = { sizeof(param) };
	param.type = type;
	param.caption = caption;
	param.szModuleName = m_szModuleName;
	param.szDataPrefix = windowName;
	param.recentCount = recentCount;
	param.timeout = timeout;
	param.ptszInitVal = result;
	BOOL res = ::EnterString(&param);
	if (res) {
		result = param.ptszResult;
		mir_free(param.ptszResult);
	}
	return res;
}

/////////////////////////////////////////////////////////////////////////////////////////
// XEP-0203 delay support

bool JabberReadXep203delay(HXML node, time_t &msgTime)
{
	HXML n = XmlGetChildByTag(node, "delay", "xmlns", _T("urn:xmpp:delay"));
	if (n == NULL)
		return false;

	const TCHAR *ptszTimeStamp = XmlGetAttrValue(n, _T("stamp"));
	if (ptszTimeStamp == NULL)
		return false;

	// skip '-' chars
	TCHAR *szStamp = NEWTSTR_ALLOCA(ptszTimeStamp);
	int si = 0, sj = 0;
	while (true) {
		if (szStamp[si] == _T('-'))
			si++;
		else if (!(szStamp[sj++] = szStamp[si++]))
			break;
	};
	msgTime = JabberIsoToUnixTime(szStamp);
	return msgTime != 0;
}

bool CJabberProto::IsMyOwnJID(LPCTSTR szJID)
{
	if (m_ThreadInfo == NULL)
		return false;

	ptrT szFrom(JabberPrepareJid(szJID));
	if (szFrom == NULL)
		return false;

	ptrT szTo(JabberPrepareJid(m_ThreadInfo->fullJID));
	if (szTo == NULL)
		return false;

	TCHAR *pDelimiter = _tcschr(szFrom, _T('/'));
	if (pDelimiter)
		*pDelimiter = 0;

	pDelimiter = _tcschr(szTo, _T('/'));
	if (pDelimiter)
		*pDelimiter = 0;

	return mir_tstrcmp(szFrom, szTo) == 0;
}

void __cdecl CJabberProto::LoadHttpAvatars(void* param)
{
	OBJLIST<JABBER_HTTP_AVATARS> &avs = *(OBJLIST<JABBER_HTTP_AVATARS>*)param;
	HANDLE hHttpCon = NULL;
	for (int i = 0; i < avs.getCount(); i++) {
		NETLIBHTTPREQUEST nlhr = { 0 };
		nlhr.cbSize = sizeof(nlhr);
		nlhr.requestType = REQUEST_GET;
		nlhr.flags = NLHRF_HTTP11 | NLHRF_REDIRECT | NLHRF_PERSISTENT;
		nlhr.szUrl = avs[i].Url;
		nlhr.nlc = hHttpCon;

		NETLIBHTTPREQUEST * res = (NETLIBHTTPREQUEST*)CallService(MS_NETLIB_HTTPTRANSACTION, (WPARAM)m_hNetlibUser, (LPARAM)&nlhr);
		if (res) {
			hHttpCon = res->nlc;
			if (res->resultCode == 200 && res->dataLength) {
				int pictureType = ProtoGetBufferFormat(res->pData);
				if (pictureType != PA_FORMAT_UNKNOWN) {
					PROTO_AVATAR_INFORMATION ai;
					ai.format = pictureType;
					ai.hContact = avs[i].hContact;

					if (getByte(ai.hContact, "AvatarType", PA_FORMAT_UNKNOWN) != (unsigned char)pictureType) {
						TCHAR tszFileName[MAX_PATH];
						GetAvatarFileName(ai.hContact, tszFileName, _countof(tszFileName));
						DeleteFile(tszFileName);
					}

					setByte(ai.hContact, "AvatarType", pictureType);

					char buffer[2 * MIR_SHA1_HASH_SIZE + 1];
					BYTE digest[MIR_SHA1_HASH_SIZE];
					mir_sha1_ctx sha;
					mir_sha1_init(&sha);
					mir_sha1_append(&sha, (BYTE*)res->pData, res->dataLength);
					mir_sha1_finish(&sha, digest);
					bin2hex(digest, sizeof(digest), buffer);

					ptrA cmpsha(getStringA(ai.hContact, "AvatarSaved"));
					if (cmpsha == NULL || strnicmp(cmpsha, buffer, sizeof(buffer))) {
						TCHAR tszFileName[MAX_PATH];
						GetAvatarFileName(ai.hContact, tszFileName, _countof(tszFileName));
						_tcsncpy_s(ai.filename, tszFileName, _TRUNCATE);
						FILE* out = _tfopen(tszFileName, _T("wb"));
						if (out != NULL) {
							fwrite(res->pData, res->dataLength, 1, out);
							fclose(out);
							setString(ai.hContact, "AvatarSaved", buffer);
							ProtoBroadcastAck(ai.hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, &ai, 0);
							debugLog(_T("Broadcast new avatar: %s"), ai.filename);
						}
						else ProtoBroadcastAck(ai.hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, &ai, 0);
					}
				}
			}
			CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, 0, (LPARAM)res);
		}
		else hHttpCon = NULL;
	}
	delete &avs;
	if (hHttpCon)
		Netlib_CloseHandle(hHttpCon);
}

/////////////////////////////////////////////////////////////////////////////////////////
// UI utilities

int UIEmulateBtnClick(HWND hwndDlg, UINT idcButton)
{
	if (IsWindowEnabled(GetDlgItem(hwndDlg, idcButton)))
		PostMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(idcButton, BN_CLICKED), (LPARAM)GetDlgItem(hwndDlg, idcButton));
	return 0;
}

void UIShowControls(HWND hwndDlg, int *idList, int nCmdShow)
{
	for (; *idList; ++idList)
		ShowWindow(GetDlgItem(hwndDlg, *idList), nCmdShow);
}
