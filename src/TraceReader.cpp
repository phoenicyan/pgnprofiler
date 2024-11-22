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

#define TRACE_READER_TIMER		1

CTraceReader::CTraceReader(void) : m_threadId(ULONG_MAX)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader ctor: create UnlimitedWait object\n");

	m_pUnlimitedWait = CreateUnlimitedWait(NULL, 8, OnTimeoutCallback, OnApcCallback);
	m_hQuit = CreateEvent(NULL, TRUE, FALSE, NULL);
	InitializeCriticalSection(&m_loggersCS);
}

CTraceReader::~CTraceReader(void)
{
	StopAll();
	DeleteCriticalSection(&m_loggersCS);
	DeleteUnlimitedWait(m_pUnlimitedWait);
}

VOID WINAPI CTraceReader::OnTimeoutCallback(PVOID lpContext)
{
	//ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnTimeoutCallback(%p)\n", lpContext);
}

VOID WINAPI CTraceReader::OnApcCallback(PVOID lpContext)
{
	//ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::OnApcCallback(%p)\n", lpContext);
}

DWORD CTraceReader::Start(CHostLoggerItem* pLogger)
{
	ATLASSERT(pLogger != 0);
	ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::Start: %s\n", pLogger->GetName());

	EnterCriticalSection(&m_loggersCS);

	m_loggers.push_back(pLogger);

	if (ULONG_MAX == m_threadId)
	{
		HANDLE hThread = CreateThread(0, 0, CTraceReader::ThreadProc, this, CREATE_SUSPENDED, &m_threadId);
		SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);
		ResumeThread(hThread);
		Sleep(0);
	}

	LeaveCriticalSection(&m_loggersCS);

	return 0;
}

void CTraceReader::StopAll()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CTraceReader::StopAll\n");
	SetEvent(m_hQuit);
}

#define DEFAULT_WAIT    (25)

DWORD WINAPI CTraceReader::ThreadProc(LPVOID pParam)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"Enter CTraceReader::ThreadProc\n");

	CTraceReader* pThis = (CTraceReader*)pParam;

	while (WaitForSingleObject(pThis->m_hQuit, 0) == WAIT_TIMEOUT)
	{
		const auto WAIT_N = 1;

		ULONG nresults = 0;
		void* results[WAIT_N] = { 0 };
		char tmp_buffer[32 * WAIT_N] = { 0 };

		if (WaitUnlimitedWaitEx(pThis->m_pUnlimitedWait, &results[0], &tmp_buffer[0], WAIT_N, &nresults, DEFAULT_WAIT, TRUE))
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"%u objects signalled.\n", nresults);
			continue;
		}

		switch(GetLastError())
		{
		    case WAIT_TIMEOUT: break;
		    case WAIT_IO_COMPLETION: break;
		    default: ATLTRACE2(atlTraceDBProvider, 0, L"** Wait error %d\n", GetLastError());
		}

		Sleep(DEFAULT_WAIT);
	}

	ATLTRACE2(atlTraceDBProvider, 0, L"Leave CTraceReader::ThreadProc\n");

	return 0;
}

BOOL WINAPI CTraceReader::HandleProcessLoggerOverlappedResult(/*CProcessLoggerItem&*/PVOID lpObjectContext, HANDLE hObject)
{
	CProcessLoggerItem& processLogger = *(CProcessLoggerItem*)lpObjectContext;
	DWORD numBytesLeft = 0;
	HANDLE hPipe = processLogger.GetPipe();
	BOOL rez = GetOverlappedResult(hPipe, &processLogger.m_op, &numBytesLeft, FALSE);
	if (!rez || GetLastError() == ERROR_BROKEN_PIPE)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"HandleProcessLoggerOverlappedResult: peer application closed the pipe\n");
		processLogger.SetState(LS_TERMINATED);
		processLogger.SetPipe(INVALID_HANDLE_VALUE);

		CloseHandle(hPipe);
		return FALSE;
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
			DWORD growSize = ((numBytesLeft - dwSpaceLeft) / MMFGROWSIZE + 1) * MMFGROWSIZE;
			ATLTRACE2(atlTraceDBProvider, 0, L"Increasing file mapping size %d by %d\n", processLogger.m_dwMMFsize, growSize);
			if (processLogger.GrowLogFile(growSize))
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
		::PostMessage(processLogger.GetMainWindow(), MYMSG_DATA_READ, (WPARAM)processLogger.GetPipe(), (LPARAM)0);
	}
	else
	{
		processLogger.ClosePipe();
		::PostMessage(processLogger.GetMainWindow(), WM_PGNPROF_STATUS, 0, 0);
	}

	return TRUE;
}

BOOL WINAPI CTraceReader::HandleHostLoggerOverlappedResult(/*CHostLoggerItem&*/PVOID lpObjectContext, HANDLE hObject)
{
	CHostLoggerItem& hostLogger = *(CHostLoggerItem*)lpObjectContext;
	DWORD numBytesLeft = 0;
	OVERLAPPED& op = hostLogger.GetOverlapped();

	BOOL rez = GetOverlappedResult(hostLogger.GetRemotePipe(), &op, &numBytesLeft, FALSE);
	if (!rez || GetLastError() == ERROR_BROKEN_PIPE)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"HandleHostLoggerOverlappedResult: peer application closed the pipe\n");

		// TODO: 
		// This probably means that remote computer disconnected, or shutdown unexpectedly.
		// Mark this host logger and all child loggers as terminated, and perform cleanup (close pipes, etc.)
		return FALSE;
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

	::PostMessage(hostLogger.GetMainWindow(), WM_PGNPROF_STATUS, (WPARAM)hostLogger.GetRemotePipe(), (LPARAM)msgReturned.release());

	return TRUE;
}
