/*************************************************************************
 ***                                                                   ***
 ***                  Author: Konstantin Izmailov                      ***
 ***                  kizmailov@gmail.com                              ***
 ***                                                                   ***
 *************************************************************************/

#include "stdafx.h"
#include "RemMsg.h"

RemMsg& RemMsg::operator<<(LPCWSTR s)
{
	*this << (DWORD)wcslen(s);	// length in wchar_t
	m_payload.insert(m_payload.end(), (BYTE*)s, (BYTE*)(s + wcslen(s)));
	return *this;
}

RemMsg& RemMsg::operator<<(bool b)
{
	m_payload.insert(m_payload.end(), (BYTE*)&b, ((BYTE*)&b) + sizeof(b));
	return *this;
}

RemMsg& RemMsg::operator<<(DWORD d)
{
	m_payload.insert(m_payload.end(), (BYTE*)&d, ((BYTE*)&d) + sizeof(d));
	return *this;
}

RemMsg& RemMsg::operator<<(__int64 d)
{
	m_payload.insert(m_payload.end(), (BYTE*)&d, ((BYTE*)&d) + sizeof(d));
	return *this;
}

RemMsg& RemMsg::operator<<(FILETIME d)
{
	m_payload.insert(m_payload.end(), (BYTE*)&d, ((BYTE*)&d) + sizeof(d));
	return *this;
}

RemMsg& RemMsg::operator>>(ATL::CString& s)
{
	if (m_bResetReadItr)
	{
		m_readItr = m_payload.begin();
		m_bResetReadItr = false;
	}

	s.Empty();
	DWORD len = 0;
	*this >> len; //this is len in wchar_t
	if (0 != len)
	{
		if (m_readItr >= m_payload.end())
			return *this;

		LPWSTR pStart = s.GetBuffer(len);
		memcpy(pStart, &(*m_readItr), len * sizeof(wchar_t));
		s.ReleaseBuffer(len);
		m_readItr += len * sizeof(wchar_t);
	}

	return *this;
}

RemMsg& RemMsg::operator>>(bool& b)
{
	if (m_bResetReadItr)
	{
		m_readItr = m_payload.begin();
		m_bResetReadItr = false;
	}

	if (m_readItr >= m_payload.end())
		return *this;

	memcpy(&b, &(*m_readItr), sizeof(b));
	m_readItr += sizeof(b);

	return *this;

}

RemMsg& RemMsg::operator>>(DWORD& d)
{
	if (m_bResetReadItr)
	{
		m_readItr = m_payload.begin();
		m_bResetReadItr = false;
	}

	if (m_readItr >= m_payload.end())
		return *this;

	memcpy(&d, &(*m_readItr), sizeof(d));
	m_readItr += sizeof(d);

	return *this;
}

RemMsg& RemMsg::operator>>(__int64& d)
{
	if (m_bResetReadItr)
	{
		m_readItr = m_payload.begin();
		m_bResetReadItr = false;
	}

	if (m_readItr >= m_payload.end())
		return *this;

	memcpy(&d, &(*m_readItr), sizeof(d));
	m_readItr += sizeof(d);

	return *this;
}


RemMsg& RemMsg::operator>>(FILETIME& d)
{
	if (m_bResetReadItr)
	{
		m_readItr = m_payload.begin();
		m_bResetReadItr = false;
	}

	if (m_readItr >= m_payload.end())
		return *this;

	memcpy(&d, &(*m_readItr), sizeof(d));
	m_readItr += sizeof(d);

	return *this;
}

const BYTE* RemMsg::GetDataToSend(DWORD& totalLen)
{
	if (NULL != m_pBuff)
		delete[] m_pBuff;

	//build our send buffer
	DWORD len = (DWORD)m_payload.size();
	totalLen = sizeof(m_msgID) + sizeof(len) + len;

	BYTE* pPtr = m_pBuff = new BYTE[totalLen];

	memcpy(pPtr, &m_msgID, sizeof(m_msgID));
	pPtr += sizeof(m_msgID);

	memcpy(pPtr, &len, sizeof(len));
	pPtr += sizeof(len);

	if (0 != len)
	{
		memcpy(pPtr, &m_payload[0], m_payload.size());
		//pPtr += m_payload.size();
	}

	return m_pBuff;
}

void RemMsg::SetFromReceivedData(BYTE* pData, DWORD dataLen)
{
	if (dataLen >= sizeof(m_msgID))
	{
		memcpy(&m_msgID, pData, sizeof(m_msgID));
		pData += sizeof(m_msgID);
		dataLen -= sizeof(m_msgID);
	}

	m_expectedLen = 0;
	if (dataLen >= sizeof(m_expectedLen))
	{
		memcpy(&m_expectedLen, pData, sizeof(m_expectedLen));
		pData += sizeof(m_expectedLen);
		dataLen -= sizeof(m_expectedLen);
	}

	m_payload.clear();
	if (0 != dataLen)
		m_payload.insert(m_payload.end(), pData, pData + dataLen);

	m_bResetReadItr = true;
}
