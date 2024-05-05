/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "RemoteApi.h"
#include "PipeItem.h"

extern ATL::CString AvailableProcesses(LPCTSTR filter);

#if 0	// delme
BOOL CPipeItem::CheckPipeConnected()
{
	BOOL bConnected = ConnectNamedPipe(m_hPipe, 0);
	if (!bConnected)
	{
		DWORD err = GetLastError();
		if (err == ERROR_PIPE_CONNECTED)
		{
			bConnected = TRUE;
		}
		else if (err != ERROR_PIPE_LISTENING)
		{
			DisconnectNamedPipe(m_hPipe);
			bConnected = ConnectNamedPipe(m_hPipe, 0);
#if _DEBUG
			err = GetLastError();
#endif
		}
	}

	PITYPE newstate = bConnected ? PIT_CONNECTED : PIT_DISCONNECTED;
	if (newstate != m_state)
	{
		m_state = newstate;

		if (bConnected)
		{
			BOOL rez = ReadFile(m_hPipe, m_tszCommandBuffer, BUFF_SIZE_HINT, NULL, &m_op);
#if _DEBUG
			DWORD err = GetLastError();
#endif
		}
	}

	return bConnected;
}

void CPipeItem::ProcessOverlappedResult()
{
	DWORD cbRead = 0;
	BOOL rez = GetOverlappedResult(this->m_hPipe, &this->m_op, &cbRead, FALSE);
	if (rez != 0)
	{
		DWORD err = GetLastError();
		if (err == ERROR_BROKEN_PIPE)
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"OnTimer: peer application closed the pipe\n");
			return;
		}
	}

	ATLTRACE2(atlTraceDBProvider, 0, L"OnTimer: %d bytes received\n", cbRead);

	RemMsg request;
	request.SetFromReceivedData(&this->m_tszCommandBuffer[0], cbRead);

	RemMsg response;

	//CRemoteApi::HandleMsg(request, response);
	switch (request.m_msgID)
	{
	case MI_LISTPIPES_REQ:
		{
			ATL::CString filter;
			request >> filter;

			ATLTRACE2(atlTraceDBProvider, 0, L"CRemoteApi::HandleMsg: obtaining list of pipes using filter '%s'.\n", LPCTSTR(filter));

			m_availProcesses = AvailableProcesses(filter);

			response.m_msgID = MI_LISTPIPES_RESP;
			response << m_availProcesses;  // semicolon-separated list of processes
			break;
		}
	default:
		ATLTRACE2(atlTraceDBProvider, 0, L"** CRemoteApi::HandleMsg: Unrecognized command id=%d.\n", request.m_msgID);

		response.m_msgID = MI_FAILURE_RESP;
		response << (DWORD)request.m_msgID;
		break;
	}

	DWORD totalLen = 0;
	const BYTE* pDataToSend = response.GetDataToSend(totalLen);
	DWORD cbWritten = 0;
	// Send a message to the pipe server. 
	BOOL fSuccess = WriteFile(
		this->m_hPipe,     // pipe handle 
		pDataToSend, // message 
		totalLen,  // message length 
		&cbWritten,// bytes written 
		NULL);     // not overlapped 
	if (!fSuccess || (cbWritten != totalLen))
	{
		DWORD gle = GetLastError();
		ATLTRACE2(atlTraceDBProvider, 0, L"** Error sending data back (%d)\n", gle);
	}

	// schedule new read
	::ResetEvent(this->m_op.hEvent);
	rez = ReadFile(this->m_hPipe, this->m_tszCommandBuffer, BUFF_SIZE_HINT, NULL, &this->m_op);
	if (rez != 0 || GetLastError() == ERROR_IO_PENDING)
	{
		///::PostMessage(hWnd, MYMSG_DATA_READ, (WPARAM)this->m_hPipe, (LPARAM)0);
	}
	else
	{
		///this->ClosePipe();
	}
}
#endif

DWORD WINAPI CPipeItem::PipeMonitorThread(LPVOID pParam)
{
	CPipeItem* pipe = (CPipeItem*)pParam;
	HANDLE hSavedEvent = pipe->m_op.hEvent;

	for (;;)
	{
		// prepare OP object for operation
		memset(&pipe->m_op, 0, sizeof(pipe->m_op));
		pipe->m_op.hEvent = hSavedEvent;

		if (PIPESTATE_INITIAL == pipe->GetPipeState())
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
				pipe->m_state = PIPESTATE_CONNECTED;
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
		DWORD waitRez = WaitForMultipleObjects(_countof(handles), handles, FALSE, 5000);
		if (waitRez == WAIT_TIMEOUT)
		{
			// check if clients can connect to the pipe
			CancelIo(pipe->m_hPipe);
			continue;
		}

		if (waitRez != WAIT_OBJECT_0)
		{
			// stop event signalled, or other condition result in thread termination
			ATLTRACE(atlTraceDBProvider, 0, _T("** CPipeItem::PipeMonitorThread: termination\n"));
			break;
		}

		// handle incoming command
		DWORD numBytesLeft = 0;
		BOOL rez = GetOverlappedResult(pipe->m_hPipe, &pipe->m_op, &numBytesLeft, FALSE);
		if (!rez || GetLastError() == ERROR_BROKEN_PIPE)
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"CPipeItem::PipeMonitorThread: peer application closed the pipe\n");

			DisconnectNamedPipe(pipe->m_hPipe);

			if (PIPESTATE_CONNECTED == pipe->m_state)
			{
				pipe->m_state = PIPESTATE_INITIAL;
				pipe->m_OnClientDisconnected(pipe);
			}
#if _DEBUG
			DWORD err = GetLastError();
#endif
			continue;
		}

		if (PIPESTATE_INITIAL == pipe->m_state)
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"CPipeItem::PipeMonitorThread: connection was made successfully!\n");
			pipe->m_state = PIPESTATE_CONNECTED;
			pipe->m_OnClientConnected(pipe);
		}
		else
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"CPipeItem::PipeMonitorThread: %d bytes received\n", numBytesLeft);

			RemMsg request;
			request.SetFromReceivedData(pipe->m_inbuffer, numBytesLeft);

			RemMsg response;

			switch (request.m_msgID)
			{
			case MI_LISTPIPES_REQ:
				{
					ATL::CString filter;
					request >> filter;

					ATLTRACE2(atlTraceDBProvider, 0, L"CPipeItem::PipeMonitorThread: obtaining list of pipes using filter '%s'.\n", LPCTSTR(filter));

					pipe->m_availProcesses = AvailableProcesses(filter);

					response.m_msgID = MI_LISTPIPES_RESP;
					response << pipe->m_availProcesses;  // semicolon-separated list of processes
					break;
				}

			default:
				ATLTRACE2(atlTraceDBProvider, 0, L"** CPipeItem::PipeMonitorThread: unknown command %d.\n", request.m_msgID);

				response.m_msgID = MI_FAILURE_RESP;
				response << (DWORD)request.m_msgID;
				break;
			}
		
			DWORD totalLen = 0;
			const BYTE* pDataToSend = response.GetDataToSend(totalLen);
			DWORD cbWritten = 0;
			// Send a message to the pipe server. 
			BOOL fSuccess = WriteFile(
				pipe->m_hPipe,     // pipe handle 
				pDataToSend, // message 
				totalLen,  // message length 
				&cbWritten,// bytes written 
				NULL);     // not overlapped 
			if (!fSuccess || (cbWritten != totalLen))
			{
				DWORD gle = GetLastError();
				ATLTRACE2(atlTraceDBProvider, 0, L"** Error sending data back (%d)\n", gle);
			}

		}

		::ResetEvent(pipe->m_op.hEvent);
	}

	SetEvent(pipe->m_evtPipeMonitorFinished);

	return 0;
}


void CPipeItems::OnClientConnected(CPipeItem* pipe)
{
#if _DEBUG
	ATLTRACE(atlTraceDBProvider, 0, _T("CPipeItems::OnClientConnected(%s)\n"), pipe->sPipename.c_str());
#endif
	CPipeItems& m_cmdPipes = *pipe->m_parent;
	if (m_cmdPipes.GetAvailablePipes() == 1)
	{
		m_cmdPipes.DecrementAvailablePipes();

		CPipeItem* pipe = m_cmdPipes.AddPipeInstance();
		pipe->StartComm();
	}
}

void CPipeItems::OnClientDisconnected(CPipeItem* pipe)
{
#if _DEBUG
	ATLTRACE(atlTraceDBProvider, 0, _T("CPipeItems::OnClientDisconnected(%s)\n"), pipe->sPipename.c_str());
#endif

	CPipeItems& m_cmdPipes = *pipe->m_parent;
	m_cmdPipes.IncrementAvailablePipes();
}


void CPipeItems::DoSubscriptions()
{
	ATL::CString processes(AvailableProcesses(PGNPIPE_PREFIX L"*"));

	EnterCriticalSection(&m_dhPipesCS);

	for_each(m_pipes.begin(), m_pipes.end(), [&](pair<HANDLE, CPipeItem*> p)
	{
		CPipeItem* pipeItem = p.second;
		
		if (pipeItem->GetPipeState() != PIPESTATE_CONNECTED)
			return;

		// refresh list of processes available for profiling, and if the list changed update remote client(s)
		if (pipeItem->m_availProcesses != processes)
		{
#if _DEBUG
			ATLTRACE(atlTraceDBProvider, 0, _T("CPipeItems::DoSubscriptions(): sending notification to %s\n"), pipeItem->sPipename.c_str());
#endif
			pipeItem->m_availProcesses = processes;

			RemMsg response(MI_LISTPIPES_NTF, true);
			response << pipeItem->m_availProcesses;  // semicolon-separated list of processes

			DWORD totalLen = 0;
			const BYTE* pDataToSend = response.GetDataToSend(totalLen);
			DWORD cbWritten = 0;
			// Send a message to the pipe server. 
			BOOL fSuccess = WriteFile(
				pipeItem->m_hPipe,     // pipe handle 
				pDataToSend, // message 
				totalLen,  // message length 
				&cbWritten,// bytes written 
				NULL);     // not overlapped 
			if (!fSuccess || (cbWritten != totalLen))
			{
				DWORD gle = GetLastError();
				ATLTRACE2(atlTraceDBProvider, 0, L"** Error sending notification (%d)\n", gle);
			}
		}
	});

	LeaveCriticalSection(&m_dhPipesCS);
}
