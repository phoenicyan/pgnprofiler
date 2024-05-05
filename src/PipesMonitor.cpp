/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "PipesMonitor.h"
#include "ProfilerDef.h"

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
	if (m_pth != nullptr && m_pth->joinable())
	{
		cancellation_token.cancel();
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

/// ntdll!NtQueryDirectoryFile (NT specific!)
///
/// The function searches a directory for a file whose name and attributes
/// match those specified in the function call.
///
/// NTSYSAPI
/// NTSTATUS
/// NTAPI
/// NtQueryDirectoryFile(
///    IN HANDLE FileHandle,                      // handle to the file
///    IN HANDLE EventHandle OPTIONAL,
///    IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
///    IN PVOID ApcContext OPTIONAL,
///    OUT PIO_STATUS_BLOCK IoStatusBlock,
///    OUT PVOID Buffer,                          // pointer to the buffer to receive the result
///    IN ULONG BufferLength,                     // length of Buffer
///    IN FILE_INFORMATION_CLASS InformationClass,// information type
///    IN BOOLEAN ReturnByOne,                    // each call returns info for only one file
///    IN PUNICODE_STRING FileTemplate OPTIONAL,  // template for search
///    IN BOOLEAN Reset                           // restart search
/// );

typedef LONG(WINAPI* PROCNTQDF)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK, PVOID, ULONG,
	UINT, BOOL, PUNICODE_STRING, BOOL);

void CPipesMonitor::doMonitor()
{
	DWORD dwWaitStatus;

	// Watch the directory for file creation and deletion.
	HANDLE dwChangeHandle = FindFirstChangeNotification(
		L"\\\\.\\pipe\\",              // directory to watch 
		FALSE,                         // do not watch subtree 
		FILE_NOTIFY_CHANGE_FILE_NAME); // watch file name changes 

	if (dwChangeHandle == INVALID_HANDLE_VALUE)
	{
		printf("\n ERROR: FindFirstChangeNotification function failed.\n");
		ExitProcess(GetLastError());
	}

	// Make a final validation check on our handles.
	if (dwChangeHandle == NULL)
	{
		printf("\n ERROR: Unexpected NULL from FindFirstChangeNotification.\n");
		ExitProcess(GetLastError());
	}

	HANDLE hDir = CreateFile(L"\\\\.\\Pipe\\", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
	if (hDir == INVALID_HANDLE_VALUE)
	{
		ExitProcess(GetLastError());
	}

	while (cancellation_token)
	{
		// Wait for notification.
		//printf("\nWaiting for notification...\n");

		dwWaitStatus = WaitForSingleObject(dwChangeHandle, 500);

		switch (dwWaitStatus)
		{
		case WAIT_OBJECT_0:

			// A file was created, renamed, or deleted in the directory.
			// Refresh this directory and restart the notification.
			checkIfProviderPipe(hDir);
			if (FindNextChangeNotification(dwChangeHandle) == FALSE)
			{
				//printf("\n ERROR: FindNextChangeNotification function failed.\n");
				ExitProcess(GetLastError());
			}
			break;

		case WAIT_TIMEOUT:

			// A timeout occurred, this would happen if some value other 
			// than INFINITE is used in the Wait call and no changes occur.
			// In a single-threaded environment you might not want an
			// INFINITE wait.
			//printf("\nNo changes in the timeout period.\n");
			break;

		default:
			//printf("\n ERROR: Unhandled dwWaitStatus.\n");
			ExitProcess(GetLastError());
			break;
		}
	}

	CloseHandle(hDir);
}

void CPipesMonitor::checkIfProviderPipe(HANDLE hPipe)
{
	std::set<int> apps;
	BOOL bReset = TRUE;

	PROCNTQDF NtQueryDirectoryFile = (PROCNTQDF)GetProcAddress(GetModuleHandle(L"ntdll"), "NtQueryDirectoryFile");
	if (!NtQueryDirectoryFile)
	{
		printf("\n ERROR: Could not access NtQueryDirectoryFile.\n");
		ExitProcess(GetLastError());
	}

	PFILE_QUERY_DIRECTORY DirInfo = (PFILE_QUERY_DIRECTORY)_alloca(1024);

	while (true)
	{
		IO_STATUS_BLOCK IoStatus;
		LONG ntStatus = NtQueryDirectoryFile(hPipe, NULL, NULL, NULL, &IoStatus, DirInfo, 1024, FileDirectoryInformation, FALSE, NULL, bReset);
		if (ntStatus != NO_ERROR)
		{
			if (ntStatus == STATUS_NO_MORE_FILES)
				break;
			return;
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
			
			//ATLTRACE2(atlTraceDBProvider, 0, "CPipesMonitor::checkIfProviderPipe() got file %S\n", fileName);

			if (0 == wcsncmp(fileName, PGNPIPE_PREFIX, PGNPIPE_PREFIX_LEN))
			{
				apps.insert(_wtoi(fileName + PGNPIPE_PREFIX_LEN));
			}

			// Restore the buffer to its correct state
			fileName[endStringAt] = oldValue;
			if (TmpInfo->NextEntryOffset == 0)
				break;

			TmpInfo = (PFILE_QUERY_DIRECTORY)((LONG_PTR)TmpInfo + TmpInfo->NextEntryOffset);
		}

		bReset = FALSE;
	}

	std::set<int> createdApps, closedApps;
	std::set_difference(m_appList.begin(), m_appList.end(), apps.begin(), apps.end(),
		std::inserter(closedApps, closedApps.end()));
	std::set_difference(apps.begin(), apps.end(), m_appList.begin(), m_appList.end(),
		std::inserter(createdApps, createdApps.end()));
	if (!createdApps.empty() || !closedApps.empty())
	{
		for (int pid : closedApps)
		{
			m_cbClosed(pid);
		}

		for (int pid : createdApps)
		{
			m_cbCreated(pid);
		}

		m_appList = apps;
	}
}

#if 1
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
	const int prefix_len = wcslen(PGNPIPE_PREFIX);
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
#endif
