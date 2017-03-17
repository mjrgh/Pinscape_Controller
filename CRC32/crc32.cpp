#include "crc32.h"

#define CRC32_POLYNOMIAL 0xEDB88320L
 
inline void CRC32Value(unsigned long &CRC, unsigned char c)
{
    /////////////////////////////////////////////////////////////////////////////////////
    //CRC must be initialized as zero 
    //c is a character from the sequence that is used to form the CRC
    //this code is a modification of the code from the Novatel OEM615 specification
    /////////////////////////////////////////////////////////////////////////////////////
    unsigned long ulTemp1 = (CRC >> 8) & 0x00FFFFFFL;
    unsigned long ulCRC = ((int)CRC ^ c) & 0xff ;
    for (int  j = 8 ; j > 0 ; j--)
    {
        if (ulCRC & 1)
        {
            ulCRC = (ulCRC >> 1) ^ CRC32_POLYNOMIAL;
        }
        else
        {
            ulCRC >>= 1;
        }
    }
    CRC = ulTemp1 ^ ulCRC;
} 
 
/* --------------------------------------------------------------------------
Calculates the CRC-32 of a block of data all at once
//the CRC is from the complete message (header plus data) 
//but excluding (of course) the CRC at the end
-------------------------------------------------------------------------- */
unsigned long CRC32(const void *data, int len)
{
    //////////////////////////////////////////////////////////////////////
    //the below code tests the CRC32Value procedure used in a markov form
    //////////////////////////////////////////////////////////////////////
    unsigned long CRC = 0;
    const unsigned char *p = (const unsigned char *)data;
    for (int i = 0 ; i < len ; i++)
        CRC32Value(CRC, *p++);

    return CRC;
}
 
/*
unsigned long CalculateBlockCRC32(
        unsigned long ulCount, 
        unsigned char *ucBuffer )
{
////////////////////////////////////////////
//original code from the OEM615 manual
////////////////////////////////////////////
    unsigned long ulTemp1;
    unsigned long ulTemp2;
    unsigned long ulCRC = 0;
    while ( ulCount-- != 0 )
    {
        ulTemp1 = ( ulCRC >> 8 ) & 0x00FFFFFFL;
        ulTemp2 = CRC32Value( ((int) ulCRC ^ *ucBuffer++ ) & 0xff );
        ulCRC = ulTemp1 ^ ulTemp2;
    }
    return( ulCRC );
}
*/