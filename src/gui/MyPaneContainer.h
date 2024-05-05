/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"

class CMyPaneContainer : public CPaneContainerImpl<CMyPaneContainer>
{
public:
	DECLARE_WND_CLASS_EX(_T("My_PaneContainer"), 0, -1)

	CMyPaneContainer(CColorPalette& palit) : m_palit(palit)
	{ }

	// Overrides
	void DrawPaneTitle(CDCHandle dc);

protected:
	CFont m_titleFont;
	CColorPalette& m_palit;
};
