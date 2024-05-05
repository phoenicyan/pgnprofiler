#pragma once
//
// RegUtil.h
//
// Methods for loading/storing application settings in registry.
//
// Example:
//	BOOL CMainWindow::LoadSettings() 
//	{
//		HKEY hOptionsKey = NULL;
//		memset(&m_wndPlacement,	0, sizeof(m_wndPlacement));
//
//		//Open the key to make things a little quicker and direct...
//		TESTC_(OpenRegKey(HKEY_CURRENT_USER, wszOPTIONS_KEY, 0, KEY_READ, &hOptionsKey),S_OK);
//
//		//Window Positions
//		GetRegEntry(hOptionsKey, NULL, L"WinPosition",		&m_wndPlacement,	sizeof(m_wndPlacement), NULL);
//		
//	CLEANUP:
//		CloseRegKey(hOptionsKey);
//		return TRUE;
//	}
//
//	BOOL CMainWindow::SaveSettings() 
//	{
//		HKEY hOptionsKey = NULL;
//
//		//Open the key to make things a little quicker and direct...
//		TESTC_(CreateRegKey(HKEY_CURRENT_USER, wszOPTIONS_KEY, &hOptionsKey),S_OK);
//		
//		//Window Positions
//		if(GetWindowPlacement())
//			SetRegEntry(hOptionsKey, NULL, L"WinPosition",		&m_wndPlacement,	sizeof(m_wndPlacement));
//
//	CLEANUP:
//		CloseRegKey(hOptionsKey);
//		return TRUE;
//	}

#define MAX_NAME_LEN			256

#define TESTC(hr)					{ if(FAILED(hr)) goto CLEANUP;				}
#define TESTC_(hr, hrExpected)		{ if((hr) != (hrExpected)) goto CLEANUP;	}

#define NUMELE(rgEle) (sizeof(rgEle) / sizeof(rgEle[0]))
#define __WIDESTRING(str) L##str
#define WIDESTRING(str) __WIDESTRING(str)

#define SAFE_ALLOC(p, type, cb)		{ (p) = (type*)CoTaskMemAlloc(ULONG((cb)*sizeof(type))); 	}
#define SAFE_FREE(p)				if(p) { CoTaskMemFree((void*)p); (p) = NULL;	}


HRESULT		CreateRegKey(HKEY hRootKey, WCHAR* pwszKeyName, HKEY* phKey, REGSAM samDesired = KEY_READ | KEY_WRITE);
HRESULT		CreateRegKeyA(HKEY hRootKey, char* pwszKeyName, HKEY* phKey, REGSAM samDesired = KEY_READ | KEY_WRITE);
HRESULT		OpenRegKey(HKEY hRootKey, WCHAR* pwszKeyName, DWORD ulOptions, REGSAM samDesired, HKEY* phKey);
HRESULT		OpenRegKeyA(HKEY hRootKey, char* pwszKeyName, DWORD ulOptions, REGSAM samDesired, HKEY* phKey);

HRESULT		GetRegEnumKey(HKEY hRootKey, WCHAR* pwszKeyName, DWORD dwIndex, WCHAR* pwszSubKeyName, ULONG cBytes);
HRESULT		GetRegEnumKeyA(HKEY hRootKey, char* pwszKeyName, DWORD dwIndex, char* pwszSubKeyName, ULONG cBytes);
HRESULT		GetRegEnumValue(HKEY hRootKey, WCHAR* pwszKeyName, DWORD dwIndex, WCHAR** ppwszValueName);
HRESULT		GetRegEnumValueA(HKEY hRootKey, char* pwszKeyName, DWORD dwIndex, char** ppwszValueName);
HRESULT		GetRegEnumValue(HKEY hRootKey, WCHAR* pwszKeyName, DWORD dwIndex, WCHAR* pwszValueName, ULONG* pcBytes);
HRESULT		GetRegEnumValueA(HKEY hRootKey, char* pwszKeyName, DWORD dwIndex, char* pwszValueName, ULONG* pcBytes);

HRESULT		GetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, WCHAR* pwszValue, ULONG cBytes);
HRESULT		GetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, char* pwszValue, ULONG cBytes);
HRESULT		GetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, WCHAR** ppwszValue);
HRESULT		GetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, char** ppwszValue);
HRESULT		GetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, ULONG* pulValue);
HRESULT		GetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, ULONG* pulValue);
HRESULT		GetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, void* pStruct, ULONG cbSize, ULONG* pcbActualSize, ULONG dwType = REG_BINARY);
HRESULT		GetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, void* pStruct, ULONG cbSize, ULONG* pcbActualSize, ULONG dwType = REG_BINARY);

HRESULT		SetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, WCHAR* pwszValue);
HRESULT		SetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, char* pwszValue);
HRESULT		SetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, ULONG ulValue);
HRESULT		SetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, ULONG ulValue);
HRESULT		SetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, void* pStruct, ULONG cbSize, ULONG dwType = REG_BINARY);
HRESULT		SetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, void* pStruct, ULONG cbSize, ULONG dwType = REG_BINARY);

HRESULT		DelRegEntry(HKEY hRootKey, WCHAR* pwszKeyName);
HRESULT		DelRegEntryA(HKEY hRootKey, char* pwszKeyName);
HRESULT		DelRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, BOOL fSubKeys);
HRESULT		DelRegEntryA(HKEY hRootKey, char* pwszKeyName, BOOL fSubKeys);
	
HRESULT		CloseRegKey(HKEY hKey);
