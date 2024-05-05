/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "StdAfx.h"
#include "resource.h"
#include "FilterView.h"
#include "ExplorerView.h"
#include "PGNProfilerView.h"
#include "DetailsView.h"
#include "aboutdlg.h"
#include "MainFrm.h"
#include "crc32.h"
#include <sstream>
#include <iostream>

LRESULT CDetailsView::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	LRESULT lResult = DefWindowProc(uMsg, wParam, lParam);

	HFONT hFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0,
							DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
							DEFAULT_PITCH | FF_DONTCARE, L"Courier New");
	SetFont(hFont);

	SetLimitText(0);

	return lResult;
}

LRESULT CDetailsView::OnContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	HMENU hMenu = ::LoadMenu(_Module.GetResourceInstance(),(LPCTSTR)IDR_DETAILSV_MENU);
	if (hMenu != NULL)
	{
		HMENU hSubMenu = ::GetSubMenu(hMenu,0);
		if(hSubMenu != NULL)
		{
			::TrackPopupMenu(hSubMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, NULL);

			::PostMessage(m_hWnd, WM_NULL, 0, 0); 
		}
		::DestroyMenu(hMenu);
	}
	return 0;
}

LRESULT CDetailsView::OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if ((MK_CONTROL & wParam) != 0)
	{
		LONG delta = (int)GET_WHEEL_DELTA_WPARAM(wParam) < 0 ? 2 : -2;

		AlterWindowFont(*this, delta);
		::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_DETAILSFONT, 0);

		return 0;
	}

	bHandled = FALSE;
	return 1;
}

LRESULT CDetailsView::OnChar(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam == '.' || wParam == '>' || wParam == '+' || wParam == '=')
	{
		AlterWindowFont(*this, -2);
		::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_DETAILSFONT, 0);
		return 1;
	}

	if (wParam == ',' || wParam == '<' || wParam == '-' || wParam == '_')
	{
		AlterWindowFont(*this, 2);
		::PostMessage(m_hMainWnd, MYMSG_SAVESETTINGS, OT_DETAILSFONT, 0);
		return 1;
	}

	bHandled = FALSE;
	return 0;
}

LRESULT CDetailsView::OnGetDlgCode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	return DLGC_WANTALLKEYS;
}

LRESULT CDetailsView::OnZoomin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	this->PostMessage(WM_CHAR, '.', 1);
	return 0;
}

LRESULT CDetailsView::OnZoomout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	this->PostMessage(WM_CHAR, ',', 1);
	return 0;
}

LRESULT CDetailsView::OnEditCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nStartChar, nEndChar;
	GetSel(nStartChar, nEndChar);
	if (nStartChar == nEndChar)
	{
		SetSelAll();
		Copy();
		SetSel(nStartChar, nEndChar, TRUE);
	}
	else
	{
		Copy();
	}
	return 0;
}

void CDetailsView::ShowSQL(const list<DWORD_PTR>& dataList)
{
	WCHAR timebuf[64], buf[1000];
	ClsCrc32 crc32;
	std::wostringstream woss;

	::SetCursor(::LoadCursor(NULL, IDC_WAIT));

	for (list<DWORD_PTR>::const_iterator it=dataList.begin(); it != dataList.end(); it++)
	{
		BYTE* baseAddr = (BYTE*)*it;
		TRC_TYPE trcType = (TRC_TYPE)baseAddr[4];
		switch (trcType)
		{
		case TRC_CLIENTSQL:
		case TRC_SYSTEMSQL:
		// deprecated SQLType
		case TRC_SCHEMA_TABLES:
		case TRC_SCHEMA_COLUMNS:
		case TRC_SCHEMA_INDEXES:
		case TRC_SCHEMA_CATALOGS:
		case TRC_SCHEMA_FOREIGN_KEYS:
		case TRC_SCHEMA_PRIMARY_KEYS:
		case TRC_SCHEMA_PROCEDURE_COLUMNS:
		case TRC_SCHEMA_PROCEDURE_PARAMETERS:
		case TRC_SCHEMA_PROCEDURES:
		// current SQLType
		case TRC_NOTIFIES:
		case TRC_SYS_SCHEMA:
		case TRC_USER_SCHEMA:
			{
				CProfSQLmsg profmsg(baseAddr, true);

				// Client SQL
				if (profmsg.ClientSQL.Length > 1)
				{
					PrintAbsTimeStamp(timebuf, profmsg.TimeStamp);
					swprintf(buf, L"----------   Client SQL   %s  %08X ------------\r\n",
						timebuf,
						crc32.Crc((LPBYTE)profmsg.ClientSQL.TextPtr, profmsg.ClientSQL.Length-1));
					woss << buf;
					CA2WEX<4000> wsClientSQL(profmsg.ClientSQL.TextPtr, CP_UTF8);
					woss << FormatItemText(wsClientSQL) << L"\r\n";
				}

				// Executed SQL
				if (profmsg.ExecutedSQL.Length > 1)
				{
					swprintf(buf, L"----------   Executed SQL   Parse:%1.3lf Prepare:%1.3lf Execute:%1.3lf GetRows:%1.3lf Rows:%d ---------\r\n",
						(double)profmsg.ParseTime * 1000 / g_qpcFreq,
						(double)profmsg.PrepareTime * 1000 / g_qpcFreq,
						(double)profmsg.ExecTime * 1000 / g_qpcFreq,
						(double)profmsg.GetdataTime * 1000 / g_qpcFreq,
						profmsg.NumRows);
					woss << buf;
					CA2WEX<4000> wsExecutedSQL(profmsg.ExecutedSQL.TextPtr, CP_UTF8);
					woss << FormatItemText(wsExecutedSQL) << L"\r\n";
				}
				woss << L"\r\n";
				break;
			}

		case TRC_ERROR:
			{
				CProfERRORmsg profmsg(baseAddr, true);

				// Client SQL
				if (profmsg.ClientSQL.Length > 1)
				{
					PrintAbsTimeStamp(timebuf, profmsg.TimeStamp);
					swprintf(buf, L"----------   Client SQL   %s  %08X ------------\r\n",
						timebuf,
						crc32.Crc((LPBYTE)profmsg.ClientSQL.TextPtr, profmsg.ClientSQL.Length-1));
					woss << buf;
					CA2WEX<4000> wsClientSQL(profmsg.ClientSQL.TextPtr, CP_UTF8);
					woss << FormatItemText(wsClientSQL) << L"\r\n";
				}
				// Error message
				woss << L"----------   Error   ---------\r\n";
				CA2WEX<4000> wsError(profmsg.ErrorText.TextPtr, CP_UTF8);
				woss << FormatItemText(wsError) << L"\r\n\r\n";
				break;
			}

		case TRC_COMMENT:
			{
				CProfSQLmsg profmsg(baseAddr, true);

				// Client SQL
				if (profmsg.ClientSQL.Length > 1)
				{
					CA2WEX<4000> wsClientSQL(profmsg.ClientSQL.TextPtr, CP_UTF8);
					woss << FormatItemText(wsClientSQL) << L"\r\n";
				}
				woss << L"\r\n";
				break;
			}

		default:
			woss << L"<< Unknown message type >>\r\n";
			break;
		}
	}

	SetWindowText(woss.str().c_str());

	//+ hack: simulate wheel scroll down and up to refresh scroll info. Is there a better way?
	CRect rc;
	GetWindowRect(&rc);
	SendMessage(WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), MAKELPARAM(rc.right - 4, rc.top + 24));
	SendMessage(WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), MAKELPARAM(rc.right - 4, rc.top + 24));
	//-

	::SetCursor(::LoadCursor(NULL, IDC_ARROW));
}

const WCHAR* CDetailsView::FormatItemText(const WCHAR* txt)
{
	m_text.clear();

	bool r = false;
	for (; *txt;)
	{
		WCHAR c = *txt++;
		if (c == '\n' && !r)
			m_text += L'\r';

		m_text += c;

		r = (c == '\r');
	}

	return m_text.c_str();
}
