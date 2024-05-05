#pragma once

//
// Will calculate 8 bit CRC as of X8 + X5 + X4 + 1
//

class crc8 
{
public:
  crc8( void );
    ~crc8();
    void init_crc8( void );
    unsigned char return_crc8( void );
    void calc_crc8( unsigned char byte_in );
    int return_byte_count( void );

private:
    unsigned char m_crc;
    int m_byte_count;
};
