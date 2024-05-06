/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "ConnectHostDlg.h"
#include "LoggerItem.h"
#include <comutil.h>		// for _bstr_t
#include <time.h>
#include <uxtheme.h>

CConnectHostDlg::CConnectHostDlg(CPGNPOptions& optionsForEdit, CColorPalette& palit)
	: m_optionsForEdit(optionsForEdit), m_bTaskInProgress(false), m_palit(palit)
	, CSkinWindow<CConnectHostDlg>(palit)
{
}

LRESULT CConnectHostDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HWND hParent = GetParent();
	CenterWindow(hParent);

	CComboBox cmbHost = GetDlgItem(IDC_HOST);
	cmbHost.LimitText(64);

	SetWindowTheme(GetDlgItem(IDC_RADIO_SSPI), L" ", L" ");	// turn off theming, otherwise SetTextColor will not have effect
	SetWindowTheme(GetDlgItem(IDC_RADIO_USERPWD), L" ", L" ");	// turn off theming, otherwise SetTextColor will not have effect
	SetWindowTheme(GetDlgItem(IDC_LOGONCREDS_STATIC), L" ", L" ");	// turn off theming, otherwise SetTextColor will not have effect

	for (auto it = m_optionsForEdit.m_remoteConnections.cbegin(); it != m_optionsForEdit.m_remoteConnections.cend(); it++)
	{
		int nIndex = cmbHost.AddString(it->_host.c_str());
		cmbHost.SetItemData(nIndex, (DWORD_PTR)&(*it));
	}

	if (m_optionsForEdit.m_remoteConnections.size() > 0)
	{
		cmbHost.SetCurSel(0);
		PostMessage(WM_COMMAND, MAKEWPARAM(IDC_HOST, CBN_SELCHANGE), (LPARAM)cmbHost.m_hWnd);
	}
	else
	{
		CButton btnSSPI = GetDlgItem(IDC_RADIO_SSPI);
		btnSSPI.SetCheck(1);
	}

	// register object for message filtering
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	pLoop->AddMessageFilter(this);

	::EnableWindow(hParent, FALSE);

	return FALSE;	// return FALSE to prevent the system from setting the default keyboard focus
}

LRESULT CConnectHostDlg::OnSetCursor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if ((HWND)wParam == m_hWnd && LOWORD(lParam) == HTCLIENT && m_bTaskInProgress)
	{
		::SetCursor(::LoadCursor(NULL, IDC_WAIT));
		return 1;
	}

	bHandled = FALSE;
	return 0;
}

LRESULT CConnectHostDlg::OnHostNameChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	CComboBox cmbHost = GetDlgItem(IDC_HOST);
	ATL::CString hostName;
	cmbHost.GetWindowText(hostName);
	hostName = hostName.Trim();

	// if host name is empty or is a dot, disable OK and TestConnect buttons
	BOOL bEnable = (0 == hostName.GetLength() || hostName == L".") ? 0 : 1;

	GetDlgItem(IDOK).EnableWindow(bEnable);
	GetDlgItem(IDC_TEST_CONNECT).EnableWindow(bEnable);

	// if host name coincides with a name from history, retrieve user name and password
	// otherwise, clear user name and password
	wstring storedUser, storedPwd;
	for (auto it = m_optionsForEdit.m_remoteConnections.cbegin(); it != m_optionsForEdit.m_remoteConnections.cend(); it++)
	{
		if (it->_host == LPCTSTR(hostName))
		{
			storedUser = it->_user;
			storedPwd = it->_pwd;
			break;
		}
	}

	SetCredentialsUI(storedUser, storedPwd);

	return 0;
}

LRESULT CConnectHostDlg::OnSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	CComboBox cmbHost = GetDlgItem(IDC_HOST);
	int nIndex = cmbHost.GetCurSel();
	CRemoteConnectionInfo* pInfo = (CRemoteConnectionInfo*)cmbHost.GetItemData(nIndex);
	SetCredentialsUI(pInfo->_user, pInfo->_pwd);

	GetDlgItem(IDOK).EnableWindow(TRUE);
	GetDlgItem(IDC_TEST_CONNECT).EnableWindow(TRUE);

	return 0;
}

// This method tries to launch this application as Windows service on remote computer, then gets list of pipes for profiling, then
// displays first 5 processes to profile, or failure code.
LRESULT CConnectHostDlg::OnTestConnect(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	::SetCursor(::LoadCursor(NULL, IDC_WAIT));

	ATL::CString hostName;
	CComboBox edtHost = GetDlgItem(IDC_HOST);
	edtHost.GetWindowText(hostName);
	hostName = hostName.Trim();

	ATL::CString sUser, sPwd;
	CButton btnNoproxy = GetDlgItem(IDC_RADIO_SSPI);
	if (btnNoproxy.GetCheck() == 0)
	{
		GetDlgItem(IDC_USER).GetWindowText(sUser);
		GetDlgItem(IDC_PWD).GetWindowText(sPwd);
	}

	USES_XMSGBOXPARAMS
	ATL::CString processes;
	wstring workPath;	// can be empty since not used by CHostLoggerItem
	CHostLoggerItem remoteHost(&workPath);
	remoteHost.SetName(hostName);
	remoteHost.SetCredentials(sUser, sPwd);
	int exitCode = remoteHost.ListProcessesOnRemoteHost(processes);

	::SetCursor(::LoadCursor(NULL, IDC_ARROW));

	if (exitCode != 0)
	{
		ATL::CString message;
		message.Format(L"Attempt to read from remote host failed, code %d.", exitCode);
		XMessageBox(m_hWnd, message, L"Error", MB_OK | MB_ICONERROR, &__xmb);
	}
	else
	{
		// application names are semi-colon separated, so lets count the semi-colons
		int appsCount = 0;
		for (LPCTSTR p = processes; *p; p++)
		{
			if (L';' == *p)
				appsCount++;
		}

		ATL::CString message;
		message.Format(L"Connected to remote host!\r\n\r\nApplications for profiling count: %d.", appsCount);
		XMessageBox(m_hWnd, message, L"Success!", MB_OK | MB_ICONASTERISK, &__xmb);
	}

	return 0;
}

LRESULT CConnectHostDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HWND hParent = GetParent();

	if (IDOK == wID)
	{
		int sspi = ((CButton)GetDlgItem(IDC_RADIO_SSPI)).GetCheck();
		ATL::CString host, user, pwd;
		
		GetDlgItem(IDC_HOST).GetWindowText(host);
		GetDlgItem(IDC_USER).GetWindowText(user);
		GetDlgItem(IDC_PWD).GetWindowText(pwd);

		// check if entry with same host name already exists in saved connections info
		bool bSaveSettings = false;
		auto it = m_optionsForEdit.m_remoteConnections.begin();
		auto re = m_optionsForEdit.m_remoteConnections.end();
		for (; it != re && (*it)._host != LPCTSTR(host); it++);
		if (it != re)
		{
			// move existing entry to front
			if (it != m_optionsForEdit.m_remoteConnections.begin())
			{
				m_optionsForEdit.m_remoteConnections.splice(m_optionsForEdit.m_remoteConnections.begin(), m_optionsForEdit.m_remoteConnections, it, std::next(it));
				bSaveSettings = true;
			}
		}
		else
		{
			// insert new entry to front
			CRemoteConnectionInfo rci = { host, user, pwd };
			m_optionsForEdit.m_remoteConnections.push_front(rci);
			bSaveSettings = true;
		}

		if (bSaveSettings)
		{
			// save information entered by user
			::PostMessage(hParent, MYMSG_SAVESETTINGS, OT_GENERAL, 0);
		}

		// send "host connected" command to be processed in main window
		CConnectParams* params = new CConnectParams(host, sspi == 0, user, pwd);
		::PostMessage(hParent, MYMSG_HOST_CONNECTED, 0, (LPARAM)params);
	}

	::EnableWindow(hParent, TRUE);
	DestroyWindow();

	return 0;
}

LRESULT CConnectHostDlg::OnRadioChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	CButton btnSSPI = GetDlgItem(IDC_RADIO_SSPI);

	EnableControls(btnSSPI.GetCheck() == 0);

	return 0;
}

void CConnectHostDlg::SetCredentialsUI(const wstring& user, const wstring& pwd)
{
	CButton btnUserPwd = GetDlgItem(IDC_RADIO_USERPWD);
	btnUserPwd.SetCheck(!user.empty());

	CButton btnSSPI = GetDlgItem(IDC_RADIO_SSPI);
	btnSSPI.SetCheck(user.empty());

	EnableControls(!user.empty());

	GetDlgItem(IDC_USER).SetWindowText(user.c_str());
	GetDlgItem(IDC_PWD).SetWindowText(pwd.c_str());
}

void CConnectHostDlg::EnableControls(BOOL bEnable)
{
	GetDlgItem(IDC_USER_STATIC).EnableWindow(bEnable);
	GetDlgItem(IDC_PWD_STATIC).EnableWindow(bEnable);
	GetDlgItem(IDC_USER).EnableWindow(bEnable);
	GetDlgItem(IDC_PWD).EnableWindow(bEnable);
}


void ScrambleText(char* result, size_t res_size, const char* original, bool decode)
{
	*result = 0;
}
