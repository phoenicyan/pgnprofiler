/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "PGNPOptions.h"
#include "OptimizerDlg.h"
#include "CCMenu.h"
#include "DropFileHandler.h"
#include "gui\MyHeader.h"

class CLoggerItemBase;

extern int FindSQLLOGColumn(const WCHAR* colName);

class CPGNProfilerView
	: public CWindowImpl<CPGNProfilerView, CListViewCtrl>
	, public CCustomDraw<CPGNProfilerView>
	, public CCMenu<CPGNProfilerView>
	, public CDropFilesHandler<CPGNProfilerView>
{
	CLoggerItemBase* m_pCurLogger;
	int m_prevTopIndex;				// used to recognize direction of SB_THUMBTRACK
	size_t m_numMessages;
	BOOL m_relativeTime;
	BOOL m_autoScroll;
	wstring m_userSQL;		// temporary buffer that holds readable SQL text (used in populating listview)
	HWND m_hMainWnd;
	CPGNPOptions& m_optionsForEdit;
	CColorPalette& m_palit;
	CMyHeader m_header;
	int m_columnMapping[20];		// the array length must be at least SQLLOG_COLUMNS_CNT
	WCHAR m_buffer[_CVTBUFSIZE];
	
	COptimizerDlg m_optimizerDlg;

public:
	DECLARE_WND_SUPERCLASS(_T("PGNPrfLV"), CListViewCtrl::GetWndClassName())

	CPGNProfilerView(CPGNPOptions& optionsForEdit, CColorPalette& palit) 
		: CCMenu<CPGNProfilerView>(palit)
		, m_pCurLogger(0), m_prevTopIndex(0), m_numMessages(MAXLONG), m_relativeTime(FALSE), m_autoScroll(TRUE)
		, m_optionsForEdit(optionsForEdit), m_palit(palit), m_header(palit)
		, m_optimizerDlg(palit)
	{
		for (int i=0; i < 20; i++)
			m_columnMapping[i] = i;	// default mapping
	}

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		pMsg;
		return FALSE;
	}

	BEGIN_MSG_MAP(CPGNProfilerView)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_HSCROLL, OnScroll)
		MESSAGE_HANDLER(WM_VSCROLL, OnScroll)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		//MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_CHAR, OnChar)
		MESSAGE_HANDLER(WM_GETDLGCODE, OnGetDlgCode)

		COMMAND_ID_HANDLER(ID_CLEAR_LOG, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_VIEW_OPTIONS, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_VIEW_ZOOMIN, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_VIEW_ZOOMOUT, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(IDM_OPTIMIZE, OnOptimize)

		NOTIFY_CODE_HANDLER(HDN_ITEMCHANGED, OnHeaderChanged)
		NOTIFY_CODE_HANDLER(HDN_ENDDRAG, OnHeaderDraged)

		CHAIN_MSG_MAP(CCMenu<CPGNProfilerView>)
		CHAIN_MSG_MAP(CDropFilesHandler<CPGNProfilerView>)
		CHAIN_MSG_MAP_ALT(CCustomDraw<CPGNProfilerView>, 1)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnSendCmdToMainWnd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOptimize(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHeaderChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
	LRESULT OnHeaderDraged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);

	DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/)
    {        
        return CDRF_NOTIFYITEMDRAW;
    }

    DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);

    LRESULT OnGetdispinfo(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

	///////////////////////////////////////////////////////////////////////////
	// Drag&drop
	BOOL IsReadyForDrop(void) 	{ /*ResetContent(); */ return TRUE; }

	BOOL HandleDroppedFile(LPCTSTR szBuff)
	{
		ATLTRACE("CPGNProfilerView::HandleDroppedFile: %s\n", szBuff);

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
	const BYTE* GetMessageData(size_t i) const;

	BOOL ShowMessages(CLoggerItemBase* pLogger);

	void Refresh();

	inline BOOL IsRelativeTime() { return m_relativeTime; }
	void SetRelativeTime(BOOL relativeTime);

	inline BOOL IsAutoScroll() { return m_autoScroll; }
	void SetAutoScroll(BOOL autoScroll);

	inline void SetMainWnd(HWND hWnd)
	{
		m_hMainWnd = hWnd;
	}

	CColorPalette& GetPalit() { return m_palit; }

	void GetColumnsSizes(wstring& columns);
	void CreateColumns();

	void SelectItems(list<DWORD_PTR>& dataList);

protected:
	const WCHAR* DisplaySQLmsg(int nSubItem, CProfSQLmsg& profmsg, map<DWORD, wstring>* pMapLogger2Appname);
	const WCHAR* DisplayERRmsg(int nSubItem, CProfERRORmsg& profmsg, map<DWORD, wstring>* pMapLogger2Appname);

	const WCHAR* FormatUserReadableSQL(const char* text);
	LRESULT InsertOptimizeMenuItem(HMENU hSubMenu);
	void ZoomFont(LONG delta);
	void DrawSelectedItem(const NMCUSTOMDRAW& nmcd);	// draw selected item differently

	COLORREF GetLineColor(TRC_TYPE trcType, SQL_QUERY_TYPE cmdType);
};

void PrintAbsTimeStamp(WCHAR* buffer, TRC_TIME timeStamp);
void PrintRelTimeStamp(WCHAR* buffer, TRC_TIME timeStamp, LONGLONG* initialTime);
