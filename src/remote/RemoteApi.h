/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

#include "RemMsg.h"

#define LOCAL_DEBUG		0		// 1-allows debugging on local computer with '-service' parameter; 0-launches remote service and copies .pdb file for debugging

class CRemoteApi
{
	ATL::CString _lpszRemote;
	ATL::CString _lpszUser;
	ATL::CString _lpszPassword;
	
	bool _bNeedToDeleteServiceFile;
	bool _bNeedToDeleteService;
	bool _bNeedToDetachFromAdmin;
	bool _bNeedToDetachFromIPC;

	HANDLE _hPipe;

	TCHAR _lpszRemoteServiceName[_MAX_PATH];

public:
	enum NETRESOURCETYPE
	{
		NET_IPC,		// IPC$
		NET_ADMIN,		// ADMIN$
	};

	CRemoteApi(LPCTSTR lpszRemote, LPCTSTR lpszUser = nullptr, LPCTSTR lpszPassword = nullptr);

	~CRemoteApi();

	bool Connect(NETRESOURCETYPE rs);

	bool Disconnect(NETRESOURCETYPE rs);

	inline LPCTSTR GetRemoteServiceName() const
	{
		return _lpszRemoteServiceName;
	}

	inline HANDLE GetPipe() const { return _hPipe; }

	void DeleteFromRemote();

	bool CopyToRemote();

	void StopAndDeleteRemoteService();

	bool InstallAndStartRemoteService();

	bool GetProcesses(ATL::CString& processes);

protected:
	bool SendRequest(RemMsg& msgOut, RemMsg& msgReturned);
};
