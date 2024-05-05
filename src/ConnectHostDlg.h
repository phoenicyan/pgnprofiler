/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"
#include "PGNPOptions.h"
#include "resource.h"
#include "gui/SkinWindow.h"

class CConnectHostDlg
	: public CDialogImpl<CConnectHostDlg>
	, public CWinDataExchange<CConnectHostDlg>
	, public CSkinWindow<CConnectHostDlg>
	, public CMessageFilter
{
	CPGNPOptions& m_optionsForEdit;
	CColorPalette& m_palit;

public:
	enum { IDD = IDD_CONNECT };

	BEGIN_MSG_MAP(CConnectHostDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CTLCOLORDLG,       OnColorDlg) // For dialog background.
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC,    OnColorDlg) // For static and read only edit.
		MESSAGE_HANDLER(WM_CTLCOLOREDIT,      OnColorDlg) // For edit boxes
		//MESSAGE_HANDLER(WM_CTLCOLORBTN,       OnColorDlg) // Owner-drawn only will respond.
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX,   OnColorDlg) // List and combo.
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
		COMMAND_ID_HANDLER(IDC_TEST_CONNECT, OnTestConnect)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_HANDLER(IDC_RADIO_SSPI, BN_CLICKED, OnRadioChange)
		COMMAND_HANDLER(IDC_RADIO_USERPWD, BN_CLICKED, OnRadioChange)
		COMMAND_HANDLER(IDC_HOST, CBN_EDITCHANGE, OnHostNameChange)
		COMMAND_HANDLER(IDC_HOST, CBN_SELCHANGE, OnSelChange)

		CHAIN_MSG_MAP(CSkinWindow<CConnectHostDlg>)
		//REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	//BEGIN_DDX_MAP(CConnectHostDlg)
	//	DDX_CHECK(IDC_CHK_PARSE, chkParse)
	//END_DDX_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return ::IsDialogMessage(m_hWnd, pMsg);
	}

	CConnectHostDlg(CPGNPOptions& optionsForEdit, CColorPalette& palit);

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	
	LRESULT OnTestConnect(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnRadioChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled);

	LRESULT OnHostNameChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled);

	LRESULT OnSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled);

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
	void SetCredentialsUI(const wstring& user, const wstring& pwd);

	void EnableControls(BOOL bEnable);

	bool m_bTaskInProgress;			// true when an async task is in progress
};

class CConnectParams
{
public:
	ATL::CString _host;
	bool _userpwd;
	ATL::CString _user;
	ATL::CString _pwd;

	CConnectParams(ATL::CString host, bool userpwd, ATL::CString user, ATL::CString pwd)
		: _host(host), _userpwd(userpwd), _user(user), _pwd(pwd)
	{}
};
