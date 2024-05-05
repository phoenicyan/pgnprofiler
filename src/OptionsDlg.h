/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "gui\PGNPropsCtrl.h"
#include "PGNPOptions.h"
#include "resource.h"
#include "gui/SkinWindow.h"

extern LVCOLUMN_EX SQLLOG_COLUMNS[];
extern int SQLLOG_COLUMNS_CNT;

class COptionsDlg
	: public CDialogImpl<COptionsDlg>
	, public CSkinWindow<COptionsDlg>
	, public CWinDataExchange<COptionsDlg>
	, public CMessageFilter
{
public:
	enum { IDD = IDD_OPTIONS };

	BEGIN_MSG_MAP(COptionsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)

		MESSAGE_HANDLER(WM_CTLCOLORDLG,       OnCtlColor) // For dialog background.
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(MYWM_THEME_CHANGE, OnPGNProfThemeChange)
		MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)

		COMMAND_ID_HANDLER(IDC_RESET_SETTINGS, OnResetSettings)
		COMMAND_ID_HANDLER(IDC_EXPORT_SETTINGS, OnExportSettings)
		COMMAND_ID_HANDLER(IDC_IMPORT_SETTINGS, OnImportSettings)
		
		CHAIN_MSG_MAP(CSkinWindow<COptionsDlg>)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return ::IsDialogMessage(m_hWnd, pMsg);
	}

	CPGNPropsCtrl* m_props;
	int m_startTheme;				// (CColorPalette::THEME) theme on the start of the dialog UI
	int m_selectItem;
	CPGNPOptions& m_optionsForEdit;
	bool m_bEnableOptimizer;		// "Optimizer Enabled" flag calculated on start of the dialog; used to check if the flag changed in the UI.

private:
	CColorPalette& m_palit;

public:
	COptionsDlg(CPGNPOptions& optionsForEdit, CColorPalette& palit, int selectItem = -1);

	// Handlers
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnCtlColor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnPGNProfThemeChange(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnResetSettings(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnExportSettings(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnImportSettings(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

protected:
	bool IsOptimizerEnabled();
	bool m_bTaskInProgress;			// true when an async task is in progress
};
