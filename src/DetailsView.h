/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once
#include "atlwin.h"
#include "CCMenu.h"
#include "ColorPalette.h"
#include "DropFileHandler.h"

class CDetailsView
	: public CWindowImpl<CDetailsView, CEdit>
	, public CCMenu<CDetailsView>
	, public CDropFilesHandler<CDetailsView>
{
	wstring m_text;		// buffer for formatted item text
	HWND m_hMainWnd;
	CColorPalette& m_palit;

public:
	DECLARE_WND_SUPERCLASS(_T("PGNPdetails"), CEdit::GetWndClassName())

	CDetailsView(CColorPalette& palit) : CCMenu<CDetailsView>(palit), m_palit(palit)
	{
	}
	
	~CDetailsView(void)
	{
	}

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP(CDetailsView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER(WM_CHAR, OnChar)
		MESSAGE_HANDLER(WM_GETDLGCODE, OnGetDlgCode)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_ZOOMIN, OnZoomin)
		COMMAND_ID_HANDLER(ID_VIEW_ZOOMOUT, OnZoomout)
		CHAIN_MSG_MAP(CCMenu<CDetailsView>)
		CHAIN_MSG_MAP(CDropFilesHandler<CDetailsView>)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnZoomin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnZoomout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	///////////////////////////////////////////////////////////////////////////
	// Drag&drop
	BOOL IsReadyForDrop(void) 	{ /*ResetContent(); */ return TRUE; }

	BOOL HandleDroppedFile(LPCTSTR szBuff)
	{
		ATLTRACE("CDetailsView::HandleDroppedFile: %s\n", szBuff);

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
	void ShowSQL(const list<DWORD_PTR>& dataList);

	inline void SetMainWnd(HWND hWnd)
	{
		m_hMainWnd = hWnd;
	}

	CColorPalette& GetPalit() { return m_palit; }

protected:
	const WCHAR* FormatItemText(const WCHAR* txt);
};
