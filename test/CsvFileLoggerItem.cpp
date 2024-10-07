/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "CsvFileLoggerItem.h"
#include "CsvReader\CsvReader.h"
#include "Profiler\ProfilerDef.h"

CCsvFileLoggerItem::CCsvFileLoggerItem(LPCTSTR pFileName, LPCTSTR pPath)
	: CLoggerItemBase(pFileName, 0)
	, m_errorCnt(0), m_path(pPath)
{
	m_bROmode = true;
}

CCsvFileLoggerItem::~CCsvFileLoggerItem()
{
	m_rgMessages.clear();
	m_rgFilteredMessages.clear();

	CloseLogFile();
}

int CCsvFileLoggerItem::OpenLogFile()
{
	// open file
	CW2A path(m_path.c_str());
	FILE* handle = 0;
	if (0 != fopen_s(&handle, path, "r"))
	{
		return GetLastError();
	}

	try
	{
		// load CSV
		CCsvReader reader(handle, true);
		auto headers = reader.GetFieldHeaders();
		map<string, DWORD> applications2loggerIDs;
		map<DWORD, string> loggerIDs2applications;
		DWORD loggerIDgen = 1;
		WCHAR szError[512];
		char* next_token = NULL;

		// check that all expected columns are present
		char buffer[] = CSV_HEADER;
		for (char* token = strtok_s(buffer, ",", &next_token); token != nullptr; token = strtok_s(nullptr, ",\r\n", &next_token))
		{
			if (!*token) continue;	// skip empty string

			if (find(headers.begin(), headers.end(), token) == headers.end())
			{
				swprintf(szError, _countof(szError), L"Failed to load CSV file. Expected column '%S' is missing.", token);
				MessageBox(NULL, szError, L"Error", MB_ICONERROR);
				CloseLogFile();
				return -1;
			}
		}

		// convert CSV fields into internal representation
		size_t fieldCount = reader.GetFieldCount();
		vector<string> fields;
		fields.resize(fieldCount);
		while (reader.ReadNextRecord())
		{
#if _DEBUG
			if (m_rgMessages.size() == 76977)
			{
				loggerIDgen++;
				loggerIDgen--;
			}
#endif

			for (size_t fieldIndex = 0; fieldIndex < fieldCount; fieldIndex++)
			{
				fields[fieldIndex] = reader[fieldIndex];
			}

			string appName(fields[15]);
			DWORD loggerID = (applications2loggerIDs.find(appName) != applications2loggerIDs.end()) ? applications2loggerIDs[appName] : loggerIDgen;

			if (fields[1] != "ERROR")	// SQLType
			{
				DWORD payloadLength = 56 + 4 + fields[9].length() + 4 + fields[10].length() + 4 + fields[2].length() + 4 + fields[3].length() + 4;
				CProfSQLmsg profmsg(fields, payloadLength, loggerID);
				m_rgMessages.push_back((BYTE*)profmsg.GetBaseAddr());
			}
			else
			{
				DWORD payloadLength = 20 + 4 + fields[9].length() + 4 + fields[10].length() + 4 + fields[2].length() + 4 + fields[3].length() + 4;
				CProfERRORmsg profmsg(fields, payloadLength, loggerID);
				m_rgMessages.push_back((BYTE*)profmsg.GetBaseAddr());
			}

			if (applications2loggerIDs.find(appName) == applications2loggerIDs.end())
			{
				applications2loggerIDs.insert(pair<string, DWORD>(appName, loggerIDgen));
				CA2W wAppName(appName.c_str());
				m_mapLogger2Appname.insert(pair<DWORD, wstring>(loggerIDgen, (WCHAR*)wAppName));
				loggerIDgen++;
			}
		}
	}
	catch (...)
	{
		return -1;
	}

	return 0;
}

map<DWORD, wstring>* CCsvFileLoggerItem::GetLogger2AppnameMap()
{
	return &m_mapLogger2Appname;
}

size_t CCsvFileLoggerItem::GetNumErrors() const
{
	return m_errorCnt;
}

size_t CCsvFileLoggerItem::GetNumMessages() const
{
	return !m_filterActive ? m_rgMessages.size() : m_rgFilteredMessages.size();
}

const BYTE* CCsvFileLoggerItem::GetMessageData(size_t i) const
{
	return (!m_filterActive ? m_rgMessages[i] : m_rgFilteredMessages[i]);
}

void CCsvFileLoggerItem::FilterLoggerItems()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CCsvFileLoggerItem::FilterLoggerItems(%s): '%s'\n", m_filterStr.c_str(), m_name.c_str());
}

void CCsvFileLoggerItem::ClearLog()
{
	m_errorCnt = 0;

	for_each(m_rgMessages.begin(), m_rgMessages.end(), [=](BYTE* p) -> void { delete[] p; });

	m_rgMessages.clear();
}
