// FreescaleIAP - custom version
//
// This is a simplified version of Erik Olieman's FreescaleIAP, a flash 
// memory writer for Freescale boards.  This version combines erase, write,
// and verify into a single API call.  The caller only has to give us a
// buffer (of any length) to write, and the address to write it to, and
// we'll do the whole thing - essentially a memcpy() to flash.
//
// This version uses an assembler implementation of the core code that
// launches an FTFA command and waits for completion, to minimize the
// size of the code and to ensure that it's placed in RAM.  The KL25Z
// flash controller prohibits any flash reads while an FTFA command is
// executing.  This includes instruction fetches; any instruction fetch
// from flash while an FTFA command is running will fail, which will 
// freeze the CPU.  Placing the execute/wait code in RAM ensures that
// the wait loop itself won't trigger a fetch.  It's also vital to disable
// interrupts while the execute/wait code is running, to ensure that we
// don't jump to an ISR in flash during the wait.
//
// Despite the dire warnings in the hardware reference manual about putting
// the FTFA execute/wait code in RAM, it doesn't actually appear to be
// necessary, as long as the wait loop is very small (in terms of machine
// code instruction count).  In testing, Erik has found that a flash-resident
// version of the code is stable, and further found (by testing combinations
// of cache control settings via the platform control register, MCM_PLACR)
// that the stability comes from the loop fitting into CPU cache, which
// allows the loop to execute without any fetches taking place.  Even so,
// I'm keeping the RAM version, out of an abundance of caution: just in
// case there are any rare or oddball conditions (interrupt timing, say) 
// where the cache trick breaks.  Putting the code in RAM seems pretty
// much guaranteed to work, whereas the cache trick seems somewhat to be
// relying on a happy accident, and I personally don't know the M0+ 
// architecture well enough to be able to convince myself that it really
// will work under all conditions.  There doesn't seem to be any benefit
// to not using the assembler, either, as it's very simple code and takes
// up little RAM (about 40 bytes).


#include "FreescaleIAP.h"

//#define IAPDEBUG

// assembly interface
extern "C" {
    // Execute the current FTFA command and wait for completion.
    // This is an assembler implementation that runs entirely in RAM,
    // to ensure strict compliance with the prohibition on reading
    // flash (for instruction fetches or any other reason) during FTFA 
    // execution.
    void iapExecAndWait();
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

// Get the size of the flash memory on the device
uint32_t FreescaleIAP::flashSize(void) 
{
    uint32_t retval = (SIM->FCFG2 & 0x7F000000u) >> (24-13);
    if (SIM->FCFG2 & (1<<23))           // Possible second flash bank
        retval += (SIM->FCFG2 & 0x007F0000u) >> (16-13);
    return retval;
}

// Check if an error occurred
static FreescaleIAP::IAPCode checkError(void) 
{
    if (FTFA->FSTAT & FTFA_FSTAT_FPVIOL_MASK) {
        #ifdef IAPDEBUG
        printf("IAP: Protection violation\r\n");
        #endif
        return FreescaleIAP::ProtectionError;
    }
    if (FTFA->FSTAT & FTFA_FSTAT_ACCERR_MASK) {
        #ifdef IAPDEBUG
        printf("IAP: Flash access error\r\n");
        #endif
        return FreescaleIAP::AccessError;
    }
    if (FTFA->FSTAT & FTFA_FSTAT_RDCOLERR_MASK) {
        #ifdef IAPDEBUG
        printf("IAP: Collision error\r\n");
        #endif
        return FreescaleIAP::CollisionError;
    }
    if (FTFA->FSTAT & FTFA_FSTAT_MGSTAT0_MASK) {
        #ifdef IAPDEBUG
        printf("IAP: Runtime error\r\n");
        #endif
        return FreescaleIAP::RuntimeError;
    }
    return FreescaleIAP::Success;
}

// check for proper address alignment
static bool checkAlign(int address) 
{
    bool retval = address & 0x03;
    #ifdef IAPDEBUG
    if (retval)
        printf("IAP: Alignment violation\r\n");
    #endif
    return retval;
}

// clear errors in the FTFA
static void clearErrors()
{
    // wait for any previous command to complete    
    while (!(FTFA->FSTAT & FTFA_FSTAT_CCIF_MASK)) ;

    // clear the error bits
    if (FTFA->FSTAT & (FTFA_FSTAT_ACCERR_MASK | FTFA_FSTAT_FPVIOL_MASK))
        FTFA->FSTAT |= FTFA_FSTAT_ACCERR_MASK | FTFA_FSTAT_FPVIOL_MASK;
}

static FreescaleIAP::IAPCode eraseSector(int address) 
{
    #ifdef IAPDEBUG
    printf("IAP: Erasing sector at %x\r\n", address);
    #endif

    // ensure proper alignment
    if (checkAlign(address))
        return FreescaleIAP::AlignError;
    
    // clear errors
    clearErrors();
    
    // Set up the command
    FTFA->FCCOB0 = EraseSector;
    FTFA->FCCOB1 = (address >> 16) & 0xFF;
    FTFA->FCCOB2 = (address >> 8) & 0xFF;
    FTFA->FCCOB3 = address & 0xFF;
    
    // execute
    iapExecAndWait();
    
    // check the result
    return checkError();
}

static FreescaleIAP::IAPCode verifySectorErased(int address)
{
    // Always verify in whole sectors.  The
    const unsigned int count = SECTOR_SIZE/4;

    #ifdef IAPDEBUG
    printf("IAP: Verify erased at %x, %d longwords (%d bytes)\r\n", address, count, count*4);
    #endif
    
    if (checkAlign(address))
        return FreescaleIAP::AlignError;

    // clear errors
    clearErrors();
    
    // Set up command
    FTFA->FCCOB0 = Read1s;
    FTFA->FCCOB1 = (address >> 16) & 0xFF;
    FTFA->FCCOB2 = (address >> 8) & 0xFF;
    FTFA->FCCOB3 = address & 0xFF;
    FTFA->FCCOB4 = (count >> 8) & 0xFF;
    FTFA->FCCOB5 = count & 0xFF;
    FTFA->FCCOB6 = 0;

    // execute    
    iapExecAndWait();
    
    // check the result
    FreescaleIAP::IAPCode retval = checkError();
    if (retval == FreescaleIAP::RuntimeError) {
        #ifdef IAPDEBUG
        printf("IAP: Flash was not erased\r\n");
        #endif
        return FreescaleIAP::EraseError;
    }
    return retval;       
}

// Write one sector.  This always writes a full sector, even if the
// requested length is greater or less than the sector size:
//
// - if len > SECTOR_SIZE, we write the first SECTOR_SIZE bytes of the data
//
// - if len < SECTOR_SIZE, we write the data, then fill in the rest of the
//   sector with 0xFF bytes ('1' bits)
//

static FreescaleIAP::IAPCode writeSector(int address, const uint8_t *p, int len)
{    
    #ifdef IAPDEBUG
    printf("IAP: Writing sector at %x with length %d\r\n", address, len);
    #endif

    // program the sector, one longword (32 bits) at a time
    for (int ofs = 0 ; ofs < SECTOR_SIZE ; ofs += 4, address += 4, p += 4, len -= 4)
    {
        // clear errors
        clearErrors();
        
        // Set up the command
        FTFA->FCCOB0 = ProgramLongword;
        FTFA->FCCOB1 = (address >> 16) & 0xFF;
        FTFA->FCCOB2 = (address >> 8) & 0xFF;
        FTFA->FCCOB3 = address & 0xFF;
        
        // Load the longword to write.  If we're past the end of the source
        // data, write all '1' bits to the balance of the sector.
        FTFA->FCCOB4 = len > 3 ? p[3] : 0xFF;
        FTFA->FCCOB5 = len > 2 ? p[2] : 0xFF;
        FTFA->FCCOB6 = len > 1 ? p[1] : 0xFF;
        FTFA->FCCOB7 = len > 0 ? p[0] : 0xFF;
        
        // execute
        iapExecAndWait();
        
        // check errors
        FreescaleIAP::IAPCode status = checkError();
        if (status != FreescaleIAP::Success)
            return status;
    }
    
    // no problems
    return FreescaleIAP::Success;
}

// Program a block of memory into flash. 
FreescaleIAP::IAPCode FreescaleIAP::programFlash(
    int address, const void *src, unsigned int length) 
{    
    #ifdef IAPDEBUG
    printf("IAP: Programming flash at %x with length %d\r\n", address, length);
    #endif
    
    // presume success
    FreescaleIAP::IAPCode status = FreescaleIAP::Success;
    
    // Show diagnostic LED colors while writing.  I'm finally convinced this
    // is well and truly 100% reliable now, but I've been wrong before, so
    // we'll keep this for now.  The idea is that if we freeze up, we'll at
    // least know which stage we're at from the last color displayed.
    extern void diagLED(int,int,int);
    
    // try a few times if we fail to verify
    for (int tries = 0 ; tries < 5 ; ++tries)
    {
        // Do the write one sector at a time
        int curaddr = address;
        const uint8_t *p = (const uint8_t *)src;
        int rem = (int)length;
        for ( ; rem > 0 ; curaddr += SECTOR_SIZE, p += SECTOR_SIZE, rem -= SECTOR_SIZE)
        {
            // erase the sector (red LED)
            diagLED(1, 0, 0);
            if ((status = eraseSector(curaddr)) != FreescaleIAP::Success)
                break;
            
            // verify that the sector is erased (yellow LED)
            diagLED(1, 1, 0);
            if ((status = verifySectorErased(curaddr)) != FreescaleIAP::Success)
                break;
            
            // write the data (white LED)
            diagLED(1, 1, 1);
            if ((status = writeSector(curaddr, p, rem)) != FreescaleIAP::Success)
                break;
                
            // back from write (purple LED)
            diagLED(1, 0, 1);
        }
        
        // if we didn't encounter an FTFA error, verify the write
        if (status == FreescaleIAP::Success)
        {
            // Verify the write.  If it was successful, we're done.
            if (memcmp((void *)address, src, length) == 0)
                break;
                
            // We have a mismatch between the flash data and the source.
            // Flag the error and go back for another attempt.
            status = FreescaleIAP::VerifyError;
        }
    }
    
    __enable_irq();
        
    // return the result
    return status;
}

