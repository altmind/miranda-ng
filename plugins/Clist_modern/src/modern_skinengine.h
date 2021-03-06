#pragma once

#ifndef ske_H_INC
#define ske_H_INC

/* Definitions */
#define GetAValue(argb)((BYTE)((argb)>>24))

#define DEFAULTSKINSECTION  "ModernSkin"



#define MAX_BUFF_SIZE       255*400
#define MAXSN_BUFF_SIZE     255*1000

/* External variables */

/* Structs */

struct SKINOBJECTSLIST
{
	DWORD dwObjLPReserved;
	DWORD dwObjLPAlocated;
	wchar_t *szSkinPlace;
	LISTMODERNMASK	*pMaskList;
	SKINOBJECTDESCRIPTOR  *pObjects;
	SortedList *pTextList;
};

struct GLYPHIMAGE
{
	wchar_t  *szFileName;
	DWORD   dwLoadedTimes;
	HBITMAP hGlyph;
	BYTE    isSemiTransp;
};

typedef GLYPHIMAGE *LPGLYPHIMAGE;

struct CURRWNDIMAGEDATA
{
	HDC hImageDC;
	HDC hBackDC;
	HDC hScreenDC;
	HBITMAP hImageDIB, hImageOld;
	HBITMAP hBackDIB, hBackOld;
	BYTE * hImageDIBByte;
	BYTE * hBackDIBByte;
	int Width, Height;
};

struct EFFECTSSTACKITEM
{
	HDC hdc;
	BYTE EffectID;
	DWORD FirstColor;
	DWORD SecondColor;
};

class IniParser
{
public:
	enum {
		FLAG_WITH_SETTINGS = 0,
		FLAG_ONLY_OBJECTS = 1,
	};

	enum { IT_UNKNOWN, IT_FILE, IT_RESOURCE };

	typedef HRESULT(*ParserCallback_t)(const char *szSection, const char *szKey, const char *szValue, IniParser *This);

	IniParser(wchar_t *szFileName, BYTE flags = FLAG_WITH_SETTINGS);
	IniParser(HINSTANCE hInst, const char *resourceName, const char *resourceType, BYTE flags = FLAG_ONLY_OBJECTS);
	~IniParser();

	bool CheckOK() { return _isValid; }
	HRESULT Parse(ParserCallback_t pLineCallBackProc, LPARAM lParam);

	static HRESULT WriteStrToDb(const char *szSection, const char *szKey, const char *szValue, IniParser *This);
	static int GetSkinFolder(IN const wchar_t *szFileName, OUT wchar_t *pszFolderName);

private:
	// common
	enum { MAX_LINE_LEN = 512 };
	int		 _eType;
	bool	_isValid;
	char *	_szSection;
	ParserCallback_t _pLineCallBackProc;
	BOOL   _SecCheck;
	int		_nLine;

	void _DoInit();
	BOOL _DoParseLine(char *szLine);

	// Processing File
	HRESULT _DoParseFile();
	FILE*	_hFile;

	// Processing resource
	void _LoadResourceIni(HINSTANCE hInst, const char *resourceName, const char *resourceType);
	HRESULT _DoParseResource();
	const char* _RemoveTailings(const char *szLine, size_t &len);

	HGLOBAL _hGlobalRes;
	DWORD   _dwSizeOfRes;
	char*	_pPosition;

	BYTE _Flags;
};


int ske_UnloadSkin(SKINOBJECTSLIST * Skin);
int ske_AddDescriptorToSkinObjectList(SKINOBJECTDESCRIPTOR *lpDescr, SKINOBJECTSLIST* Skin);
INT_PTR ske_Service_DrawGlyph(WPARAM wParam, LPARAM lParam);



#endif

