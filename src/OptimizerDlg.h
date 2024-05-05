/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"
#include "ProfilerDef.h"
#include "resource.h"
#include "gui/SkinWindow.h"

#include "Optimizer.h"

class CPGNPOptions;

class COptimizerDlg
	: public CDialogImpl<COptimizerDlg>
	, public CSkinWindow<COptimizerDlg>
	, public CWinDataExchange<COptimizerDlg>
	, public CMessageFilter
{
public:
	string m_clientSQL;
	CPGNPOptions* m_pOptionsForEdit;
	CColorPalette& m_palit;

	enum { IDD = IDD_OPTIMIZER };

	BEGIN_MSG_MAP(COptimizerDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CTLCOLORDLG,       OnColorDlg) // For dialog background.
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC,    OnColorDlg) // For static and read only edit.
		MESSAGE_HANDLER(WM_CTLCOLOREDIT,      OnColorDlg) // For edit boxes
		MESSAGE_HANDLER(WM_CTLCOLORBTN,       OnColorDlg) // Owner-drawn only will respond.
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX,   OnColorDlg) // List and combo.
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(MYMSG_LOAD_OPTIMIZED, OnLoadOptimized);
		MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_CODE_HANDLER(EN_CHANGE, OnChange)
		COMMAND_HANDLER(IDC_ENABLE_OPTIMIZATION, BN_CLICKED, OnChange)
		COMMAND_HANDLER(IDC_RADIO_EXACT, BN_CLICKED, OnRadioChange)
		COMMAND_HANDLER(IDC_RADIO_TOKEN, BN_CLICKED, OnRadioChange)

		CHAIN_MSG_MAP(CSkinWindow<COptimizerDlg>)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return ::IsDialogMessage(m_hWnd, pMsg);
	}

	COptimizerDlg(CColorPalette& palit) 
		: m_pOptionsForEdit(nullptr), m_bTaskInProgress(false), m_optimizerID(0), m_exactHash(0), m_parameterizedHash(0)
		, CSkinWindow<COptimizerDlg>(palit)
		, m_palit(palit)
	{}

	// Handlers
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	
	LRESULT OnLoadOptimized(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	
	LRESULT OnSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled);

	LRESULT OnRadioChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled);

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
	bool m_bTaskInProgress;			// true when an async task is in progress

	DWORD m_optimizerID;			// existing optimizer ID, or 0 when an optimization hint did not previously exist
	HASH m_exactHash, m_parameterizedHash;

	bool IsModified() const;
	void SetModified(BOOL bModified = TRUE);

	void SetTitleForConnection(const wstring& connectionString);
};
