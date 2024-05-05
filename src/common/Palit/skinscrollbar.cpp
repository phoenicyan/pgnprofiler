/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "SkinScrollBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define TIMERID_NOTIFY	100
#define TIMERID_DELAY	200
#define TIME_DELAY		250
#define TIME_INTER		100

#define THUMB_BORDER	3
#define THUMB_MINSIZE	(THUMB_BORDER*2)

/////////////////////////////////////////////////////////////////////////////
// CSkinScrollBar

CSkinScrollBar::CSkinScrollBar()
{
	ATLTRACE("CSkinScrollBar::CSkinScrollBar()\n");
	m_hBmp = NULL;
	m_bDrag=FALSE;
	memset(&m_si,0,sizeof(SCROLLINFO));
	m_si.nTrackPos=-1;
	m_uClicked=-1;
	m_bNotify=FALSE;
	m_uHtPrev=-1;
	m_bPause=FALSE;
	m_bTrace=FALSE;
}

CSkinScrollBar::~CSkinScrollBar()
{
	ATLTRACE("CSkinScrollBar::~CSkinScrollBar()\n");
}

/////////////////////////////////////////////////////////////////////////////
// CSkinScrollBar message handlers

BOOL CSkinScrollBar::IsVertical()
{
	DWORD dwStyle=GetStyle();
	return dwStyle&SBS_VERT;
}

UINT CSkinScrollBar::HitTest(CPoint pt)
{
	int nTestPos=pt.y;
	if(!IsVertical()) nTestPos=pt.x;
	if(nTestPos<0) return -1;
	SCROLLINFO si=m_si;
	int nInterHei=m_nHei-2*m_nWid;
	if(nInterHei<0) nInterHei=0;
	int	nSlideHei=si.nPage*nInterHei/(si.nMax-si.nMin+1);
	if(nSlideHei<THUMB_MINSIZE) nSlideHei=THUMB_MINSIZE;
	if(nInterHei<THUMB_MINSIZE) nSlideHei=0;
	int nEmptyHei=nInterHei-nSlideHei;
	
	int nArrowHei=(nInterHei==0)?(m_nHei/2):m_nWid;
	int nBottom=0;
	int nSegLen=nArrowHei;
	nBottom+=nSegLen;
	UINT uHit=SB_LINEUP;
	if(nTestPos<nBottom) goto end;
	if(si.nTrackPos==-1) si.nTrackPos=si.nPos;
	uHit=SB_PAGEUP;
	if((si.nMax-si.nMin-si.nPage+1)==0)
		nSegLen=nEmptyHei/2;
	else
		nSegLen=nEmptyHei*si.nTrackPos/(si.nMax-si.nMin-si.nPage+1);
	nBottom+=nSegLen;
	if(nTestPos<nBottom) goto end;
	nSegLen=nSlideHei;
	nBottom+=nSegLen;
	uHit=SB_THUMBTRACK;
	if(nTestPos<nBottom) goto end;
	nBottom=m_nHei-nArrowHei;
	uHit=SB_PAGEDOWN;
	if(nTestPos<nBottom) goto end;
	uHit=SB_LINEDOWN;
end:
	//ATLTRACE("CSkinScrollBar::HitTest(x=%d, y=%d) -> %d\n", pt.x, pt.y, uHit);
	return uHit;
}

void CSkinScrollBar::SetBitmap(HBITMAP hBmp)
{
	ATLTRACE("CSkinScrollBar::SetBitmap(%x)\n", hBmp);
	ATLASSERT(m_hWnd);
	m_hBmp=hBmp;
	BITMAP bm;
	GetObject(hBmp,sizeof(bm),&bm);
	m_nWid=bm.bmWidth/9;
	m_nFrmHei=bm.bmHeight/3;
	CRect rc;
	GetWindowRect(&rc);
	::MapWindowPoints(NULL, GetParent(), (LPPOINT)&rc, 2);	//MFC equivalent for ScreenToClient
	if(IsVertical())
	{
		rc.right=rc.left+m_nWid;
	}else
	{
		rc.bottom=rc.top+m_nWid;
	}
	MoveWindow(&rc);
}

LRESULT CSkinScrollBar::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);
	int cx = GET_X_LPARAM(lParam);
	int cy = GET_Y_LPARAM(lParam);

	m_nHei=IsVertical()?cy:cx;
	ATLTRACE("CSkinScrollBar::OnSize(%d, %d) -> m_nHei=%d\n", cx, cy, m_nHei);
	return lRet;
}

LRESULT CSkinScrollBar::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
	CPaintDC dc(m_hWnd); // device context for painting
	if(!m_hBmp) return 0;
	CDC memdc;
	memdc.CreateCompatibleDC(dc);
	HGDIOBJ hOldBmp=::SelectObject(memdc,m_hBmp);

	RECT rcSour={0,0,m_nWid,m_nWid};
	if(!IsVertical()) OffsetRect(&rcSour,m_nWid*4,0);
	RECT rcDest;
	rcDest=GetRect(SB_LINEUP);
	if((rcDest.right-rcDest.left != rcSour.right-rcSour.left)
		||(rcDest.bottom-rcDest.top != rcSour.bottom-rcSour.top))
		dc.StretchBlt(rcDest.left,rcDest.top,rcDest.right-rcDest.left,rcDest.bottom-rcDest.top,
			memdc,
			rcSour.left,rcSour.top,rcSour.right-rcSour.left,rcSour.bottom-rcSour.top,
			SRCCOPY);
	else
		dc.BitBlt(rcDest.left,rcDest.top,m_nWid,m_nWid,memdc,rcSour.left,rcSour.top,SRCCOPY);
	rcDest=GetRect(SB_LINEDOWN);
	OffsetRect(&rcSour,m_nWid,0);
	if((rcDest.right-rcDest.left != rcSour.right-rcSour.left)
		||(rcDest.bottom-rcDest.top != rcSour.bottom-rcSour.top))
		dc.StretchBlt(rcDest.left,rcDest.top,rcDest.right-rcDest.left,rcDest.bottom-rcDest.top,
			memdc,
			rcSour.left,rcSour.top,rcSour.right-rcSour.left,rcSour.bottom-rcSour.top,
			SRCCOPY);
	else
		dc.BitBlt(rcDest.left,rcDest.top,m_nWid,m_nWid,memdc,rcSour.left,rcSour.top,SRCCOPY);
	rcDest=GetRect(SB_THUMBTRACK);
	OffsetRect(&rcSour,m_nWid,0);
	DrawThumb(dc,&rcDest,&memdc,&rcSour);
	OffsetRect(&rcSour,m_nWid,0);
	rcDest=GetRect(SB_PAGEUP);
	TileBlt(dc,&rcDest,&memdc,&rcSour);
	rcDest=GetRect(SB_PAGEDOWN);
	TileBlt(dc,&rcDest,&memdc,&rcSour);

	::SelectObject(memdc,hOldBmp);

	return 0;
}

RECT CSkinScrollBar::GetImageRect(UINT uSBCode,int nState)
{
	int nIndex=0;
	switch(uSBCode)
	{
	case SB_LINEUP:nIndex=0;break;
	case SB_PAGEUP:nIndex=3;break;
	case SB_THUMBTRACK:nIndex=2;break;
	case SB_PAGEDOWN:nIndex=3;break;
	case SB_LINEDOWN:nIndex=1;break;
	}
	if(!IsVertical())nIndex+=4;
	RECT rcRet={m_nWid*nIndex,m_nWid*nState,m_nWid*(nIndex+1),m_nWid*(nState+1)};
	//ATLTRACE("CSkinScrollBar::GetImageRect(uSBCode=%d, nState=%d) -> (%d, %d, %d, %d)\n", uSBCode, nState, rcRet.left, rcRet.top, rcRet.right, rcRet.bottom);
	return rcRet;
}

RECT CSkinScrollBar::GetRect(UINT uSBCode)
{
	SCROLLINFO si=m_si;
	if(si.nTrackPos==-1) si.nTrackPos=si.nPos;
	int nInterHei=m_nHei-2*m_nWid;
	if(nInterHei<0) nInterHei=0;
	int	nSlideHei=si.nPage*nInterHei/(si.nMax-si.nMin+1);
	if(nSlideHei<THUMB_MINSIZE) nSlideHei=THUMB_MINSIZE;
	if(nInterHei<THUMB_MINSIZE) nSlideHei=0;
	if((si.nMax-si.nMin-si.nPage+1)==0) nSlideHei=0;
	int nEmptyHei=nInterHei-nSlideHei;
	int nArrowHei=m_nWid;
	if(nInterHei==0) nArrowHei=m_nHei/2;
	RECT rcRet={0,0,m_nWid,nArrowHei};
	if(uSBCode==SB_LINEUP) goto end;
	rcRet.top=rcRet.bottom;
	if((si.nMax-si.nMin-si.nPage+1)==0)
		rcRet.bottom+=nEmptyHei/2;
	else
		rcRet.bottom+=nEmptyHei*si.nTrackPos/(si.nMax-si.nMin-si.nPage+1);
	if(uSBCode==SB_PAGEUP) goto end;
	rcRet.top=rcRet.bottom;
	rcRet.bottom+=nSlideHei;
	if(uSBCode==SB_THUMBTRACK) goto end;
	rcRet.top=rcRet.bottom;
	rcRet.bottom=m_nHei-nArrowHei;
	if(uSBCode==SB_PAGEDOWN) goto end;
	rcRet.top=rcRet.bottom;
	rcRet.bottom=m_nHei;
	if(uSBCode==SB_LINEDOWN) goto end;
end:
	if(!IsVertical())
	{
		int t=rcRet.left;
		rcRet.left=rcRet.top;
		rcRet.top=t;
		t=rcRet.right;
		rcRet.right=rcRet.bottom;
		rcRet.bottom=t;
	}
	//ATLTRACE("CSkinScrollBar::GetRect(uSBCode=%d) -> (%d, %d, %d, %d)\n", uSBCode, rcRet.left, rcRet.top, rcRet.right, rcRet.bottom);
	return rcRet;
}

void CSkinScrollBar::TileBlt(HDC hDestDC, RECT *pDestRect, CDC *pSourDC, RECT *pSourRect)
{
	int nSourHei=pSourRect->bottom-pSourRect->top;
	int nSourWid=pSourRect->right-pSourRect->left;

	int y=pDestRect->top;
	while(y<pDestRect->bottom)
	{
		int nHei=nSourHei;
		if(y+nHei>pDestRect->bottom) nHei=pDestRect->bottom-y;

		int x=pDestRect->left;
		while(x<pDestRect->right)
		{
			int nWid=nSourWid;
			if(x+nWid>pDestRect->right) nWid=pDestRect->right-x;
			BitBlt(hDestDC, x, y, nWid, nHei, *pSourDC, pSourRect->left, pSourRect->top, SRCCOPY);
			x+=nWid;
		}
		y+=nHei;
	}
}

void CSkinScrollBar::DrawThumb(HDC hDestDC, RECT *pDestRect, CDC *pSourDC, RECT *pSourRect)
{
	if(IsRectEmpty(pDestRect)) return;
	RECT rcDest=*pDestRect,rcSour=*pSourRect;
	if(IsVertical())
	{
		ATLASSERT(pDestRect->bottom-pDestRect->top>=THUMB_MINSIZE);
		BitBlt(hDestDC, pDestRect->left, pDestRect->top, m_nWid, THUMB_BORDER, *pSourDC, pSourRect->left, pSourRect->top, SRCCOPY);
		BitBlt(hDestDC, pDestRect->left, pDestRect->bottom - THUMB_BORDER, m_nWid, THUMB_BORDER, *pSourDC, pSourRect->left, pSourRect->bottom - THUMB_BORDER, SRCCOPY);
		rcDest.top+=THUMB_BORDER,rcDest.bottom-=THUMB_BORDER;
		rcSour.top+=THUMB_BORDER,rcSour.bottom-=THUMB_BORDER;
		TileBlt(hDestDC,&rcDest,pSourDC,&rcSour);
	}else
	{
		ATLASSERT(pDestRect->right-pDestRect->left>=THUMB_MINSIZE);
		BitBlt(hDestDC, pDestRect->left, pDestRect->top, THUMB_BORDER, m_nWid, *pSourDC, pSourRect->left, pSourRect->top, SRCCOPY);
		BitBlt(hDestDC, pDestRect->right - THUMB_BORDER, pDestRect->top, THUMB_BORDER, m_nWid, *pSourDC, pSourRect->right - THUMB_BORDER, pSourRect->top, SRCCOPY);
		rcDest.left+=THUMB_BORDER,rcDest.right-=THUMB_BORDER;
		rcSour.left+=THUMB_BORDER,rcSour.right-=THUMB_BORDER;
		TileBlt(hDestDC,&rcDest,pSourDC,&rcSour);
	}
}


LRESULT CSkinScrollBar::OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	SetCapture();
	CPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	UINT uHit = HitTest(point);
	if(uHit==SB_THUMBTRACK)
	{
		ATLTRACE("CSkinScrollBar::OnLButtonDown(SB_THUMBTRACK) -> m_si.nPos=%d\n", m_si.nPos);
		m_bDrag = TRUE;
		m_ptDrag=point;
		m_si.nTrackPos=m_si.nPos;
		m_nDragPos=m_si.nPos;
	}else
	{
		ATLTRACE("CSkinScrollBar::OnLButtonDown(not thumb) -> sending to parent uHit=%d\n", uHit);
		m_uClicked = uHit;
		::SendMessage(GetParent(), IsVertical()?WM_VSCROLL:WM_HSCROLL,MAKELONG(m_uClicked,0),(LPARAM)m_hWnd);
		SetTimer(TIMERID_DELAY,TIME_DELAY,NULL);
		m_bPause=FALSE;
		if(uHit==SB_LINEUP||uHit==SB_LINEDOWN) DrawArrow(uHit,2);
	}
	return 0;
}

LRESULT CSkinScrollBar::OnMouseMove(UINT uMsg, WPARAM nFlags, LPARAM lParam, BOOL& bHandled)
{
	CPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

	if (!m_bTrace && nFlags != -1)
	{
		m_bTrace=TRUE;
		TRACKMOUSEEVENT tme;
		tme.cbSize = sizeof(tme);
		tme.hwndTrack = m_hWnd;
		tme.dwFlags = TME_LEAVE;
		tme.dwHoverTime = 1;
		m_bTrace = _TrackMouseEvent(&tme);
	}

	if(m_bDrag)
	{
		int nInterHei=m_nHei-2*m_nWid;
		int	nSlideHei=m_si.nPage*nInterHei/(m_si.nMax-m_si.nMin+1);
		if(nSlideHei<THUMB_MINSIZE) nSlideHei=THUMB_MINSIZE;
		if(nInterHei<THUMB_MINSIZE) nSlideHei=0;
		int nEmptyHei=nInterHei-nSlideHei;
		int nDragLen=IsVertical()?point.y-m_ptDrag.y:point.x-m_ptDrag.x;
		int nSlide=(nEmptyHei==0)?0:(nDragLen*(int)(m_si.nMax-m_si.nMin-m_si.nPage+1)/nEmptyHei);
		int nNewTrackPos=m_nDragPos+nSlide;
		if(nNewTrackPos<m_si.nMin)
		{
			nNewTrackPos=m_si.nMin;
		}else if(nNewTrackPos>(int)(m_si.nMax-m_si.nMin-m_si.nPage+1))
		{
			nNewTrackPos=m_si.nMax-m_si.nMin-m_si.nPage+1;
		}
		if(nNewTrackPos!=m_si.nTrackPos)
		{
			HDC hDC=::GetDC(m_hWnd);
			CDC memdc;
			memdc.CreateCompatibleDC(hDC);
			HGDIOBJ hOldBmp=SelectObject(memdc,m_hBmp);
			RECT rcThumb1=GetRect(SB_THUMBTRACK);
			m_si.nTrackPos=nNewTrackPos;
			RECT rcThumb2=GetRect(SB_THUMBTRACK);

			RECT rcSourSlide=GetImageRect(SB_PAGEUP,0);
			RECT rcSourThumb=GetImageRect(SB_THUMBTRACK,2);
			RECT rcOld;
			if(IsVertical())
			{
				rcOld.left=0,rcOld.right=m_nWid;
				if(rcThumb2.bottom>rcThumb1.bottom)
				{
					rcOld.top=rcThumb1.top;
					rcOld.bottom=rcThumb2.top;
				}else
				{
					rcOld.top=rcThumb2.bottom;
					rcOld.bottom=rcThumb1.bottom;
				}
			}else
			{
				rcOld.top=0,rcOld.bottom=m_nWid;
				if(rcThumb2.right>rcThumb1.right)
				{
					rcOld.left=rcThumb1.left;
					rcOld.right=rcThumb2.left;
				}else
				{
					rcOld.left=rcThumb2.right;
					rcOld.right=rcThumb1.right;
				}
			}
			TileBlt(hDC,&rcOld,&memdc,&rcSourSlide);
			DrawThumb(hDC,&rcThumb2,&memdc,&rcSourThumb);
			SelectObject(memdc,hOldBmp);
			ReleaseDC(hDC);
			
			ATLTRACE("CSkinScrollBar::OnMouseMove() -> sending SB_THUMBTRACK with m_si.nTrackPos=%d to parent\n", m_si.nTrackPos);
			::SendMessage(GetParent(), IsVertical() ? WM_VSCROLL : WM_HSCROLL, MAKELONG(SB_THUMBTRACK, m_si.nTrackPos), (LPARAM)m_hWnd);
		}
	}else if(m_uClicked!=-1)
	{
		CRect rc(GetRect(m_uClicked));
		m_bPause=!rc.PtInRect(point);
		if(m_uClicked==SB_LINEUP||m_uClicked==SB_LINEDOWN)
		{
			DrawArrow(m_uClicked,m_bPause?0:2);
		}
	}else
	{
		UINT uHit=HitTest(point);
		if(uHit!=m_uHtPrev)
		{
			if(m_uHtPrev!=-1)
			{
				if(m_uHtPrev==SB_THUMBTRACK)
				{
					HDC hDC = ::GetDC(m_hWnd);
					CDC memdc;
					memdc.CreateCompatibleDC(hDC);
					HGDIOBJ hOldBmp=SelectObject(memdc,m_hBmp);
					RECT rcDest=GetRect(SB_THUMBTRACK);
					RECT rcSour=GetImageRect(SB_THUMBTRACK,0);
					DrawThumb(hDC,&rcDest,&memdc,&rcSour);
					ReleaseDC(hDC);
					SelectObject(memdc,hOldBmp);
				}else if(m_uHtPrev==SB_LINEUP||m_uHtPrev==SB_LINEDOWN)
				{
					DrawArrow(m_uHtPrev,0);
				}
			}
			if(uHit!=-1)
			{
				if(uHit==SB_THUMBTRACK)
				{
					HDC hDC = ::GetDC(m_hWnd);
					CDC memdc;
					memdc.CreateCompatibleDC(hDC);
					HGDIOBJ hOldBmp=SelectObject(memdc,m_hBmp);
					RECT rcDest=GetRect(SB_THUMBTRACK);
					RECT rcSour=GetImageRect(SB_THUMBTRACK,1);
					DrawThumb(hDC,&rcDest,&memdc,&rcSour);
					ReleaseDC(hDC);
					SelectObject(memdc,hOldBmp);
				}else if(uHit==SB_LINEUP||uHit==SB_LINEDOWN)
				{
					DrawArrow(uHit,1);
				}
			}
			m_uHtPrev=uHit;
		}
	}
	return 0;
}

LRESULT CSkinScrollBar::OnLButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	ReleaseCapture();
	if(m_bDrag)
	{
		m_bDrag=FALSE;
		ATLTRACE("CSkinScrollBar::OnLButtonUp() -> sending SB_THUMBPOSITION with m_si.nTrackPos=%d to parent\n", m_si.nTrackPos);
		::SendMessage(GetParent(), IsVertical() ? WM_VSCROLL : WM_HSCROLL, MAKELONG(SB_THUMBPOSITION, m_si.nTrackPos), (LPARAM)m_hWnd);
		HDC hDC = ::GetDC(m_hWnd);
		CDC memdc;
		memdc.CreateCompatibleDC(hDC);
		HGDIOBJ hOldBmp=SelectObject(memdc,m_hBmp);
		if(m_si.nTrackPos != m_si.nPos)
		{
			RECT rcThumb=GetRect(SB_THUMBTRACK);
			RECT rcSour={m_nWid*3,0,m_nWid*4,m_nWid};
			if(!IsVertical())  OffsetRect(&rcSour,m_nWid*4,0);
			TileBlt(hDC,&rcThumb,&memdc,&rcSour);
		}
		m_si.nTrackPos=-1;

		RECT rcThumb=GetRect(SB_THUMBTRACK);
		RECT rcSour={m_nWid*2,0,m_nWid*3,m_nWid};
		CPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		if (PtInRect(&rcThumb, point)) OffsetRect(&rcSour, 0, m_nWid);
		if(!IsVertical()) OffsetRect(&rcSour,m_nWid*4,0);
		DrawThumb(hDC,&rcThumb,&memdc,&rcSour);
		SelectObject(memdc,hOldBmp);
		ReleaseDC(hDC);
	}else if(m_uClicked!=-1)
	{
		if(m_bNotify)
		{
			KillTimer(TIMERID_NOTIFY);
			m_bNotify=FALSE;
		}else
		{
			KillTimer(TIMERID_DELAY);
		}
		if(m_uClicked==SB_LINEUP||m_uClicked==SB_LINEDOWN) DrawArrow(m_uClicked,0);
		m_uClicked=-1;
	}
	return 0;
}

LRESULT CSkinScrollBar::OnTimer(UINT /*uMsg*/, WPARAM nIDEvent, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// TODO: Add your message handler code here and/or call default
	if(nIDEvent==TIMERID_DELAY)
	{
		m_bNotify=TRUE;
		nIDEvent=TIMERID_NOTIFY;
		KillTimer(TIMERID_DELAY);
		SetTimer(TIMERID_NOTIFY,TIME_INTER,NULL);
	}
	if(nIDEvent==TIMERID_NOTIFY && !m_bPause)
	{
		ATLASSERT(m_uClicked!=-1 && m_uClicked!=SB_THUMBTRACK);
		
		switch(m_uClicked)
		{
		case SB_LINEUP:
			if(m_si.nPos==m_si.nMin)
			{
				KillTimer(TIMERID_NOTIFY);
				break;
			}
			::SendMessage(GetParent(), IsVertical()?WM_VSCROLL:WM_HSCROLL,MAKELONG(SB_LINEUP,0),(LPARAM)m_hWnd);
			break;
		case SB_LINEDOWN:
			if(m_si.nPos==m_si.nMax)
			{
				KillTimer(TIMERID_NOTIFY);
				break;
			}
			::SendMessage(GetParent(), IsVertical()?WM_VSCROLL:WM_HSCROLL,MAKELONG(SB_LINEDOWN,0),(LPARAM)m_hWnd);
			break;
		case SB_PAGEUP:
		case SB_PAGEDOWN:
			{
				CPoint pt;
				GetCursorPos(&pt);
				ScreenToClient(&pt);
				CRect rc=GetRect(SB_THUMBTRACK);
				if(rc.PtInRect(pt))
				{
					KillTimer(TIMERID_NOTIFY);
					break;
				}
				::SendMessage(GetParent(), IsVertical()?WM_VSCROLL:WM_HSCROLL,MAKELONG(m_uClicked,0),(LPARAM)m_hWnd);
			}
			break;
		default:
			ATLASSERT(FALSE);
			break;
		}
	}
	return 0;
}

void CSkinScrollBar::DrawArrow(UINT uArrow, int nState)
{
	ATLASSERT(uArrow==SB_LINEUP||uArrow==SB_LINEDOWN);
	HDC hDC = ::GetDC(m_hWnd);
	CDC memdc;
	memdc.CreateCompatibleDC(hDC);
	HGDIOBJ hOldBmp=::SelectObject(memdc,m_hBmp);
	
	RECT rcDest=GetRect(uArrow);
	RECT rcSour=GetImageRect(uArrow,nState);
	if((rcDest.right-rcDest.left != rcSour.right-rcSour.left)
		||(rcDest.bottom-rcDest.top != rcSour.bottom-rcSour.top))
		::StretchBlt(hDC,rcDest.left,rcDest.top,rcDest.right-rcDest.left,rcDest.bottom-rcDest.top,
			memdc,
			rcSour.left,rcSour.top,rcSour.right-rcSour.left,rcSour.bottom-rcSour.top,
			SRCCOPY);
	else
		::BitBlt(hDC,rcDest.left,rcDest.top,m_nWid,m_nWid,memdc,rcSour.left,rcSour.top,SRCCOPY);

	ReleaseDC(hDC);
	::SelectObject(memdc,hOldBmp);
}


LRESULT CSkinScrollBar::OnMouseLeave(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	m_bTrace = FALSE;

	OnMouseMove(WM_MOUSEMOVE, -1, -1, bHandled);

	return 0;
}

LRESULT CSkinScrollBar::OnLButtonDblClk(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	ATLTRACE("CSkinScrollBar::OnLButtonDblClk() -> ignored\n");
	return 0;
}

LRESULT CSkinScrollBar::OnSetScrollInfo(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	BOOL bRedraw=wParam;
	LPSCROLLINFO lpScrollInfo=(LPSCROLLINFO)lParam;
	//ATLTRACE("CSkinScrollBar::OnSetScrollInfo(bRedraw=%d, fMask=%x)\n", bRedraw, lpScrollInfo->fMask);
	if (lpScrollInfo->fMask&SIF_PAGE) m_si.nPage = lpScrollInfo->nPage;
	if(lpScrollInfo->fMask&SIF_POS) m_si.nPos=lpScrollInfo->nPos;
	if(lpScrollInfo->fMask&SIF_RANGE)
	{
		m_si.nMin=lpScrollInfo->nMin;
		m_si.nMax=lpScrollInfo->nMax;
	}
	if (bRedraw)
	{
		HDC hDC = ::GetDC(m_hWnd);
		CDC memdc;
		memdc.CreateCompatibleDC(hDC);
		HGDIOBJ hOldBmp = ::SelectObject(memdc, m_hBmp);

		RECT rcSour = GetImageRect(SB_PAGEUP);
		RECT rcDest = GetRect(SB_PAGEUP);
		TileBlt(hDC, &rcDest, &memdc, &rcSour);
		rcDest = GetRect(SB_THUMBTRACK);
		rcSour = GetImageRect(SB_THUMBTRACK);
		DrawThumb(hDC, &rcDest, &memdc, &rcSour);
		rcDest = GetRect(SB_PAGEDOWN);
		rcSour = GetImageRect(SB_PAGEDOWN);
		TileBlt(hDC, &rcDest, &memdc, &rcSour);
		::SelectObject(memdc, hOldBmp);
		ReleaseDC(hDC);
	}
	return TRUE;
}

LRESULT CSkinScrollBar::OnGetScrollInfo(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LPSCROLLINFO lpScrollInfo=(LPSCROLLINFO)lParam;
	int nMask=lpScrollInfo->fMask;
	if(nMask&SIF_PAGE) lpScrollInfo->nPage=m_si.nPage;
	if(nMask&SIF_POS) lpScrollInfo->nPos=m_si.nPos;
	if(nMask&SIF_TRACKPOS) lpScrollInfo->nTrackPos=m_si.nTrackPos;
	if(nMask&SIF_RANGE)
	{
		lpScrollInfo->nMin=m_si.nMin;
		lpScrollInfo->nMax=m_si.nMax;
	}
	ATLTRACE("CSkinScrollBar::OnGetScrollInfo(fMask=%x)\n", nMask);
	return TRUE;
}
