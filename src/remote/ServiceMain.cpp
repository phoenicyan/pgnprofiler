/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "RemoteApi.h"
#include "PipeItem.h"

#define SERVICE_NAME  _T("PGNProfilerSvc")
#define SUBSCRIPTIONS_TIMER	3

SERVICE_STATUS			g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE	g_StatusHandle = NULL;
HWND					m_hWnd;

void WINAPI ServiceControlHandler(DWORD dwControl)
{
	switch (dwControl)
	{
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		ATLTRACE2(atlTraceDBProvider, 0, _T("ServiceControlHandler: SERVICE_CONTROL_STOP Request\n"));

		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		/*
		* Perform tasks neccesary to stop the service here
		*/
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			ATLTRACE2(atlTraceDBProvider, 0, _T("** ServiceControlHandler: SetServiceStatus returned error\n"));
		}

		// This will signal the worker thread to start shutting down
		//SetEvent(g_ServiceStopEvent);

		PostMessage(m_hWnd, WM_QUIT, 0, 0);
		break;

	case SERVICE_CONTROL_PAUSE:
		break;

	case SERVICE_CONTROL_CONTINUE:
		break;

	case SERVICE_CONTROL_INTERROGATE:
		break;
	}
}

extern BOOL Is64bitProcess(HANDLE hProcess);
extern DWORD GetAppList(DWORD* appList, /*inout*/int* entries);

ATL::CString AvailableProcesses(LPCTSTR /*filter*/)
{
	DWORD appList[5000];
	int entries = 5000;
	GetAppList(appList, &entries);

	ATL::CString processes;

	// Make semi-colon separated list of application names. 
	// Example result: App1.exe:234x64;App2:7580;
	// App name is followed by colon, and 'x64' word for 64-bit apps only.
	for (int i = 0; i < entries; i++)
	{
		// Get the process name.
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION /*| PROCESS_VM_READ*/, FALSE, appList[i]);
		if (hProcess == 0)
		{
			DWORD err = GetLastError();
			ATLTRACE2(atlTraceDBProvider, 0, L"** CRemoteApi::HandleMsg: Could not open process id=%d, err=%d.\n", appList[i], err);
			continue;
		}
		TCHAR buf[16], szProcessName[MAX_PATH * 2], *pProcessName;
		if (GetProcessImageFileName(hProcess, szProcessName, MAX_PATH * 2) != 0)
		{
			pProcessName = wcsrchr(szProcessName, '\\') + 1;
			if (wcsnicmp(L"PGNProfiler.exe", pProcessName, 15) != 0 &&	// do not add PGNProfiler.exe
				wcsnicmp(L"PGNPUpdate.exe", pProcessName, 14) != 0)		// do not add PGNPUpdate.exe
			{
				processes.Append(pProcessName);
				processes.AppendChar(L':');
				processes.Append(_itow(appList[i], buf, 10));
				if (Is64bitProcess(hProcess))
					processes.AppendChar(L'x');

				processes.AppendChar(L';');
			}
			CloseHandle(hProcess);
		}
	}

	return processes;
}

static void OnTimer(HWND hWnd, int timerID)
{
	if (SUBSCRIPTIONS_TIMER == timerID)
	{
		CPipeItems& m_cmdPipes = *(CPipeItems*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		m_cmdPipes.DoSubscriptions();

		return;
	}
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		break;

	case WM_TIMER:
		OnTimer(hWnd, (int)wParam);
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}


VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"ServiceMain is starting.\n");
	
	CPipeItems m_cmdPipes;

	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceControlHandler);
	if (0 == g_StatusHandle)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"** RegisterServiceCtrlHandler failed, error=%d.\n", GetLastError());
#if !_DEBUG	// proceed for debugging
		return;
#endif
	}

	// Tell the service controller we are starting
	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		ATLTRACE2(atlTraceDBProvider, 0, _T("** SetServiceStatus(SERVICE_START_PENDING) returned error\n"));
	}

	ATLTRACE2(atlTraceDBProvider, 0, _T("Performing Service Start Operations\n"));

	//// Create stop event to wait on later.
	//g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	//if (g_ServiceStopEvent == NULL)
	//{
	//	ATLTRACE2(atlTraceDBProvider, 0, _T("** CreateEvent(g_ServiceStopEvent) returned error\n"));

	//	g_ServiceStatus.dwControlsAccepted = 0;
	//	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	//	g_ServiceStatus.dwWin32ExitCode = GetLastError();
	//	g_ServiceStatus.dwCheckPoint = 1;

	//	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	//	{
	//		ATLTRACE2(atlTraceDBProvider, 0, _T("** SetServiceStatus(SERVICE_STOPPED) returned error\n"));
	//	}
	//	goto EXIT;
	//}

	// Tell the service controller we are started
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; // service name isn't validated since there is only one service running in the process
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;
	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		ATLTRACE2(atlTraceDBProvider, 0, _T("** SetServiceStatus(SERVICE_RUNNING) returned error\n"));
	}

	ATLTRACE2(atlTraceDBProvider, 0, L"Service is running. Entering main loop!\n");

	// CreateVirtualWindow
	WNDCLASSEX wndcls;
	HINSTANCE hInst = GetModuleHandle(NULL);

	if (!::GetClassInfoEx(hInst, PGNPROF_SERVICE_CLASS, &wndcls))
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
		wndcls.lpszClassName = PGNPROF_SERVICE_CLASS;		// name of window class 
		wndcls.hIconSm = NULL;

		if (!::RegisterClassEx(&wndcls))
		{
			ATLTRACE2(atlTraceDBProvider, 0, _T("** Could nor register worker window class\n"));
			return;
		}
	}

	m_hWnd = ::CreateWindow(PGNPROF_SERVICE_CLASS, L"", WS_POPUPWINDOW, 0, 0, 0, 0, NULL, NULL, hInst, NULL);
	if (m_hWnd != NULL)
	{
		MSG msg;
		::PeekMessage(&msg, m_hWnd, 0, 0, PM_NOREMOVE);
	}
	else
	{
		ATLTRACE2(atlTraceDBProvider, 0, _T("PGNP Profiler Service: ServiceMain: Could nor create worker window\n"));
		return;
	}

	SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)&m_cmdPipes);

	// create profiler pipe
	CPipeItem* pipe = m_cmdPipes.AddPipeInstance();
	pipe->StartComm();

	// configure subscriptions timer
	SetTimer(m_hWnd, SUBSCRIPTIONS_TIMER, 10000, 0);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	::DestroyWindow(m_hWnd);
	::UnregisterClass(PGNPROF_SERVICE_CLASS, hInst);

	m_cmdPipes.Clear();

	ATLTRACE2(atlTraceDBProvider, 0, L"Exiting loop.\n");

	//CloseHandle(g_ServiceStopEvent);

	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 3;
	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		ATLTRACE2(atlTraceDBProvider, 0, _T("** SetServiceStatus(SERVICE_STOPPED) returned error\n"));
	}

EXIT:
	ATLTRACE2(atlTraceDBProvider, 0, _T("ServiceMain: Exit\n"));
}


int StartLocalService()
{
	ATLTRACE2(atlTraceDBProvider, 0, _T("PGNP Profiler Service: Main: Entry\n"));

	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
		{ NULL, NULL }
	};

	// start as NT service
	if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
	{
		DWORD gle = GetLastError();

		ATLTRACE2(atlTraceDBProvider, 0, _T("** StartServiceCtrlDispatcher returned error %d\n"), gle);

#if _DEBUG
		if (ERROR_FAILED_SERVICE_CONTROLLER_CONNECT == gle)	// for debugging purposes still call ServiceMain
		{
			ServiceMain(0, NULL);
		}
#endif
		return gle;
	}

	ATLTRACE2(atlTraceDBProvider, 0, _T("PGNP Profiler Service: Main: Exit\n"));

	// take an additional step and try to delete ourself
	SC_HANDLE sch = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);
	if (NULL != sch)
	{
		DWORD myPID = GetCurrentProcessId();

		BYTE buffer[63 * 1024] = { 0 };
		DWORD ignored;
		DWORD serviceCount = 0;
		DWORD resume = 0;
		if (EnumServicesStatusEx(sch, SC_ENUM_PROCESS_INFO, SERVICE_WIN32_OWN_PROCESS, SERVICE_STATE_ALL, buffer, sizeof(buffer), &ignored, &serviceCount, &resume, NULL))
		{
			ENUM_SERVICE_STATUS_PROCESS* pStruct = (ENUM_SERVICE_STATUS_PROCESS*)buffer;
			for (DWORD i = 0; i < serviceCount; i++)
			{
				if (pStruct[i].ServiceStatusProcess.dwProcessId == myPID)
				{
					SC_HANDLE hSvc = OpenService(sch, pStruct[i].lpServiceName, SC_MANAGER_ALL_ACCESS);
					if (NULL != hSvc)
					{
						//service should be marked as stopped if we are down here
						::DeleteService(hSvc);
						CloseServiceHandle(hSvc);
					}
					break;
				}
			}
		}

		CloseServiceHandle(sch);
	}
	
	return 0;
}
