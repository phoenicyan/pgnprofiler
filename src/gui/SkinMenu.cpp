/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "SkinMenu.h"
#include "SkinMenuMgr.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//ISkinMenuRender* CSkinMenu::s_pRenderer = NULL;

enum { REDRAWALL = -2 };

#ifndef SPI_GETMENUANIMATION
#define SPI_GETMENUANIMATION  0x1002
#endif

#ifndef SPI_GETMENUFADE
#define SPI_GETMENUFADE  0x1012
#endif

#define TITLE_BG						(m_palit.CrBackGndAlt)
#define TITLE_TX						(m_palit.CrTextAlt)
#define MENU_BG							(m_palit.CrMenuBkg)
#define ICON_BG							(m_palit.CrIconBkg)
#define MENU_TX							(m_palit.CrText)
#define MENU_DT							(m_palit.CrMenuDT)
#define SELECT_BG						(m_palit.CrMenuSelectBkg)
#define SELECT_BR						(m_palit.CrTextAlt)
#define SEPARATOR_PN					(m_palit.CrTextAlt)

#ifndef OBM_CHECK
#define OBM_CHECK                       32760
#endif

#define BitmapCX  16
#define BitmapCY  15
#define ButtonCX  (BitmapCX + 6)
#define ButtonCY  (BitmapCY + 6)

//////////////////////////////////////////////////////////////////////

struct colorMapping
{
	int nSrcColor;
	int nDestColor;
};

static colorMapping colors[] = 
{
//	{ COLOR_HIGHLIGHT, COLOR_HIGHLIGHT },
	{ COLOR_WINDOWTEXT, COLOR_WINDOWTEXT },
	{ COLOR_GRAYTEXT, COLOR_MENU },
	{ COLOR_HIGHLIGHTTEXT, COLOR_HIGHLIGHTTEXT },
	{ COLOR_3DHILIGHT, COLOR_MENU },
//	{ COLOR_3DDKSHADOW, COLOR_MENU },
	{ COLOR_3DSHADOW, COLOR_3DSHADOW },
	{ COLOR_3DFACE, COLOR_MENU },
	{ COLOR_MENU, COLOR_MENU },

};

// Transforms an RGB colour by increasing or reducing its luminance and/or saturation in HLS space.
COLORREF HLS_TRANSFORM(COLORREF rgb, int percent_L, int percent_S)
{
	WORD h, l, s;
	ColorRGBToHLS(rgb, &h, &l, &s);
	if (percent_L > 0)
	{
		l = WORD(l + ((240 - l) * percent_L) / 100);
	}
	else if (percent_L < 0)
	{
		l = WORD((l * (100 + percent_L)) / 100);
	}
	if (percent_S > 0)
	{
		s = WORD(s + ((240 - s) * percent_S) / 100);
	}
	else if (percent_S < 0)
	{
		s = WORD((s * (100 + percent_S)) / 100);
	}
	if (l > 240) l = 240; if (l < 0) l = 0;
	if (s > 240) s = 240; if (s < 0) s = 0;
	return ColorHLSToRGB(h, l, s);
}

CSkinMenu::CSkinMenu(CColorPalette& palit, DWORD dwStyle, int nSBWidth)
	: m_palit(palit), m_nSidebarWidth(nSBWidth), m_dwStyle(dwStyle), m_pParentMenu(NULL)
{
	m_nSelIndex = REDRAWALL; // this ensures a full repaint when we first show
	//m_hContextWnd = NULL;
	m_hWnd = NULL;
	m_hMenu = NULL;

	// fix for animated menus
	m_bFirstRedraw = TRUE;

	m_bAnimatedMenus = FALSE;
	SystemParametersInfo(SPI_GETMENUANIMATION, 0, &m_bAnimatedMenus, 0);
}

CSkinMenu::~CSkinMenu()
{
}

BOOL CSkinMenu::IsMenuWnd(HWND hWnd)
{
	TCHAR szWndClass[128] = _T("");

	::GetClassName(hWnd, szWndClass, 127);
	return (lstrcmpi(WC_MENU, szWndClass) == 0);
}

BOOL CSkinMenu::AttachWindow(HWND hWnd) 
{ 
	if (!IsMenuWnd(hWnd))
		return FALSE;
	
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::AttachWindow(hWnd=%08x)\n", hWnd);

	if (SubclassWindow(hWnd))
	{
		return TRUE;
	}
	
	// else
	return FALSE;
}

BOOL CSkinMenu::DetachWindow() 
{ 
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::DetachWindow()  m_hWnd=%08x\n", m_hWnd);
	UnsubclassWindow();
	return TRUE;
}

LRESULT CSkinMenu::OnNcPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::ProcessWindowMessage(WM_NCPAINT)\n");

	// the very first WM_NCPAINT appears to be responsible for
	// doing any menu animation. since our std OnNcPaint does not
	// deal with animation we must leave it to the default handler.
	// fortunately, the default handler calls WM_PRINT to implement
	// the animation.
	if (!m_bAnimatedMenus || !m_bFirstRedraw)
	{
		OnNcPaint(&CDC(GetDC(m_hWnd)));	///GetWindowDC()
		return 0;
	}

	bHandled = FALSE;

	return 1;
}

LRESULT CSkinMenu::OnPrint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::ProcessWindowMessage(WM_PRINT)\n");
	LRESULT lResult = GetWindowProc()(m_hWnd, uMsg, wParam, 0);//CSubclassWnd::WindowProc(hWnd, uMsg, wParam, lResult);
	OnNcPaint(&CDC((HDC)wParam));
	return lResult;
}

LRESULT CSkinMenu::OnPrintClient(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::ProcessWindowMessage(WM_PRINTCLIENT)\n");
	OnPrintClient(&CDC((HDC)wParam), 0);
	return 0;
}

LRESULT CSkinMenu::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::ProcessWindowMessage(WM_PAINT)\n");
	//LPDRAWITEMSTRUCT lpDrawItemStruct = (LPDRAWITEMSTRUCT)lParam;
	//MenuItemData* pMI = (MenuItemData*)lpDrawItemStruct->itemData;
	//if (lpDrawItemStruct->CtlType == ODT_MENU && pMI != NULL)
	//{
	//	//DrawItem(lpDrawItemStruct);
	//}
	CPaintDC dc(GetCWnd());
	//SendMessage(GetCWnd(), WM_PRINTCLIENT, (WPARAM)(HDC)dc, PRF_CLIENT | PRF_CHECKVISIBLE);

	OnPrintClient(WM_PRINTCLIENT, (WPARAM)(HDC)dc, PRF_CLIENT | PRF_CHECKVISIBLE, bHandled);

	return 0;
}


LRESULT CSkinMenu::OnNcCalcSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT lResult = 0;
	if (Sidebar())
	{
		//lResult = DefWindowProc();

		LPRECT pRect = wParam ? &((LPNCCALCSIZE_PARAMS)lResult)->rgrc[0] : (LPRECT)lResult;
		pRect->left += m_nSidebarWidth;

		//return lResult;
	}

	return 0;
}

LRESULT CSkinMenu::OnWindowPosChanging(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	WINDOWPOS* pWP = (WINDOWPOS*)lParam;
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::ProcessWindowMessage(WM_WINDOWPOSCHANGING)\n");

	// adjust width for sidebar
	if (Sidebar() && !(pWP->flags & SWP_NOSIZE))
		pWP->cx += m_nSidebarWidth;

	// if we have a parent menu we may need to adjust our
	// pos to avoid client repainting issues
	if (m_pParentMenu && !(pWP->flags & SWP_NOMOVE))
	{
		// if we are on the right side of our parent
		// then we need to adjust ourselves to avoid the client rect
		CRect rParentWindow;
		::GetWindowRect(m_pParentMenu->GetHwnd(), rParentWindow);

		if (pWP->x > rParentWindow.left) // right
		{
			CRect rParentClient;
			::GetClientRect(m_pParentMenu->GetHwnd(), rParentClient);

			::ClientToScreen(m_pParentMenu->m_hWnd, (LPPOINT)&rParentClient);
			::ClientToScreen(m_pParentMenu->m_hWnd, &((LPPOINT)&rParentClient)[1]);

			pWP->x = rParentClient.right;
		}
	}

	return 0;
}

LRESULT CSkinMenu::OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	return TRUE;
}

void CSkinMenu::OnPrintClient(CDC* pDC, DWORD dwFlags)
{
	CRect rClient;
	GetClientRect(GetHwnd(), rClient);

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::OnPrintClient(%d, %d, %d, %d)\n", rClient.left, rClient.top, rClient.right, rClient.bottom);

	CRect rClip(rClient);
	GetClipBox(pDC->m_hDC, rClip);

	// create standard back buffer and another dc on which 
	// to layer the background and foreground
	CDC dcMem, dcMem2;
	dcMem.CreateCompatibleDC(NULL);
	dcMem2.CreateCompatibleDC(NULL);
	
	// use screen dc for creating bitmaps because
	// menu dc's seem not to be standard.
	HDC pDCScrn = GetDC(GetDesktopWindow());
	
	CBitmap bmMem, bmMem2;
	bmMem.CreateCompatibleBitmap(pDCScrn, rClient.right, rClient.bottom);
	bmMem2.CreateCompatibleBitmap(pDCScrn, rClient.right, rClient.bottom);

	// release screen dc asap
	ReleaseDC(GetDesktopWindow(), pDCScrn);
	
	// prepare dc's
	dcMem.SetBkMode(TRANSPARENT);
	dcMem.SetBkColor(GetSysColor(COLOR_MENU));

	HBITMAP pOldBM = dcMem.SelectBitmap(bmMem.m_hBitmap);
	HFONT pOldFont = dcMem.SelectFont(m_palit.m_titleFont);
	HBITMAP pOldBM2 = dcMem2.SelectBitmap(bmMem2.m_hBitmap);

	// trim clip rgn
	if (rClip.top)
		dcMem.ExcludeClipRect(0, 0, rClient.right, rClip.top);

	if (rClip.bottom < rClient.bottom)
		dcMem.ExcludeClipRect(0, rClip.bottom, rClient.right, rClient.bottom);

	// draw background
	dcMem.FillSolidRect(rClip, m_palit.CrMenuBkg);	// m_palit.CrBackGndAlt

	// default draw
	DefWindowProc(WM_PRINTCLIENT, (WPARAM)(HDC)dcMem, (LPARAM)dwFlags);
	
	// blt the lot to pDC
	pDC->BitBlt(rClip.left, rClip.top, rClip.Width(), rClip.Height(), dcMem, rClip.left, rClip.top, SRCCOPY);
	
	// cleanup
	dcMem.SelectBitmap(pOldBM);
	dcMem.SelectFont(pOldFont);
	dcMem.DeleteDC();
	bmMem.DeleteObject();
	
	dcMem2.SelectBitmap(pOldBM2);
	dcMem2.DeleteDC();
	bmMem2.DeleteObject();
}

void CSkinMenu::OnPaint(CDC* pDC)
{
	// construct a back buffer for the default draw
	CRect rClient;
	GetClientRect(GetHwnd(), rClient);
	
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::OnPaint(%d, %d, %d, %d)\n", rClient.left, rClient.top, rClient.right, rClient.bottom);

	CBitmap bmMem;
	bmMem.CreateCompatibleBitmap(pDC->m_hDC, rClient.right, rClient.bottom);
	
	CDC dcMem;
	dcMem.CreateCompatibleDC(NULL);
	
	dcMem.SetBkMode(TRANSPARENT);
	dcMem.SetBkColor(GetSysColor(COLOR_MENU));
	HBITMAP pOldBM = dcMem.SelectBitmap(bmMem.m_hBitmap);
	HFONT pOldFont = dcMem.SelectFont(m_palit.m_titleFont);
	
	// draw background
	dcMem.FillSolidRect(rClient, GetSysColor(COLOR_MENU));
	
	// default draw
	//DefWindowProc(WM_PAINT, (WPARAM)(HDC)dcMem, 0);
	
	// create another dc on which to layer the background and foreground
	CDC dcMem2;
	dcMem2.CreateCompatibleDC(NULL);
	
	CBitmap bmMem2;
	bmMem2.CreateCompatibleBitmap(pDC->m_hDC, rClient.right, rClient.bottom);
	HBITMAP pOldBM2 = dcMem2.SelectBitmap(bmMem2.m_hBitmap);
	
	// blt the lot to pDC
	pDC->BitBlt(0, 0, rClient.right, rClient.bottom, dcMem, 0, 0, SRCCOPY);
	
	// cleanup
	dcMem.SelectBitmap(pOldBM);
	dcMem.SelectFont(pOldFont);
	dcMem.DeleteDC();
	bmMem.DeleteObject();
	
	dcMem2.SelectBitmap(pOldBM2);
	dcMem2.DeleteDC();
	bmMem2.DeleteObject();
}

void CSkinMenu::OnNcPaint(CDC* pDC)
{
	CRect rWindow, rClient;
	GetDrawRect(rWindow, rClient);

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::OnNcPaint(%d, %d, %d, %d)\n", rClient.left, rClient.top, rClient.right, rClient.bottom);

	CRect rClip;
	pDC->GetClipBox(rClip);

	//ATLTRACE ("CSkinMenu::OnNcPaint(clip box = {%d x %d})\n", rClip.Width(), rClip.Height());

	// back buffer
	CBitmap bmMem;
	bmMem.CreateCompatibleBitmap(pDC->m_hDC, rWindow.right, rWindow.bottom);
	
	CDC dcMem;
	dcMem.CreateCompatibleDC(NULL);
	
	HBITMAP pOldBM = dcMem.SelectBitmap(bmMem.m_hBitmap);
	
	// blt to screen
	int nSaveDC = pDC->SaveDC(); // must restore dc to original state

	pDC->ExcludeClipRect(rClient);
	pDC->BitBlt(0, 0, rWindow.right, rWindow.bottom, dcMem, 0, 0, SRCCOPY);

	pDC->RestoreDC(nSaveDC);

	// cleanup
	dcMem.SelectBitmap(pOldBM);
}

void CSkinMenu::GetDrawRect(LPRECT pWindow, LPRECT pClient)
{
	CRect rWindow;
	GetWindowRect(GetHwnd(), rWindow);
	
	if (pClient)
	{
		GetClientRect(GetHwnd(), pClient);
		ClientToScreen(GetHwnd(), (LPPOINT)pClient);
		ClientToScreen(GetHwnd(), &((LPPOINT)pClient)[1]);
		::OffsetRect(pClient, -rWindow.left, -rWindow.top);
	}
	
	if (pWindow)
	{
		rWindow.OffsetRect(-rWindow.TopLeft());
		*pWindow = rWindow;
	}
}

void CSkinMenu::SetContextWnd(HWND hWnd) 
{ 
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::SetContextWnd(%x)\n", hWnd);
}

void CSkinMenu::SetMenu(HMENU hMenu, CSkinMenu* pParentMenu) 
{ 
	if (m_hMenu && hMenu) // already set
		return;
	
	// else
	if (hMenu && ::IsMenu(hMenu)) 
		m_hMenu = hMenu;
	else
		m_hMenu = NULL; 

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::SetMenu(hwnd = %08x, hmenu = %08x)\n", m_hWnd, hMenu);

	m_pParentMenu = pParentMenu;
}

void CSkinMenu::GetInvalidRect(int nCurSel, int nPrevSel, LPRECT lpRect)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenu::GetInvalidRect()\n");
	ATLASSERT (lpRect);

	if (!m_hMenu || nCurSel == REDRAWALL || nPrevSel == REDRAWALL)
		GetClientRect(GetHwnd(), lpRect);

	else if (m_hMenu)
	{
		::SetRectEmpty(lpRect);

		if (nCurSel >= 0 || nPrevSel >= 0)
		{
			if (nCurSel >= 0)
			{
				GetMenuItemRect(NULL, nCurSel, lpRect);
			}
					
			if (nPrevSel >= 0)
			{
				CRect rTemp;
			
				GetMenuItemRect(NULL, nPrevSel, rTemp);
				::UnionRect(lpRect, lpRect, rTemp);
			}

			// convert to client coords
			//ScreenToClient(GetHwnd(), lpRect);
		}
	}
}

int CSkinMenu::GetCurSel()
{
	ATLASSERT (m_hMenu);
	int nItem = GetMenuItemCount();
	
	while (nItem--)
	{
		if (GetMenuState(nItem, MF_BYPOSITION) & MF_HILITE)
			return nItem;
	}

	return -1; // nothing selected
}
