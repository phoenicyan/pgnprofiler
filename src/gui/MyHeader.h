/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"

class CMyHeader
	: public CWindowImpl<CMyHeader, CHeaderCtrl>
{
public:
	CMyHeader(CColorPalette& palit) : m_palit(palit) {}

private:
	BEGIN_MSG_MAP(CMyHeader)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()

	LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 1;
	}
	
	LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		PAINTSTRUCT ps;
		TCHAR buf[256];

		HDC hdc = BeginPaint(&ps);

		FillRect(hdc, &ps.rcPaint, m_palit.m_brBackGndAlt);

		HFONT hFont = GetParent().GetFont();
		::SelectObject(hdc, hFont);

		::SetTextColor(hdc, m_palit.CrText);
		::SetBkColor(hdc, m_palit.CrBackGndAlt);

		int nItems = GetItemCount();
		for (int i = 0; i < nItems; i++)
		{
			HD_ITEM hditem1;
			hditem1.mask = HDI_TEXT | HDI_FORMAT | HDI_ORDER;
			hditem1.pszText = buf;
			hditem1.cchTextMax = 255;
			GetItem(i, &hditem1);

			CRect rect;
			GetItemRect(i, &rect);
			rect.left += 4;

			::DrawText(hdc, buf, _tcslen(buf), rect, DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
		}

		EndPaint(&ps);
		return 0;
	}

	CColorPalette& m_palit;
};
