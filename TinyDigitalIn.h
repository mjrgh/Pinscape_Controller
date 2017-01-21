#include "mbed.h"

// TinyDigitalIn - a simpler verison of DigitalIn that takes less
// memory.
//
// This version uses the same mbed library infrastructure as the
// regular DigitalIn, but we save a little memory by storing only
// the minimum set of fields needed to read the pin.  The mbed
// DigitalIn has a larger memory footprint because it stores the 
// full set of machine register pointers for the pin, most of 
// which aren't needed for an input-only pin.

class TinyDigitalIn
{
public:
    TinyDigitalIn(PinName pin) { assignPin(pin); }
    TinyDigitalIn() { assignPin(NC); }
    
    void assignPin(PinName pin)
    {
        if (pin != NC)
        {
            // initialize the pin as a GPIO Digital In port
            gpio_t gpio;
            gpio_init_in(&gpio, pin);
            
            // get the register input port and mask
            pdir = gpio.reg_in;
            uint32_t mask = gpio.mask;
            
            // Figure the bit shift: find how many right shifts it takes
            // to shift the mask bit into the 0th bit position.  This lets
            // us pull out the result value in read() as a 0 or 1 by shifting
            // the register by this same amount and masking it against 0x01.
            // The right shift is slightly more efficient than a conditional
            // to convert a bit in the middle of the register to a canonical
            // 0 or 1 result, and we have to do the mask anyway to pull out
            // the one bit, so this makes the overall read slightly faster.
            for (shift = 0 ; 
                 mask != 0 && (mask & 0x00000001) == 0 ;
                 mask >>= 1, shift++) ;
         }
         else
         {
             // not connected - point to a dummy port that always reads as 0
             pdir = (volatile uint32_t *)&pdir_nc;
             shift = 0;
         }
    }
    
    inline int read() { return (*pdir >> shift) & 0x00000001; }
    inline operator int() { return read(); }
    
private:
    volatile uint32_t *pdir;    // pointer to GPIO register for this port 
    uint8_t shift;              // number of bits to shift register value to get our port bit
    
    // pointer to dummy location for NC ports - reads as all 1 bits,
    // as though it were wired to a pull-up port that's not connected
    // to anything external
    static const uint32_t pdir_nc;
} __attribute__((packed));
