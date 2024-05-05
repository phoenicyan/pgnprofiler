/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "StdAfx.h"
#include "LoggerItem.h"
#include "TraceReader.h"
#include "ProfilerDef.h"
#include "Remote/RemoteApi.h"
#include <time.h>

CTraceReader::CTraceReader(void) : m_threadId(-1), m_mainWindow(0), m_hWnd(0)
{
	InitializeCriticalSection(&m_loggersCS);
}

CTraceReader::~CTraceReader(void)
{
	StopAll();

	DeleteCriticalSection(&m_loggersCS);
}

void CTraceReader::SetMainWindow(HWND mainWindow)
{
	m_mainWindow = mainWindow;

	HANDLE hThread = CreateThread(0, 0, CTraceReader::ThreadProc, this, CREATE_SUSPENDED, &m_threadId);
	SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);
	ResumeThread(hThread);
}

DWORD CTraceReader::Start(CHostLoggerItem* pLogger)
{
	ATLASSERT(pLogger != 0);

	EnterCriticalSection(&m_loggersCS);

	m_loggers.push_back(pLogger);

	LeaveCriticalSection(&m_loggersCS);

	return 0;
}

void CTraceReader::StopAll()
{
	PostThreadMessage(m_threadId, WM_QUIT, 0, 0);
}

HWND CTraceReader::CreateVirtualWindow()
{
	WNDCLASSEX wndcls; 
	HINSTANCE hInst = _AtlBaseModule.GetModuleInstance();

	if (!::GetClassInfoEx(hInst, PGNPROF_VREADER_CLASS, &wndcls))
    {
		wndcls.cbSize = sizeof(wndcls);					// size of structure 
		wndcls.style = 0;								// redraw if size changes 
		wndcls.lpfnWndProc = (WNDPROC)WndProc;			// points to window procedure 
		wndcls.cbClsExtra = 0;							// no extra class memory 
		wndcls.cbWndExtra = 0;							// no extra window memory 
		wndcls.hInstance = hInst;						// handle to instance 
		wndcls.hIcon = NULL;							// predefined app. icon 
		wndcls.hCursor = NULL;							// predefined arrow 
		wndcls.hbrBackground = NULL;					// white background brush 
		wndcls.lpszMenuName = NULL;						// name of menu resource 
		wndcls.lpszClassName = PGNPROF_VREADER_CLASS;		// name of window class 
		wndcls.hIconSm = NULL; 
	 
		if (!::RegisterClassEx(&wndcls))
			return NULL;
	}

	m_hWnd = ::CreateWindow(PGNPROF_VREADER_CLASS, L"", WS_POPUPWINDOW, 0, 0, 0, 0, NULL, NULL, hInst, NULL);
	if (m_hWnd != NULL)
	{
		MSG msg;
		::PeekMessage(&msg, m_hWnd, 0, 0, PM_NOREMOVE);
	}
	return m_hWnd;
}

void CTraceReader::DestroyVirtualWindow(HWND hWnd)
{
	::DestroyWindow(hWnd);
	::UnregisterClass(PGNPROF_VREADER_CLASS, _AtlBaseModule.GetModuleInstance());
}

#define TRACE_READER_TIMER		1

DWORD WINAPI CTraceReader::ThreadProc(LPVOID pParam)
{
	CTraceReader* pThis = (CTraceReader*)pParam;

	HWND hWnd = pThis->CreateVirtualWindow();

	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pParam);

	SetTimer(hWnd, TRACE_READER_TIMER, 133, 0);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	pThis->DestroyVirtualWindow(hWnd);

	return (DWORD)msg.wParam;
}

void CTraceReader::HandleProcessLoggerOverlappedResult(CProcessLoggerItem& processLogger)
{
	DWORD numBytesLeft = 0;
	HANDLE hPipe = processLogger.GetPipe();
	BOOL rez = GetOverlappedResult(hPipe, &processLogger.m_op, &numBytesLeft, FALSE);
	if (!rez || GetLastError() == ERROR_BROKEN_PIPE)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"HandleProcessLoggerOverlappedResult: peer application closed the pipe\n");
		processLogger.SetState(LS_TERMINATED);
		processLogger.SetPipe(INVALID_HANDLE_VALUE);

		DisconnectNamedPipe(hPipe);
		CloseHandle(hPipe);
		return;
	}

	ATLTRACE2(atlTraceDBProvider, 0, L"HandleProcessLoggerOverlappedResult: %d bytes received from '%s'\n", numBytesLeft, processLogger.GetName());

	BYTE* ptr = &processLogger.m_tszBuffer[0];
	while ((int)numBytesLeft > 0)
	{
		const DWORD dwWriteOffset = (processLogger.m_logWritePos - processLogger.m_logStart);
		const DWORD dwSpaceLeft = processLogger.m_dwMMFsize - dwWriteOffset;

		// check free space
		if (numBytesLeft >= dwSpaceLeft)
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"HandleProcessLoggerOverlappedResult: increasing file mapping size\n");
			if (processLogger.GrowLogFile(MMFGROWSIZE))
			{
				break;
			}

			continue;
		}

		// State machine implementation for processing IO buffer.
		if (0 == processLogger.m_pendingBytes)
		{
			// State1. There was no incomplete message from previous processing of IO buffer
			if (numBytesLeft >= sizeof(DWORD))
			{
				DWORD payloadLength = *(DWORD*)ptr;		// can determine payload length
				if (payloadLength <= numBytesLeft)
				{	// Entire message fitted into IO buffer. 
					// Lets add it to the list of available messages.
					memcpy(processLogger.m_logWritePos, ptr, payloadLength);

					if (processLogger.AddMessageFromWritePos())
					{
						processLogger.m_logWritePos += (payloadLength + 3) & ~3;	// align to 4 bytes
					}

					ptr += payloadLength;
					numBytesLeft -= payloadLength;
				}
				else
				{	// Only part of the message fitted into the IO buffer.
					// Lets set data for the incomplete message.
					processLogger.m_pendingBytes = payloadLength - numBytesLeft;		// Goto State2.
					processLogger.m_pendingWritePos = processLogger.m_logWritePos;
					processLogger.m_pendingLengthUnknown = false;

					ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnTimer: Goto State2. PendingBytes=%d\n", processLogger.m_pendingBytes);

					memcpy(processLogger.m_logWritePos, ptr, numBytesLeft);
					processLogger.m_logWritePos += numBytesLeft;

					break;
				}
			}
			else
			{
				// not enought data to determine payload length
				processLogger.m_pendingBytes = sizeof(DWORD) - numBytesLeft;		// Goto State3.
				processLogger.m_pendingWritePos = processLogger.m_logWritePos;
				processLogger.m_pendingLengthUnknown = true;

				ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnTimer: Goto State3. PendingBytes=%d\n", processLogger.m_pendingBytes);

				memcpy(processLogger.m_logWritePos, ptr, numBytesLeft);
				processLogger.m_logWritePos += numBytesLeft;

				break;
			}
		}
		// State2. There was an incomplete message left from previous processing of IO buffer.
		else if (!processLogger.m_pendingLengthUnknown)
		{
			if (processLogger.m_pendingBytes <= numBytesLeft)
			{	// Entire message fitted into IO buffer. 
				// Lets add it to the list of available messages.
				memcpy(processLogger.m_logWritePos, ptr, processLogger.m_pendingBytes);

				ptr += processLogger.m_pendingBytes;
				numBytesLeft -= processLogger.m_pendingBytes;

				processLogger.m_pendingBytes = 0;		// Goto State1
				processLogger.m_logWritePos = processLogger.m_pendingWritePos;
				DWORD payloadLength = *(DWORD*)processLogger.m_logWritePos;		// now can determine payload length

				ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnTimer: Goto State1. Previous PayloadLength=%d\n", payloadLength);

				if (processLogger.AddMessageFromWritePos())
				{
					processLogger.m_logWritePos += (payloadLength + 3) & ~3;	// align to 4 bytes
				}
			}
			else
			{	// Only part of the message fitted into the IO buffer.
				// Lets set data for the incomplete message.
				processLogger.m_pendingBytes -= numBytesLeft;		// Remain in State2.

				ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnTimer: Remain in State2. PendingBytes=%d\n", processLogger.m_pendingBytes);

				memcpy(processLogger.m_logWritePos, ptr, numBytesLeft);
				processLogger.m_logWritePos += numBytesLeft;

				break;
			}
		}
		// State3. There was an incomplete message left from previous processing of IO buffer and payload length is unknown.
		else
		{
			if (processLogger.m_pendingBytes > numBytesLeft)
			{
				ATLTRACE2(atlTraceDBProvider, 0, L"** CTraceReader::OnTimer: still not enough data in State3\n");
				break;	// probably error - still not enough data to determine payload length (todo: need better handling)
			}
			memcpy(processLogger.m_logWritePos, ptr, processLogger.m_pendingBytes);
			processLogger.m_logWritePos += processLogger.m_pendingBytes;

			ptr += processLogger.m_pendingBytes;
			numBytesLeft -= processLogger.m_pendingBytes;

			processLogger.m_pendingBytes = *(DWORD*)processLogger.m_pendingWritePos;		// Goto State2
			processLogger.m_pendingBytes -= sizeof(DWORD);
			processLogger.m_pendingLengthUnknown = false;

			ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnTimer: Goto State2. PendingBytes=%d\n", processLogger.m_pendingBytes);
		}
	}

	::ResetEvent(processLogger.m_op.hEvent);

	rez = ReadFile(processLogger.GetPipe(), processLogger.m_tszBuffer, PIPEBUFFERSIZE, NULL, &processLogger.m_op);
	if (rez != 0 || GetLastError() == ERROR_IO_PENDING)
	{
		::PostMessage(m_mainWindow, MYMSG_DATA_READ, (WPARAM)processLogger.GetPipe(), (LPARAM)0);
	}
	else
	{
		processLogger.ClosePipe();
		::PostMessage(m_mainWindow, WM_PGNPROF_STATUS, 0, 0);
	}
}

void CTraceReader::HandleHostLoggerOverlappedResult(CHostLoggerItem& hostLogger)
{
	DWORD numBytesLeft = 0;
	OVERLAPPED& op = hostLogger.GetOverlapped();

	BOOL rez = GetOverlappedResult(hostLogger.GetRemotePipe(), &op, &numBytesLeft, FALSE);
	if (!rez || GetLastError() == ERROR_BROKEN_PIPE)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"HandleHostLoggerOverlappedResult: peer application closed the pipe\n");

		// TODO: 
		// This probably means that remote computer disconnected, or shutdown unexpectedly.
		// Mark this host logger and all child loggers as terminated, and perform cleanup (close pipes, etc.)
		return;
	}

	ATLTRACE2(atlTraceDBProvider, 0, L"HandleHostLoggerOverlappedResult: %d bytes received from '%s'\n", numBytesLeft, hostLogger.GetName());

	int maxSize;
	BYTE* buffer = hostLogger.GetIObuffer(&maxSize);

	std::unique_ptr<RemMsg> msgReturned(new RemMsg());
	msgReturned->SetFromReceivedData(buffer, numBytesLeft);

	::ResetEvent(op.hEvent);

	rez = ReadFile(hostLogger.GetRemotePipe(), buffer, maxSize, NULL, &op);
	if (0 == rez && GetLastError() != ERROR_IO_PENDING)
	{
		hostLogger.CloseRemotePipe();
	}

	::PostMessage(m_mainWindow, WM_PGNPROF_STATUS, (WPARAM)hostLogger.GetRemotePipe(), (LPARAM)msgReturned.release());
}

void CTraceReader::OnTimer(HWND hWnd, int timerID)
{
	vector<HANDLE> hostEvents;		// these are op-events used to refresh lists of processes on remote hosts
	vector<CHostLoggerItem*> waitableHostLoggers;

	EnterCriticalSection(&m_loggersCS);

	for (auto it = m_loggers.begin(); it != m_loggers.end(); it++)
	{
		CHostLoggerItem* pHostLogger = *it;

		//pHostLogger->Lock();

		OVERLAPPED& op = pHostLogger->GetOverlapped();
		if (/*pRemote &&*/ op.hEvent != 0)
		{
			hostEvents.push_back(op.hEvent);
			waitableHostLoggers.push_back(pHostLogger);
		}

		// fill in array of overlapped handles
		const list<CLoggerItemBase*>& processLoggers = pHostLogger->GetChildren();

		HANDLE handles[MAXIMUM_WAIT_OBJECTS];
		CProcessLoggerItem* waitableProcessLoggers[MAXIMUM_WAIT_OBJECTS];
		int numHandles = 0;
		for (list<CLoggerItemBase*>::const_iterator it = processLoggers.begin(); it != processLoggers.end(); it++)
		{
			try
			{
				CProcessLoggerItem& processLogger = *(CProcessLoggerItem*)(*it);
				if (processLogger.GetState() == LS_RUN && processLogger.GetPipe() != INVALID_HANDLE_VALUE)
				{
					handles[numHandles] = processLogger.m_op.hEvent;
					waitableProcessLoggers[numHandles] = &processLogger;
					numHandles++;
				}
			}
			catch (...)
			{
				ATLTRACE2(atlTraceDBProvider, 0, L"** Exception in CTraceReader::OnTimer numHandles=%d\n", numHandles);
			}
		}

		if (numHandles > 0)
		{
			// check if data is available
			DWORD waitRez = WaitForMultipleObjects(numHandles, handles, FALSE, 0);
			switch (waitRez)
			{
			case WAIT_FAILED:
			case WAIT_TIMEOUT:
				//ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnTimer(%d, %d, x%x, wr = %d) ?????\n", (int)time(0), numHandles, handles[0], waitRez);
				break;

			default:
				//ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnTimer(%d, %d, x%x, wr = %d)!!\n", (int)time(0), numHandles, handles[0], waitRez);
				HandleProcessLoggerOverlappedResult(*waitableProcessLoggers[waitRez]);
				break;
			}
		}

		//pHostLogger->Unlock();
	}

	if (hostEvents.size() > 0)
	{
		// check if data is available
		DWORD waitRez = WaitForMultipleObjects(hostEvents.size(), &hostEvents[0], FALSE, 0);
		switch (waitRez)
		{
		case WAIT_FAILED:
		case WAIT_TIMEOUT:
			//ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnTimer(%d, %d, x%x, wr = %d) ?????\n", (int)time(0), numHandles, handles[0], waitRez);
			break;

		default:
			//ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnTimer(%d, %d, x%x, wr = %d)!!\n", (int)time(0), numHandles, handles[0], waitRez);
			HandleHostLoggerOverlappedResult(*waitableHostLoggers[waitRez]);
			break;
		}
	}

	LeaveCriticalSection(&m_loggersCS);
}

LRESULT CALLBACK CTraceReader::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (WM_TIMER == msg)
	{
		CTraceReader* pThis = (CTraceReader*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		pThis->OnTimer(hWnd, (int)wParam);
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
