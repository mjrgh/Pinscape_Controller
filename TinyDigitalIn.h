// TinyDigitalIn - a simpler verison of DigitalIn that takes less
// memory.
//
// This version uses the same mbed library infrastructure as the
// regular DigitalIn, but we save a bit of memory by storing only
// the minimum set of fields needed to read the pin.  The mbed
// DigitalIn has a larger memory footprint because it stores the 
// full set of machine register pointers for the pin, most of 
// which aren't needed for an input-only pin.

class TinyDigitalIn
{
public:
    TinyDigitalIn(PinName pin)
    {
        // make sure there's a pin connected
        if (pin != NC)
        {
            // initialize the pin as a GPIO Digital In port
            gpio_t gpio;
            gpio_init_in(&gpio, pin);
            
            // get the register input port and mask
            pdir = gpio.reg_in;
            mask = gpio.mask;
        }
        else
        {
            // no pin - set a null read register
            pdir = 0;
        }
    }
    
    inline int read() { return pdir != 0 && (*pdir & mask) ? 1 : 0; }
    inline operator int() { return read(); }
    
private:
    volatile uint32_t *pdir;
    uint32_t mask;
};
