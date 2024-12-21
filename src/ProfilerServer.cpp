/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "resource.h"
#include "LoggerItem.h"
#include "TraceReader.h"
#include "ProfilerDef.h"
#include "ProfilerServer.h"

//#ifndef MSGFLT_ADD		// if Vista SDK is not installed or WINVER < 0x0600
#define MSGFLT_ADD			1
#define MSGFLT_REMOVE		2
typedef BOOL (WINAPI *CHANGEWINDOWMESSAGEFILTER)(UINT message, DWORD dwFlag);
//#endif

extern LONGLONG g_qpcFreq;	// MSDN: "The frequency cannot change while the system is running."

DWORD ProfilerTimerInit()
{
	//// Add WM_PGNPROF_STATUS message to the User Interface Privilege Isolation (UIPI) message filter.
	//CHANGEWINDOWMESSAGEFILTER ChangeWindowMessageFilter = (CHANGEWINDOWMESSAGEFILTER)GetProcAddress(GetModuleHandle(L"user32"), "ChangeWindowMessageFilter");
	//if (ChangeWindowMessageFilter != 0)
	//	ChangeWindowMessageFilter(WM_PGNPROF_STATUS, MSGFLT_ADD);

	LARGE_INTEGER liFreq;
	QueryPerformanceFrequency(&liFreq);
	g_qpcFreq = liFreq.QuadPart;

	return 0;
}

/// Start or Stop capture
extern "C" void CaptureToggle(CLoggerItemBase* pBlock, UnlimitedWait* pUnlimitedWait)
{
	if (pBlock->GetType() != LT_PROCESS && pBlock->GetType() != LT_PROCESS64)
		return;

	CProcessLoggerItem& processBlock = *(CProcessLoggerItem*)pBlock;

	if (processBlock.GetState() == LS_PAUSED)
	{
		// open pipe for overlapped read
		HANDLE hPipe = INVALID_HANDLE_VALUE;
		const WCHAR* pipeName = processBlock.GetPipeName();
		int attempts = 0;

		ATLTRACE2(atlTraceDBProvider, 0, L"CaptureToggle(): pipe %s\n", pipeName);

		for (;;)
		{
			hPipe = CreateFile(pipeName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
			if (hPipe == INVALID_HANDLE_VALUE)
			{
				DWORD err = GetLastError();
				if (ERROR_PIPE_BUSY == err && attempts++ < 50)	// check attempts count to break infinite loop
				{
					Sleep(100);
					continue;
				}
				ATLTRACE2(atlTraceDBProvider, 0, L"** Process has probably been terminated (%s)\n", pipeName);
				processBlock.SetState(LS_TERMINATED);
				return;
			}
			else
			{
				//DWORD mode = PIPE_READMODE_BYTE;
				//SetNamedPipeHandleState(hPipe, &mode, 0, 0);
				break;
			}
		}

		BOOL rez = ReadFile(hPipe, processBlock.m_tszBuffer, PIPEBUFFERSIZE, NULL, &processBlock.m_op);
		ATLASSERT(rez != 0 || GetLastError() == ERROR_IO_PENDING);

		processBlock.SetPipe(hPipe);
		processBlock.SetState(LS_RUN);

		ATLTRACE2(atlTraceDBProvider, 0, L"CaptureToggle(): adding wait object\n");

		AddUnlimitedWaitObject(pUnlimitedWait, processBlock.m_op.hEvent, CTraceReader::HandleProcessLoggerOverlappedResult, &processBlock);
	}
	else if (processBlock.GetState() == LS_RUN)
	{
		HANDLE hPipe = processBlock.GetPipe();

		processBlock.SetState(LS_PAUSED);
		processBlock.SetPipe(INVALID_HANDLE_VALUE);

		ATLTRACE2(atlTraceDBProvider, 0, L"CaptureToggle(): removing wait object\n");

		RemoveUnlimitedWaitObject(pUnlimitedWait, processBlock.m_op.hEvent, FALSE);

		CloseHandle(hPipe);
	}

	ATLTRACE2(atlTraceDBProvider, 0, L"Leave CaptureToggle()\n");
}
