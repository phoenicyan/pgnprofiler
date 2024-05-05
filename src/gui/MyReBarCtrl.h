/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"

class CMyReBarCtrl : public CWindowImpl<CMyReBarCtrl, CReBarCtrl>
{
public:
	CMyReBarCtrl(CColorPalette& palit) : m_palit(palit) {}

private:
	BEGIN_MSG_MAP(CMyReBarCtrl)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		//MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()

	LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CRect rect;
		GetClientRect(&rect);

		::FillRect((HDC)wParam, &rect, m_palit.m_brBackGndAlt);

		return 1;
	}

	CColorPalette& m_palit;
};

