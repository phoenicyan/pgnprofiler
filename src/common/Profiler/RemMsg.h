/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#pragma once

// Remote host commands, and responces
#define MI_LISTPIPES_REQ			(1000)
#define MI_LISTPIPES_RESP			(1001)
#define MI_LISTPIPES_NTF			(1002)
#define MI_TRACELEVEL_REQ			(2000)
#define MI_PRMFORMAT_REQ			(2001)		// 0 (default) - truncated after 255 symbols, e.g. [DBTYPE_WSTR | DBTYPE_BYREF,"107200010001,10720003..."] 
												// 1 - not truncated, e.g. [DBTYPE_WSTR | DBTYPE_BYREF,"107200010001,107200010002,107200010003"] 
												// 2 - pgAdmin compatible, e.g. '107200010001,107200010002,107200010003'
#define MI_FAILURE_RESP				(-1)

// Command status/error code
#define MS_FAILED			(-1)
#define MS_OK				(0)

class RemMsg
{
public:
	WORD				m_msgID;
	bool				m_response;			// true for responses
	bool				m_bResetReadItr;
	std::vector<BYTE>	m_payload;
	std::vector<BYTE>::iterator m_readItr;
	DWORD				m_expectedLen;

	RemMsg() { m_msgID = 0; m_response = false; m_bResetReadItr = true; m_pBuff = NULL; m_expectedLen = 0; }
	
	RemMsg(WORD msgID, bool response = false)
	{
		m_msgID = msgID;
		m_response = response;
		m_bResetReadItr = true;
		m_pBuff = NULL;
		m_expectedLen = 0;
	}
	
	virtual ~RemMsg()
	{
		delete[] m_pBuff;
	}

	const BYTE* GetDataToSend(DWORD& totalLen);
	void SetFromReceivedData(BYTE* pData, DWORD dataLen); //returns whether all data received or not

	RemMsg& operator<<(LPCWSTR s);
	RemMsg& operator<<(bool b);
	RemMsg& operator<<(DWORD d);
	RemMsg& operator<<(__int64 d);
	RemMsg& operator<<(FILETIME d);

	RemMsg& operator>>(ATL::CString& s);
	RemMsg& operator>>(bool& b);
	RemMsg& operator>>(DWORD& d);
	RemMsg& operator>>(__int64& d);
	RemMsg& operator>>(FILETIME& d);

private:
	BYTE*	m_pBuff;

	RemMsg(const RemMsg&); //not implemented
	RemMsg& operator=(const RemMsg&); //not implemented
};
