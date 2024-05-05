/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "PGNPOptions.h"
#include "LoggerItem.h"
#include "CCMenu.h"
#include "ColorPalette.h"
#include "DropFileHandler.h"

class CExplorerView
	: public CWindowImpl<CExplorerView, CTreeViewCtrl>
	, public CCustomDraw<CExplorerView>
	, public CCMenu<CExplorerView>
	, public CDropFilesHandler<CExplorerView>
{
	CHostLoggerItem m_rootLogger;
	CLoggerItemBase* m_pCurLogger;
	list<CLoggerItemBase*> m_remoteHosts;		// list of CHostLoggerItem
	list<CLoggerItemBase*> m_logFiles;
	CColorPalette& m_palit;
	HWND m_hMainWnd;

public:
	DECLARE_WND_SUPERCLASS(_T("PGNProfilerTV"), CTreeViewCtrl::GetWndClassName())

	CExplorerView(CPGNPOptions& options, CColorPalette& palit)
		: CCMenu<CExplorerView>(palit)
		, m_rootLogger(&options.m_sWorkPath), m_pCurLogger(&m_rootLogger), m_palit(palit)
	{
		m_rootLogger.SetName(L"localhost (127.0.0.1)");
	}
	
	//BOOL Select(CLoggerItem* pBlock);
	BOOL Populate();
	//BOOL RemoveItem(CLoggerItem* pBlock);

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP(CExplorerView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		//MESSAGE_HANDLER(WM_NCPAINT, OnNCPaint)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_RBUTTONDOWN, OnContextMenu)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_CHAR, OnChar)
		MESSAGE_HANDLER(WM_GETDLGCODE, OnGetDlgCode)
		COMMAND_ID_HANDLER(ID_CONNECTTOHOST, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_CAPTURE, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_CLEAR_LOG, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_REFRESH, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_TRACELEVEL_SQLSTATEMENTSONLY, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_TRACELEVEL_COMMENTLEVEL1, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_TRACELEVEL_COMMENTLEVEL2, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_TRACELEVEL_COMMENTLEVEL3, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_VIEW_ZOOMIN, OnZoomin)
		COMMAND_ID_HANDLER(ID_VIEW_ZOOMOUT, OnZoomout)
		CHAIN_MSG_MAP(CCMenu<CExplorerView>)
		CHAIN_MSG_MAP(CDropFilesHandler<CExplorerView>)
		CHAIN_MSG_MAP_ALT(CCustomDraw<CExplorerView>, 1)
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	//LRESULT OnNCPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnSendCmdToMainWnd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnZoomin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnZoomout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	///////////////////////////////////////////////////////////////////////////
	// Drag&drop
	BOOL IsReadyForDrop(void) 	{ /*ResetContent(); */ return TRUE; }

	BOOL HandleDroppedFile(LPCTSTR szBuff)
	{
		ATLTRACE("CExplorerView::HandleDroppedFile: %s\n", szBuff);

		size_t cch = _tcslen(szBuff);
		LPTSTR szBuffCopy = new TCHAR[cch + 1];
		_tcscpy(szBuffCopy, szBuff);
		::PostMessage(m_hMainWnd, MYMSG_DRAGDROP_FILE, (WPARAM)szBuffCopy, (LPARAM)0);

		// Return TRUE unless you're done handling files (e.g., if you want 
		// to handle only the first relevant file, and you have already found it).
		return TRUE;
	}

	void EndDropFiles(void)
	{
		// Sometimes, if your file handling is not trivial,  you might want to add all
		// file names to some container (std::list<CString> comes to mind), and do the 
		// handling afterwards, in a worker thread. 
		// If so, use this function to create your worker thread.
	}

	///////////////////////////////////////////////////////////////////////////
	inline void SetMainWnd(HWND hWnd)
	{
		m_hMainWnd = hWnd;
	}
	
	CColorPalette& GetPalit() { return m_palit; }

	DWORD OnPrePaint(int idCtrl, LPNMCUSTOMDRAW lpNMCustomDraw)
	{
		return CDRF_NOTIFYITEMDRAW;		// We need per-item notifications
	}

	DWORD OnItemPrePaint(int idCtrl, LPNMCUSTOMDRAW lpNMCustomDraw);

	//LRESULT OnGetdispinfo(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

	CHostLoggerItem& GetRootLogger() { return m_rootLogger; }
	CLoggerItemBase* GetCurrentLoggerPtr() { return m_pCurLogger; }
	void SetCurrentLoggerPtr(CLoggerItemBase* pLogger) { m_pCurLogger = pLogger; }

	int AddLogFile(CLoggerItemBase* pLogFile);
	void RemoveClearedLogFiles();

	bool IsLogFileOpen(LPCTSTR lpFileName);
	
	int AddHost(CHostLoggerItem* pHost);
	bool IsHostConnected(LPCTSTR lpHostName);
	size_t GetHostsCount() const { return m_remoteHosts.size(); }
	CHostLoggerItem* FindHost(HANDLE hPipe);
	void CancelFilters();

private:
	CLoggerItemBase* FindLogger(const list<CLoggerItemBase*>& loggers, const WCHAR* strText);

	HTREEITEM _PopulateTree(const CLoggerItemBase* pBlock, HTREEITEM hParent);
	void _UpdateHostIcon(const CLoggerItemBase* pBlock, HTREEITEM hParent);
	bool _CleanupItem(HTREEITEM hItem, bool& bLoggerSelected);
};
