#include "stdafx.h"
#include "mu_common.h"

#include <map>
#include <set>

// {7649A9F5-E57F-4E5D-A715-7BD8B3496DBE}
static MUUID menu_id = { 0x7649a9f5, 0xe57f, 0x4e5d, 0xa7, 0x15, 0x7b, 0xd8, 0xb3, 0x49, 0x6d, 0xbe };

namespace mu
{
	/*
	 * clist
	 */

	namespace clist
	{
		HGENMENU addMainMenuItem(const wchar_t* pszName, DWORD flags, int position, HICON hIcon, const char* pszService, HGENMENU hRoot)
		{
			// TODO: support for unicode-core with unicode-aware CList
			CMenuItem mi;
			mi.name.w = (wchar_t*)pszName;
			mi.flags = flags | CMIF_UNICODE;
			mi.position = position;
			mi.hIcolibItem = hIcon;
			mi.pszService = const_cast<char*>(pszService);
			mi.root = hRoot;
			mi.uid = menu_id; menu_id.d[7]++;
			return Menu_AddMainMenuItem(&mi);
		}

		HGENMENU addContactMenuItem(const wchar_t* pszName, DWORD flags, int position, HICON hIcon, const char* pszService)
		{
			// TODO: support for unicode-core with unicode-aware CList
			CMenuItem mi;
			mi.name.w = (wchar_t*)pszName;
			mi.flags = flags | CMIF_UNICODE;
			mi.position = position;
			mi.hIcolibItem = hIcon;
			mi.pszService = const_cast<char*>(pszService);
			mi.uid = menu_id; menu_id.d[7]++;
			return Menu_AddContactMenuItem(&mi);
		}
	}

	/*
	 * db
	 */

	namespace db
	{
		int getProfilePath(int cbName, wchar_t* pszName)
		{
			return CallService(MS_DB_GETPROFILEPATHW, cbName, reinterpret_cast<LPARAM>(pszName));
		}

		int getProfileName(int cbName, wchar_t* pszName)
		{
			return CallService(MS_DB_GETPROFILENAMEW, cbName, reinterpret_cast<LPARAM>(pszName));
		}

		void setSafetyMode(bool safetyMode)
		{
			CallService(MS_DB_SETSAFETYMODE, BOOL_(safetyMode), 0);
		}
	}

	/*
	 * db_time
	 */

	namespace db_contact
	{
		int enumSettings(MCONTACT hContact, const char* szModule, DBSETTINGENUMPROC pEnumProc, LPARAM lProcParam)
		{
			DBCONTACTENUMSETTINGS dbces;

			dbces.pfnEnumProc = pEnumProc;
			dbces.lParam = lProcParam;
			dbces.szModule = szModule;
			dbces.ofsSettings = 0;

			return CallService(MS_DB_CONTACT_ENUMSETTINGS, hContact, reinterpret_cast<LPARAM>(&dbces));
		}
	}

	/*
	 * icolib
	 */

	namespace icolib
	{
		bool _available()
		{
			return true;
		}

		void addIcon(const wchar_t* szSection, const wchar_t* szDescription, const char* szIconName, const char* szDefaultFile, int iDefaultIndex)
		{
			SKINICONDESC sid = { 0 };
			sid.section.w = const_cast<wchar_t*>(szSection);
			sid.description.w = const_cast<wchar_t*>(szDescription);
			sid.pszName = const_cast<char*>(szIconName);
			sid.defaultFile.a = const_cast<char*>(szDefaultFile);
			sid.iDefaultIndex = iDefaultIndex;
			sid.flags = SIDF_UNICODE;
			IcoLib_AddIcon(&sid);
		}

		HICON getIcon(const char* szIconName)
		{
			return IcoLib_GetIcon(szIconName);
		}
	}

	/*
	 * opt
	 */

	namespace opt
	{
		void addPage(WPARAM addInfo, const wchar_t* pszGroup, const wchar_t* pszTitle, const wchar_t* pszTab, DLGPROC pfnDlgProc, const char* pszTemplate, HINSTANCE hInstance, DWORD flags /* = ODPF_BOLDGROUPS */)
		{
			OPTIONSDIALOGPAGE odp = { 0 };
			odp.pwszTitle = const_cast<wchar_t*>(pszTitle);
			odp.pfnDlgProc = pfnDlgProc;
			odp.pszTemplate = const_cast<char*>(pszTemplate);
			odp.hInstance = hInstance;
			odp.pwszGroup = const_cast<wchar_t*>(pszGroup);
			odp.flags = flags | ODPF_UNICODE;
			odp.pwszTab = const_cast<wchar_t*>(pszTab);
			Options_AddPage(addInfo, &odp);
		}
	}

	/*
	 * protosvc
	 */

	namespace protosvc
	{
		DWORD getCaps(const char* szProto, int flagNum)
		{
			return (DWORD)CallProtoService(szProto, PS_GETCAPS, static_cast<WPARAM>(flagNum), 0);
		}

		HICON loadIcon(const char* szProto, int whichIcon)
		{
			return reinterpret_cast<HICON>(CallProtoService(szProto, PS_LOADICON, static_cast<WPARAM>(whichIcon), 0));
		}
	}

	/*
	 * system
	 */

	namespace system
	{
		DWORD getVersion()
		{
			return static_cast<DWORD>(CallService(MS_SYSTEM_GETVERSION, 0, 0));
		}

		int getVersionText(int cchVersion, char* szVersion)
		{
			return CallService(MS_SYSTEM_GETVERSIONTEXT, cchVersion, reinterpret_cast<LPARAM>(szVersion));
		}

		int terminated()
		{
			return CallService(MS_SYSTEM_TERMINATED, 0, 0);
		}
	}

	/*
	 * core interface functions
	 */

	bool load()
	{
		// check for version
		if (!isMirandaVersionOk(system::getVersion()))
			return false;

		return true;
	}

	void unload()
	{}

	DWORD getMinimalMirandaVersion()
	{
		// MEMO: version dependency check
		return PLUGIN_MAKE_VERSION(0, 6, 7, 0);
	}

	bool isMirandaVersionOk(DWORD version)
	{
		return (version >= getMinimalMirandaVersion());
	}

	bool isMirandaUnicode()
	{
		if (system::getVersion() < PLUGIN_MAKE_VERSION(0, 4, 3, 33))
			return false;

		char szVersion[256] = { 0 };

		if (system::getVersionText(256, szVersion) != 0)
			return false;

		if (!strstr(szVersion, "Unicode"))
			return false;

		return true;
	}

	/*
	 * string handling
	 */

	char* wideToAnsiDup(const WCHAR* pszWide, UINT uCP /* = CP_ACP */)
	{
		if (!pszWide)
			return NULL;

		int len = WideCharToMultiByte(uCP, 0, pszWide, -1, NULL, 0, NULL, NULL);
		char* result = reinterpret_cast<char*>(malloc(sizeof(char)* len));
		if (!result)
			return NULL;

		WideCharToMultiByte(uCP, 0, pszWide, -1, result, len, NULL, NULL);
		result[len - 1] = 0;
		return result;
	}

	WCHAR* ansiToWideDup(const char* pszAnsi, UINT uCP /* = CP_ACP */)
	{
		if (!pszAnsi)
			return NULL;

		int len = MultiByteToWideChar(uCP, 0, pszAnsi, -1, NULL, 0);
		WCHAR* result = reinterpret_cast<WCHAR*>(malloc(sizeof(WCHAR)* len));
		if (!result)
			return NULL;

		MultiByteToWideChar(uCP, 0, pszAnsi, -1, result, len);
		result[len - 1] = 0;
		return result;
	}

	char* wideToAnsi(const WCHAR* pszWide, char* pszRes, int maxLen, UINT uCP /* = CP_ACP */)
	{
		if (!pszWide)
			return NULL;

		WideCharToMultiByte(uCP, 0, pszWide, -1, pszRes, maxLen, NULL, NULL);
		return pszRes;
	}

	WCHAR* ansiToWide(const char* pszAnsi, WCHAR* pszRes, int maxLen, UINT uCP /* = CP_ACP */)
	{
		if (!pszAnsi)
			return NULL;

		MultiByteToWideChar(uCP, 0, pszAnsi, -1, pszRes, maxLen);
		return pszRes;
	}
}
