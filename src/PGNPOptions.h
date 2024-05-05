/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "json\\json.h"
#include "ColorPalette.h"
#include "LoggerItem.h"

class CVisibleColumnsList : public list<wstring>
{
	mutable wstring _tmp;
public:
	const WCHAR* c_str() const
	{
		_tmp.clear();
		for (auto it=cbegin(); it != cend(); it++)
		{
			_tmp += *it;
			_tmp += L';';
		}
		return _tmp.c_str();
	}
	
	void operator = (const wstring& rhs)
	{
		WCHAR buffer[2000];
		wcscpy(buffer, rhs.c_str());
		clear();
		for (WCHAR *token = wcstok(buffer, L";"); token != NULL; token = wcstok(NULL, L";"))
		{
			push_back(token);
		}
	}
};

struct CRemoteConnectionInfo
{
	wstring _host, _user, _pwd;
};

extern void ScrambleText(char* result, size_t res_size, const char* original, bool decode = false);

// Used in storing and retrieving connections history
class CRemoteConnections : public list < CRemoteConnectionInfo >
{
	mutable wstring _tmp;
public:
	// return json-serialized string
	const WCHAR* c_str() const
	{
		_tmp.clear();

		Json::Value root;

		for (auto it = cbegin(); it != cend(); it++)
		{
			const CRemoteConnectionInfo& info = *it;

			char skey[16];
			itoa(root.size(), skey, 10);

			wstring text(info._host + L"\1" + info._user + L"\1" + info._pwd + L"\1");
			while (text.size() < 50)	// make up for some size
				text.append(L"\205");

			char svalue[16*1024];
			ScrambleText(svalue, sizeof(svalue), CW2A(text.c_str()));

			root[skey] = svalue;
		}

		Json::StyledWriter writer;
		std::string jsonText = writer.write(root);

		wchar_t buf1[2048];
		size_t num_chars = mbstowcs(buf1, jsonText.c_str(), jsonText.size());
		_tmp.assign(buf1, num_chars);

		return _tmp.c_str();
	}

	void operator = (const wstring& rhs)
	{
		char txt[16384];
		WideCharToMultiByte(CP_ACP, 0, rhs.c_str(), -1, txt, sizeof(txt), NULL, NULL);

		clear();

		Json::Value root;
		Json::Reader reader;

		bool parsingSuccessful = reader.parse(txt, &txt[strlen(txt)], root);
		if (!parsingSuccessful)
		{
			return;
		}

	}
};

class CPGNPOptions
{
public:
	enum
	{
		EQUAL_CAT_GENERAL = 1,		// flag that indicated that General categories are the same
		EQUAL_CAT_QCLRLIGHT = 2,	// -//- Highlighting categories are the same
		EQUAL_CAT_QCLRDARK = 4,	// -//- Highlighting categories are the same
		EQUAL_CAT_COLUMNS = 8,		// -//- Columns categories are the same
		EQUAL_CAT_OPTIMIZER = 16,	// -//- Optimizer connection strings are the same
		EQUAL_CAT_REMOTE = 32,		// -//- Remote connections are the same

		EQUAL_CAT_MASK = EQUAL_CAT_GENERAL | EQUAL_CAT_QCLRLIGHT | EQUAL_CAT_QCLRDARK | EQUAL_CAT_COLUMNS | EQUAL_CAT_OPTIMIZER | EQUAL_CAT_REMOTE
	};

	LONGLONG  m_llMaxLogFileSize;
	ULONG m_dwProfileAllApps, m_dwDeleteWorkFiles, /*m_dwRefreshApps,*/ m_dwColorTheme, m_dwParamFormat, m_dwConnectToHostEnabled;
	wstring m_sWorkPath;
	wstring m_sOptimizerConnectionString;
	vector<COLORREF> m_qclrLight, m_qclrDark;	// events highlighting colors
	CVisibleColumnsList m_visibleColumnsList;	// the list contains names, order and widths of visible columns.
	CRemoteConnections m_remoteConnections;		// history of connection info
	LONG m_panel1FontHt, m_DetailsFontHt, m_ExplorerFontHt, m_FilterFontHt;

	CPGNPOptions();

	void Reset(int theme = 0);

	int operator==(const CPGNPOptions& rhs) const;

	wstring GetVisibleColumn(const wstring& name) const;

	static LONGLONG AbbreviatedToNumber(const WCHAR* buffer, LONGLONG defaultNum = MAX_LOGFILE_SIZE, LONGLONG minNum = MMFGROWSIZE);
};
