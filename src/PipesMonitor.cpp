/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "PipesMonitor.h"
#include "ProfilerDef.h"
//TODO: replace boost with std?
#include <boost/chrono/system_clocks.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
namespace pt = boost::posix_time;
using namespace boost::interprocess;

CPipesMonitor::CPipesMonitor(PipeCallback cbCreated, PipeCallback cbClosed)
	: m_cbCreated(cbCreated), m_cbClosed(cbClosed)
{
}

void CPipesMonitor::start()
{
	m_pth = new rethread::thread(std::bind(&CPipesMonitor::doMonitor, this));
}

void CPipesMonitor::stop()
{
	cancellation_token.cancel();

	if (m_pth != nullptr)
	{
		if (m_pth->joinable())
			m_pth->join();
		delete m_pth;
		m_pth = nullptr;
	}
}

#define FileDirectoryInformation 1   
#define STATUS_NO_MORE_FILES 0x80000006L  

typedef struct
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

typedef struct _IO_STATUS_BLOCK {
	union {
		NTSTATUS Status;
		PVOID    Pointer;
	};
	ULONG_PTR Information;
} IO_STATUS_BLOCK, * PIO_STATUS_BLOCK;

typedef struct
{
	ULONG NextEntryOffset;
	ULONG FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	ULONG FileAttributes;
	ULONG FileNameLength;
	union
	{
		struct
		{
			WCHAR FileName[1];
		} FileDirectoryInformationClass;

		struct
		{
			DWORD dwUknown1;
			WCHAR FileName[1];
		} FileFullDirectoryInformationClass;

		struct
		{
			DWORD dwUknown2;
			USHORT AltFileNameLen;
			WCHAR AltFileName[12];
			WCHAR FileName[1];
		} FileBothDirectoryInformationClass;
	};
} FILE_QUERY_DIRECTORY, * PFILE_QUERY_DIRECTORY;

typedef LONG(WINAPI* PROCNTQDF)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK, PVOID, ULONG,
	UINT, BOOL, PUNICODE_STRING, BOOL);

void CPipesMonitor::doMonitor()
{
	OVERLAPPED oConnect;
	oConnect.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	ATLASSERT(oConnect.hEvent != 0);

	BOOL fPendingIO = CreateAndConnectInstance(&oConnect);

	while (cancellation_token)
	{
		const DWORD dwWait = WaitForSingleObjectEx(oConnect.hEvent, 1000, TRUE);

		switch (dwWait)
		{
		case 0:
		{
			// The wait conditions are satisfied by a completed connect operation. 
			// If an operation is pending, get the result of the connect operation. 
			if (fPendingIO)
			{
				DWORD cbRet;
				if (!GetOverlappedResult(m_hPipe, &oConnect, &cbRet, FALSE))
				{
					ATLTRACE2(atlTraceDBProvider, 0, _T("** ConnectNamedPipe error %d\n"), GetLastError());
					return;
				}
			}

			LPPIPEINST lpPipeInst = (LPPIPEINST)GlobalAlloc(GPTR, sizeof(PIPEINST));
			lpPipeInst->hPipeInst = m_hPipe;
			lpPipeInst->p = this;

			// Start the read operation for this client. 
			// Note that this same routine is later used as a 
			// completion routine after a write operation. 
			lpPipeInst->cbToWrite = 0;
			CompletedWriteRoutine(0, 0, lpPipeInst);

			// Create new pipe instance for the next client. 
			fPendingIO = CreateAndConnectInstance(&oConnect);
			break;
		}

		case WAIT_IO_COMPLETION:
			// The wait is satisfied by a completed read or write 
			// operation. This allows the system to execute the 
			// completion routine. 
			break;

		default:
			// An error occurred in the wait function. 
			ATLTRACE2(atlTraceDBProvider, 3, _T("** WaitForSingleObjectEx (%d)\n"), GetLastError());
		}
	}
}

VOID WINAPI CPipesMonitor::CompletedWriteRoutine(DWORD dwErr, DWORD cbWritten, LPOVERLAPPED lpOverLap)
{
	ATLTRACE2(atlTraceDBProvider, 0, _T("Enter CompletedWriteRoutine\n"));

	BOOL fRead = FALSE;

	// lpOverlap points to storage for this instance. 
	LPPIPEINST lpPipeInst = (LPPIPEINST)lpOverLap;

	// The write operation has finished, so read the next request (if there is no error). 
	if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))
		fRead = ReadFileEx(
			lpPipeInst->hPipeInst,
			&lpPipeInst->chRequest[0],
			BUFSIZE * sizeof(TCHAR),
			lpOverLap,
			(LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);

	// Disconnect if an error occurred.
	if (!fRead)
	{
		//ATLTRACE2(atlTraceDBProvider, 0, _T("** Error in ReadFileEx %d\n"), GetLastError());
		lpPipeInst->p->DisconnectAndClose(lpPipeInst);
	}

	ATLTRACE2(atlTraceDBProvider, 0, _T("Leave CompletedWriteRoutine\n"));
}

// CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED) 
// This routine is called as an I/O completion routine after reading 
// a request from the client. It gets data and writes it to the pipe. 
VOID WINAPI CPipesMonitor::CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead, LPOVERLAPPED lpOverLap)
{
	ATLTRACE2(atlTraceDBProvider, 0, _T("Enter CompletedReadRoutine\n"));

	BOOL fWrite = FALSE;

	// lpOverlap points to storage for this instance. 
	LPPIPEINST lpPipeInst = (LPPIPEINST)lpOverLap;

	// The read operation has finished, so write a response (if no error occurred). 
	if ((dwErr == 0) && (cbBytesRead != 0))
	{
		ATLTRACE2(atlTraceDBProvider, 0, _T("[%p] %s\n"), lpPipeInst->hPipeInst, lpPipeInst->chRequest);

		StartupComm sc(0, 0);
		sc.fromBase64(lpPipeInst->chRequest);

		switch (sc.GetID())
		{
		case 1:
			lpPipeInst->p->m_cbCreated(sc.GetPayload()); break;
		case 2:
			lpPipeInst->p->m_cbClosed(sc.GetPayload()); break;
		default:
			break;
		}

		_tcscpy(lpPipeInst->chReply, TEXT("T0s="));
		lpPipeInst->cbToWrite = (lstrlen(lpPipeInst->chReply) + 1) * sizeof(TCHAR);

		fWrite = WriteFileEx(
			lpPipeInst->hPipeInst,
			lpPipeInst->chReply,
			lpPipeInst->cbToWrite,
			lpOverLap,
			(LPOVERLAPPED_COMPLETION_ROUTINE)CompletedWriteRoutine);
	}

	// Disconnect if an error occurred.
	if (!fWrite)
	{
		ATLTRACE2(atlTraceDBProvider, 0, _T("** Error in WriteFileEx %d\n"), GetLastError());
		lpPipeInst->p->DisconnectAndClose(lpPipeInst);
	}

	ATLTRACE2(atlTraceDBProvider, 0, _T("Leave CompletedReadRoutine\n"));
}

// DisconnectAndClose(LPPIPEINST) 
// This routine is called when an error occurs or the client closes 
// its handle to the pipe. 
VOID CPipesMonitor::DisconnectAndClose(LPPIPEINST lpPipeInst)
{
	ATLTRACE2(atlTraceDBProvider, 0, _T("Enter DisconnectAndClose(%p)\n"), lpPipeInst->hPipeInst);

	// Disconnect the pipe instance. 
	if (!DisconnectNamedPipe(lpPipeInst->hPipeInst))
	{
		ATLTRACE2(atlTraceDBProvider, 0, _T("** DisconnectNamedPipe failed with %d.\n"), GetLastError());
	}

	// Close the handle to the pipe instance. 
	CloseHandle(lpPipeInst->hPipeInst);

	// Release the storage for the pipe instance. 
	if (lpPipeInst != NULL)
		GlobalFree(lpPipeInst);

	ATLTRACE2(atlTraceDBProvider, 0, _T("Leave DisconnectAndClose\n"));
}

// CreateAndConnectInstance(LPOVERLAPPED) 
// This function creates a pipe instance and connects to the client. 
// It returns TRUE if the connect operation is pending, and FALSE if 
// the connection has been completed. 
BOOL CPipesMonitor::CreateAndConnectInstance(LPOVERLAPPED lpoOverlap)
{
	ATLTRACE2(atlTraceDBProvider, 0, _T("Enter CreateAndConnectInstance()\n"));

	LPCTSTR lpszPipename = _T("\\\\.\\pipe\\") PGNPIPE_PREFIX _T("comm");

	m_hPipe = CreateNamedPipe(
		lpszPipename,             // pipe name 
		PIPE_ACCESS_DUPLEX |      // read/write access 
		FILE_FLAG_OVERLAPPED,     // overlapped mode 
		PIPE_TYPE_MESSAGE |       // message-type pipe 
		PIPE_READMODE_MESSAGE |   // message read mode 
		PIPE_WAIT,                // blocking mode 
		PIPE_UNLIMITED_INSTANCES, // unlimited instances 
		BUFSIZE * sizeof(TCHAR),    // output buffer size 
		BUFSIZE * sizeof(TCHAR),    // input buffer size 
		PIPE_TIMEOUT,             // client time-out 
		NULL);                    // default security attributes
	if (m_hPipe == INVALID_HANDLE_VALUE)
	{
		ATLTRACE2(atlTraceDBProvider, 0, _T("** CreateNamedPipe failed with %d.\n"), GetLastError());
		return 0;
	}

	// Call a subroutine to connect to the new client. 
	auto rez = ConnectToNewClient(m_hPipe, lpoOverlap);

	ATLTRACE2(atlTraceDBProvider, 0, _T("Leave CreateAndConnectInstance()\n"));

	return rez;
}

BOOL CPipesMonitor::ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
	ATLTRACE2(atlTraceDBProvider, 0, _T("Enter ConnectToNewClient(hPipe=%p)\n"), hPipe);

	BOOL fPendingIO = FALSE;

	// Start an overlapped connection for this pipe instance. 
	BOOL fConnected = ConnectNamedPipe(hPipe, lpo);

	// Overlapped ConnectNamedPipe should return zero. 
	if (fConnected)
	{
		ATLTRACE2(atlTraceDBProvider, 0, _T("** ConnectNamedPipe failed with %d.\n"), GetLastError());
		return 0;
	}

	switch (GetLastError())
	{
	case ERROR_IO_PENDING:		// The overlapped connection in progress
		fPendingIO = TRUE;
		break;

	case ERROR_PIPE_CONNECTED:		// Client is already connected, so signal an event
		if (SetEvent(lpo->hEvent))
			break;

	default:		// If an error occurs during the connect operation... 
		{
			ATLTRACE2(atlTraceDBProvider, 0, _T("** ConnectNamedPipe failed with %d.\n"), GetLastError());
			return 0;
		}
	}

	ATLTRACE2(atlTraceDBProvider, 0, _T("Leave ConnectToNewClient\n"));

	return fPendingIO;
}

/// appList - destination list of PIDs;
/// entries - [in] max size of PIDs, [out] actual number of PIDs in appList;
/// Return: 0 - success, otherwise error.
DWORD GetAppList(DWORD* appList, /*inout*/int* entries)
{
	if (appList == 0 || entries == 0 || *entries <= 0)
		return 2;

	HANDLE hPipe;
	BOOL bReset = TRUE;

	PROCNTQDF NtQueryDirectoryFile = (PROCNTQDF)GetProcAddress(GetModuleHandle(L"ntdll"), "NtQueryDirectoryFile");
	if (!NtQueryDirectoryFile)
		return 1;

	hPipe = CreateFile(L"\\\\.\\Pipe\\", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		DWORD dwErr = GetLastError();
		return 1;
	}

	PFILE_QUERY_DIRECTORY DirInfo = (PFILE_QUERY_DIRECTORY)_alloca(1024);
	const auto prefix_len = wcslen(PGNPIPE_PREFIX);
	const int max_entries = *entries;
	*entries = 0;

	while (*entries < max_entries)
	{
		IO_STATUS_BLOCK IoStatus;
		LONG ntStatus = NtQueryDirectoryFile(hPipe, NULL, NULL, NULL, &IoStatus, DirInfo, 1024, FileDirectoryInformation, FALSE, NULL, bReset);

		if (ntStatus != NO_ERROR)
		{
			if (ntStatus == STATUS_NO_MORE_FILES)
				break;
			return 1;
		}

		PFILE_QUERY_DIRECTORY TmpInfo = DirInfo;

		while (1)
		{
			// Store old values before we mangle the buffer
			const int endStringAt = TmpInfo->FileNameLength / sizeof(WCHAR);
			WCHAR* fileName = &TmpInfo->FileDirectoryInformationClass.FileName[0];
			const WCHAR oldValue = fileName[endStringAt];

			// Place a null character at the end of the string so wprintf doesn't read past the end
			fileName[endStringAt] = NULL;

			//ATLTRACE2(atlTraceDBProvider, 0, "GetAppList() got file %S\n", fileName);

			if (0 == wcsncmp(fileName, PGNPIPE_PREFIX, prefix_len))
			{
				appList[*entries] = _wtoi(fileName + prefix_len);

				if (max_entries == ++(*entries))
					break;
			}

			// Restore the buffer to its correct state
			fileName[endStringAt] = oldValue;
			if (TmpInfo->NextEntryOffset == 0)
				break;

			TmpInfo = (PFILE_QUERY_DIRECTORY)((LONG_PTR)TmpInfo + TmpInfo->NextEntryOffset);
		}

		bReset = FALSE;
	}

	CloseHandle(hPipe);

	return 0;
}
