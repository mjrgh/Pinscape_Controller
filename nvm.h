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
// Hack alert!
//
// Our use of flash for this purpose is ad hoc and not supported
// by the mbed platform.  mbed doesn't impose a file system (or any
// other kind of formal structure) on the KL25Z flash; it simply 
// treats the flash as a raw storage space for linker output and 
// assumes that the linker is the only thing using it.  So in order
// to use the flash, we basically have to do it on the sly, by 
// using space that the linker happens to leave unused.
// 
// Fortunately, it's fairly easy to do this, because the flash is
// mapped in the obvious way, as a single contiguous block in the
// CPU memory space, and because the mbed linker seems to do the 
// obvious thing, storing its entire output in a single contiguous 
// block starting at the lowest flash address.  This means that all 
// flash memory from (lowest flash address + length of linker output)
// to (highest flash address) is unused and available for our sneaky
// system.  Unfortunately, there's no reliable way for the program
// to determine the length of the linker output, so we can't know 
// where our available region starts.  But we do know how much flash 
// there is overall, so we know where the flash ends.  We can 
// therefore align our storage region at the end of memory and hope 
// that it's small enough not to encroach on the linker space.  We
// can actually do a little better than hope: the mbed tools tell us 
// at the UI level how much flash the linker is using, even though it 
// doesn't expose that information to us programmatically, so we can 
// manually check that we have enough room.  As of this writing, the 
// configuration structure is much much smaller than the available 
// leftover flash space, so we should be safe indefinitely, barring 
// a major expansion of the configuration structure or code size.
// (And if we get to the point where we actually don't have space
// for our ~1K structure, we'll be up against the limits of the
// device anyway, so we'd have to rein in our ambitions or write
// more efficient code for deeper reasons than sharing this tiny
// sliver of memory.)
//
// The boot loader seems to erase the entire flash space every time
// we load new firmware, so our configuration structure is lost
// when we update.  Furthermore, since we explicitly choose to put
// the config structure in space that isn't initialized by the linker,
// we can't specify the new contents stored on these erasure events.
// To deal with this, we use a signature and checksum to check the
// integrity of the stored data.  The erasure leaves deterministic
// values in memory unused by the linker, so we'll always detect
// an uninitialized config structure after an update.
//
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
    int valid() const
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
        
        // figure the number of sectors required
        int sectors = (sizeof(NVM) + SECTOR_SIZE - 1) / SECTOR_SIZE;
        for (int i = 0 ; i < sectors ; ++i)
            iap.erase_sector(addr + i*SECTOR_SIZE);

        // save the data
        iap.program_flash(addr, this, sizeof(*this));
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
