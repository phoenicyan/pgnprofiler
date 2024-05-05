/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "PGNPOptions.h"

COLORREF g_qclr_light[QCLR_SIZE] = {
	/*QCLR_FOCUS*/				RGB(41, 131, 225),
	/*QCLR_SELECT*/				White,
	/*QCLR_INSERT*/				Cyan,
	/*QCLR_UPDATE*/				Yellow,
	/*QCLR_DELETE*/				LightGray,
	/*QCLR_CREATEDROPALTER*/	0x64EACC,
	/*QCLR_PROCEDURES*/			Orange,
	/*QCLR_SYSSCHEMA*/			0xFFD5E4,
	/*QCLR_SYSTEM*/				0xFFE4D5,
	/*QCLR_ERROR*/				OrangeRed,
	/*QCLR_NOTIFY*/				0xE22E84,	// violet
	/*QCLR_NONE*/				0xFF80FF,
	/*QCLR_COMMENT*/			0x22B14C,	// green 34,177,76
};

COLORREF g_qclr_dark[QCLR_SIZE] = {
	/*QCLR_FOCUS*/				RGB(41, 131, 225),
	/*QCLR_SELECT*/				RGB(51, 51, 55),
	/*QCLR_INSERT*/				0x7F7F00,
	/*QCLR_UPDATE*/				0x006666,
	/*QCLR_DELETE*/				DarkSlateGray,
	/*QCLR_CREATEDROPALTER*/	0x12866D,
	/*QCLR_PROCEDURES*/			0x006399,
	/*QCLR_SYSSCHEMA*/			0x990027,
	/*QCLR_SYSTEM*/				0x7F2C00,
	/*QCLR_ERROR*/				0x00217F,
	/*QCLR_NOTIFY*/				0x740F40,	// violet
	/*QCLR_NONE*/				0xB700B7,
	/*QCLR_COMMENT*/			0x265911,	// green 34,177,76
};

CPGNPOptions::CPGNPOptions()
{
	Reset();
}

extern LVCOLUMN_EX SQLLOG_COLUMNS[];
extern int SQLLOG_COLUMNS_CNT;

void CPGNPOptions::Reset(int theme)
{
	m_llMaxLogFileSize = MAX_LOGFILE_SIZE;
	m_dwProfileAllApps = 0;
	m_dwDeleteWorkFiles = 1;
	//m_dwRefreshApps = 10;
	m_dwColorTheme = theme;
	m_dwParamFormat = PRMFMT_DEFAULT;
	m_dwConnectToHostEnabled = 0;
	m_sWorkPath.clear();
	m_sOptimizerConnectionString.clear();

	if (theme != CColorPalette::THEME_DARK)
	{
		m_qclrLight.clear();
		m_qclrLight.assign(g_qclr_light, g_qclr_light + QCLR_SIZE);
	}
	else
	{
		m_qclrDark.clear();
		m_qclrDark.assign(g_qclr_dark, g_qclr_dark + QCLR_SIZE);
	}

	m_visibleColumnsList.clear();
	m_remoteConnections.clear();

	WCHAR buf[32];
	for (int i = 0; i < SQLLOG_COLUMNS_CNT; i++)
	{
		wstring token(SQLLOG_COLUMNS[i].pszText);
		token += L':';
		token += _itow(SQLLOG_COLUMNS[i].cx, buf, 10);
		m_visibleColumnsList.push_back(token);
	}

	m_panel1FontHt = -12;
	m_DetailsFontHt = -12;
	m_ExplorerFontHt = -12;
	m_FilterFontHt = -12;
}

int CPGNPOptions::operator==(const CPGNPOptions& rhs) const
{
	int rez = 0;	// assume all categories are different

	if (m_dwProfileAllApps == rhs.m_dwProfileAllApps &&
		m_dwDeleteWorkFiles == rhs.m_dwDeleteWorkFiles &&
		//m_dwRefreshApps == rhs.m_dwRefreshApps &&
		m_dwColorTheme == rhs.m_dwColorTheme &&
		m_dwParamFormat == rhs.m_dwParamFormat &&
		m_sWorkPath == rhs.m_sWorkPath)
		rez |= EQUAL_CAT_GENERAL;

	if (m_qclrLight == rhs.m_qclrLight)
		rez |= EQUAL_CAT_QCLRLIGHT;

	if (m_qclrDark == rhs.m_qclrDark)
		rez |= EQUAL_CAT_QCLRDARK;

	if (wcscmp(m_visibleColumnsList.c_str(), rhs.m_visibleColumnsList.c_str()) == 0)
		rez |= EQUAL_CAT_COLUMNS;

	if (wcscmp(m_sOptimizerConnectionString.c_str(), rhs.m_sOptimizerConnectionString.c_str()) == 0)
		rez |= EQUAL_CAT_OPTIMIZER;

	if (wcscmp(m_remoteConnections.c_str(), rhs.m_remoteConnections.c_str()) == 0)
		rez |= EQUAL_CAT_REMOTE;

	return rez;
}

wstring CPGNPOptions::GetVisibleColumn(const wstring& name) const
{
	for (CVisibleColumnsList::const_iterator it = m_visibleColumnsList.begin(); it != m_visibleColumnsList.end(); it++)
	{
		if (0 == wcsncmp(it->c_str(), name.c_str(), name.length()))
			return *it;
	}
	return L"";	// not found
}

// Convert abbreviated string to number, e.g.:
// nK --> n'000
// nM --> n'000'000
// nG --> n'000'000'000
// nT --> n'000'000'000'000
// where n - is any number (such as 1234)
// Special case: input buffer contains '0' - return 0.
// Incorrect format or negative - return defaultNum.
// Tests: "" -> defaultNum, "0" -> 0, "1K" -> 1000, "360M" -> 360'000'000, "2.6G" -> 2'000'000'000, "3T" -> 3'000'000'000'000
LONGLONG CPGNPOptions::AbbreviatedToNumber(const WCHAR* buffer, LONGLONG defaultNum /*= MAX_LOGFILE_SIZE*/, LONGLONG minNum /*= MMFGROWSIZE*/)
{
	size_t len = wcslen(buffer);
	if (0 == len) return defaultNum;
	if (1 == len) return (L'0' == buffer[0]) ? 0 : defaultNum;

	const WCHAR suffix = buffer[len-1] & ~0x20;	// get uppercase char
	WCHAR multipliers[] = L"KMGT";
	WCHAR* szMultiplier = wcschr(multipliers, suffix);
	if (0 == szMultiplier) return defaultNum;

	LONGLONG multiplier = 1000;
	for (int i=0; i < (szMultiplier - multipliers); i++)
		multiplier *= 1000;

	LONGLONG n = _wtoi64(buffer);
	if (n <= 0) return defaultNum;

	n *= multiplier;

	return (n < minNum) ? minNum : n;
}
