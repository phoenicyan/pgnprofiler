/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "SkinScrollWnd.h"

#define SKINSCROLLBAR_CLASSNAME _T("SkinSBclass")
#define WNDLIMIT_CLASSNAME _T("WndLimitClass")
#define TIMER_UPDATE 100

static LRESULT CALLBACK
HookWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	CSkinScrollWnd *pSkin=(CSkinScrollWnd*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
	if(msg==WM_DESTROY)
	{
		WNDPROC procOld=pSkin->m_funOldProc;
		UnskinWndScroll(hwnd);
		return ::CallWindowProc(procOld,hwnd,msg,wp,lp);
	}
	
	LRESULT lr=::CallWindowProc(pSkin->m_funOldProc,hwnd,msg,wp,lp);
	if(pSkin->m_bOp) return lr;
	if(msg==WM_ERASEBKGND)
	{
			SCROLLINFO si;
			DWORD dwStyle=::GetWindowLong(hwnd,GWL_STYLE);
			if(dwStyle&WS_VSCROLL)
			{
				memset(&si,0,sizeof(si));
				si.cbSize=sizeof(si);
				si.fMask=SIF_ALL;
				::GetScrollInfo(hwnd,SB_VERT,&si);
				if(si.nMax!=pSkin->m_sbVert.m_si.nMax 
					||si.nMin!=pSkin->m_sbVert.m_si.nMin
					||si.nPos!=pSkin->m_sbVert.m_si.nPos
					||si.nPage!=pSkin->m_sbVert.m_si.nPage)
				{
					pSkin->m_sbVert.SetScrollInfo(&si,!pSkin->m_bOp);
					pSkin->m_sbVert.EnableWindow(si.nMax>=si.nPage);
				}
			}
			if(dwStyle&WS_HSCROLL)
			{
				memset(&si,0,sizeof(si));
				si.cbSize=sizeof(si);
				si.fMask=SIF_ALL;
				::GetScrollInfo(hwnd,SB_HORZ,&si);
				if(si.nMax!=pSkin->m_sbHorz.m_si.nMax 
					||si.nMin!=pSkin->m_sbHorz.m_si.nMin
					||si.nPos!=pSkin->m_sbHorz.m_si.nPos
					||si.nPage!=pSkin->m_sbHorz.m_si.nPage)
				{
					pSkin->m_sbHorz.SetScrollInfo(&si,!pSkin->m_bOp);
					pSkin->m_sbHorz.EnableWindow(si.nMax>=si.nPage);
				}
			}
	}else if(msg==WM_NCCALCSIZE && wp)
	{
			LPNCCALCSIZE_PARAMS pNcCalcSizeParam=(LPNCCALCSIZE_PARAMS)lp;
			DWORD dwStyle=::GetWindowLong(hwnd,GWL_STYLE);
			DWORD dwExStyle=::GetWindowLong(hwnd,GWL_EXSTYLE);
			BOOL  bLeftScroll=dwExStyle&WS_EX_LEFTSCROLLBAR;
			int nWid=::GetSystemMetrics(SM_CXVSCROLL);
			if(dwStyle&WS_VSCROLL) 
			{
				if(bLeftScroll)
					pNcCalcSizeParam->rgrc[0].left-=nWid-pSkin->m_nScrollWid;
				else
					pNcCalcSizeParam->rgrc[0].right+=nWid-pSkin->m_nScrollWid;
			}
			if(dwStyle&WS_HSCROLL) pNcCalcSizeParam->rgrc[0].bottom+=nWid-pSkin->m_nScrollWid;
			
			RECT rc,rcVert,rcHorz;
			::GetWindowRect(hwnd,&rc);
			::OffsetRect(&rc,-rc.left,-rc.top);
			
			nWid=pSkin->m_nScrollWid;
			if(bLeftScroll)
			{
				int nLeft=pNcCalcSizeParam->rgrc[0].left;
				int nBottom=pNcCalcSizeParam->rgrc[0].bottom;
				rcVert.right=nLeft;
				rcVert.left=nLeft-nWid;
				rcVert.top=0;
				rcVert.bottom=nBottom;
				rcHorz.left=nLeft;
				rcHorz.right=pNcCalcSizeParam->rgrc[0].right;
				rcHorz.top=nBottom;
				rcHorz.bottom=nBottom+nWid;
			}else
			{
				int nRight=pNcCalcSizeParam->rgrc[0].right;
				int nBottom=pNcCalcSizeParam->rgrc[0].bottom;
				rcVert.left=nRight;
				rcVert.right=nRight+nWid;
				rcVert.top=0;
				rcVert.bottom=nBottom;
				rcHorz.left=0;
				rcHorz.right=nRight;
				rcHorz.top=nBottom;
				rcHorz.bottom=nBottom+nWid;
			}
			if(dwStyle&WS_VSCROLL && dwStyle&WS_HSCROLL)
			{
				pSkin->m_nAngleType=bLeftScroll?1:2;
			}else
			{
				pSkin->m_nAngleType=0;
			}
			if(dwStyle&WS_VSCROLL)
			{
				pSkin->m_sbVert.MoveWindow(&rcVert);
				pSkin->m_sbVert.ShowWindow(SW_SHOW);
			}else
			{
				pSkin->m_sbVert.ShowWindow(SW_HIDE);
			}
			if(dwStyle&WS_HSCROLL)
			{
				pSkin->m_sbHorz.MoveWindow(&rcHorz);
				pSkin->m_sbHorz.ShowWindow(SW_SHOW);
			}else
			{
				pSkin->m_sbHorz.ShowWindow(SW_HIDE);
			}
			pSkin->PostMessage(MYWM_DESTMOVE, dwStyle&WS_VSCROLL, bLeftScroll);
	}
	return lr;
}


extern "C" CSkinScrollWnd* SkinWndScroll(CWindow pWnd, CColorPalette& palit)
{
	ATLTRACE("SkinWndScroll(hWnd=%x)\n", pWnd.m_hWnd);
	CSkinScrollWnd *pFrame = new CSkinScrollWnd(palit);
	pFrame->SkinWindow(pWnd, palit.m_hBmpScrollBarActive);
	return pFrame;
}


extern "C" BOOL UnskinWndScroll(CWindow pWnd)
{
	ATLTRACE("UnskinWndScroll(hWnd=%x)\n", pWnd.m_hWnd);
	CSkinScrollWnd *pFrame = (CSkinScrollWnd *)::GetWindowLongPtr(pWnd.m_hWnd, GWLP_USERDATA);
	if(pFrame) 
	{
		RECT rc;
		CWindow pParent=pFrame->GetParent();
		pFrame->GetWindowRect(&rc);
		pParent.ScreenToClient(&rc);
		SetWindowLongPtr(pWnd.m_hWnd,GWLP_WNDPROC,(LONG_PTR)pFrame->m_funOldProc);
		SetWindowLongPtr(pWnd.m_hWnd,GWLP_USERDATA,0);
		pWnd.SetParent(pParent);
		pWnd.MoveWindow(&rc);
		pFrame->DestroyWindow();
		delete pFrame;
	}
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CSkinScrollWnd

CSkinScrollWnd::CSkinScrollWnd(CColorPalette& palit) : m_wndLimit(palit)
{
	m_funOldProc=NULL;
	m_bOp=FALSE;
	m_nScrollWid=16;
	m_hBmpScroll=0;
	m_nAngleType=0;
}

CSkinScrollWnd::~CSkinScrollWnd()
{
}


CLimitWindow::CLimitWindow(CColorPalette& palit) : m_palit(palit)
{
}

LRESULT CLimitWindow::OnCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HDC hdc = (HDC)wParam;
	::SetTextColor(hdc, m_palit.CrText);
	if (CLR_INVALID != m_palit.CrBackGnd)
		::SetBkColor(hdc, m_palit.CrBackGnd);
	::SelectObject(hdc, m_palit.m_brBackGnd);

	return (LRESULT)m_palit.m_brBackGnd.m_hBrush;
}

LRESULT CLimitWindow::OnSendCmdToMainWnd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	ATLTRACE("CLimitWindow::OnSendCmdToMainWnd(wID=%d)\n", wID);
	GetParent().GetParent().SendMessage(WM_COMMAND, MAKEWPARAM((WORD)wID, wNotifyCode), (LPARAM)m_hWnd);
	return 0;
}

BOOL CLimitWindow::RegisterClass()
{
	WNDCLASS wndcls;
	HINSTANCE hInst = _AtlBaseModule.GetModuleInstance();
	if (!(::GetClassInfo(hInst, WNDLIMIT_CLASSNAME, &wndcls)))
	{
		wndcls.style = CS_SAVEBITS | CS_DBLCLKS;
		wndcls.lpfnWndProc = WindowProc;
		wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
		wndcls.hInstance = hInst;
		wndcls.hIcon = NULL;
		wndcls.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		wndcls.hbrBackground = NULL;
		wndcls.lpszMenuName = NULL;
		wndcls.lpszClassName = WNDLIMIT_CLASSNAME;

		if (!::RegisterClass(&wndcls))
		{
			///AfxThrowResourceException();
			return FALSE;
		}
	}

	return TRUE;
}

BOOL CSkinScrollWnd::RegisterClass()
{
	WNDCLASS wndcls;
	HINSTANCE hInst = _AtlBaseModule.GetModuleInstance();
	if (!(::GetClassInfo(hInst, SKINSCROLLBAR_CLASSNAME, &wndcls)))
	{
		wndcls.style = CS_SAVEBITS | CS_DBLCLKS;
		wndcls.lpfnWndProc = WindowProc;
		wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
		wndcls.hInstance = hInst;
		wndcls.hIcon = NULL;
		wndcls.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		wndcls.hbrBackground = NULL;
		wndcls.lpszMenuName = NULL;
		wndcls.lpszClassName = SKINSCROLLBAR_CLASSNAME;

		if (!::RegisterClass(&wndcls))
		{
			///AfxThrowResourceException();
			return FALSE;
		}
	}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CSkinScrollWnd message handlers

BOOL CSkinScrollWnd::SkinWindow(CWindow pWnd,HBITMAP hBmpScroll)
{
	ATLTRACE("CSkinScrollWnd::SkinWindow(hWnd=%x)\n", pWnd.m_hWnd);
	ATLASSERT(m_hWnd == NULL);
	m_hBmpScroll=hBmpScroll;
	BITMAP bm;
	GetObject(hBmpScroll,sizeof(bm),&bm);
	m_nScrollWid=bm.bmWidth/9;

	CWindow parent=pWnd.GetParent();
	ATLASSERT(parent.m_hWnd);
	RECT rcFrm,rcWnd;
	pWnd.GetWindowRect(&rcFrm);
	parent.ScreenToClient(&rcFrm);
	rcWnd=rcFrm;
	OffsetRect(&rcWnd,-rcWnd.left,-rcWnd.top);
	UINT uID=pWnd.GetDlgCtrlID();
	DWORD dwStyle=pWnd.GetStyle();
	DWORD dwExStyle=pWnd.GetExStyle();
	DWORD dwFrmStyle=WS_CHILD|SS_NOTIFY;
	DWORD dwFrmStyleEx=0;
	if(dwExStyle & WS_EX_TOOLWINDOW)
	{
		dwFrmStyleEx|=WS_EX_TOOLWINDOW;
		dwFrmStyleEx |=WS_EX_TOPMOST;
	}
	if(dwStyle&WS_VISIBLE) dwFrmStyle|=WS_VISIBLE;

	if(dwStyle&WS_BORDER)
	{
		dwFrmStyle|=WS_BORDER;
		pWnd.ModifyStyle(WS_BORDER,0);
		int nBorder=::GetSystemMetrics(SM_CXBORDER);
		rcWnd.right-=nBorder*2;
		rcWnd.bottom-=nBorder*2;
	}
	if(dwExStyle&WS_EX_CLIENTEDGE)
	{
		pWnd.ModifyStyleEx(WS_EX_CLIENTEDGE,0);
		int nBorder=::GetSystemMetrics(SM_CXEDGE);
		rcWnd.right-=nBorder*2;
		rcWnd.bottom-=nBorder*2;
		dwFrmStyleEx|=WS_EX_CLIENTEDGE;
	}
	
	RegisterClass();
	Create(parent, rcFrm, _T("SkinScrollBarFrame"), dwFrmStyle, dwFrmStyleEx, ATL::_U_MENUorID(uID));
	if (m_hWnd == NULL)
	{
		DWORD err = GetLastError();
		ATLTRACE("** Error creating SkinScrollBarFrame %d\n", err);
		return FALSE;
	}

	m_wndLimit.RegisterClass();
	m_wndLimit.Create(m_hWnd, rcDefault, _T("LIMIT"), WS_CHILD | WS_VISIBLE, 0/*dwExStyle*/, ATL::_U_MENUorID(277));
	m_sbHorz.Create(m_hWnd, rcDefault, _T(""), WS_CHILD, 0/*dwExStyle*/, ATL::_U_MENUorID(177));
	m_sbVert.Create(m_hWnd, rcDefault, _T(""), WS_CHILD | SBS_VERT, 0/*dwExStyle*/, ATL::_U_MENUorID(178));
	m_sbHorz.SetBitmap(m_hBmpScroll);
	m_sbVert.SetBitmap(m_hBmpScroll);

	pWnd.SetParent(m_wndLimit);
	::SetWindowLongPtr(pWnd.m_hWnd,GWLP_USERDATA,(LONG_PTR)this);
	m_funOldProc=(WNDPROC)::SetWindowLongPtr(pWnd.m_hWnd,GWLP_WNDPROC,(LONG_PTR)HookWndProc);

	pWnd.MoveWindow(&rcWnd);
	SetTimer(TIMER_UPDATE,250,NULL);
	return TRUE;
}

void CSkinScrollWnd::SetBmpScroll(HBITMAP hBmpScroll)
{
	m_hBmpScroll = hBmpScroll;
	m_sbHorz.SetBitmap(m_hBmpScroll);
	m_sbVert.SetBitmap(m_hBmpScroll);
}

LRESULT CSkinScrollWnd::OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	return 1;   // no need for the background
}

LRESULT CSkinScrollWnd::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);
	int cx = GET_X_LPARAM(lParam);
	int cy = GET_Y_LPARAM(lParam);
	if (cx == 0 || cy == 0) return lRet;
	if(m_wndLimit.m_hWnd)
	{
		ATLTRACE("CSkinScrollWnd::OnSize(cx=%d, cy=%d) -> moving the limit window\n", cx, cy);
		CWindow pWnd = m_wndLimit.GetWindow(GW_CHILD);
		pWnd.MoveWindow(0,0,cx,cy);
	}

	return lRet;
}

LRESULT CSkinScrollWnd::OnVScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CWindow pWnd = m_wndLimit.GetWindow(GW_CHILD);
	m_bOp = TRUE;
	LRESULT lRet = ::SendMessage(pWnd, WM_VSCROLL, wParam, 0/*lParam*/);
	m_bOp=FALSE;

	UINT nSBCode = LOWORD(wParam);
	UINT nPos = HIWORD(wParam);
	ATLTRACE("CSkinScrollWnd::OnVScroll(nSBCode=%d, nPos=%d, lParam=%x) -> the Limit hWnd=%x returned %d\n", nSBCode, nPos, lParam, pWnd.m_hWnd, lRet);
	if (nSBCode == SB_THUMBTRACK) return 0;

	SCROLLINFO si = { sizeof(si), SIF_ALL };
	//SCROLLINFO si = { sizeof(si), SIF_PAGE | SIF_POS | SIF_TRACKPOS };
	::GetScrollInfo(pWnd, SB_VERT, &si);
	si.nPos = si.nTrackPos = nPos;
	si.fMask = SIF_PAGE | SIF_POS | SIF_TRACKPOS;
	ATLTRACE("   setting v-scroll: nMin=%d, nMax=%d, nPage=%d, nPos=%d, nTrackPos=%d\n", si.nMin, si.nMax, si.nPage, si.nPos, si.nTrackPos);
	//ATLTRACE("   setting v-scroll: nPage=%d, nPos=%d, nTrackPos=%d\n", si.nPage, si.nPos, si.nTrackPos);
	::SetScrollInfo((HWND)lParam, SB_VERT, &si, TRUE);

	return lRet;
}

LRESULT CSkinScrollWnd::OnHScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CWindow pWnd=m_wndLimit.GetWindow(GW_CHILD);
	m_bOp=TRUE;
	::SendMessage(pWnd, WM_HSCROLL, wParam, 0/*lParam*/);
	m_bOp=FALSE;
	UINT nSBCode = LOWORD(wParam);
	UINT nPos = HIWORD(wParam);
	ATLTRACE("CSkinScrollWnd::OnHScroll(nSBCode=%d, nPos=%d)\n", nSBCode, nPos);
	if (nSBCode == SB_THUMBTRACK) return 0;
	//SCROLLINFO si = { sizeof(si), SIF_ALL };
	SCROLLINFO si = { sizeof(si), SIF_PAGE | SIF_POS | SIF_TRACKPOS };
	::GetScrollInfo(pWnd, SB_HORZ, &si);
	ATLTRACE("   setting h-scroll info: nPage=%d, nPos=%d, nTrackPos=%d\n", si.nPage, si.nPos, si.nTrackPos);
	::SetScrollInfo((HWND)lParam, SB_HORZ, &si, TRUE);
	return 0;
}

LRESULT CSkinScrollWnd::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dc(m_hWnd); // device context for painting
	CDC memdc;
	memdc.CreateCompatibleDC(dc);
	HGDIOBJ hOldBmp=::SelectObject(memdc,m_hBmpScroll);
	RECT rc;
	GetClientRect(&rc);
	if(m_nAngleType==1)
		dc.BitBlt(rc.left,rc.bottom-m_nScrollWid,m_nScrollWid,m_nScrollWid,memdc,m_nScrollWid*8,m_nScrollWid*1,SRCCOPY);
	else if(m_nAngleType==2)
		dc.BitBlt(rc.right-m_nScrollWid,rc.bottom-m_nScrollWid,m_nScrollWid,m_nScrollWid,memdc,m_nScrollWid*8,m_nScrollWid*0,SRCCOPY);
	::SelectObject(memdc,hOldBmp);
	return 0;
}

LRESULT CSkinScrollWnd::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CWindow pWnd=m_wndLimit.GetWindow(GW_CHILD);
	SCROLLINFO si1 = { sizeof(SCROLLINFO), SIF_ALL };
	SCROLLINFO si2 = { sizeof(SCROLLINFO), SIF_ALL };
	if(::GetWindowLong(m_sbVert.m_hWnd,GWL_STYLE)&WS_VISIBLE && !m_sbVert.m_bDrag)
	{
		::GetScrollInfo(pWnd, SB_VERT, &si1);
		::GetScrollInfo(m_sbVert.m_hWnd, SB_VERT, &si2);
		if(si1.nMax!=si2.nMax||si1.nMin!=si2.nMin||si1.nPos!=si2.nPos||si1.nPage!=si2.nPage)
			m_sbVert.SetScrollInfo(&si1);
	}
	if(::GetWindowLong(m_sbHorz.m_hWnd,GWL_STYLE)&WS_VISIBLE && !m_sbHorz.m_bDrag)
	{
		::GetScrollInfo(pWnd, SB_HORZ, &si1);
		::GetScrollInfo(m_sbHorz.m_hWnd, SB_HORZ, &si2);
		if (si1.nMax != si2.nMax || si1.nMin != si2.nMin || si1.nPos != si2.nPos || si1.nPage != si2.nPage)
			m_sbHorz.SetScrollInfo(&si1);
	}
	return 0;
}

LRESULT CSkinScrollWnd::OnDestMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	ATLTRACE("CSkinScrollWnd::OnDestMove()\n");
	m_bOp=TRUE;
	BOOL bVScroll=wParam;
	BOOL bLeftScroll=lParam;
	RECT rcClient;
	GetClientRect(&rcClient);
	RECT rcLimit,rcWnd;
	rcWnd=rcClient;
	rcLimit=rcClient;
	if(::GetWindowLong(m_sbHorz.m_hWnd,GWL_STYLE)&WS_VISIBLE) rcLimit.bottom-=m_nScrollWid;
	if(bLeftScroll)
	{
		if(bVScroll)
		{
			rcLimit.left+=m_nScrollWid;
			OffsetRect(&rcWnd,-m_nScrollWid,0);
		}
	}else
	{
		if(bVScroll) rcLimit.right-=m_nScrollWid;
	}
	m_wndLimit.MoveWindow(&rcLimit);
	m_wndLimit.GetWindow(GW_CHILD).SetWindowPos(NULL,rcWnd.left,0,0,0,SWP_NOSIZE|SWP_NOZORDER);
	m_bOp=FALSE;
	return 0;
}
