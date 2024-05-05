/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "MyDialogs.h"
#include "OptionsDlg.h"
#include "SkinScrollWnd.h"
#include <memory>
#include <ppltasks.h>
using namespace concurrency;

#import "ado\msado20.tlb" no_namespace rename("EOF", "IsEOF") rename("BOF", "IsBOF")

#define OPTIONS_V_MINOR		1
#define OPTIONS_V_MAJOR		2

extern char g_svalue[64 * 1024];

extern int FindSQLLOGColumn(const WCHAR* colName);

static LPCTSTR lpcstrFilter = L"PGNP Profiler Settings (*." JSON_FILE_EXT L")|*." JSON_FILE_EXT L"|All Files (*.*)|*.*|";

COptionsDlg::COptionsDlg(CPGNPOptions& optionsForEdit, CColorPalette& palit, int selectItem /*= -1*/)
	: m_props(0), m_selectItem(selectItem), m_optionsForEdit(optionsForEdit), m_bTaskInProgress(false)
	, CSkinWindow<COptionsDlg>(palit)
	, m_bEnableOptimizer(false)
	, m_startTheme(CColorPalette::THEME_DEFAULT)	// cannot assign optionsForEdit.m_dwColorTheme here because options might not have been loaded yet
	, m_palit(palit)
{}

LRESULT COptionsDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HWND hParent = GetParent();
	CenterWindow(hParent);

	CRect rcClient;
	GetClientRect(&rcClient);

	rcClient.DeflateRect(5,5,5,40);

	m_startTheme = m_optionsForEdit.m_dwColorTheme;

	// Determine if Query Optimizer is enabled
	m_bEnableOptimizer = IsOptimizerEnabled();
	m_props = new CPGNPropsCtrl(m_optionsForEdit, m_palit, m_bEnableOptimizer);
	m_props->Create(m_hWnd, rcClient, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0/*WS_EX_CLIENTEDGE, IDC_PROFILER_TV*/);

	if (m_selectItem != -1)
	{
		m_props->SelectIterItem(m_props->FindItem(m_selectItem));
	}

	m_props->SetFocus();

	// register object for message filtering
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	pLoop->AddMessageFilter(this);

	::EnableWindow(hParent, FALSE);

	return FALSE;	// return FALSE to prevent the system from setting the default keyboard focus
}

LRESULT COptionsDlg::OnSetCursor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if ((HWND)wParam == m_hWnd && LOWORD(lParam) == HTCLIENT && m_bTaskInProgress)
	{
		::SetCursor(::LoadCursor(NULL, IDC_WAIT));
		return 1;
	}

	bHandled = FALSE;
	return 0;
}

LRESULT COptionsDlg::OnCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	HDC hdc = (HDC)wParam;

	::SetTextColor(hdc, m_palit.CrText);
	if (CLR_INVALID != m_palit.CrBackGnd)
		::SetBkColor(hdc, m_palit.CrBackGnd);
	::SelectObject(hdc, m_palit.m_brBackGnd);

	return (LRESULT)m_palit.m_brBackGnd.m_hBrush;
}

LRESULT COptionsDlg::OnDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	m_palit.DrawButton((LPDRAWITEMSTRUCT)lParam);

	return 0;
}

// wParam = { THEME_DEFAULT, THEME_LIGHT, THEME_DARK }
LRESULT COptionsDlg::OnPGNProfThemeChange(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	::SendMessage(GetParent().m_hWnd, MYWM_THEME_CHANGE, wParam, lParam);

	RedrawWindow(NULL, NULL, RDW_FRAME | RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_UPDATENOW);

	GetDlgItem(IDC_RESET_SETTINGS).EnableWindow(wParam != CColorPalette::THEME_DEFAULT);

	bHandled = TRUE;
	return 0;
}

LRESULT COptionsDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HWND hParent = GetParent();

	if (IDOK == wID)
	{
		m_optionsForEdit.m_dwProfileAllApps = m_props->m_bProfileAllApps;
		m_optionsForEdit.m_dwDeleteWorkFiles = m_props->m_bRemoveTempFiles;
		m_optionsForEdit.m_sWorkPath = m_props->m_psWorkingPath->c_str();
		//m_optionsForEdit.m_dwRefreshApps = m_props->m_iRefreshApps;
		m_optionsForEdit.m_dwColorTheme = m_props->m_iColorTheme;
		m_optionsForEdit.m_dwParamFormat = m_props->m_iParamFormat;
		//m_optionsForEdit.m_dwTraceLevel = m_props->m_nTraceLevel;

		m_props->GetMessagesColors();
		if (m_props->m_iColorTheme == CColorPalette::THEME_DARK)
			m_optionsForEdit.m_qclrDark = m_palit.m_qclr;
		if (m_props->m_iColorTheme == CColorPalette::THEME_LIGHT)
			m_optionsForEdit.m_qclrLight = m_palit.m_qclr;

		// populate m_visibleColumnsList
		list<wstring> tmpList;
		WPGSiblingIterator& iter = m_props->GetCategoryItems(L"Columns");
		while (&iter != nullptr)
		{
			bool bVisible = *(bool*)GetDataPtr(iter);
			if (bVisible)
			{
				wstring colName(GetName(iter));
				wstring tmp(m_optionsForEdit.GetVisibleColumn(colName));
				if (tmp.length() == 0)
				{
					int iCol = FindSQLLOGColumn(colName.c_str());
					WCHAR buf[32];
					tmp = SQLLOG_COLUMNS[iCol].pszText;
					tmp += L':';
					tmp += _itow(SQLLOG_COLUMNS[iCol].cx, buf, 10);
					tmp += L';';
				}
				tmpList.push_back(tmp);
			}

			m_props->Next(iter);
		}

		if (!tmpList.empty())
		{
			m_optionsForEdit.m_visibleColumnsList.clear();
			std::copy(tmpList.begin(), tmpList.end(), std::back_inserter(m_optionsForEdit.m_visibleColumnsList));
		}

		if (m_bEnableOptimizer != m_props->m_bEnableOptimizer || m_optionsForEdit.m_sOptimizerConnectionString != *m_props->m_psConnectionString)
		{
			m_optionsForEdit.m_sOptimizerConnectionString = m_props->m_psConnectionString->c_str();
			m_bEnableOptimizer = m_props->m_bEnableOptimizer;

			// ensure that pgnp_optimizer table is created in the target DB, and store the Connection String and the "Optimizer Enabled" flag
			task<void> t = create_task([this, hParent]()
			{
				m_bTaskInProgress = true;

				HRESULT hRes = ::CoInitialize(NULL);

				_ConnectionPtr spCon;
				hRes = spCon.CreateInstance(__uuidof(Connection));

				_CommandPtr spCommand;
				spCommand.CreateInstance(__uuidof(Command));

				try
				{
					spCon->Open(m_optionsForEdit.m_sOptimizerConnectionString.c_str(), L"", L"", adConnectUnspecified);
					spCon->CursorLocation = adUseClient;

					spCommand->ActiveConnection = spCon;
					spCommand->CommandType = adCmdText;

					_variant_t vRecordsAffected;

					// [1] Check if pgnp_optimizer table exists
					spCommand->CommandText = L"SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME='pgnp_optimizer' AND TABLE_TYPE='BASE TABLE'";

					_RecordsetPtr spRS = spCommand->Execute(&vRecordsAffected, NULL, adCmdText);
					bool bCreateTable = spRS->IsEOF != FALSE;
					spRS->Close();
					spRS.Release();

					if (bCreateTable)
					{
						// [2] Create the pgnp_optimizer table
						spCommand->CommandText = L"CREATE TABLE PGNP_OPTIMIZER(OPTIMIZER_ID serial PRIMARY KEY, HASHTYPE int2 NOT NULL, ENABLED char, ORIGINAL text, HASH int4 NOT NULL, FINAL text, CATEGORY varchar(32), MODIFIED timestamp)";
						spCommand->Execute(&vRecordsAffected, NULL, adCmdText);
					}

					// [3] Insert the 'enable' entry
					WCHAR command[] = { L"UPDATE PGNP_OPTIMIZER SET ENABLED='?' WHERE HASH=0;"
						L"INSERT INTO PGNP_OPTIMIZER(HASHTYPE, ENABLED, HASH, CATEGORY, MODIFIED) "
						L"SELECT 0, '?', 0, 'System', now() WHERE NOT EXISTS (SELECT 1 FROM PGNP_OPTIMIZER WHERE HASH=0)" };

					for (WCHAR* p = &command[0]; *p; p++)
					{
						if (*p == '?')
							*p = m_bEnableOptimizer ? 'Y' : 'N';
					}
					spCommand->CommandText = command;
					spCommand->Execute(&vRecordsAffected, NULL, adCmdText);
				}
				catch (_com_error& e)
				{
					//MessageBox((TCHAR*)e.Description(), (TCHAR*)e.ErrorMessage(), MB_ICONERROR | MB_OK);
					//PostMessage(WM_CLOSE);
					ATLTRACE2(atlTraceDBProvider, 0, L"** COptionsDlg::OnCloseCmd(tc-task): %s %s\n", (TCHAR*)e.Description(), (TCHAR*)e.ErrorMessage());
					goto EXIT;
				}
				catch (...)
				{
					//MessageBox(L"Failed to query Optimizer table. Check if the connection string valid.", L"Error");
					//PostMessage(WM_CLOSE);
					ATLTRACE2(atlTraceDBProvider, 0, L"** COptionsDlg::OnCloseCmd(tc-task): Failed to query Optimizer table. Check if the connection string valid.\n");
					goto EXIT;
				}
EXIT:
				::CoUninitialize();

				m_bTaskInProgress = false;
			});
		}

		::PostMessage(hParent, MYMSG_SAVESETTINGS, OT_GENERAL, 0);
	}
	else if (m_startTheme != m_props->m_iColorTheme)
	{
		// user changed theme and then clicked Cancel
		::PostMessage(GetParent().m_hWnd, MYWM_THEME_CHANGE, m_startTheme, 0);
	}

	::EnableWindow(hParent, TRUE);
	m_props->DestroyWindow();
	DestroyWindow();
	delete m_props;

	return 0;
}

LRESULT COptionsDlg::OnResetSettings(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (XMessageBox(m_hWnd, L"All application settings will be reset into default. Confirm?", PGNPTITLE, MB_YESNO | MB_ICONQUESTION) != IDYES)
		return 0;

	m_optionsForEdit.Reset(m_props->m_iColorTheme);

	m_props->m_bProfileAllApps = m_optionsForEdit.m_dwProfileAllApps != 0;
	m_props->m_bRemoveTempFiles = m_optionsForEdit.m_dwDeleteWorkFiles != 0;
	m_props->m_psWorkingPath->clear();
	m_props->m_psConnectionString->clear();
	//m_props->m_iRefreshApps = m_optionsForEdit.m_dwRefreshApps;
	m_props->m_iColorTheme = m_optionsForEdit.m_dwColorTheme;
	m_props->m_iParamFormat = m_optionsForEdit.m_dwParamFormat;
	for (int i = 0; i < SQLLOG_COLUMNS_CNT; i++)
		m_props->m_rgSelectColumn[i] = false;
		
	m_props->m_bEnableOptimizer = false;

	if (m_props->m_iColorTheme == CColorPalette::THEME_DARK)
		m_palit.m_qclr = m_optionsForEdit.m_qclrDark;
	if (m_props->m_iColorTheme == CColorPalette::THEME_LIGHT)
		m_palit.m_qclr = m_optionsForEdit.m_qclrLight;

	m_props->SetMessagesColors();

	return 0;
}

Json::Value qclrToJsonValue(vector<COLORREF>& qclr)
{
	Json::Value qclrValue;
	char buf[2048] = { 0 };

	itoa(qclr[QCLR_FOCUS], buf, 16);
	qclrValue["qclr_focus"] = buf;
	itoa(qclr[QCLR_SELECT], buf, 16);
	qclrValue["qclr_select"] = buf;
	itoa(qclr[QCLR_INSERT], buf, 16);
	qclrValue["qclr_insert"] = buf;
	itoa(qclr[QCLR_UPDATE], buf, 16);
	qclrValue["qclr_update"] = buf;
	itoa(qclr[QCLR_DELETE], buf, 16);
	qclrValue["qclr_delete"] = buf;
	itoa(qclr[QCLR_CREATEDROPALTER], buf, 16);
	qclrValue["qclr_alter"] = buf;
	itoa(qclr[QCLR_PROCEDURES], buf, 16);
	qclrValue["qclr_procedures"] = buf;
	itoa(qclr[QCLR_SYSSCHEMA], buf, 16);
	qclrValue["qclr_sysschema"] = buf;
	itoa(qclr[QCLR_SYSTEM], buf, 16);
	qclrValue["qclr_system"] = buf;
	itoa(qclr[QCLR_ERROR], buf, 16);
	qclrValue["qclr_error"] = buf;
	itoa(qclr[QCLR_NOTIFY], buf, 16);
	qclrValue["qclr_notify"] = buf;
	itoa(qclr[QCLR_NONE], buf, 16);
	qclrValue["qclr_none"] = buf;
	itoa(qclr[QCLR_COMMENT], buf, 16);
	qclrValue["qclr_comment"] = buf;

	return qclrValue;
}

LRESULT COptionsDlg::OnExportSettings(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// Ask user for the file destination.
	CMyFileDialog dlg(FALSE, JSON_FILE_EXT, L"pgnprofiler.json", OFN_EXPLORER, lpcstrFilter);
	if (dlg.DoModal() == IDOK)
	{
		// check if file exists
		if (GetFileAttributes(dlg.m_ofn.lpstrFile) != INVALID_FILE_ATTRIBUTES)
		{
			WCHAR message[1024];
			swprintf(message, L"File \"%s\" already exists. Do you want to overwrite it?", dlg.m_ofn.lpstrFile);
			if (XMessageBox(m_hWnd, message, PGNPTITLE, MB_YESNO | MB_ICONQUESTION) != IDYES)
				return 0;
		}

		// open file for writing
		HANDLE hFile = CreateFile(dlg.m_ofn.lpstrFile, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		if (INVALID_HANDLE_VALUE == hFile)
		{
			WCHAR message[1024];
			swprintf(message, L"Could not create file \"%s\". Error %d.", dlg.m_ofn.lpstrFile, GetLastError());
			XMessageBox(m_hWnd, message, PGNPTITLE, MB_OK | MB_ICONERROR);
			return 0;
		}

		USES_CONVERSION;
		Json::Value root, visibleColumns, remoteConnections;
		char buf[2048] = {0};

		itoa(OPTIONS_V_MAJOR, buf, 10);
		root["optionsVer"] = buf;

		root["profileAllApps"] = m_optionsForEdit.m_dwProfileAllApps ? "1" : "0";

		root["deleteWorkFiles"] = m_optionsForEdit.m_dwDeleteWorkFiles ? "1" : "0";

		wcstombs(buf, m_optionsForEdit.m_sWorkPath.c_str(), m_optionsForEdit.m_sWorkPath.size());
		root["workPath"] = buf;

		itoa(m_optionsForEdit.m_dwColorTheme, buf, 10);
		root["colorTheme"] = buf;

		root["paramFormat"] = m_optionsForEdit.m_dwParamFormat ? "1" : "0";

		memset(buf, 0, sizeof(buf));
		wcstombs(buf, m_optionsForEdit.m_sOptimizerConnectionString.c_str(), m_optionsForEdit.m_sOptimizerConnectionString.size());
		root["optimizerConnStr"] = buf;

		// events highlightinh (light)
		root["qclrLight"] = qclrToJsonValue(m_optionsForEdit.m_qclrLight);

		// events highlightinh (dark)
		root["qclrDark"] = qclrToJsonValue(m_optionsForEdit.m_qclrDark);

		itoa(m_optionsForEdit.m_panel1FontHt, buf, 10);
		root["viewFontHt"] = buf;

		itoa(m_optionsForEdit.m_DetailsFontHt, buf, 10);
		root["detailsFontHt"] = buf;

		itoa(m_optionsForEdit.m_ExplorerFontHt, buf, 10);
		root["explorerFontHt"] = buf;

		itoa(m_optionsForEdit.m_FilterFontHt, buf, 10);
		root["filterFontHt"] = buf;

		Json::StyledWriter writer;
		std::string jsonText = writer.write(root);

		DWORD numBytes;
		WriteFile(hFile, jsonText.c_str(), jsonText.size(), &numBytes, 0);

		CloseHandle(hFile);
	}

	return 0;
}

bool qclrFromString(vector<COLORREF>& qclr, unsigned hash, const string& s)
{
	char* endPtr;

	switch (hash)
	{
	case 0x12E1091A: /* qclr_focus */
		qclr[QCLR_FOCUS] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_FOCUS] = RGB(41, 131, 225);
		break;
	case 0x9CFC775E: /* qclr_select */
		qclr[QCLR_SELECT] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_SELECT] = White;
		break;
	case 0x172A7F01: /* qclr_insert */
		qclr[QCLR_INSERT] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_INSERT] = RGB(0x00, 0xFF, 0xFF);			// cyan
		break;
	case 0xA606ED67: /* qclr_update */
		qclr[QCLR_UPDATE] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_UPDATE] = RGB(0xFF, 0xFF, 0x00);			// yellow
		break;
	case 0x57FC858F: /* qclr_delete */
		qclr[QCLR_DELETE] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_DELETE] = RGB(0xD3, 0xD3, 0xD3);			// ltgray
		break;
	case 0x803745F8: /* qclr_alter */
		qclr[QCLR_CREATEDROPALTER] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_CREATEDROPALTER] = RGB(0xBC, 0xEA, 0x64);	// light green
		break;
	case 0x798F91C8: /* qclr_procedures */
		qclr[QCLR_PROCEDURES] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_PROCEDURES] = RGB(0xFF, 0xA5, 0x00);		// orange
		break;
	case 0xF119F3FE: /* qclr_sysschema */
		qclr[QCLR_SYSSCHEMA] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_SYSSCHEMA] = RGB(0xE4, 0xD5, 0xFF);
		break;
	case 0x119B7073: /* qclr_system */
		qclr[QCLR_SYSTEM] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_SYSTEM] = RGB(0xD5, 0xE4, 0xFF);
		break;
	case 0xD63E9DAE: /* qclr_error */
		qclr[QCLR_ERROR] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_ERROR] = RGB(0xFF, 0x45, 0x00);			// orange-red
		break;
	case 0xCF88E715: /* qclr_notify */
		qclr[QCLR_NOTIFY] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_NOTIFY] = RGB(0x84, 0x2E, 0xE2);			// violet
		break;
	case 0x47B8725C: /* qclr_none */
		qclr[QCLR_NONE] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_NONE] = RGB(0xFF, 0x80, 0xFF);
		break;
	case 0x1E803BA5: /* qclr_comment */
		qclr[QCLR_COMMENT] = strtoul(s.c_str(), &endPtr, 16);
		if (*endPtr)
			qclr[QCLR_COMMENT] = RGB(0x4C, 0xB1, 0x22);
		break;
	}

	return false;
}

LRESULT COptionsDlg::OnImportSettings(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	//// uncomment following lines to generate hashes in the log
	//str2hash("profileAllApps", 0);
	//str2hash("deleteWorkFiles", 0);
	//str2hash("workPath", 0);
	//str2hash("colorTheme", 0);
	//str2hash("paramFormat", 0);
	//str2hash("optimizerConnStr", 0);
	//str2hash("qclrLight", 0);
	//str2hash("qclrDark", 0);
	//str2hash("qclr_focus", 0);
	//str2hash("qclr_select", 0);
	//str2hash("qclr_insert", 0);
	//str2hash("qclr_update", 0);
	//str2hash("qclr_delete", 0);
	//str2hash("qclr_alter", 0);
	//str2hash("qclr_procedures", 0);
	//str2hash("qclr_sysschema", 0);
	//str2hash("qclr_system", 0);
	//str2hash("qclr_error", 0);
	//str2hash("qclr_notify", 0);
	//str2hash("qclr_none", 0);
	//str2hash("qclr_comment", 0);
	//str2hash("visibleColumns", 0);
	//str2hash("host", 0);
	//str2hash("user", 0);
	//str2hash("pwd", 0);
	//str2hash("remoteConnections", 0);
	//str2hash("viewFontHt", 0);
	//str2hash("detailsFontHt", 0);
	//str2hash("explorerFontHt", 0);
	//str2hash("filterFontHt", 0);

	// Open file
	CMyFileDialog dlg(TRUE, JSON_FILE_EXT, _T(""), OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_EXPLORER, lpcstrFilter);
	if (dlg.DoModal() == IDOK)
	{
		HANDLE hFile = ::CreateFile(dlg.m_ofn.lpstrFile, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (INVALID_HANDLE_VALUE == hFile)
		{
			DWORD err = GetLastError();
			WCHAR message[128];
			swprintf(message, L"Unable to read file, error %d.", err);
			XMessageBox(m_hWnd, message, L"Error", MB_OK | MB_ICONERROR);
			return 1;
		}

		Json::Value root;
		Json::Reader reader;

		DWORD cbRead;
		char *endPtr;
		char buffer[16384] = {0};
		BOOL fSuccess = ReadFile(hFile, buffer, sizeof(buffer), &cbRead, NULL);

		if (reader.parse(buffer, root))
		{
			Json::Value versionValue = root.get("optionsVer", Json::Value::null);
			int version = (versionValue == Json::Value::null ? 1 : atoi(versionValue.asString().c_str()));
			if (version < OPTIONS_V_MINOR || version > OPTIONS_V_MAJOR)
			{
				WCHAR message[128];
				swprintf(message, L"Unsupported version, error %d.", version);
				XMessageBox(m_hWnd, message, L"Error", MB_OK | MB_ICONERROR);
				return 2;
			}

			Json::Value::Members members = root.getMemberNames();
			for (auto it = members.begin(); it != members.end(); it++)
			{
				auto jsonval = root[*it];
				string s(jsonval.asString());
				unsigned hash = str2hash(it->c_str(), 0);
				switch (hash)
				{
				case 0xCFA7ADDD: /* profileAllApps */
					m_optionsForEdit.m_dwProfileAllApps = (!s.empty() && s[0] != '0') ? 1 : 0;
					break;
				case 0x4C85C4E8: /* deleteWorkFiles */
					m_optionsForEdit.m_dwDeleteWorkFiles = (!s.empty() && s[0] != '0') ? 1 : 0;
					break;
				case 0x66BA7809: /* workPath */
					{
						CA2WEX<4000> workPath(s.c_str(), CP_UTF8);
						m_optionsForEdit.m_sWorkPath.assign(workPath);
					}
					break;
				case 0xAEBB22E9: /* colorTheme */
					m_optionsForEdit.m_dwColorTheme = atoi(s.c_str());
					if (m_optionsForEdit.m_dwColorTheme > 2)
						m_optionsForEdit.m_dwColorTheme = 2;
					break;
				case 0x197285C9: /* paramFormat */
					m_optionsForEdit.m_dwParamFormat = (!s.empty() && s[0] != '0') ? 1 : 0;
					break;
				case 0x0EC95EB7: /* optimizerConnStr */
					{
						CA2WEX<4000> connStr(s.c_str(), CP_UTF8);
						m_optionsForEdit.m_sOptimizerConnectionString.assign(connStr);
					}
					break;
				case 0xAC28DAB7: /* qclrLight */
					for each (auto qclrMember in jsonval.getMemberNames())
					{
						auto jv = jsonval[qclrMember.c_str()];
						qclrFromString(m_optionsForEdit.m_qclrLight, str2hash(qclrMember.c_str(), 0), jv.asString());
					}
					break;
				case 0x8C949115: /* qclrDark */
					for each (auto qclrMember in jsonval.getMemberNames())
					{
						auto jv = jsonval[qclrMember.c_str()];
						qclrFromString(m_optionsForEdit.m_qclrDark, str2hash(qclrMember.c_str(), 0), jv.asString());
					}
					break;
				case 0xF5231BCE: /* visibleColumns */
					// TODO
					break;
				case 0x7C735525: /* host */
					break;
				case 0x7C78F7F4: /* user */
					break;
				case 0x0B872026: /* pwd */
					break;
				case 0xA4DB2864: /* remoteConnections */
					break;
				case 0x200117A7: /* viewFontHt */
					m_optionsForEdit.m_panel1FontHt = atoi(s.c_str());
					if (m_optionsForEdit.m_panel1FontHt < -72 || m_optionsForEdit.m_panel1FontHt > -2)
						m_optionsForEdit.m_panel1FontHt = -12;
					break;
				case 0x848A3F08: /* detailsFontHt */
					m_optionsForEdit.m_DetailsFontHt = atoi(s.c_str());
					if (m_optionsForEdit.m_DetailsFontHt < -72 || m_optionsForEdit.m_DetailsFontHt > -2)
						m_optionsForEdit.m_DetailsFontHt = -12;
					break;
				case 0xA18C91A1: /* explorerFontHt */
					m_optionsForEdit.m_ExplorerFontHt = atoi(s.c_str());
					if (m_optionsForEdit.m_ExplorerFontHt < -72 || m_optionsForEdit.m_ExplorerFontHt > -2)
						m_optionsForEdit.m_ExplorerFontHt = -12;
					break;
				case 0x1E783E0A: /* filterFontHt */
					m_optionsForEdit.m_FilterFontHt = atoi(s.c_str());
					if (m_optionsForEdit.m_FilterFontHt < -72 || m_optionsForEdit.m_FilterFontHt > -2)
						m_optionsForEdit.m_FilterFontHt = -12;
					break;
				default:
					if (OPTIONS_V_MINOR == version)
						qclrFromString(m_optionsForEdit.m_qclrLight, hash, s);
				}
			}
		}
		else
		{
			XMessageBox(m_hWnd, L"Failed to parse file.", L"Error", MB_OK | MB_ICONERROR);
		}

		CloseHandle(hFile);
	}

	return 0;
}

bool COptionsDlg::IsOptimizerEnabled()
{
	if (m_optionsForEdit.m_sOptimizerConnectionString.empty())
	{
		// Cannot determine if Optimizer is enabled because Connection String is not valid
		return false;
	}

	DWORD optimizerEnableID = 0;

	HRESULT hRes = ::CoInitialize(NULL);

	try
	{
		_ConnectionPtr spCon;
		hRes = spCon.CreateInstance(__uuidof(Connection));
		spCon->Open(m_optionsForEdit.m_sOptimizerConnectionString.c_str(), L"", L"", adConnectUnspecified);
		spCon->CursorLocation = adUseClient;

		_CommandPtr spCommand;
		spCommand.CreateInstance(__uuidof(Command));
		spCommand->ActiveConnection = spCon;
		spCommand->CommandText = L"SELECT optimizer_id FROM pgnp_optimizer WHERE hash=0 AND enabled='Y'";
		spCommand->CommandType = adCmdText;

		_variant_t vRecordsAffected;
		_RecordsetPtr spRS = spCommand->Execute(&vRecordsAffected, NULL, adCmdText);
		if (spRS->IsEOF == FALSE)
		{
			optimizerEnableID = (DWORD)spRS->Fields->GetItem("optimizer_id")->Value;
		}
	}
	catch (_com_error& e)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"** COptionsDlg::IsOptimizerEnabled(): %s %s\n", (TCHAR*)e.Description(), (TCHAR*)e.ErrorMessage());
	}
	catch (...)
	{
		//MessageBox(L"Failed to query Optimizer table. Check DB connection in the Options dialog.", L"Error");
		//PostMessage(WM_CLOSE);
	}

	::CoUninitialize();

	return optimizerEnableID != 0;
}
