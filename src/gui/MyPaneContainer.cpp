/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "../resource.h"
#include "MyPaneContainer.h"

//////////////////////////////////////////////////////////////////////

void CMyPaneContainer::DrawPaneTitle(CDCHandle dc)
{
	RECT rect;

	// If we're not using a colored header, let the base class do the default drawing.
	// CPaneContainerImpl<CMyPaneContainer>::DrawPaneTitle ( dc );

	GetClientRect(&rect);

	if (IsVertical())
	{
		TRIVERTEX tv[] = { { rect.left, rect.top, 0xff00 },
		{ rect.left + m_cxyHeader, rect.bottom, 0, 0xff00 } };
		GRADIENT_RECT gr = { 0, 1 };

		dc.GradientFill(tv, 2, &gr, 1, GRADIENT_FILL_RECT_V);
	}
	else
	{
		TRIVERTEX tv[] = { { rect.left, rect.top, 0xff00 },
							{ rect.right, rect.top + m_cxyHeader, 0, 0xff00 } };

		dc.FillRect(&rect, m_palit.m_brBackGndAlt);

		RECT rect1 = rect;
		rect.top -= 2;
		//rect1.right += 1;
		dc.FrameRect(&rect1, m_palit.m_brBackGndSel);

		rect.bottom = rect.top + m_cxyHeader;

		// draw title only for horizontal pane container
		dc.SaveDC();

		dc.SetTextColor(m_palit.CrTextAlt);
		dc.SetBkMode(TRANSPARENT);
		dc.SelectFont ( GetTitleFont() );

		rect.left += m_cxyTextOffset;
		rect.right -= m_cxyTextOffset;

		if (m_tb.m_hWnd != NULL)
			rect.right -= m_cxToolBar;

		dc.DrawText(m_szTitle, -1, &rect, DT_LEFT | DT_SINGLELINE | DT_BOTTOM | DT_END_ELLIPSIS);

		dc.RestoreDC(-1);
	}
}
