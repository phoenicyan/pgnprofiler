/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

class COneInstance
{
public:
	virtual ~COneInstance();

	COneInstance(const wchar_t* appName);
	HANDLE  m_hInstanceGuard;
};

