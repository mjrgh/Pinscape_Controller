#include "FreescaleIAP.h"
 
//#define IAPDEBUG
 
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

static inline void run_command(FTFA_Type *);
bool check_boundary(int address, unsigned int length);
bool check_align(int address);
IAPCode check_error(void);
    
FreescaleIAP::FreescaleIAP()
{
}
 
FreescaleIAP::~FreescaleIAP()
{
} 

// We use an assembly language implementation of the EXEC function in
// order to satisfy the requirement (mentioned in the hardware reference)
// that code that writes to Flash must reside in RAM.  There's a potential
// for a deadlock if the code that triggers a Flash write operation is 
// itself stored in Flash, as an instruction fetch to Flash can deadlock 
// against the erase/write.  In practice this seems to be rare, but I
// seem to be able to trigger it once in a while.  (Which is to say that
// I can trigger occasional lock-ups during writes.  It's not clear that
// the Flash bus deadlock is the actual cause, but the timing strongly
// suggests this.)
// 
// The mbed tools don't have a way to put a C function in RAM.  The mbed
// assembler can, though.  So to get our invoking code into RAM, we have
// to write it in assembly.  Fortunately, the code involved is very simple:
// just a couple of writes to the memory-mapped Flash controller register,
// and a looped read and bit test from the same location to wait until the
// operation finishes.
//
#define USE_ASM_EXEC 1
#if USE_ASM_EXEC
extern "C" void iapExecAsm(volatile uint8_t *);
#endif

// execute an FTFA command
static inline void run_command(FTFA_Type *ftfa) 
{    
    // Disable interupts.  It's critical that we don't service any
    // interrupts while a Flash operation is taking place because 
    // an ISR would normally be a C routine located in Flash, so 
    // fetching its instructions could deadlock against the write
    // or erase operation we're performing.
    __disable_irq();

#if USE_ASM_EXEC
    // Call our RAM-based assembly routine to do this work.  The
    // assembler routine implements the same ftfa->FSTAT register
    // operations in the C alternative code below.
    iapExecAsm(&ftfa->FSTAT);

#else // USE_ASM_EXEC
    // Clear possible old errors, start command, wait until done
    ftfa->FSTAT = FTFA_FSTAT_FPVIOL_MASK | FTFA_FSTAT_ACCERR_MASK | FTFA_FSTAT_RDCOLERR_MASK;
    ftfa->FSTAT = FTFA_FSTAT_CCIF_MASK;
    while (!(ftfa->FSTAT & FTFA_FSTAT_CCIF_MASK));

#endif // USE_ASM_EXEC
    
    // done with the Flash access - re-enable interrupts
    __enable_irq();
}    

 
IAPCode FreescaleIAP::erase_sector(int address) {
    #ifdef IAPDEBUG
    printf("IAP: Erasing at %x\r\n", address);
    #endif
    if (check_align(address))
        return AlignError;
    
    //Setup command
    FTFA->FCCOB0 = EraseSector;
    FTFA->FCCOB1 = (address >> 16) & 0xFF;
    FTFA->FCCOB2 = (address >> 8) & 0xFF;
    FTFA->FCCOB3 = address & 0xFF;
    
    run_command(FTFA);
    
    return check_error();
}
 
IAPCode FreescaleIAP::program_flash(int address, const void *vp, unsigned int length) {
    
    const char *data = (const char *)vp;
    
    #ifdef IAPDEBUG
    printf("IAP: Programming flash at %x with length %d\r\n", address, length);
    #endif
    if (check_align(address))
        return AlignError;
        
    IAPCode eraseCheck = verify_erased(address, length);
    if (eraseCheck != Success)
        return eraseCheck;
    
    IAPCode progResult;
    for (int i = 0; i < length; i+=4) {
        progResult = program_word(address + i, data + i);
        if (progResult != Success)
            return progResult;
    }
    
    return Success;
}
 
uint32_t FreescaleIAP::flash_size(void) {
    uint32_t retval = (SIM->FCFG2 & 0x7F000000u) >> (24-13);
    if (SIM->FCFG2 & (1<<23))           //Possible second flash bank
        retval += (SIM->FCFG2 & 0x007F0000u) >> (16-13);
    return retval;
}
 
IAPCode FreescaleIAP::program_word(int address, const char *data) {
    #ifdef IAPDEBUG
    printf("IAP: Programming word at %x, %d - %d - %d - %d\r\n", address, data[0], data[1], data[2], data[3]);
    #endif
    if (check_align(address))
        return AlignError;
    
    //Setup command
    FTFA->FCCOB0 = ProgramLongword;
    FTFA->FCCOB1 = (address >> 16) & 0xFF;
    FTFA->FCCOB2 = (address >> 8) & 0xFF;
    FTFA->FCCOB3 = address & 0xFF;
    FTFA->FCCOB4 = data[3];
    FTFA->FCCOB5 = data[2];
    FTFA->FCCOB6 = data[1];
    FTFA->FCCOB7 = data[0];
    
    run_command(FTFA);
    
    return check_error();
}
 
/* Check if no flash boundary is violated
   Returns true on violation */
bool check_boundary(int address, unsigned int length) {
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
bool check_align(int address) {
    bool retval = address & 0x03;
    #ifdef IAPDEBUG
    if (retval)
        printf("IAP: Alignment violation\r\n");
    #endif
    return retval;
}
 
/* Check if an area of flash memory is erased
   Returns error code or Success (in case of fully erased) */
IAPCode FreescaleIAP::verify_erased(int address, unsigned int length) {
    #ifdef IAPDEBUG
    printf("IAP: Verify erased at %x with length %d\r\n", address, length);
    #endif
    
    if (check_align(address))
        return AlignError;
    
    //Setup command
    FTFA->FCCOB0 = Read1s;
    FTFA->FCCOB1 = (address >> 16) & 0xFF;
    FTFA->FCCOB2 = (address >> 8) & 0xFF;
    FTFA->FCCOB3 = address & 0xFF;
    FTFA->FCCOB4 = (length >> 10) & 0xFF;
    FTFA->FCCOB5 = (length >> 2) & 0xFF;
    FTFA->FCCOB6 = 0;
    
    run_command(FTFA);
    
    IAPCode retval = check_error();
    if (retval == RuntimeError) {
        #ifdef IAPDEBUG
        printf("IAP: Flash was not erased\r\n");
        #endif
        return EraseError;
    }
    return retval;
        
}
 
/* Check if an error occured 
   Returns error code or Success*/
IAPCode check_error(void) {
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