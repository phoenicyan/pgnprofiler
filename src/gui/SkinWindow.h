/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

template <class T>
class CSkinWindow
{
public:
	enum WINSTATE
	{
		NORMAL,
		MAXIMIZED,
		MINIMIZED,
	};
	
private:
	WINSTATE m_winstate, m_prev_winstate;

	UINT m_oldHitTest, m_moveHitTest, m_downHitTest, m_mousedown;
	BYTE m_btnState[4];		// index 0 - close, 1 - maximize, 2 - minimize, 3 - reserved; value 0 - normal state, 1 - hover, 2 - pressed, 255 - invisible

	CColorPalette& m_palit;

	DWORD m_style;
	DWORD m_active_tracking;
	BOOL m_isActive;

	bool getSizable() { return m_style & WS_SIZEBOX; }
	bool getMaxable() { return m_style & WS_MAXIMIZEBOX; }
	bool getMinable() { return m_style & WS_MINIMIZEBOX; }
	bool getSysMenu() { return m_style & WS_SYSMENU; }

	CRect m_restoreWinRect;

public:
	CSkinWindow<T>(CColorPalette& palit)
		: m_isActive(FALSE)
		, m_palit(palit)
		, m_style(0)
		, m_active_tracking(0)
		, Sizable(this)
		, Minable(this)
		, Maxable(this)
		, SysMenu(this)
		, m_winstate(NORMAL)
		, m_prev_winstate(NORMAL)
		, m_oldHitTest(0), m_moveHitTest(0), m_downHitTest(0), m_mousedown(0)
	{
		memset(m_btnState, 0, sizeof(m_btnState));
	}

	~CSkinWindow()
	{
	}

	void SetWinState(WINSTATE state, WINSTATE prev_state, const RECT& r);

	cpp_property::ROProperty<bool, CSkinWindow<T>, &CSkinWindow<T>::getSizable> Sizable;

	cpp_property::ROProperty<bool, CSkinWindow<T>, &CSkinWindow<T>::getMaxable> Maxable;

	cpp_property::ROProperty<bool, CSkinWindow<T>, &CSkinWindow<T>::getMinable> Minable;

	cpp_property::ROProperty<bool, CSkinWindow<T>, &CSkinWindow<T>::getSysMenu> SysMenu;

	BEGIN_MSG_MAP(CSkinWindow)
		MESSAGE_HANDLER(WM_SYSCOMMAND, OnSysCommand)
		MESSAGE_HANDLER(WM_NCCREATE, OnNcCreate)
		MESSAGE_HANDLER(WM_NCPAINT, OnNcPaint)
		MESSAGE_HANDLER(WM_NCCALCSIZE, OnNcCalcSize)
		MESSAGE_HANDLER(WM_NCACTIVATE, OnNcActivate)
		MESSAGE_HANDLER(WM_NCHITTEST, OnNcHitTest)
		MESSAGE_HANDLER(WM_NCLBUTTONUP, OnNcLButtonUp)
		MESSAGE_HANDLER(WM_NCLBUTTONDOWN, OnNcLButtonDown)
		MESSAGE_HANDLER(WM_NCLBUTTONDBLCLK, OnNcLButtonDblClk)
		MESSAGE_HANDLER(WM_NCRBUTTONUP, OnNcRButtonUp)
		MESSAGE_HANDLER(WM_NCRBUTTONDOWN, OnNcRButtonDown)
		MESSAGE_HANDLER(WM_NCMOUSEMOVE, OnNcMouseMove)
		MESSAGE_HANDLER(WM_NCMOUSELEAVE, OnNcMouseLeave)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)

		COMMAND_ID_HANDLER(ID_SC_CLOSE, OnSysMenuCommand)
		COMMAND_ID_HANDLER(ID_SC_MINIMIZE, OnSysMenuCommand)
		COMMAND_ID_HANDLER(ID_SC_MAXIMIZE, OnSysMenuCommand)
		COMMAND_ID_HANDLER(ID_SC_RESTORE, OnSysMenuCommand)
	END_MSG_MAP()

	LRESULT OnSysCommand(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcCreate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcPaint(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcCalcSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcActivate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcHitTest(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcLButtonUp(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcLButtonDblClk(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcRButtonUp(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcRButtonDown(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcMouseMove(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNcMouseLeave(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSysMenuCommand(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	BOOL MaximizeWindow();
	BOOL MinimizeWindow();
	BOOL RestoreWindow();
	BYTE CalcState(UINT btn);
	BYTE* PopulateBtnState();
	void TrackMouseEvents(DWORD mouse_tracking_flags);
	void DisplaySysMenu(const POINT& point);
};
