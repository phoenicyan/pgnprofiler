/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"

#include "DebugHelper.h"
#include "LogContext.h"
#include "Profiler/RemMsg.h"
#include "Profiler/OneInstance.h"
#include "THREADSYNC/ReaderWriter.h"

#include <iostream>
#include <iomanip>
#include <sstream>
using namespace std;

#include <atlcomtime.h>		// COleDateTime

#define OUT_BUFSIZE (24*1024*1024)
#define IN_BUFSIZE (10*1024)
#define PIPE_TIMEOUT 5000

class DH_PIPE;

typedef void (*ON_CLIENT_CONNECTED_TO_PIPE)(DH_PIPE*);
typedef void (*ON_CLIENT_DISCONNECTED_FROM_PIPE)(DH_PIPE*);

static LARGE_INTEGER liCheckTraceTime, liFreq;

static LONGLONG g_qpcFreq;
static LONGLONG g_qpcInitialPerfCount;
static FILETIME g_InitialTime;

///////////////////////////////////////////////////////////////////////////////

enum DH_STATE
{
	PIPE_INITIAL,
	PIPE_CONNECTED,
};

class CDHPipes;

class DH_PIPE
{
	friend class CDHPipes;

	CRITICAL_SECTION m_pipeCS;	// controls access to the pipe and buffer below from multiple threads
	DH_STATE m_state;
	HANDLE m_hPipe;
	HANDLE m_evtStopPipeMonitor, m_evtPipeMonitorFinished, m_hCommThread;
	OVERLAPPED m_op;
	DWORD m_dwTraceLevel;	// 0 - only SQL statements; 1,2,3 - comments levels.
	DWORD m_dwParamFormat;	// 0 - default; 1 - no truncation; 2 - pgAdmin compatible.
	ON_CLIENT_CONNECTED_TO_PIPE m_OnClientConnected;
	ON_CLIENT_DISCONNECTED_FROM_PIPE m_OnClientDisconnected;
#if _DEBUG
	wstring sPipename;
#endif

	BYTE m_inbuffer[IN_BUFSIZE];

public:
	DH_PIPE(ON_CLIENT_CONNECTED_TO_PIPE OnClientConnected, ON_CLIENT_DISCONNECTED_FROM_PIPE OnClientDisconnected)
		: m_state(PIPE_INITIAL), m_dwTraceLevel(0), m_dwParamFormat(PRMFMT_DEFAULT)
		, m_evtStopPipeMonitor(INVALID_HANDLE_VALUE), m_evtPipeMonitorFinished(INVALID_HANDLE_VALUE), m_hCommThread(INVALID_HANDLE_VALUE)
		, m_OnClientConnected(OnClientConnected), m_OnClientDisconnected(OnClientDisconnected)
		//, m_pendingBytes(0), m_pendingWritePos(0), m_pendingLengthUnknown(false)
	{
		InitializeCriticalSection(&m_pipeCS);

		memset(&m_op, 0, sizeof(m_op));

		// Create communication pipe
		WCHAR buf[16];
#if !_DEBUG
		wstring sPipename;
#endif
		sPipename.assign(L"\\\\.\\pipe\\" PGNPIPE_PREFIX);
		sPipename += _itow(GetCurrentProcessId(), buf, 10);
		m_hPipe = CreateNamedPipe(sPipename.c_str(),
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, // rw asynch access allowed
			PIPE_TYPE_BYTE | PIPE_WAIT,	// blocking mode; according to MSDN: "Note that nonblocking mode ... should not be used to achieve asynchronous I/O with named pipes"; 
										// also, when PIPE_NOWAIT used, ReadFile returns immediately with ERROR_NO_DATA, i.e. no overlapped operation occur.
			PIPE_UNLIMITED_INSTANCES, // max. instances
			OUT_BUFSIZE, // output buffer size
			IN_BUFSIZE, // input buffer size
			PIPE_TIMEOUT, // client time-out
			NULL); // no security attribute

		if (m_hPipe == INVALID_HANDLE_VALUE)
		{
			//DWORD err = GetLastError();
			ATLASSERT(0);
		}
	}

	~DH_PIPE()
	{
		if (IsCommActive())
			StopComm();

		DisconnectNamedPipe(m_hPipe);
		m_hPipe = INVALID_HANDLE_VALUE;

		DeleteCriticalSection(&m_pipeCS);
	}

	void StartComm()
	{
		if (IsCommActive())
			return;		// already started

		ATLTRACE(atlTraceDBProvider, 2, _T("DH_PIPE::StartComm()\n"));

		m_evtStopPipeMonitor = CreateEvent(NULL, TRUE, FALSE, NULL);
		m_evtPipeMonitorFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
		m_op.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		m_hCommThread = CreateThread(0, 0, PipeMonitorThread, this, CREATE_SUSPENDED, NULL);
		ResumeThread(m_hCommThread);
		Sleep(0);
	}

	void StopComm()
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("DH_PIPE::StopComm()\n"));
		
		SetEvent(m_evtStopPipeMonitor);

		DWORD waitResult = WaitForSingleObject(m_evtPipeMonitorFinished, 50);
		if (waitResult != WAIT_OBJECT_0)
		{
			ATLTRACE(atlTraceDBProvider, 0, _T("** DH_PIPE::StopComm: TerminateThread\n"));
			TerminateThread(m_hCommThread, 0);
		}

		CloseHandle(m_evtStopPipeMonitor);
		CloseHandle(m_evtPipeMonitorFinished);
		CloseHandle(m_op.hEvent);
		m_evtStopPipeMonitor = m_evtPipeMonitorFinished = m_op.hEvent = m_hCommThread = INVALID_HANDLE_VALUE;
	}

	inline HANDLE GetPipeHandle() const { return m_hPipe; }

	inline bool IsCommActive() const { return m_evtStopPipeMonitor != INVALID_HANDLE_VALUE; }

	inline DH_STATE GetPipeState() const { return m_state; }

	inline DWORD GetTraceLevel() const { return m_dwTraceLevel;	}

	inline DWORD GetParamFormat() const { return m_dwParamFormat; }

private:
	static DWORD WINAPI PipeMonitorThread(LPVOID pParam);
};

///////////////////////////////////////////////////////////////////////////////

class CDHPipes
{
	ReadWriteProtector	m_rwpPipes;
	int m_availablePipes;
	BOOL m_bConnected;				// cached value that indicates if any pipes are connected
	unsigned m_lastCheck;			// time of last check for connected pipes (ms)
	map<HANDLE, DH_PIPE*> m_pipes;
	BYTE m_outbuffer[OUT_BUFSIZE];	// buffer data before sending to pipes

	CDHPipes(CDHPipes&);

public:
	CDHPipes() : m_availablePipes(0), m_bConnected(FALSE), m_lastCheck(0)
	{}

	~CDHPipes()
	{}

	DH_PIPE* AddPipeInstance()
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("CDHPipes::AddPipeInstance()\n"));

		DH_PIPE* pipe = new DH_PIPE(OnClientConnected, OnClientDisconnected);

		RWPWriter sync(m_rwpPipes);

		m_pipes.insert({ pipe->GetPipeHandle(), pipe });

		m_availablePipes++;

		return pipe;
	}

	inline int GetAvailablePipes() const { return m_availablePipes; }

	inline int DecrementAvailablePipes() { if (m_availablePipes > 0) --m_availablePipes;  return m_availablePipes; }

	inline int IncrementAvailablePipes() { return ++m_availablePipes; }

	inline BYTE* GetOutBuffer() { return &m_outbuffer[0]; }

	static void OnClientConnected(DH_PIPE* pipe);
	
	static void OnClientDisconnected(DH_PIPE* pipe);

	DWORD GetAggregateParamFormat()
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("CDHPipes::GetAggregateParamFormat()\n"));

		DWORD dwParamFormat = PRMFMT_DEFAULT;

		RWPReader sync(m_rwpPipes);

		for_each(m_pipes.begin(), m_pipes.end(), [&dwParamFormat](pair<HANDLE, DH_PIPE*> p)
		{
			DWORD temp = p.second->GetParamFormat();
			if (dwParamFormat < temp)
				dwParamFormat = temp;
		});

		return dwParamFormat;
	}

	BOOL IsTraceEnabled()
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		unsigned cur_time = (1000LL * now.QuadPart) / g_qpcFreq;
		unsigned elapsed = m_lastCheck - cur_time;
		if (elapsed > 50)
		{
			InterlockedExchange(&m_lastCheck, cur_time);

			RWPReader sync(m_rwpPipes);

			std::all_of(m_pipes.begin(), m_pipes.end(), [&](pair<HANDLE, DH_PIPE*> p)
			{
				if (PIPE_CONNECTED == p.second->GetPipeState())
				{
					m_bConnected = TRUE;
					return false;	// break loop
				}

				return true;
			});
		}

		return m_bConnected;
	}

	void Clear()
	{
		ATLTRACE(atlTraceDBProvider, 0, _T("CDHPipes::Clear()\n"));

		RWPWriter sync(m_rwpPipes);

		for_each(m_pipes.begin(), m_pipes.end(), [](pair<HANDLE, DH_PIPE*> p)
		{
			p.second->StopComm();
		});

		m_pipes.clear();

		m_availablePipes = 0;
	}

#pragma region Log_functions
	void LogClientSQL(CLogContext* logContext, int cmdType, const char* sClientSQL, const char* sExecutedSQL, CMDPROFILE& profile, LONG cmdId)
	{
		RWPWriter sync(m_rwpPipes);

		for_each(m_pipes.begin(), m_pipes.end(), [&](pair<HANDLE, DH_PIPE*> p)
		{
			if (p.second->GetPipeState() != PIPE_CONNECTED)
				return;

			CProfSQLmsg profmsg(GetOutBuffer(), false);
			profmsg.TrcType = (BYTE)(logContext ? logContext->_trcType : TRC_NONE);
			profmsg.CmdType = (BYTE)cmdType;
			profmsg.Cursor = (BYTE)profile._cursor;

			profmsg.License = GLB_LICCHIP;

			LONGLONG tsDelta = (LONGLONG)((double)(profile._timestamp - g_qpcInitialPerfCount) * 10000000L / g_qpcFreq);
			profmsg.TimeStamp = (*(LONGLONG*)&g_InitialTime) + tsDelta;

			profmsg.ParseTime = profile._parse;
			profmsg.PrepareTime = profile._prepare;
			profmsg.ExecTime = profile._execute;
			profmsg.GetdataTime = profile._getdata;
			profmsg.NumRows = profile._rows;

			profmsg.SessionId = (SHORT)(logContext ? logContext->GetSessionId() : -1);
			profmsg.CommandId = (SHORT)cmdId;

			if (logContext != 0)
			{
				const string& database(logContext->GetDbName());
				profmsg.Database = PROFMSG_TEXT(database.length() + 1, database.c_str());

				const string& user(logContext->GetUser());
				profmsg.UserName = PROFMSG_TEXT(user.length() + 1, user.c_str());
			}
			else
			{
				profmsg.Database = PROFMSG_TEXT(1, "");
				profmsg.UserName = PROFMSG_TEXT(1, "");
			}

			if (0 == sClientSQL)
				sClientSQL = "";

			if (0 == sExecutedSQL)
				sExecutedSQL = "";

			profmsg.ClientSQL = PROFMSG_TEXT(strlen(sClientSQL) + 1, sClientSQL);
			profmsg.ExecutedSQL = PROFMSG_TEXT(strlen(sExecutedSQL) + 1, sExecutedSQL);

			DWORD payloadSize = profmsg.FlushPayload();

			DWORD dwBytes = 0;
			if (!WriteFile(p.second->GetPipeHandle(), GetOutBuffer(), payloadSize, &dwBytes, 0))
			{
				//DWORD err = GetLastError();
				//ATLASSERT(0);
				profile._traceEnabled = FALSE;		// looks like Profiler shut down unexpectedly, do not continue logging
			}
		});
	}

	void LogError(CLogContext* logContext, const char* sClientSQL, const char* sError, CMDPROFILE& profile, LONG cmdId)
	{
		RWPWriter sync(m_rwpPipes);

		for_each(m_pipes.begin(), m_pipes.end(), [&](pair<HANDLE, DH_PIPE*> p)
		{
			if (p.second->GetPipeState() != PIPE_CONNECTED)
				return;

			CProfERRORmsg profmsg(GetOutBuffer(), false);
			profmsg.TrcType = TRC_ERROR;
			profmsg.Cursor = (BYTE)profile._cursor;
			profmsg.Reserved = 0;

			profmsg.License = GLB_LICCHIP;

			LONGLONG tsDelta = (LONGLONG)((double)(profile._timestamp - g_qpcInitialPerfCount) * 10000000L / g_qpcFreq);
			profmsg.TimeStamp = (*(LONGLONG*)&g_InitialTime) + tsDelta;

			profmsg.SessionId = (SHORT)(logContext ? logContext->GetSessionId() : -1);
			profmsg.CommandId = (SHORT)cmdId;

			if (logContext != 0)
			{
				const string& database(logContext->GetDbName());
				profmsg.Database = PROFMSG_TEXT(database.length() + 1, database.c_str());

				const string& user(logContext->GetUser());
				profmsg.UserName = PROFMSG_TEXT(user.length() + 1, user.c_str());
			}
			else
			{
				profmsg.Database = PROFMSG_TEXT(1, "");
				profmsg.UserName = PROFMSG_TEXT(1, "");
			}

			if (sError == nullptr)
				sError = "<null>";

			if (sClientSQL == nullptr)
				sClientSQL = "<null>";

			profmsg.ErrorText = PROFMSG_TEXT(strlen(sError) + 1, sError);
			profmsg.ClientSQL = PROFMSG_TEXT(strlen(sClientSQL) + 1, sClientSQL);

			DWORD payloadSize = profmsg.FlushPayload();

			DWORD dwBytes = 0;
			if (!WriteFile(p.second->GetPipeHandle(), GetOutBuffer(), payloadSize, &dwBytes, 0))
			{
				//DWORD err = GetLastError();
				//ATLASSERT(0);
				profile._traceEnabled = FALSE;		// looks like Profiler shut down unexpectedly, do not continue logging
			}
		});
	}

	void LogComment(CLogContext* logContext, COMMENT_LEVEL level, const char* sComment, LONG cmdId, va_list argList)
	{
		if (0 == sComment)
			sComment = "";

		char szBuffer[4096] = { 0 };

		RWPWriter sync(m_rwpPipes);

		for_each(m_pipes.begin(), m_pipes.end(), [&](pair<HANDLE, DH_PIPE*> p)
		{
			if (level > p.second->GetTraceLevel())
				return;

			if (p.second->GetPipeState() != PIPE_CONNECTED)
				return;

			if (!szBuffer[0])
			{
				_vsnprintf(szBuffer, _countof(szBuffer), sComment, argList);
				szBuffer[_countof(szBuffer) - 1] = 0;
			}

			CProfSQLmsg profmsg(GetOutBuffer(), false);
			profmsg.TrcType = (BYTE)TRC_COMMENT;
			profmsg.CmdType = (BYTE)0;
			profmsg.Cursor = CURSOR_FORWARD_ONLY;

			profmsg.License = GLB_LICCHIP;

			LONGLONG b;
			QueryPerformanceCounter((LARGE_INTEGER*)&b);
			LONGLONG tsDelta = (LONGLONG)((double)(b - g_qpcInitialPerfCount) * 10000000L / g_qpcFreq);
			profmsg.TimeStamp = (*(LONGLONG*)&g_InitialTime) + tsDelta;

			profmsg.ParseTime = 0;
			profmsg.PrepareTime = 0;
			profmsg.ExecTime = 0;
			profmsg.GetdataTime = 0;
			profmsg.NumRows = 0;

			profmsg.SessionId = (SHORT)(logContext ? logContext->GetSessionId() : -1);
			profmsg.CommandId = (SHORT)cmdId;

			if (logContext != 0)
			{
				const string& database(logContext->GetDbName());
				profmsg.Database = PROFMSG_TEXT(database.length() + 1, database.c_str());

				const string& user(logContext->GetUser());
				profmsg.UserName = PROFMSG_TEXT(user.length() + 1, user.c_str());
			}
			else
			{
				profmsg.Database = PROFMSG_TEXT(1, "");
				profmsg.UserName = PROFMSG_TEXT(1, "");
			}

			profmsg.ClientSQL = PROFMSG_TEXT(strlen(szBuffer) + 1, szBuffer);
			profmsg.ExecutedSQL = PROFMSG_TEXT(1, "");

			DWORD payloadSize = profmsg.FlushPayload();

			DWORD dwBytes = 0;
			if (!WriteFile(p.second->GetPipeHandle(), GetOutBuffer(), payloadSize, &dwBytes, 0))
			{
				//DWORD err = GetLastError();
				//ATLASSERT(0);
				//profile._traceEnabled = FALSE;		// looks like Profiler shut down unexpectedly, do not continue logging
			}
		});
	}

	void LogStartup(CLogContext* logContext, const char* sMessage, va_list argList)
	{
		char szBuffer[4096] = { 0 };

		RWPWriter sync(m_rwpPipes);

		for_each(m_pipes.begin(), m_pipes.end(), [&](pair<HANDLE, DH_PIPE*> p)
		{
			if (p.second->GetPipeState() != PIPE_CONNECTED)
				return;

			if (!szBuffer[0])
			{
				_vsnprintf(szBuffer, _countof(szBuffer), sMessage, argList);
				szBuffer[_countof(szBuffer) - 1] = 0;
			}

			CProfSQLmsg profmsg(GetOutBuffer(), false);
			profmsg.TrcType = (BYTE)TRC_STARTUP;
			profmsg.CmdType = (BYTE)0;
			profmsg.Cursor = CURSOR_FORWARD_ONLY;

			profmsg.License = GLB_LICCHIP;

			LONGLONG b;
			QueryPerformanceCounter((LARGE_INTEGER*)&b);
			LONGLONG tsDelta = (LONGLONG)((double)(b - g_qpcInitialPerfCount) * 10000000L / g_qpcFreq);
			profmsg.TimeStamp = (*(LONGLONG*)&g_InitialTime) + tsDelta;

			profmsg.ParseTime = 0;
			profmsg.PrepareTime = 0;
			profmsg.ExecTime = 0;
			profmsg.GetdataTime = 0;
			profmsg.NumRows = 0;

			profmsg.SessionId = (SHORT)(logContext ? logContext->GetSessionId() : -1);
			profmsg.CommandId = (SHORT)0;

			if (logContext != 0)
			{
				const string& database(logContext->GetDbName());
				profmsg.Database = PROFMSG_TEXT(database.length() + 1, database.c_str());

				const string& user(logContext->GetUser());
				profmsg.UserName = PROFMSG_TEXT(user.length() + 1, user.c_str());
			}
			else
			{
				profmsg.Database = PROFMSG_TEXT(1, "");
				profmsg.UserName = PROFMSG_TEXT(1, "");
			}

			profmsg.ClientSQL = PROFMSG_TEXT(strlen(szBuffer) + 1, szBuffer);
			profmsg.ExecutedSQL = PROFMSG_TEXT(1, "");

			DWORD payloadSize = profmsg.FlushPayload();

			DWORD dwBytes = 0;
			if (!WriteFile(p.second->GetPipeHandle(), GetOutBuffer(), payloadSize, &dwBytes, 0))
			{
				//DWORD err = GetLastError();
				//ATLASSERT(0);
				//profile._traceEnabled = FALSE;		// looks like Profiler shut down unexpectedly, do not continue logging
			}
		});
	}

	void LogMsg(const BYTE* baseAddr)
	{
		RWPWriter sync(m_rwpPipes);

		for_each(m_pipes.begin(), m_pipes.end(), [&](pair<HANDLE, DH_PIPE*> p)
		{
			if (TRC_ERROR == (TRC_TYPE)baseAddr[4])
			{
				CProfERRORmsg srcmsg((BYTE*)baseAddr, true);
				CProfERRORmsg dstmsg(GetOutBuffer(), false);
				dstmsg.TrcType = TRC_ERROR;
				dstmsg.Cursor = srcmsg.Cursor;
				dstmsg.Reserved = 0;

				dstmsg.License = GLB_LICCHIP;

				dstmsg.TimeStamp = srcmsg.TimeStamp;

				dstmsg.SessionId = srcmsg.SessionId;
				dstmsg.CommandId = srcmsg.CommandId;

				dstmsg.Database = srcmsg.Database;
				dstmsg.UserName = srcmsg.UserName;

				dstmsg.ErrorText = srcmsg.ErrorText;
				dstmsg.ClientSQL = srcmsg.ClientSQL;

				DWORD dwBytes = 0;
				if (!WriteFile(p.second->GetPipeHandle(), GetOutBuffer(), dstmsg.FlushPayload(), &dwBytes, 0))
				{
					DWORD err = GetLastError();
					err = 0;
					//ATLASSERT(0);
				}
			}
			else
			{
				CProfSQLmsg srcmsg((BYTE*)baseAddr, true);

				CProfSQLmsg dstmsg(GetOutBuffer(), false);
				dstmsg.TrcType = srcmsg.TrcType;
				dstmsg.CmdType = srcmsg.CmdType;
				dstmsg.Cursor = srcmsg.Cursor;

				dstmsg.License = GLB_LICCHIP;

				dstmsg.TimeStamp = srcmsg.TimeStamp;

				dstmsg.ParseTime = srcmsg.ParseTime;
				dstmsg.PrepareTime = srcmsg.PrepareTime;
				dstmsg.ExecTime = srcmsg.ExecTime;
				dstmsg.GetdataTime = srcmsg.GetdataTime;
				dstmsg.NumRows = srcmsg.NumRows;

				dstmsg.SessionId = srcmsg.SessionId;
				dstmsg.CommandId = srcmsg.CommandId;

				dstmsg.Database = srcmsg.Database;
				dstmsg.UserName = srcmsg.UserName;

				dstmsg.ClientSQL = srcmsg.ClientSQL;
				dstmsg.ExecutedSQL = srcmsg.ExecutedSQL;

				DWORD dwBytes = 0;
				if (!WriteFile(p.second->GetPipeHandle(), GetOutBuffer(), dstmsg.FlushPayload(), &dwBytes, 0))
				{
					DWORD err = GetLastError();
					err = 0;
					//ATLASSERT(0);
				}
			}
		});
	}
#pragma endregion Log_functions
};

static CDHPipes m_dhPipes;


static __declspec(thread) char BUFFER[4000];

string StringFormat(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	_vsnprintf(BUFFER, _countof(BUFFER), format, args);
	BUFFER[_countof(BUFFER) - 1] = 0;

	return BUFFER;
}


void CDHPipes::OnClientConnected(DH_PIPE* pipe)
{
#if _DEBUG
	ATLTRACE(atlTraceDBProvider, 1, _T("CDHPipes::OnClientConnected(): pipe %s\n"), pipe->sPipename.c_str());
#endif

	if (m_dhPipes.GetAvailablePipes() == 1)
	{
		m_dhPipes.DecrementAvailablePipes();

		DH_PIPE* pipe = m_dhPipes.AddPipeInstance();
		pipe->StartComm();
	}
}

void CDHPipes::OnClientDisconnected(DH_PIPE* pipe)
{
	ATLTRACE(atlTraceDBProvider, 1, _T("CDHPipes::OnClientDisconnected()\n"));

	m_dhPipes.IncrementAvailablePipes();
}

static void LogStartup(CLogContext* logContext, const char* sComment, ...);

#define BUFSIZE		4096

///////////////////////////////////////////////////////////////////////////////
// Creates named pipe to send profiling information.
// Returns: 0 if success, otherwise error code from GetLastError().
DWORD ProfilerClientInit()
{
	// save QueryPerformanceFrequency (it never changes while computer runs)
	LARGE_INTEGER liFreq;
	QueryPerformanceFrequency(&liFreq);
	g_qpcFreq = liFreq.QuadPart;

	// remember initialization time
	GetSystemTimeAsFileTime(&g_InitialTime);
	QueryPerformanceCounter((LARGE_INTEGER*)&g_qpcInitialPerfCount);

//DebugBreak();

	// create profiler pipe
	DH_PIPE* pipe = m_dhPipes.AddPipeInstance();
	pipe->StartComm();

	// notify PGNProfiler (if any) that new instance of provider is started
	COneInstance guard(_T("Global\\")_T(PROFILERSGN));
	DWORD waitrez = ::WaitForSingleObject(guard.m_hInstanceGuard, 0);
	if (NULL == guard.m_hInstanceGuard || WAIT_TIMEOUT == waitrez)
	{
		StartupComm sc(1, GetCurrentProcessId());
		LPTSTR lpszWrite = (LPTSTR)sc.asBase64();
		TCHAR chReadBuf[BUFSIZE];
		BOOL fSuccess;
		DWORD cbRead, err;
		LPTSTR lpszPipename = (LPTSTR)TEXT("\\\\.\\pipe\\pgnprof_comm");
		
		do {
			fSuccess = CallNamedPipe(
				lpszPipename,				// pipe name 
				lpszWrite,					// message to server 
				(lstrlen(lpszWrite) + 1) * sizeof(TCHAR), // message length 
				chReadBuf,					// buffer to receive reply 
				BUFSIZE * sizeof(TCHAR),	// size of read buffer 
				&cbRead,					// number of bytes read 
				2000);						// waits for 2 seconds
			err = GetLastError();
			if (fSuccess || err == ERROR_MORE_DATA)
			{
				//_tprintf(TEXT("%s\n"), chReadBuf);
				LogStartup(NULL, "Process with PID=%d started", GetCurrentProcessId());
			}
			else
			{
				ATLTRACE(atlTraceDBProvider, 0, _T("** ProfilerClientInit: CallNamedPipe failed with error %d\n"), err);
			}
		} while (!fSuccess && err == ERROR_PIPE_BUSY);
	}
	else
	{
		// Profiler is not running
	}

	return 0;
}


DWORD ProfilerClientUninit()
{
	m_dhPipes.Clear();

	DWORD recipients = BSM_APPLICATIONS;
	BroadcastSystemMessage(BSF_POSTMESSAGE, &recipients, WM_PGNPROF_STATUS, PGNPROF_STATUS_SHUTDOWN, GetCurrentProcessId());

	Sleep(0);

	return 0;
}


DWORD WINAPI DH_PIPE::PipeMonitorThread(LPVOID pParam)
{
	DH_PIPE* pipe = (DH_PIPE*)pParam;
	HANDLE hSavedEvent = pipe->m_op.hEvent;

	for (;;)
	{
		// prepare OP object for operation
		memset(&pipe->m_op, 0, sizeof(pipe->m_op));
		pipe->m_op.hEvent = hSavedEvent;

		if (PIPE_INITIAL == pipe->GetPipeState())
		{
			// spawn connect
			if (ConnectNamedPipe(pipe->m_hPipe, &pipe->m_op))
			{
				DWORD err = GetLastError();
				ATLTRACE(atlTraceDBProvider, 0, _T("** CheckPipe(): overlapped connect returned error %d\n"), err);
				break;//???
				//return FALSE;
			}

			DWORD err = GetLastError();
			if (err == ERROR_PIPE_CONNECTED)
			{
				pipe->m_state = PIPE_CONNECTED;
				pipe->m_OnClientConnected(pipe);
				continue;
			}

			ATLASSERT(err == ERROR_IO_PENDING);
		}
		else
		{
			// spawn read
			BOOL rez = ReadFile(pipe->m_hPipe, pipe->m_inbuffer, sizeof(pipe->m_inbuffer), NULL, &pipe->m_op);
			if (rez != 0 || GetLastError() == ERROR_IO_PENDING)
			{
				//::PostMessage(m_mainWindow, MYMSG_DATA_READ, (WPARAM)processLogger.GetPipe(), (LPARAM)0);
			}
			else
			{
				//processLogger.ClosePipe();
				//::PostMessage(m_mainWindow, WM_PGNPROF_STATUS, 0, 0);
			}
		}

		HANDLE handles[2] = { pipe->m_op.hEvent, pipe->m_evtStopPipeMonitor };
		DWORD waitRez = WaitForMultipleObjects(_countof(handles), handles, FALSE, PIPE_TIMEOUT);
		if (waitRez == WAIT_TIMEOUT)
		{
			// check if clients can connect to the pipe
			CancelIo(pipe->m_hPipe);
			continue;
		}

		if (waitRez != WAIT_OBJECT_0)
		{
			// stop event signalled, or other condition result in thread termination
			ATLTRACE(atlTraceDBProvider, 0, _T("** DH_PIPE::PipeMonitorThread: termination\n"));
			break;
		}

		// handle incoming command
		DWORD numBytesLeft = 0;
		BOOL rez = GetOverlappedResult(pipe->m_hPipe, &pipe->m_op, &numBytesLeft, FALSE);
		if (0 == rez || GetLastError() == ERROR_BROKEN_PIPE)
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"DH_PIPE::PipeMonitorThread: peer application closed the pipe\n");

			DisconnectNamedPipe(pipe->m_hPipe);

			if (PIPE_CONNECTED == pipe->m_state)
			{
				pipe->m_state = PIPE_INITIAL;
				pipe->m_OnClientDisconnected(pipe);
			}
#if _DEBUG
			DWORD err = GetLastError();
#endif
			continue;
		}

		if (PIPE_INITIAL == pipe->m_state)
		{
			ATLTRACE2(atlTraceDBProvider, 1, L"DH_PIPE::PipeMonitorThread: connection was made successfully!\n");
			pipe->m_state = PIPE_CONNECTED;
			pipe->m_OnClientConnected(pipe);
		}
		else
		{
			ATLTRACE2(atlTraceDBProvider, 1, L"DH_PIPE::PipeMonitorThread: %d bytes received\n", numBytesLeft);

			BYTE* ptr = &pipe->m_inbuffer[0];
			RemMsg cmd;
			cmd.SetFromReceivedData(ptr, numBytesLeft);
			switch (cmd.m_msgID)
			{
			case MI_TRACELEVEL_REQ:
				cmd >> pipe->m_dwTraceLevel;
				ATLTRACE2(atlTraceDBProvider, 0, L"DH_PIPE::PipeMonitorThread: set trace level to %d.\n", pipe->m_dwTraceLevel);
				break;

			case MI_PRMFORMAT_REQ:
				cmd >> pipe->m_dwParamFormat;
				ATLTRACE2(atlTraceDBProvider, 0, L"DH_PIPE::PipeMonitorThread: set param format to %d.\n", pipe->m_dwParamFormat);
				break;

			default:
				ATLTRACE2(atlTraceDBProvider, 0, L"** DH_PIPE::PipeMonitorThread: unknown command %d.\n", cmd.m_msgID);
			}
		}

		::ResetEvent(pipe->m_op.hEvent);
	}

	SetEvent(pipe->m_evtPipeMonitorFinished);

	return 0;
}


BOOL IsTraceEnabled()
{
	return m_dhPipes.IsTraceEnabled();
}

void LogClientSQL(CLogContext* logContext, int cmdType, const char* sClientSQL, const char* sExecutedSQL, CMDPROFILE& profile, LONG cmdId)
{
	if (profile._was_logged)
		return;

	m_dhPipes.LogClientSQL(logContext, cmdType, sClientSQL, sExecutedSQL, profile, cmdId);

	profile._rows = 0;

	profile._begin.QuadPart = 0;
	profile._end.QuadPart = 0;
	profile._elapsed = 0;

	profile._timestamp = 0;
	profile._parse = 0;
	profile._prepare = 0;
	profile._execute = 0;
	profile._getdata = 0;
	profile._was_logged = TRUE;
}

void LogError(CLogContext* logContext, const char* sClientSQL, const char* sError, CMDPROFILE& profile, LONG cmdId)
{
	if (profile._was_logged)
		return;

	m_dhPipes.LogError(logContext, sClientSQL, sError, profile, cmdId);
	
	profile._rows = 0;

	profile._begin.QuadPart = 0;
	profile._end.QuadPart = 0;
	profile._elapsed = 0;

	profile._timestamp = 0;
	profile._parse = 0;
	profile._prepare = 0;
	profile._execute = 0;
	profile._getdata = 0;
	profile._was_logged = TRUE;
}

extern "C"
void LogComment(CLogContext* logContext, COMMENT_LEVEL level, const char* sComment, LONG cmdId, ...)
{
	va_list argList;
	va_start(argList, cmdId);
	m_dhPipes.LogComment(logContext, level, sComment, cmdId, argList);
	va_end(argList);
}

static void LogStartup(CLogContext* logContext, const char* sComment, ...)
{
	va_list argList;
	va_start(argList, sComment);
	m_dhPipes.LogStartup(logContext, sComment, argList);
	va_end(argList);
}

void LogMsg(const BYTE* baseAddr)
{
	m_dhPipes.LogMsg(baseAddr);
}

template<typename T>
void nextParam(T** pstr, basic_string<T>& result)
{
	bool instring = false;
	bool foundParam = false;
	T* pstart = *pstr;
	size_t len = 0;
	result.clear();

	for(; **pstr && !foundParam; len++, (*pstr)++)
	{
		if (L'"' == **pstr || L'\'' == **pstr)
		{
			const T endstring = **pstr;
			instring = true;
			(*pstr)++;
			len++;
			for (; instring && **pstr; len++,(*pstr)++) 
			{
				if (**pstr == endstring && **pstr != *(*pstr+1)) // ignore if two '' or "" follow each other
					instring = false;
			}
		}
#if VERTICA
		else if (L'?' == **pstr)
#else
		else if (L'$' == **pstr)
#endif
			foundParam = true;
	}

	if (foundParam)
	{
		//result.reserve(len);
		result.assign(pstart, len-1);
		//result[len-1] = 0;
		while (isdigit(**pstr)) (*pstr)++;
	}
	else
		result = pstart;
}


void j2date(int jd, short *year, unsigned short *month, unsigned short *day);
void dt2time(PGTimeStamp time, unsigned short *hour, unsigned short *min, unsigned short *sec, ULONG *fsec);


string FormatPreparedClientSQL(const char* szSQL, DBPARAMS* pParams, DBCOUNTITEM cBindings, DBBINDING* rgBindings)
{
	string query("EXECUTE \"");
	query += szSQL;
	
	if (0 == cBindings)
	{
		query += '"';
	}
	else
	{
		query += "\"(";
		char buf[32] = "$";
		for (DBCOUNTITEM i=0; i < cBindings; i++)
		{
			if (i) query += ',';
			_itoa(i, &buf[1], 10);
			query += buf;
		}
		query += ')';
	}
	
	return FormatClientSQL(query.c_str(), pParams, cBindings, rgBindings);
}

extern void AppendChar(string& dst, unsigned char c);
static char hexchars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

int date2j(int y, int m, int d)
{
	if (m > 2)
	{
		m += 1;
		y += 4800;
	}
	else
	{
		m += 13;
		y += 4799;
	}

	int century = y / 100;
	int julian = y * 365 - 32167;
	julian += y / 4 - century + century / 4;
	julian += 7834 * m / 256 + d;

	return julian;
}

// return time in milionth of a second
__int64 time2t(const int hour, const int min, const int sec, ULONG fraction)
{
	return (((((__int64)hour * MINS_PER_HOUR) + min) * SECS_PER_MINUTE) + sec) * 1000000 + BILLIONTH_TO_MILLIONTH(fraction);
}

void j2date(int jd, short* year, unsigned short* month, unsigned short* day)
{
	unsigned int julian;
	unsigned int quad;
	unsigned int extra;
	int			y;

	julian = jd;
	julian += 32044;
	quad = julian / 146097;
	extra = (julian - quad * 146097) * 4 + 3;
	julian += 60 + quad * 3 + extra / 146097;
	quad = julian / 1461;
	julian -= quad * 1461;
	y = julian * 4 / 1461;
	julian = ((y != 0) ? ((julian + 305) % 365) : ((julian + 306) % 366))
		+ 123;
	y += quad * 4;
	*year = y - 4800;
	quad = julian * 2141 / 65536;
	*day = julian - 7834 * quad / 256;
	*month = (quad + 10) % 12 + 1;
}

void dt2time(PGTimeStamp time, unsigned short* hour, unsigned short* min, unsigned short* sec, ULONG* fsec)
{
	*hour = (time / 3600000000i64);
	time -= ((*hour) * 3600000000i64);
	*min = (time / 60000000i64);
	time -= ((*min) * 60000000i64);
	*sec = (time / 1000000i64);
	*fsec = (time - (*sec * 1000000i64));
}

class CDbType2StringStream : public stringstream
{
	const DWORD TXTTRUNCLIM, BINTRUNCLIM;
	DWORD _dwParamFormat;

public:
	CDbType2StringStream(DWORD dwParamFormat) : TXTTRUNCLIM(256), BINTRUNCLIM(128), _dwParamFormat(dwParamFormat)
	{}

	void AppendBOOL(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_BOOL, ";

		*this << "'" << (int)*(unsigned char*)pValue << "'";

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendSTR(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_STR, ";

		*this << "'" << (LPSTR)pValue << "'";

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendSTRBYREF(const BYTE* pValue, DBLENGTH* pLength)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_STR | DBTYPE_BYREF, ";

		char* strbuf = *(LPSTR*)pValue;

		// if string length is too big, truncate it
		if (PRMFMT_DEFAULT == _dwParamFormat && ((pLength && *pLength > TXTTRUNCLIM) || (!pLength && strlen(strbuf) > TXTTRUNCLIM)))
		{
			string tmpstr(strbuf, TXTTRUNCLIM);
			tmpstr[255] = 0;
			*this << "'" << tmpstr.c_str() << "...'";
		}
		else
		{
			*this << "'" << strbuf << "'";
		}

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendWSTR(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_WSTR, ";

		*this << "'" << CW2AEX<256>((LPWSTR)pValue, CP_UTF8) << "'";

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendBSTR(const BYTE* pValue, DBLENGTH* pLength)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_BSTR, ";

		wchar_t* strbuf = *(LPWSTR*)pValue;

		if (PRMFMT_DEFAULT == _dwParamFormat && ((pLength && *pLength > TXTTRUNCLIM) || (!pLength && wcslen(strbuf) > TXTTRUNCLIM)))
		{
			// if string length is too big, truncate it by first TXTTRUNCLIM chars
			wstring tmpstr(strbuf, TXTTRUNCLIM);
			*this << "'" << CW2A((LPWSTR)tmpstr.c_str()) << "...'";
		}
		else
		{
			*this << "'" << CW2AEX<256>((LPWSTR)strbuf, CP_UTF8) << "'";
		}

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendWSTRBYREF(const BYTE* pValue, DBLENGTH* pLength)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_WSTR | DBTYPE_BYREF, ";

		wchar_t* strbuf = *(LPWSTR*)pValue;

		// if string length is too big, truncate it
		if (PRMFMT_DEFAULT == _dwParamFormat && ((pLength && *pLength > TXTTRUNCLIM) || (!pLength && wcslen(strbuf) > TXTTRUNCLIM)))
		{
			wstring tmpstr(strbuf, TXTTRUNCLIM);
			//tmpstr[TXTTRUNCLIM-1] = 0;
			*this << "'" << CW2A((LPWSTR)tmpstr.c_str()) << "...'";
		}
		else
		{
			*this << "'" << CW2A((LPWSTR)strbuf) << "'";
		}

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendBYTES(const BYTE* pValue, DBLENGTH* pLength)
	{
		ATLASSERT(pLength != NULL);

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_BYTES x";

		// if string length is too big, truncate it
		if (PRMFMT_DEFAULT == _dwParamFormat && *pLength > BINTRUNCLIM)
		{
			//for (int i = 0; i < BINTRUNCLIM; i++)
			//	*this << hexchars[(pValue[i] & 0xF0) >> 4] << hexchars[pValue[i] & 0xF0];
			for (int i = 0; i < BINTRUNCLIM; i++)
				*this << "\\" << hexchars[(pValue[i] & 0xC0) >> 6] << hexchars[(pValue[i] & 0x38) >> 3] << hexchars[pValue[i] & 0x7];

			*this << "... length=" << *pLength;
		}
		else
		{
			for (int i = 0; i < *pLength; i++)
				*this << hexchars[(pValue[i] & 0xF0) >> 4] << hexchars[(pValue[i] & 0xF0) >> 4];
		}

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendR8(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_R8, ";

		*this << *(double*)pValue;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendR4(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_R4, ";

		*this << *(float*)pValue;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendDBTIMESTAMP(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_DBTIMESTAMP, ";

		DBTIMESTAMP* tmpbuf = (DBTIMESTAMP*)pValue;
		char tmpoutbuf[64];

		sprintf(tmpoutbuf, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
			tmpbuf->year, tmpbuf->month, tmpbuf->day,
			tmpbuf->hour, tmpbuf->minute, tmpbuf->second, tmpbuf->fraction / 1000000);
		*this << tmpoutbuf;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendDBTIMESTAMPOFFSET(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_DBTIMESTAMP, ";

		DBTIMESTAMPOFFSET* tmpbuf = (DBTIMESTAMPOFFSET*)pValue;
		char tmpoutbuf[64];

		sprintf(tmpoutbuf, "%04d-%02d-%02d %02d:%02d:%02d.%03d %d:%02d",
			tmpbuf->year, tmpbuf->month, tmpbuf->day,
			tmpbuf->hour, tmpbuf->minute, tmpbuf->second, tmpbuf->fraction / 1000000,
			tmpbuf->timezone_hour, tmpbuf->timezone_minute);
		*this << tmpoutbuf;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendDATE(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_DATE, ";

		COleDateTime tmpDateTime(*(DATE*)pValue);
		char tmpoutbuf[16];

		sprintf(tmpoutbuf, "%04d-%02d-%02d", tmpDateTime.GetYear(), tmpDateTime.GetMonth(), tmpDateTime.GetDay());
		*this << tmpoutbuf;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendDBDATE(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_DBDATE, ";

		char tmpoutbuf[16];
		sprintf(tmpoutbuf, "%04hi-%02hu-%02hu", ((DBDATE*)pValue)->year, ((DBDATE*)pValue)->month, ((DBDATE*)pValue)->day);
		*this << tmpoutbuf;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendDBTIME(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_DBTIME, ";

		char tmpoutbuf[16];
		sprintf(tmpoutbuf, "%02hu:%02hu:%02hu", ((DBTIME *)pValue)->hour, ((DBTIME *)pValue)->minute, ((DBTIME *)pValue)->second);
		*this << tmpoutbuf;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendDBTIME2(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_DBTIME2, ";

		char tmpoutbuf[32];
		sprintf(tmpoutbuf, "%02hu:%02hu:%02hu.%d", ((DBTIME2*)pValue)->hour, ((DBTIME2*)pValue)->minute, ((DBTIME2*)pValue)->second, ((DBTIME2*)pValue)->fraction);
		*this << tmpoutbuf;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendI1(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_I1, ";

		*this << (int)*(unsigned char*)pValue;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendI2(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_I2, ";

		*this << (int)*(short*)pValue;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendI4(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_I4, ";

		*this << *(int*)pValue;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendI8(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_I8, ";

		*this << *(__int64*)pValue;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendVARIANT(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_VARIANT, ";

		VARIANT* varBuf = (VARIANT*)pValue;
		CComVariant strVariant;

		VariantChangeType(&strVariant, varBuf, VARIANT_ALPHABOOL, VT_BSTR);

		if (V_VT(&strVariant) == VT_BSTR)
		{
			*this << "'" << CW2A((LPWSTR)V_BSTR(&strVariant)) << "'";
		}
		else
		{
			*this << "'Cannot Convert to BSTR'";
		}

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendGUID(const BYTE* pValue)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[DBTYPE_GUID, ";

		string tmpBuf("<guid>");
		//CPqBoundType::WinToString(CP_ACP, DBTYPE_GUID, pValue, 16, tmpBuf, AppendChar);
		*this << "'" << tmpBuf << "'";

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}
};

string FormatClientSQL(const char* szSQL, DBPARAMS* pParams, DBCOUNTITEM cBindings, DBBINDING* rgBindings)
{
	if (pParams && cBindings) // extract as many params as we can
	{
		CDbType2StringStream mystream(m_dhPipes.GetAggregateParamFormat());

		BYTE* pData = (BYTE*)pParams->pData;

		string tmpBuf;
		char* pcur = const_cast<char*>(szSQL);

		for (unsigned cParamNo = 0; cParamNo < cBindings; cParamNo++)
		{
			DBBINDING* pBind = &rgBindings[cParamNo];
			if (DBPARAMIO_OUTPUT == pBind->eParamIO)
				continue;

			nextParam<char>(&pcur, tmpBuf);
			mystream << tmpBuf;
	
			DBSTATUS* pStatus = (DBSTATUS*)(pData + pBind->obStatus);
			DBLENGTH* pLength = ((pBind->dwPart & DBPART_LENGTH) ? (DBLENGTH*)(pData + pBind->obLength) : NULL);
			BYTE* pValue = pData + pBind->obValue;

			if (DBSTATUS_S_ISNULL == *pStatus)
			{
				mystream << "[NULL]";
			}
			else
			{
				switch (pBind->wType)
				{
				case DBTYPE_BOOL:	mystream.AppendBOOL(pValue); break;
				case DBTYPE_STR:	mystream.AppendSTR(pValue); break;
				case DBTYPE_STR | DBTYPE_BYREF:	mystream.AppendSTRBYREF(pValue, pLength); break;
				case DBTYPE_WSTR:	mystream.AppendWSTR(pValue); break;
				case DBTYPE_BSTR:	mystream.AppendBSTR(pValue, pLength); break;
				case DBTYPE_WSTR | DBTYPE_BYREF:mystream.AppendWSTRBYREF(pValue, pLength); break;
				case DBTYPE_BYTES | DBTYPE_BYREF:
				case DBTYPE_BYTES:	mystream.AppendBYTES(pValue, pLength); break;
				case DBTYPE_R8:		mystream.AppendR8(pValue); break;
				case DBTYPE_R4:		mystream.AppendR4(pValue); break;
				case DBTYPE_DBTIMESTAMP:	mystream.AppendDBTIMESTAMP(pValue); break;
				case DBTYPE_DBTIMESTAMPOFFSET:mystream.AppendDBTIMESTAMPOFFSET(pValue); break;
				case DBTYPE_DATE:	mystream.AppendDATE(pValue); break;
				case DBTYPE_DBDATE:	mystream.AppendDBDATE(pValue); break;
				case DBTYPE_DBTIME:	mystream.AppendDBTIME(pValue); break;
				case DBTYPE_DBTIME2:mystream.AppendDBTIME2(pValue); break;
				case DBTYPE_I1:
				case DBTYPE_UI1:	mystream.AppendI1(pValue); break;
				case DBTYPE_I2:
				case DBTYPE_UI2:	mystream.AppendI2(pValue); break;
				case DBTYPE_I4:
				case DBTYPE_UI4:	mystream.AppendI4(pValue); break;
				case DBTYPE_I8:
				case DBTYPE_UI8:	mystream.AppendI8(pValue); break;
				case DBTYPE_IUNKNOWN:mystream << "[DBTYPE_IUNKNOWN]"; break;
				case DBTYPE_VARIANT:mystream.AppendVARIANT(pValue); break;
				case DBTYPE_GUID:	mystream.AppendGUID(pValue); break;
				default:
					{
						mystream << "[DBTYPE is unknown: " << pBind->wType << "]";
					}
					break;
				}
			}
		}

		if (*pcur)
			mystream << pcur;

		mystream << endl;

		return mystream.str();
	}

	string buffer(szSQL);

	// remove newlines
	for (wchar_t* p = (wchar_t*)buffer.c_str(); *p != 0; p++)
	{
		if (L'\n' == *p || L'\r' == *p)
			*p = L' ';
	}
	return buffer;
}


class CPgType2StringStream : public stringstream
{
	const DWORD TXTTRUNCLIM;
	const DWORD XMLTRUNCLIM;
	DWORD _dwParamFormat;
public:
	CPgType2StringStream(DWORD dwParamFormat) : TXTTRUNCLIM(256), XMLTRUNCLIM(1000), _dwParamFormat(dwParamFormat)
	{}

	void AppendBOOL(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_BOOL, ";

		*this << (*(BYTE*)val ? "'t'" : "'f'");

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendNAME(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_NAME, ";

		*this << "'" << val << "'";

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendINT8(const char* val)
	{
		__int64 tmp;
		((u_long*)&tmp)[0] = ntohl(((u_long*)val)[1]);
		((u_long*)&tmp)[1] = ntohl(((u_long*)val)[0]);

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_INT8, ";

		*this << tmp;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendINT2(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_INT2, ";

		*this << ntohs(*(u_short*)val);

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendINT4(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_INT4, ";

		*this << ntohl(*(u_long*)val);

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendTEXT(const char* val, int len)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_TEXT, ";

		if (PRMFMT_DEFAULT == _dwParamFormat && len > TXTTRUNCLIM)
		{
			string tmp;
			tmp.assign(len, TXTTRUNCLIM);
			*this << "'" << tmp << "...'";
		}
		else
			*this << "'" << val << "'";

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendXML(const char* val, int len)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_XML, ";

		if (PRMFMT_DEFAULT == _dwParamFormat && len > XMLTRUNCLIM)
		{
			string tmp;
			tmp.assign(val, XMLTRUNCLIM);
			*this << "'" << tmp << "...'";
		}
		else
			*this << "'" << val << "'";

		*this << "]";
	}

	void AppendFLOAT(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_FLOAT, ";

		*this << (float)ntohl(*(u_long*)val);

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}
	
	void AppendUI1(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_UI1, ";

		*this << *(BYTE*)val;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendFLOAT8(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_FLOAT8, ";

		double tmp;
		((u_long*)&tmp)[0] = ntohl(((u_long*)val)[1]);
		((u_long*)&tmp)[1] = ntohl(((u_long*)val)[0]);
		*this << tmp;

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendOID(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_OID, ";
		
		*this << ntohl(*(u_long*)val);

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendXID(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_XID, ";
		
		*this << ntohl(*(u_long*)val);

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendBPCHAR(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_BPCHAR, ";

		*this << "'" << val << "'";

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendVARCHAR(const char* val)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_VARCHAR, ";

		*this << "'" << val << "'";

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendDBDATE(const char* val_)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_DBDATE, ";

		u_long val = ntohl(*(u_long*)val_);

		DBDATE winDate;
		j2date(val + POSTGRES_EPOCH_JDATE, &winDate.year, &winDate.month, &winDate.day);
		*this << winDate.year << setw(2) << setfill('0') << winDate.month << winDate.day << setw(1);

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}

	void AppendTIME(const char* val, bool bIntegerDatetimes)
	{
		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "[PGTYPE_TIME, ";

		DBTIME ts;

		PGTimeStamp time;

		u_long* pData = (u_long*)val;

		if (bIntegerDatetimes)
		{
			((u_long*)&time)[0] = ntohl(pData[1]);
			((u_long*)&time)[1] = ntohl(pData[0]);
		}
		else
		{
			double tmpTime;
			((u_long*)&tmpTime)[0] = ntohl(pData[1]);
			((u_long*)&tmpTime)[1] = ntohl(pData[0]);

			time = tmpTime * 1000000;
		}

		ULONG dummy;
		dt2time(time, &(ts.hour), &(ts.minute), &(ts.second), &dummy);
		*this << setw(2) << setfill('0') << ts.hour << ':' << ts.minute << ':' << ts.second << setw(1);

		if (_dwParamFormat != PRMFMT_PGADMIN)
			*this << "]";
	}
};

string FormatPGSQL(const char* szSQL, int nParams, const unsigned int* paramTypes, char** paramValues, const int* paramLengths, const int* /*paramFormats*/, bool bIntegerDatetimes)
{
	if (nParams > 0)
	{
		CPgType2StringStream mystream(m_dhPipes.GetAggregateParamFormat());
		string tmpBuf;
		char* pcur = const_cast<char*>(szSQL);

		for (int cParamNo = 0; cParamNo < nParams; cParamNo++)
		{
			nextParam<char>(&pcur, tmpBuf);
			mystream << tmpBuf;
	
			switch (paramTypes[cParamNo])
			{
			default:	mystream << "[PGTYPE_ID=" << paramTypes[cParamNo] << "]"; break;
			case PGTYPE_BOOL:	mystream.AppendBOOL(paramValues[cParamNo]); break;
			//case PGTYPE_BYTEA:	mystream << "[PGTYPE_BYTEA]"; break;
			case PGTYPE_NAME:	mystream.AppendNAME(paramValues[cParamNo]); break;
			case PGTYPE_INT8:	mystream.AppendINT8(paramValues[cParamNo]); break;
			case PGTYPE_INT2:	mystream.AppendINT2(paramValues[cParamNo]); break;
			case PGTYPE_INT4:	mystream.AppendINT4(paramValues[cParamNo]); break;
			case PGTYPE_TEXT:	mystream.AppendTEXT(paramValues[cParamNo], paramLengths[cParamNo]); break;
			case PGTYPE_XML:	mystream.AppendXML(paramValues[cParamNo], paramLengths[cParamNo]); break;
			case PGTYPE_FLOAT4:	mystream.AppendFLOAT(paramValues[cParamNo]); break;
			case PGTYPE_UI1:	mystream.AppendUI1(paramValues[cParamNo]); break;
			case PGTYPE_FLOAT8:	mystream.AppendFLOAT8(paramValues[cParamNo]); break;
			case PGTYPE_OID:	mystream.AppendOID(paramValues[cParamNo]); break;
			case PGTYPE_XID:	mystream.AppendXID(paramValues[cParamNo]); break;
			//case PGTYPE_MONEY:	mystream << "[PGTYPE_MONEY]";		//todo
			//	break;
			case PGTYPE_BPCHAR:	mystream.AppendBPCHAR(paramValues[cParamNo]); break;
			case PGTYPE_VARCHAR:mystream.AppendVARCHAR(paramValues[cParamNo]); break;
			case PGTYPE_DBDATE:	mystream.AppendDBDATE(paramValues[cParamNo]); break;
			case PGTYPE_TIME:	mystream.AppendTIME(paramValues[cParamNo], bIntegerDatetimes); break;
			//case PGTYPE_TIMESTAMP:
			//case PGTYPE_TIMESTAMPTZ:
			//	mystream << "[PGTYPE_TIMESTAMP]";		//todo
			//	break;
			//case PGTYPE_NUMERIC:
			//	mystream << "[PGTYPE_NUMERIC]";		//todo
			//	break;
			//case PGTYPE_UUID:
			//	mystream << "[PGTYPE_UUID]";		//todo
			//	break;
			}
		}
		if ( *pcur)
			mystream << pcur;

		return mystream.str();
	}

	string buffer(szSQL);

	// remove newlines
	for (char* p = (char*)buffer.c_str(); *p != 0; p++)
	{
		if ('\n' == *p || '\r' == *p)
			*p = L' ';
	}
	return buffer;
}
