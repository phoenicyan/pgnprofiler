/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

class CHostLoggerItem;

class CTraceReader
{
	DWORD m_threadId;
	HWND m_mainWindow, m_hWnd;
	CRITICAL_SECTION m_loggersCS;		// guard for m_loggers collection
	list<CHostLoggerItem*> m_loggers;

public:
	CTraceReader(void);
	~CTraceReader(void);

	void SetMainWindow(HWND mainWindow);
	DWORD Start(CHostLoggerItem* pLogger);
	void StopAll();

protected:
	static DWORD WINAPI ThreadProc(LPVOID);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
	HWND CreateVirtualWindow();
	void DestroyVirtualWindow(HWND hWnd);

	void OnTimer(HWND hWnd, int timerID);

	void HandleProcessLoggerOverlappedResult(CProcessLoggerItem& processLogger);

	void HandleHostLoggerOverlappedResult(CHostLoggerItem& hostLogger);
};
