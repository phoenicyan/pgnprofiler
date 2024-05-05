/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

/////////////////////////////////////////////////////////////////////////////
// CSkinScrollBar window
class CSkinScrollBar : public CWindowImpl<CSkinScrollBar, CScrollBar>
{
// Construction
public:
	DECLARE_WND_SUPERCLASS(_T("skin_scrollbar"), CScrollBar::GetWndClassName())

	CSkinScrollBar();
	HBITMAP	m_hBmp;
	int		m_nWid;
	int		m_nFrmHei;
	int		m_nHei;

	SCROLLINFO	m_si;
	BOOL		m_bDrag;
	CPoint		m_ptDrag;
	int			m_nDragPos;

	UINT		m_uClicked;
	BOOL		m_bNotify;
	UINT		m_uHtPrev;
	BOOL		m_bPause;
	BOOL		m_bTrace;
// Attributes
public:

// Operations
public:

// Overrides

// Implementation
public:
	void DrawArrow(UINT uArrow,int nState);
	void SetBitmap(HBITMAP hBmp);
	BOOL IsVertical();
	virtual ~CSkinScrollBar();

	// Generated message map functions
protected:
	void DrawThumb(HDC hDestDC, RECT *pDestRect, CDC *pSourDC, RECT *pSourRect);
	void TileBlt(HDC hDestDC,RECT *pDestRect,CDC *pSourDC,RECT *pSourRect);
	RECT GetRect(UINT uSBCode);
	RECT GetImageRect(UINT uSBCode,int nState=0);
	UINT HitTest(CPoint pt);

	BEGIN_MSG_MAP(CSkinScrollBar)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
		//WindowProc:
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClk)
		MESSAGE_HANDLER(SBM_SETSCROLLINFO, OnSetScrollInfo)
		MESSAGE_HANDLER(SBM_GETSCROLLINFO, OnGetScrollInfo)
		//
		//DEFAULT_REFLECTION_HANDLER()
		//REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);	
	LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnLButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseLeave(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnLButtonDblClk(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnSetScrollInfo(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnGetScrollInfo(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
};

/////////////////////////////////////////////////////////////////////////////
