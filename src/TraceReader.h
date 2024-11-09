/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

class CHostLoggerItem;

#include "UnlimitedWait.h"

class CTraceReader final
{
	UnlimitedWait* volatile m_pUnlimitedWait;
	HANDLE m_hQuit;
	DWORD m_threadId;
	HWND m_mainWindow;
	CRITICAL_SECTION m_loggersCS;		// guard for m_loggers collection
	list<CHostLoggerItem*> m_loggers;

public:
	CTraceReader(void);
	~CTraceReader(void);

	inline UnlimitedWait* GetUnlimitedWait()
	{
		ATLASSERT(m_pUnlimitedWait != nullptr);
		return m_pUnlimitedWait;
	}

	DWORD Start(CHostLoggerItem* pLogger);
	void StopAll();

	static BOOL WINAPI HandleProcessLoggerOverlappedResult(/*CProcessLoggerItem&*/PVOID lpObjectContext, HANDLE hObject);
	static BOOL WINAPI HandleHostLoggerOverlappedResult(/*CHostLoggerItem&*/PVOID lpObjectContext, HANDLE hObject);

protected:
	static DWORD WINAPI ThreadProc(LPVOID);
	static VOID WINAPI OnTimeoutCallback(PVOID lpWaitContext);
	static VOID WINAPI OnApcCallback(PVOID lpWaitContext);
};
