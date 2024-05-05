/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "LoggerItem.h"
#include "CCMenu.h"
#include "ColorPalette.h"
#include "DropFileHandler.h"

class CFilterView
	: public CWindowImpl<CFilterView, CEdit>
	, public CCMenu<CFilterView>
	, public CDropFilesHandler<CFilterView>
{
public:
	DECLARE_WND_SUPERCLASS(_T("PGNPfilter"), CEdit::GetWndClassName())

	CColorPalette& m_palit;
	bool m_filterOn;
	HWND m_hMainWnd;

	CFilterView(CColorPalette& palit) : CCMenu<CFilterView>(palit), m_palit(palit), m_filterOn(false)
	{
	}
	
	BOOL Populate();

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP(CFilterView)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		//MESSAGE_HANDLER(WM_NCCREATE, OnNCCreate)
		//MESSAGE_HANDLER(WM_NCPAINT, OnNCPaint)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_GETDLGCODE, OnGetDlgCode)
		MESSAGE_HANDLER(WM_CHAR, OnChar)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_EDIT_PASTE, OnEditPaste)
		COMMAND_ID_HANDLER(ID_FILTER, OnSendCmdToMainWnd)
		COMMAND_ID_HANDLER(ID_PRECONFIGUREDFILTERS_CLENTSQLLIKE, OnPreconfiguredFilter)
		COMMAND_ID_HANDLER(ID_PRECONFIGUREDFILTERS_SHOWERRORS, OnPreconfiguredFilter)
		COMMAND_ID_HANDLER(ID_PRECONFIGUREDFILTERS_HIDESYSTEM, OnPreconfiguredFilter)
		COMMAND_ID_HANDLER(ID_PRECONFIGUREDFILTERS_EXECUTETIME, OnPreconfiguredFilter)
		COMMAND_ID_HANDLER(ID_PRECONFIGUREDFILTERS_SCHEMAALTERATIONS, OnPreconfiguredFilter)
		COMMAND_ID_HANDLER(ID_PRECONFIGUREDFILTERS_STOREDPROCEDURESANDNOTIFICATIONS, OnPreconfiguredFilter)
		COMMAND_ID_HANDLER(ID_VIEW_ZOOMIN, OnZoomin)
		COMMAND_ID_HANDLER(ID_VIEW_ZOOMOUT, OnZoomout)
		CHAIN_MSG_MAP(CCMenu<CFilterView>)
		CHAIN_MSG_MAP(CDropFilesHandler<CFilterView>)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	//LRESULT OnNCCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	//LRESULT OnNCPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);

	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditPaste(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnSendCmdToMainWnd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPreconfiguredFilter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnZoomin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnZoomout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	///////////////////////////////////////////////////////////////////////////
	// Drag&drop
	BOOL IsReadyForDrop(void) 	{ /*ResetContent(); */ return TRUE; }

	BOOL HandleDroppedFile(LPCTSTR szBuff)
	{
		ATLTRACE("CFilterView::HandleDroppedFile: %s\n", szBuff);

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
};
