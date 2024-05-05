/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "PGNPropsCtrl.h"
#include "../PGNPOptions.h"
#include <algorithm>
#import "ado\msado20.tlb" no_namespace rename("EOF", "IsEOF") rename("BOF", "IsBOF")
#import "Ole DB\oledb32.dll" rename_namespace("oledb")

extern LVCOLUMN_EX SQLLOG_COLUMNS[];
extern int SQLLOG_COLUMNS_CNT;
extern int FindSQLLOGColumn(const WCHAR* colName);

/////////////////////////////////////////////////////////////////////////////
// CPGNPropsCtrl

CPGNPropsCtrl::CPGNPropsCtrl(CPGNPOptions& optionsForEdit, CColorPalette& palit, bool bEnableOptimizer)
	: m_ctrl(MakeCtrlForPalit(palit))
	, m_id(1), m_propId_EditWorkLogPath(0), m_propId_ColorTheme(0), m_propId_EditOptimizerConnectionString(0)
	, m_bProfileAllApps(optionsForEdit.m_dwProfileAllApps != 0), m_bRemoveTempFiles(optionsForEdit.m_dwDeleteWorkFiles != 0)
	, m_psWorkingPath(NULL), m_psConnectionString(NULL)
	, m_iColorTheme(optionsForEdit.m_dwColorTheme), m_iParamFormat(optionsForEdit.m_dwParamFormat), m_rgSelectColumn(new bool[SQLLOG_COLUMNS_CNT])
	, m_bEnableOptimizer(bEnableOptimizer)
	, m_palit(palit)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CPGNPropsCtrl::CPGNPropsCtrl()\n");

	SetMessagesColors();
	SetCustomDrawManager(m_ctrl, palit);

	ShowComments(m_ctrl, true);
	ShowButtons(m_ctrl, false);
	SetCommentsHeight(m_ctrl, 80);

	SetSecondColumnWidth(m_ctrl, 150);

	memset(m_rgSelectColumn, 0, SQLLOG_COLUMNS_CNT);
	
	PopulateProps(optionsForEdit);
}

CPGNPropsCtrl::~CPGNPropsCtrl()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CPGNPropsCtrl::~CPGNPropsCtrl()\n");

	///delete m_listBox;

	delete[] m_rgSelectColumn;
}

/////////////////////////////////////////////////////////////////////////////
// CPGNPropsCtrl message handlers

void CPGNPropsCtrl::PopulateProps(CPGNPOptions& optionsForEdit)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CPGNPropsCtrl::Populate\n");

	//-------------------------------------------
	// "General" category

	WPGDeepIterator& catIter1 = AddRootCategory(m_ctrl, m_id++, _T("General"));

	// Profile all apps ::checkbox
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, catIter1, m_id++, _T("Profile all apps"), WPGPropValueType::PVT_BOOL, &m_bProfileAllApps);
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_CHECKBOX));
		SetLookForValue(propIter, MakePropertyCheckboxLook());
		//(*propIter)->GetValue()->SetLabel(_T("Label"));
		SetComment(propIter, _T("When checked, start profiling all applications after launch. Note: Modification of this option may require the application restart."));
	}

	// Remove temporary files ::checkbox
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, catIter1, m_id++, _T("Delete work files on exit"), WPGPropValueType::PVT_BOOL, &m_bRemoveTempFiles);
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_CHECKBOX));
		SetLookForValue(propIter, MakePropertyCheckboxLook());
		SetComment(propIter, _T("When checked, the Profiler application deletes all work files on exit. Note: Modification of this option may require the application restart."));
	}

	// Working Path ::stlstring
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, catIter1, m_propId_EditWorkLogPath = m_id++, _T("Work Log Path"), WPGPropValueType::PVT_STLSTRING);
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_EDITBUTTON));
		SetStringValue(propIter, optionsForEdit.m_sWorkPath.c_str());
		m_psWorkingPath = (wstring*)GetDataPtr(propIter);
		SetComment(propIter, _T("Path to work log files. Note: Modification of this option may require the application restart."));
	}

	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, catIter1, m_propId_ColorTheme = m_id++, _T("Color Theme"), WPGPropValueType::PVT_ENUM, &m_iColorTheme);
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_LIST));
		AddRestrictedValue(propIter, _T("Windows Default"), 0);
		AddRestrictedValue(propIter, _T("Light"), 1);
		AddRestrictedValue(propIter, _T("Dark"), 2);
		SetComment(propIter, _T("Color theme."));
	}

	//-------------------------------------------
	// "Query Optimizer" category

	WPGDeepIterator& catIter2 = AddRootCategory(m_ctrl, m_id++, _T("Query Optimizer"));

	// Enable Query Optimizer ::checkbox
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, catIter2, m_id++, _T("Enable Query Optimizer"), WPGPropValueType::PVT_BOOL, &m_bEnableOptimizer);
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_CHECKBOX));
		SetLookForValue(propIter, MakePropertyCheckboxLook());
		SetComment(propIter, _T("Enable Query Optimizer."));
	}

	// Connection String ::stlstring
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, catIter2, m_propId_EditOptimizerConnectionString = m_id++, _T("Connection String"), WPGPropValueType::PVT_STLSTRING);
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_EDITBUTTON));
		SetStringValue(propIter, optionsForEdit.m_sOptimizerConnectionString.c_str());
		m_psConnectionString = (wstring*)GetDataPtr(propIter);
		SetComment(propIter, _T("Optimizer Connection String."));
	}

	//-------------------------------------------
	// "Trace Window" category

	WPGDeepIterator& catIter3 = AddRootCategory(m_ctrl, m_id++, _T("Trace Window"));

	// SQL Parameters Formatting ::list
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, catIter3, m_id++, _T("SQL Parameters Formatting"), WPGPropValueType::PVT_ENUM, &m_iParamFormat);
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_LIST));
		AddRestrictedValue(propIter, _T("Default"), PRMFMT_DEFAULT);
		AddRestrictedValue(propIter, _T("No truncation"), PRMFMT_NOTRUNC);
		AddRestrictedValue(propIter, _T("pgAdmin compatible"), PRMFMT_PGADMIN);
		SetComment(propIter, _T("Specifies how SQL parameters are formatted in the trace."));
	}

	//-------------------------------------------
	// "Messages Highlighting" sub-category

	WPGDeepIterator& subCatIter1 = AddSubCategoryUnderCategory(m_ctrl, catIter3, m_id++, _T("Messages Highlighting"));

	// "SELECT" color picker
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("SELECT"), WPGPropValueType::PVT_COLORRGB, &m_clrSelect,
			_T("SELECT queries from the user application."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("UPDATE"), WPGPropValueType::PVT_COLORRGB, &m_clrUpdate,
			_T("UPDATE statement from the user application."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("INSERT/COPY"), WPGPropValueType::PVT_COLORRGB, &m_clrInsert,
			_T("INSERT or COPY statement from the user application."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("DELETE"), WPGPropValueType::PVT_COLORRGB, &m_clrDelete,
			_T("DELETE statement from the user application."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("CREATE/ALTER/DROP"), WPGPropValueType::PVT_COLORRGB, &m_clrCreateDropAlter,
			_T("Schema modification statement from the user application."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("Procedures"), WPGPropValueType::PVT_COLORRGB, &m_clrProcedures,
			_T("Stored procedure call from the user application."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("SYS_SCHEMA"), WPGPropValueType::PVT_COLORRGB, &m_clrSysSchema,
			_T("SYS_SCHEMA queries from the PGNP provider."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("SYSTEM"), WPGPropValueType::PVT_COLORRGB, &m_clrSystem,
			_T("Commands generated by PGNP provider."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("ERROR"), WPGPropValueType::PVT_COLORRGB, &m_clrError,
			_T("Error while parsing, preparation or execution of a SQL statement."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("NOTIFY"), WPGPropValueType::PVT_COLORRGB, &m_clrNotify,
			_T("Notification from Postgres server about schema change caused by one of the connected PGNP providers."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("NONE"), WPGPropValueType::PVT_COLORRGB, &m_clrNone,
			_T("Statements the PGNP provider could not parse and passed through."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}
	{
		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter1, m_id++, _T("COMMENT"), WPGPropValueType::PVT_COLORRGB, &m_clrComment,
			_T("Application trace message."));
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_BUTTON));
	}

	//-------------------------------------------
	// "Columns" sub-category

	WPGDeepIterator& subCatIter2 = AddSubCategoryUnderCategory(m_ctrl, catIter3, m_id++, _T("Columns"));

	list<wstring> addedColumns;
	for (list<wstring>::const_iterator it=optionsForEdit.m_visibleColumnsList.begin(); it != optionsForEdit.m_visibleColumnsList.end(); it++)
	{
		const WCHAR* p = wcschr(it->c_str(), ':');
		if (!p) continue;
		int iCol = FindSQLLOGColumn(it->substr(0, p - it->c_str()).c_str());
		if (iCol < 0) continue;

		m_rgSelectColumn[iCol] = true;

		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter2, m_id++, SQLLOG_COLUMNS[iCol].pszText, WPGPropValueType::PVT_BOOL, &m_rgSelectColumn[iCol]);
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_CHECKBOXSPIN));
		SetLookForValue(propIter, MakePropertySpinCheckboxLook());
		SetComment(propIter, SQLLOG_COLUMNS[iCol].pszDescription);
		EnableItem(m_ctrl, propIter, SQLLOG_COLUMNS[iCol].bMayBeInvis);

		addedColumns.push_back(SQLLOG_COLUMNS[iCol].pszText);
	}

	for (int iCol=0; iCol < SQLLOG_COLUMNS_CNT; iCol++)
	{
		if (std::find(addedColumns.begin(), addedColumns.end(), SQLLOG_COLUMNS[iCol].pszText) != addedColumns.end())
			continue;

		m_rgSelectColumn[iCol] = false;

		WPGDeepIterator& propIter = AddPropertyUnderCategory(m_ctrl, subCatIter2, m_id++, SQLLOG_COLUMNS[iCol].pszText, WPGPropValueType::PVT_BOOL, &m_rgSelectColumn[iCol]);
		SetFeel(propIter, GetRegisteredFeel(m_ctrl, WPG_FEEL_CHECKBOXSPIN));
		SetLookForValue(propIter, MakePropertySpinCheckboxLook());
		SetComment(propIter, SQLLOG_COLUMNS[iCol].pszDescription);
		EnableItem(m_ctrl, propIter, SQLLOG_COLUMNS[iCol].bMayBeInvis);
	}
}

HWND CPGNPropsCtrl::Create(HWND hWndParent, _U_RECT rcClient, LPCTSTR szWindowName, DWORD dwStyle, DWORD dwExStyle, _U_MENUorID MenuOrID /*= 0U*/, LPVOID lpCreateParam /*= 0*/)
{
	return CreateCtrl(m_ctrl, hWndParent, rcClient, szWindowName, dwStyle, dwExStyle);
}

//+ 3rdparty
extern BOOL XBrowseForFolder(HWND hWnd,
					  LPCTSTR lpszInitialFolder,
					  int nRootFolder,
					  LPCTSTR lpszCaption,
					  LPTSTR lpszBuf,
					  DWORD dwBufSize,
					  BOOL bEditBox /*= FALSE*/);
//-

inline void TESTHR( HRESULT hr ) { if( FAILED(hr) ) _com_issue_error( hr ); }

bool CPGNPropsCtrl::OnPropertyButtonClicked(WPGDeepIterator& iter)
{
	int propertyId = GetID(iter);

	ATLTRACE2(atlTraceDBProvider, 0, L"CPGNPropsCtrl::OnPropertyButtonClicked  propertyId=%d\n", propertyId);

	if (m_propId_EditWorkLogPath == propertyId)
	{
		// Edit Work Log Path
		TCHAR szFolder[MAX_PATH*2] = { _T('\0') };

		BOOL bRet = XBrowseForFolder(GetHWnd(m_ctrl),
									 m_psWorkingPath->c_str(),			// lpszInitial,
									 0x0011,							// My Computer, i.e. CSIDL_DRIVES
									 L"Work Log Path",					// Caption
									 szFolder,							// Root folder
									 sizeof(szFolder)/sizeof(TCHAR)-2,
									 FALSE);
		if (bRet)
		{
			SetStringValue(iter, szFolder);
			return true;
		}
	}
	else if (m_propId_EditOptimizerConnectionString == propertyId)
	{
		// Edit Optimizer Connection String
		oledb::IDataSourceLocatorPtr p_IDSL = NULL;			// Data Link dialog object
		_ConnectionPtr p_conn = NULL;						// Connection returned from Data Link dialog
		BOOL b_ConnValid = FALSE;

		try
		{
			// Create an instance of the IDataSourceLocator interface and check for errors.
			HRESULT	hr = p_IDSL.CreateInstance(__uuidof( oledb::DataLinks));
			TESTHR(hr);

			// If the passed in Connection String is blank, we are creating a new connection
			p_conn.CreateInstance("ADODB.Connection");
			if (m_psConnectionString->length() == 0)
			{
				p_conn->ConnectionString = L"Provider=PGNP.1;Password=;User ID=;Initial Catalog=;Data Source=localhost;Connect Timeout=120;";
			}
			else
			{
				// We are going to use a pre-existing connection string, so first we need to
				// create a connection object ourselves. After creating it, we'll copy the
				// connection string into the object's connection string member
				p_conn->ConnectionString = m_psConnectionString->c_str();
			}

			// When editing a data link, the IDataSourceLocator interface requires we pass 
			// it a connection object under the guise of an IDispatch interface. So,
			// here we'll query the interface to get a pointer to the IDispatch interface,
			// and we'll pass that to the data link dialog.
			IDispatch * p_Dispatch = NULL;
			p_conn.QueryInterface(IID_IDispatch, (LPVOID*)&p_Dispatch);

			p_IDSL->PuthWnd((LONG)GetParent(m_ctrl));

			if (p_IDSL->PromptEdit(&p_Dispatch)) b_ConnValid = TRUE;

			p_Dispatch->Release();

			if (b_ConnValid)
			{
				SetStringValue(iter, (TCHAR*)p_conn->ConnectionString);
			}

			if (p_conn->GetState() == adStateOpen) p_conn->Close();
		}
		catch (_com_error& e)
		{
			XMessageBox(GetParent(m_ctrl), (TCHAR*)e.Description(), (TCHAR*)e.ErrorMessage(), MB_ICONERROR | MB_OK);
		}
	}
	else if (WPGPropValueType::PVT_COLORRGB == GetValueType(iter))
	{
		if (m_iColorTheme != CColorPalette::THEME_DEFAULT)
		{
			CColorDialog dlg;
			dlg.m_cc.rgbResult = GetValueColor(iter);
			dlg.m_cc.Flags |= CC_FULLOPEN | CC_RGBINIT;

			if (dlg.DoModal() == IDOK)
			{
				SetValueColor(iter, dlg.GetColor());
				return true;
			}
		}
	}
	else
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"** CPGNPropsCtrl::OnPropertyButtonClicked: ERROR undefined propertyId=%d\n", propertyId);
	}

	return false;	// We consider that the underlying value has not changed
}

void CPGNPropsCtrl::OnShowInPlaceCtrl(CWindow* pWnd, WPGDeepIterator& iter)
{
	int propertyId = GetID(iter);

	ATLTRACE2(atlTraceDBProvider, 0, L"CPGNPropsCtrl::OnShowInPlaceCtrl  propertyId=%d\n", propertyId);
}

void CPGNPropsCtrl::OnHotLinkClicked(WPGDeepIterator& iter)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CPGNPropsCtrl::OnHotLinkClicked\n");

}

BOOL CPGNPropsCtrl::OnPropertyChanged(WPGDeepIterator& iter)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CPGNPropsCtrl::OnPropertyChanged\n");

	int propertyId = GetID(iter);

	if (propertyId == m_propId_ColorTheme)
	{
		ATLTRACE2(atlTraceDBProvider, 0, L"		m_iColorTheme = %d\n", m_iColorTheme);
		GetMessagesColors();
		::SendMessage(GetParent(m_ctrl), MYWM_THEME_CHANGE, m_iColorTheme, 0);
		SetMessagesColors();
		SetCtrlRefColor(m_ctrl, GetCtrlBkgColorAlt(m_ctrl), true);
		RedrawGridWindow(m_ctrl);
	}
	else if (propertyId == 29)
	{
		///SetCtrlFont((HFONT)m_font);
	}
	return TRUE;
}

void CPGNPropsCtrl::OnEnableItem(WPGDeepIterator& iter, bool enable, bool direct)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CPGNPropsCtrl::OnEnableItem\n");

	int propertyId = GetID(iter);
}

void CPGNPropsCtrl::GetMessagesColors()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CPGNPropsCtrl::GetMessagesColors()\n");

	m_palit.SetDynColor(QCLR_SELECT, m_clrSelect);
	m_palit.SetDynColor(QCLR_INSERT, m_clrInsert);
	m_palit.SetDynColor(QCLR_UPDATE, m_clrUpdate);
	m_palit.SetDynColor(QCLR_DELETE, m_clrDelete);
	m_palit.SetDynColor(QCLR_CREATEDROPALTER, m_clrCreateDropAlter);
	m_palit.SetDynColor(QCLR_PROCEDURES, m_clrProcedures);
	m_palit.SetDynColor(QCLR_SYSSCHEMA, m_clrSysSchema);
	m_palit.SetDynColor(QCLR_SYSTEM, m_clrSystem);
	m_palit.SetDynColor(QCLR_ERROR, m_clrError);
	m_palit.SetDynColor(QCLR_NOTIFY, m_clrNotify);
	m_palit.SetDynColor(QCLR_NONE, m_clrNone);
	m_palit.SetDynColor(QCLR_COMMENT, m_clrComment);
}

void CPGNPropsCtrl::SetMessagesColors()
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CPGNPropsCtrl::SetMessagesColors(palit)\n");

	m_clrSelect = m_palit.GetDynColor(QCLR_SELECT);// m_crTrcSelect;
	m_clrInsert = m_palit.GetDynColor(QCLR_INSERT);// m_crTrcInsert;
	m_clrUpdate = m_palit.GetDynColor(QCLR_UPDATE);// m_crTrcUpdate;
	m_clrDelete = m_palit.GetDynColor(QCLR_DELETE);// m_crTrcDelete;
	m_clrCreateDropAlter = m_palit.GetDynColor(QCLR_CREATEDROPALTER);// m_crTrcAlter;
	m_clrProcedures = m_palit.GetDynColor(QCLR_PROCEDURES);// m_crTrcInternalProc;
	m_clrSysSchema = m_palit.GetDynColor(QCLR_SYSSCHEMA);// m_crTrcSysSchema;
	m_clrSystem = m_palit.GetDynColor(QCLR_SYSTEM);// m_crTrcSystem;
	m_clrError = m_palit.GetDynColor(QCLR_ERROR);// m_crTrcError;
	m_clrNotify = m_palit.GetDynColor(QCLR_NOTIFY);// m_crTrcNotifies;
	m_clrNone = m_palit.GetDynColor(QCLR_NONE);// m_crTrcNone;
	m_clrComment = m_palit.GetDynColor(QCLR_COMMENT);// m_crTrcComment;
}

void CPGNPropsCtrl::SetFocus()
{
	SetCtrlFocus(m_ctrl);
}

void CPGNPropsCtrl::DestroyWindow()
{
	DestroyCtrlWindow(m_ctrl);
}

WPGDeepIterator& CPGNPropsCtrl::FindItem(int id)
{
	return FindCtrlItem(m_ctrl, id);
}

void CPGNPropsCtrl::SelectIterItem(WPGDeepIterator& iter)
{
	::SelectIterItem(m_ctrl, iter);
}

void CPGNPropsCtrl::SetRefColor(COLORREF color, bool force /*= false*/)
{
	SetCtrlRefColor(m_ctrl, color, force);
}

COLORREF CPGNPropsCtrl::GetRefColor()
{
	return GetCtrlRefColor(m_ctrl);
}

COLORREF CPGNPropsCtrl::GetBkgColorAlt()
{
	return GetCtrlBkgColorAlt(m_ctrl);
}

WPGSiblingIterator& CPGNPropsCtrl::GetCategoryItems(LPCTSTR catName)
{
	return GetCtrlCategoryItems(m_ctrl, catName);
}

void CPGNPropsCtrl::Next(WPGSiblingIterator& iter)
{
	Advance(m_ctrl, iter);
}
