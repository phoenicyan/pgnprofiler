/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "../resource.h"
#include "../MainFrm.h"
#include "SkinWindow.h"
#include "../LikeRegexDlg.h"

template<class T>
void CSkinWindow<T>::SetWinState(WINSTATE state, WINSTATE prev_state, const RECT& r)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::SetWinState(srare=%d, prev_state=%d, r={%d,%d,%d,%d})\n", 
		state, prev_state, r.left, r.top, r.right, r.bottom);
	
	m_winstate = state;
	m_prev_winstate = prev_state;
	m_restoreWinRect = r;
}

template<class T>
LRESULT CSkinWindow<T>::OnSysCommand(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	UINT uCmdType = (UINT)wParam;

	bHandled = FALSE;

	if (m_palit.m_theme > CColorPalette::THEME_DEFAULT)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnSysCommand(uCmdType=%x, lParam='%c')\n", uCmdType, lParam);

		// MSDN: "If the wParam is SC_KEYMENU, lParam contains the character code of the key that is used with the ALT key to display the popup menu."
		if ((uCmdType & 0xFFF0) == SC_KEYMENU && lParam == ' ')
		{
			T* pT = (T*)this;

			CRect window_rect;
			pT->GetWindowRect(&window_rect);
			window_rect.top += GetSystemMetrics(SM_CYCAPTION);

			DisplaySysMenu(window_rect.TopLeft());

			bHandled = TRUE;
		}
	}

	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	T* pT = (T*)this;

	m_style = pT->GetWindowLong(GWL_STYLE);
	if (m_palit.m_theme > CColorPalette::THEME_DEFAULT)
		pT->SetWindowLong(GWL_STYLE, m_style & ~(WS_MINIMIZEBOX| WS_MAXIMIZEBOX| WS_SYSMENU));

	//LRESULT lRet = pT->DefWindowProc(uMsg, wParam, lParam);
	//return lRet;

	bHandled = FALSE;
	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	T* pT = (T*)this;

	if (m_palit.m_theme > CColorPalette::THEME_DEFAULT)
	{
		HRGN rgn = (HRGN)wParam;
		CRect window_rect, dirty_region;
		pT->GetWindowRect(&window_rect);

		// A value of 1 indicates paint all.
		if (!rgn || rgn == reinterpret_cast<HRGN>(1))
		{
			dirty_region = window_rect;
			dirty_region.OffsetRect(-dirty_region.left, -dirty_region.top);
		}
		else
		{
			CRect rgn_bounding_box;
			GetRgnBox(rgn, &rgn_bounding_box);
			if (!IntersectRect(&dirty_region, &rgn_bounding_box, &window_rect))
			{
				bHandled = FALSE;
				return 1;  // Dirty region doesn't intersect window bounds, bail.
			}

			// rgn_bounding_box is in screen coordinates. Map it to window coordinates.
			dirty_region.OffsetRect(-window_rect.left, -window_rect.top);
		}

		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcPaint(dirty=%d, %d)\n", dirty_region.Width(), dirty_region.Height());

		m_palit.DrawFrame(pT->m_hWnd, dirty_region.Width(), dirty_region.Height(), PopulateBtnState(), m_isActive, m_winstate == MAXIMIZED ? TRUE : FALSE);
	}
	else
	{
		bHandled = FALSE;
	}

	return 0;
}

template<class T>
BOOL CSkinWindow<T>::MaximizeWindow()
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::MaximizeWindow()\n");

	T* pT = (T*)this;
	//RECT r;
	//SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&r, 0);
	pT->GetWindowRect(&m_restoreWinRect);

	m_winstate = MAXIMIZED;
	//pT->MoveWindow(r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
	//pT->UpdateWindow();

	pT->ShowWindow(SW_MAXIMIZE);

	return TRUE;
}

template<class T>
BOOL CSkinWindow<T>::MinimizeWindow()
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::MinimizeWindow()\n");

	T* pT = (T*)this;

	m_prev_winstate = m_winstate;
	m_winstate = MINIMIZED;
	pT->ShowWindow(SW_MINIMIZE);

	return TRUE;
}

template<class T>
BOOL CSkinWindow<T>::RestoreWindow()
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::RestoreWindow()\n");

	T* pT = (T*)this;

	if (m_winstate == MAXIMIZED)
	{
		pT->MoveWindow(m_restoreWinRect.left, m_restoreWinRect.top, m_restoreWinRect.Width(), m_restoreWinRect.Height(), TRUE);
		m_winstate = NORMAL;
		pT->UpdateWindow();
	}

	return TRUE;
}

template<class T>
BYTE CSkinWindow<T>::CalcState(UINT btn)
{
	if (m_downHitTest == btn)
		return 1;
	
	if (m_moveHitTest == btn)
		return (m_mousedown == btn) ? 1 : 2;
	
	return 0;
}

template<class T>
BYTE* CSkinWindow<T>::PopulateBtnState()
{
	m_btnState[0] = CalcState(HTCLOSE);
	m_btnState[1] = Maxable ? CalcState(HTMAXBUTTON) : 255;
	m_btnState[2] = Minable ? CalcState(HTMINBUTTON) : 255;
	m_btnState[3] = SysMenu ? 0 : 255;

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::PopulateBtnState(downHT=%d, moveHTt=%d), sysmenu=%d, min=%d, max=%d, close=%d\n",
		m_downHitTest, m_moveHitTest, m_btnState[3], m_btnState[2], m_btnState[1], m_btnState[0]);

	return &m_btnState[0];
}

static void MakeClientRect(RECT* windowRect, bool menuBar)
{
	const int cxFrame = 4;//GetSystemMetrics(SM_CXFRAME);
	const int cyFrame = 4;//GetSystemMetrics(SM_CYFRAME);
	const int cxBorder = GetSystemMetrics(SM_CXBORDER) + cxFrame;
	const int cyBorder = GetSystemMetrics(SM_CYBORDER) + cyFrame;
	const int bmHeight = GetSystemMetrics(SM_CYCAPTION) /*- 2 * GetSystemMetrics(SM_CYBORDER)*/;

	windowRect->left += cxBorder;
	windowRect->right -= cxBorder;
	windowRect->top += cyBorder + bmHeight;
	if (menuBar)
	{
		const int cyMenu = GetSystemMetrics(SM_CYMENU);
		windowRect->top += cyMenu;
	}

	windowRect->bottom -= cyBorder;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcCalcSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	T* pT = (T*)this;

	RECT* pRect = wParam ? &((LPNCCALCSIZE_PARAMS)lParam)->rgrc[0] : (RECT*)lParam;

	ATLTRACE2(atlTraceDBProvider, 0, "CSkinWindow::OnNcCalcSize(%d, %d, %d, %d)\n", pRect->left, pRect->top, pRect->right, pRect->bottom);

	MakeClientRect(pRect, pT->GetMenu() != NULL);

	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcActivate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcActivate(%x)\n", wParam);

	if (m_palit.m_theme > CColorPalette::THEME_DEFAULT)
	{
		m_isActive = (BOOL)wParam;

		OnNcPaint(uMsg, wParam, lParam, bHandled);
	}

	// return TRUE to direct the system to complete the change of active window
	return TRUE;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcHitTest(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	if (m_palit.m_theme == CColorPalette::THEME_DEFAULT)
	{
		bHandled = FALSE;
		return 0;
	}
	
	T* pT = (T*)this;
	CPoint point(LOWORD(lParam), HIWORD(lParam));

	CRect WinRect, rect;
	pT->GetWindowRect(&WinRect);

	int width = WinRect.right - WinRect.left;
	int height = WinRect.bottom - WinRect.top;

	point.x -= WinRect.left;
	point.y -= WinRect.top;

	rect = m_palit.GetButtonRect(pT->m_hWnd, 0);
	if (PtInRect(&rect, point))
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTCLOSE\n", point.x, point.y);
		return HTCLOSE;
	}

	rect = m_palit.GetButtonRect(pT->m_hWnd, 1);
	if (PtInRect(&rect, point) && Maxable)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTMAXBUTTON\n", point.x, point.y);
		return HTMAXBUTTON;
	}

	rect = m_palit.GetButtonRect(pT->m_hWnd, 2);
	if (PtInRect(&rect, point) && Minable)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTMINBUTTON\n", point.x, point.y);
		return HTMINBUTTON;
	}

	rect = m_palit.GetButtonRect(pT->m_hWnd, 3);
	if (PtInRect(&rect, point) && SysMenu)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTSYSMENU\n", point.x, point.y);
		return HTSYSMENU;
	}

	const int cxBorder = GetSystemMetrics(SM_CXBORDER);
	const int cyBorder = GetSystemMetrics(SM_CYBORDER);
	const int bmHeight = GetSystemMetrics(SM_CYCAPTION) /*- 2 * cyBorder*/;
	const int cx = GetSystemMetrics(SM_CXSMICON);
	const int cy = GetSystemMetrics(SM_CYSMICON);
	const int cyMenu = GetSystemMetrics(SM_CYMENU);

	SetRect(&rect, -2, -2, cx + 2, cy + 2);
	if (PtInRect(&rect, point) && Sizable && m_winstate != MAXIMIZED)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTTOPLEFT\n", point.x, point.y);
		return HTTOPLEFT;
	}

	SetRect(&rect, width - cx - 2,  -2, width + 2, cy + 2);
	if (PtInRect(&rect, point) && Sizable && m_winstate != MAXIMIZED)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTTOPRIGHT\n", point.x, point.y);
		return HTTOPRIGHT;
	}

	SetRect(&rect, -2, height - cy - 2, cx + 2, height + 2);
	if (PtInRect(&rect, point) && Sizable && m_winstate != MAXIMIZED)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTBOTTOMLEFT\n", point.x, point.y);
		return HTBOTTOMLEFT;
	}

	SetRect(&rect, width - cx - 2, height - cy - 2, width + 2, height + 2);
	if (PtInRect(&rect, point) && Sizable && m_winstate != MAXIMIZED)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTBOTTOMRIGHT\n", point.x, point.y);
		return HTBOTTOMRIGHT;
	}

	SetRect(&rect, -2, cy, cxBorder + 2, height - cy);
	if (PtInRect(&rect, point) && Sizable && m_winstate != MAXIMIZED)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTLEFT\n", point.x, point.y);
		return HTLEFT;
	}

	SetRect(&rect, width - cxBorder - 2, cy, width + 2, height - cy);
	if (PtInRect(&rect, point) && Sizable && m_winstate != MAXIMIZED)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTRIGHT\n", point.x, point.y);
		return HTRIGHT;
	}

	SetRect(&rect, cx, height - cyBorder - 2, width - cx, height + 2);
	if (PtInRect(&rect, point) && Sizable && m_winstate != MAXIMIZED)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTBOTTOM\n", point.x, point.y);
		return HTBOTTOM;
	}

	SetRect(&rect, cx, -2, width - cx, cyBorder + 2);
	if (PtInRect(&rect, point) && Sizable && m_winstate != MAXIMIZED)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTTOP\n", point.x, point.y);
		return HTTOP;
	}

	SetRect(&rect, cxBorder + 1, cyBorder + 1, width - cxBorder - 2, bmHeight + cyBorder + 2);
	if (PtInRect(&rect, point))
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTCAPTION\n", point.x, point.y);
		return HTCAPTION;
	}

	SetRect(&rect, cxBorder + 1, bmHeight + cyBorder + 2, width - cxBorder - 2, bmHeight + cyBorder + 2 + cyMenu);
	if (PtInRect(&rect, point))
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTMENU\n", point.x, point.y);
		return HTMENU;
	}

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcHitTest(%d, %d) -> HTCLIENT\n", point.x, point.y);
	
	if (m_active_tracking)
	{
		TrackMouseEvents(TME_CANCEL);
		m_moveHitTest = m_oldHitTest = 0;
		OnNcPaint(WM_NCPAINT, 0/*wParam*/, 0/*lParam*/, bHandled);
	}

	return HTCLIENT;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcLButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	T* pT = (T*)this;

	if (m_palit.m_theme > CColorPalette::THEME_DEFAULT)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcLButtonUp(wParam=%d)\n", wParam);

		if (wParam == HTCLOSE)
		{
			pT->PostMessage(WM_CLOSE, 0, 0);
			return 0;
		}

		if (wParam == HTMINBUTTON)
		{
			MinimizeWindow();
		}
		else if (wParam == HTMAXBUTTON)
		{
			if (m_winstate == MAXIMIZED)
				RestoreWindow();
			else
				MaximizeWindow();
		}
		else
		{
			return 0;
		}

		m_downHitTest = 0;
		m_moveHitTest = 0;
		m_mousedown = 0;

		pT->OnNcPaint(uMsg, 0/*wParam*/, 0/*lParam*/, bHandled);
	}
	else
	{
		bHandled = FALSE;
	}

	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	T* pT = (T*)this;

	if (m_palit.m_theme > CColorPalette::THEME_DEFAULT)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcLButtonDown(wParam=%d)\n", wParam);

		m_downHitTest = (UINT)wParam;
		m_moveHitTest = m_downHitTest;

		pT->OnNcPaint(uMsg, 0/*wParam*/, 0/*lParam*/, bHandled);

		if ((wParam >= HTLEFT && wParam <= HTBOTTOMRIGHT) || (wParam == HTCAPTION && m_winstate != MAXIMIZED) || wParam == HTMENU)
		{
			bHandled = FALSE;
			return 0;
		}

		UINT uDblclkTime = GetDoubleClickTime();
		pT->SetTimer(23456, uDblclkTime + 100);
	}
	else
	{
		bHandled = FALSE;
	}

	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcLButtonDblClk(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	T* pT = (T*)this;

	if (m_palit.m_theme > CColorPalette::THEME_DEFAULT)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcLButtonDblClk(wParam=%d)\n", wParam);

		if (wParam == HTCAPTION && pT->Sizable)
		{
			if (m_winstate == MAXIMIZED)
				RestoreWindow();
			else
				MaximizeWindow();

			m_downHitTest = 0;
			m_moveHitTest = 0;
			pT->OnNcPaint(uMsg, 0, 0, bHandled);
		}
		else if (wParam == HTSYSMENU && pT->SysMenu)
		{
			m_downHitTest = 0;
			m_moveHitTest = 0;

			pT->PostMessage(WM_CLOSE, 0, 0);
		}
	}
	else
	{
		bHandled = FALSE;
	}

	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcRButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	T* pT = (T*)this;

	if (m_palit.m_theme > CColorPalette::THEME_DEFAULT)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcRButtonUp(wParam=%d)\n", wParam);

		CPoint point(LOWORD(lParam), HIWORD(lParam));
		if (HTCAPTION == wParam)
		{
			DisplaySysMenu(point);
		}
	}
	else
	{
		bHandled = FALSE;
	}

	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcRButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	T* pT = (T*)this;

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcRButtonDown(wParam=%d)\n", wParam);

	//bHandled = FALSE;

	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (m_palit.m_theme == CColorPalette::THEME_DEFAULT)
	{
		bHandled = FALSE;
		return 0;
	}
	
	T* pT = (T*)this;

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcMouseMove(m_moveHitTest=wParam=%d)\n", wParam);

	if ((wParam >= HTLEFT && wParam <= HTBOTTOMRIGHT) || (wParam == HTCAPTION && m_winstate != MAXIMIZED))
	{
		bHandled = FALSE;
		return 0;
	}

	m_moveHitTest = (UINT)wParam;
	m_downHitTest = 0;

	//WM_MOUSEFIRST
	//WM_MOUSELAST

	if (wParam == HTCLOSE || wParam == HTMINBUTTON || wParam == HTMAXBUTTON)
		TrackMouseEvents(TME_NONCLIENT | TME_LEAVE);

	if (m_oldHitTest != wParam) {
		OnNcPaint(uMsg, 0/*wParam*/, 0/*lParam*/, bHandled);
		m_oldHitTest = (UINT)wParam;
	}

	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnNcMouseLeave(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (m_palit.m_theme == CColorPalette::THEME_DEFAULT)
	{
		bHandled = FALSE;
		return 0;
	}

	T* pT = (T*)this;

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnNcMouseLeave()\n");

	if (m_active_tracking)
	{
		TrackMouseEvents(TME_CANCEL);
		m_moveHitTest = m_oldHitTest = 0;
		OnNcPaint(WM_NCPAINT, 0/*wParam*/, 0/*lParam*/, bHandled);
	}

	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ATLTRACE2(atlTraceDBProvider, 0, "CSkinWindow::OnTimer(id=%d)\n", wParam);

	T* pT = (T*)this;
	pT->KillTimer(wParam);

	//CRect rect(m_palit.GetButtonRect(pT->m_hWnd, 3));
	CRect rect;
	pT->GetWindowRect(&rect);

	switch (m_downHitTest)
	{
	case HTSYSMENU:
		{
			CPoint point(rect.left, rect.top + 24);
			DisplaySysMenu(point);
			break;
		}
	case HTCLOSE:
	case HTMINBUTTON:
	case HTMAXBUTTON:
	default:
		break;
	}

	return 0;
}

template<class T>
LRESULT CSkinWindow<T>::OnSysMenuCommand(WORD wNotifyCode, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	T* pT = (T*)this;

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::OnSysMenuCommand(wNotifyCode=%d, wID=%d)\n", wNotifyCode, wID);

	switch (wID)
	{
	case ID_SC_CLOSE:
		pT->PostMessage(WM_CLOSE, 0, 0);
		break;

	case ID_SC_MINIMIZE:
		MinimizeWindow();
		break;
	
	case ID_SC_MAXIMIZE:
	case ID_SC_RESTORE:
		if (m_winstate == MAXIMIZED)
			RestoreWindow();
		else
			MaximizeWindow();
		break;
	}

	return 0;
}

template<class T>
void CSkinWindow<T>::TrackMouseEvents(DWORD mouse_tracking_flags)
{
	T* pT = (T*)this;

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::TrackMouseEvents(%d), m_active_tracking=%d\n", mouse_tracking_flags, m_active_tracking);

	// Begin tracking mouse events for this HWND so that we get WM_MOUSELEAVE
	// when the user moves the mouse outside this HWND's bounds.
	if (m_active_tracking == 0 || (mouse_tracking_flags & TME_CANCEL) != 0)
	{
		if (mouse_tracking_flags & TME_CANCEL)
		{
			// We're about to cancel active mouse tracking, so empty out the stored state.
			m_active_tracking = 0;
		}
		else
		{
			m_active_tracking = mouse_tracking_flags;
		}

		TRACKMOUSEEVENT tme;
		tme.cbSize = sizeof(tme);
		tme.dwFlags = mouse_tracking_flags;
		tme.hwndTrack = pT->m_hWnd;
		tme.dwHoverTime = 0;
		TrackMouseEvent(&tme);
	}
	else if (mouse_tracking_flags != m_active_tracking)
	{
		TrackMouseEvents(m_active_tracking | TME_CANCEL);
		TrackMouseEvents(mouse_tracking_flags);
	}
}

template<class T>
void CSkinWindow<T>::DisplaySysMenu(const POINT& point)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinWindow::DisplaySysMenu()\n");

	T* pT = (T*)this;

	HMENU hMenu = ::LoadMenu(_Module.GetResourceInstance(), (LPCTSTR)IDR_SYSTEM_MENU);
	if (hMenu != NULL)
	{
		HMENU hSubMenu = ::GetSubMenu(hMenu, 0);
		if (hSubMenu != NULL)
		{
			::SetMenuDefaultItem(hSubMenu, ID_SC_CLOSE, FALSE);
			::EnableMenuItem(hSubMenu, m_winstate != MAXIMIZED ? ID_SC_RESTORE : ID_SC_MAXIMIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			::TrackPopupMenu(hSubMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON, point.x, point.y, 0, pT->m_hWnd, NULL);

			::PostMessage(pT->m_hWnd, WM_NULL, 0, 0);
		}

		::DestroyMenu(hMenu);
	}
}

// Explicitly request instantiation of CSkinWindow class to prevent LNK2019 error.
template CSkinWindow<CAboutDlg>;
template CSkinWindow<CConnectHostDlg>;
template CSkinWindow<CLikeRegexDlg>;
template CSkinWindow<CMainFrame>;
template CSkinWindow<COptimizerDlg>;
template CSkinWindow<COptionsDlg>;
