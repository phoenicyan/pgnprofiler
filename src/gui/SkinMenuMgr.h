/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"
#include "hookmgr.h"
#include <atlcoll.h>

class CSkinMenu;

// pure static class
class CSkinMenuMgr : protected CHookMgr<CSkinMenuMgr>
{
	friend class CHookMgr<CSkinMenuMgr>; // to allow access to protected c'tor

public:
	CSkinMenuMgr(CColorPalette& palit);

	virtual ~CSkinMenuMgr();

	static BOOL Initialize(DWORD dwMenuStyle /*= SKMS_SIDEBAR | SKMS_FLAT*/, int nSBWidth = 10, BOOL bNotXP = TRUE);

protected:
	ATL::CAtlMap<HWND, CSkinMenu*> m_mapMenus;
	DWORD m_dwMenuStyle;
	CColorPalette& m_palit;
	int m_nSidebarWidth;
	CSkinMenu* m_pCurSkinMenu;
	HWND m_hCurContextWnd;
	HMENU m_hCurMenu;

protected:
	CSkinMenuMgr();

	// message handlers
	virtual void OnCallWndProc(const MSG& msg);
	virtual BOOL OnCbt(int nCode, WPARAM wParam, LPARAM lParam);

	CSkinMenu* GetSkinMenu(HWND hWnd);
	BOOL Skin(HWND hWnd); 
	BOOL Unskin(HWND hWnd);
	CSkinMenu* GetParentSkinMenu(HMENU hMenu);
	void Release();
};
