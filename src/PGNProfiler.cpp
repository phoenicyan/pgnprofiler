/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "resource.h"
#include "FilterView.h"
#include "ExplorerView.h"
#include "PGNProfilerView.h"
#include "DetailsView.h"
#include "aboutdlg.h"
#include "MainFrm.h"
#include "ProfilerServer.h"
#include "OneInstance.h"
#include "PipesMonitor.h"

extern int StartLocalService();

CAppModule _Module;
CColorPalette g_palit(CColorPalette::THEME_NONE, *(const vector<COLORREF>*)nullptr);

int Run(LPTSTR lpstrCmdLine = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
	if (0 == _tcscmp(lpstrCmdLine, L"-service"))
	{
		// Run in "service" mode, i.e. start as Windows Service and supply list of open pipes via a pipe with known name.
		return StartLocalService();
	}
	else
	{
		// Run in "window application" mode, i.e. collect and display trace in UI.
		LoadLibrary(CRichEditCtrl::GetLibraryName());

		CMessageLoop theLoop;
		_Module.AddMessageLoop(&theLoop);

		COneInstance guard(_T("Global\\") _T(PROFILERSGN));
		if (NULL == guard.m_hInstanceGuard || WAIT_TIMEOUT == ::WaitForSingleObject(guard.m_hInstanceGuard, 0))
		{
			// application is already running, exit
			MessageBox(NULL, L"Only one instance of the PGNP Profiler can run at a time.", L"PGNP Profiler", MB_OK | MB_ICONEXCLAMATION);
			return 0;
		}

		// grant debugger priviledges to ourselves
		HANDLE hToken;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
		{
			TOKEN_PRIVILEGES tp;
			tp.PrivilegeCount = 1;
			LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
			CloseHandle(hToken);
		}

		CMainFrame wndMain(g_palit);
		if (!wndMain.CreateMainWindow(lpstrCmdLine, nCmdShow))
		{
			return 0;
		}

		ProfilerTimerInit();

		CPipesMonitor monitor(std::bind(&CMainFrame::pipeCreatedCallback, &wndMain, std::placeholders::_1)
			, std::bind(&CMainFrame::pipeClosedCallback, &wndMain, std::placeholders::_1));
		monitor.start();

		int nRet = theLoop.Run();

		monitor.stop();

		_Module.RemoveMessageLoop();
		return nRet;
	}
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	HRESULT hRes = ::CoInitialize(NULL);
	// If you are running on NT 4.0 or higher you can use the following call instead to 
	// make the EXE free threaded. This means that calls come in on a random RPC thread.
	//HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	__AllocStdCallThunk();
	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	int nRet = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
