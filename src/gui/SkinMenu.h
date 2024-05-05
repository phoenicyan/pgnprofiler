/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"

// style flags
enum
{
	SKMS_ROUNDCORNERS = 0x0001,
	SKMS_SIDEBAR = 0x0002,
	SKMS_FLAT = 0x0004,
};

//////////////////////////

#define WC_MENUA              "#32768"	// ansi
#define WC_MENUW              L"#32768"	// wide

#ifdef UNICODE
#define WC_MENU               WC_MENUW
#else
#define WC_MENU               WC_MENUA
#endif


#define MAX_MENU_ITEM_TEXT_LENGTH       (100)
#define IMGPADDING                      (6)
#define TEXTPADDING                     (8)

#define SIDEBAR_FONT					(_T("TREBUCHET"))

// Bit flags for "fExtra":
#define MFT_EX_HTITLE					(0x00000001L)
#define MFT_EX_VTITLE					(0x00000002L)
#define MFT_EX_KEEP						(0x00000004L)

struct MenuItemData
{
	LPTSTR lpstrText;
	UINT fType;
	UINT fState;
	UINT fExtra;
	HICON hIcon;
};

//////////////////////////

class ISkinMenuRender
{
public:
	virtual BOOL DrawMenuBorder(CDC* pDC, LPRECT pRect) { return FALSE; }
	virtual BOOL DrawMenuSidebar(CDC* pDC, LPRECT pRect, LPCTSTR szTitle = NULL) { return FALSE; }
	virtual BOOL DrawMenuClientBkgnd(CDC* pDC, LPRECT pRect, LPRECT pClip) { return FALSE; }
	virtual BOOL DrawMenuNonClientBkgnd(CDC* pDC, LPRECT pRect) { return FALSE; }
};

class CMenuWithHWND : public CMenuT<true>
{
public:
	HWND m_hWnd;

	inline HWND GetHwnd() { return m_hWnd; }
};

class CSkinGlobals;

#define MN_1e5	0x01e5

class CSkinMenu : public CWindowImpl<CSkinMenu, CMenuWithHWND>
{
public:
	CSkinMenu(CColorPalette& palit, DWORD dwStyle = SKMS_SIDEBAR, int nSBWidth = 10);
	virtual ~CSkinMenu();
	
	BEGIN_MSG_MAP(CSkinMenu)
		MESSAGE_HANDLER(WM_NCPAINT, OnNcPaint)
		MESSAGE_HANDLER(WM_PRINT, OnPrint)
		MESSAGE_HANDLER(WM_PRINTCLIENT, OnPrintClient)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		//MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
		//MESSAGE_HANDLER(MN_1e5, OnMenu1e5)
		MESSAGE_HANDLER(WM_NCCALCSIZE, OnNcCalcSize)
		MESSAGE_HANDLER(WM_WINDOWPOSCHANGING, OnWindowPosChanging)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
	END_MSG_MAP()

	LRESULT OnNcPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnPrint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnPrintClient(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	//LRESULT OnKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	//LRESULT OnMenu1e5(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnNcCalcSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnWindowPosChanging(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	virtual BOOL AttachWindow(HWND hWnd);
	BOOL DetachWindow();

	void SetContextWnd(HWND hWnd);
	void SetMenu(HMENU hMenu, CSkinMenu* pParentMenu = NULL);

	const HMENU GetMenu() const { return m_hMenu; }

	static BOOL IsMenuWnd(HWND hWnd);
	//static void SetRenderer(ISkinMenuRender* pRenderer) { s_pRenderer = pRenderer; }

protected:
	int m_nSelIndex;
	DWORD m_dwStyle;
	CColorPalette& m_palit;
	int m_nSidebarWidth;
	//HWND m_hContextWnd;		// unused field
	CSkinMenu* m_pParentMenu;

	BOOL m_bAnimatedMenus;
	BOOL m_bFirstRedraw; // fix for animated menus

	//static ISkinMenuRender* s_pRenderer;

protected:
	virtual void OnNcPaint(CDC* pDC);
	virtual void OnPrintClient(CDC* pDC, DWORD dwFlags);
	virtual void OnPaint(CDC* pDC);

	//BOOL ProcessWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult, DWORD dwMsgMapID = 0);
	BOOL IsHooked() { return m_hWnd != NULL; } // is it hooked up and ready to go

	void GetDrawRect(LPRECT pWindow, LPRECT pClient = NULL);
	void SetTransparent();
	CDC* ReplaceSystemColors(CDC* pDCDest, CDC* pDestSrc, LPRECT pRect, LPRECT pClip);

	void GetInvalidRect(int nCurSel, int nPrevSel, LPRECT lpRect); // in client coords
	int GetCurSel();

	// style helpers
	BOOL Sidebar() { return m_dwStyle & SKMS_SIDEBAR; }
	BOOL RoundCorners() { return m_dwStyle & SKMS_ROUNDCORNERS; }
	BOOL Flat() { return m_dwStyle & SKMS_FLAT; }

	inline void SwapDCs(CDC*& pDC1, CDC*& pDC2)
	{
		CDC* pTemp = pDC1;
		pDC1 = pDC2;
		pDC2 = pTemp;
	}

	inline HWND GetCWnd() const
	{
		return m_hWnd;
	}
};
