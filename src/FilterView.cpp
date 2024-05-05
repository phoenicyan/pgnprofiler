/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "resource.h"
#include "LikeRegexDlg.h"
#include "FilterView.h"

BOOL CFilterView::PreTranslateMessage(MSG* pMsg)
{
   pMsg;
   return FALSE;
}

LRESULT CFilterView::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	LRESULT lResult = DefWindowProc(uMsg, wParam, lParam);

	LOGFONT lf;
	::SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), &lf, 0);
	HFONT hFont = ::CreateFontIndirect(&lf);

	SetFont(hFont);

	return lResult;
}

LRESULT CFilterView::OnContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	HMENU hMenu = ::LoadMenu(_Module.GetResourceInstance(),(LPCTSTR)IDR_FILTERV_MENU);
	if (hMenu != NULL)
	{
		HMENU hSubMenu = ::GetSubMenu(hMenu,0);
		if(hSubMenu != NULL)
		{
			::TrackPopupMenu(hSubMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, NULL);

			::PostMessage(m_hWnd, WM_NULL, 0, 0); 
		}
		::DestroyMenu(hMenu);
	}
	return 0;
}

LRESULT CFilterView::OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CRect rect;
	GetClientRect(&rect);

	::FillRect((HDC)wParam, &rect, m_palit.m_brBackGndAlt);

	return 1;
}

LRESULT CFilterView::OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if ((MK_CONTROL & wParam) != 0)
	{
		LONG delta = (int)GET_WHEEL_DELTA_WPARAM(wParam) < 0 ? 2 : -2;

		AlterWindowFont(*this, delta);
		::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_FILTERFONT, 0);

		return 0;
	}

	bHandled = FALSE;
	return 1;
}

LRESULT CFilterView::OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	return DLGC_WANTALLKEYS;
}

LRESULT CFilterView::OnChar(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (::GetKeyState(VK_CONTROL) & 0x8000)
	{
		if (wParam == '.' || wParam == '>' || wParam == '+' || wParam == '=')
		{
			AlterWindowFont(*this, -2);
			::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_FILTERFONT, 0);
			return 1;
		}

		if (wParam == ',' || wParam == '<' || wParam == '-' || wParam == '_')
		{
			AlterWindowFont(*this, 2);
			::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_FILTERFONT, 0);
			return 1;
		}
	}

	bHandled = FALSE;
	return 0;
}

LRESULT CFilterView::OnEditCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nStartChar, nEndChar;
	GetSel(nStartChar, nEndChar);
	if (nStartChar == nEndChar)
	{
		SetSelAll();
		Copy();
		SetSelNone();
	}
	else
	{
		Copy();
	}
	return 0;
}

LRESULT CFilterView::OnEditPaste(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Paste();
	return 0;
}

LRESULT CFilterView::OnSendCmdToMainWnd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	GetParent().SendMessage(WM_COMMAND, MAKEWPARAM((WORD)wID, BN_CLICKED), (LPARAM)m_hWnd);
	return 0;
}

LRESULT CFilterView::OnPreconfiguredFilter(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	wstring wsFilter;
	bool filterOn = m_filterOn;	// save current filter status in local variable since it can change during execution of the routine.

	if (filterOn)
		GetParent().SendMessage(WM_COMMAND, MAKEWPARAM(ID_FILTER, BN_CLICKED), (LPARAM)m_hWnd);

	switch (wID)
	{
	case ID_PRECONFIGUREDFILTERS_CLENTSQLLIKE:
		{
			//wsFilter = L"clientsql ilike '^select(.)+'";
			CLikeRegexDlg dlg(m_palit);
			if (dlg.DoModal() != IDOK)
				return 0;

			wsFilter = L"clientsql ";
			wsFilter += (dlg.cmbOperator == 0) ? L"ilike '" : L"like '";
			wsFilter += dlg.sRegex;
			wsFilter += L"'";
		}
		break;
	case ID_PRECONFIGUREDFILTERS_SHOWERRORS:
		wsFilter = L"SQLType = ERROR";
		break;
	case ID_PRECONFIGUREDFILTERS_HIDESYSTEM:
		wsFilter = L"SQLType <> SYSTEM && SQLType <> SYS_SCHEMA";
		break;
	case ID_PRECONFIGUREDFILTERS_EXECUTETIME:
		wsFilter = L"execute > 1.0";
		break;
	case ID_PRECONFIGUREDFILTERS_SCHEMAALTERATIONS:
		wsFilter = L"(ClientSQL ilike 'Alter(.)+' || "
					L"ClientSQL ilike 'Create(.)+' || "
					L"ClientSQL ilike 'Drop(.)+') && "
					L"SQLType != ERROR";
		break;
	case ID_PRECONFIGUREDFILTERS_STOREDPROCEDURESANDNOTIFICATIONS:
		wsFilter = L"CMDType == PROCEDURE || CMDType == INTERNALPROC || CMDType == NOTIFY";
		break;
	}
	
	SetWindowText(wsFilter.c_str());

	int l = wsFilter.length();
	SendMessage(EM_SETSEL, l, l);
	SendMessage(EM_SCROLLCARET, 0, 0L);

	if (filterOn)
	{
		// allow UI update
		MSG msg = { 0 };
		while (::PeekMessage(&msg, m_hWnd, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		// restore filter state
		GetParent().SendMessage(WM_COMMAND, MAKEWPARAM(ID_FILTER, BN_CLICKED), (LPARAM)m_hWnd);
	}
	return 0;
}

LRESULT CFilterView::OnZoomin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	AlterWindowFont(*this, -2);
	::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_FILTERFONT, 0);
	return 0;
}

LRESULT CFilterView::OnZoomout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	AlterWindowFont(*this, 2);
	::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_FILTERFONT, 0);
	return 0;
}
