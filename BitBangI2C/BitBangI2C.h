// Bit-bang I2C for KL25Z
//
// This implements an I2C interface that can operate on any KL25Z GPIO
// ports, whether or not they're connected to I2C hardware on the MCU.  
// We simply send and receive bits using direct port manipulation (often
// called "bit banging") instead of using the MCU I2C hardware.  This
// is more flexible than the mbed I2C class, since that only works with
// a small number of pins, and there are only two I2C modules in the
// system.  This GPIO version can be to gain additional I2C ports if
// the hardware I2C modules are committed to other purposes, or all of
// the I2C-capable pins are being used for other purposes.
//
// The tradeoff for the added flexibility is that the hardware I2C is
// faster.  This implementation can take advantage of bus speeds up to 
// about 500kHz, which produces data rates of about 272 kbps.  Higher 
// clock speeds are allowed, but the actual bit rate will plateau at 
// this level due to the performance constraints of the CPU (and of
// this code itself; some additional performance could probably be 
// gained by optimizing it further).  The KL25Z I2C hardware can double 
// our speed: it can achieve bus speeds of 1MHz and data rates of about
// 540kbps.  Of course, such high speeds can only be used with compatible 
// devices; many devices are limited to the "standard mode" at 100kHz or 
// "fast mode" at 400kHz, both of which we can fully saturate.  However, 
// even at the slower bus speeds, the hardware I2C has another advantage:
// it's capable of DMA operation.  That's vastly superior for large 
// transactions since it lets the CPU do other work in parallel with 
// I2C bit movement.
//
// This class isn't meant to be directly compatible with the mbed I2C 
// class, but we try to adhere to the mbed conventions and method names 
// to make it a mostly drop-in replacement.  In particular, we use the 
// mbed library's "2X" device address convention.  Most device data sheets
// list the device I2C address in 7-bit format, so you'll have to shift
// the nominal address from the data sheet left one bit in each call
// to a routine here.
//
// Electrically, the I2C bus is designed as a a pair of open-collector 
// lines with pull-up resistors.  Any device can pull a line low by
// shorting it to ground, but no one can pull a line high: instead, you
// *allow* a line to go high by releasing it, which is to say putting
// your connection to it in a high-Z state.  On an MCU, we put a GPIO
// pin in high-Z state by setting its direction to INPUT mode.  So our
// GPIO write strategy is like this:
//
//   - take a pin low (0):   
//        pin.input(); 
//        pin.write(0);
//
//   - take a pin high (1):
//        pin.output();
//
// Note that we don't actually have to write the '0' on each pull low,
// since we just leave the port output register set with '0'.  Changing
// the direction to output is enough to assert the low level, since the
// hardware asserts the level that was previously stored in the output
// register whenever the direction is changed from input to output.


#ifndef _BITBANGI2C_H_
#define _BITBANGI2C_H_
 
#include "mbed.h"
#include "gpio_api.h"
#include "pinmap.h"


// DigitalInOut replacmement class for I2C use.  I2C uses pins a little
// differently from other use cases.  I2C is a bus, where many devices can
// be attached to each line.  To allow this shared access, devices can 
// only drive the line low.  No device can drive the line high; instead,
// the line is *pulled* high, by the attached pull-up resistors, when no 
// one is driving it low.  As a result, we can't use the normal DigitalOut
// write(), since that would try to actively drive the pin high on write(1).
// Instead, write(1) needs to change the pin to high-impedance (high-Z) 
// state instead of driving it, which on the KL25Z is accomplished by 
// changing the port direction mode to INPUT.  So:  
//
//   write(0) = direction->OUTPUT (pin->0)
//   write(1) = direction->INPUT
//
class I2CInOut
{
public:
    I2CInOut(PinName pin)
    {
        // initialize the pin
        gpio_t g;
        gpio_init(&g, pin);
        
        // get the registers
        unsigned int port = (unsigned int)pin >> PORT_SHIFT;
        FGPIO_Type *r = (FGPIO_Type *)(FPTA_BASE + port*0x40);
        __IO uint32_t *pin_pcr = (__IO uint32_t*)(PORTA_BASE + pin); 
        
        // set no-pull-up mode (clear PE bit = Pull Enable)
        *pin_pcr &= ~0x02;
           
        // save the register information we'll need later
        this->mask = g.mask;
        this->PDDR = &r->PDDR;          
        this->PDIR = &r->PDIR;
        
        // initially set as input to release the line
        r->PDDR &= ~mask;
        
        // Set the output value to 0.  It will always be zero, since
        // this is the only value we ever drive.  When we want the port
        // to go high, we release it by changing the direction to input.
        r->PCOR = mask;
    }
    
    // write a 1 (high) or 0 (low) value to the pin
    inline void write(int b) { if (b) hi(); else lo(); }
    
    // Take the line high: set as input to put it in high-Z state so that
    // the pull-up resistor takes over.
    inline void hi() { *PDDR &= ~mask; }
    
    // Take the line low: set as output to assert our '0' on the line and
    // pull it low.  Note that we don't have to explicitly write the port
    // output register, since we initialized it with a '0' on our port and
    // never change it.  The hardware will assert the level stored in the
    // register each time we change the direction to output, so there's no
    // need to write the port output register again each time.
    inline void lo() { *PDDR |= mask; }
    
    // read the line
    inline int read()
    {
        *PDDR &= ~mask;         // set as input
        return *PDIR & mask;    // read the port
    }
    
    // direction register
    volatile uint32_t *PDDR;
    
    // input register
    volatile uint32_t *PDIR;
    
    // pin mask 
    uint32_t mask;
};



// bit-bang I2C
class BitBangI2C 
{
public:    
    // create the interface
    BitBangI2C(PinName sda, PinName scl);

    // set the bus frequency in Hz
    void frequency(uint32_t freq);

    // set START condition on the bus
    void start();

    // set STOP condition on the bus
    void stop();
    
    // Write a series of bytes.  Returns 0 on success, non-zero on failure.
    // Important: 'addr' is 2X the nominal address - shift left by one bit.
    int write(uint8_t addr, const uint8_t *data, size_t len, bool repeated = false);
    
    // write a byte; returns true if ACK was received
    int write(uint8_t data);
    
    // Read a series of bytes.  Returns 0 on success, non-zero on failure.
    // Important: 'addr' is 2X the nominal address - shift left by one bit.
    int read(uint8_t addr, uint8_t *data, size_t len, bool repeated = false);

    // read a byte, optionally sending an ACK on receipt
    int read(bool ack);

    // wait for ACK; returns true if ACK was received
    bool wait(uint32_t timeout_us);

    // reset the bus
    void reset();

protected:
    // read/write a bit
    int readBit();
    
    // write a bit
    inline void writeBit(int bit)
    {
        // put the bit on the SDA line
        sdaPin.write(bit);
        hiResWait(tSuDat);
        
        // clock it
        sclPin.hi();
        hiResWait(tData);
        
        // drop the clock
        sclPin.lo();
        hiResWait(tLow);
    }
    
    // set SCL/SDA lines to high (1) or low(0)
    inline void scl(int level) { sclPin.write(level); }
    inline void sda(int level) { sdaPin.write(level); }
    
    inline void sclHi() { sclPin.hi(); }
    inline void sclLo() { sclPin.lo(); }
    inline void sdaHi() { sdaPin.hi(); }
    inline void sdaLo() { sdaPin.lo(); }

    // SCL and SDA pins
    I2CInOut sclPin;
    I2CInOut sdaPin;

    // inverse of frequency = clock period in microseconds
    uint32_t clkPeriod_us;
    
    // High-resolution wait.  This provides sub-microsecond wait
    // times, to get minimum times for I2C events.  With the ARM
    // compiler, this produces measured wait times as follows:
    //
    //    n=0    104ns
    //    n=1    167ns
    //    n=2    271ns
    //    n=3    375ns
    //    n=4    479ns
    //
    // For n > 1, the wait time is 167ns + (n-1)*104ns.
    // These times take into account caller overhead to load the
    // wait time from a member variable.  Callers getting the wait
    // time from a constant or stack variable will have different 
    // results.
    inline void hiResWait(volatile int n)
    {
        while (n != 0)
            --n;
    }
    
    // Figure the hiResWait() time for a given nanosecond time.
    // We use this during setup to precompute the wait times required
    // for various events at a given clock speed.
    int calcHiResWaitTime(int nanoseconds)
    {
        // the shortest wait time is 104ns
        if (nanoseconds <= 104)
            return 0;
            
        // Above that, we work in 104ns increments with a base 
        // of 167ns.  We round at the halfway point, because we
        // assume there's always a little extra overhead in the
        // caller itself that will pad by at least one instruction
        // of 60ns, which is more than half our interval.
        return (nanoseconds - 167 + 52)/104 + 1;
    }
    
    // Time delays for I2C events.  I2C has minimum timing requirements
    // based on the clock speed.  Some of these are as short as 50ns.
    // The mbed wait timer has microsecond resolution, which is much
    // too coarse for fast I2C clock speeds, so we implement our own
    // finer-grained wait.
    //
    // These are in hiResWait() units - see above.
    //
    int tLow;       // SCL low period
    int tHigh;      // SCL high period
    int tBuf;       // bus free time between start and stop conditions
    int tHdSta;     // hold time for start condition
    int tSuSta;     // setup time for repeated start condition
    int tSuSto;     // setup time for stop condition
    int tSuDat;     // data setup time
    int tAck;       // ACK time
    int tData;      // data valid time
};
 
#endif /* _BITBANGI2C_H_ */
