/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "LoggerItemBase.h"

class CFileLoggerItem : public CLoggerItemBase
{
public:
	// data members
	HANDLE m_hLogFile, m_hMapping;
	BYTE* m_logStart;

	vector<LONG> m_rgMessageOffsets;
	vector<LONG> m_rgFilteredMessages;
	size_t m_errorCnt;

	wstring m_path;

	map<DWORD, wstring> m_mapLogger2Appname;

	// ctor/dtor
	CFileLoggerItem(LPCTSTR pFileName, LPCTSTR pPath);

	~CFileLoggerItem();

	// methods
	int OpenLogFile(DWORD dwReserved = 0) override;

	void CloseLogFile() override;

	virtual map<DWORD, wstring>* GetLogger2AppnameMap();

	size_t GetNumErrors() const override;

	size_t GetNumMessages() const override;

	const BYTE* GetMessageData(size_t i) const override;

	void FilterLoggerItems() override;

	void ClearLog() override;

//protected:
//	void ApplyFilter(const rethread::cancellation_token& loc_ct);
};
