#include "StdAfx.h"
#include "RegUtil.h"

////////////////////////////////////////////////////////
// HRESULT CreateRegKey
//
////////////////////////////////////////////////////////
HRESULT CreateRegKey(HKEY hRootKey, WCHAR* pwszKeyName, HKEY* phKey, REGSAM samDesired)
{
	HRESULT hr = E_FAIL;
	ULONG dwDisposition = 0;

	//Need the name of the key to open
	if(!pwszKeyName)
		return E_FAIL;
	
	hr = RegCreateKeyExW(hRootKey, pwszKeyName, 0, NULL, REG_OPTION_NON_VOLATILE, samDesired, NULL, phKey, &dwDisposition);

	if(hr != S_OK)
		hr = E_FAIL;
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT CreateRegKeyA
//
////////////////////////////////////////////////////////
HRESULT CreateRegKeyA(HKEY hRootKey, char* pwszKeyName, HKEY* phKey, REGSAM samDesired)
{
	HRESULT hr = E_FAIL;
	ULONG dwDisposition = 0;

	//Need the name of the key to open
	if(!pwszKeyName)
		return E_FAIL;
	
	hr = RegCreateKeyExA(hRootKey, pwszKeyName, 0, NULL, REG_OPTION_NON_VOLATILE, samDesired, NULL, phKey, &dwDisposition);

	if(hr != S_OK)
		hr = E_FAIL;
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT OpenRegKey
//
////////////////////////////////////////////////////////
HRESULT OpenRegKey(HKEY hRootKey, WCHAR* pwszKeyName, DWORD ulOptions, REGSAM samDesired, HKEY* phKey)
{
	HRESULT hr = E_FAIL;

	//Obtain the Key for HKEY_CLASSES_ROOT\"SubKey"
	hr = RegOpenKeyExW(hRootKey, pwszKeyName, ulOptions, samDesired, phKey);

	if(hr != S_OK)
		return E_FAIL;
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT OpenRegKey
//
////////////////////////////////////////////////////////
HRESULT OpenRegKeyA(HKEY hRootKey, char* pwszKeyName, DWORD ulOptions, REGSAM samDesired, HKEY* phKey)
{
	HRESULT hr = E_FAIL;

	//Obtain the Key for HKEY_CLASSES_ROOT\"SubKey"
	hr = RegOpenKeyExA(hRootKey, pwszKeyName, ulOptions, samDesired, phKey);

	if(hr != S_OK)
		return E_FAIL;
	return hr;
}

////////////////////////////////////////////////////////
// HRESULT GetRegEnumKey
//
////////////////////////////////////////////////////////
HRESULT GetRegEnumKey(HKEY hRootKey, WCHAR* pwszKeyName, DWORD dwIndex, WCHAR* pwszSubKeyName, ULONG cBytes)
{
	HRESULT hr = E_FAIL;
	HKEY hKey = NULL;

	//Need some place to put the key name returned!
	if(!pwszSubKeyName)
		return E_FAIL;
	
	//Obtain the Key for HKEY_CLASSES_ROOT\"KeyName"
	if(pwszKeyName)
	{
		if(FAILED(OpenRegKey(hRootKey, pwszKeyName, 0, KEY_READ, &hKey)))
			goto CLEANUP;
	}
	
	//Obtain the specified RegItem at the index specified
	hr = RegEnumKeyW(hKey ? hKey : hRootKey, dwIndex, pwszSubKeyName, cBytes);

CLEANUP:
	if(hr != S_OK && hr != ERROR_NO_MORE_ITEMS)
		hr = E_FAIL;
	CloseRegKey(hKey);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT GetRegEnumKey
//
////////////////////////////////////////////////////////
HRESULT GetRegEnumKeyA(HKEY hRootKey, char* pwszKeyName, DWORD dwIndex, char* pwszSubKeyName, ULONG cBytes)
{
	HRESULT hr = E_FAIL;
	HKEY hKey = NULL;

	//Need some place to put the key name returned!
	if(!pwszSubKeyName)
		return E_FAIL;
	
	//Obtain the Key for HKEY_CLASSES_ROOT\"KeyName"
	if(pwszKeyName)
	{
		if(FAILED(OpenRegKeyA(hRootKey, pwszKeyName, 0, KEY_READ, &hKey)))
			goto CLEANUP;
	}
	
	//Obtain the specified RegItem at the index specified
	hr = RegEnumKeyA(hKey ? hKey : hRootKey, dwIndex, pwszSubKeyName, cBytes);

CLEANUP:
	if(hr != S_OK && hr != ERROR_NO_MORE_ITEMS)
		hr = E_FAIL;
	CloseRegKey(hKey);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT GetRegEnumValue
//
////////////////////////////////////////////////////////
HRESULT GetRegEnumValue(HKEY hRootKey, WCHAR* pwszKeyName, DWORD dwIndex, WCHAR** ppwszValueName)
{
	HRESULT hr = S_OK;
	ULONG cBytes = 0;
	HKEY hKey = NULL;
	ULONG cMaxValueChars = 0;

	//Obtain the Key for HKEY_CLASSES_ROOT\"KeyName"
	if(pwszKeyName)
	{
		if(FAILED(OpenRegKey(hRootKey, pwszKeyName, 0, KEY_READ, &hKey)))
			goto CLEANUP;
	}

	//First obtain the length of the Value...
	if(S_OK == RegQueryInfoKey(hKey ? hKey : hRootKey, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &cMaxValueChars, NULL, NULL, NULL))
	{
		//Alloc a buffer large enough...
		SAFE_ALLOC(*ppwszValueName, WCHAR, cMaxValueChars+1);
		(*ppwszValueName)[0] = L'\0';

		//Now obtain the data...
		cBytes = (cMaxValueChars+1)*sizeof(WCHAR);
		hr = GetRegEnumValue(hRootKey, pwszKeyName, dwIndex, *ppwszValueName, &cBytes);
	}
	
CLEANUP:
	CloseRegKey(hKey);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT GetRegEnumValue
//
////////////////////////////////////////////////////////
HRESULT GetRegEnumValueA(HKEY hRootKey, char* pwszKeyName, DWORD dwIndex, char** ppwszValueName)
{
	HRESULT hr = S_OK;
	ULONG cBytes = 0;
	HKEY hKey = NULL;
	ULONG cMaxValueChars = 0;

	//Obtain the Key for HKEY_CLASSES_ROOT\"KeyName"
	if(pwszKeyName)
	{
		if(FAILED(OpenRegKeyA(hRootKey, pwszKeyName, 0, KEY_READ, &hKey)))
			goto CLEANUP;
	}

	//First obtain the length of the Value...
	if(S_OK == RegQueryInfoKey(hKey ? hKey : hRootKey, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &cMaxValueChars, NULL, NULL, NULL))
	{
		//Alloc a buffer large enough...
		SAFE_ALLOC(*ppwszValueName, char, cMaxValueChars+1);
		(*ppwszValueName)[0] = '\0';

		//Now obtain the data...
		cBytes = (cMaxValueChars+1)*sizeof(char);
		hr = GetRegEnumValueA(hRootKey, pwszKeyName, dwIndex, *ppwszValueName, &cBytes);
	}
	
CLEANUP:
	CloseRegKey(hKey);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT GetRegEnumValue
//
////////////////////////////////////////////////////////
HRESULT GetRegEnumValue(HKEY hRootKey, WCHAR* pwszKeyName, DWORD dwIndex, WCHAR* pwszValueName, ULONG* pcBytes)
{
	HRESULT hr = E_FAIL;
	HKEY hKey = NULL;
	ATLASSERT(pcBytes);

	//Obtain the Key for HKEY_CLASSES_ROOT\"KeyName"
	if(pwszKeyName)
	{
		if(FAILED(OpenRegKey(hRootKey, pwszKeyName, 0, KEY_READ, &hKey)))
			goto CLEANUP;
	}

	//Obtain the specified RegItem at the index specified
	hr = RegEnumValueW(hKey ? hKey : hRootKey, dwIndex, pwszValueName, pcBytes, 0, NULL, NULL, 0);
	
CLEANUP:
	if(hr != S_OK && hr != ERROR_NO_MORE_ITEMS)
		hr = E_FAIL;
	CloseRegKey(hKey);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT GetRegEnumValue
//
////////////////////////////////////////////////////////
HRESULT GetRegEnumValueA(HKEY hRootKey, char* pwszKeyName, DWORD dwIndex, char* pwszValueName, ULONG* pcBytes)
{
	HRESULT hr = E_FAIL;
	HKEY hKey = NULL;
	ATLASSERT(pcBytes);

	//Obtain the Key for HKEY_CLASSES_ROOT\"KeyName"
	if(pwszKeyName)
	{
		if(FAILED(OpenRegKeyA(hRootKey, pwszKeyName, 0, KEY_READ, &hKey)))
			goto CLEANUP;
	}

	//Obtain the specified RegItem at the index specified
	hr = RegEnumValueA(hKey ? hKey : hRootKey, dwIndex, pwszValueName, pcBytes, 0, NULL, NULL, 0);
	
CLEANUP:
	if(hr != S_OK && hr != ERROR_NO_MORE_ITEMS)
		hr = E_FAIL;
	CloseRegKey(hKey);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT GetRegEntry
//
////////////////////////////////////////////////////////
HRESULT GetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, WCHAR* pwszValue, ULONG cBytes)
{
	return GetRegEntry(hRootKey, pwszKeyName, pwszValueName, pwszValue, cBytes, NULL, REG_SZ);
}


////////////////////////////////////////////////////////
// HRESULT GetRegEntry
//
////////////////////////////////////////////////////////
HRESULT GetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, char* pwszValue, ULONG cBytes)
{
	return GetRegEntryA(hRootKey, pwszKeyName, pwszValueName, pwszValue, cBytes, NULL, REG_SZ);
}


////////////////////////////////////////////////////////
// HRESULT GetRegEntry
//
////////////////////////////////////////////////////////
HRESULT GetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, WCHAR** ppwszValue)
{
	HRESULT hr = S_OK;
	ATLASSERT(ppwszValue);
	ULONG cBytes = 0;
	CHAR* pszBuffer = NULL;

	//Obtain the length first (value = NULL)
	TESTC(hr = GetRegEntry(hRootKey, pwszKeyName, pwszValueName, NULL, 0, &cBytes, REG_SZ));

	//Allocate the string
	//NOTE: The length returned from RegQueryValueEx also includes the NULL terminator
	SAFE_ALLOC(*ppwszValue, WCHAR, cBytes);

	//Now obtain the data
	TESTC(hr = GetRegEntry(hRootKey, pwszKeyName, pwszValueName, *ppwszValue, cBytes, &cBytes, REG_SZ));

CLEANUP:
	SAFE_FREE(pszBuffer);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT GetRegEntry
//
////////////////////////////////////////////////////////
HRESULT GetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, char** ppwszValue)
{
	HRESULT hr = S_OK;
	ATLASSERT(ppwszValue);
	ULONG cBytes = 0;
	CHAR* pszBuffer = NULL;

	//Obtain the length first (value = NULL)
	TESTC(hr = GetRegEntryA(hRootKey, pwszKeyName, pwszValueName, NULL, 0, &cBytes, REG_SZ));

	//Allocate the string
	//NOTE: The length returned from RegQueryValueEx also includes the NULL terminator
	SAFE_ALLOC(*ppwszValue, char, cBytes);

	//Now obtain the data
	TESTC(hr = GetRegEntryA(hRootKey, pwszKeyName, pwszValueName, *ppwszValue, cBytes, &cBytes, REG_SZ));

CLEANUP:
	SAFE_FREE(pszBuffer);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT GetRegEntry
//
////////////////////////////////////////////////////////
HRESULT GetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, ULONG* pulValue)
{
	return GetRegEntry(hRootKey, pwszKeyName, pwszValueName, pulValue, sizeof(ULONG), NULL, REG_DWORD);
}


////////////////////////////////////////////////////////
// HRESULT GetRegEntry
//
////////////////////////////////////////////////////////
HRESULT GetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, ULONG* pulValue)
{
	return GetRegEntryA(hRootKey, pwszKeyName, pwszValueName, pulValue, sizeof(ULONG), NULL, REG_DWORD);
}


////////////////////////////////////////////////////////
// HRESULT GetRegEntry
//
////////////////////////////////////////////////////////
HRESULT GetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, void* pStruct, ULONG cBytes, ULONG* pcActualBytes, ULONG dwType)
{
	HRESULT hr = E_FAIL;
	HKEY hKey = NULL;

	//NOTE: pStruct can be NULL if the user is just obtaining the length...

	//Obtain the Data for the above key
	if(pwszKeyName)
	{
		if(FAILED(OpenRegKeyA(hRootKey, pwszKeyName, 0, KEY_READ, &hKey)))
			goto CLEANUP;
	}
		
	hr = RegQueryValueExA(hKey ? hKey : hRootKey, pwszValueName, NULL, &dwType, (BYTE*)pStruct, &cBytes);
	if(hr != S_OK)
		hr = E_FAIL;

CLEANUP:
	if(pcActualBytes)
		*pcActualBytes = cBytes;

	CloseRegKey(hKey);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT GetRegEntry
//
////////////////////////////////////////////////////////
HRESULT GetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, void* pStruct, ULONG cBytes, ULONG* pcActualBytes, ULONG dwType)
{
	HRESULT hr = E_FAIL;
	HKEY hKey = NULL;

	//NOTE: pStruct can be NULL if the user is just obtaining the length...

	//Obtain the Data for the above key
	if(pwszKeyName)
	{
		if(FAILED(OpenRegKey(hRootKey, pwszKeyName, 0, KEY_READ, &hKey)))
			goto CLEANUP;
	}
		
	hr = RegQueryValueExW(hKey ? hKey : hRootKey, pwszValueName, NULL, &dwType, (BYTE*)pStruct, &cBytes);
	if(hr != S_OK)
		hr = E_FAIL;

CLEANUP:
	if(pcActualBytes)
		*pcActualBytes = cBytes;

	CloseRegKey(hKey);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT SetRegEntry
//
////////////////////////////////////////////////////////
HRESULT SetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, WCHAR* pwszValue)
{
	return SetRegEntry(hRootKey, pwszKeyName, pwszValueName, pwszValue ? pwszValue : L"", pwszValue ? (wcslen(pwszValue)+1)*sizeof(WCHAR) : sizeof(WCHAR), REG_SZ);
}


////////////////////////////////////////////////////////
// HRESULT SetRegEntry
//
////////////////////////////////////////////////////////
HRESULT SetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, char* pwszValue)
{
	return SetRegEntryA(hRootKey, pwszKeyName, pwszValueName, pwszValue ? pwszValue : "", pwszValue ? (strlen(pwszValue)+1)*sizeof(char) : sizeof(char), REG_SZ);
}


////////////////////////////////////////////////////////
// HRESULT SetRegEntry
//
////////////////////////////////////////////////////////
HRESULT SetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, ULONG ulValue)
{
	return SetRegEntry(hRootKey, pwszKeyName, pwszValueName, &ulValue, sizeof(ULONG), REG_DWORD);
}


////////////////////////////////////////////////////////
// HRESULT SetRegEntry
//
////////////////////////////////////////////////////////
HRESULT SetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, ULONG ulValue)
{
	return SetRegEntryA(hRootKey, pwszKeyName, pwszValueName, &ulValue, sizeof(ULONG), REG_DWORD);
}


////////////////////////////////////////////////////////
// HRESULT SetRegEntry
//
////////////////////////////////////////////////////////
HRESULT SetRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, WCHAR* pwszValueName, void* pStruct, ULONG cBytes, DWORD dwType)
{
	HRESULT hr = E_FAIL;
	HKEY hKey = NULL;

	//Obtain the Data for the above key
	if(pwszKeyName)
	{
		if(FAILED(hr = CreateRegKey(hRootKey, pwszKeyName, &hKey)))
			goto CLEANUP;
	}
	
	//Set the data for the above key (or the root key...)
	hr = RegSetValueExW(hKey ? hKey : hRootKey, pwszValueName, 0, dwType, (BYTE*)pStruct, cBytes);
	if(hr != S_OK)
		hr = E_FAIL;

CLEANUP:
	CloseRegKey(hKey);
	return hr;
}


////////////////////////////////////////////////////////
// HRESULT SetRegEntry
//
////////////////////////////////////////////////////////
HRESULT SetRegEntryA(HKEY hRootKey, char* pwszKeyName, char* pwszValueName, void* pStruct, ULONG cBytes, DWORD dwType)
{
	HRESULT hr = E_FAIL;
	HKEY hKey = NULL;

	//Obtain the Data for the above key
	if(pwszKeyName)
	{
		if(FAILED(hr = CreateRegKeyA(hRootKey, pwszKeyName, &hKey)))
			goto CLEANUP;
	}
	
	//Set the data for the above key (or the root key...)
	hr = RegSetValueExA(hKey ? hKey : hRootKey, pwszValueName, 0, dwType, (BYTE*)pStruct, cBytes);
	if(hr != S_OK)
		hr = E_FAIL;

CLEANUP:
	CloseRegKey(hKey);
	return hr;
}



////////////////////////////////////////////////////////
// HRESULT DelRegEntry
//
////////////////////////////////////////////////////////
HRESULT DelRegEntry(HKEY hRootKey, WCHAR* pwszKeyName)
{
	HRESULT hr;

	//DelRegEntry
	hr = RegDeleteKeyW(hRootKey, pwszKeyName);

	//Entry successfully deleted - return S_OK
	if(hr==S_OK) 
		return S_OK;

	//Entry not found - return S_FALSE
	if(hr==ERROR_FILE_NOT_FOUND)
		return S_FALSE;

	return E_FAIL;
}


////////////////////////////////////////////////////////
// HRESULT DelRegEntry
//
////////////////////////////////////////////////////////
HRESULT DelRegEntryA(HKEY hRootKey, char* pwszKeyName)
{
	HRESULT hr;

	//DelRegEntry
	hr = RegDeleteKeyA(hRootKey, pwszKeyName);

	//Entry successfully deleted - return S_OK
	if(hr==S_OK) 
		return S_OK;

	//Entry not found - return S_FALSE
	if(hr==ERROR_FILE_NOT_FOUND)
		return S_FALSE;

	return E_FAIL;
}


////////////////////////////////////////////////////////
// HRESULT DelRegEntry
//
////////////////////////////////////////////////////////
HRESULT DelRegEntry(HKEY hRootKey, WCHAR* pwszKeyName, BOOL fSubKeys)
{
	HKEY hKey = NULL;
	WCHAR wszBuffer[MAX_NAME_LEN];
	HRESULT hr = S_OK;
	
	//RegDeleteKey only deletes the key if there are no subkeys
	if(SUCCEEDED(hr = OpenRegKey(hRootKey, pwszKeyName, 0, KEY_READ | KEY_WRITE, &hKey)))
	{
		//This is a pain to always have to delete the subkeys to remove the key...
		if(fSubKeys)
		{
			//Loop over all subkeys...
			//NOTE: GetRegEnum requires KEY_ENUMERATE_SUB_KEYS which is found in KEY_READ.
			while((hr = GetRegEnumKey(hKey, NULL, 0, wszBuffer, MAX_NAME_LEN))==S_OK)
			{
				//Recurse and delete the sub key...
				if(FAILED(hr = DelRegEntry(hKey, wszBuffer, fSubKeys)))
					break;
			}
		}
		
		//Now we can delete the root key
		hr = DelRegEntry(hRootKey, pwszKeyName);
		CloseRegKey(hKey);
	}

	return hr;
}


////////////////////////////////////////////////////////
// HRESULT DelRegEntry
//
////////////////////////////////////////////////////////
HRESULT DelRegEntryA(HKEY hRootKey, char* pwszKeyName, BOOL fSubKeys)
{
	HKEY hKey = NULL;
	char wszBuffer[MAX_NAME_LEN];
	HRESULT hr = S_OK;
	
	//RegDeleteKey only deletes the key if there are no subkeys
	if(SUCCEEDED(hr = OpenRegKeyA(hRootKey, pwszKeyName, 0, KEY_READ | KEY_WRITE, &hKey)))
	{
		//This is a pain to always have to delete the subkeys to remove the key...
		if(fSubKeys)
		{
			//Loop over all subkeys...
			//NOTE: GetRegEnum requires KEY_ENUMERATE_SUB_KEYS which is found in KEY_READ.
			while((hr = GetRegEnumKeyA(hKey, NULL, 0, wszBuffer, MAX_NAME_LEN))==S_OK)
			{
				//Recurse and delete the sub key...
				if(FAILED(hr = DelRegEntryA(hKey, wszBuffer, fSubKeys)))
					break;
			}
		}
		
		//Now we can delete the root key
		hr = DelRegEntryA(hRootKey, pwszKeyName);
		CloseRegKey(hKey);
	}

	return hr;
}

	
////////////////////////////////////////////////////////
// HRESULT CloseRegKey
//
////////////////////////////////////////////////////////
HRESULT CloseRegKey(HKEY hKey)
{
	HRESULT hr = S_OK;
	
	//RegCloseKey
	if(hKey)
		hr = RegCloseKey(hKey);
	
	if(hr != S_OK)
		hr = E_FAIL;
	return hr;
}
