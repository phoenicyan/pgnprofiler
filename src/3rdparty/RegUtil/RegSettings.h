// RegSettings.h
//
// Copyright (c) 2001 Magomed Abdurakhmanov
// maq@hotbox.ru, http://mickels.iwt.ru/en
//
// No warranties are given. Use at your own risk.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <AtlMisc.h>
#include <AtlCtrls.h>

class CWindowSettings
{
public:
	WINDOWPLACEMENT m_WindowPlacement;

	CWindowSettings();
	void GetFrom(CWindow& Wnd);
	void ApplyTo(CWindow& Wnd, int nCmdShow = SW_SHOWNORMAL)const;

	bool Load(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey = HKEY_CURRENT_USER);
	bool Save(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey = HKEY_CURRENT_USER)const;
};

class CReBarSettings
{
public:
	struct BandInfo
	{
		DWORD ID;
		DWORD cx;
		bool BreakLine;
	}* m_pBands;

	DWORD m_cbBandCount;

	CReBarSettings();
	~CReBarSettings();

	void GetFrom(CReBarCtrl& ReBar);
	void ApplyTo(CReBarCtrl& ReBar)const;

	bool Load(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey = HKEY_CURRENT_USER);
	bool Save(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey = HKEY_CURRENT_USER) const;
};


class CSplitterSettings
{
public:
	DWORD m_dwPos;

	template <class T> void GetFrom(const T& Splitter)
	{
		m_dwPos = Splitter.GetSplitterPos();
	}

	template <class T> void ApplyTo(T& Splitter)const
	{
		Splitter.SetSplitterPos(m_dwPos);
	}

	bool Load(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey = HKEY_CURRENT_USER);
	bool Save(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey = HKEY_CURRENT_USER)const;
};
