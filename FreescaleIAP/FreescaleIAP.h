/*
 *  Freescale FTFA Flash Memory programmer
 *
 *  Sample usage:

#include "mbed.h"
#include "FreescaleIAP.h"
  
int main() 
{
    int address = flashSize() - SECTOR_SIZE;           //Write in last sector
    
    int *data = (int*)address;
    printf("Starting\r\n"); 
    erase_sector(address);
    int numbers[10] = {0, 1, 10, 100, 1000, 10000, 1000000, 10000000, 100000000, 1000000000};
    programFlash(address, (char*)&numbers, 40);        //10 integers of 4 bytes each: 40 bytes length
    while (true) ;
}

*/

#ifndef FREESCALEIAP_H
#define FREESCALEIAP_H
 
#include "mbed.h"
 
#ifdef TARGET_KLXX
#define SECTOR_SIZE     1024
#elif TARGET_K20D5M
#define SECTOR_SIZE     2048
#elif TARGET_K64F
#define SECTOR_SIZE     4096
#else
#define SECTOR_SIZE     1024
#endif

class FreescaleIAP
{
public:
    enum IAPCode {
        BoundaryError = -99,    // Commands may not span several sectors
        AlignError,             // Data must be aligned on longword (two LSBs zero)
        ProtectionError,        // Flash sector is protected
        AccessError,            // Something went wrong
        CollisionError,         // During writing something tried to flash which was written to
        LengthError,            // The length must be multiples of 4
        RuntimeError,           // FTFA runtime error reports
        EraseError,             // The flash was not erased before writing to it
        VerifyError,            // The data read back from flash didn't match what we wrote
        Success = 0
    };

    FreescaleIAP() { }
    ~FreescaleIAP() { }
 
    /** Program flash.  This erases the area to be written, then writes the data.
     *
     * @param address starting address where the data needs to be programmed (must be longword alligned: two LSBs must be zero)
     * @param data pointer to array with the data to program
     * @param length number of bytes to program (must be a multiple of 4)
     * @param return Success if no errors were encountered, otherwise one of the error states
     */
    IAPCode programFlash(int address, const void *data, unsigned int length);
     
    /**
     * Returns size of flash memory
     * 
     * This is the first address which is not flash
     *
     * @param return length of flash memory in bytes
     */
    uint32_t flashSize(void);
    
private:
    // program a word of flash
    IAPCode program_word(int address, const char *data);
    
    // verify that a flash area has been erased
    IAPCode verify_erased(int address, unsigned int length);
};
 
#endif
