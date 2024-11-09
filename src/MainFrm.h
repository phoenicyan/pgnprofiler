/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "FilterView.h"
#include "ExplorerView.h"
#include "PGNProfilerView.h"
#include "DetailsView.h"
#include "ProfilerDef.h"
#include "TraceReader.h"
#include "PGNPOptions.h"
#include "AboutDlg.h"
#include "CCMenu.h"
#include "ConnectHostDlg.h"
#include "gui\MyCommandBar.h"
#include "MyDialogs.h"
#include "gui\MyPaneContainer.h"
#include "gui\MySplitterWindowT.h"
#include "gui\MyToolBar.h"
#include "gui\MyReBarCtrl.h"
#include "OptionsDlg.h"

#include <chrono>
using namespace std::chrono;

// important: timer ids cannot divide evenly by 4!
#define REFRESH_APPS_TIMER		123

#define MAX_CONNECTED_HOSTS		10

class CLoggerItemBase;
class CSkinScrollWnd;

enum
{
	IDC_PROFILER_TV = 32790,
	IDC_PROFILER_LV = 32791,
};

class CMainFrame
	: public CFrameWindowImpl<CMainFrame>
	, public CCMenu<CMainFrame>
	, public CSkinWindow<CMainFrame>
	, public CUpdateUI<CMainFrame>
	, public CMessageFilter
	, public CIdleHandler
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	// Options
	CPGNPOptions m_optionsForWork;
	CPGNPOptions m_optionsForEdit;

	// UI
	CMyCommandBar m_CmdBar;
	HMENU m_hMenu;
	CMultiPaneStatusBarCtrl m_status;
	CMySplitterWindow m_vSplitter;
	CMyHorSplitterWindow m_hzDetailsSplitter, m_hzFilterSplitter;

	CMyPaneContainer m_appsPane, m_filterPane;

	CFilterView m_filter;
	CSkinScrollWnd *m_pSkinFilter;

	CExplorerView m_explorer;
	CSkinScrollWnd *m_pSkinExplorer;

	CPGNProfilerView m_view;
	CSkinScrollWnd *m_pSkinView;

	CDetailsView m_details;
	CSkinScrollWnd *m_pSkinDetails;

	CTraceReader m_traceReader;

	CAboutDlg m_aboutDlg;

	CConnectHostDlg m_connectDlg;

	COptionsDlg m_optionsDlg;

	CMyToolBar m_toolbar;
	
	CMyReBarCtrl m_myrebar;

	// Other
	bool m_bSelectionChanged, m_bOptionsWerePreviouslyDifferent;

	list<DWORD_PTR> m_itemsToSelect;	// Container used to select items "on-idle" after switching from filtered view to normal.
	
private:
	system_clock::time_point m_filterStart;		// keeps filtration start time
	volatile LONG m_prevFilterProgress, m_filterProgress;
	CriticalSection	_lock;
	CColorPalette& m_palit;
	WCHAR m_szVersion[64];

public:
	CMainFrame(CColorPalette& palit);

	BOOL CreateMainWindow(LPTSTR lpstrCmdLine, int nCmdShow);

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
			return TRUE;

		if (m_view.PreTranslateMessage(pMsg))
			return TRUE;

		if (m_explorer.PreTranslateMessage(pMsg))
			return TRUE;

		return FALSE;
	}

	virtual BOOL OnIdle();

	BEGIN_UPDATE_UI_MAP(CMainFrame)
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
		//UPDATE_ELEMENT(ID_AUTOSCROLL, UPDUI_MENUBAR)
		UPDATE_ELEMENT(ID_FILE_SAVE, UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_FILTER, UPDUI_TOOLBAR)
		//UPDATE_ELEMENT(0, UPDUI_STATUSBAR)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP(CMainFrame)
        NOTIFY_HANDLER(IDC_PROFILER_LV, LVN_GETDISPINFO, m_view.OnGetdispinfo)
		NOTIFY_HANDLER(IDC_PROFILER_LV, NM_CUSTOMDRAW, m_view.OnCustomDraw)
		NOTIFY_HANDLER(IDC_PROFILER_LV, LVN_ITEMCHANGED, OnListItemChanged)
		NOTIFY_HANDLER(IDC_PROFILER_TV, TVN_SELCHANGED, OnTreeItemChanged)
		NOTIFY_HANDLER(IDC_PROFILER_TV, NM_CUSTOMDRAW, m_explorer.OnCustomDraw)
		//MESSAGE_HANDLER(WM_CTLCOLORSCROLLBAR, OnCtlColor)

		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_PGNPROF_STATUS, OnPGNProfStatus)
		MESSAGE_HANDLER(MYMSG_START_SINGLE, OnPGNProfStartSingle)
		MESSAGE_HANDLER(MYWM_THEME_CHANGE, OnPGNProfThemeChange)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_SYSCOMMAND, OnSysCommand)
		COMMAND_ID_HANDLER(ID_CONNECTTOHOST, OnConnectToHost)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnFileSave)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(ID_CAPTURE, OnCapture)
		COMMAND_ID_HANDLER(ID_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_TRACELEVEL_SQLSTATEMENTSONLY, OnTraceLevel)
		COMMAND_ID_HANDLER(ID_TRACELEVEL_COMMENTLEVEL1, OnTraceLevel)
		COMMAND_ID_HANDLER(ID_TRACELEVEL_COMMENTLEVEL2, OnTraceLevel)
		COMMAND_ID_HANDLER(ID_TRACELEVEL_COMMENTLEVEL3, OnTraceLevel)
		COMMAND_ID_HANDLER(ID_CLEAR_LOG, OnClearLog)
		COMMAND_ID_HANDLER(ID_AUTOSCROLL, OnAutoScroll)
		COMMAND_ID_HANDLER(ID_TIMEFORMAT, OnTimeFormat)
		COMMAND_ID_HANDLER(ID_FILTER, OnFilter)
		COMMAND_ID_HANDLER(ID_VIEW_ZOOMIN, OnZoomin)
		COMMAND_ID_HANDLER(ID_VIEW_ZOOMOUT, OnZoomout)
		COMMAND_ID_HANDLER(ID_VIEW_OPTIONS, OnHighlighting)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_EDIT_PASTE, OnEditPaste)
		MESSAGE_HANDLER(MYMSG_DATA_READ, OnDataRead)
		MESSAGE_HANDLER(MYMSG_DRAGDROP_FILE, OnDragDropFile)
		MESSAGE_HANDLER(MYMSG_PROFILE_ALL_APPS, OnProfileAllApps)
		MESSAGE_HANDLER(MYMSG_SAVESETTINGS, OnSaveSettings)
		MESSAGE_HANDLER(MYMSG_HOST_CONNECTED, OnHostConnected)
		MESSAGE_HANDLER(MYMSG_UI_UPDATE, OnUIUpdate)
		CHAIN_MSG_MAP(CCMenu<CMainFrame>)
		CHAIN_MSG_MAP(CSkinWindow<CMainFrame>)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
		//CHAIN_COMMANDS_MEMBER((m_view))
		//REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	// Handlers
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);

	LRESULT OnPGNProfStatus(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnPGNProfStartSingle(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnPGNProfThemeChange(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnSysCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnDataRead(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	LRESULT OnDragDropFile(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		
	LRESULT OnProfileAllApps(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnSaveSettings(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnHostConnected(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnUIUpdate(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnConnectToHost(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnFileSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}

	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		BOOL bVisible = !::IsWindowVisible(m_hWndToolBar);
		::ShowWindow(m_hWndToolBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_TOOLBAR, bVisible);
		UpdateLayout();
		return 0;
	}

	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
		::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
		UpdateLayout();
		return 0;
	}

	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnCapture(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	
	LRESULT OnTraceLevel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnClearLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnAutoScroll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnTimeFormat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnFilter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnZoomin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnZoomout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnHighlighting(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnEditPaste(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnTreeItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);

	LRESULT OnListItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);

	void pipeCreatedCallback(int pid);
	void pipeClosedCallback(int pid);
	
	inline CColorPalette& GetPalit() { return m_palit; }

private:
	HWND CreateMyToolBar(DWORD dwStyle, CColorPalette::THEME theme);		// different implementation of CreateSimpleToolBar();
	void DisplayWindowTitle(const TCHAR* wszLogName = nullptr);
	void SetupColorTheme(CColorPalette::THEME theme, const vector<COLORREF>& qclr);
	TCHAR* GetExplorerItemText(const TCHAR* wszPrefix, HTREEITEM hItem, TCHAR* wszLogName, size_t cnt);
	void SetPaneWidths(int* arrWidths, int nPanes);
	static void SetFontFromOption(ATL::CWindow window, LONG fontHt);
	static void StoreFontSetting(ATL::CWindow window, LONG fontHt, HKEY hOptionsKey, WCHAR* setting);

	void MakeFileLoggerNode(LPCTSTR lpFilePath, bool csv = false, bool bWarnUser = false);
	void CheckAndAddProcessLoggerNode(LPCTSTR pProcessName, DWORD pid, bool bIs64bitProcess, map<CLoggerItemBase*, LOGGER_STATE>& statesMap, CLoggerItemBase& rootLogger);

	void UpdateFileSaveBtn();
	void UpdateCaptureButtonImg(const CLoggerItemBase* pLogger);
	void UpdateFilterButtonImg(const CLoggerItemBase* pLogger);
	void UpdateFilterText(const CLoggerItemBase* pLogger);

	//int FilterHostLoggerItems(CHostLoggerItem* pCurLogger);
	//int FilterProcessLoggerItems(CProcessLoggerItem* pCurLogger);

	void LoadSettings();
	void SaveSettings(DWORD flags);

	void WriteCsvLog(HANDLE hLogFile, CLoggerItemBase* pCurLogger);
	void WriteBinLog(HANDLE hLogFile, CLoggerItemBase* pCurLogger);

	void DisplayNumMessages(CLoggerItemBase* pLogger);
	void FilterProgress(LONG value);
	void Lock();
	void Unlock();

	LPCTSTR MakeIntRes(WORD id);
	void UpdateMenuIcons();
};
