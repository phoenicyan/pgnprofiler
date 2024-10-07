/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "FileLoggerItem.h"
#include "Profiler\ProfilerDef.h"

CFileLoggerItem::CFileLoggerItem(LPCTSTR pFileName, LPCTSTR pPath)
	: CLoggerItemBase(pFileName, 0)
	, m_hLogFile(INVALID_HANDLE_VALUE), m_hMapping(0), m_logStart(0)
	, m_errorCnt(0), m_path(pPath)
{
	m_bROmode = true;
}

CFileLoggerItem::~CFileLoggerItem()
{
	m_rgMessageOffsets.clear();
	m_rgFilteredMessages.clear();

	::CloseHandle(m_hLogFile);
}

int CFileLoggerItem::OpenLogFile(DWORD dwReserved)
{
	// Open file and read text...
	m_hLogFile = CreateFile(m_path.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (INVALID_HANDLE_VALUE == m_hLogFile)
	{
		return GetLastError();
	}

	DWORD dwFileSize = GetFileSize(m_hLogFile, 0);
	//if (dwFileSize < 50)
	//{
	//	::CloseHandle(m_hLogFile);
	//	m_hLogFile = NULL;
	//	return -1;
	//}

	m_hMapping = ::CreateFileMapping(m_hLogFile, 0, PAGE_READONLY, 0, dwFileSize, 0);
	if (m_hMapping == 0)
	{
		::CloseHandle(m_hLogFile);
		m_hLogFile = NULL;

		//DWORD err = GetLastError();
		//ATLASSERT(0);
		return -2;
	}

	m_logStart = (BYTE*)::MapViewOfFile(m_hMapping, FILE_MAP_READ, 0, 0, 0);

	ATLASSERT(m_logStart != 0);
	if (m_logStart == 0)
	{
		::CloseHandle(m_hMapping);
		m_hMapping = NULL;

		::CloseHandle(m_hLogFile);
		m_hLogFile = NULL;
		return -2;
	}

	if (0 == strncmp((char*)m_logStart, PGL_HEADER, PGL_HEADER_SIZE))
	{
		// bSavedLogFormat
		DWORD numMsgs = *(DWORD*)&m_logStart[PGL_HEADER_SIZE];
		m_errorCnt = *(DWORD*)&m_logStart[PGL_HEADER_SIZE + 4];
		LONG offset = PGL_HEADER_SIZE + 8 + numMsgs * sizeof(int);
		for (unsigned i = 0; i < numMsgs; i++)
		{
			m_rgMessageOffsets.push_back(offset);
			offset += *(DWORD*)&m_logStart[PGL_HEADER_SIZE + 8 + i * sizeof(LONG)];
		}

		// read apps names
		if (numMsgs > 0)
		{
			offset = (offset + 3) & ~3;				// now offset points to the map
			while (offset < dwFileSize)
			{
				DWORD loggerID = *(DWORD*)&m_logStart[offset];
				char* nameA = (char*)&m_logStart[offset + 4];
				CA2W appName(nameA);
				m_mapLogger2Appname.insert(std::pair<DWORD, wstring>(loggerID, wstring(appName)));

				offset += strlen(nameA) + 1 + 4;
			}
		}
	}
	else if (0 == strncmp((char*)m_logStart, PGL_HEADER_WORK, PGL_HEADER_SIZE))
	{
		// bWorkLogFormat
		BYTE* logWritePos = &m_logStart[PGL_HEADER_SIZE];

		while (logWritePos - m_logStart < dwFileSize)
		{
			TRC_TYPE trcType = (TRC_TYPE)logWritePos[4];
			if (TRC_NONE == trcType)
				break;

			DWORD offset = (DWORD)(logWritePos - m_logStart);
			m_rgMessageOffsets.push_back(offset);

			DWORD dwLen;
			if (TRC_ERROR == trcType)
			{
				CProfERRORmsg profmsg(logWritePos, true);
				dwLen = profmsg.GetLength();
				m_errorCnt++;
			}
			else
			{
				CProfSQLmsg profmsg(logWritePos, true);
				dwLen = profmsg.GetLength();
			}
			logWritePos += (dwLen + 3) & ~3;
		}
	}
	else //if (!bNewFileFormat && !bWorkLogFormat)
	{
		::UnmapViewOfFile(m_logStart);

		::CloseHandle(m_hMapping);
		m_hMapping = NULL;

		::CloseHandle(m_hLogFile);
		m_hLogFile = NULL;
		return -1;
	}

	// set initial time
	if (!m_rgMessageOffsets.empty())
	{
		auto baseAddr = m_logStart + m_rgMessageOffsets[0];
		if (TRC_ERROR == (TRC_TYPE)baseAddr[4])
		{
			CProfERRORmsg profmsg(baseAddr, true);
			*(TRC_TIME*)&m_initialTime = profmsg.TimeStamp;
		}
		else
		{
			CProfSQLmsg profmsg(baseAddr, true);
			*(TRC_TIME*)&m_initialTime = profmsg.TimeStamp;
		}
	}

	return 0;
}

void CFileLoggerItem::CloseLogFile()
{
	::UnmapViewOfFile(m_logStart);
	m_logStart = 0;

	CloseHandle(m_hMapping);
	m_hMapping = 0;

	CloseHandle(m_hLogFile);
	m_hLogFile = INVALID_HANDLE_VALUE;
}

map<DWORD, wstring>* CFileLoggerItem::GetLogger2AppnameMap()
{
	return &m_mapLogger2Appname;
}

size_t CFileLoggerItem::GetNumErrors() const
{
	return m_errorCnt;
}

size_t CFileLoggerItem::GetNumMessages() const
{
	return !m_filterActive ? m_rgMessageOffsets.size() : m_rgFilteredMessages.size();
}

const BYTE* CFileLoggerItem::GetMessageData(size_t i) const
{
	return m_logStart + (!m_filterActive ? m_rgMessageOffsets[i] : m_rgFilteredMessages[i]);
}

void CFileLoggerItem::FilterLoggerItems()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CFileLoggerItem::FilterLoggerItems(%s): '%s'\n", m_filterStr.c_str(), m_name.c_str());
}

void CFileLoggerItem::ClearLog()
{
	m_errorCnt = 0;
	m_rgMessageOffsets.clear();
}
