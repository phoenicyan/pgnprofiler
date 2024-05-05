// Crc16.h

#ifndef USHORT
#define USHORT	unsigned short
#endif

#ifndef BYTE
#define BYTE	unsigned char
#endif

#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef FALSE
#define FALSE	(0)
#endif

class CCrc16
{
public:
	CCrc16();
	USHORT	ReverseBits( USHORT x );
	BYTE	ReverseBits( BYTE b );
	void	SimpleNextByte( BYTE b );
	USHORT	SimpleComputeBlock( BYTE* data, int length );
	USHORT	SimpleGetCrc();
	USHORT	CreateTableEntry( int index );
	void	CreateTable();
	USHORT	ComputeCrcUsingTable(  BYTE* data, int length );

	USHORT	m_Crc;
	USHORT	m_ResetValue;
	USHORT	m_Polynomial;
	BOOL	m_Reflect;
	BOOL	m_ReflectCrc;
	USHORT	m_CrcXorOutput;
	USHORT	m_Table[256];
};