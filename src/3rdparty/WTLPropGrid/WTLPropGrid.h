#pragma once

typedef struct WPGDeepIterator_ WPGDeepIterator;
typedef struct WPGSiblingIterator_ WPGSiblingIterator;
typedef struct WPGPropertiesCtrl_ WPGPropertiesCtrl;
typedef struct WPGPropertyFeel_ WPGPropertyFeel;
typedef struct WPGPropertyLook_ WPGPropertyLook;
typedef struct WPGPropertyCheckboxLook_ WPGPropertyCheckboxLook;
typedef struct WPGPropertySpinCheckboxLook_ WPGPropertySpinCheckboxLook;

enum WPGPropValueType
{
	PVT_NONE,
	PVT_CSTRING,
	PVT_STLSTRING,
	PVT_BOOL,
	PVT_INT,
	PVT_UINT,
	PVT_DOUBLE,
	PVT_FLOAT,
	PVT_SHORT,
	PVT_USHORT,
	PVT_CHAR,
	PVT_ENUM,
	PVT_BITSET,
	PVT_COLORRGB,
};

// Constants used in calls to GetRegisteredFeel()
#define WPG_FEEL_EDIT			_T("edit")
#define WPG_FEEL_EDITSPIN		_T("editspin")
#define WPG_FEEL_EDITPASSWORD	_T("editpwd")
#define WPG_FEEL_LIST			_T("list")
#define WPG_FEEL_EDITLIST		_T("editlist")
#define WPG_FEEL_CUSTOMLIST		_T("customlist")
#define WPG_FEEL_BUTTON			_T("button")
#define WPG_FEEL_EDITBUTTON		_T("editbutton")
#define WPG_FEEL_POPUP			_T("popup")
#define WPG_FEEL_EDITPOPUP		_T("editpopup")
#define WPG_FEEL_FONTBUTTON		_T("fontbutton")
#define WPG_FEEL_DATETIME		_T("datetime")
#define WPG_FEEL_HOTKEY			_T("hotkey")
#define WPG_FEEL_CHECKBOX		_T("checkbox")
#define WPG_FEEL_CHECKBOXMULTI	_T("multicheckbox")
#define WPG_FEEL_CHECKBOXSPIN	_T("checkboxspin")
#define WPG_FEEL_RADIOBUTTON		_T("radiobutton")
#define WPG_FEEL_SLIDER			_T("slider")
#define WPG_FEEL_SLIDEREDIT		_T("slideredit")
#define WPG_FEEL_EDITMULTILINE	_T("multilineedit")

int GetID(WPGDeepIterator& iter);
class CColorPalette;
WPGPropertiesCtrl& MakeCtrlForPalit(CColorPalette& palit);
HWND CreateCtrl(WPGPropertiesCtrl& ctrl, HWND hWndParent, _U_RECT rcClient, LPCTSTR szWindowName, DWORD dwStyle, DWORD dwExStyle);
void SetCustomDrawManager(WPGPropertiesCtrl& ctrl, CColorPalette& palit);
void ShowComments(WPGPropertiesCtrl& ctrl, bool flag);
void ShowButtons(WPGPropertiesCtrl& ctrl, bool flag);
void SetCommentsHeight(WPGPropertiesCtrl& ctrl, int height);
void EnableItem(WPGPropertiesCtrl& ctrl, WPGDeepIterator& iter, bool flag);
void SetSecondColumnWidth(WPGPropertiesCtrl& ctrl, int height);

WPGDeepIterator& AddRootCategory(WPGPropertiesCtrl& ctrl, int id, LPCTSTR wszName);
WPGDeepIterator& AddPropertyUnderCategory(WPGPropertiesCtrl& ctrl, WPGDeepIterator& catIter, int id, LPCTSTR desc, WPGPropValueType valtyp, void* pv = nullptr, LPCTSTR comment = nullptr);
WPGDeepIterator& AddSubCategoryUnderCategory(WPGPropertiesCtrl& ctrl, WPGDeepIterator& catIter, int id, LPCTSTR desc);
WPGPropertyFeel& GetRegisteredFeel(WPGPropertiesCtrl& ctrl, LPCTSTR feelName);
void SelectIterItem(WPGPropertiesCtrl& ctrl, WPGDeepIterator& iter);
void SetFeel(WPGDeepIterator& iter, WPGPropertyFeel& feel);
void SetLookForValue(WPGDeepIterator& iter, WPGPropertyCheckboxLook& look);
void SetLookForValue(WPGDeepIterator& iter, WPGPropertySpinCheckboxLook& look);
void SetComment(WPGDeepIterator& iter, LPCTSTR text);
void SetStringValue(WPGDeepIterator& iter, LPCTSTR text);
void AddRestrictedValue(WPGDeepIterator& iter, LPCTSTR text, int value);
void* GetDataPtr(WPGDeepIterator& iter);
void* GetDataPtr(WPGSiblingIterator& iter);
WTL::CString GetName(WPGSiblingIterator& iter);

HWND GetHWnd(WPGPropertiesCtrl& ctrl);
HWND GetParent(WPGPropertiesCtrl& ctrl);
void SetCtrlFocus(WPGPropertiesCtrl& ctrl);
void RedrawGridWindow(WPGPropertiesCtrl& ctrl);
void DestroyCtrlWindow(WPGPropertiesCtrl& ctrl);
WPGDeepIterator& FindCtrlItem(WPGPropertiesCtrl& ctrl, int id);
COLORREF GetCtrlRefColor(WPGPropertiesCtrl& ctrl);
COLORREF GetCtrlBkgColorAlt(WPGPropertiesCtrl& ctrl);
void SetCtrlRefColor(WPGPropertiesCtrl& ctrl, COLORREF color, bool force);
WPGPropValueType GetValueType(WPGDeepIterator& iter);
COLORREF GetValueColor(WPGDeepIterator& iter);
void SetValueColor(WPGDeepIterator& iter, COLORREF color);

WPGPropertyCheckboxLook& MakePropertyCheckboxLook();
WPGPropertySpinCheckboxLook& MakePropertySpinCheckboxLook();

WPGSiblingIterator& GetCtrlCategoryItems(WPGPropertiesCtrl& ctrl, LPCTSTR catName);
void Advance(WPGPropertiesCtrl& ctrl, WPGSiblingIterator& iter);

#pragma comment(lib, "WTLPropGrid.lib")
