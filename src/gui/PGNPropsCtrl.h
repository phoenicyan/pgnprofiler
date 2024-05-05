/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "WTLPropGrid.h"

class CPGNPOptions;

/////////////////////////////////////////////////////////////////////////////
// CPGNPropsCtrl window

class CPGNPropsCtrl
{
	friend class COptionsDlg;

// Construction
public:
	CPGNPropsCtrl(CPGNPOptions& optionsForEdit, CColorPalette& palit, bool bEnableOptimizer);
	virtual ~CPGNPropsCtrl();

// Attributes
public:
	WPGPropertiesCtrl& m_ctrl;
	int m_id, m_propId_EditWorkLogPath, m_propId_ColorTheme, m_propId_EditOptimizerConnectionString;

	// General properties
	bool m_bProfileAllApps, m_bRemoveTempFiles;
	bool m_bEnableOptimizer;
	CColorPalette& m_palit;

	wstring* m_psWorkingPath, *m_psConnectionString;
	int m_iColorTheme;
	int m_iParamFormat;

	// Highlighting properties
	COLORREF m_clrSelect, m_clrInsert, m_clrUpdate, m_clrDelete, m_clrCreateDropAlter, m_clrProcedures, 
		m_clrSysSchema, m_clrSystem, m_clrError, m_clrNotify, m_clrNone, m_clrComment;

	// Select Columns properties
	bool* m_rgSelectColumn;

// Operations
public:
	HWND Create(HWND hWndParent, _U_RECT rcClient, LPCTSTR szWindowName, DWORD dwStyle, DWORD dwExStyle, _U_MENUorID MenuOrID = 0U, LPVOID lpCreateParam = 0);

	void PopulateProps(CPGNPOptions& optionsForEdit);
	void SetFocus();
	void DestroyWindow();
	WPGDeepIterator& FindItem(int id);
	void SelectIterItem(WPGDeepIterator& iter);
	void SetRefColor(COLORREF color, bool force = false);
	COLORREF GetRefColor();
	COLORREF GetBkgColorAlt();
	WPGSiblingIterator& GetCategoryItems(LPCTSTR catName);
	void Next(WPGSiblingIterator& iter);

// Callbacks
	bool OnPropertyButtonClicked(WPGDeepIterator& iter);
	void OnShowInPlaceCtrl(CWindow* pWnd, WPGDeepIterator& iter);
	void OnHotLinkClicked(WPGDeepIterator& iter);
	BOOL OnPropertyChanged(WPGDeepIterator& iter);
	void OnEnableItem(WPGDeepIterator& iter, bool enable, bool direct);

protected:
	void GetMessagesColors();
	void SetMessagesColors();
};
