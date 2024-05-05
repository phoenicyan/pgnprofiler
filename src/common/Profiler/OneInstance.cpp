/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "OneInstance.h"

COneInstance::COneInstance(const wchar_t* pstrName)
{
	m_hInstanceGuard = ::CreateMutexW(NULL, FALSE, pstrName);
}

COneInstance::~COneInstance()
{
	::CloseHandle(m_hInstanceGuard);
}

