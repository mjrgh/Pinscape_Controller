// Bit-bang I2C driver.  This provides I2C functionality on arbitrary
// GPIO pins, allowing more flexibility in wiring an I2C device.  Most
// MCUs with hardware I2C modules can only access the hardware I2C
// functionality through specific GPIO pins.  
// 
// Hardware I2C is preferable when available, and can be accessed through
// the mbed I2C class.  The only reason to use this class rather than the
// native mbed I2C is that you need I2C functionality on pins that don't
// support I2C in the MCU hardware.
// 
// This class isn't meant to be directly compatible with the mbed I2C class, 
// but we try to adhere to the mbed conventions and method names to make it
// a mostly drop-in replacement.  In particular, we use the mbed library's
// "2X" device address convention.
//
// IMPORTANT: Wherever a device address is specified, pass in 2X the nominal
// address.  For example, if the device's actual address is 0x10, pass 0x20
// for all 'addr' parameters.

#ifndef _BITBANGI2C_H_
#define _BITBANGI2C_H_
 
#include "mbed.h"
 
class BitBangI2C 
{
public:    
    // create the interface
    BitBangI2C(PinName sda, PinName scl);

    // set the bus frequency in Hz
    void setFrequency(uint32_t freq);

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
    void writeBit(int bit);
    
    // set SCL/SDA lines to high (1) or low(0)
    inline void scl(int level) { setLevel(sclPin, level); }
    inline void sda(int level) { setLevel(sdaPin, level); }
    void setLevel(DigitalInOut pin, int level);

protected:
    // SCL and SDA pins
    DigitalInOut sclPin;
    DigitalInOut sdaPin;

    // inverse of frequency = clock period in microseconds
    uint32_t clkPeriod_us;
};
 
#endif /* _BITBANGI2C_H_ */
