// FreescaleIAP, private version
//
// This is a heavily modified version of Erik Olieman's FreescaleIAP, a
// flash memory writer for Freescale boards.  This version is adapted to
// the special needs of the KL25Z.
//
// Simplifications:
//
// Unlike EO's original version, this version combines erase and write
// into a single opreation, so the caller can simply give us a buffer
// and a location, and we'll write it, including the erase prep.  We
// don't need to be able to separate the operations, so the combined
// interface is simpler at the API level and also lets us do all of the
// interrupt masking in one place (see below).
//
// Stability improvements:
//
// The KL25Z has an important restriction on flash writing that makes it
// very delicate.  Specifically, the flash controller (FTFA) doesn't allow 
// any read operations while a sector erase is in progress.  This complicates
// things for a KL25Z app because all program code is stored in flash by 
// default.  This means that every instruction fetch is a flash read.  The
// FTFA's response to a read while an erase is in progress is to fail the
// read.  When the read is actually an instruction fetch, this results in
// CPU lockup.  Making this even more complicated, the erase operation can
// only operate on a whole sector at a time, which takes on the order of 
// milliseconds, which is a very long time for the CPU to go without any
// instruction fetches.  Even if the code that initiates the erase is 
// located in RAM and is very careful to loop within the RAM code block,
// any interrupt could take us out of the RAM loop and trigger a fetch
// on a flash location.
//
// We use two strategies to avoid flash fetches while we're working.
// First, the code that performs all of the FTFA operations is written
// in assembly, in a module AREA marked READWRITE.  This forces the
// linker to put the code in RAM.  The code could otherwise just have
// well been written in C++, but as far as I know there's no way to tell
// the mbed C++ compiler to put code in RAM.  Since the FTFA code is all
// in RAM, it doesn't by itself trigger any flash fetches as it executes,
// so we're left with interrupts as the only concern.  Second, we explicitly 
// disable all of the peripheral interrupts that we use anywhere in the 
// program (USB, all the timers, GPIO ports, etc) via the NVIC.  From
// testing, it's clear that disabling interrupts at the CPU level via
// __disable_irq() (or the equivalent assembly instruction CPSID I) isn't
// enough.  We have to turn interrupts off at the peripheral (NVIC) level.
// I'm really not sure why this is required, since you'd think the CPSID I
// masking would be enough, but experimentally it's clearly not.  This is
// a detail of ARM hardware architecture that I need to look into more,
// since it leaves me uneasy that there might be even more subtleties 
// left to uncover.   But at least things seem very stable after blocking
// interrupts at the NVIC level.

#include "FreescaleIAP.h"
 
//#define IAPDEBUG

// assembly interface
extern "C" {
    void iapEraseSector(FTFA_Type *ftfa, uint32_t address);
    void iapProgramBlock(FTFA_Type *ftfa, uint32_t address, const void *src, uint32_t length);
}


 
enum FCMD {
    Read1s = 0x01,
    ProgramCheck = 0x02,
    ReadResource = 0x03,
    ProgramLongword = 0x06,
    EraseSector = 0x09,
    Read1sBlock = 0x40,
    ReadOnce = 0x41,
    ProgramOnce = 0x43,
    EraseAll = 0x44,
    VerifyBackdoor = 0x45
};


/* Check if an error occured 
   Returns error code or Success*/
static IAPCode check_error(void) 
{
    if (FTFA->FSTAT & FTFA_FSTAT_FPVIOL_MASK) {
        #ifdef IAPDEBUG
        printf("IAP: Protection violation\r\n");
        #endif
        return ProtectionError;
    }
    if (FTFA->FSTAT & FTFA_FSTAT_ACCERR_MASK) {
        #ifdef IAPDEBUG
        printf("IAP: Flash access error\r\n");
        #endif
        return AccessError;
    }
    if (FTFA->FSTAT & FTFA_FSTAT_RDCOLERR_MASK) {
        #ifdef IAPDEBUG
        printf("IAP: Collision error\r\n");
        #endif
        return CollisionError;
    }
    if (FTFA->FSTAT & FTFA_FSTAT_MGSTAT0_MASK) {
        #ifdef IAPDEBUG
        printf("IAP: Runtime error\r\n");
        #endif
        return RuntimeError;
    }
    #ifdef IAPDEBUG
    printf("IAP: No error reported\r\n");
    #endif
    return Success;
}
 
IAPCode FreescaleIAP::program_flash(int address, const void *src, unsigned int length) 
{    
    #ifdef IAPDEBUG
    printf("IAP: Programming flash at %x with length %d\r\n", address, length);
    #endif
                
    // presume success
    IAPCode status = Success;

    // I'm not 100% convinced this is 100% reliable yet.  So let's show
    // some diagnostic lights while we're working.  If anyone sees any
    // freezes, the lights that are left on at the freeze will tell us
    // which step is crashing.
    extern void diagLED(int,int,int);
    
    // Erase the sector(s) covered by the write.  Before writing, we must
    // erase each sector that we're going to touch on the write.
    for (uint32_t ofs = 0 ; ofs < length ; ofs += SECTOR_SIZE)
    {
        // Show RED on the first sector, GREEN on second, BLUE on third.  Each
        // sector is 1K, so I don't think we'll need more than 3 for the 
        // foreseeable future.  (RAM on the KL25Z is so tight that it will
        // probably stop us from adding enough features to require more
        // configuration variables than 3K worth.)
        diagLED(ofs/SECTOR_SIZE == 0, ofs/SECTOR_SIZE == 1, ofs/SECTOR_SIZE == 2);
        
        // erase the sector
        iapEraseSector(FTFA, address + ofs);
    }
        
    // If the erase was successful, write the data.
    if ((status = check_error()) == Success)
    {
        // show cyan while the write is in progress
        diagLED(0, 1, 1);

        // do the write
        iapProgramBlock(FTFA, address, src, length);
        
        // purple when done
        diagLED(1, 0, 1);
        
        // check again for errors
        status = check_error();
    }
    
    // return the result
    return status;
}
 
uint32_t FreescaleIAP::flash_size(void) 
{
    uint32_t retval = (SIM->FCFG2 & 0x7F000000u) >> (24-13);
    if (SIM->FCFG2 & (1<<23))           // Possible second flash bank
        retval += (SIM->FCFG2 & 0x007F0000u) >> (16-13);
    return retval;
}
 
/* Check if no flash boundary is violated
   Returns true on violation */
bool check_boundary(int address, unsigned int length) 
{
    int temp = (address+length - 1) / SECTOR_SIZE;
    address /= SECTOR_SIZE;
    bool retval = (address != temp);
    #ifdef IAPDEBUG
    if (retval)
        printf("IAP: Boundary violation\r\n");
    #endif
    return retval;
}
 
/* Check if address is correctly aligned
   Returns true on violation */
bool check_align(int address) 
{
    bool retval = address & 0x03;
    #ifdef IAPDEBUG
    if (retval)
        printf("IAP: Alignment violation\r\n");
    #endif
    return retval;
}
 
