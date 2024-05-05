#pragma once

#include <windows.h>

// Class to perform 32 bit CRC checksumming.
class ClsCrc32
{
private:
	DWORD		m_dwCRC;

public:
	// Implementation
				ClsCrc32(void):m_dwCRC(0xFFFFFFFF)  {};

	DWORD		Crc( LPBYTE pData, DWORD dwLength );
	void		CrcAdd( LPBYTE pData, DWORD dwLength );
	
	void		CrcReset() { m_dwCRC = 0xFFFFFFFF; }
	
	DWORD		GetCrc(void) { return m_dwCRC ^ 0xFFFFFFFF; };

protected:
	// Data
	static DWORD	s_dwCrc32[ 256 ];
};
