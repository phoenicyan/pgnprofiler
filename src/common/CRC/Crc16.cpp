#include "crc16.h"

// This class provides functions for computing 16-bit CRC values.
// It implements both a slow bit-at-a-time method and a table-driven
// method.  The simple method was implemented as a way of verifying
// that the optimized table-method works.  


// POPULAR POLYNOMIALS

//  CCITT:	x^16 + x^12 + x^5 + x^0 (set m_Polynomial to 0x1021)
//
//	CRC-16:	x^16 + x^15 + x^2 + x^0 (set m_Polynomial to 0x8005)


// REFLECTED INPUT/CRC

//	Reflected input and reflected CRC is common with serial interfaces.
//  This is because the least-significant-bit is typically transmitted 
//  first with serial interfaces.


/* USAGE:
-----------------------------------------------------------------------
		#include "Crc16.h"

		char TestData[] = "Test Data abcdefghijklmnopqrstuvwxyz";

		void main( int argc, char *argv[] )
		{
			CCrc16	crc;

			USHORT	crc_1, crc_2;

			crc.m_Polynomial = 0x8005;	// Most commonly 0x8005 or 0x1021
			crc.m_Reflect = FALSE;		// use TRUE to reverse bits of input bytes
			crc.m_ReflectCrc = FALSE;	// use TRUE to reverse bits of output crc


			crc.m_CrcXorOutput = 0;		// use 0xFFFF to invert crc output
			crc.m_ResetValue = 0;		// Most commonly 0 or 0xFFFF

			crc_1 = crc.SimpleComputeBlock( TestData, strlen( TestData ) );

			crc.CreateTable()

			crc_2 = crc.ComputeCrcUsingTable( TestData, strlen( TestData ) );

			printf( "CRC by simple method: %04X\n", crc_1 );
			printf( "CRC using table:      %04X\n", crc_2 );
		}
-----------------------------------------------------------------------

*/



CCrc16::CCrc16()
{
	m_Polynomial = 0x1021;
	m_Reflect = TRUE;
	m_ReflectCrc = TRUE;
	m_CrcXorOutput = 0xFFFF;
	m_ResetValue = 0xFFFF;
	m_Crc = m_ResetValue;
}

USHORT CCrc16::ReverseBits( USHORT x )
{
	int i;
	USHORT newValue = 0;

	for( i=15 ; i>=0; i-- )
	{
		newValue |= (x & 1) << i;
		x >>= 1;
	}
	return newValue;
}

BYTE CCrc16::ReverseBits( BYTE b )
{
	int i;
	BYTE newValue = 0;

	for( i=7 ; i>=0; i-- )
	{
		newValue |= (b & 1) << i;
		b >>= 1;
	}
	return newValue;
}


void CCrc16::SimpleNextByte( BYTE b )
{
	int   i;

	if ( m_Reflect )
		b = ReverseBits( b );

	m_Crc ^= ( (USHORT) b << 8);

	for (i=0; i<8; i++)
	{
		if( m_Crc & 0x8000 )
			m_Crc = (m_Crc << 1) ^ m_Polynomial;
	    else
		   m_Crc <<= 1;
	}
}


USHORT CCrc16::SimpleComputeBlock( BYTE* data, int length )
{
	m_Crc = m_ResetValue;

	while (length--)
		SimpleNextByte( *(data++) );

	return SimpleGetCrc();
}

USHORT CCrc16::SimpleGetCrc()
{
	if ( m_ReflectCrc )
		return m_CrcXorOutput ^ ReverseBits( m_Crc );
	else
		return m_CrcXorOutput ^ m_Crc;
}

USHORT CCrc16::CreateTableEntry( int index )
{
	int   i;
	USHORT r;

	USHORT inbyte = (USHORT) index;

	if ( m_Reflect )
		inbyte = ReverseBits( (BYTE) inbyte );

	r = inbyte << 8;

	for (i=0; i<8; i++)
    if ( r & 0x8000 )
		r = (r << 1) ^ m_Polynomial;
    else
		r <<= 1;
	if ( m_Reflect )
		r = ReverseBits( r );

	return r;
}


void CCrc16::CreateTable()
{

	for( int i=0; i<256; i++ )
		m_Table[i] = CreateTableEntry( i );
}

USHORT CCrc16::ComputeCrcUsingTable(  BYTE* data, int length )
{
	int i;
	m_Crc = m_ResetValue;

	// Note: If using a table, m_ReflectCrc and m_Reflect
	// should match ( both TRUE or both FALSE ).  Otherwise
	// the CRC calculation will be incorrect.

//	ASSERT( (m_ReflectCrc && m_Reflect) ||
//				(!m_ReflectCrc && !m_Reflect));

	if( m_ReflectCrc )
	{
		for( i=0; i < length; i++ )
		{
			m_Crc = m_Table[ (m_Crc ^ *(data++)) & 0xFF ] ^ ( m_Crc >> 8 );
		}
	}
	else
	{
		for( i=0; i < length; i++ )
		{
			m_Crc = m_Table[ ((m_Crc >> 8 ) ^ *(data++)) & 0xFF ] ^ (m_Crc << 8 );
		}
	}

	m_Crc = m_CrcXorOutput ^ m_Crc;

	return m_Crc;
}
