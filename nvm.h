// NVM - Non-Volatile Memory
//
// This module handles the storage of our configuration settings
// and calibration data in flash memory, which allows us to
// retrieve these settings after each power cycle.


#ifndef NVM_H
#define NVM_H

#include "config.h"
#include "FreescaleIAP.h"


// Non-volatile memory (NVM) structure
//
// This structure defines the layout of our saved configuration
// and calibration data in flash memory.
//
// The space in flash for the structure is reserved in the main
// program, by exploiting the linker's placement of const data
// in flash memory.  This gives us a region of the appropriate
// size.
struct NVM
{
public:
    // checksum - we use this to determine if the flash record
    // has been properly initialized
    uint32_t checksum;

    // signature and version reference values
    static const uint32_t SIGNATURE = 0x4D4A522A;
    static const uint16_t VERSION = 0x0003;
    
    // Is the data structure valid?  We test the signature and 
    // checksum to determine if we've been properly stored.
    bool valid() const
    {
        return (d.sig == SIGNATURE 
                && d.vsn == VERSION
                && d.sz == sizeof(NVM)
                && checksum == CRC32(&d, sizeof(d)));
    }
    
    // save to non-volatile memory
    void save(FreescaleIAP &iap, int addr)
    {
        // update the checksum and structure size
        d.sig = SIGNATURE;
        d.vsn = VERSION;
        d.sz = sizeof(NVM);
        checksum = CRC32(&d, sizeof(d));
        
        // save the data to flash
        iap.program_flash(addr, this, sizeof(*this));
    }
    
    // verify that the NVM matches the in-memory configuration
    bool verify(int addr)
    {
        return memcmp((NVM *)addr, this, sizeof(*this)) == 0;
    }
    
    // stored data (excluding the checksum)
    struct
    {
        // Signature, structure version, and structure size, as further
        // verification that we have valid data.
        uint32_t sig;
        uint16_t vsn;
        int sz;
        
        // configuration and calibration data
        Config c;
    } d;
};

#endif /* NVM_M */
