/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include <algorithm>
#include <functional>
#include <set>
#include "thread.hpp"
#include "cancellation_token.hpp"

typedef std::function<void(int pid)> PipeCallback;

#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096

class CPipesMonitor;

//typedef struct OVERLAPPEDX_ : public OVERLAPPED { CPipesMonitor* p; } OVERLAPPEDX;

typedef struct : public OVERLAPPED
{
	HANDLE hPipeInst;
	CPipesMonitor* p;
	TCHAR chRequest[BUFSIZE];
	DWORD cbRead;
	TCHAR chReply[BUFSIZE];
	DWORD cbToWrite;
} PIPEINST, *LPPIPEINST;

// Purpose of the CPipesMonitor is to receive notification when pgnprof_nnn pipes created or closed (by load/unload of PGNP.DLL).
// The nnn in pipe name is a process id that loads PGNP.DLL.
// Idea for implementation is from https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-server-using-completion-routines.
class CPipesMonitor
{
public:
	CPipesMonitor(PipeCallback cbCreated, PipeCallback cbClosed);

	void start();

	void stop();

private:
	PipeCallback m_cbCreated, m_cbClosed;
	rethread::standalone_cancellation_token cancellation_token;
	rethread::thread* m_pth;
	void doMonitor();

	static VOID DisconnectAndClose(LPPIPEINST lpPipeInst);
	BOOL CreateAndConnectInstance(LPOVERLAPPED);
	BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);

	static VOID WINAPI CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED);
	static VOID WINAPI CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED);

	HANDLE m_hPipe;
};
