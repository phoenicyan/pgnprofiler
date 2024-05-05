/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"

class CMyToolBar
	: public CWindowImpl<CMyToolBar, CToolBarCtrl>
{
public:
	CMyToolBar(CColorPalette& palit) : m_palit(palit) {}

private:
	BEGIN_MSG_MAP(CMyToolBar)
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
		const int cxSeparator = 8;
		const int cxyButtonMargin = 7;
		CImageList il = GetImageList();
		CSize btnSize(GetButtonSize());

		HDC hdc = BeginPaint(&ps);

		FillRect(hdc, &ps.rcPaint, m_palit.m_brBackGnd);

		int nItems = GetButtonCount();
		int leftOffset = 0;
		for (int i = 0; i < nItems; i++)
		{
			TBBUTTON tbb;
			if (!GetButton(i, &tbb))
				continue;
			
			ATLTRACE2(atlTraceDBProvider, 2, L"CMyToolBar::OnPaint: iBitmap=%d, state=%d, style=%d\n", tbb.iBitmap, tbb.fsState, tbb.fsStyle);

			if (BTNS_BUTTON == tbb.fsStyle)
			{
				//::BitBlt(hdc, leftOffset, 0, sizeDst.cx, sizeDst.cy, hdcSrc, 0, 0, SRCCOPY);
				CPoint pt(leftOffset, 4);
				il.Draw(hdc, tbb.iBitmap, pt, BTNS_BUTTON);
				leftOffset += btnSize.cx;
			}
			else if (BTNS_SEP == tbb.fsStyle)
			{
				leftOffset += cxSeparator;
			}
		}

		EndPaint(&ps);

		return 0;
	}

	CColorPalette& m_palit;
};
