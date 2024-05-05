/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "../ProfilerServer.h"
#include "RemoteApi.h"

CRemoteApi::CRemoteApi(LPCTSTR lpszRemote, LPCTSTR lpszUser, LPCTSTR lpszPassword)
	: _lpszRemote(lpszRemote), _lpszUser(lpszUser), _lpszPassword(lpszPassword)
	, _bNeedToDeleteServiceFile(false), _bNeedToDeleteService(false), _bNeedToDetachFromAdmin(false), _bNeedToDetachFromIPC(false)
	, _hPipe(INVALID_HANDLE_VALUE)
{
	_tcscpy(_lpszRemoteServiceName, _T(PROFILERSGN));
#if LOCAL_DEBUG
	_lpszRemote = L".";
#else
	_itow(GetCurrentProcessId(), &_lpszRemoteServiceName[_tcslen(_lpszRemoteServiceName)], 10);
	_tcscat(_lpszRemoteServiceName, L"-");
	DWORD nSize = _countof(_lpszRemoteServiceName);
	GetComputerNameEx(ComputerNamePhysicalNetBIOS, &_lpszRemoteServiceName[_tcslen(_lpszRemoteServiceName)], &nSize);
#endif

	ATLTRACE2(atlTraceDBProvider, 0, L"CRemoteApi::CRemoteApi: made service name '%s'\n", _lpszRemoteServiceName);
}

CRemoteApi::~CRemoteApi()
{
	if (_hPipe != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_hPipe);
		_hPipe = INVALID_HANDLE_VALUE;
	}

	if (_bNeedToDeleteService)
		StopAndDeleteRemoteService(); // always cleanup
	if (_bNeedToDeleteServiceFile)
		DeleteFromRemote();
	if (_bNeedToDetachFromAdmin)
		Disconnect(CRemoteApi::NET_ADMIN);
	if (_bNeedToDetachFromIPC)
		Disconnect(CRemoteApi::NET_IPC);
}

bool CRemoteApi::Connect(NETRESOURCETYPE rs)
{
	if ((_bNeedToDetachFromIPC && rs == NET_IPC) || (_bNeedToDetachFromAdmin && rs == NET_ADMIN) || 0 == wcscmp(_lpszRemote, L"."))
		return true; // already connected, or no need to connect to self

	TCHAR remoteResource[_MAX_PATH * 2];
	_tcscpy(remoteResource, L"\\\\");
	_tcscat(remoteResource, _lpszRemote);
	_tcscat(remoteResource, (rs == NET_IPC) ? L"\\IPC$" : L"\\ADMIN$");

	// check if we already connected
	HANDLE hEnum = NULL;
	if (NO_ERROR == WNetOpenEnum(RESOURCE_CONNECTED, RESOURCETYPE_ANY, 0, NULL, &hEnum))
	{
		BYTE buf[65536] = { 0 };
		DWORD count = (DWORD)-1;
		DWORD bufSize = sizeof(buf);
		WNetEnumResource(hEnum, &count, buf, &bufSize);
		for (DWORD i = 0; i < count; i++)
		{
			NETRESOURCE* pNR = (NETRESOURCE*)buf;
			if (0 == _wcsicmp(pNR[i].lpRemoteName, remoteResource))
			{
				WNetCloseEnum(hEnum);
				return true;
			}
		}

		WNetCloseEnum(hEnum);
	}

	NETRESOURCE nr = { 0 };
	nr.dwType = RESOURCETYPE_ANY;
	nr.lpLocalName = NULL;
	nr.lpRemoteName = (LPWSTR)(LPCWSTR)remoteResource;
	nr.lpProvider = NULL;

	// Establish connection (using username/pwd)
	LPCTSTR lpszPassword = NULL, lpszUser = NULL;
	if (!_lpszUser.IsEmpty())
	{
		lpszUser = LPCTSTR(_lpszUser);
		lpszPassword = LPCTSTR(_lpszPassword);
	}

	DWORD rc = WNetAddConnection2(&nr, lpszPassword, lpszUser, 0);	//CONNECT_INTERACTIVE

	if (NO_ERROR == rc)
	{
		if (rs == NET_IPC)
			_bNeedToDetachFromIPC = true;
		else
			_bNeedToDetachFromAdmin = true;

		return true;
	}

	return false;
}

bool CRemoteApi::Disconnect(NETRESOURCETYPE rs)
{
	if (0 != wcscmp(_lpszRemote, L"."))
	{
		TCHAR remoteResource[_MAX_PATH * 2];
		_tcscpy(remoteResource, L"\\\\");
		_tcscat(remoteResource, _lpszRemote);

		_tcscat(remoteResource, (rs == NET_IPC) ? L"\\IPC$" : L"\\ADMIN$");

		DWORD rc = WNetCancelConnection2(remoteResource, 0, FALSE);

		if (rs == NET_IPC)
			_bNeedToDetachFromIPC = false;
		else
			_bNeedToDetachFromAdmin = false;
	}

	return true;
}

void CRemoteApi::DeleteFromRemote()
{
	if (0 == wcscmp(_lpszRemote, L"."))
	{
		return;
	}
	
	wchar_t myPath[_MAX_PATH * 2] = { 0 };
	GetModuleFileName(NULL, myPath, _countof(myPath));

	LPCWSTR pFileName = wcsrchr(myPath, L'\\');
	TCHAR dest[_MAX_PATH * 2];
	_tcscpy(dest, L"\\\\");
	_tcscat(dest, _lpszRemote);
	_tcscat(dest, L"\\ADMIN$\\");
	_tcscat(dest, GetRemoteServiceName());
	_tcscat(dest, L".exe");

	int tryCount = 0;
	while (FALSE == DeleteFile(dest))
	{
		if (++tryCount < 30)
		{
			Sleep(200);
			continue;
		}

		DWORD err = GetLastError();
		ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to delete [%s], error=%d.\n", dest, err);
		break;
	}

#if _DEBUG
	_tcscpy(dest, L"\\\\");
	_tcscat(dest, _lpszRemote);
	_tcscat(dest, L"\\ADMIN$\\PGNProfiler.pdb");
	DeleteFile(dest);
#endif
}

bool CRemoteApi::CopyToRemote()
{
	if (0 == wcscmp(_lpszRemote, L"."))
	{
		return true;
	}

	wchar_t myPath[_MAX_PATH * 2] = { 0 };
	GetModuleFileName(NULL, myPath, _countof(myPath));

	TCHAR dest[_MAX_PATH * 2];
	_tcscpy(dest, L"\\\\");
	_tcscat(dest, _lpszRemote);
	_tcscat(dest, L"\\ADMIN$\\");
	_tcscat(dest, GetRemoteServiceName());
	_tcscat(dest, L".exe");

	if ((FALSE == CopyFile(myPath, dest, FALSE)) && (false == _bNeedToDetachFromAdmin))
	{
		//if (gbStop)
		//	return false;

		// try attaching, and then try copy again
		Connect(NET_ADMIN);

		//if (gbStop)
		//	return false;

		if (FALSE == CopyFile(myPath, dest, FALSE))
		{
			DWORD err = GetLastError();
			if (ERROR_SHARING_VIOLATION != err)
			{
				ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to copy [%s] to [%s], error=%d.\n", myPath, dest, err);
				return false;
			}
		}
		else
		{
			_bNeedToDeleteServiceFile = true;
#if _DEBUG
			_tcscpy(&myPath[_tcslen(myPath) - 4], L".pdb");
			_tcscpy(dest, L"\\\\");
			_tcscat(dest, _lpszRemote);
			_tcscat(dest, L"\\ADMIN$\\PGNProfiler.pdb");
			CopyFile(myPath, dest, FALSE);
#endif
		}
	}
	else
	{
		_bNeedToDeleteServiceFile = true;
#if _DEBUG
		_tcscpy(&myPath[_tcslen(myPath)-4], L".pdb");
		_tcscpy(dest, L"\\\\");
		_tcscat(dest, _lpszRemote);
		_tcscat(dest, L"\\ADMIN$\\PGNProfiler.pdb");
		CopyFile(myPath, dest, FALSE);
#endif
	}

	return true;
}

void CRemoteApi::StopAndDeleteRemoteService()
{
	LPCTSTR remoteServer = _lpszRemote;
	if (0 == wcscmp(remoteServer, L"."))
		remoteServer = NULL;

	SC_HANDLE hSCM = ::OpenSCManager(remoteServer, NULL, SC_MANAGER_ALL_ACCESS);

	DWORD gle = 0;
	if (NULL == hSCM && !_bNeedToDetachFromIPC)
	{
		// try attaching, and then try open again
		Connect(NET_IPC);
		hSCM = ::OpenSCManager(remoteServer, NULL, SC_MANAGER_ALL_ACCESS);
		gle = GetLastError();
	}

	if (NULL == hSCM)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to connect to Service Control Manager on %s, error=%d.\n", remoteServer ? remoteServer : L"(local)", gle);
		return;
	}

	// try to stop and delete now
	SC_HANDLE hService = ::OpenService(hSCM, GetRemoteServiceName(), SERVICE_ALL_ACCESS);
	if (hService != NULL)
	{
		SERVICE_STATUS stat = { 0 };
		BOOL b = ControlService(hService, SERVICE_CONTROL_STOP, &stat);
		if (FALSE == b)
		{
			gle = GetLastError();
			ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to stop PGNProfiler service, error=%d.\n", gle);
		}

		int tryCount = 0;
		while (true)
		{
			SERVICE_STATUS_PROCESS ssp = { 0 };
			DWORD needed = 0;
			if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (BYTE*)&ssp, sizeof(ssp), &needed))
			{
				if (SERVICE_STOPPED == ssp.dwCurrentState)
				{
					if (DeleteService(hService))
						break;
					else
					{
						ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to delete PGNProfiler service, error=%d\n", GetLastError());
						break;
					}
				}
			}

			tryCount++;
			if (tryCount > 300)
				break; //waited 30 seconds

			Sleep(100);
		}

		::CloseServiceHandle(hService);
	}

	::CloseServiceHandle(hSCM);
}

// Installs and starts the remote service on remote machine
bool CRemoteApi::InstallAndStartRemoteService()
{
	LPCTSTR remoteServer = _lpszRemote;
	if (0 == wcscmp(remoteServer, L"."))
		remoteServer = NULL;

	SC_HANDLE hSCM = ::OpenSCManager(remoteServer, NULL, SC_MANAGER_ALL_ACCESS);
	DWORD gle = GetLastError();

	//if (gbStop)
	//	return false;

	if (NULL == hSCM && !_bNeedToDetachFromIPC)
	{
		//try attaching, and then try open again
		Connect(NET_IPC);
		hSCM = ::OpenSCManager(remoteServer, NULL, SC_MANAGER_ALL_ACCESS);
		gle = GetLastError();
	}

	//if (gbStop)
	//	return false;

	if (NULL == hSCM)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to connect to Service Control Manager on %s, error=%d.", remoteServer ? remoteServer : L"(local)", gle);
		return false;
	}

	LPCTSTR remoteServiceName = GetRemoteServiceName();

	// Maybe it's already there and installed, let's try to run
	SC_HANDLE hService = ::OpenService(hSCM, remoteServiceName, SERVICE_ALL_ACCESS);
	if (NULL == hService)
	{
		DWORD serviceType = SERVICE_WIN32_OWN_PROCESS;
		//if (((DWORD)-1 != settings.sessionToInteractWith) || (settings.bInteractive))
			serviceType |= SERVICE_INTERACTIVE_PROCESS;

		TCHAR svcExePath[_MAX_PATH * 2];
		_tcscpy(svcExePath, L"%SystemRoot%\\");
		_tcscat(svcExePath, remoteServiceName);
		_tcscat(svcExePath, L".exe");

		if (NULL == remoteServer)
		{
			GetWindowsDirectory(svcExePath, _countof(svcExePath));
			_tcscat(svcExePath, L"\\");
			_tcscat(svcExePath, remoteServiceName);
			_tcscat(svcExePath, L".exe");
		}

		_tcscat(svcExePath, L" -service"); // so it knows it is the service when it starts

		hService = ::CreateService(hSCM, remoteServiceName, remoteServiceName,
			SERVICE_ALL_ACCESS,
			serviceType,
			SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
			svcExePath,
			NULL, NULL, NULL,
			NULL, NULL); //using LocalSystem

		if (NULL == hService)
		{
			DWORD gle = GetLastError();
			::CloseServiceHandle(hSCM);
			ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to install service on %s, error=%d.\n", remoteServer ? remoteServer : L"(local)", gle);
			return false;
		}

		_bNeedToDeleteService = true;
	}

	//if (gbStop)
	//	return false;

	if (!StartService(hService, 0, NULL))
	{
		DWORD gle = GetLastError();
		if (ERROR_SERVICE_ALREADY_RUNNING != gle)
		{
			::CloseServiceHandle(hService);
			::CloseServiceHandle(hSCM);
			ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to start service on %s, error=%d\n", remoteServer ? remoteServer : L"(local)", gle);
			return false;
		}
	}

	::CloseServiceHandle(hService);
	::CloseServiceHandle(hSCM);

	return true;
}

// Populate list of processes available for profiling on remote host
// Return: true - if success; false - if erroe occurred.
bool CRemoteApi::GetProcesses(ATL::CString& processes)
{
	RemMsg msgOut(MI_LISTPIPES_REQ), msgReturned;

	msgOut << PGNPIPE_PREFIX L"*";	// retrieve filter

	ATLTRACE2(atlTraceDBProvider, 0, L"--> MI_LISTPIPES\n");

	bool res = SendRequest(msgOut, msgReturned);
	if (MI_LISTPIPES_RESP == msgReturned.m_msgID)
	{
		msgReturned >> processes;
		ATLTRACE2(atlTraceDBProvider, 0, L"<-- msgReturned(%s)\n", processes);
		return true;
	}

	ATLTRACE2(atlTraceDBProvider, 0, L"<-- FAILURE ***\n");
	
	return false;
}

bool CRemoteApi::SendRequest(RemMsg& msgOut, RemMsg& msgReturned)
{
	DWORD gle = 0;

	if (INVALID_HANDLE_VALUE == _hPipe) //need to connect?
	{
		TCHAR pipeName[_MAX_PATH];
		_tcscpy(pipeName, L"\\\\");
		_tcscat(pipeName, _lpszRemote);
		_tcscat(pipeName, L"\\pipe\\");
		_tcscat(pipeName, GetRemoteServiceName());
		_tcscat(pipeName, L".exe");

		int count = 0;
		while (true)
		{
			_hPipe = CreateFile(pipeName, GENERIC_READ | GENERIC_WRITE,
				0,              // no sharing 
				NULL,           // default security attributes
				OPEN_EXISTING,  // opens existing pipe 
				FILE_FLAG_OVERLAPPED, //using overlapped so we can poll gbStop flag too
				NULL);          // no template file 

			// Break if the pipe handle is valid. 
			if (_hPipe != INVALID_HANDLE_VALUE)
				break;

			// Exit if an error other than ERROR_PIPE_BUSY occurs. 
			gle = GetLastError();
			if ((ERROR_PIPE_BUSY != gle) && (ERROR_FILE_NOT_FOUND != gle))
			{
				ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to open communication channel to %s, %d.\n", _lpszRemote, gle);
				return false;
			}

			count++;
			if (10 == count)
			{
				gle = GetLastError();
				ATLTRACE2(atlTraceDBProvider, 0, L"** Timed out waiting for communication channel to %s, %d.\n", _lpszRemote, gle);
				return false;
			}

			Sleep(1000);
		}
	}

	//have a pipe so send
	// The pipe connected; change to message-read mode. 

	DWORD dwMode = PIPE_READMODE_MESSAGE;
	BOOL fSuccess = SetNamedPipeHandleState(
		_hPipe,    // pipe handle 
		&dwMode,  // new pipe mode 
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 
	if (!fSuccess)
	{
		gle = GetLastError();
		ATLTRACE2(atlTraceDBProvider, 0, L"** Failed to set communication channel to %s, error=%d.\n", _lpszRemote, gle);
		CloseHandle(_hPipe);
		_hPipe = INVALID_HANDLE_VALUE;
		return false;
	}

	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	OVERLAPPED ol = { 0 };
	ol.hEvent = hEvent;

	DWORD totalLen = 0;
	const BYTE* pDataToSend = msgOut.GetDataToSend(totalLen);
	DWORD cbWritten = 0;
	// Send a message to the pipe server. 
	fSuccess = WriteFile(
		_hPipe,     // pipe handle 
		pDataToSend, // message 
		totalLen,  // message length 
		&cbWritten,// bytes written 
		&ol);

	if (!fSuccess)
	{
		gle = GetLastError();
		if (ERROR_IO_PENDING != gle)
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"** Error communicating with %s, %d.\n", _lpszRemote, gle);
			CloseHandle(_hPipe);
			_hPipe = INVALID_HANDLE_VALUE;
			return false;
		}
	}

	while (true)
	{
		if (WAIT_OBJECT_0 == WaitForSingleObject(ol.hEvent, 0))
		{
			GetOverlappedResult(_hPipe, &ol, &cbWritten, FALSE);
			break;
		}

		//if (gbStop)
		//	return false;

		Sleep(100);
	}

	bool bFirstRead = true;
	do
	{
		BYTE buffer[16384] = { 0 };

		ol.Offset = 0;
		ol.OffsetHigh = 0;

		DWORD cbRead = 0;
		// Read from the pipe. 
		fSuccess = ReadFile(
			_hPipe,			// pipe handle 
			buffer,			// buffer to receive reply 
			sizeof(buffer), // size of buffer 
			&cbRead,  // number of bytes read 
			&ol);

		if (!fSuccess)
		{
			gle = GetLastError();
			if (ERROR_IO_PENDING != gle)
			{
				ATLTRACE2(atlTraceDBProvider, 0, L"** Error reading response from %s, %d.\n", _lpszRemote, gle);
				CloseHandle(_hPipe);
				_hPipe = INVALID_HANDLE_VALUE;
				return false;
			}
		}

		while (true)
		{
			if (WAIT_OBJECT_0 == WaitForSingleObject(ol.hEvent, 0))
			{
				GetOverlappedResult(_hPipe, &ol, &cbRead, FALSE);
				break;
			}

			//if (gbStop)
			//	return false;

			Sleep(100);
		}

		if (bFirstRead)
		{
			if (cbRead < sizeof(WORD))
			{
				gle = GetLastError();
				ATLTRACE2(atlTraceDBProvider, 0, L"Received too little data from %s, %d.\n", _lpszRemote, gle);
				CloseHandle(_hPipe);
				_hPipe = INVALID_HANDLE_VALUE;
				return false;
			}
			msgReturned.SetFromReceivedData(buffer, cbRead);
			bFirstRead = false;
		}
		else
			msgReturned.m_payload.insert(msgReturned.m_payload.end(), buffer, buffer + cbRead);
	} while (msgReturned.m_expectedLen != msgReturned.m_payload.size());

	CloseHandle(hEvent);
	return true;
}
