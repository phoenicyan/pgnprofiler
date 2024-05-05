/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"

#include "SkinMenuMgr.h"
#include "skinmenu.h"

CSkinMenuMgr::CSkinMenuMgr(CColorPalette& palit)
	: m_palit(palit)
	, m_dwMenuStyle(SKMS_SIDEBAR)
	, m_nSidebarWidth(10)
	, m_pCurSkinMenu(NULL)
	, m_hCurContextWnd(NULL)
	, m_hCurMenu(NULL)
{}

CSkinMenuMgr::~CSkinMenuMgr()
{
	// cleanup any remaining windows
	ATLASSERT (!m_mapMenus.GetCount());

	if (m_mapMenus.GetCount() != 0)
	{
		HWND hwnd;
		CSkinMenu* pSkin;
		POSITION pos = m_mapMenus.GetStartPosition();

		while (pos)
		{
			m_mapMenus.GetNextAssoc(pos, hwnd, pSkin);

			if (pSkin)
				delete pSkin;
		}

		m_mapMenus.RemoveAll();
	}

	//m_skGlobals.Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// static methods start

BOOL CSkinMenuMgr::Initialize(DWORD dwMenuStyle, int nSBWidth, BOOL bNotXP)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenuMgr::Initialize()\n");

	ATLVERIFY(GetHookMgrInstance().InitHooks());

	GetHookMgrInstance().m_dwMenuStyle = dwMenuStyle;
	GetHookMgrInstance().m_nSidebarWidth = nSBWidth;

	return TRUE;
}

// static methods end
///////////////////////////////////////////////////////////////////////////////////

void CSkinMenuMgr::OnCallWndProc(const MSG& msg)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenuMgr::OnCallWndProc(0x%04x)\n", msg.message);

	static BOOL bSkinning = FALSE; // initialized first time only but persistent

	if (!bSkinning)
	{
		bSkinning = TRUE;

#if 1
		// skin/unskin menus at each and every opportunity 
		switch (msg.message)
		{
		case WM_CREATE:
			ATLTRACE2(atlTraceDBProvider, 3, " -- WM_CREATE\n");
			if (CSkinMenu::IsMenuWnd(msg.hwnd))
			{
				BOOL bRes = Skin(msg.hwnd);
			}
			break;
			
		case WM_WINDOWPOSCHANGING: 
			if (CSkinMenu::IsMenuWnd(msg.hwnd) && msg.lParam)
			{
				ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenuMgr::OnCallWndProc(WM_WINDOWPOSCHANGING, hWnd=x%x, lParam=%d)\n", msg.hwnd, msg.lParam);

				WINDOWPOS* pWPos = (WINDOWPOS*)msg.lParam;

				if (pWPos->flags & SWP_SHOWWINDOW)
				{
					ATLTRACE2(atlTraceDBProvider, 3, " -- WM_WINDOWPOSCHANGING -> Skin(%08x)\n", msg.hwnd);
					BOOL bRes = Skin(msg.hwnd);
				}
				else if (pWPos->flags & SWP_HIDEWINDOW)
				{
					ATLTRACE2(atlTraceDBProvider, 3, " -- WM_WINDOWPOSCHANGING -> Unskin(%08x)\n", msg.hwnd);
					BOOL bRes = Unskin(msg.hwnd);
				}
			}
			break;

		case 0x1e2:
			ATLTRACE2(atlTraceDBProvider, 3, " -- 0x1e2\n");
			if (CSkinMenu::IsMenuWnd(msg.hwnd) && msg.wParam)
			{
				BOOL bRes = Skin(msg.hwnd);

				if (!bRes)
					ATLTRACE("Skin failed on 0x1e2");
			}
			break;
			
		case WM_SHOWWINDOW: 
			ATLTRACE2(atlTraceDBProvider, 3, " -- WM_SHOWWINDOW\n");
			if (CSkinMenu::IsMenuWnd(msg.hwnd))
			{
				if (msg.wParam)
				{
					BOOL bRes = Skin(msg.hwnd);
				}
				else // if (!msg.wParam)
				{
					BOOL bRes = Unskin(msg.hwnd);
				}
			}
			break;

		//case WM_SIZE:
		//	ATLTRACE2(atlTraceDBProvider, 3, " -- WM_SIZE\n");
		//	break;

		case WM_DESTROY:
		case WM_NCDESTROY:
			ATLTRACE2(atlTraceDBProvider, 3, " -- WM_DESTROY\n");
			if (CSkinMenu::IsMenuWnd(msg.hwnd))
			{
				BOOL bRes = Unskin(msg.hwnd);
			}
			break;

			// grab the menu handle at each and every opportunity
			//
			// notes:
			//
			// 1. menu bars generate a WM_INITMENUPOPUP prior to showing their menus
			//
			// 2. edit windows do not generate WM_INITMENUPOPUP prior to showing
			//    their context menu so the best we can do is grab their window handle
			//
			// 3. other controls display their context menus prior to generating
			//    a WM_INITMENUPOPUP

		case WM_CONTEXTMENU: // means a menu may be about to appear
			{
				ATLTRACE2(atlTraceDBProvider, 3, " -- WM_CONTEXTMENU\n");
				m_pCurSkinMenu = NULL;
				m_hCurContextWnd = msg.hwnd;

				TCHAR szWndClass[128] = _T("");
				::GetClassName(msg.hwnd, szWndClass, 127);
				ATLTRACE ("WM_CONTEXTMENU sent to window of type '%S'\n", szWndClass);
			}
			break;

		case WM_INITMENUPOPUP:
			if (m_pCurSkinMenu && !m_pCurSkinMenu->GetMenu())
			{
				ATLTRACE2(atlTraceDBProvider, 3, " -- WM_INITMENUPOPUP\n");
				m_pCurSkinMenu->SetContextWnd(NULL);
				m_pCurSkinMenu->SetMenu((HMENU)msg.wParam, GetParentSkinMenu((HMENU)msg.wParam));

				m_hCurMenu = NULL;
				m_pCurSkinMenu = NULL;
				m_hCurContextWnd = NULL;
			}
			else
			{
				ATLTRACE2(atlTraceDBProvider, 3, " -- WM_INITMENUPOPUP save for menu skinning\n");
				m_hCurMenu = (HMENU)msg.wParam;
				//BOOL bHandled = TRUE;
				//if ((BOOL)HIWORD(msg.lParam))
				//{	// System menu, do nothing
				//	bHandled = FALSE;
				//}
			}
			break;
		}
#endif
		bSkinning = FALSE;
	}
}

BOOL CSkinMenuMgr::OnCbt(int nCode, WPARAM wParam, LPARAM lParam)
{   
	if (nCode == HCBT_SYSCOMMAND)
	{
		switch (wParam)
		{
		case SC_MOUSEMENU:
			ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenuMgr::OnCbt(HCBT_SYSCOMMAND, SC_MOUSEMENU)\n");
			if (lParam)
			{
				int xPos = GET_X_LPARAM(lParam);
				int yPos = GET_Y_LPARAM(lParam);

				HWND hwnd = WindowFromPoint(CPoint(xPos, yPos));

				if (hwnd && (GetWindowLong(hwnd, GWL_STYLE) & WS_SYSMENU))
				{
					// convert to window coords
					CRect rWindow;
					GetWindowRect(hwnd, rWindow);

					CPoint ptScreen(xPos, yPos);

					xPos -= rWindow.left;
					yPos -= rWindow.top;

					if (xPos > 0 && xPos < 16 && yPos > 0 && yPos < 16)
					{
						///CSkinBase::DoSysMenu(CWnd::FromHandle(hwnd), ptScreen, NULL, TRUE);
						return TRUE; // handled
					}
				}
			}
			break;
		}
	}

	return FALSE;
}

CSkinMenu* CSkinMenuMgr::GetSkinMenu(HWND hWnd)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenuMgr::GetSkinMenu(%08x)\n", hWnd);

	CSkinMenu* pSkin = NULL;
	
	m_mapMenus.Lookup(hWnd, pSkin);
	return pSkin;
}

BOOL CSkinMenuMgr::Unskin(HWND hWnd)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenuMgr::Unskin(%08x)\n", hWnd);

	m_pCurSkinMenu = NULL;
	m_hCurContextWnd = NULL;
	m_hCurMenu = NULL;

	ATLASSERT (CSkinMenu::IsMenuWnd(hWnd));

	CSkinMenu* pSkinMenu = GetSkinMenu(hWnd);
	
	if (!pSkinMenu)
		return TRUE; // already done
	
	ATLTRACE2(atlTraceDBProvider, 3, "menu unskinned (%08X)\n", (UINT)hWnd);
	m_mapMenus.RemoveKey(hWnd);

	pSkinMenu->DetachWindow();
	//delete pSkinMenu;

	return TRUE;
}

BOOL CSkinMenuMgr::Skin(HWND hWnd)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenuMgr::Skin(%08x)\n", hWnd);

	ATLASSERT (CSkinMenu::IsMenuWnd(hWnd));

	CSkinMenu* pSkinMenu = GetSkinMenu(hWnd);
	
	if (pSkinMenu)
	{
		ATLTRACE2(atlTraceDBProvider, 3, "  -- already skinned %08x\n", hWnd);
		return TRUE;
	}

	pSkinMenu = new CSkinMenu(m_palit, m_dwMenuStyle, m_nSidebarWidth);
	
	if (pSkinMenu && pSkinMenu->AttachWindow(hWnd))
	{
		ATLTRACE2(atlTraceDBProvider, 3, "menu skinned (%08X)\n", (UINT)hWnd);

		m_pCurSkinMenu = pSkinMenu;
		m_pCurSkinMenu->SetContextWnd(m_hCurContextWnd);
		m_pCurSkinMenu->SetMenu(m_hCurMenu, GetParentSkinMenu(m_hCurMenu));

		m_mapMenus[hWnd] = pSkinMenu;

		return TRUE;
	}
	
	// else
	delete pSkinMenu;

	return FALSE;
}

CSkinMenu* CSkinMenuMgr::GetParentSkinMenu(HMENU hMenu)
{
	if (!hMenu)
		return NULL;

	ATLTRACE2(atlTraceDBProvider, 3, "CSkinMenuMgr::GetParentSkinMenu(hMenu=%8x)\n", hMenu);

	// search the map if CSkinMenus looking for a menu
	// having this menu as a popup
	HWND hwnd;
	CSkinMenu* pSkin;
	POSITION pos = m_mapMenus.GetStartPosition();

	while (pos)
	{
		m_mapMenus.GetNextAssoc(pos, hwnd, pSkin);

		const HMENU hOther = pSkin->GetMenu();

		if (hOther && hOther != hMenu)
		{
			// iterate the items looking for submenus
			int nMenu = GetMenuItemCount(hOther);

			while (nMenu-- > 0)
			{
				if (GetSubMenu(hOther, nMenu) == hMenu) // submenu
					return pSkin;
			}
		}
	}
	
	return NULL;
}
