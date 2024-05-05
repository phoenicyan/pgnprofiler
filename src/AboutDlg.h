/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"
#include "gui\SkinWindow.h"

class CAboutDlg 
	: public CDialogImpl<CAboutDlg>
	, public CSkinWindow<CAboutDlg>
	, public CMessageFilter
{
public:
	WCHAR _appTitle[128];
	WCHAR _licensedTo[128];
	WCHAR _type[64];
	WCHAR _purchaseDate[32];
	WCHAR _special[64];
	WCHAR _subscription[128];

	CAboutDlg(CColorPalette& palit) : m_palit(palit)
		, CSkinWindow<CAboutDlg>(palit)
	{
		_appTitle[0] = 0;
		_licensedTo[0] = 0;
		_type[0] = 0;
		_purchaseDate[0] = 0;
		_special[0] = 0;
		_subscription[0] = 0;
	}

	enum { IDD = IDD_ABOUTBOX };

	BEGIN_MSG_MAP(CAboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CTLCOLORDLG,       OnColorDlg) // For dialog background.
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC,    OnColorDlg) // For static and read only edit.
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)

		CHAIN_MSG_MAP(CSkinWindow<CAboutDlg>)
		//REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return ::IsDialogMessage(m_hWnd, pMsg);
	}

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		HWND hParent = GetParent();
		CenterWindow(hParent);

		m_wndLink.SubclassWindow(GetDlgItem(IDC_PGNP_LINK));
		m_wndLink.SetHyperLink(_T("http://www.pgoledb.com/"));

		GetDlgItem(IDC_PRODVER).SetWindowText(_appTitle);
		GetDlgItem(IDC_LICENSEDTO).SetWindowText((wstring(L"Licensed To: ") + _licensedTo).c_str());
		GetDlgItem(IDC_TYPE).SetWindowText((wstring(L"Type: ") + _type).c_str());
		GetDlgItem(IDC_PURCHASEDATE).SetWindowText((wstring(L"Purchase Date: ") + _purchaseDate).c_str());
		GetDlgItem(IDC_LICENSE).SetWindowText((wstring(L"License: ") + _special).c_str());
		GetDlgItem(IDC_SUBSCRIPTION).SetWindowText(_subscription[0] ? (wstring(L"Subscription: ") + _subscription).c_str() : L"");

		// register object for message filtering
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		pLoop->AddMessageFilter(this);

		::EnableWindow(hParent, FALSE);
		return TRUE;
	}

	LRESULT OnKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		switch ((UINT)wParam)
		{
			case VK_SPACE:
			case VK_RETURN:
				break;
		}
		return 0;
	}

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		//EndDialog(wID);
		HWND hParent = GetParent();
		::EnableWindow(hParent, TRUE);
		m_wndLink.DestroyWindow();
		DestroyWindow();
		return 0;
	}

	LRESULT OnColorDlg(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		HDC hdc = (HDC)wParam;

		::SetTextColor(hdc, m_palit.CrText);
		::SetBkColor(hdc, m_palit.CrBackGnd);
		::SelectObject(hdc, m_palit.m_brBackGnd);
		
		return (LRESULT)m_palit.m_brBackGnd.m_hBrush;
	}

	LRESULT OnDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
	{
		m_palit.DrawButton((LPDRAWITEMSTRUCT)lParam);
		return TRUE;
	}

protected:
	CHyperLink m_wndLink;
	CColorPalette& m_palit;
};
