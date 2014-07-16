/*
 *  Freescale FTFA Flash Memory programmer
 *
 *  Sample usage:

#include "mbed.h"
#include "FreescaleIAP.h"
  
int main() {
    int address = flash_size() - SECTOR_SIZE;           //Write in last sector
    
    int *data = (int*)address;
    printf("Starting\r\n"); 
    erase_sector(address);
    int numbers[10] = {0, 1, 10, 100, 1000, 10000, 1000000, 10000000, 100000000, 1000000000};
    program_flash(address, (char*)&numbers, 40);        //10 integers of 4 bytes each: 40 bytes length
    printf("Resulting flash: \r\n");
    for (int i = 0; i<10; i++)
        printf("%d\r\n", data[i]);
    
    printf("Done\r\n\n");
        
 
    while (true) {
    }
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
 
enum IAPCode {
    BoundaryError = -99,    //Commands may not span several sectors
    AlignError,             //Data must be aligned on longword (two LSBs zero)
    ProtectionError,        //Flash sector is protected
    AccessError,            //Something went wrong
    CollisionError,         //During writing something tried to flash which was written to
    LengthError,            //The length must be multiples of 4
    RuntimeError,           
    EraseError,             //The flash was not erased before writing to it
    Success = 0
    };
 
/** Erase a flash sector
 *
 * The size erased depends on the used device
 *
 * @param address address in the sector which needs to be erased
 * @param return Success if no errors were encountered, otherwise one of the error states
 */
IAPCode erase_sector(int address);
 
/** Program flash
 *
 * Before programming the used area needs to be erased. The erase state is checked
 * before programming, and will return an error if not erased.
 *
 * @param address starting address where the data needs to be programmed (must be longword alligned: two LSBs must be zero)
 * @param data pointer to array with the data to program
 * @param length number of bytes to program (must be a multiple of 4)
 * @param return Success if no errors were encountered, otherwise one of the error states
 */
IAPCode program_flash(int address, char *data, unsigned int length);
 
/**
 * Returns size of flash memory
 * 
 * This is the first address which is not flash
 *
 * @param return length of flash memory in bytes
 */
uint32_t flash_size(void);
 
#endif
