/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "StdAfx.h"
#include "LoggerItem.h"
#include "ProfilerDef.h"
#include "ProfilerServer.h"
#include "CsvReader.h"
#include <shlobj.h>			// SHGetFolderPath
#include <atlcomtime.h>		// COleDateTime

using std::pair;

extern "C" __declspec(dllimport) int __stdcall SHCreateDirectory(HWND hwnd, LPCWSTR pszPath);

#define LIC_CODE  0x55

#if USE_PARF
#define THRESHOLD_4PARF	1000	// defines threshold of when to start parallel filtering
#endif

#pragma region CLoggerItemBase
CriticalSection CLoggerItemBase::_lock;

/////////////////////////////////////////////////////////////////////////////
// CLoggerItemBase - Abstract base class for Explorer nodes.
/////////////////////////////////////////////////////////////////////////////

CLoggerItemBase::CLoggerItemBase() : CLoggerItemBase(L"", nullptr, LT_LOGROOT, LS_PAUSED)
{}

CLoggerItemBase::CLoggerItemBase(LPCTSTR pstrName, CLoggerItemBase* pParent, LOGGER_TYPE type, LOGGER_STATE state)
	: m_name(pstrName), m_type(type), m_state(state), m_pParent(pParent), m_initialTime(), m_filterActive(false)
	, m_bROmode(false), m_psWorkPath(0), m_dwTraceLevel(0)
	, m_completionCode(0)
{}

CLoggerItemBase::~CLoggerItemBase()
{
	DeleteChildren();
}

int CLoggerItemBase::OpenLogFile(DWORD dwReserved)
{
	return 0;
}

void CLoggerItemBase::CloseLogFile()
{
}

map<DWORD, wstring>* CLoggerItemBase::GetLogger2AppnameMap()
{
	return nullptr;
}

BOOL CLoggerItemBase::DeleteChildren()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CLoggerItemBase::DeleteChildren(): %ld\n", m_children.size());

	for (auto it=m_children.begin(); it != m_children.end(); )
	{
		CLoggerTracker::Instance().RemoveLogger(*it);
		delete (*it);
		it = m_children.erase(it);
	}
	return TRUE;
}

BOOL CLoggerItemBase::AddChild(CProcessLoggerItem* pBlock)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CLoggerItemBase::AddChild(pid=%d)\n", pBlock->GetPID());

	CLoggerTracker::Instance().AddLogger((USHORT)pBlock->GetPID(), pBlock);	// assume that PID_n <> PID_m + 65536

	m_children.push_back(pBlock);

	return TRUE;
}

void CLoggerItemBase::SetFilterActive(bool isActive, std::function<void(int)> fnProgressCallback)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CLoggerItemBase::SetFilterActive(%s)\n", isActive ? L"true" : L"false");

	if (isActive)
	{
		m_filterActive = true;

		if (m_ct.is_cancelled())
			m_ct.reset();

		if (m_filterStr.length() > 0)
		{
			wstring	filterStr(m_filterStr);
			if (filterStr[filterStr.length() - 1] != L';')
				filterStr += L';';

			CW2A filterA(filterStr.c_str());
			bool result = m_driver.Prepare(filterA, "input");
			if (result)
			{
				m_completionCode = 0;
				m_filterProgressSignal.connect(fnProgressCallback);
				CMyTaskExecutor::Instance().AddTask(std::bind(&CLoggerItemBase::FilterLoggerItems, this));
			}
			else
			{
				CA2W wsLastError(m_driver.lastError);
				MessageBox(NULL, wsLastError, L"Error", MB_ICONERROR);

				m_completionCode = 1;
			}
		}
	}
	else
	{
		m_ct.cancel();
		m_filterActive = false;
	}
}

void CLoggerItemBase::SetVariables(Driver& driver, const BYTE* baseAddr, LONGLONG initialTime, DWORD pid, LPCSTR appName)
{
	TRC_TYPE trcType = (TRC_TYPE)baseAddr[4];
	SQL_QUERY_TYPE cmdType = (SQL_QUERY_TYPE)baseAddr[5];

	if (trcType == TRC_ERROR)
	{
		CProfERRORmsg profmsg((BYTE*)baseAddr, true);

		vector<CVariable*>& vars = driver.GetVariables();
		for (vector<CVariable*>::iterator vit=vars.begin(); vit != vars.end(); vit++)
		{
			switch ((*vit)->GetIndex())
			{
			case 0:	//abstime
				{
					SYSTEMTIME sysTime, locTime;
					FileTimeToSystemTime((FILETIME*)&profmsg.TimeStamp, &sysTime);
					SystemTimeToTzSpecificLocalTime(NULL, &sysTime, &locTime);

					char abstime_buf[_CVTBUFSIZE];
					sprintf(abstime_buf, "%04d/%02d/%02d %02d:%02d:%02d.%03d", locTime.wYear, locTime.wMonth, locTime.wDay, locTime.wHour, locTime.wMinute, locTime.wSecond, locTime.wMilliseconds);
					(*vit)->SetText(abstime_buf);
				}
				break;
			case 1:	//reltime
				(*vit)->SetNumeric((double)(profmsg.TimeStamp - initialTime) / g_qpcFreq);
				break;
			case 2:	//sqltype
				(*vit)->SetEnumVal(profmsg.TrcType);
				break;
			case 3:	//clientsql
				(*vit)->SetText(profmsg.ErrorText.TextPtr);		// not ClientSQL!
				break;
			
			/* 4-8 are not used */

			case 9:	//database
				(*vit)->SetText(profmsg.Database.TextPtr);
				break;
			case 10:	//user
				(*vit)->SetText(profmsg.UserName.TextPtr);
				break;
			case 11:	//pid
				(*vit)->SetNumeric((double)pid);
				break;
			case 12:	//sessid
				(*vit)->SetNumeric((double)profmsg.SessionId);
				break;
			case 13:	//cmdid
				(*vit)->SetNumeric((double)profmsg.CommandId);
				break;
			case 14:	//cursormode
				(*vit)->SetNumeric((double)profmsg.Cursor);
				break;
			case 15:	//application
				(*vit)->SetText(appName);
				break;
			}
		}
	}
	else
	{
		CProfSQLmsg profmsg((BYTE*)baseAddr, true);

		vector<CVariable*>& vars = driver.GetVariables();
		for (vector<CVariable*>::iterator vit=vars.begin(); vit != vars.end(); vit++)
		{
			switch ((*vit)->GetIndex())
			{
			case 0:	//abstime
				{
					SYSTEMTIME sysTime, locTime;
					FileTimeToSystemTime((FILETIME*)&profmsg.TimeStamp, &sysTime);
					SystemTimeToTzSpecificLocalTime(NULL, &sysTime, &locTime);

					char abstime_buf[_CVTBUFSIZE];
					sprintf(abstime_buf, "%04d/%02d/%02d %02d:%02d:%02d.%03d", locTime.wYear, locTime.wMonth, locTime.wDay, locTime.wHour, locTime.wMinute, locTime.wSecond, locTime.wMilliseconds);
					(*vit)->SetText(abstime_buf);
				}
				break;
			case 1:	//reltime
				(*vit)->SetNumeric((double)(profmsg.TimeStamp - initialTime) / g_qpcFreq);
				break;
			case 2:	//sqltype
				(*vit)->SetEnumVal(profmsg.TrcType);
				break;
			case 3:	//clientsql
				(*vit)->SetText(*profmsg.ClientSQL.TextPtr ? profmsg.ClientSQL.TextPtr : profmsg.ExecutedSQL.TextPtr);
				break;
			case 4:	//parse
				(*vit)->SetNumeric((double)profmsg.ParseTime * 1000 / g_qpcFreq);
				break;
			case 5:	//prepare
				(*vit)->SetNumeric((double)profmsg.PrepareTime * 1000 / g_qpcFreq);
				break;
			case 6:	//execute
				(*vit)->SetNumeric((double)profmsg.ExecTime * 1000 / g_qpcFreq);
				break;
			case 7:	//getrows
				(*vit)->SetNumeric((double)profmsg.GetdataTime * 1000 / g_qpcFreq);
				break;
			case 8:	//rows
				(*vit)->SetNumeric((double)profmsg.NumRows * 1000 / g_qpcFreq);
				break;
			case 9:	//database
				(*vit)->SetText(profmsg.Database.TextPtr);
				break;
			case 10:	//user
				(*vit)->SetText(profmsg.UserName.TextPtr);
				break;
			case 11:	//pid
				(*vit)->SetNumeric((double)pid);
				break;
			case 12:	//sessid
				(*vit)->SetNumeric((double)profmsg.SessionId);
				break;
			case 13:	//cmdid
				(*vit)->SetNumeric((double)profmsg.CommandId);
				break;
			case 14:	//cursormode
				(*vit)->SetNumeric((double)profmsg.Cursor);
				break;
			case 15:	//application
				(*vit)->SetText(appName);
				break;
			case 16:	//cmdtype
				(*vit)->SetEnumVal(profmsg.CmdType);
				break;
			}
		}
	}
}
#pragma endregion

#pragma region CHostLoggerItem
/////////////////////////////////////////////////////////////////////////////
// CHostLoggerItem - Host/Machine node
/////////////////////////////////////////////////////////////////////////////

CHostLoggerItem::CHostLoggerItem(const wstring* psWorkPath)
	: m_pRemote(0)
{
	SetWorkPath(psWorkPath);

	memset(&m_op, 0, sizeof(m_op));
}

CHostLoggerItem::~CHostLoggerItem()
{
	m_rgPackedMessages.clear();
	m_rgFilteredMessages.clear();

	delete m_pRemote;
}

void CHostLoggerItem::SetName(/*in*/LPCTSTR hostName)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CHostLoggerItem::SetName(%s)\n", hostName);

	m_name = hostName;

	m_remoteName.assign(0 != _tcsstr(hostName, _T("127.0.0.1")) ? _T(".") : hostName);
}

void CHostLoggerItem::SetCredentials(/*in*/LPCTSTR sUser, /*in*/LPCTSTR sPwd)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CHostLoggerItem::SetCredentials(%s, %s)\n", sUser, sPwd);

	m_user = sUser;
	m_pwd = sPwd;
}

int CHostLoggerItem::ListProcessesOnRemoteHost(/*out*/ATL::CString& processes)
{
	int exitCode = 0;

	ATLTRACE2(atlTraceDBProvider, 0, L"CHostLoggerItem::ListProcessesOnRemoteHost: Connecting to '%s'...\n", m_name.c_str());

	if (nullptr == m_pRemote)
	{
		m_pRemote = new CRemoteApi(m_name.c_str(), m_user.c_str(), m_pwd.c_str());

		//ADMIN$ and IPC$ connections will be made as needed in CopyPAExecToRemote and InstallAndStartRemoteService
#if !LOCAL_DEBUG
		// copy self to remote system
		if (!m_pRemote->CopyToRemote()) //logs on error
		{
			exitCode = -4;
			goto PerServerCleanup;
		}

		ATLTRACE2(atlTraceDBProvider, 0, L"Starting PGNProfiler service on '%s'...\n", m_name.c_str());

		// install self as a remote service and start remote service
		if (false == m_pRemote->InstallAndStartRemoteService())
		{
			exitCode = -6;
			goto PerServerCleanup;
		}
#endif
	}

	// Get list of pipes available on the remote host
	if (!m_pRemote->GetProcesses(processes))
	{
		exitCode = -7;
	}

PerServerCleanup:
	return exitCode;
}

int CHostLoggerItem::SubscribeForEvents()
{
	if (!m_pRemote)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"** CHostLoggerItem::SubscribeForEvents/%s: failed since RemoteAPI was not instantiated\n", m_name.c_str());
		return 2;		// connection to remote host was not open, execute ListProcessesOnRemoteHost first
	}

	if (m_op.hEvent)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"** CHostLoggerItem::SubscribeForEvents/%s: already subscribed\n", m_name.c_str());
		return 1;		// already subscribed
	}

	m_op.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	ATLTRACE2(atlTraceDBProvider, 0, L"CHostLoggerItem::SubscribeForEvents/%s: launching overlapped read!\n", m_name.c_str());

	BOOL res = ReadFile(m_pRemote->GetPipe(), m_buffer, sizeof(m_buffer), NULL, &m_op);
	ATLASSERT(!res && GetLastError() == ERROR_IO_PENDING);

	return 0;	// success!
}

void CHostLoggerItem::CloseRemotePipe()
{
}

void CHostLoggerItem::AddPackedMessage(CPackedMessage pMessage)
{
	if (m_rgPackedMessages.size() == 0)
	{
		// remember initialization time
		GetSystemTimeAsFileTime(&m_initialTime);
	}

	m_rgPackedMessages.push_back(pMessage);

	if (m_filterActive)
	{
		CProcessLoggerItem* pLogger = (CProcessLoggerItem*) CLoggerTracker::Instance().GetLogger(pMessage._v._itemId);
		ATLASSERT(pLogger != nullptr);
		const BYTE* baseAddr = pLogger->m_logStart + pMessage._v._offset;
		CLoggerItemBase::SetVariables(m_driver, baseAddr, *pLogger->GetInitialTimePtr(), pLogger->GetPID(), pLogger->GetNameA());

		// apply filter
		if (!m_driver.Evaluate() && m_driver._bisrez != 0.0)
		{
			m_rgFilteredMessages.push_back(pMessage);
		}
	}
}

size_t CHostLoggerItem::GetNumErrors() const
{
	size_t errorCnt = 0;

	for (auto it=m_children.cbegin(); it != m_children.cend(); it++)
	{
		errorCnt += (*it)->GetNumErrors();
	}
	return errorCnt;
}

size_t CHostLoggerItem::GetNumMessages() const
{
	size_t num = !m_filterActive ? m_rgPackedMessages.size() : m_rgFilteredMessages.size();
	return num;
}

const BYTE* CHostLoggerItem::GetMessageData(size_t i) const
{
	CPackedMessage pMessage = !m_filterActive ? m_rgPackedMessages[i] : m_rgFilteredMessages[i];

	CProcessLoggerItem* pLogger = (CProcessLoggerItem*) CLoggerTracker::Instance().GetLogger(pMessage._v._itemId);
	if (pLogger != nullptr)
		return pLogger->m_logStart + pMessage._v._offset;
	return nullptr;
}

void CHostLoggerItem::FilterLoggerItems()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CHostLoggerItem::FilterLoggerItems(%s): '%s'\n", m_filterStr.c_str(), m_name.c_str());

	int rez = 0;

	m_filterProgressSignal(FILTER_START);

	// It is ok to clear filtered messages here because they aren't displayed yet (by design).
	// So if a parser error occurs, we lost nothing.
	m_rgFilteredMessages.clear();

	size_t totalMessages = m_rgPackedMessages.size();
#if USE_PARF
	if (totalMessages > THRESHOLD_4PARF)
	{
		const char* err = 0;
		CW2A filt(m_filterStr.c_str());
		concurrency::combinable<Driver*> parDriver([&filt]() { auto d = new Driver(); d->Prepare(filt, "input"); return d; });
		concurrency::cancellation_token_source cts;
		concurrency::run_with_cancellation_token([&]()
		{
			concurrency::parallel_for(size_t(0), totalMessages, [&](size_t i)
			{
				CPackedMessage pMessage = m_rgPackedMessages[i];
				CProcessLoggerItem* pLogger = (CProcessLoggerItem*) CLoggerTracker::Instance().GetLogger(pMessage._v._itemId);
				ATLASSERT(pLogger != nullptr);
				const BYTE* baseAddr = pLogger->m_logStart + pMessage._v._offset;
				CLoggerItemBase::SetVariables(*parDriver.local(), baseAddr, *pLogger->GetInitialTimePtr(), pLogger->GetPID(), pLogger->GetNameA());

				if (parDriver.local()->Evaluate())
				{
					err = parDriver.local()->lastError;	// not safe
					cts.cancel();
				}
				else if (parDriver.local()->_bisrez != 0.0)
				{
					m_rgFilteredMessages.push_back(pMessage);
				}
			});
		}, cts.get_token());

		if (err != 0)
		{
			CA2W wsLastError(err);
			MessageBox(NULL, wsLastError, L"Error", MB_ICONERROR);
			rez = 1;
		}

		parDriver.clear();
	}
	else
#endif
	{
		size_t i = 0;
		for (vector<CPackedMessage>::iterator it = m_rgPackedMessages.begin(); it != m_rgPackedMessages.end(); it++)
		{
			CPackedMessage pMessage = *it;
			CProcessLoggerItem* pLogger = (CProcessLoggerItem*) CLoggerTracker::Instance().GetLogger(pMessage._v._itemId);
			ATLASSERT(pLogger != nullptr);
			const BYTE* baseAddr = pLogger->m_logStart + pMessage._v._offset;
			CLoggerItemBase::SetVariables(m_driver, baseAddr, *pLogger->GetInitialTimePtr(), pLogger->GetPID(), pLogger->GetNameA());

			if (m_driver.Evaluate())
			{
				CA2W wsLastError(m_driver.lastError);
				MessageBox(NULL, wsLastError, L"Error", MB_ICONERROR);
				rez = 1;
				break;
			}

			if (m_driver._bisrez != 0.0)
			{
				m_rgFilteredMessages.push_back(pMessage);
			}

			if (0 == (++i % 1000))
				m_filterProgressSignal((i * 100) / totalMessages);
		}
	}

	m_filterProgressSignal(FILTER_FINISH);

	//return rez;
}

void CHostLoggerItem::ClearLog()
{
	m_rgPackedMessages.clear();

	for (auto it=m_children.begin(); it != m_children.end(); it++)
	{
		(*it)->ClearLog();
	}
}

void CHostLoggerItem::SetTraceLevel(DWORD dwTraceLevel)
{
	m_dwTraceLevel = dwTraceLevel;

	for (auto it = m_children.begin(); it != m_children.end(); it++)
	{
		(*it)->SetTraceLevel(dwTraceLevel);
	}
}
#pragma endregion

#pragma region CProcessLoggerItem
/////////////////////////////////////////////////////////////////////////////
// CProcessLoggerItem - Process node
/////////////////////////////////////////////////////////////////////////////

CProcessLoggerItem::CProcessLoggerItem(LPCTSTR pstrName, CLoggerItemBase* pParent, LOGGER_STATE state, LONGLONG llMaxLogFileSize, BOOL is64)
	: CLoggerItemBase(pstrName, pParent, is64 ? LT_PROCESS64 : LT_PROCESS, state), m_dwMMFsize(0)
	, m_hLogFile(INVALID_HANDLE_VALUE), m_hMapping(0), m_logStart(0), m_logWritePos(0)
	, m_pendingBytes(0), m_pendingWritePos(0), m_pendingLengthUnknown(false), m_llMaxLogFileSize(llMaxLogFileSize)
	, m_pid(0), m_hFile(INVALID_HANDLE_VALUE), m_errorCnt(0), m_dwDeleteOnClose(1)
{
	memset(&m_op, 0, sizeof(m_op));
	m_op.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

	*m_pipeName = 0;

	if (pParent)
		SetWorkPath(pParent->GetWorkPath());
}

CProcessLoggerItem::~CProcessLoggerItem()
{
	m_rgMessageOffsets.clear();
	m_rgFilteredMessages.clear();

	::CloseHandle(m_op.hEvent);

	CloseLogFile();
}

LPCTSTR CProcessLoggerItem::GetDisplayName() const
{
	if (m_displayName.empty())
	{
		// make application display name by prefixing with host\., and removing (PID) ending
		m_displayName.assign(((CHostLoggerItem*)m_pParent)->GetRemoteName());
		m_displayName.append(L"\\");
		const wchar_t* ending = wcsrchr(m_name.c_str(), L'(');
		size_t len = ending ? ending - m_name.c_str() - 1 : m_name.size();
		m_displayName.append(m_name.c_str(), len);
	}

	return m_displayName.c_str();
}

int CProcessLoggerItem::InternalOpenLogFile()
{
	WCHAR path[MAX_PATH];
	wcscpy(path, m_psWorkPath->c_str());

	SHCreateDirectory(NULL, path);	// make sure that directory exists

	AppendLogFileName(path);

	DWORD dwFlags = FILE_ATTRIBUTE_NORMAL;
	if (m_dwDeleteOnClose != 0)
		dwFlags |= FILE_FLAG_DELETE_ON_CLOSE;

	m_hLogFile = ::CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_ALWAYS, dwFlags, 0);
	if (INVALID_HANDLE_VALUE == m_hLogFile)
	{
		DWORD err = GetLastError();
		ATLASSERT(0);
		return 1;
	}

	return 0;
}

void CProcessLoggerItem::WriteHeader()
{
	// write work log header
	DWORD numBytes;
	WriteFile(m_hLogFile, PGL_HEADER_WORK, PGL_HEADER_SIZE, &numBytes, 0);

	ATLASSERT(numBytes == PGL_HEADER_SIZE);

	m_logWritePos += PGL_HEADER_SIZE;
}

int CProcessLoggerItem::OpenLogFile(DWORD dwDeleteOnClose)
{
	m_dwDeleteOnClose = dwDeleteOnClose;

	if (InternalOpenLogFile() || GrowLogFile(MMFGROWSIZE))
	{
		return 1;
	}

	WriteHeader();

	return 0;
}

int CProcessLoggerItem::GrowLogFile(DWORD size)
{
	CriticalSectionLock lock(_lock);

	DWORD dwWriteOffset;

	m_dwMMFsize += size;

	if (m_llMaxLogFileSize != 0 &&								// there is limit for max size
		(m_logWritePos - m_logStart) > PGL_HEADER_SIZE &&		// smth was written already
		0 == m_pendingBytes &&									// no pending message
		m_dwMMFsize > m_llMaxLogFileSize)						// new size will exceed limit
	{
		CloseLogFile();

		if (InternalOpenLogFile())
		{
			return 1;
		}

		m_dwMMFsize = MMFGROWSIZE;

		WriteHeader();

		dwWriteOffset = m_logWritePos - m_logStart;
	}
	else
	{
		if (m_logStart != 0)
			::UnmapViewOfFile(m_logStart);

		if (m_hMapping != 0)
			CloseHandle(m_hMapping);

		dwWriteOffset = m_logWritePos - m_logStart;

		m_logWritePos = m_logStart = 0;
	}

	m_hMapping = ::CreateFileMapping(m_hLogFile, 0, PAGE_READWRITE, 0, m_dwMMFsize, 0);
	if (m_hMapping == 0)
	{
		DWORD err = GetLastError();
		ATLASSERT(0);
		return 1;
	}

	m_logStart = (BYTE*)::MapViewOfFile(m_hMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
	if (m_logStart == 0)
	{
		DWORD err = GetLastError();
		ATLASSERT(0);
		return 1;
	}

	m_logWritePos = m_logStart + dwWriteOffset;

	return 0;
}

void CProcessLoggerItem::CloseLogFile()
{
	const DWORD dwWriteOffset = m_logWritePos - m_logStart;

	ATLTRACE2(atlTraceDBProvider, 2, L"CProcessLoggerItem::CloseLogFile()  size = %d\n", dwWriteOffset);

	::UnmapViewOfFile(m_logStart);
	m_logWritePos = m_logStart = 0;

	::CloseHandle(m_hMapping);
	m_hMapping = 0;

	SetFilePointer(m_hLogFile, dwWriteOffset, 0, FILE_BEGIN);
	SetEndOfFile(m_hLogFile);

	::CloseHandle(m_hLogFile);
	m_hLogFile = INVALID_HANDLE_VALUE;

	// TODO: if (m_logWritePos == PGL_HEADER_SIZE) delete the log file since it is empty anyway?
}

void CProcessLoggerItem::AppendLogFileName(WCHAR* path)
{
	COleDateTime curTime = COleDateTime::GetCurrentTime();

	swprintf(&path[wcslen(path)], MAX_PATH, L"\\%02d%02d%02d-%02d%02d%02d-%05d." PGL_FILE_EXT, curTime.GetYear(), curTime.GetMonth(), curTime.GetDay(), curTime.GetHour(), curTime.GetMinute(), curTime.GetSecond(), GetPID());
}

// If message successfully added returns true, otherwise - false.
bool CProcessLoggerItem::AddMessageFromWritePos()
{
	TRC_TYPE trcType = (TRC_TYPE)m_logWritePos[4];

	if (TRC_ERROR == trcType)
		m_errorCnt++;

	if (m_rgMessageOffsets.size() == 0)
	{
		// remember initialization time
		GetSystemTimeAsFileTime(&m_initialTime);
	}

	// Overwrite PayloadLength with _itemId of the this logger.
	const auto itemId = (USHORT)this->GetPID();
	*(DWORD*)m_logWritePos = itemId;

	DWORD offset = (DWORD)(m_logWritePos-m_logStart);
	m_rgMessageOffsets.push_back(offset);

	if (m_filterActive)
	{
		CLoggerItemBase::SetVariables(m_driver, m_logWritePos, *GetInitialTimePtr(), GetPID(), GetNameA());

		// apply filter
		if (!m_driver.Evaluate() && m_driver._bisrez != 0.0)
		{
			m_rgFilteredMessages.push_back(offset);
		}
	}

	((CHostLoggerItem*)m_pParent)->AddPackedMessage(CPackedMessage::Make(itemId, offset));

	return true;
}

size_t CProcessLoggerItem::GetNumErrors() const
{
	return m_errorCnt;
}

size_t CProcessLoggerItem::GetNumMessages() const
{
	return !m_filterActive ? m_rgMessageOffsets.size() : m_rgFilteredMessages.size();
}

const BYTE* CProcessLoggerItem::GetMessageData(size_t i) const
{
	return m_logStart + (!m_filterActive ? m_rgMessageOffsets[i] : m_rgFilteredMessages[i]);
}

void CProcessLoggerItem::FilterLoggerItems()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CProcessLoggerItem::FilterLoggerItems(%s): '%s'\n", m_filterStr.c_str(), m_name.c_str());

	int rez = 0;

	m_filterProgressSignal(FILTER_START);

	// It is ok to clear filtered messages here because they aren't displayed yet (by design).
	// So if a parser error occurs, we lost nothing.
	m_rgFilteredMessages.clear();

	size_t totalMessages = m_rgMessageOffsets.size();
#if USE_PARF
	if (totalMessages > THRESHOLD_4PARF)
	{
		const char* err = 0;
		CW2A filt(m_filterStr.c_str());
		concurrency::combinable<Driver*> parDriver([&filt]() { auto d = new Driver(); d->Prepare(filt, "input"); return d; });
		concurrency::cancellation_token_source cts;
		concurrency::run_with_cancellation_token([&]()
		{
			concurrency::parallel_for(size_t(0), totalMessages, [&](size_t i)
			{
				CLoggerItemBase::SetVariables(*parDriver.local(), m_logStart + m_rgMessageOffsets[i], *GetInitialTimePtr(), GetPID(), GetNameA());

				if (parDriver.local()->Evaluate())
				{
					err = parDriver.local()->lastError;	// not safe
					cts.cancel();
				}
				else if (parDriver.local()->_bisrez != 0.0)
				{
					m_rgFilteredMessages.push_back(m_rgMessageOffsets[i]);
				}
			});
		}, cts.get_token());

		if (err != 0)
		{
			CA2W wsLastError(err);
			MessageBox(NULL, wsLastError, L"Error", MB_ICONERROR);
			rez = 1;
		}
	}
	else
#endif
	{
		for (size_t i = 0; i < totalMessages; i++, (0 == (i % 1000)) ? m_filterProgressSignal((i * 100) / totalMessages) : 0)
		{
			CLoggerItemBase::SetVariables(m_driver, m_logStart + m_rgMessageOffsets[i], *GetInitialTimePtr(), GetPID(), GetNameA());

			if (m_driver.Evaluate())
			{
				CA2W wsLastError(m_driver.lastError);
				MessageBox(NULL, wsLastError, L"Error", MB_ICONERROR);
				rez = 1;
				break;
			}

			if (m_driver._bisrez != 0.0)
			{
				m_rgFilteredMessages.push_back(m_rgMessageOffsets[i]);
			}
		}
	}

	m_filterProgressSignal(FILTER_FINISH);
}

void CProcessLoggerItem::ClearLog()
{
	if (!m_rgMessageOffsets.size())
		return;	// no messages to clear

	m_errorCnt = 0;

	const auto itemId = (USHORT)this->GetPID();

	vector<CPackedMessage>& parentMsgs = ((CHostLoggerItem*)m_pParent)->GetPackedMessages();
	for (auto it=parentMsgs.begin(); it != parentMsgs.end(); )
	{
		// clear the message and fetch next target
		if (it->_v._itemId == itemId)
			it = parentMsgs.erase(it);
		else
			it++;
	}

	m_rgMessageOffsets.clear();
}

void CProcessLoggerItem::SetTraceLevel(DWORD dwTraceLevel)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"--> MI_TRACELEVEL_REQ, %d\n", dwTraceLevel);

	RemMsg cmdSetLevel(MI_TRACELEVEL_REQ);
	cmdSetLevel << dwTraceLevel;

	DWORD totalLen = 0;
	const BYTE* pDataToSend = cmdSetLevel.GetDataToSend(totalLen);
	DWORD cbWritten = 0;
	BOOL fSuccess = WriteFile(m_hFile, pDataToSend, totalLen, &cbWritten, 0);
	if (!fSuccess)
	{
		DWORD err = GetLastError();
		ATLTRACE2(atlTraceDBProvider, 0, L"** CProcessLoggerItem::SetTraceLevel: error %d.\n", err);
	}

	m_dwTraceLevel = dwTraceLevel;
}

void CProcessLoggerItem::SetParamFormat(ULONG dwParamFormat)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"--> MI_PRMFORMAT_REQ, %d\n", dwParamFormat);

	RemMsg cmdSetLevel(MI_PRMFORMAT_REQ);
	cmdSetLevel << dwParamFormat;

	DWORD totalLen = 0;
	const BYTE* pDataToSend = cmdSetLevel.GetDataToSend(totalLen);
	DWORD cbWritten = 0;
	BOOL fSuccess = WriteFile(m_hFile, pDataToSend, totalLen, &cbWritten, 0);
	if (!fSuccess)
	{
		DWORD err = GetLastError();
		ATLTRACE2(atlTraceDBProvider, 0, L"** CProcessLoggerItem::SetParamFormat: error %d.\n", err);
	}
}

const WCHAR* CProcessLoggerItem::GetPipeName()
{
	if (0 == *m_pipeName)
	{
		WCHAR buf[16];
		wcscpy(m_pipeName, L"\\\\");
		wcscat(m_pipeName, ((CHostLoggerItem*)m_pParent)->GetRemoteName());
		wcscat(m_pipeName, L"\\pipe\\" PGNPIPE_PREFIX);
		wcscat(m_pipeName, _itow(m_pid, buf, 10));
	}

	return m_pipeName;
}

void CProcessLoggerItem::ClosePipe()
{
	CloseHandle(m_hFile);

	m_hFile = INVALID_HANDLE_VALUE;
}
#pragma endregion

#pragma region CFileLoggerItem
/////////////////////////////////////////////////////////////////////////////
// CFileLoggerItem - Log File node (read-only)
/////////////////////////////////////////////////////////////////////////////

CFileLoggerItem::CFileLoggerItem(LPCTSTR pFileName, LPCTSTR pPath)
	: CLoggerItemBase(pFileName, 0, LT_LOGFILE, LS_TERMINATED)
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
		// note: any message type can be treated as "SQLmsg" here since the offset to TimeStamp field is the same for all message types (4xBYTE)
		CProfSQLmsg profmsg(m_logStart + m_rgMessageOffsets[0], true);
		*(TRC_TIME*)&m_initialTime = profmsg.TimeStamp;
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

	// It is ok to clear filtered messages here because they aren't displayed yet (by design).
	// So if a parser error occurs, we lost nothing.
	m_rgFilteredMessages.clear();

	m_filterProgressSignal(FILTER_START);

	size_t totalMessages = m_rgMessageOffsets.size();
#if USE_PARF
	if (totalMessages > THRESHOLD_4PARF)
	{
		// The following is not the most optimal implementation of parallel filter. See "yacee" article on codeproject, I wrote in December 2009,
		// that uses more efficient approach that avoids sorting the filtered messages. I made this alternative implemetation with sole purpose
		// of learning ppl/tbb.
		const char* err = 0;

		wstring	filterStr(m_filterStr);
		if (filterStr[filterStr.length() - 1] != L';')
			filterStr += L';';

		CW2A filterA(filterStr.c_str());

		LPCSTR appName = GetNameA();	// pre-calc ansi name - kinda hack :(
		LONGLONG initialTime = *GetInitialTimePtr();
		LONGLONG totalProgress = 0;

		concurrency::combinable<Driver*> parDriver([&filterA]() { auto d = new Driver(); d->Prepare(filterA, "input"); return d; });
		concurrency::combinable<vector<LONG>> rgFilteredMessages;
		concurrency::cancellation_token_source cts;
		concurrency::run_with_cancellation_token([&]()
		{
			concurrency::parallel_for(size_t(0), totalMessages, [&](size_t i)
			{
				Driver& driver = *parDriver.local();
				CLoggerItemBase::SetVariables(driver, m_logStart + m_rgMessageOffsets[i], initialTime, 0, appName);

				if (driver.Evaluate())
				{
					err = driver.lastError;	// not safe
					cts.cancel();
				}
				else if (m_ct.is_cancelled())
				{
					cts.cancel();
				}
				else if (driver._bisrez != 0.0)
				{
					rgFilteredMessages.local().push_back(m_rgMessageOffsets[i]);
				}

				LONGLONG curProgress = InterlockedIncrement64(&totalProgress);
				if (0 == (curProgress % 1000))
				{
					m_filterProgressSignal((curProgress * 100) / totalMessages);
				}
			});
		}, cts.get_token());

		if (err != 0)
		{
			CA2W wsLastError(err);
			MessageBox(NULL, wsLastError, L"Error", MB_ICONERROR);
			m_completionCode = 1;
		}
		else
		{
			rgFilteredMessages.combine_each([&](vector<LONG>& local)
			{
				std::move(local.begin(), local.end(), std::back_inserter(m_rgFilteredMessages));
			});

			std::sort(m_rgFilteredMessages.begin(), m_rgFilteredMessages.end());
		}
	}
	else
#endif USE_PARF
	{
		for (size_t i = 0; m_ct && i < totalMessages; i++, (0 == (i % 1000)) ? m_filterProgressSignal((i * 100) / totalMessages) : 0)
		{
			CLoggerItemBase::SetVariables(m_driver, m_logStart + m_rgMessageOffsets[i], *GetInitialTimePtr(), 0, GetNameA());

			if (m_driver.Evaluate())
			{
				m_completionCode = 1;
				break;
			}

			if (m_driver._bisrez != 0.0)
			{
				m_rgFilteredMessages.push_back(m_rgMessageOffsets[i]);
			}

			//Sleep(1);	// make it slow for debugging
		}
	}

	m_filterProgressSignal(FILTER_FINISH);
}

void CFileLoggerItem::ClearLog()
{
	m_errorCnt = 0;
	m_rgMessageOffsets.clear();
}
#pragma endregion

#pragma region CCsvFileLoggerItem
/////////////////////////////////////////////////////////////////////////////
// CCsvFileLoggerItem - Csv Log File node (read-only)
/////////////////////////////////////////////////////////////////////////////

CCsvFileLoggerItem::CCsvFileLoggerItem(LPCTSTR pFileName, LPCTSTR pPath)
	: CLoggerItemBase(pFileName, 0, LT_LOGFILE, LS_TERMINATED)
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
	FILE* handle = fopen(path, "r");
	if (0 == handle)
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

		// check that all expected columns are present
		char buffer[] = CSV_HEADER;
		for (char *token = strtok(buffer, ","); token != nullptr; token = strtok(nullptr, ",\r\n"))
		{
			if (!*token) continue;	// skip empty string

			if (find(headers.begin(), headers.end(), token) == headers.end())
			{
				swprintf(szError, L"Failed to load CSV file. Expected column '%S' is missing.", token);
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

	// It is ok to clear filtered messages here because they aren't displayed yet (by design).
	// So if a parser error occurs, we lost nothing.
	m_rgFilteredMessages.clear();

	m_filterProgressSignal(FILTER_START);

	size_t totalMessages = m_rgMessages.size();
#if USE_PARF
	if (totalMessages > THRESHOLD_4PARF)
	{
		// The following is not the most optimal implementation of parallel filter. See "yacee" article on codeproject, I wrote in December 2009,
		// that uses more efficient approach that avoids sorting the filtered messages. I made this alternative implemetation with sole purpose
		// of learning ppl/tbb.
		const char* err = 0;

		wstring	filterStr(m_filterStr);
		if (filterStr[filterStr.length() - 1] != L';')
			filterStr += L';';

		CW2A filterA(filterStr.c_str());

		LPCSTR appName = GetNameA();	// pre-calc ansi name - kinda hack :(
		LONGLONG initialTime = *GetInitialTimePtr();
		LONGLONG totalProgress = 0;

		concurrency::combinable<Driver*> parDriver([&filterA]() { auto d = new Driver(); d->Prepare(filterA, "input"); return d; });
		concurrency::combinable<vector<BYTE*>> rgFilteredMessages;
		concurrency::cancellation_token_source cts;
		concurrency::run_with_cancellation_token([&]()
		{
			concurrency::parallel_for(size_t(0), totalMessages, [&](size_t i)
			{
				Driver& driver = *parDriver.local();
				CLoggerItemBase::SetVariables(driver, m_rgMessages[i], initialTime, 0, appName);

				if (driver.Evaluate())
				{
					err = driver.lastError;	// not safe
					cts.cancel();
				}
				else if (m_ct.is_cancelled())
				{
					cts.cancel();
				}
				else if (driver._bisrez != 0.0)
				{
					rgFilteredMessages.local().push_back(m_rgMessages[i]);
				}
			});
		}, cts.get_token());

		if (err != 0)
		{
			CA2W wsLastError(err);
			MessageBox(NULL, wsLastError, L"Error", MB_ICONERROR);
			m_completionCode = 1;
		}
		else
		{
			rgFilteredMessages.combine_each([&](vector<BYTE*>& local)
			{
				std::move(local.begin(), local.end(), std::back_inserter(m_rgFilteredMessages));
			});

			std::sort(m_rgFilteredMessages.begin(), m_rgFilteredMessages.end(), [](BYTE* baseAddrA, BYTE* baseAddrB) {
				TRC_TYPE trcTypeA = (TRC_TYPE)baseAddrA[4];
				TRC_TYPE trcTypeB = (TRC_TYPE)baseAddrB[4];

				TRC_TIME tsA = (trcTypeA == TRC_ERROR) ? CProfERRORmsg((BYTE*)baseAddrA, true).TimeStamp : CProfSQLmsg((BYTE*)baseAddrA, true).TimeStamp;
				TRC_TIME tsB = (trcTypeB == TRC_ERROR) ? CProfERRORmsg((BYTE*)baseAddrB, true).TimeStamp : CProfSQLmsg((BYTE*)baseAddrB, true).TimeStamp;

				return tsA > tsB;
			});
		}
	}
	else
#endif USE_PARF
	{
		for (size_t i = 0; m_ct && i < totalMessages; i++, (0 == (i % 1000)) ? m_filterProgressSignal((i * 100) / totalMessages) : 0)
		{
			CLoggerItemBase::SetVariables(m_driver, m_rgMessages[i], *GetInitialTimePtr(), 0, GetNameA());

			if (m_driver.Evaluate())
			{
				m_completionCode = 1;
				break;
			}

			if (m_driver._bisrez != 0.0)
			{
				m_rgFilteredMessages.push_back(m_rgMessages[i]);
			}

			//Sleep(1);	// make it slow for debugging
		}
	}

	m_filterProgressSignal(FILTER_FINISH);
}

void CCsvFileLoggerItem::ClearLog()
{
	m_errorCnt = 0;
	
	for_each (m_rgMessages.begin(), m_rgMessages.end(), [=](BYTE* p) -> void { delete [] p; });

	m_rgMessages.clear();
}
#pragma endregion
