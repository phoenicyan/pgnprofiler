/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"

template <bool t_bVertical = true>
class CMySplitterWindowT : public CSplitterWindowImpl<CMySplitterWindowT<t_bVertical> >
{
public:
	DECLARE_WND_CLASS_EX(_T("My_SplitterWindow"), CS_DBLCLKS, COLOR_WINDOW)

	CMySplitterWindowT(CColorPalette& palit)
		: CSplitterWindowImpl<CMySplitterWindowT<t_bVertical> >(t_bVertical)
		, m_palit(palit)
	{}

	// Overrideables
	void DrawSplitterBar(CDCHandle dc)
	{
		RECT rect;

		// If we're not using a colored bar, let the base class do the default drawing.
		// CSplitterWindowImpl<CMySplitterWindowT<t_bVertical> >::DrawSplitterBar(dc);

		//// Create a brush to paint with, if we haven't already done so.
		//if (m_br.IsNull())
		//	m_br.CreateHatchBrush(HS_DIAGCROSS, t_bVertical ? RGB(255, 0, 0) : RGB(0, 0, 255));

		if (GetSplitterBarRect(&rect))
		{
			dc.FillRect(&rect, m_palit.m_brBackGnd);

			// draw 3D edge if needed
			if ((GetExStyle() & WS_EX_CLIENTEDGE) != 0)
				dc.DrawEdge(&rect, EDGE_RAISED, t_bVertical ? (BF_LEFT | BF_RIGHT) : (BF_TOP | BF_BOTTOM));
		}
	}

protected:
	CColorPalette& m_palit;
};

typedef CMySplitterWindowT<true>    CMySplitterWindow;
typedef CMySplitterWindowT<false>   CMyHorSplitterWindow;
