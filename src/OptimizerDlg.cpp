/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "OptimizerDlg.h"
#include "PGNPOptions.h"
#include <atlcomtime.h>		// for COleDateTime
#include <memory>
#include <ppltasks.h>
using namespace concurrency;
#include <time.h>
#include <uxtheme.h>
#include "SkinScrollWnd.h"
#include "tokenizer.h"
#import "ado\msado20.tlb" no_namespace rename("EOF", "IsEOF") rename("BOF", "IsBOF")

LRESULT COptimizerDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HWND hParent = GetParent();
	CenterWindow(hParent);

	CButton btnEnable = GetDlgItem(IDC_ENABLE_OPTIMIZATION);
	btnEnable.SetCheck(1);

	CButton btnExact = GetDlgItem(IDC_RADIO_EXACT);
	btnExact.SetCheck(1);

	// turn off theming, otherwise SetTextColor will not have effect
	SetWindowTheme(btnEnable, L" ", L" ");
	SetWindowTheme(btnExact, L" ", L" ");
	SetWindowTheme(GetDlgItem(IDC_RADIO_TOKEN), L" ", L" ");
	SetWindowTheme(GetDlgItem(IDC_OPTIMETHOD_STATIC), L" ", L" ");

	m_exactHash = COptimizer::CalculateHash(m_clientSQL.c_str(), m_clientSQL.length());
	tokens_vec tokens;
	m_parameterizedHash = COptimizer::CalculateHash(m_clientSQL.c_str(), m_clientSQL.length(), &tokens);

	CRichEditCtrl edtOriginal(GetDlgItem(IDC_ORIGINAL_STMT));
	edtOriginal.SetBackgroundColor(m_palit.CrBackGnd);
	CHARFORMAT cf = { sizeof(cf), CFM_COLOR, 0, 0, 0, m_palit.CrTextAlt };
	edtOriginal.SetCharFormat(cf, CFM_COLOR);
	CA2WEX<4000> stmt(m_clientSQL.c_str(), CP_UTF8);
	edtOriginal.SetWindowText(stmt);

	WCHAR buf[100];
	swprintf(buf, L"(hash:%08X)", m_exactHash);
	CStatic lblHash = GetDlgItem(IDC_OPTIMIZER_HASH);
	lblHash.SetWindowText(buf);

	const wchar_t* copyStart = tokens._tokenizer.copy;

	CHARFORMAT2 cfHigh;
	ZeroMemory(&cfHigh, sizeof(cfHigh));
	cfHigh.cbSize = sizeof(cfHigh);
	
	edtOriginal.GetDefaultCharFormat(cfHigh);

	cfHigh.crBackColor = RGB(255, 255, 0);
	cfHigh.dwMask = CFM_BACKCOLOR;
	cfHigh.dwEffects &= ~CFE_AUTOBACKCOLOR;

	CHARFORMAT cfSuper = { 0 };
	cfSuper.cbSize = sizeof(cfSuper);
	cfSuper.crTextColor = m_palit.CrTextAlt;
	cfSuper.dwMask = CFM_BOLD | CFM_COLOR | CFM_OFFSET;
	cfSuper.dwEffects = CFE_BOLD;
	//cfSuper.dwEffects &= ~CFE_AUTOCOLOR;
	cfSuper.yOffset += 60;

	LONG prmIndex = 1, prmOffset = 0;
	buf[0] = L'$';

	for_each(tokens.begin(), tokens.end(), [&](const TOKEN& t)
	{
		LONG start = t.tokStart - copyStart + prmOffset;
		LONG end = t.curPos - copyStart + prmOffset;
		if (TOK_USTRING == t.token)
			start--;

		// print $n after literal value
		_itow(prmIndex++, &buf[1], 10);
		size_t prmLen = wcslen(buf);
		edtOriginal.InsertText(end, buf);

		edtOriginal.SetSel(end, end + prmLen);
		edtOriginal.SetSelectionCharFormat(cfSuper);

		// highlight the literal value
		edtOriginal.SetSel(start, end);
		edtOriginal.SetSelectionCharFormat(cfHigh);

		prmOffset += prmLen;
	});

	edtOriginal.SetSel(0, 0);	// hide selection

	CEdit edtOptimized = GetDlgItem(IDC_OPTIMIZED_STMT);
	SkinWndScroll(edtOptimized, m_palit);
	edtOptimized.SetWindowText(L"Loading...");

	// register object for message filtering
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	pLoop->AddMessageFilter(this);

	PostMessage(MYMSG_LOAD_OPTIMIZED);

	::EnableWindow(hParent, FALSE);
	return FALSE;	// return FALSE to prevent the system from setting the default keyboard focus
}

LRESULT COptimizerDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HWND hParent = GetParent();

	if (IDOK == wID && IsModified())
	{
		// save changes to DB
		CButton btnEnable = GetDlgItem(IDC_ENABLE_OPTIMIZATION);
		CButton btnExact = GetDlgItem(IDC_RADIO_EXACT);
		CRichEditCtrl edtOriginal(GetDlgItem(IDC_ORIGINAL_STMT));
		CEdit edtOptimized = GetDlgItem(IDC_OPTIMIZED_STMT);
		CEdit edtCategory = GetDlgItem(IDC_OPTIMIZATION_NOTE);
	
		HASH hash = (btnExact.GetCheck() == 0) ? m_parameterizedHash : m_exactHash;
	
		::SetCursor(::LoadCursor(NULL, IDC_WAIT));
	
		try
		{
			_ConnectionPtr spCon;
			spCon.CreateInstance(__uuidof(Connection));
			spCon->Open(m_pOptionsForEdit->m_sOptimizerConnectionString.c_str(), L"", L"", adConnectUnspecified);
			spCon->CursorLocation = adUseClient;

			_CommandPtr spCommand;
			spCommand.CreateInstance(__uuidof(Command));
			spCommand->ActiveConnection = spCon;
			spCommand->CommandType = adCmdText;

			_variant_t vRecordsAffected;

			if (m_optimizerID != 0)
			{
				// update existing optimizer hint
				spCommand->CommandText = L"UPDATE pgnp_optimizer SET hashtype=?,enabled=?,hash=?,final=?,category=?,modified=now() WHERE optimizer_id=?";

				_ParameterPtr spParam = spCommand->CreateParameter(_bstr_t(L"hashtype"), adInteger, adParamInput, sizeof(int), variant_t(btnExact.GetCheck() ? 0 : 1));
				spCommand->Parameters->Append(spParam);

				spParam = spCommand->CreateParameter(_bstr_t(L"enabled"), adBSTR, adParamInput, 1, _bstr_t(btnEnable.GetCheck() ? "Y" : "N"));
				spCommand->Parameters->Append(spParam);

				spParam = spCommand->CreateParameter(_bstr_t(L"hash"), adInteger, adParamInput, sizeof(int), variant_t(hash));
				spCommand->Parameters->Append(spParam);

				_bstr_t final;
				edtOptimized.GetWindowText(final.GetBSTR());
				spParam = spCommand->CreateParameter(_bstr_t(L"final"), adBSTR, adParamInput, final.length(), final);
				spCommand->Parameters->Append(spParam);

				_bstr_t category;
				edtCategory.GetWindowText(category.GetBSTR());
				spParam = spCommand->CreateParameter(_bstr_t(L"category"), adBSTR, adParamInput, category.length(), category);
				spCommand->Parameters->Append(spParam);

				spParam = spCommand->CreateParameter(_bstr_t(L"id"), adInteger, adParamInput, sizeof(int), variant_t(m_optimizerID));
				spCommand->Parameters->Append(spParam);

				spCommand->Execute(&vRecordsAffected, NULL, adCmdText);
			}
			else
			{
				// insert new optimizer hint
				spCommand->CommandText = L"INSERT INTO pgnp_optimizer(hashtype,enabled,original,hash,final,category,modified) VALUES(?,?,?,?,?,?,now())";

				_ParameterPtr spParam = spCommand->CreateParameter(_bstr_t(L"hashtype"), adInteger, adParamInput, sizeof(int), variant_t(btnExact.GetCheck() ? 0 : 1));
				spCommand->Parameters->Append(spParam);

				spParam = spCommand->CreateParameter(_bstr_t(L"enabled"), adBSTR, adParamInput, 1, _bstr_t(btnEnable.GetCheck() ? "Y" : "N"));
				spCommand->Parameters->Append(spParam);

				_bstr_t original;
				edtOriginal.GetWindowText(original.GetBSTR());
				spParam = spCommand->CreateParameter(_bstr_t(L"original"), adBSTR, adParamInput, original.length(), original);
				spCommand->Parameters->Append(spParam);

				spParam = spCommand->CreateParameter(_bstr_t(L"hash"), adInteger, adParamInput, sizeof(int), variant_t(hash));
				spCommand->Parameters->Append(spParam);

				_bstr_t final;
				edtOptimized.GetWindowText(final.GetBSTR());
				spParam = spCommand->CreateParameter(_bstr_t(L"final"), adBSTR, adParamInput, final.length(), final);
				spCommand->Parameters->Append(spParam);

				_bstr_t category;
				edtCategory.GetWindowText(category.GetBSTR());
				spParam = spCommand->CreateParameter(_bstr_t(L"category"), adBSTR, adParamInput, category.length(), category);
				spCommand->Parameters->Append(spParam);

				spCommand->Execute(&vRecordsAffected, NULL, adCmdText);
			}

			if (vRecordsAffected.lVal == 1)
			{
				spCommand->CommandText = L"NOTIFY pgnp_optimizer;";
				spCommand->Parameters->Refresh();
				spCommand->Execute(NULL, NULL, adCmdText);
			}
			else
			{
				XMessageBox(m_hWnd, L"Number of affected rows is not equal to 1.", L"Optimizer", MB_ICONEXCLAMATION);
			}
		}
		catch (_com_error& e)
		{
			XMessageBox(m_hWnd, (TCHAR*)e.Description(), (TCHAR*)e.ErrorMessage(), MB_ICONERROR | MB_OK);
		}
		catch (...)
		{
			XMessageBox(m_hWnd, L"Failed to save optimized query.", L"Error");
		}

		::SetCursor(::LoadCursor(NULL, IDC_ARROW));
	}

	::EnableWindow(hParent, TRUE);
	DestroyWindow();
	return 0;
}

LRESULT COptimizerDlg::OnSetCursor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if ((HWND)wParam == m_hWnd && LOWORD(lParam) == HTCLIENT && m_bTaskInProgress)
	{
		//DWORD dwPos = ::GetMessagePos();
		//POINT ptPos = { GET_X_LPARAM(dwPos), GET_Y_LPARAM(dwPos) };
		//ScreenToClient(&ptPos);

		::SetCursor(::LoadCursor(NULL, IDC_WAIT));
		return 1;
	}

	bHandled = FALSE;
	return 0;
}

LRESULT COptimizerDlg::OnLoadOptimized(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	task<void> t = create_task([this]()
	{
		m_bTaskInProgress = true;
		m_optimizerID = 0;

		CButton btnEnable = GetDlgItem(IDC_ENABLE_OPTIMIZATION);
		CButton btnExact = GetDlgItem(IDC_RADIO_EXACT);
		CButton btnToken = GetDlgItem(IDC_RADIO_TOKEN);
		CEdit edtNote = GetDlgItem(IDC_OPTIMIZATION_NOTE);
		CEdit edtOptimized = GetDlgItem(IDC_OPTIMIZED_STMT);
		CRichEditCtrl edtOriginal(GetDlgItem(IDC_ORIGINAL_STMT));

		btnEnable.EnableWindow(FALSE);
		btnExact.EnableWindow(FALSE);
		btnToken.EnableWindow(FALSE);
		edtNote.EnableWindow(FALSE);
		edtOptimized.EnableWindow(FALSE);
		edtOriginal.EnableWindow(FALSE);

		HRESULT hRes = ::CoInitialize(NULL);

		try
		{
			_ConnectionPtr spCon;
			hRes = spCon.CreateInstance(__uuidof(Connection));
			spCon->Open(m_pOptionsForEdit->m_sOptimizerConnectionString.c_str(), L"", L"", adConnectUnspecified);
			spCon->CursorLocation = adUseClient;

			_CommandPtr spCommand;
			spCommand.CreateInstance(__uuidof(Command));
			spCommand->ActiveConnection = spCon;
			spCommand->CommandType = adCmdText;
			spCommand->CommandText = L"SELECT optimizer_id,hashtype,enabled,final,category FROM pgnp_optimizer WHERE hash IN (?,?)";

			_ParameterPtr spParam = spCommand->CreateParameter(_bstr_t(L"hashExact"), adInteger, adParamInput, sizeof(int), variant_t(m_exactHash));
			spCommand->Parameters->Append(spParam);
			spParam = spCommand->CreateParameter(_bstr_t(L"hashParameterized"), adInteger, adParamInput, sizeof(int), variant_t(m_parameterizedHash));
			spCommand->Parameters->Append(spParam);

			_variant_t vRecordsAffected;
			_RecordsetPtr spRS = spCommand->Execute(&vRecordsAffected, NULL, adCmdText);

			if (spRS->IsEOF == FALSE)
			{
				m_optimizerID = (DWORD)spRS->Fields->GetItem("optimizer_id")->Value;
				_bstr_t enable = (_bstr_t)spRS->Fields->GetItem("enabled")->Value;
				int hashtype = (int)spRS->Fields->GetItem("hashtype")->Value;
				_bstr_t final = (_bstr_t)spRS->Fields->GetItem("final")->Value;
				_variant_t category = spRS->Fields->GetItem("category")->Value;

				btnEnable.SetCheck(enable == _bstr_t("Y"));
				btnExact.SetCheck(hashtype == 0);
				btnToken.SetCheck(hashtype != 0);
				edtOptimized.SetWindowText(final);
				if (category.vt != VT_NULL)
					edtNote.SetWindowText((_bstr_t)category);
			}
			else
			{
				CA2WEX<4000> stmt(m_clientSQL.c_str(), CP_UTF8);
				edtOptimized.SetWindowText(stmt);
			}

			SetModified(spRS->IsEOF);
			spRS->Close();

			SetTitleForConnection(m_pOptionsForEdit->m_sOptimizerConnectionString);

			CButton btnOK = GetDlgItem(IDOK);
			btnOK.EnableWindow();
		}
		catch (_com_error& e)
		{
			XMessageBox(m_hWnd, (TCHAR*)e.Description(), (TCHAR*)e.ErrorMessage(), MB_ICONERROR | MB_OK);
			PostMessage(WM_CLOSE);
			goto EXIT;
		}
		catch (...)
		{
			XMessageBox(m_hWnd, L"Failed to query Optimizer table. Check DB connection in the Options dialog.", L"Error");
			PostMessage(WM_CLOSE);
			goto EXIT;
		}

		btnEnable.EnableWindow();
		btnExact.EnableWindow();
		btnToken.EnableWindow();
		edtNote.EnableWindow();
		edtOptimized.EnableWindow();
		edtOriginal.EnableWindow();

		SendMessage(WM_SETCURSOR, (WPARAM)m_hWnd, HTCLIENT);
EXIT:
		::CoUninitialize();

		m_bTaskInProgress = false;
	});
	return 0;
}

LRESULT COptimizerDlg::OnChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	SetModified();
	return 0;
}

LRESULT COptimizerDlg::OnRadioChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	CButton btnExact = GetDlgItem(IDC_RADIO_EXACT);
	CStatic lblHash = GetDlgItem(IDC_OPTIMIZER_HASH);

	HASH hash = (btnExact.GetCheck() == 0) ? m_parameterizedHash : m_exactHash;
	WCHAR buf[100];
	swprintf(buf, L"(hash:%08X)", hash);

	lblHash.SetWindowText(buf);
	SetModified();
	return 0;
}

bool COptimizerDlg::IsModified() const
{
	WCHAR buf[100];
	GetWindowText(buf, _countof(buf));

	return buf[0] == '*';
}

void COptimizerDlg::SetModified(BOOL bModified)
{
	WCHAR buf[300];
	GetWindowText(buf, _countof(buf));

	if (bModified && buf[0] != '*')
	{
		memmove(&buf[2], &buf[0], 2*(wcslen(buf)+1));
		buf[0] = '*';
		buf[1] = ' ';
	}
	else if (!bModified && buf[0] == '*')
	{
		memmove(&buf[0], &buf[2], 2*(wcslen(buf)-1));
	}

	SetWindowText(buf);
}

static void GetNameValuePairs(WCHAR* szConnStr, map<wstring, wstring>& pvMap)
{
	list<wstring>	nameValuesLst;
	const WCHAR *strDelimit = L";";

	for (WCHAR* token = wcstok(szConnStr, strDelimit); token != NULL; token = wcstok(NULL, strDelimit))
	{
		nameValuesLst.push_back(token);
	}

	for(list<wstring>::const_iterator it=nameValuesLst.begin(); it != nameValuesLst.end(); it++)
	{
		WCHAR* copy = (WCHAR*)_alloca(((*it).length()+1) * sizeof(WCHAR));
		memcpy(copy, (*it).c_str(), ((*it).length()+1) * sizeof(WCHAR));

		wstring name(wcstok(copy, L"="));

		WCHAR* tmp = wcstok(NULL, L"=");	
		wstring value(tmp ? tmp : L"");
		pvMap.insert(std::pair<wstring, wstring>(name, value));
	}
}

static void AppendValue(WCHAR*& pAppendPoint, const map<wstring, wstring>& pvMap, const wstring& key)
{
	map<wstring, wstring>::const_iterator it = pvMap.find(key);
	if (it != pvMap.end())
	{
		memcpy(pAppendPoint, it->second.c_str(), it->second.length() * sizeof(WCHAR));
		pAppendPoint += it->second.length();
	}
}

void COptimizerDlg::SetTitleForConnection(const wstring& connectionString)
{
	WCHAR buf[300];
	GetWindowText(buf, _countof(buf));

	WCHAR* pAppendPoint = wcsstr(buf, L" [");
	if (pAppendPoint == NULL)
		pAppendPoint = &buf[wcslen(buf)];

	*pAppendPoint++ = L' ';
	*pAppendPoint++ = L'[';

	map<wstring, wstring> pvMap;
	int len = (connectionString.length()+1) * sizeof(WCHAR);
	WCHAR* szConnStr = (WCHAR*)_alloca(len);
	memcpy(szConnStr, connectionString.c_str(), len);
	GetNameValuePairs(szConnStr, pvMap);

	AppendValue(pAppendPoint, pvMap, L"Data Source");
	*pAppendPoint++ = L'|';
	AppendValue(pAppendPoint, pvMap, L"Initial Catalog");
	*pAppendPoint++ = L'|';
	AppendValue(pAppendPoint, pvMap, L"User ID");

	*pAppendPoint++ = L']';
	*pAppendPoint++ = L'\0';

	SetWindowText(buf);
}
