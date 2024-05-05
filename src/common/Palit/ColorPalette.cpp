/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "ColorPalette.h"

#define IDB_SCROLLBAR_DARK              201
#define IDB_SCROLLBAR_LIGHT             202

#define BorderLeftWidth			2
#define BorderRightWidth		2
#define BorderTopHeight			2
#define BorderBottomHeight		2

CColorPalette::CColorPalette(THEME theme, const vector<COLORREF>& qclr)
	: m_theme(THEME_NONE)
	, m_hBmpScrollBarDark(NULL), m_hBmpScrollBarLight(NULL)
	, m_crText(RGB(0,0,0))
	, m_crTextAlt(RGB(0,0,0))
	, m_crBackGnd(RGB(255,255,255))
	, m_brBackGnd(0)
	, m_brBackGndAlt(0)
	, m_brBackGndSel(0)
	, m_hi(0)
	//, m_hMinBtnBmp(0)
	//, m_hMaxBtnBmp(0)
	//, m_hRestoreBtnBmp(0)
	//, m_hCloseBtnBmp(0)
	, CrText(this)
	, CrTextAlt(this)
	, CrBackGnd(this)
	, CrBackGndAlt(this)
	, CrBackGndSel(this)
	, Cr3DLight(this)
	, Cr3DShadow(this)
	, CrActiveBorder(this)
	, CrMenuBkg(this)
	, CrIconBkg(this)
	, CrMenuDT(this)
	, CrMenuSelectBkg(this)
{
	CBitmap bmpDark, bmpLight;
	bmpDark.LoadBitmap(IDB_SCROLLBAR_DARK);
	bmpLight.LoadBitmap(IDB_SCROLLBAR_LIGHT);
	
	BITMAP bm;
	bmpDark.GetBitmap(&bm);
	m_hBmpScrollBarDark = (HBITMAP)bmpDark.Detach();
	
	bmpLight.GetBitmap(&bm);
	m_hBmpScrollBarLight = (HBITMAP)bmpLight.Detach();
	
	LOGFONT lf = { 0 };
	lf.lfHeight = 14;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfQuality = DEFAULT_QUALITY;
	lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	wcscpy(lf.lfFaceName, L"Tahoma");

	m_titleFont.Attach(CreateFontIndirect(&lf));

	Set(theme, qclr);
}

CColorPalette::~CColorPalette()
{
	if (!m_titleFont.IsNull())
		m_titleFont.DeleteObject();
	//if (m_hMinBtnBmp)
	//	::DeleteObject(m_hMinBtnBmp);
	//if (m_hMaxBtnBmp)
	//	::DeleteObject(m_hMaxBtnBmp);
	//if (m_hRestoreBtnBmp)
	//	::DeleteObject(m_hRestoreBtnBmp);
	//if (m_hCloseBtnBmp)
	//	::DeleteObject(m_hCloseBtnBmp);
}

bool CColorPalette::Set(THEME theme, const vector<COLORREF>& qclr)
{
	if (theme == m_theme)
		return false;

	m_theme = theme;

	switch (theme)
	{
	default:
	case CColorPalette::THEME_DEFAULT:
		m_hBmpScrollBarActive = m_hBmpScrollBarLight;
		m_crText = ::GetSysColor(COLOR_WINDOWTEXT);
		m_crTextAlt = ::GetSysColor(COLOR_CAPTIONTEXT);
		m_crBackGnd = ::GetSysColor(COLOR_WINDOW);
		m_crBackGndAlt = ::GetSysColor(COLOR_BTNFACE);
		m_crBackGndSel = RGB(41, 131, 225);
		m_cr3DLight = ::GetSysColor(COLOR_3DLIGHT);
		m_cr3DShadow = ::GetSysColor(COLOR_3DSHADOW);
		m_crActiveBorder = ::GetSysColor(COLOR_ACTIVEBORDER);

		m_crMenuBkg = HLS_TRANSFORM(::GetSysColor(COLOR_3DFACE), -4, 60);
		m_crIconBkg = HLS_TRANSFORM(::GetSysColor(COLOR_3DFACE), +20, 60);
		m_crMenuDT = ::GetSysColor(COLOR_GRAYTEXT);
		m_crMenuSelectBkg = HLS_TRANSFORM(::GetSysColor(COLOR_HIGHLIGHT), +70, -57);

		if (m_brBackGnd.m_hBrush != NULL)
			m_brBackGnd.DeleteObject();
		m_brBackGnd = ::GetSysColorBrush(COLOR_WINDOW);
		
		if (m_brBackGndAlt.m_hBrush != NULL)
			m_brBackGndAlt.DeleteObject();
		m_brBackGndAlt = ::GetSysColorBrush(COLOR_BTNFACE);

		if (m_brBackGndSel.m_hBrush != NULL)
			m_brBackGndSel.DeleteObject();
		m_brBackGndSel.CreateSolidBrush(m_crBackGndSel);

		//m_crTrcNone = 0xFF80FF;
		//m_crTrcSelect = White;
		//m_crTrcInsert = Cyan;
		//m_crTrcUpdate = Yellow;
		//m_crTrcDelete = LightGray;
		//m_crTrcAlter = 0x64EACC;
		//m_crTrcInternalProc = Orange;
		//m_crTrcSysSchema = 0xFFD5E4;
		//m_crTrcSystem = 0xFFE4D5;
		//m_crTrcNotifies = 0xE22E84;	// violet
		//m_crTrcError = OrangeRed;
		//m_crTrcComment = 0x22B14C;	// green 34,177,76
		break;

	case CColorPalette::THEME_LIGHT:
		m_hBmpScrollBarActive = m_hBmpScrollBarLight;
		m_crText = Black;
		m_crTextAlt = Black;
		m_crBackGnd = White;
		m_crBackGndAlt = 0xF0F0F0;
		m_crBackGndSel = RGB(41, 131, 225);
		m_cr3DLight = 0xE3E3E3;
		m_cr3DShadow = 0xA0A0A0;
		m_crActiveBorder = 0xB4B4B4;

		m_crMenuBkg = RGB(215, 215, 244);
		m_crIconBkg = RGB(235, 235, 250);
		m_crMenuDT = Gray;
		m_crMenuSelectBkg = RGB(191, 213, 230);

		if (m_brBackGnd.m_hBrush != NULL)
			m_brBackGnd.DeleteObject();
		m_brBackGnd.CreateSolidBrush(m_crBackGnd);

		if (m_brBackGndAlt.m_hBrush != NULL)
			m_brBackGndAlt.DeleteObject();
		m_brBackGndAlt.CreateSolidBrush(m_crBackGndAlt);

		if (m_brBackGndSel.m_hBrush != NULL)
			m_brBackGndSel.DeleteObject();
		m_brBackGndSel.CreateSolidBrush(m_crBackGndSel);

		//m_crTrcNone = 0xFF80FF;
		//m_crTrcSelect = White;
		//m_crTrcInsert = Cyan;
		//m_crTrcUpdate = Yellow;
		//m_crTrcDelete = LightGray;
		//m_crTrcAlter = 0x64EACC;
		//m_crTrcInternalProc = Orange;
		//m_crTrcSysSchema = 0xFFD5E4;
		//m_crTrcSystem = 0xFFE4D5;
		//m_crTrcNotifies = 0xE22E84;	// violet
		//m_crTrcError = OrangeRed;
		//m_crTrcComment = 0x22B14C;	// green 34,177,76
		break;

	case CColorPalette::THEME_DARK:
		m_hBmpScrollBarActive = m_hBmpScrollBarDark;
		m_crText = RGB(250, 250, 255);
		m_crTextAlt = RGB(155, 155, 155);
		m_crBackGnd = RGB(37, 37, 38);
		m_crBackGndAlt = RGB(51, 51, 55);
		m_crBackGndSel = Black;
		m_cr3DLight = RGB(11, 103, 165);
		m_cr3DShadow = RGB(22, 83, 126);
		m_crActiveBorder = RGB(0, 122, 204);

		//m_crTitleBkg = ::GetSysColor(COLOR_HIGHLIGHT);
		//m_crTitleText = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
		m_crMenuBkg = RGB(27, 27, 28);
		m_crIconBkg = RGB(17, 17, 18);
		m_crMenuDT = LightSlateGray;
		m_crMenuSelectBkg = RGB(51, 51, 52);

		if (m_brBackGnd.m_hBrush != NULL)
			m_brBackGnd.DeleteObject();
		m_brBackGnd.CreateSolidBrush(m_crBackGnd);

		if (m_brBackGndAlt.m_hBrush != NULL)
			m_brBackGndAlt.DeleteObject();
		m_brBackGndAlt.CreateSolidBrush(m_crBackGndAlt);

		if (m_brBackGndSel.m_hBrush != NULL)
			m_brBackGndSel.DeleteObject();
		m_brBackGndSel.CreateSolidBrush(m_crBackGndSel);
		//m_brBackGndSel.CreateSolidBrush(Magenta);

		//m_crTrcNone = 0xB700B7;
		//m_crTrcSelect = m_crBackGndAlt;
		//m_crTrcInsert = 0x7F7F00;
		//m_crTrcUpdate = 0x006666;
		//m_crTrcDelete = DarkSlateGray;
		//m_crTrcAlter = 0x12866D;
		//m_crTrcInternalProc = 0x006399;
		//m_crTrcSysSchema = 0x990027;
		//m_crTrcSystem = 0x7F2C00;
		//m_crTrcNotifies = 0x740F40;	// violet
		//m_crTrcError = 0x00217F;
		//m_crTrcComment = 0x265911;	// green 34,177,76
		break;
	}

	if (&qclr != nullptr && qclr.size() != 0)
	{
		m_qclr.clear();
		m_qclr = qclr;
	}

	return true;
}

void CColorPalette::DrawButton(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	CDCHandle dc = lpDrawItemStruct->hDC;
	CRect rect = lpDrawItemStruct->rcItem;

	dc.FillSolidRect(&rect, m_crBackGndAlt);//Here you can define the required color to appear on the Button.
	dc.Draw3dRect(&rect, m_crTextAlt, m_crTextAlt);

	//UINT state = lpDrawItemStruct->itemState;  //This defines the state of the Push button either pressed or not. 
	//if ((state & ODS_SELECTED))
	//{
	//	dc.DrawEdge(&rect, EDGE_SUNKEN, BF_RECT);
	//}
	//else
	//{
	//	dc.DrawEdge(&rect, EDGE_RAISED, BF_RECT);
	//}

	dc.SetBkColor(m_crBackGndAlt);
	dc.SetTextColor(IsWindowEnabled(lpDrawItemStruct->hwndItem) ? m_crText : m_crTextAlt);

	//Get the Caption of Button Window 
	TCHAR buffer[MAX_PATH];
	ZeroMemory(buffer, MAX_PATH);
	::GetWindowText(lpDrawItemStruct->hwndItem, buffer, MAX_PATH);

	::DrawText(dc, buffer, _tcslen(buffer), &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

#define PAD1  (3)
#define PAD2  (8)

BOOL CColorPalette::DrawTitleButton(HWND hWnd, int index, BYTE state)
{
	if (state == 255)
		return FALSE;

	ATLTRACE2(atlTraceDBProvider, 3, "CColorPalette::DrawTitleButton(index=%d, state=%d)\n", index, state);

	CRect rect(GetButtonRect(hWnd, index % 10));

	const int cxFrame = GetSystemMetrics(SM_CXFRAME);
	const int cyFrame = GetSystemMetrics(SM_CYFRAME);
	const int cxBorder = GetSystemMetrics(SM_CXBORDER);
	const int cyBorder = GetSystemMetrics(SM_CYBORDER);
	const int bmHeight = GetSystemMetrics(SM_CYCAPTION) - 2 * cyBorder;

	CWindowDC dc(hWnd);

	if (state != 0)
	{
		dc.FillSolidRect(&rect, m_crBackGndSel);
	}

	CPen newpen;
	newpen.CreatePen(PS_SOLID, 1, m_crText);
	HPEN oldpen = dc.SelectPen(newpen);

	HBRUSH oldbrush = (HBRUSH)::SelectObject(dc, ::GetStockObject(NULL_BRUSH));

	LONG w = min(rect.Width(), rect.Height() - cyFrame);

	// center image
	LONG dx = (rect.Width() - w + PAD1) / 2;
	rect.left += dx;
	rect.top += cyBorder + cyFrame;

	if (index == 0)			// close button
	{
		dc.MoveTo(rect.left + PAD1, rect.top + PAD1);
		dc.LineTo(rect.left + w - PAD2, rect.top + w - PAD2);
		dc.LineTo(rect.left + PAD1, rect.top + PAD1);

		dc.MoveTo(rect.left + PAD1, rect.top + w - PAD2);
		dc.LineTo(rect.left + w - PAD2, rect.top + PAD1);
		dc.LineTo(rect.left + PAD1, rect.top + w - PAD2);
	}
	else if (index == 1)	// max button
	{
		dc.MoveTo(rect.left + PAD1, rect.top + PAD1);
		dc.LineTo(rect.left + w - PAD2, rect.top + PAD1);
		dc.LineTo(rect.left + w - PAD2, rect.top + w - PAD2);
		dc.LineTo(rect.left + PAD1, rect.top + w - PAD2);
		dc.LineTo(rect.left + PAD1, rect.top + PAD1);
	}
	else if (index == 11)	// restore button
	{
		dc.MoveTo(rect.left + PAD1, rect.top + PAD1 + 3);
		dc.LineTo(rect.left + w - PAD2 - 3, rect.top + PAD1 + 3);
		dc.LineTo(rect.left + w - PAD2 - 3, rect.top + w - PAD2);
		dc.LineTo(rect.left + PAD1, rect.top + w - PAD2);
		dc.LineTo(rect.left + PAD1, rect.top + PAD1 + 3);

		dc.MoveTo(rect.left + PAD1 + 3, rect.top + PAD1);
		dc.LineTo(rect.left + w - PAD2, rect.top + PAD1);
		dc.LineTo(rect.left + w - PAD2, rect.top + w - PAD2 - 3);
	}
	else if (index == 2)	// min button
	{
		dc.MoveTo(rect.left + PAD1, rect.top + (w/2));
		dc.LineTo(rect.left + w - PAD2, rect.top + (w/2));
	}

	dc.SelectBrush(oldbrush);
	dc.SelectPen(oldpen);

	return TRUE;
}

// Transforms an RGB colour by increasing or reducing its luminance and/or saturation in HLS space.
COLORREF CColorPalette::HLS_TRANSFORM(COLORREF rgb, int percent_L, int percent_S)
{
	WORD h, l, s;
	ColorRGBToHLS(rgb, &h, &l, &s);
	if (percent_L > 0)
	{
		l = WORD(l + ((240 - l) * percent_L) / 100);
	}
	else if (percent_L < 0)
	{
		l = WORD((l * (100 + percent_L)) / 100);
	}
	if (percent_S > 0)
	{
		s = WORD(s + ((240 - s) * percent_S) / 100);
	}
	else if (percent_S < 0)
	{
		s = WORD((s * (100 + percent_S)) / 100);
	}
	if (l > 240) l = 240; if (l < 0) l = 0;
	if (s > 240) s = 240; if (s < 0) s = 0;
	return ColorHLSToRGB(h, l, s);
}

void CColorPalette::DrawFrame(HWND hWnd, int width, int height, BYTE* btnState, BOOL isActive, BOOL isMaximized)
{
	ATLTRACE2(atlTraceDBProvider, 3, "CColorPalette::DrawFrame(w=%d, h=%d, active=%x, maximized=%d)\n", width, height, isActive, isMaximized);

	const int cxFrame = 4;// GetSystemMetrics(SM_CXFRAME);
	const int cyFrame = 4;//GetSystemMetrics(SM_CYFRAME);
	const int cxBorder = GetSystemMetrics(SM_CXBORDER) + cxFrame;
	const int cyBorder = GetSystemMetrics(SM_CYBORDER) + cyFrame;
	const int bmHeight = GetSystemMetrics(SM_CYCAPTION);

	CWindowDC dc(hWnd);
	//DrawLeft(hDc, 0, 0, height);
	//DrawRight(hDc, width - BorderRightWidth, 0, height);
	//DrawBottom(hDc, BorderLeftWidth, height - BorderBottomHeight, width - BorderRightWidth - BorderLeftWidth);
	//DrawTitle(hWnd, btnState, BorderLeftWidth, 0, width - BorderRightWidth - BorderLeftWidth + 1, isMaximized);

	// draw left
	CRect rect(0, 0, cxFrame + 2, height);
	dc.FillSolidRect(&rect, m_crBackGndAlt);

	// draw right
	rect = CRect(width - cxFrame - 2, 0, width, height);
	dc.FillSolidRect(&rect, m_crBackGndAlt);

	// draw bottom
	rect = CRect(0, height - cyFrame - 2, width, height);
	dc.FillSolidRect(&rect, m_crBackGndAlt);

	DrawTitle(hWnd, btnState, width, isMaximized);

	if (isActive)
	{
		CPen newpen;
		newpen.CreatePen(PS_SOLID, 1, m_crActiveBorder);
		HPEN oldpen = dc.SelectPen(newpen);

		HBRUSH oldbrush = (HBRUSH)::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
		dc.Rectangle(0, 0, width, height);
		dc.SelectBrush(oldbrush);
		dc.SelectPen(oldpen);
	}
}

// from resource.h
#define IDB_MIN                         230
#define IDB_MAX                         231
#define IDB_RESTORE                     232
#define IDB_CLOSE                       233

void CColorPalette::DrawTitle(HWND hWnd, BYTE* btnState, int width, BOOL isMaximized)
{
	const int cxFrame = 4;//GetSystemMetrics(SM_CXFRAME);
	const int cyFrame = 4;//GetSystemMetrics(SM_CYFRAME);
	const int cxBorder = GetSystemMetrics(SM_CXBORDER) + cxFrame;
	const int cyBorder = GetSystemMetrics(SM_CYBORDER) + cyFrame;
	const int bmHeight = GetSystemMetrics(SM_CYCAPTION);

	CWindowDC dc(hWnd);
	CRect rect(0, 0, width, cyBorder + bmHeight);

	dc.FillSolidRect(&rect, m_crBackGndAlt);

	LONG hOffset = 6;

	// draw icon
	if (btnState[3] != 255)
	{
		int cx = GetSystemMetrics(SM_CXSMICON);
		int cy = GetSystemMetrics(SM_CYSMICON);
		if (!m_hi)
			m_hi = (HICON)SendMessage(hWnd, WM_GETICON, ICON_SMALL, 0);
		if (!m_hi)
			m_hi = LoadIcon(_Module.GetResourceInstance(), MAKEINTRESOURCE(128/*IDR_MAINFRAME*/));
		if (m_hi)
		{
			dc.DrawIconEx(8, cyBorder + cyFrame, m_hi/*(HICON)CopyImage( hi, IMAGE_ICON, cx, cy, 0)*/, cx, cy, 0, 0, DI_NORMAL);
			hOffset = 30;
		}
	}

	//draw text
	TCHAR windowText[MAX_PATH] = { 0 };
	int len = ::GetWindowText(hWnd, windowText, _countof(windowText));

	CRect textRect(hOffset, cyBorder, width - 80, cyBorder + cyFrame + bmHeight);

	dc.SetBkMode(TRANSPARENT);
	HFONT hOldFont = (HFONT)::SelectObject(dc, m_titleFont);
	dc.SetTextColor(m_crText);
	dc.DrawText(windowText, len, &textRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_WORD_ELLIPSIS);

	DrawTitleButton(hWnd, 2, btnState[2]);
	DrawTitleButton(hWnd, isMaximized ? 11 : 1, btnState[1]);
	DrawTitleButton(hWnd, 0, btnState[0]);
}

//void CColorPalette::DrawLeft(HDC hDc, int x, int y, int height)
//{
//	CDCHandle dc = hDc;
//	CRect rect(x, y, x + BorderLeftWidth, y + height);
//
//	dc.FillSolidRect(&rect, m_crBackGndAlt);
//}
//
//void CColorPalette::DrawRight(HDC hDc, int x, int y, int height)
//{
//	CDCHandle dc = hDc;
//	CRect rect(x, y, x + BorderRightWidth, y + height);
//
//	dc.FillSolidRect(&rect, m_crBackGndAlt);
//}
//
//void CColorPalette::DrawBottom(HDC hDc, int x, int y, int width)
//{
//	CDCHandle dc = hDc;
//	CRect rect(x, y, x + width, y + BorderBottomHeight);
//
//	dc.FillSolidRect(&rect, m_crBackGndAlt);
//}

RECT CColorPalette::GetButtonRect(HWND hWnd, int index) {
	RECT WindowRect = { 0 }, rect = { 0 };
	::GetWindowRect(hWnd, &WindowRect);

	const int cxFrame = 4;//GetSystemMetrics(SM_CXFRAME);
	const int cyFrame = 4;//GetSystemMetrics(SM_CYFRAME);
	const int cxBorder = GetSystemMetrics(SM_CXBORDER);
	const int cyBorder = GetSystemMetrics(SM_CYBORDER);
	const int bmHeight = GetSystemMetrics(SM_CYCAPTION) - 2 * cyBorder;
	const int bmWidth = (3 * bmHeight / 2) + 3;

	if (index < 3)			
	{
		// 0 - close button rect
		// 1 - max button rect
		// 2 - min button rect
		const RECT maxBtnRect = { cxBorder + index * bmWidth + 1,		cyBorder, cxBorder + (index + 1) * bmWidth, cyBorder + cyFrame + bmHeight };
		rect = maxBtnRect;
		rect.left = WindowRect.right - WindowRect.left - maxBtnRect.right;
		rect.right = WindowRect.right - WindowRect.left - maxBtnRect.left;
	}
	else if (index == 3)
	{
		// sysmenu button rect
		const RECT sysBtnRect = { cxBorder,	cyBorder, cxBorder + bmHeight, cyBorder + cyFrame + bmHeight };
		rect = sysBtnRect;
	}

	return rect;
}
