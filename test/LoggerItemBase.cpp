/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "LoggerItemBase.h"
#include "Profiler\ProfilerDef.h"

extern "C" __declspec(dllimport) int __stdcall SHCreateDirectory(HWND hwnd, LPCWSTR pszPath);

#if USE_PARF
#define THRESHOLD_4PARF	1000	// defines threshold of when to start parallel filtering
#endif

CLoggerItemBase::CLoggerItemBase() : CLoggerItemBase(L"", nullptr)
{}

CLoggerItemBase::CLoggerItemBase(LPCTSTR pstrName, CLoggerItemBase* pParent)
	: m_name(pstrName), m_pParent(pParent), m_initialTime(), m_filterActive(false)
	, m_bROmode(false), m_psWorkPath(0), m_dwTraceLevel(0)
{}

CLoggerItemBase::~CLoggerItemBase()
{
	DeleteChildren();
}

int CLoggerItemBase::OpenLogFile(DWORD dwReserved)
{
	return 0;
}

void CLoggerItemBase::CloseLogFile()
{
}

map<DWORD, wstring>* CLoggerItemBase::GetLogger2AppnameMap()
{
	return nullptr;
}

BOOL CLoggerItemBase::DeleteChildren()
{
	for (list<CLoggerItemBase*>::iterator it = m_children.begin(); it != m_children.end(); it++)
	{
		//CLoggerTracker::Instance().RemoveLogger(*it);

		delete (*it);
	}
	return TRUE;
}

BOOL CLoggerItemBase::AddChild(CLoggerItemBase* pBlock)
{
	//CLoggerTracker::Instance().AddLogger(pBlock);

	m_children.push_back(pBlock);

	return TRUE;
}

void CLoggerItemBase::SetFilterActive(bool isActive, std::function<void(int)> fnProgressCallback)
{
	ATLTRACE2(atlTraceDBProvider, 0, L"CLoggerItemBase::SetFilterActive(%s)\n", isActive ? L"true" : L"false");
}
