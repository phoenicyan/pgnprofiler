/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "ColorPalette.h"

#pragma region Ancillary

class CPenDC
{
protected:
	CPen m_pen;
	HDC m_hDC;
	HPEN m_hOldPen;

public:
	CPenDC(HDC hDC, COLORREF crColor) : m_hDC(hDC)
	{
		ATLVERIFY(m_pen.CreatePen(PS_SOLID, 1, crColor));
		m_hOldPen = (HPEN)::SelectObject(m_hDC, m_pen);
	}

	~CPenDC()
	{
		::SelectObject(m_hDC, m_hOldPen);
	}

	void Color(COLORREF crColor)
	{
		::SelectObject(m_hDC, m_hOldPen);
		ATLVERIFY(m_pen.DeleteObject());
		ATLVERIFY(m_pen.CreatePen(PS_SOLID, 1, crColor));
		m_hOldPen = (HPEN)::SelectObject(m_hDC, m_pen);
	}

	COLORREF Color() const
	{
		LOGPEN logPen;

		((CPenDC*)this)->m_pen.GetLogPen(&logPen);

		return logPen.lopnColor;
	}
};

class CBrushDC
{
protected:
	CBrush m_brush;
	HDC m_hDC;
	HBRUSH m_hOldBrush;

public:
	CBrushDC(HDC hDC, COLORREF crColor) : m_hDC(hDC)
	{
		if (crColor == CLR_NONE) m_brush.Attach((HBRUSH)::GetStockObject(NULL_BRUSH));
		else                       ATLVERIFY(m_brush.CreateSolidBrush(crColor));
		m_hOldBrush = (HBRUSH)::SelectObject(m_hDC, m_brush);
	}

	~CBrushDC()
	{
		::SelectObject(m_hDC, m_hOldBrush);
	}

	void Color(COLORREF crColor)
	{
		::SelectObject(m_hDC, m_hOldBrush);
		ATLVERIFY(m_brush.DeleteObject());
		if (crColor == CLR_NONE) m_brush.Attach((HBRUSH)::GetStockObject(NULL_BRUSH));
		else                       ATLVERIFY(m_brush.CreateSolidBrush(crColor));
		m_hOldBrush = (HBRUSH)::SelectObject(m_hDC, m_brush);
	}

	COLORREF Color() const
	{
		LOGBRUSH logBrush;

		((CBrushDC*)this)->m_brush.GetLogBrush(&logBrush);

		return logBrush.lbColor;
	}
};

#pragma endregion

extern COLORREF HLS_TRANSFORM(COLORREF rgb, int percent_L, int percent_S);

class CMyCommandBar : public CCommandBarCtrlImpl< CMyCommandBar >
{
public:
	CMyCommandBar(CColorPalette& palit) : m_palit(palit) {}

	//DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())
	DECLARE_WND_SUPERCLASS(_T("WTL_CommandBarXP"), GetWndClassName())

	// Message map and handlers
	typedef CMyCommandBar   thisClass;
	typedef CCommandBarCtrlImpl< CMyCommandBar >     baseClass;
	BEGIN_MSG_MAP(CCommandBarCtrlXP)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(1)      // Parent window messages
		NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnCustomDraw)
		CHAIN_MSG_MAP_ALT(baseClass, 1)
		ALT_MSG_MAP(2)		// Client window messages
		CHAIN_MSG_MAP_ALT(baseClass, 2)
		ALT_MSG_MAP(3)		// Message hook messages
		CHAIN_MSG_MAP_ALT(baseClass, 3)
	END_MSG_MAP()

	LRESULT OnCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
	{
		TCHAR sClass[128];

		GetClassName(pnmh->hwndFrom, sClass, 128);

		if (_tcscmp(sClass, _T("WTL_CommandBarXP")))
		{
			bHandled = FALSE;
			return CDRF_DODEFAULT;
		}

		NMCUSTOMDRAW* pCustomDraw = (NMCUSTOMDRAW*)pnmh;

		if (pCustomDraw->dwDrawStage == CDDS_PREPAINT)
		{
			// Request prepaint notifications for each item
			return CDRF_NOTIFYITEMDRAW;
		}
		if (pCustomDraw->dwDrawStage == CDDS_ITEMPREPAINT)
		{
			ATLTRACE2(atlTraceDBProvider, 3, _T("CMyCommandBar::OnCustomDraw(CDDS_ITEMPREPAINT) uItemState=x%X\n"), pCustomDraw->uItemState);

			CDCHandle cDC(pCustomDraw->hdc);
			CRect rc = pCustomDraw->rc;
			TCHAR sBtnText[128];

			::SendMessage(pnmh->hwndFrom, TB_GETBUTTONTEXT, pCustomDraw->dwItemSpec, (LPARAM)sBtnText);

			cDC.FillSolidRect(rc, (pCustomDraw->uItemState & CDIS_HOT) != 0 ? (COLORREF)m_palit.CrMenuBkg : (COLORREF)m_palit.CrBackGndAlt);
			//cDC.SetTextColor(::GetSysColor(m_bParentActive ? COLOR_BTNTEXT : COLOR_3DSHADOW));
			cDC.SetTextColor(m_bParentActive ? (COLORREF)m_palit.CrText : (COLORREF)m_palit.Cr3DShadow);

			cDC.SetBkMode(TRANSPARENT);
			cDC.SelectFont((HFONT)GetStockObject(DEFAULT_GUI_FONT));
			
			cDC.DrawText(sBtnText, _tcslen(sBtnText), rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

			return CDRF_SKIPDEFAULT;
		}

		return CDRF_DODEFAULT;
	}

#define IMGPADDING 6
#define TEXTPADDING 8

private:
	CColorPalette& m_palit;
};
