/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "resource.h"
#include "MainFrm.h"
#include "ProfilerServer.h"
#include "RegUtil.h"
#include "gui\SkinMenuMgr.h"
#include "gui\SkinMenu.h"
#include "SkinScrollWnd.h"
#include "common\VersionInfo.h"
#include "3rdparty\RegUtil\RegSettings.h"
#include <algorithm>
#include <dwmapi.h>
#include <uxtheme.h>
#include "boost/program_options.hpp"
#include "boost/program_options/parsers.hpp"

extern LVCOLUMN_EX SQLLOG_COLUMNS[];
extern int SQLLOG_COLUMNS_CNT;
extern const WCHAR* TRC_TYPE_STR[];
extern const WCHAR* CMD_TYPE_STR[26];
extern const WCHAR* CURSOR_TYPE_STR[];

static LPCTSTR lpcstrFilter = L"PGNP Profiler Binary Log (*." PGL_FILE_EXT L")|*." PGL_FILE_EXT L"|PGNP Profiler CSV Log (*." CSV_FILE_EXT L")|*." CSV_FILE_EXT L"|All Files (*.*)|*.*|";

static WCHAR statusPaneBuf[4][128];
char g_svalue[64 * 1024];

CMainFrame::CMainFrame(CColorPalette& palit)
	: CSkinWindow<CMainFrame>(palit)
	, CCMenu<CMainFrame>(palit)
	, m_CmdBar(palit), m_hMenu(NULL), m_vSplitter(palit), m_hzDetailsSplitter(palit), m_hzFilterSplitter(palit)
	, m_palit(palit), m_appsPane(palit), m_filterPane(palit)
	, m_pSkinFilter(NULL), m_pSkinExplorer(NULL), m_pSkinView(NULL), m_pSkinDetails(NULL)
	, m_filter(palit), m_explorer(m_optionsForWork, palit), m_view(m_optionsForEdit, palit), m_details(palit)
	, m_bSelectionChanged(false), m_bOptionsWerePreviouslyDifferent(false)
	, m_aboutDlg(palit), m_connectDlg(m_optionsForEdit, palit), m_optionsDlg(m_optionsForEdit, palit, 2)
	, m_toolbar(palit), m_myrebar(palit), m_prevFilterProgress(0), m_filterProgress(0)
{
	_fn_MakeIntRes = std::bind(&CMainFrame::MakeIntRes, this, std::placeholders::_1);

	m_szVersion[0] = 0;
	m_filterStart = system_clock::now();	// do we need this at all?
}

static int IdMapping[][2] = {
	{ ID_FILE_OPEN, IDI_FILE_OPEN_DARK },
	{ ID_FILE_SAVE, IDI_FILE_SAVE_DARK },
	{ ID_EDIT_COPY, IDI_EDIT_COPY_DARK },
	{ ID_CAPTURE, IDI_CAPTURE_DARK },
	{ ID_PAUSE_CAPTURE, IDI_PAUSE_DARK },
	{ ID_TIMEFORMAT, IDI_TIMEFORMAT_DARK },
	{ IDI_TIMEFORMATOFF, IDI_TIMEFORMATOFF_DARK },
	{ ID_FILTER, IDI_FILTER_DARK },
	{ IDI_FILTEROFF, IDI_FILTEROFF_DARK },
	{ ID_APP_ABOUT, IDI_APP_ABOUT_DARK },
	{ 0, 0}
};

LPCTSTR CMainFrame::MakeIntRes(WORD id)
{
	ATLTRACE2(atlTraceDBProvider, 3, L"CMainFrame::MakeIntRes(%d)\n", id);

	if (ID_FILTER == id && m_filter.m_filterOn)
	{
		id = IDI_FILTEROFF;
	}
	else if (ID_AUTOSCROLL == id && !m_view.IsAutoScroll())
	{
		id = IDI_AUTOSCROLLOFF;
	}
	else if (ID_TIMEFORMAT == id && m_view.IsRelativeTime())
	{
		id = IDI_TIMEFORMATOFF;
	}
	else if (ID_CAPTURE == id && m_explorer.GetCurrentLoggerPtr() && LS_RUN == m_explorer.GetCurrentLoggerPtr()->GetState())
	{
		id = ID_PAUSE_CAPTURE;
	}

	const int icoIdx = CColorPalette::THEME_DARK == m_palit.m_theme ? 1 : 0;
	for (auto pIdMap = &IdMapping[0]; **pIdMap; pIdMap++)
	{
		if (id == **pIdMap)
		{
			id = (*pIdMap)[icoIdx];
			break;
		}
	}

	return MAKEINTRESOURCE(id);
}

BOOL CMainFrame::CreateMainWindow(LPTSTR lpstrCmdLine, int nCmdShow)
{
	CWindowSettings ws;
	if (ws.Load(REG_PATH, L"MainFrm"))
	{
		if (CreateEx(0, ws.m_WindowPlacement.rcNormalPosition, 0, 0, lpstrCmdLine) == NULL)
		{
			ATLTRACE(_T("Main window creation failed!\n"));
			return FALSE;
		}
		ws.ApplyTo(*this, ws.m_WindowPlacement.showCmd);

		SetWinState(ws.m_WindowPlacement.showCmd == SW_SHOWMAXIMIZED ? CSkinWindow::MAXIMIZED : CSkinWindow::NORMAL, CSkinWindow::NORMAL, ws.m_WindowPlacement.rcNormalPosition);
	}
	else
	{
		if (CreateEx(0, 0, 0, 0, lpstrCmdLine) == NULL)
		{
			ATLTRACE(_T("Main window creation failed!\n"));
			return FALSE;
		}
		ShowWindow(nCmdShow);
	}

	// Restore the vertical splitter position if it's saved in the registry
	CSplitterSettings vSet;
	if (vSet.Load(REG_PATH, _T("V")))
		vSet.ApplyTo(m_vSplitter);

	// Restore the horizontal splitter position if it's saved in the registry
	CSplitterSettings hSet;
	if (hSet.Load(REG_PATH, _T("H")))
		hSet.ApplyTo(m_hzDetailsSplitter);

	// Restore the horizontal splitter position if it's saved in the registry
	if (hSet.Load(REG_PATH, _T("HF")))
		hSet.ApplyTo(m_hzFilterSplitter);

	return TRUE;
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	LoadSettings();
	m_palit.Set((CColorPalette::THEME)m_optionsForWork.m_dwColorTheme, CColorPalette::THEME_DARK == (CColorPalette::THEME)m_optionsForWork.m_dwColorTheme ?
		m_optionsForWork.m_qclrDark : m_optionsForWork.m_qclrLight);

	// create command bar
	HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);

	m_CmdBar.AttachMenu(GetMenu());

	if (m_optionsForWork.m_dwConnectToHostEnabled)
	{
		TCHAR* szItem = L"Connect To Host...";

		MENUITEMINFO mii = {
			sizeof(MENUITEMINFO),
			MIIM_ID | MIIM_STRING | MIIM_DATA,
			MFT_STRING,
			MFS_ENABLED,
			ID_CONNECTTOHOST,
			NULL, NULL, NULL, NULL,
			szItem,
			wcslen(szItem),
			0
		};

		HMENU hSubMenu = ::GetSubMenu(m_CmdBar.m_hMenu, 0);
		::InsertMenuItem(hSubMenu, 0, TRUE, &mii);
	}

	// remove old menu
	SetMenu(NULL);

	HWND hWndToolBar = CreateMyToolBar(ATL_SIMPLE_TOOLBAR_PANE_STYLE, (CColorPalette::THEME)m_optionsForWork.m_dwColorTheme);
	m_toolbar.SubclassWindow(hWndToolBar);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndCmdBar, NULL, FALSE, 200);
	AddSimpleReBarBand(hWndToolBar);
	
	m_myrebar.SubclassWindow(m_hWndToolBar);

	UIAddToolBar(hWndToolBar);
	//UISetCheck(ID_VIEW_TOOLBAR, 1);
	//UISetCheck(ID_VIEW_STATUS_BAR, 1);

	CreateSimpleStatusBar();
	m_status.SubclassWindow(m_hWndStatusBar);

	int arrPanes[] = { ID_DEFAULT_PANE, ID_STBLEVEL, ID_STBERROR, ID_STBINFORMATION };
    m_status.SetPanes(arrPanes, _countof(arrPanes), false);

    int arrWidths[] = { 0, 110, 110, 110 };
    SetPaneWidths(arrWidths, _countof(arrWidths));

	m_status.SetPaneIcon(ID_STBERROR, AtlLoadIconImage(ID_STBERROR, LR_DEFAULTCOLOR, 16, 16));
	m_status.SetPaneIcon(ID_STBINFORMATION, AtlLoadIconImage(ID_STBINFORMATION, LR_DEFAULTCOLOR, 16, 16));
	SetWindowTheme(m_status.m_hWnd, L" ", L" ");	// turn off theming for statusbar, otherwise SetBkgColor will not have effect

	// client rect for vertical splitter
	RECT rcClient;
	GetClientRect(&rcClient);

	// create the vertical splitter
	m_hWndClient = m_vSplitter.Create(m_hWnd, rcClient);
	m_vSplitter.SetSplitterExtendedStyle(0);
	m_vSplitter.m_cxyMin = 35; // minimum size

	m_vSplitter.SetSplitterPos(200);	// from left
	m_vSplitter.m_bFullDrag = false; // ghost bar enabled

	// client rect for horizontal splitter
	CRect rcHorz;
	m_vSplitter.GetSplitterPaneRect(SPLIT_PANE_RIGHT, &rcHorz);

	//====================================================================================
	// create the horizontal splitters. Note that vSplitter is parent of m_hzDetailsSplitter and m_hzFilterSplitter.
	m_hzDetailsSplitter.Create(m_vSplitter.m_hWnd, rcHorz, 0, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_hzDetailsSplitter.m_cxyMin = 35; // minimum size
	m_hzDetailsSplitter.SetSplitterPos(rcHorz.Height()*3/4);
	m_hzDetailsSplitter.m_bFullDrag = false; // ghost bar enabled

	m_hzFilterSplitter.Create(m_vSplitter.m_hWnd, rcHorz, 0, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_hzFilterSplitter.m_cxyMin = 35; // minimum size
	m_hzFilterSplitter.SetSplitterPos(rcHorz.Height()*3/4);
	m_hzFilterSplitter.m_bFullDrag = false; // ghost bar enabled
	//====================================================================================

	CImageListCtrl loggersImages;		// The images
	loggersImages.CreateFromImage(CColorPalette::THEME_DARK == (CColorPalette::THEME)m_optionsForWork.m_dwColorTheme ? IDB_LOGGERS_DARK : IDB_LOGGERS, 16, 0, CLR_DEFAULT, IMAGE_BITMAP, LR_CREATEDIBSECTION);

	m_filterPane.Create(m_hzFilterSplitter, L"Filter Expression");
	m_filter.Create(m_filterPane, rcDefault, NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS | WS_CLIPCHILDREN /*| WS_VSCROLL | ES_AUTOVSCROLL*/ | ES_MULTILINE, 0/*WS_EX_CLIENTEDGE*/);
	//m_filterPane.SetClient(m_filter);
	m_filterPane.SetPaneContainerExtendedStyle(PANECNT_NOCLOSEBUTTON);

	m_appsPane.Create(m_hzFilterSplitter, L"Applications/Logs");
	m_explorer.Create(m_appsPane, rcDefault, NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0/*WS_EX_CLIENTEDGE*/, IDC_PROFILER_TV);
	m_explorer.SetImageList(loggersImages.Detach(), TVSIL_NORMAL);
	//m_appsPane.SetClient(m_explorer);
	m_appsPane.SetPaneContainerExtendedStyle(PANECNT_NOCLOSEBUTTON);

	m_hzFilterSplitter.SetSplitterPanes(m_filterPane, m_appsPane);

	m_view.Create(m_hzDetailsSplitter, rcDefault, NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | LVS_OWNERDATA, 0/*WS_EX_CLIENTEDGE*/, IDC_PROFILER_LV, (LPVOID)&m_optionsForWork);

	// set extended properties on the view to eliminate flickering when new messages added
	LRESULT viewStyles = m_view.SendMessage(LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
    m_view.SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, viewStyles | LVS_EX_DOUBLEBUFFER /*| LVS_EX_BORDERSELECT*/);

	m_details.Create(m_hzDetailsSplitter, rcDefault, NULL, WS_BORDER | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY, 0/*WS_EX_CLIENTEDGE*/);

	m_view.SetMainWnd(m_hWnd);
	m_details.SetMainWnd(m_hWnd);
	m_explorer.SetMainWnd(m_hWnd);
	m_filter.SetMainWnd(m_hWnd);

	//if (m_optionsForWork.m_dwColorTheme != CColorPalette::THEME_DEFAULT)
	SetupColorTheme((CColorPalette::THEME)m_optionsForWork.m_dwColorTheme, CColorPalette::THEME_DARK == (CColorPalette::THEME)m_optionsForWork.m_dwColorTheme ?
		m_optionsForWork.m_qclrDark : m_optionsForWork.m_qclrLight);

	m_pSkinView = SkinWndScroll(m_view, m_palit);
	m_pSkinDetails = SkinWndScroll(m_details, m_palit);
	m_pSkinExplorer = SkinWndScroll(m_explorer, m_palit);
	m_pSkinFilter = SkinWndScroll(m_filter, m_palit);
	
	m_appsPane.SetClient(m_pSkinExplorer->m_hWnd/*m_explorer*/);
	m_filterPane.SetClient(m_pSkinFilter->m_hWnd/*m_filter*/);

	// enable drag&drop
	m_view.RegisterDropHandler(TRUE);
	m_details.RegisterDropHandler(TRUE);
	m_explorer.RegisterDropHandler(TRUE);
	m_filter.RegisterDropHandler(TRUE);

	SetFontFromOption(m_view, m_optionsForWork.m_panel1FontHt);
	SetFontFromOption(m_details, m_optionsForWork.m_DetailsFontHt);
	SetFontFromOption(m_explorer, m_optionsForWork.m_ExplorerFontHt);
	SetFontFromOption(m_filter, m_optionsForWork.m_FilterFontHt);

	// add the main view to the top pane (0) of horizontal splitter
	m_hzDetailsSplitter.SetSplitterPanes(m_pSkinView->m_hWnd/*m_view*/, m_pSkinDetails->m_hWnd/*m_details*/);

	m_vSplitter.SetSplitterPanes(m_hzFilterSplitter, m_hzDetailsSplitter);

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	_ASSERTE((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	_ASSERTE(IDM_ABOUTBOX < 0xF000);

	CMenu SysMenu = GetSystemMenu(FALSE);
	if(::IsMenu(SysMenu))
	{
		TCHAR szAboutMenu[256];
		if(::LoadString(_Module.GetResourceInstance(), IDS_ABOUTBOX, szAboutMenu, 255) > 0)
		{
			SysMenu.AppendMenu(MF_SEPARATOR);
			SysMenu.AppendMenu(MF_STRING, IDM_ABOUTBOX, szAboutMenu);
		}
	}
	SysMenu.Detach();

	CSkinMenuMgr::Initialize(SKMS_FLAT, 8, FALSE);

	// Disable non-clientarea rendering on the window.
	DWMNCRENDERINGPOLICY ncrp = DWMNCRP_ENABLED; // DWMNCRP_DISABLED to toggle back
	HRESULT hr = DwmSetWindowAttribute(m_hWnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));
	ATLASSERT(SUCCEEDED(hr));

	VersionInfo verInfo(L"PGNProfiler.exe");
	swprintf(m_szVersion, L"%d.%d.%d.%d", verInfo.majorVersion(), verInfo.minorVersion(), verInfo.subBuild(), verInfo.build());

	DisplayWindowTitle();

	// start trace reader
	m_traceReader.Start(&m_explorer.GetRootLogger());

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	// check if command string contains files to open
	CREATESTRUCT* pCreateStruct = (CREATESTRUCT*)lParam;
	if (pCreateStruct->lpCreateParams != NULL)
	{
		try
		{
			const wstring ext_pgl(L".pgl"), ext_csv(L".csv");
			for (auto arg : boost::program_options::split_winmain((LPTSTR)pCreateStruct->lpCreateParams))
			{
				if (arg.length() > 4 && (std::equal(ext_pgl.rbegin(), ext_pgl.rend(), arg.rbegin()) || std::equal(ext_csv.rbegin(), ext_csv.rend(), arg.rbegin())))
				{
					LPTSTR szBuffCopy = new TCHAR[arg.length() + 1];
					_tcscpy(szBuffCopy, arg.c_str());
					PostMessage(MYMSG_DRAGDROP_FILE, (WPARAM)szBuffCopy, (LPARAM)0);
				}
			}
		}
		catch (...)
		{
			// ignore any exceptions during parameters handling
		}
	}

	PostMessage(WM_PGNPROF_STATUS);

	if (m_optionsForWork.m_dwProfileAllApps)
		PostMessage(MYMSG_PROFILE_ALL_APPS, 0, 0);
	
	//SetTimer(REFRESH_APPS_TIMER, 10000, 0);

	return 0;
}

void CMainFrame::DisplayWindowTitle(const TCHAR* wszLogName)
{
	WCHAR appTitle[512] = { 0 };
	
	if (wszLogName != nullptr && *wszLogName != 0)
	{
		wcscat(appTitle, wszLogName);
		wcscat(appTitle, L" - ");
	}

	swprintf(&appTitle[wcslen(appTitle)], PGNPTITLE_FULL, m_szVersion);
	SetWindowText(appTitle);
	PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(20, 5));	// without this line the title does not redraw itself
}

void CMainFrame::SetupColorTheme(CColorPalette::THEME theme, const vector<COLORREF>& qclr)
{
	if (m_palit.Set(theme, qclr))
	{
		//// SetMenuInfo with MIM_BACKGROUND doesn't work because the theme is enabled; SetWindowTheme(hwndMain, L"", L"")
		MENUINFO mi = { sizeof(MENUINFO), MIM_BACKGROUND, 0, 0, HBRUSH(m_palit.m_brBackGndSel) };
		::SetMenuInfo(m_hMenu, &mi);

		CImageListCtrl loggersImages;		// The images
		loggersImages.CreateFromImage(CColorPalette::THEME_DARK == theme ? IDB_LOGGERS_DARK : IDB_LOGGERS, 16, 0, CLR_DEFAULT, IMAGE_BITMAP, LR_CREATEDIBSECTION);

		m_explorer.SetImageList(loggersImages.Detach(), TVSIL_NORMAL);
	}

	// since some controls do not send WM_CTLCOLOR* their color must be set here explicitly
	m_explorer.SetBkColor(m_palit.CrBackGnd);
	m_explorer.SetLineColor(m_palit.CrText);
	m_explorer.SetTextColor(m_palit.CrText);

	m_view.SetBkColor(m_palit.CrBackGnd);

	if (m_pSkinView != NULL)
	{
		m_pSkinView->SetBmpScroll(m_palit.m_hBmpScrollBarActive);
		m_pSkinDetails->SetBmpScroll(m_palit.m_hBmpScrollBarActive);
		m_pSkinExplorer->SetBmpScroll(m_palit.m_hBmpScrollBarActive);
		m_pSkinFilter->SetBmpScroll(m_palit.m_hBmpScrollBarActive);
	}
	
	RedrawWindow(NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_UPDATENOW);

	m_status.SetBkColor(m_palit.CrBackGndAlt);
	m_status.RedrawWindow(NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_UPDATENOW);
}

TCHAR* CMainFrame::GetExplorerItemText(const TCHAR* wszPrefix, HTREEITEM hItem, TCHAR* wszLogName, size_t cnt)
{
	if (wszPrefix && *wszPrefix)
	{
		wcscpy(wszLogName, wszPrefix);
		wcscat(wszLogName, L"\\");
	}
	else
	{
		*wszLogName = 0;
	}

	m_explorer.GetItemText(hItem, &wszLogName[wcslen(wszLogName)], cnt);

	return wszLogName;
}

// Save window position and panels sizes.
// Stop trace readers, perform cleanup.
LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// stop trace reader
	m_traceReader.StopAll();

	// cancel any filtration if in progress
	m_explorer.CancelFilters();

	// stop task thread
	CMyTaskExecutor::Instance().Terminate();

	CSplitterSettings vSet;
	vSet.GetFrom(m_vSplitter);
	vSet.Save(REG_PATH, _T("V"));

	CSplitterSettings hSet;
	hSet.GetFrom(m_hzDetailsSplitter);
	hSet.Save(REG_PATH, _T("H"));

	hSet.GetFrom(m_hzFilterSplitter);
	hSet.Save(REG_PATH, _T("HF"));

	CWindowSettings ws;
	ws.GetFrom(*this);
	ws.Save(REG_PATH, L"MainFrm");

	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnConnectToHost(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_explorer.GetHostsCount() > MAX_CONNECTED_HOSTS)
	{
		MessageBox(L"Maximum number of remote hosts are already connected.\r\n\r\nPlease close a connection to be able to open new connection.", L"Warning", MB_OK | MB_ICONEXCLAMATION);
	}
	else
	{
		m_connectDlg.Create(m_hWnd);
		m_connectDlg.ShowWindow(SW_SHOWNORMAL);
	}

	return 0;
}

LRESULT CMainFrame::OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CMyFileDialog dlg(TRUE, PGL_FILE_EXT, _T(""), OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_EXPLORER, lpcstrFilter);
	if (dlg.DoModal() == IDOK)
	{
		MakeFileLoggerNode(dlg.m_ofn.lpstrFile, dlg.m_ofn.nFilterIndex == 2, true);
	}

	return 0;
}

LRESULT CMainFrame::OnFileSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	USES_XMSGBOXPARAMS
	CLoggerItemBase* pCurLogger = m_explorer.GetCurrentLoggerPtr();

	if (pCurLogger && pCurLogger->GetNumMessages() != 0)
	{
		// Build file name as logger name with replaced parenthesis info with current date-time:
		//    localhost (127.0.0.1)  --> localhost_20090909_13:25:54.pgl
		//    ADOExplorer.exe (3456) --> ADOExplorer.exe_20090909_13:27:30.pgl
		WCHAR name[1024];
		wcscpy(name, pCurLogger->GetName());
		WCHAR* p = wcschr(name, L'(');
		if (p == 0)
			p = &name[wcslen(name)-1];

		if (p[-1] == L' ') --p;

		time_t ltime = time(0);
		struct tm *today = localtime(&ltime);
		wcsftime(p, name+1024-p, L"_%Y%m%d_%H%M%S", today);

		// Ask user for the file destination.
		CMyFileDialog dlg(FALSE, PGL_FILE_EXT, name, OFN_EXPLORER, lpcstrFilter);
		//dlg.m_ofn.lpstrInitialDir = sPath;
		if (dlg.DoModal() == IDOK)
		{
			// check if file exists
			if (GetFileAttributes(dlg.m_ofn.lpstrFile) != INVALID_FILE_ATTRIBUTES)
			{
				WCHAR message[1024];
				swprintf(message, L"File \"%s\" already exists. Do you want to overwrite it?", dlg.m_ofn.lpstrFile);
				if (XMessageBox(m_hWnd, message, PGNPTITLE, MB_YESNO | MB_ICONQUESTION, &__xmb) != IDYES)
					return 0;
			}

			// open file for writing
			HANDLE hLogFile = CreateFile(dlg.m_ofn.lpstrFile, GENERIC_READ|GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
			if (INVALID_HANDLE_VALUE == hLogFile)
			{
				WCHAR message[1024];
				swprintf(message, L"Could not create file \"%s\". Error %d.", dlg.m_ofn.lpstrFile, GetLastError());
				XMessageBox(m_hWnd, message, PGNPTITLE, MB_OK | MB_ICONERROR, &__xmb);
				return 0;
			}

			if (dlg.m_ofn.nFilterIndex == 2)
			{
				WriteCsvLog(hLogFile, pCurLogger);
			}
			else
			{
				WriteBinLog(hLogFile, pCurLogger);
			}

			CloseHandle(hLogFile);
		}
	}
	else
	{
		XMessageBox(m_hWnd, L"No messages to save.", PGNPTITLE, MB_OK | MB_ICONEXCLAMATION, &__xmb);
	}
	return 0;
}

void CMainFrame::MakeFileLoggerNode(LPCTSTR lpFilePath, bool csv, bool bWarnUser)
{
	USES_XMSGBOXPARAMS
	
	// Create "FileLogger" node
	LPCTSTR lpFileName = wcsrchr(lpFilePath, L'\\');
	if (lpFileName != 0)
		lpFileName++;
	else
		lpFileName = lpFilePath;

	if (m_explorer.IsLogFileOpen(lpFileName))
		return;

	CLoggerItemBase* pLogFile;

	if (csv)
	{
		pLogFile = new CCsvFileLoggerItem(lpFileName, lpFilePath);
	}
	else
	{
		pLogFile = new CFileLoggerItem(lpFileName, lpFilePath);
	}

	int err = pLogFile->OpenLogFile();
	switch (err)
	{
	case 0:	// success!
		m_explorer.AddLogFile(pLogFile);
		::EnableWindow(m_filter, pLogFile != 0);
		m_explorer.SetCurrentLoggerPtr(pLogFile);
		UpdateFileSaveBtn();
		UpdateCaptureButtonImg(pLogFile);
		UpdateFilterButtonImg(pLogFile);
		UpdateFilterText(pLogFile);

		// trigger details view update
		m_bSelectionChanged = true;

		PostMessage(WM_PGNPROF_STATUS);

		if (0 == pLogFile->GetNumMessages() && bWarnUser)
		{
			__xmb.nTimeoutSeconds = 10;
			XMessageBox(m_hWnd, L"No messages read.", PGNPTITLE, MB_OK | MB_ICONINFORMATION, &__xmb);
		}
		break;

	case -1:
		if (bWarnUser)
		{
			XMessageBox(m_hWnd, L"Invalid file format.", PGNPTITLE, MB_OK | MB_ICONERROR, &__xmb);
		}
		break;

	case -2:
		if (bWarnUser)
		{
			XMessageBox(m_hWnd, L"Could not map the file content.", PGNPTITLE, MB_OK | MB_ICONERROR, &__xmb);
		}
		break;

	default:
		if (bWarnUser)
		{
			WCHAR message[1024];
			swprintf(message, L"Could not open file \"%s\". Error %d.", lpFileName, err);
			XMessageBox(m_hWnd, message, PGNPTITLE, MB_OK | MB_ICONERROR, &__xmb);
		}
		break;
	}
}

static void AppendCsvSQL(string& csvmsg, const char* pSQL)
{
	csvmsg += '"';
	for (; *pSQL; pSQL++)
	{
		csvmsg += *pSQL;
		if (*pSQL == '"')
			csvmsg += '"';		// append extra quote as an escape
	}
	csvmsg += '"';
}

void CMainFrame::WriteCsvLog(HANDLE hLogFile, CLoggerItemBase* pCurLogger)
{
	unsigned nRows = pCurLogger->GetNumMessages();

	// write csv header
	DWORD numBytes;
	WriteFile(hLogFile, CSV_HEADER, CSV_HEADER_SIZE, &numBytes, 0);

	// write messages in CSV format
	string csvmsg;
	CHAR buffer[_CVTBUFSIZE];
	WCHAR wbuffer[_CVTBUFSIZE];

	LPCSTR nameA = pCurLogger->GetNameA();
	map<DWORD, wstring>* pMapLogger2Appname = pCurLogger->IsROmode() ? pCurLogger->GetLogger2AppnameMap()  : nullptr;

	for (unsigned i=0; i < nRows; i++)
	{
		pCurLogger->Lock();	// prevent accessing invalidated m_logStart due to remapping in CTraceReader::HandleProcessLoggerOverlappedResult

		const BYTE* baseAddr = pCurLogger->GetMessageData(i);
		TRC_TIME& timeStamp = *(TRC_TIME*)(baseAddr + 8);	// see CProfSQLmsg or CProfERRORmsg ctors

		csvmsg.clear();

		// AbsTime
		csvmsg += '"';
		PrintAbsTimeStamp(wbuffer, timeStamp);
		csvmsg += CW2A(wbuffer);
		csvmsg += '"';
		csvmsg += ',';

		if (TRC_ERROR == (TRC_TYPE)baseAddr[4])
		{
			CProfERRORmsg profmsg((BYTE*)baseAddr, true);

			// SQLType
			csvmsg += CW2A(TRC_TYPE_STR[profmsg.TrcType]);
			csvmsg += ',';

			// ClientSQL
			AppendCsvSQL(csvmsg, profmsg.ClientSQL.TextPtr);
			csvmsg += ',';

			// Error text
			AppendCsvSQL(csvmsg, profmsg.ErrorText.TextPtr);
			csvmsg.append(",,,,,,");

			// Database
			csvmsg.append(profmsg.Database.TextPtr);
			csvmsg += ',';

			// UserName
			csvmsg.append(profmsg.UserName.TextPtr);
			csvmsg += ',';

			// PID
			DWORD pid = 0;
			if (!pCurLogger->IsROmode())
			{
				CProcessLoggerItem* pLogger = (CProcessLoggerItem*) CLoggerTracker::Instance().GetLogger(*(USHORT*)baseAddr);
				if (pLogger != nullptr)
				{
					pid = pLogger->GetPID();
					nameA = pLogger->GetNameA();
				}
			}
			csvmsg.append(_itoa(pid, buffer, 10));
			csvmsg += ',';

			// Session
			csvmsg.append(_itoa(profmsg.SessionId, buffer, 10));
			csvmsg += ',';

			// Command
			csvmsg.append(_itoa(profmsg.CommandId, buffer, 10));
			csvmsg += ',';

			// Cursor Mode
			csvmsg += CW2A(CURSOR_TYPE_STR[profmsg.Cursor]);
			csvmsg += ',';

			// Application Name
			if (pMapLogger2Appname)
			{
				// message from a log file
				map<DWORD, wstring>::const_iterator it = pMapLogger2Appname->find((DWORD)*(CLoggerItemBase**)baseAddr);
				if (it != pMapLogger2Appname->end())
					csvmsg.append(CW2A(it->second.c_str()));
			}
			else
			{
				csvmsg.append(nameA);
			}

			csvmsg += ',';
		}
		else
		{
			CProfSQLmsg profmsg((BYTE*)baseAddr, true);
			
			// SQLType
			csvmsg += CW2A(TRC_TYPE_STR[profmsg.TrcType]);
			csvmsg += ',';

			// ClientSQL
			AppendCsvSQL(csvmsg, profmsg.ClientSQL.TextPtr);
			csvmsg += ',';

			// Executed SQL
			AppendCsvSQL(csvmsg, profmsg.ExecutedSQL.TextPtr);
			csvmsg += ',';

			// Parse,Prepare,Execute,GetRows,Rows
			sprintf(buffer, "%1.3lf,%1.3lf,%1.3lf,%1.3lf,%d,",
				(double)profmsg.ParseTime * 1000 / g_qpcFreq,
				(double)profmsg.PrepareTime * 1000 / g_qpcFreq,
				(double)profmsg.ExecTime * 1000 / g_qpcFreq,
				(double)profmsg.GetdataTime * 1000 / g_qpcFreq,
				profmsg.NumRows);
			csvmsg.append(buffer);

			// Database
			csvmsg.append(profmsg.Database.TextPtr);
			csvmsg += ',';

			// UserName
			csvmsg.append(profmsg.UserName.TextPtr);
			csvmsg += ',';

			// PID
			DWORD pid = 0;
			if (!pCurLogger->IsROmode())
			{
				CProcessLoggerItem* pLogger = (CProcessLoggerItem*) CLoggerTracker::Instance().GetLogger(*(USHORT*)baseAddr);
				if (pLogger != nullptr)
				{
					pid = pLogger->GetPID();
					nameA = pLogger->GetNameA();
				}
			}
			csvmsg.append(_itoa(pid, buffer, 10));
			csvmsg += ',';

			// Session
			csvmsg.append(_itoa(profmsg.SessionId, buffer, 10));
			csvmsg += ',';

			// Command
			csvmsg.append(_itoa(profmsg.CommandId, buffer, 10));
			csvmsg += ',';

			// Cursor Mode
			csvmsg += CW2A(CURSOR_TYPE_STR[profmsg.Cursor]);
			csvmsg += ',';

			// Application Name
			if (pMapLogger2Appname)
			{
				// message from a log file
				map<DWORD, wstring>::const_iterator it = pMapLogger2Appname->find((DWORD)*(CLoggerItemBase**)baseAddr);
				if (it != pMapLogger2Appname->end())
					csvmsg.append(CW2A(it->second.c_str()));
			}
			else
			{
				csvmsg.append(nameA);
			}
			csvmsg += ',';

			// Command Type
			if (profmsg.CmdType+1 >= sizeof(CMD_TYPE_STR)/sizeof(CMD_TYPE_STR[0]))
				csvmsg += CW2A(CMD_TYPE_STR[0]);
			else
				csvmsg += CW2A(CMD_TYPE_STR[profmsg.CmdType]);
		}

		csvmsg.append("\r\n");
		WriteFile(hLogFile, csvmsg.c_str(), csvmsg.length(), &numBytes, 0);

		pCurLogger->Unlock();
	}
}

void CMainFrame::WriteBinLog(HANDLE hLogFile, CLoggerItemBase* pCurLogger)
{
	// write header
	DWORD numBytes;
	WriteFile(hLogFile, PGL_HEADER, PGL_HEADER_SIZE, &numBytes, 0);

	unsigned n = pCurLogger->GetNumMessages();
	unsigned* pNumbers = (unsigned*)calloc(n + 2, sizeof(int));
	pNumbers[0] = n;
	pNumbers[1] = pCurLogger->GetNumErrors();
	WriteFile(hLogFile, pNumbers, (n + 2) * sizeof(int), &numBytes, 0);

	int zero = 0;
	map<DWORD, string> mapLogger2Appname;

	// write logged messages
	for (unsigned i=0; i < n; i++)
	{
		pCurLogger->Lock();	// prevent accessing invalidated m_logStart due to remapping in CTraceReader::HandleProcessLoggerOverlappedResult

		const BYTE* baseAddr = pCurLogger->GetMessageData(i);
		unsigned msgLen;
		if (TRC_ERROR == (TRC_TYPE)baseAddr[4])
		{
			CProfERRORmsg profmsg((BYTE*)baseAddr, true);
			msgLen = profmsg.GetLength();
		}
		else
		{
			CProfSQLmsg profmsg((BYTE*)baseAddr, true);
			msgLen = profmsg.GetLength();
		}
		WriteFile(hLogFile, baseAddr, msgLen, &numBytes, 0);

		DWORD dwLoggerID = *(DWORD*)baseAddr;
		map<DWORD, string>::const_iterator it = mapLogger2Appname.find(dwLoggerID);
		if (it == mapLogger2Appname.end())
		{
			CLoggerItemBase* pLoggerN = (CLoggerItemBase*)CLoggerTracker::Instance().GetLogger(*(USHORT*)baseAddr);
			mapLogger2Appname.insert(std::pair<DWORD, string>(dwLoggerID, pLoggerN->GetNameA()));
		}

		unsigned alignment = ((msgLen + 3) & ~3) - msgLen;
		if (alignment > 0)
			WriteFile(hLogFile, &zero, alignment, &numBytes, 0);
		pNumbers[i+2] = msgLen + alignment;

		pCurLogger->Unlock();
	}

	// write map that will allow retrieving app names
	for (map<DWORD, string>::const_iterator it=mapLogger2Appname.begin(); it != mapLogger2Appname.end(); it++)
	{
		WriteFile(hLogFile, &it->first, 4, &numBytes, 0);
		WriteFile(hLogFile, it->second.c_str(), it->second.length() + 1, &numBytes, 0);
	}

	// finish
	DWORD dwSize = 12 + 8 + n*sizeof(int);		//::GetFileSize(hLogFile, 0);
	HANDLE hMapping = ::CreateFileMapping(hLogFile, 0, PAGE_READWRITE, 0, dwSize, 0);
	BOOL err = (hMapping == 0);
	if (!err)
	{
		LPVOID lpBase = ::MapViewOfFile(hMapping, FILE_MAP_READ|FILE_MAP_WRITE, 0, 0, 0);
		err = (lpBase == 0);
		if (!err)
		{
			unsigned* pDest = (unsigned*)((BYTE*)lpBase + 12 + 8);
			memcpy(pDest, &pNumbers[2], n*sizeof(int));
			UnmapViewOfFile(lpBase);
		}
		CloseHandle(hMapping);
	}

	free(pNumbers);
}

// If IsWow64Process() returns true, the process is 32-bit running on a 64-bit OS.
// If IsWow64Process() returns false (or does not exist), then the process is 32-bit on a 32-bit OS, and 64-bit on a 64-bit OS. Get(Native)SystemInfo() tells if the OS itself is 32-bit or 64-bit.
typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

BOOL Is64bitProcess(HANDLE hProcess)
{
	BOOL f64 = FALSE;
	LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(_T("kernel32")), "IsWow64Process");
	if (NULL != fnIsWow64Process && !fnIsWow64Process(hProcess, &f64))
	{
		SYSTEM_INFO si;
		GetNativeSystemInfo(&si);
		return PROCESSOR_ARCHITECTURE_AMD64 == si.wProcessorArchitecture;
	}

	return !f64;
}

void CMainFrame::CheckAndAddProcessLoggerNode(LPCTSTR pProcessName, DWORD pid, bool bIs64bitProcess, map<CLoggerItemBase*, LOGGER_STATE>& statesMap, CLoggerItemBase& rootLogger)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::CheckAndAddProcessLoggerNode('%s', root='%s')\n", pProcessName, rootLogger.GetName());

	auto processes = rootLogger.GetChildren();

	// find process
	CProcessLoggerItem* pProcess = 0;
	for (auto it = processes.cbegin(); it != processes.cend(); it++)
	{
		wstring name((*it)->GetName());
		if (name == pProcessName)
		{
			pProcess = (CProcessLoggerItem*)*it;
			auto statesIt = statesMap.find(pProcess);
			LOGGER_STATE state = statesIt != statesMap.end() ? statesIt->second : LS_PAUSED;
			if (LS_TERMINATED == state)
			{
				state = (*it)->GetParent() != nullptr ? (*it)->GetParent()->GetState() : LS_PAUSED;	// the state cannot be terminated at this point because we were able to find the process! Set same state as parent.
				if (LS_RUN == state)
				{
					pProcess->SetState(LS_PAUSED);
					CaptureToggle(pProcess, m_traceReader.GetUnlimitedWait());
				}
			}

			pProcess->SetState(state);
			break;
		}
	}

	// if process not found create new (just started)
	if (pProcess == 0 && wcsnicmp(L"PGNProfiler.exe", pProcessName, 15) != 0 &&	// do not add PGNProfiler.exe
		wcsnicmp(L"PGNPUpdate.exe", pProcessName, 14) != 0)					// do not add PGNPUpdate.exe
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::CheckAndAddProcessLoggerNode(): open log file\n");

		pProcess = new CProcessLoggerItem(pProcessName, &rootLogger, LS_PAUSED, m_optionsForWork.m_llMaxLogFileSize, bIs64bitProcess);
		pProcess->SetPID(pid);
		pProcess->SetMainWindow(m_hWnd);
		if (pProcess->OpenLogFile(m_optionsForWork.m_dwDeleteWorkFiles) == 0)
		{
			rootLogger.AddChild(pProcess);

			LOGGER_STATE rootState = rootLogger.GetState();
			if (rootState == LS_RUN)
			{
				CaptureToggle(pProcess, m_traceReader.GetUnlimitedWait());
			}

			if (rootLogger.GetTraceLevel() != 0)
			{
				pProcess->SetTraceLevel(rootLogger.GetTraceLevel());
			}

			if (m_optionsForEdit.m_dwParamFormat != PRMFMT_DEFAULT)
			{
				pProcess->SetParamFormat(m_optionsForEdit.m_dwParamFormat);
			}
		}
	}
}

// Handles WM_PGNPROF_STATUS broadcast message.
// Refreshes tree view with list of running processes.
// wParam - 0 (STATUS_STARTUP) means: app started, and the RootLogger must be populated;
//        - 1 (STATUS_SHUTDOWN) means: a local process closed;
//        - other (non-zero multiple of 4) means: handle to a remote host pipe.
// lParam - if wParam={0,1}, contains PID;
//        - otherwise, pointer to RemMsg (message received from remote host).
LRESULT CMainFrame::OnPGNProfStatus(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bool bLocal = PGNPROF_STATUS_STARTUP == wParam || PGNPROF_STATUS_SHUTDOWN == wParam;
	CLoggerItemBase& rootLogger = bLocal ? m_explorer.GetRootLogger() : *m_explorer.FindHost((HANDLE)wParam);
	ATLASSERT(&rootLogger != NULL);
	CLoggerItemBase* pCurLogger = m_explorer.GetCurrentLoggerPtr();

	// remember all loggers states, then mark all processes as terminated
	map<CLoggerItemBase*, LOGGER_STATE> statesMap;
	auto& processes = rootLogger.GetChildren();
	for (auto it=processes.cbegin(); it != processes.cend(); it++)
	{
		statesMap.insert(std::pair<CLoggerItemBase*, LOGGER_STATE>(*it, (*it)->GetState()));
		(*it)->SetState(LS_TERMINATED);
	}

	if (bLocal)
	{
#pragma region LOCAL_HOST
		DWORD appList[5000];
		int entries = _countof(appList);
		GetAppList(appList, &entries);

		for (int i = 0; i < entries; i++)
		{
			// Get the process name.
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION /*| PROCESS_VM_READ*/, FALSE, appList[i]);
			if (hProcess == 0)
			{
				DWORD err = GetLastError();
				continue;
			}
			TCHAR buf[16], szProcessName[MAX_PATH * 2], *pProcessName;
			if (GetProcessImageFileName(hProcess, szProcessName, MAX_PATH * 2) != 0)
			{
				pProcessName = wcsrchr(szProcessName, '\\') + 1;
				wcscat(pProcessName, L" (");
				wcscat(pProcessName, _itow(appList[i], buf, 10));
				wcscat(pProcessName, L")");

				CheckAndAddProcessLoggerNode(pProcessName, appList[i], Is64bitProcess(hProcess), statesMap, rootLogger);
			}

			CloseHandle(hProcess);
		}
#pragma endregion
	}
	else
	{
#pragma region REMOTE_HOST
		unique_ptr<RemMsg> msgReturned((RemMsg*)lParam);
		switch (msgReturned->m_msgID)
		{
		case MI_LISTPIPES_NTF:
		case MI_LISTPIPES_RESP:
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::OnHostMessage(MI_LISTPIPES_RESP)\n");

			ATL::CString processes;
			*msgReturned.get() >> processes;

			TCHAR* buffer = (TCHAR*)LPCTSTR(processes);
			for (TCHAR *pProcessName = _tcstok(buffer, L";"); pProcessName != NULL; pProcessName = _tcstok(NULL, L";"))
			{
				if (0 == *pProcessName)
					continue;

				TCHAR* pPID = _tcschr(pProcessName, L':');
				if (pPID != NULL)
					*pPID++ = 0;
				else
					pPID = L"0";

				TCHAR* pX64 = _tcschr(pPID, L'x');
				if (pX64 != NULL) *pX64 = 0;

				wstring sProcessName(pProcessName);
				sProcessName.append(L" (");
				sProcessName.append(pPID);
				sProcessName.append(L")");

				CheckAndAddProcessLoggerNode(sProcessName.c_str(), _wtoi(pPID), pX64 != NULL, statesMap, rootLogger);
			}

			break;
		}
		default:
			ATLTRACE2(atlTraceDBProvider, 0, L"** CMainFrame::OnHostMessage: unknown command %d\n", msgReturned->m_msgID);
			break;
		}
#pragma endregion
	}

	CHostLoggerItem& hostLogger = m_explorer.GetRootLogger();
	hostLogger.Lock();

	// clean-up terminated processes
	for (auto it=processes.begin(); it != processes.end(); )
	{
		auto procItem = *it;
		if (!procItem->IsFilterActive() && procItem->GetState() == LS_TERMINATED && procItem->GetNumMessages() == 0)
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::OnPGNProfStatus(): removed process logger pid=%d\n", procItem->GetPID());

			if (m_explorer.GetCurrentLoggerPtr() == *it)
			{
				m_explorer.SetCurrentLoggerPtr(0);
				UpdateFileSaveBtn();
			}

			CLoggerTracker::Instance().RemoveLogger(procItem);

			it = processes.erase(it);
			//DO NOT delete procItem; because it is owned by the ExplorerView
		}
		else
			it++;
	}

	// close log files
	m_explorer.RemoveClearedLogFiles();

	// render processes tree
	m_explorer.Populate();

	hostLogger.Unlock();

	// render profiler messages listview
	m_view.ShowMessages(m_explorer.GetCurrentLoggerPtr());

	// Display number of messages in the log
	DisplayNumMessages(m_explorer.GetCurrentLoggerPtr());

	HTREEITEM hItem = m_explorer.GetSelectedItem();
	//if (hItem != NULL)
	//{
		CLoggerItemBase* pLogger = (CLoggerItemBase*)m_explorer.GetItemData(hItem);
		bool bProcess = pLogger->GetType() == LT_PROCESS || pLogger->GetType() == LT_PROCESS64;
		TCHAR wszLogName[512] = { 0 };
		DisplayWindowTitle(GetExplorerItemText(bProcess ? pLogger->GetParent()->GetName() : L"", hItem, wszLogName, _countof(wszLogName)));
	//}
	//else
	//{
	//}

	bHandled = TRUE;
	return 0;
}

// Handle event of loading pgnp.dll into any process on the localhost.
// lParam = 0 process starts;
//        = 1 process terminates;
LRESULT CMainFrame::OnPGNProfStartSingle(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"Enter CMainFrame::OnPGNProfStartSingle(pid=%d %s)\n", wParam, lParam ? L"Terminated" : L"Launched");

	CHostLoggerItem& hostLogger = m_explorer.GetRootLogger();
	ATLASSERT(&hostLogger != NULL);
	CLoggerItemBase* pCurLogger = m_explorer.GetCurrentLoggerPtr();
	auto& processes = hostLogger.GetChildren();

	if (!lParam)
	{
		// remember all loggers states
		map<CLoggerItemBase*, LOGGER_STATE> statesMap;
		for (auto it = processes.cbegin(); it != processes.cend(); it++)
		{
			statesMap.insert(std::pair<CLoggerItemBase*, LOGGER_STATE>(*it, (*it)->GetState()));
			if (1 == lParam && static_cast<CProcessLoggerItem*>(*it)->GetPID() == wParam)
				(*it)->SetState(LS_TERMINATED);
		}

		// Get the process name.
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION /*| PROCESS_VM_READ*/, FALSE, wParam);
		if (hProcess == 0)
		{
			DWORD err = GetLastError();
			ATLTRACE2(atlTraceDBProvider, 0, L"** CMainFrame::OnPGNProfStartSingle: undefined PID=%d, err=%d\n", wParam, err);
			goto lEnd;
		}

		TCHAR buf[16], szProcessName[MAX_PATH * 2], *pProcessName;
		if (GetProcessImageFileName(hProcess, szProcessName, MAX_PATH * 2) != 0)
{
			pProcessName = wcsrchr(szProcessName, '\\') + 1;
			wcscat(pProcessName, L" (");
			wcscat(pProcessName, _itow(wParam, buf, 10));
			wcscat(pProcessName, L")");

			CheckAndAddProcessLoggerNode(pProcessName, wParam, Is64bitProcess(hProcess), statesMap, hostLogger);
		}

		CloseHandle(hProcess);
	}
	else
	{
		for (auto it = processes.cbegin(); it != processes.cend(); it++)
		{
			if (static_cast<CProcessLoggerItem*>(*it)->GetPID() == wParam)
			{
				(*it)->SetState(LS_TERMINATED);
				break;
			}
		}
	}

	hostLogger.Lock();

	// render processes tree
	m_explorer.Populate();

	hostLogger.Unlock();

	// render profiler messages listview
	m_view.ShowMessages(m_explorer.GetCurrentLoggerPtr());

	// Display number of messages in the log
	DisplayNumMessages(m_explorer.GetCurrentLoggerPtr());

lEnd:
	ATLTRACE2(atlTraceDBProvider, 0, L"Leave CMainFrame::OnPGNProfStartSingle\n");
	return 0;
}


LRESULT CMainFrame::OnPGNProfThemeChange(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CColorPalette::THEME theme = ((int)wParam) == -1 ? (CColorPalette::THEME)m_optionsForWork.m_dwColorTheme : (CColorPalette::THEME)wParam;
	SetupColorTheme(theme, CColorPalette::THEME_DARK == theme ? m_optionsForWork.m_qclrDark : m_optionsForWork.m_qclrLight);
	RedrawWindow(NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
	Invalidate();
	return 0;
}

LRESULT CMainFrame::OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CRect rect;
	GetClientRect(&rect);

	::FillRect((HDC)wParam, &rect, m_palit.m_brBackGndAlt);

	return 1;
}

LRESULT CMainFrame::OnDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LPDRAWITEMSTRUCT lpDrawItemStruct = (LPDRAWITEMSTRUCT)lParam;
	if (ATL_IDW_STATUS_BAR == wParam)	// lpDrawItemStruct->itemID
	{
		CDC dc(lpDrawItemStruct->hDC);
		dc.SetTextColor(m_palit.CrText);
		dc.SetBkColor(m_palit.CrBackGndAlt);
		lpDrawItemStruct->rcItem.top += 2;
		lpDrawItemStruct->rcItem.left += lpDrawItemStruct->itemID > 1 ? 20 : 2;
		LPTSTR lpstrText = (LPTSTR)lpDrawItemStruct->itemData;
		ATLTRACE2(atlTraceDBProvider, 2, L"CMainFrame::OnDrawItem(itemID=%d, action=%d, state=%d, '%s')\n", lpDrawItemStruct->itemID, lpDrawItemStruct->itemAction, lpDrawItemStruct->itemState, lpstrText);
		dc.DrawText(lpstrText, _tcslen(lpstrText), &lpDrawItemStruct->rcItem, DT_LEFT);

		return TRUE;
	}

	// Indicate that the message was not processed, so that other chained members can process it. Otherwise colored menu does not paint.
	bHandled = FALSE;
	return 0;
}

LRESULT CMainFrame::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (wParam == REFRESH_APPS_TIMER)
	{
		PostMessage(WM_PGNPROF_STATUS);
	}
	else if (wParam == 23456)
	{
		bHandled = FALSE;
		KillTimer(wParam);
	}
	else if (0 == (wParam % 4))		// PIDs in Windows are multiples of 4
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::OnTimer: posting termination for process pid=%d\n", wParam);
		KillTimer(wParam);
		PostMessageW(MYMSG_START_SINGLE, wParam, 1);
	}

	return 0;
}

LRESULT CMainFrame::OnSysCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	UINT uCmdType = (UINT)wParam;

	if((uCmdType & 0xFFF0) == IDM_ABOUTBOX)
	{
		OnAppAbout(uMsg, wParam, (HWND)lParam, bHandled);
	}
	else
		bHandled = FALSE;

	return 0;
}

// !This array must be modified to correspond IDB_MAINFRAME
static int ButtonsToAdd[] = {
/*000*/	ID_FILE_OPEN,
/*001*/	ID_FILE_SAVE,
/*002*/	ID_EDIT_COPY,
		1,	// separator
/*003*/	ID_CAPTURE,
/*004*/	0,	// ID_PAUSE_CAPTURE
		1,	// separator
/*005*/	ID_CLEAR_LOG,
/*006*/	ID_AUTOSCROLL,
/*007*/	0,	// ID_DONT_AUTOSCROLL
/*008*/	ID_TIMEFORMAT,
/*009*/	0,	// ID_TIMEFORMAT2
/*010*/	ID_FILTER,
/*011*/	0,	// ID_FILTER off
		1,	// separator
/*012*/	ID_APP_ABOUT,
		-1	// eol marker
};

HWND CMainFrame::CreateMyToolBar(DWORD dwStyle, CColorPalette::THEME theme)
{
	const UINT nResourceID = CColorPalette::THEME_DARK == theme ? IDB_MAINFRAME_DARK : IDB_MAINFRAME;
	const int initialSeparator = 1;
	const UINT nID = ATL_IDW_TOOLBAR;

	HINSTANCE hInst = ModuleHelper::GetResourceInstance();
	HRSRC hRsrc = ::FindResource(hInst, MAKEINTRESOURCE(nResourceID), RT_BITMAP);

	if (hRsrc == NULL)
		return NULL;

	HGLOBAL hGlobal = ::LoadResource(hInst, hRsrc);
	if (hGlobal == NULL)
		return NULL;

	LPBITMAPINFOHEADER pBitmapInfoHeader = (LPBITMAPINFOHEADER)::LockResource(hGlobal);
	if (pBitmapInfoHeader == 0)
		return 0;

	vector<TBBUTTON> btnList;
	const int cxSeparator = 8;

	// set initial separator (half width)
	if(initialSeparator)
	{
		TBBUTTON tbBtn;
		tbBtn.iBitmap = cxSeparator / 2;
		tbBtn.idCommand = 0;
		tbBtn.fsState = 0;
		tbBtn.fsStyle = TBSTYLE_SEP;
		tbBtn.dwData = 0;
		tbBtn.iString = 0;
		btnList.push_back(tbBtn);
	}

	int nBmp = 0;
	for (int* pButtonsToAdd = ButtonsToAdd; *pButtonsToAdd != -1; pButtonsToAdd++)
	{
		if (*pButtonsToAdd == 1)
		{
			TBBUTTON tbBtn;
			tbBtn.iBitmap = cxSeparator;
			tbBtn.idCommand = 0;
			tbBtn.fsState = 0;
			tbBtn.fsStyle = TBSTYLE_SEP;
			tbBtn.dwData = 0;
			tbBtn.iString = 0;
			btnList.push_back(tbBtn);
		}
		else 
		{
			if (*pButtonsToAdd != 0)
			{
				TBBUTTON tbBtn;
				tbBtn.iBitmap = nBmp;
				tbBtn.idCommand = *pButtonsToAdd;
				tbBtn.fsState = TBSTATE_ENABLED;
				tbBtn.fsStyle = TBSTYLE_BUTTON;
				tbBtn.dwData = 0;
				tbBtn.iString = 0;
				btnList.push_back(tbBtn);
			}
			nBmp++;
		}
	}

	HWND hWnd = ::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, dwStyle, 0, 0, 100, 100, m_hWnd, (HMENU)LongToHandle(nID), ModuleHelper::GetModuleInstance(), NULL);
	if(hWnd == NULL)
	{
		ATLASSERT(FALSE);
		return NULL;
	}

	::SendMessage(hWnd, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0L);

	// check if font is taller than our bitmaps
	CFontHandle font = (HFONT)::SendMessage(hWnd, WM_GETFONT, 0, 0L);
	if(font.IsNull())
		font = AtlGetDefaultGuiFont();
	LOGFONT lf = { 0 };
	font.GetLogFont(lf);
	WORD cyFontHeight = (WORD)abs(lf.lfHeight);

	CImageListCtrl images;
	images.CreateFromImage(nResourceID, 16, 0, CLR_DEFAULT, IMAGE_BITMAP, LR_CREATEDIBSECTION);
	::SendMessage(hWnd, TB_SETIMAGELIST, 0, (LPARAM)images.Detach());

	size_t nItems = btnList.size();
	CTempBuffer<TBBUTTON, _WTL_STACK_ALLOC_THRESHOLD> buff;
	TBBUTTON* pTBBtn = buff.Allocate(nItems);
	for(size_t i = 0; i < nItems; i++)
	{
		pTBBtn[i].iBitmap = btnList[i].iBitmap;
		pTBBtn[i].idCommand = btnList[i].idCommand;
		pTBBtn[i].fsState = btnList[i].fsState;
		pTBBtn[i].fsStyle = btnList[i].fsStyle;
		pTBBtn[i].dwData = btnList[i].dwData;
		pTBBtn[i].iString = btnList[i].iString;
	}

	::SendMessage(hWnd, TB_ADDBUTTONS, nItems, (LPARAM)pTBBtn);
	::SendMessage(hWnd, TB_SETBITMAPSIZE, 0, MAKELONG(16, max(pBitmapInfoHeader->biHeight, (LONG)cyFontHeight)));
	const int cxyButtonMargin = 7;
	::SendMessage(hWnd, TB_SETBUTTONSIZE, 0, MAKELONG(16 + cxyButtonMargin, max(pBitmapInfoHeader->biHeight, (LONG)cyFontHeight) + cxyButtonMargin));

	return hWnd;
}

void CMainFrame::SetPaneWidths(int* arrWidths, int nPanes)
{ 
    // find the size of the borders
    int arrBorders[3];	// an integer array that has three elements. The first element receives the width of the horizontal border, the second receives the width of the vertical border, and the third receives the width of the border between rectangles.
    m_status.GetBorders(arrBorders);

    // calculate right edge of default pane (0)
    arrWidths[0] += arrBorders[2];
    for (int i = 1; i < nPanes; i++)
        arrWidths[0] += arrWidths[i];

    // calculate right edge of remaining panes (1 thru nPanes-1)
    for (int j = 1; j < nPanes; j++)
        arrWidths[j] += arrBorders[2] + arrWidths[j - 1];

    // set the pane widths
    m_status.SetParts(m_status.m_nPanes, arrWidths); 
}

void CMainFrame::SetFontFromOption(ATL::CWindow window, LONG fontHt)
{
	if (fontHt < SMALLEST_FONT && fontHt > LARGEST_FONT)
	{
		LOGFONT lf;
		HFONT oldFont = window.GetFont();
		if (GetObject(oldFont, sizeof(LOGFONT), &lf))
		{
			lf.lfHeight = fontHt;

			HFONT newFont = CreateFontIndirect(&lf);
			window.SetFont(newFont);
		}
	}
}

void CMainFrame::StoreFontSetting(ATL::CWindow window, LONG fontHt, HKEY hOptionsKey, WCHAR* setting)
{
	LOGFONT lf;
	HFONT hFont = window.GetFont();
	if (GetObject(hFont, sizeof(LOGFONT), &lf) && fontHt != lf.lfHeight)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::StoreFontSetting: %s=%d\n", setting, lf.lfHeight);
		WCHAR buf[16];
		SetRegEntry(hOptionsKey, NULL, setting, _itow(lf.lfHeight, buf, 10));
	}
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	swprintf(m_aboutDlg._appTitle, PGNPTITLE_FULL, L"");
	wcscpy(m_aboutDlg._licensedTo, L"");
	wcscpy(m_aboutDlg._type, L"");
	wcscpy(m_aboutDlg._purchaseDate, L"");
	wcscpy(m_aboutDlg._special, L"");
	wcscpy(m_aboutDlg._subscription, L"");

	m_aboutDlg.Create(m_hWnd);
	m_aboutDlg.ShowWindow(SW_SHOWNORMAL);

	return 0;
}

LRESULT CMainFrame::OnCapture(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HTREEITEM hItem = m_explorer.GetSelectedItem();
	if (hItem == 0)
		return 0;

	CLoggerItemBase* pLogger = (CLoggerItemBase*)m_explorer.GetItemData(hItem);
	if (pLogger == 0 || pLogger->IsROmode())
		return 0;		// cannot start capture if logger is not selected or the logger is in RO mode

	switch (pLogger->GetType())
	{
	case LT_LOGROOT:
		{
			bool profileAllApps = (pLogger->GetState() == LS_PAUSED);
			auto processes = pLogger->GetChildren();
			for (auto it=processes.cbegin(); it != processes.cend(); it++)
			{
				if ((profileAllApps && (*it)->GetState() == LS_PAUSED) ||
					(!profileAllApps && (*it)->GetState() == LS_RUN))
				{
					CaptureToggle(*it, m_traceReader.GetUnlimitedWait());
				}
			}
			pLogger->SetState(profileAllApps ? LS_RUN : LS_PAUSED);
			break;
		}

	case LT_PROCESS:
	case LT_PROCESS64:
		CaptureToggle(pLogger, m_traceReader.GetUnlimitedWait());
	}

	UpdateCaptureButtonImg(pLogger);
	UpdateMenuIcons();

	m_explorer.Populate();
	return 0;
}

LRESULT CMainFrame::OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PostMessage(WM_PGNPROF_STATUS);
	return 0;
}

LRESULT CMainFrame::OnTraceLevel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HTREEITEM hItem = m_explorer.GetSelectedItem();
	if (hItem == 0)
		return 0;

	int level = 0;
	switch (wID)
	{
	case ID_TRACELEVEL_COMMENTLEVEL1: level = 1; break;
	case ID_TRACELEVEL_COMMENTLEVEL2: level = 2; break;
	case ID_TRACELEVEL_COMMENTLEVEL3: level = 3; break;
	}

	CLoggerItemBase* pLogger = (CLoggerItemBase*)m_explorer.GetItemData(hItem);
	pLogger->SetTraceLevel(level);

	return 0;
}

LRESULT CMainFrame::OnClearLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HTREEITEM hItem = m_explorer.GetSelectedItem();
	if (hItem == 0)
		return 0;

	// clear messages in selected logger
	CLoggerItemBase* pLogger = (CLoggerItemBase*)m_explorer.GetItemData(hItem);
	pLogger->SetFilterActive(false);
	pLogger->ClearLog();

	UISetCheck(ID_FILTER, 0);

	// clear details windows
	list<DWORD_PTR> dataList;
	m_details.ShowSQL(dataList);

	// Display number of messages in the log
	DisplayNumMessages(m_explorer.GetCurrentLoggerPtr());

	PostMessage(WM_PGNPROF_STATUS);
	return 0;
}

LRESULT CMainFrame::OnAutoScroll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	TBBUTTONINFO tbbi = {0};
	tbbi.cbSize = sizeof(TBBUTTONINFO);
	tbbi.dwMask = TBIF_IMAGE;

	if (m_toolbar.GetButtonInfo(ID_AUTOSCROLL, &tbbi) != -1)
	{
		tbbi.iImage ^= 1;
		m_toolbar.SetButtonInfo(ID_AUTOSCROLL, &tbbi);
		m_view.SetAutoScroll(!m_view.IsAutoScroll());
	}

	UpdateMenuIcons();

	return 0;
}

LRESULT CMainFrame::OnTimeFormat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	TBBUTTONINFO tbbi = {0};
	tbbi.cbSize = sizeof(TBBUTTONINFO);
	tbbi.dwMask = TBIF_IMAGE;

	if (m_toolbar.GetButtonInfo(ID_TIMEFORMAT, &tbbi) != -1)
	{
		tbbi.iImage ^= 1;
		m_toolbar.SetButtonInfo(ID_TIMEFORMAT, &tbbi);
		m_view.SetRelativeTime(!m_view.IsRelativeTime());
		m_view.Invalidate(FALSE);
	}

	UpdateMenuIcons();

	return 0;
}

LRESULT CMainFrame::OnFilter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLoggerItemBase* pCurLogger = m_explorer.GetCurrentLoggerPtr();
	if (pCurLogger == 0)
		return 0;	// nothing to do.

	TBBUTTONINFO tbbi = {0};
	tbbi.cbSize = sizeof(TBBUTTONINFO);
	tbbi.dwMask = TBIF_IMAGE;

	if (m_toolbar.GetButtonInfo(ID_FILTER, &tbbi) != -1)
	{
		if (11 == tbbi.iImage)		// see definition of ButtonsToAdd for image indexes
		{
			// Store selected items; they will be processed in OnIdle.
			m_itemsToSelect.clear();

			int nItem = -1;
			UINT cnt = m_view.GetSelectedCount();

			for (UINT i = 0; i < cnt; i++)
			{
				nItem = m_view.GetNextItem(nItem, LVNI_SELECTED);

				m_itemsToSelect.push_back((DWORD_PTR)m_view.GetMessageData(nItem));
			}

			// Turn filter off.
			pCurLogger->SetFilterActive(false);
			m_filter.m_filterOn = false;
		}
		else
		{
			// Get the filter text entered by user.
			WCHAR wsFilter[4000+2] = {0};
			int len;
			if (!(len = ::GetWindowText(m_filter.m_hWnd, wsFilter, 4000)) || !*wsFilter)
				return 0;	// no filter set

			pCurLogger->SetFilterStr(wsFilter);
			pCurLogger->SetFilterActive(true, std::bind(&CMainFrame::FilterProgress, this, std::placeholders::_1));
			m_filter.m_filterOn = true;

		}

		UpdateMenuIcons();

		// refresh messages view
		m_view.Refresh();

		// clear details windows
		list<DWORD_PTR> dataList;
		m_details.ShowSQL(dataList);

		//UpdateLayout();

		tbbi.iImage ^= 1;
		m_toolbar.SetButtonInfo(ID_FILTER, &tbbi);
	}

	return 0;
}

void CMainFrame::UpdateMenuIcons()
{
	CMenuHandle menuPopup = (HMENU)m_CmdBar.m_hMenu;
	ATLASSERT(menuPopup.m_hMenu != NULL);

	static const WORD updateIds[] = { ID_AUTOSCROLL, ID_FILTER, ID_CAPTURE, ID_TIMEFORMAT, 0 };

	for (auto ids = &updateIds[0]; *ids; ids++)
	{
		CMenuItemInfo mii;
		mii.fMask = MIIM_ID | MIIM_DATA | MIIM_STATE;
		menuPopup.GetMenuItemInfo(*ids, FALSE, &mii);

		MenuItemData* pMI = (MenuItemData*)mii.dwItemData;
		if (pMI != NULL)
		{
			if (pMI->hIcon != NULL) DestroyIcon(pMI->hIcon);
			pMI->hIcon = (HICON)LoadImage(_Module.GetResourceInstance(), MakeIntRes(*ids), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
			menuPopup.SetMenuItemInfo(*ids, FALSE, &mii);
		}
	}
}

LRESULT CMainFrame::OnZoomin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HWND h = GetFocus();

	ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::OnZoomin(h=%d)\n", h);

	::PostMessage(h, WM_CHAR, '.', 1);
	return 0;
}

LRESULT CMainFrame::OnZoomout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	::PostMessage(GetFocus(), WM_CHAR, ',', 1);
	return 0;
}

LRESULT CMainFrame::OnHighlighting(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	::SetCursor(::LoadCursor(NULL, IDC_WAIT));

	m_optionsDlg.Create(m_hWnd);
	m_optionsDlg.ShowWindow(SW_SHOWNORMAL);

	::SetCursor(::LoadCursor(NULL, IDC_ARROW));

	return 0;
}

LRESULT CMainFrame::OnEditCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HWND hWnd = GetFocus();
	if (hWnd == m_details.m_hWnd || hWnd == m_filter.m_hWnd || hWnd == m_view.m_hWnd)
		::SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(wID, BN_CLICKED), (LPARAM)m_hWnd);
	return 0;
}

LRESULT CMainFrame::OnEditPaste(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HWND hWnd = GetFocus();
	if (hWnd == m_filter.m_hWnd)
		::SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(wID, BN_CLICKED), (LPARAM)m_hWnd);
	return 0;
}

void CMainFrame::DisplayNumMessages(CLoggerItemBase* pLogger)
{
	switch (pLogger->GetTraceLevel())
	{
	case COMMENT_1:
		m_status.SetPaneText(ID_STBLEVEL, L"Comment Level 1", SBT_NOBORDERS | SBT_OWNERDRAW);
		break;
	case COMMENT_2:
		m_status.SetPaneText(ID_STBLEVEL, L"Comment Level 2", SBT_NOBORDERS | SBT_OWNERDRAW);
		break;
	case COMMENT_3:
		m_status.SetPaneText(ID_STBLEVEL, L"Comment Level 3", SBT_NOBORDERS | SBT_OWNERDRAW);
		break;
	default:
		m_status.SetPaneText(ID_STBLEVEL, L"Commands Only", SBT_NOBORDERS | SBT_OWNERDRAW);
	}

	int nErrors = pLogger ? pLogger->GetNumErrors() : 0;
	int nInfoMsgs = pLogger ? pLogger->GetNumMessages() : 0;

	wchar_t* buf = statusPaneBuf[2];
	_itow(nErrors, buf, 10);
	m_status.SetPaneText(ID_STBERROR, wcscat(buf, nErrors != 1 ? L" Errors" : L" Error"), SBT_NOBORDERS | SBT_OWNERDRAW);

	buf = statusPaneBuf[3];
	_itow(nInfoMsgs, buf, 10);
	m_status.SetPaneText(ID_STBINFORMATION, wcscat(buf, nInfoMsgs != 1 ? L" Messages" : L" Message"), SBT_NOBORDERS | SBT_OWNERDRAW);
}

void CMainFrame::Lock()
{
	_lock.Wait();
}

void CMainFrame::Unlock()
{
	_lock.Release();
}

void CMainFrame::FilterProgress(LONG value)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::FilterProgress(%d)\n", value);

	InterlockedExchange(&m_filterProgress, (unsigned)value);

	if (value == FILTER_START || value == FILTER_FINISH)
	{
		PostMessage(MYMSG_UI_UPDATE, UPDUI_STATUSBAR, value);
	}
	else
	{
		system_clock::time_point now = system_clock::now();
		duration<double> elapsed_seconds = now - m_filterStart;
		if (elapsed_seconds.count() > 0.3)
		{
			Lock();
			m_filterStart = now;
			Unlock();

			//PostMessage(MYMSG_UI_UPDATE, UPDUI_STATUSBAR, value);
		}
	}
}

BOOL CMainFrame::OnIdle()
{
	UIUpdateToolBar();

	// render profiler messages listview
	CLoggerItemBase* pLogger = m_explorer.GetCurrentLoggerPtr();
	BOOL msgCountChanged = m_view.ShowMessages(pLogger);
	if (msgCountChanged || m_bSelectionChanged)
	{
		DisplayNumMessages(pLogger);

		if (m_explorer.GetCurrentLoggerPtr() != nullptr && !m_explorer.GetCurrentLoggerPtr()->IsFilterActive())
		{
			// set Ready in statusbar only when filter is not active
			wchar_t* buf = statusPaneBuf[0];
			if (::LoadString(ModuleHelper::GetResourceInstance(), ATL_IDS_IDLEMESSAGE, buf, 128) > 0)
			{
				m_status.SetText(0, buf, SBT_NOBORDERS | SBT_OWNERDRAW);
			}
		}
	}

	if (m_bSelectionChanged)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::OnIdle: logger %s\n", m_explorer.GetCurrentLoggerPtr()->GetDisplayName());
		m_filter.m_filterOn = m_explorer.GetCurrentLoggerPtr() != nullptr && m_explorer.GetCurrentLoggerPtr()->IsFilterActive();
		UpdateMenuIcons();

		m_bSelectionChanged = false;

		int nItem = -1;
		UINT cnt = m_view.GetSelectedCount();
		list<DWORD_PTR> dataList;

		for (UINT i=0; i < cnt; i++)
		{
			nItem = m_view.GetNextItem(nItem, LVNI_SELECTED);

			//DWORD_PTR index = m_view.GetItemData(nItem);
			dataList.push_back((DWORD_PTR)m_view.GetMessageData(nItem));
		}

		m_details.ShowSQL(dataList);
	}
	else if (m_itemsToSelect.size() > 0)
	{
		m_view.SelectItems(m_itemsToSelect);

		m_itemsToSelect.clear();
	}

	if (m_filterProgress != m_prevFilterProgress)
	{
		m_prevFilterProgress = m_filterProgress;

		ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::OnIdle: progress %d\n", m_prevFilterProgress);

		if (m_prevFilterProgress >= 0 && m_prevFilterProgress <= 100)
		{
			wchar_t* buf = statusPaneBuf[0];
			wcscpy(buf, L"Filtering: ");
			_itow(m_prevFilterProgress, &buf[wcslen(buf)], 10);
			wcscat(buf, L"%");

			m_status.SetText(0, buf, SBT_NOBORDERS | SBT_OWNERDRAW);
		}

		::SetCursor(::LoadCursor(NULL, IDC_APPSTARTING));
	}

	return FALSE;
}

LRESULT CMainFrame::OnTreeItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	NMTREEVIEW* pnmtv = (NMTREEVIEW*) pnmh;

	if (pnmtv->action || !m_filter.IsWindowEnabled())
	{
		HTREEITEM hItem = pnmtv->itemNew.hItem;
		CLoggerItemBase* pLogger = (CLoggerItemBase*)m_explorer.GetItemData(hItem);
		::EnableWindow(m_filter, pLogger != 0);
		m_explorer.SetCurrentLoggerPtr(pLogger);
		UpdateFileSaveBtn();
		UpdateCaptureButtonImg(pLogger);
		UpdateFilterButtonImg(pLogger);
		UpdateFilterText(pLogger);

		// trigger details view update
		m_bSelectionChanged = true;

		bool bProcess = pLogger->GetType() == LT_PROCESS || pLogger->GetType() == LT_PROCESS64;
		TCHAR wszLogName[512] = { 0 };
		DisplayWindowTitle(GetExplorerItemText(bProcess ? pLogger->GetParent()->GetName() : L"", hItem, wszLogName, _countof(wszLogName)));
	}

	bHandled = TRUE;
	return 0;
}

void CMainFrame::UpdateFileSaveBtn()
{
	const CLoggerItemBase* pCurLogger = m_explorer.GetCurrentLoggerPtr();
	m_toolbar.SetState(ID_FILE_SAVE, (pCurLogger /*&& !pCurLogger->IsROmode()*/) ? TBSTATE_ENABLED : 0);
}

void CMainFrame::UpdateCaptureButtonImg(const CLoggerItemBase* pLogger)
{
	if (!pLogger)
		return;

	TBBUTTONINFO tbbi = {0};
	tbbi.cbSize = sizeof(TBBUTTONINFO);
	tbbi.dwMask = TBIF_IMAGE|TBIF_STATE;

	m_toolbar.GetButtonInfo(ID_CAPTURE, &tbbi);

	if (pLogger->GetState() == LS_PAUSED)
	{
		tbbi.iImage = 003;	// see definition of ButtonsToAdd for image indexes
		tbbi.fsState = TBSTATE_ENABLED;
		m_toolbar.SetButtonInfo(ID_CAPTURE, &tbbi);
	}
	else if (pLogger->GetState() == LS_RUN)
	{
		tbbi.iImage = 004;	// see definition of ButtonsToAdd for image indexes
		tbbi.fsState = TBSTATE_ENABLED;
		m_toolbar.SetButtonInfo(ID_CAPTURE, &tbbi);
	}
	else if (pLogger->GetState() == LS_TERMINATED)
	{
		tbbi.iImage = 003;	// see definition of ButtonsToAdd for image indexes
		tbbi.fsState = 0;
		m_toolbar.SetButtonInfo(ID_CAPTURE, &tbbi);
	}
}

void CMainFrame::UpdateFilterButtonImg(const CLoggerItemBase* pLogger)
{
	TBBUTTONINFO tbbi = {0};
	tbbi.cbSize = sizeof(TBBUTTONINFO);
	tbbi.dwMask = TBIF_IMAGE;

	if (m_toolbar.GetButtonInfo(ID_FILTER, &tbbi) != -1)
	{
		tbbi.iImage = (pLogger && pLogger->IsFilterActive()) ? 11 : 10;	// see definition of ButtonsToAdd for image indexes
		m_toolbar.SetButtonInfo(ID_FILTER, &tbbi);
	}
}

void CMainFrame::UpdateFilterText(const CLoggerItemBase* pLogger)
{
	wstring wsFilter(pLogger ? pLogger->GetFilterStr() : L"");

	::SetWindowText(m_filter.m_hWnd, wsFilter.c_str());
}

LRESULT CMainFrame::OnListItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLISTVIEW* pNMLV = (NMLISTVIEW*) pnmh;

	// If the change wasn't in the state of an item, then we don't have to do anything.
	if (0 == (pNMLV->uChanged & LVIF_STATE))
		return 0;

	// trigger details view update
	m_bSelectionChanged = true;

	bHandled = TRUE;

	return 0;
}

LRESULT CMainFrame::OnDragDropFile(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LPCTSTR lpFileName = (LPCTSTR)wParam;
	size_t len = _tcslen(lpFileName);
	bool csv = len > 4 && 0 == _tcsicmp(&lpFileName[len - 4], L".csv");

	MakeFileLoggerNode(lpFileName, csv);

	delete[] lpFileName;

	return 0;
}

LRESULT CMainFrame::OnProfileAllApps(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::OnProfileAllApps()\n");

	CHostLoggerItem& logger = m_explorer.GetRootLogger();
	auto processes = logger.GetChildren();
	for (auto it=processes.cbegin(); it != processes.cend(); it++)
	{
		if ((*it)->GetState() == LS_PAUSED)
		{
			CaptureToggle(*it, m_traceReader.GetUnlimitedWait());
		}
	}

	if (logger.GetState() == LS_PAUSED)
		logger.SetState(LS_RUN);

	UpdateCaptureButtonImg(&logger);

	m_explorer.Populate();

	return 0;
}

LRESULT CMainFrame::OnSaveSettings(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	SaveSettings(wParam);

	return 0;
}

LRESULT CMainFrame::OnHostConnected(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CConnectParams* params = (CConnectParams*)lParam;

	// Check if the same host is already connected
	if (m_explorer.IsHostConnected(params->_host))
	{
		// TODO: select existing host. Should we reconnect (if credentials are different)?
		return 0;
	}

	// Create "RemoteHost" node
	unique_ptr<wstring> workPath(new wstring(m_optionsForWork.m_sWorkPath));
	workPath->append(L"\\");
	workPath->append(params->_host);

	unique_ptr<CHostLoggerItem> pHostLogger(new CHostLoggerItem(workPath.release()));
	pHostLogger->SetName(params->_host);
	pHostLogger->SetCredentials(params->_user, params->_pwd);
	pHostLogger->SetMainWindow(m_hWnd);

	ATL::CString processes;
	int exitCode = pHostLogger->ListProcessesOnRemoteHost(processes);
	if (exitCode != 0)
	{
		USES_XMSGBOXPARAMS
		XMessageBox(m_hWnd, L"Failed to connect to remote host.", L"Error", MB_OK | MB_ICONERROR, &__xmb);
	}
	else
	{
		m_explorer.AddHost(pHostLogger.get());

		TCHAR* buffer = (TCHAR*)LPCTSTR(processes);
		for (TCHAR *pProcessName = _tcstok(buffer, L";"); pProcessName != NULL; pProcessName = _tcstok(NULL, L";"))
		{
			if (0 == *pProcessName)
				continue;

			TCHAR* pPID = _tcschr(pProcessName, L':');
			if (pPID != NULL)
				*pPID++ = 0;
			else
				pPID = L"0";

			TCHAR* pX64 = _tcschr(pPID, L'x');
			if (pX64 != NULL) *pX64 = 0;

			CProcessLoggerItem* pProcess = new CProcessLoggerItem((wstring(pProcessName) + L" (" + pPID + L")").c_str(), pHostLogger.get(), LS_PAUSED, pX64 != NULL);
			pProcess->SetPID(_wtoi(pPID));
			pProcess->SetMainWindow(m_hWnd);
			if (pProcess->OpenLogFile(m_optionsForWork.m_dwDeleteWorkFiles) == 0)
			{
				pHostLogger->AddChild(pProcess);

				//if (rootState == LS_RUN)
				//{
				//	CaptureToggle(pProcess, m_traceReader.GetUnlimitedWait());
				//}
			}
		}

		m_traceReader.Start(pHostLogger.get());
		
		::EnableWindow(m_filter, pHostLogger.get() != 0);
		m_explorer.SetCurrentLoggerPtr(pHostLogger.get());
		UpdateFileSaveBtn();
		UpdateCaptureButtonImg(pHostLogger.get());
		UpdateFilterButtonImg(pHostLogger.get());
		UpdateFilterText(pHostLogger.get());

		pHostLogger->SubscribeForEvents();
		pHostLogger.release();

		// trigger details view update
		m_bSelectionChanged = true;

		PostMessage(WM_PGNPROF_STATUS);
	}

	return 0;
}

LRESULT CMainFrame::OnUIUpdate(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (UPDUI_STATUSBAR == wParam)
	{
		if (lParam == FILTER_START)
		{
			m_status.SetText(0, L"Filtering messages...", SBT_NOBORDERS | SBT_OWNERDRAW);

			Lock();
			m_filterStart = system_clock::now();
			Unlock();

			::SetCursor(::LoadCursor(NULL, IDC_APPSTARTING));
		}
		else if (lParam == FILTER_FINISH)
		{
			m_status.SetText(0, L"Filter complete.", SBT_NOBORDERS | SBT_OWNERDRAW);
			
			::SetCursor(::LoadCursor(NULL, IDC_ARROW));
		}
		else
		{
			//wchar_t* buf = statusPaneBuf[0];
			//wcscpy(buf, L"Filtering: ");
			//_itow(lParam, &buf[wcslen(buf)], 10);
			//wcscat(buf, L"%");
			//
			//m_status.SetText(0, buf, SBT_NOBORDERS | SBT_OWNERDRAW);

			//::SetCursor(::LoadCursor(NULL, IDC_APPSTARTING));
		}
	}

	return 0;
}

static void GetLogFilePath(/*[out]*/WCHAR* path)
{
	HRESULT hr = SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path);
	if (FAILED(hr))
	{	// get the path from registry
		HKEY hkey;
		if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", 0, KEY_READ, &hkey))
		{
			DWORD dwSize = MAX_PATH;

			RegQueryValueEx(hkey, L"Local AppData", NULL, NULL, (BYTE*)path, &dwSize);	// check if returned ERROR_SUCCESS
			
			RegCloseKey(hkey);
		}
	}

	wcscat(path, L"\\PGNP");
}

void CMainFrame::pipeCreatedCallback(int pid)
{
	SendMessage(MYMSG_START_SINGLE, (WPARAM)pid, 0);
}

void CMainFrame::pipeClosedCallback(int pid)
{
	SetTimer(pid, 1000, 0);
}

extern bool qclrFromString(vector<COLORREF>& qclr, unsigned hash, const string& s);
extern COLORREF g_qclr_light[QCLR_SIZE];
extern COLORREF g_qclr_dark[QCLR_SIZE];

void CMainFrame::LoadSettings() 
{
	try
	{
		WCHAR buffer[16384];
		GetLogFilePath(buffer);
		m_optionsForWork.m_sWorkPath = buffer;

		HKEY hOptionsKey = NULL;

		// Open the key to make things a little quicker and direct...
		TESTC_(OpenRegKey(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hOptionsKey),S_OK);

		// General options
		if (SUCCEEDED(GetRegEntry(hOptionsKey, NULL, L"MaxLogFileSize", buffer, sizeof(buffer))))
		{
			m_optionsForWork.m_llMaxLogFileSize = CPGNPOptions::AbbreviatedToNumber(buffer);
		}
		GetRegEntry(hOptionsKey, NULL, L"ProfileAllApps",		&m_optionsForWork.m_dwProfileAllApps);
		GetRegEntry(hOptionsKey, NULL, L"DeleteWorkFiles",		&m_optionsForWork.m_dwDeleteWorkFiles);
		if (SUCCEEDED(GetRegEntry(hOptionsKey, NULL, L"WorkPath", buffer, sizeof(buffer))))
		{
			m_optionsForWork.m_sWorkPath = buffer;
		}
		//GetRegEntry(hOptionsKey, NULL, L"RefreshApps",			&m_optionsForWork.m_dwRefreshApps);
		GetRegEntry(hOptionsKey, NULL, L"ColorTheme",			&m_optionsForWork.m_dwColorTheme);
		if (m_optionsForWork.m_dwColorTheme != CColorPalette::THEME_LIGHT && m_optionsForWork.m_dwColorTheme != CColorPalette::THEME_DARK)
			m_optionsForWork.m_dwColorTheme = CColorPalette::THEME_DEFAULT;
		GetRegEntry(hOptionsKey, NULL, L"ParamFormat",			&m_optionsForWork.m_dwParamFormat);
		GetRegEntry(hOptionsKey, NULL, L"ConnectToHostEnabled",	&m_optionsForWork.m_dwConnectToHostEnabled);
		//GetRegEntry(hOptionsKey, NULL, L"TraceLevel",			&m_optionsForWork.m_dwTraceLevel);
		//SetTraceLevel(m_optionsForWork.m_dwTraceLevel);
	
		if (SUCCEEDED(GetRegEntryA(hOptionsKey, NULL, "qclrLight", (char*)buffer, sizeof(buffer))))
		{
			Json::Value root;
			Json::Reader reader;

			ATLTRACE2(atlTraceDBProvider, 0, "CMainFrame::LoadSettings: read registry key qclrLight = %s\n", (char*)buffer);

			if (reader.parse((char*)buffer, root))
			{
				m_optionsForWork.m_qclrLight.assign(QCLR_SIZE, 0);

				for each (auto qclrMember in root.getMemberNames())
				{
					auto jv = root[qclrMember.c_str()];
					qclrFromString(m_optionsForWork.m_qclrLight, str2hash(qclrMember.c_str(), 0), jv.asString());
				}

				ATLTRACE2(atlTraceDBProvider, 0, _T("CMainFrame::LoadSettings: qclrLight size = %d\n"), m_optionsForWork.m_qclrLight.size());
			}
		}
	
		if (QCLR_SIZE != m_optionsForWork.m_qclrLight.size())
		{
			ATLTRACE2(atlTraceDBProvider, 0, _T("CMainFrame::LoadSettings: setting qclrLight to default (new size=%d)\n"), QCLR_SIZE);

			m_optionsForWork.m_qclrLight.clear();
			m_optionsForWork.m_qclrLight.assign(g_qclr_light, g_qclr_light + QCLR_SIZE);
		}

		if (SUCCEEDED(GetRegEntryA(hOptionsKey, NULL, "qclrDark", (char*)buffer, sizeof(buffer))))
		{
			Json::Value root;
			Json::Reader reader;

			ATLTRACE2(atlTraceDBProvider, 0, "CMainFrame::LoadSettings: read registry key qclrDark = %s\n", (char*)buffer);

			if (reader.parse((char*)buffer, root))
			{
				m_optionsForWork.m_qclrDark.assign(QCLR_SIZE, 0);

				for each (auto qclrMember in root.getMemberNames())
				{
					auto jv = root[qclrMember.c_str()];
					qclrFromString(m_optionsForWork.m_qclrDark, str2hash(qclrMember.c_str(), 0), jv.asString());
				}

				ATLTRACE2(atlTraceDBProvider, 0, _T("CMainFrame::LoadSettings: qclrDark size = %d\n"), m_optionsForWork.m_qclrDark.size());
			}
		}

		if (QCLR_SIZE != m_optionsForWork.m_qclrDark.size())
		{
			ATLTRACE2(atlTraceDBProvider, 0, _T("CMainFrame::LoadSettings: setting qclrDark to default (new size=%d)\n"), QCLR_SIZE);

			m_optionsForWork.m_qclrDark.clear();
			m_optionsForWork.m_qclrDark.assign(g_qclr_dark, g_qclr_dark + QCLR_SIZE);
		}

		m_optionsForWork.m_visibleColumnsList.clear();
		if (SUCCEEDED(GetRegEntry(hOptionsKey, NULL, L"VisibleColumns", buffer, sizeof(buffer))))
		{
			for (WCHAR *token = wcstok(buffer, L";"); token != NULL; token = wcstok(NULL, L";"))
			{
				m_optionsForWork.m_visibleColumnsList.push_back(token);
			}
		}

		if (SUCCEEDED(GetRegEntry(hOptionsKey, NULL, L"RemoteHosts", buffer, sizeof(buffer))))
		{
			Json::Value root;
			Json::Reader reader;

			char stxt[16384];
			WideCharToMultiByte(CP_ACP, 0, buffer, -1, stxt, sizeof(stxt), NULL, NULL);

			bool parsingSuccessful = reader.parse(stxt, root);
			if (parsingSuccessful)
			{
				Json::Value::Members members = root.getMemberNames();
				for (auto it = members.begin(); it != members.end(); it++)
				{
					auto jsonval = root[*it];

					ScrambleText(g_svalue, sizeof(g_svalue), jsonval.asString().c_str(), true);

					string host, user, pwd;
					const char *p = g_svalue;
					while (*p && *p != '\1')
						host += *p++;

					p++;
					while (*p && *p != '\1')
						user += *p++;

					p++;
					while (*p && *p != '\1')
						pwd += *p++;

					CRemoteConnectionInfo rci = { CA2W(host.c_str()), CA2W(user.c_str()), CA2W(pwd.c_str()) };
					m_optionsForWork.m_remoteConnections.push_back(rci);
				}
			}
		}

		if (SUCCEEDED(GetRegEntry(hOptionsKey, NULL, L"Panel1FontHt", buffer, sizeof(buffer))))
		{
			LONG newHeight = _wtoi(buffer);
			if (newHeight < SMALLEST_FONT && newHeight > LARGEST_FONT)		// do not allow too small or too large font
			{
				ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::LoadSettings: Panel1FontHt=%d\n", newHeight);

				m_optionsForWork.m_panel1FontHt = newHeight;
			}
		}

		if (SUCCEEDED(GetRegEntry(hOptionsKey, NULL, L"DetailsFontHt", buffer, sizeof(buffer))))
		{
			LONG newHeight = _wtoi(buffer);
			if (newHeight < SMALLEST_FONT && newHeight > LARGEST_FONT)		// do not allow too small or too large font
			{
				ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::LoadSettings: DetailsFontHt=%d\n", newHeight);

				m_optionsForWork.m_DetailsFontHt = newHeight;
			}
		}

		if (SUCCEEDED(GetRegEntry(hOptionsKey, NULL, L"ExplorerFontHt", buffer, sizeof(buffer))))
		{
			LONG newHeight = _wtoi(buffer);
			if (newHeight < SMALLEST_FONT && newHeight > LARGEST_FONT)		// do not allow too small or too large font
			{
				ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::LoadSettings: ExplorerFontHt=%d\n", newHeight);

				m_optionsForWork.m_ExplorerFontHt = newHeight;
			}
		}

		if (SUCCEEDED(GetRegEntry(hOptionsKey, NULL, L"FilterFontHt", buffer, sizeof(buffer))))
		{
			LONG newHeight = _wtoi(buffer);
			if (newHeight < SMALLEST_FONT && newHeight > LARGEST_FONT)		// do not allow too small or too large font
			{
				ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::LoadSettings: FilterFontHt=%d\n", newHeight);

				m_optionsForWork.m_FilterFontHt = newHeight;
			}
		}

		if (SUCCEEDED(GetRegEntry(hOptionsKey, NULL, L"OptimizerConnect", buffer, sizeof(buffer))))
		{
			m_optionsForWork.m_sOptimizerConnectionString = buffer;
		}
		else
		{
			m_optionsForWork.m_sOptimizerConnectionString.clear();
		}

CLEANUP:
		CloseRegKey(hOptionsKey);

		if (0 == m_optionsForWork.m_visibleColumnsList.size())	// if no columns were added from registry settings, then add them all
		{
			WCHAR buf[32];
			for (int i = 0; i < SQLLOG_COLUMNS_CNT; i++)
			{
				wstring token(SQLLOG_COLUMNS[i].pszText);
				token += L':';
				token += _itow(SQLLOG_COLUMNS[i].cx, buf, 10);
				m_optionsForWork.m_visibleColumnsList.push_back(token);
			}
		}
	}
	catch (...)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"** CMainFrame::LoadSettings: exception occurred\n");
	}

	// Copy work options to editable options
	m_optionsForEdit = m_optionsForWork;
}

static string qclrToJson(const vector<COLORREF>& qclr)
{
	Json::Value root;
	char buf[2048] = { 0 };
	itoa(qclr[QCLR_FOCUS], buf, 16);
	root["qclr_focus"] = buf;
	itoa(qclr[QCLR_SELECT], buf, 16);
	root["qclr_select"] = buf;
	itoa(qclr[QCLR_INSERT], buf, 16);
	root["qclr_insert"] = buf;
	itoa(qclr[QCLR_UPDATE], buf, 16);
	root["qclr_update"] = buf;
	itoa(qclr[QCLR_DELETE], buf, 16);
	root["qclr_delete"] = buf;
	itoa(qclr[QCLR_CREATEDROPALTER], buf, 16);
	root["qclr_alter"] = buf;
	itoa(qclr[QCLR_PROCEDURES], buf, 16);
	root["qclr_procedures"] = buf;
	itoa(qclr[QCLR_SYSSCHEMA], buf, 16);
	root["qclr_sysschema"] = buf;
	itoa(qclr[QCLR_SYSTEM], buf, 16);
	root["qclr_system"] = buf;
	itoa(qclr[QCLR_ERROR], buf, 16);
	root["qclr_error"] = buf;
	itoa(qclr[QCLR_NOTIFY], buf, 16);
	root["qclr_notify"] = buf;
	itoa(qclr[QCLR_NONE], buf, 16);
	root["qclr_none"] = buf;
	itoa(qclr[QCLR_COMMENT], buf, 16);
	root["qclr_comment"] = buf;

	Json::StyledWriter writer;
	return writer.write(root);
}

void CMainFrame::SaveSettings(DWORD flags) 
{
	HKEY hOptionsKey = NULL;
	bool bShowRestartMessage = false;

	// Open the key to make things a little quicker and direct...
	TESTC_(CreateRegKey(HKEY_CURRENT_USER, REG_PATH, &hOptionsKey),S_OK);
	
	if ((flags & OT_GENERAL) != 0)
	{
		int optionsEquality = (m_optionsForWork == m_optionsForEdit);
		if (!(optionsEquality & CPGNPOptions::EQUAL_CAT_GENERAL) || m_bOptionsWerePreviouslyDifferent)
		{
			// General options
			SetRegEntry(hOptionsKey, NULL, L"ProfileAllApps",		m_optionsForEdit.m_dwProfileAllApps);
			SetRegEntry(hOptionsKey, NULL, L"DeleteWorkFiles",		m_optionsForEdit.m_dwDeleteWorkFiles);
			SetRegEntry(hOptionsKey, NULL, L"WorkPath",				(WCHAR*)m_optionsForEdit.m_sWorkPath.c_str());
			//SetRegEntry(hOptionsKey, NULL, L"RefreshApps",			m_optionsForEdit.m_dwRefreshApps);
			SetRegEntry(hOptionsKey, NULL, L"ColorTheme",			m_optionsForEdit.m_dwColorTheme);
			SetRegEntry(hOptionsKey, NULL, L"ParamFormat",			m_optionsForEdit.m_dwParamFormat);
			//SetRegEntry(hOptionsKey, NULL, L"TraceLevel",			m_optionsForEdit.m_dwTraceLevel);

			bShowRestartMessage = true;
		}

		//SetTraceLevel(m_optionsForEdit.m_dwTraceLevel);

		if (!(optionsEquality & CPGNPOptions::EQUAL_CAT_QCLRLIGHT) || m_bOptionsWerePreviouslyDifferent)
		{
			std::string jsonText = qclrToJson(m_optionsForEdit.m_qclrLight);
			SetRegEntryA(hOptionsKey, NULL, (char*)"qclrLight", (char*)jsonText.c_str());

			RedrawWindow(NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
			Invalidate();
		}

		if (!(optionsEquality & CPGNPOptions::EQUAL_CAT_QCLRDARK) || m_bOptionsWerePreviouslyDifferent)
		{
			std::string jsonText = qclrToJson(m_optionsForEdit.m_qclrDark);
			SetRegEntryA(hOptionsKey, NULL, (char*)"qclrDark", (char*)jsonText.c_str());

			RedrawWindow(NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
			Invalidate();
		}

		if (!(optionsEquality & CPGNPOptions::EQUAL_CAT_COLUMNS) || m_bOptionsWerePreviouslyDifferent)
		{
			// Columns options
			SetRegEntry(hOptionsKey, NULL, L"VisibleColumns",		(WCHAR*)m_optionsForEdit.m_visibleColumnsList.c_str());

			m_view.CreateColumns();
			m_view.Invalidate(FALSE);
		}
		
		if (!(optionsEquality & CPGNPOptions::EQUAL_CAT_OPTIMIZER))
		{
			// Columns options
			SetRegEntry(hOptionsKey, NULL, L"OptimizerConnect",		(WCHAR*)m_optionsForEdit.m_sOptimizerConnectionString.c_str());
		}

		if (!(optionsEquality & CPGNPOptions::EQUAL_CAT_REMOTE))
		{
			// Columns options
			SetRegEntry(hOptionsKey, NULL, L"RemoteHosts", (WCHAR*)m_optionsForEdit.m_remoteConnections.c_str());
		}

		m_bOptionsWerePreviouslyDifferent = optionsEquality != CPGNPOptions::EQUAL_CAT_MASK;
	}

	if ((flags & OT_COLUI) != 0)
	{
		wstring columns;
		m_view.GetColumnsSizes(columns);
		if (wcscmp(columns.c_str(), m_optionsForEdit.m_visibleColumnsList.c_str()) != 0)
		{
			ATLTRACE2(atlTraceDBProvider, 0, L"CMainFrame::SaveSettings: VisibleColumns='%s'\n", columns.c_str());
			m_optionsForEdit.m_visibleColumnsList = columns;
			m_view.Invalidate(FALSE);

			SetRegEntry(hOptionsKey, NULL, L"VisibleColumns", (WCHAR*)columns.c_str());

			StoreFontSetting(m_view, m_optionsForEdit.m_panel1FontHt, hOptionsKey, L"Panel1FontHt");
		}
	}

	if ((flags & OT_DETAILSFONT) != 0)
	{
		StoreFontSetting(m_details, m_optionsForEdit.m_DetailsFontHt, hOptionsKey, L"DetailsFontHt");
	}

	if ((flags & OT_EXPLORERFONT) != 0)
	{
		StoreFontSetting(m_explorer, m_optionsForEdit.m_ExplorerFontHt, hOptionsKey, L"ExplorerFontHt");
	}

	if ((flags & OT_FILTERFONT) != 0)
	{
		StoreFontSetting(m_filter, m_optionsForEdit.m_FilterFontHt, hOptionsKey, L"FilterFontHt");
	}

CLEANUP:
	CloseRegKey(hOptionsKey);

	if (bShowRestartMessage)
	{
		USES_XMSGBOXPARAMS
		__xmb.nTimeoutSeconds = 10;
		XMessageBox(m_hWnd, L"Please restart PGNProfiler for changes to make effect", PGNPTITLE, MB_OK | MB_ICONINFORMATION, &__xmb);
	}
}

static map<HWND, HFONT> g_AllocatedFontsMap;

// Change windows font size by specified number of points.
// Returns: new font size if success, otherwise - zero (indicating no change made).
LONG AlterWindowFont(ATL::CWindow window, LONG delta)
{
	LOGFONT lf;
	HFONT oldFont = window.GetFont();
	if (GetObject(oldFont, sizeof(LOGFONT), &lf))
	{
		LONG newHeight = lf.lfHeight + delta;
		if (newHeight < SMALLEST_FONT && newHeight > LARGEST_FONT)		// do not allow too small or too large font
		{
			lf.lfHeight = newHeight;

			HFONT newFont = CreateFontIndirect(&lf);
			window.SetFont(newFont);

			auto it = g_AllocatedFontsMap.find(window.m_hWnd);
			if (it != g_AllocatedFontsMap.end())
			{
				ATLTRACE2(atlTraceDBProvider, 0, L"AlterWindowFont: deleting previously allocated font %p\n", it->second);
				DeleteObject(it->second);

				it->second = newFont;
			}
			else
			{
				g_AllocatedFontsMap.insert(std::pair<HWND, HFONT>(window.m_hWnd, newFont));
			}
			return newHeight;
		}
	}

	return 0;	// no change made
}
