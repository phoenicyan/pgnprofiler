/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"
#include "gui/SkinWindow.h"

class CLikeRegexDlg
	: public CDialogImpl<CLikeRegexDlg>
	, public CSkinWindow<CLikeRegexDlg>
	, public CWinDataExchange<CLikeRegexDlg>
{
public:
	CColorPalette& m_palit;
	int cmbOperator;
	WCHAR sRegex[2048];

	enum { IDD = IDD_LIKE_REGEX };

	BEGIN_MSG_MAP(CLikeRegexDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CTLCOLORDLG, OnColorDlg) // For dialog background.
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnColorDlg) // For static and read only edit.
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)

		CHAIN_MSG_MAP(CSkinWindow<CLikeRegexDlg>)
	END_MSG_MAP()

	BEGIN_DDX_MAP(CLikeRegexDlg)
		DDX_TEXT_LEN(IDC_REGEX_EDIT, sRegex, 2048)
	END_DDX_MAP()

	CLikeRegexDlg(CColorPalette& palit) : m_palit(palit), cmbOperator(0)
		, CSkinWindow<CLikeRegexDlg>(palit)
	{
		sRegex[0] = 0;
	}

	// Handlers
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CenterWindow(GetParent());

		CComboBox cmb(GetDlgItem(IDC_OPERATOR_CBX));
		cmb.AddString(L"ILIKE");
		cmb.AddString(L"LIKE");
		cmb.SetCurSel(cmbOperator);

		DoDataExchange(FALSE);

		::SetWindowText(GetDlgItem(IDC_EXAMPLE_EDIT), L"^select(.)+");
		::SetFocus(GetDlgItem(IDC_REGEX_EDIT));
		return FALSE;
	}

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		DoDataExchange(TRUE);

		CComboBox cmb(GetDlgItem(IDC_OPERATOR_CBX));
		cmbOperator = cmb.GetCurSel();

		EndDialog(wID);
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
};
