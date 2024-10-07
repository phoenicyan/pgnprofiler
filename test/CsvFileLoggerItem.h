/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "LoggerItemBase.h"

class CCsvFileLoggerItem : public CLoggerItemBase
{
public:
	// data members
	vector<BYTE*> m_rgMessages;
	vector<BYTE*> m_rgFilteredMessages;
	size_t m_errorCnt;

	wstring m_path;

	map<DWORD, wstring> m_mapLogger2Appname;

	// ctor/dtor
	CCsvFileLoggerItem(LPCTSTR pFileName, LPCTSTR pPath);

	~CCsvFileLoggerItem();

	virtual int OpenLogFile();

	virtual map<DWORD, wstring>* GetLogger2AppnameMap();

	size_t GetNumErrors() const override;

	size_t GetNumMessages() const override;

	const BYTE* GetMessageData(size_t i) const override;

	void FilterLoggerItems() override;

	void ClearLog() override;
};
