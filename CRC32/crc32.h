 
#ifndef CRC32_H
#define CRC32_H
 
void CRC32Value(unsigned long &CRC, unsigned char c);
unsigned long CRC32(const void *data, int len);
 
#endif
 
 