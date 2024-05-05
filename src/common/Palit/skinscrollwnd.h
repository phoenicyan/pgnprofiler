/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"
#include "SkinScrollBar.h"

class CLimitWindow : public CWindowImpl<CLimitWindow>
{
	CColorPalette& m_palit;

public:
	CLimitWindow(CColorPalette& palit);
	
	BOOL RegisterClass();

protected:
	BEGIN_MSG_MAP(CLimitWindow)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC,    OnCtlColor) // For static and read only edit.
		MESSAGE_HANDLER(WM_CTLCOLOREDIT,      OnCtlColor) // For edit boxes
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX,   OnCtlColor) // List and combo.
		COMMAND_RANGE_HANDLER(32769, 32769 + 200, OnSendCmdToMainWnd)
	END_MSG_MAP()

	LRESULT OnCtlColor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSendCmdToMainWnd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};

/////////////////////////////////////////////////////////////////////////////
// CSkinScrollWnd window

class CSkinScrollWnd : public CWindowImpl<CSkinScrollWnd>
{
// Construction
public:
	CSkinScrollWnd(CColorPalette& palit);
	BOOL RegisterClass();

// Attributes
public:
	int			m_nScrollWid;
	CSkinScrollBar	m_sbHorz,m_sbVert;
	CLimitWindow	m_wndLimit;
	HBITMAP		m_hBmpScroll;
	BOOL		m_bOp;
	int			m_nAngleType;
// Operations
public:
	WNDPROC		m_funOldProc;
// Overrides
protected:

// Implementation
public:
	virtual ~CSkinScrollWnd();

	BOOL SkinWindow(CWindow pWnd, HBITMAP hBmpScroll);
	void SetBmpScroll(HBITMAP hBmpScroll);

	// Generated message map functions
protected:
	BEGIN_MSG_MAP(CSkinScrollWnd)
		//MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_VSCROLL, OnVScroll)
		MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(MYWM_DESTMOVE, OnDestMove)
		//DEFAULT_REFLECTION_HANDLER()
		//REFLECT_NOTIFICATIONS()
	END_MSG_MAP()


	LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnVScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnHScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
};

extern "C" BOOL UnskinWndScroll(CWindow pWnd);

extern "C" CSkinScrollWnd* SkinWndScroll(CWindow pWnd, CColorPalette& palit);

/////////////////////////////////////////////////////////////////////////////
