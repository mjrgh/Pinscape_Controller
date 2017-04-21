// Bit Bang BitBangI2C implementation for KL25Z
//

#include "mbed.h"
#include "BitBangI2C.h"


// --------------------------------------------------------------------------
//
// Debugging:
//
//   0  -> no debugging
//   1  -> print (on console) error messages only
//   2  -> print full diagnostics
//
// dprintf() = general debug diagnostics (printed only in case 2)
// eprintf() = error diagnostics (printed in case 1 and above)
//
#define BBI2C_DEBUG 1
#if BBI2C_DEBUG
# define eprintf(...) printf(__VA_ARGS__)
# if BBI2C_DEBUG >= 2
#  define dprintf(...) printf(__VA_ARGS__)
# else
#  define dprintf(...)
# endif
static const char *dbgbytes(const uint8_t *bytes, size_t len)
{
    static char buf[128];
    char *p = buf;
    for (int i = 0 ; i < len && p + 4 < buf + sizeof(buf) ; ++i)
    {
        if (i > 0) *p++ = ',';
        sprintf(p, "%02x", bytes[i]);
        p += 2;
    }
    *p = '\0';
    return buf;
}
#else
# define dprintf(...)
# define eprintf(...)
#endif

// --------------------------------------------------------------------------
//
// Bit-bang I2C implementation
//
BitBangI2C::BitBangI2C(PinName sda, PinName scl) :
    sclPin(scl), sdaPin(sda)
{
    // set the default frequency to 100kHz
    frequency(100000);
}

void BitBangI2C::frequency(uint32_t freq)
{
    // figure the clock time per cycle
    clkPeriod_us = 1000000/freq;

    // Figure wait times according to frequency
    if (freq <= 100000)
    {
        // standard mode I2C bus - up to 100kHz
        tLow = calcHiResWaitTime(4700);
        tHigh = calcHiResWaitTime(4000);
        tBuf = calcHiResWaitTime(4700);
        tHdSta = calcHiResWaitTime(4000);
        tSuSta = calcHiResWaitTime(4700);
        tSuSto = calcHiResWaitTime(4000);
        tAck = calcHiResWaitTime(300);
        tData = calcHiResWaitTime(300);
        tSuDat = calcHiResWaitTime(250);
    }
    else if (freq <= 400000)
    {
        // fast mode I2C - up to 400kHz
        tLow = calcHiResWaitTime(1300);
        tHigh = calcHiResWaitTime(600);
        tBuf = calcHiResWaitTime(1300);
        tHdSta = calcHiResWaitTime(600);
        tSuSta = calcHiResWaitTime(600);
        tSuSto = calcHiResWaitTime(600);
        tAck = calcHiResWaitTime(100);
        tData = calcHiResWaitTime(100);
        tSuDat = calcHiResWaitTime(100);
    }
    else
    {
        // fast mode plus - up to 1MHz
        tLow = calcHiResWaitTime(500);
        tHigh = calcHiResWaitTime(260);
        tBuf = calcHiResWaitTime(500);
        tHdSta = calcHiResWaitTime(260);
        tSuSta = calcHiResWaitTime(260);
        tSuSto = calcHiResWaitTime(260);
        tAck = calcHiResWaitTime(50);
        tData = calcHiResWaitTime(50);
        tSuDat = calcHiResWaitTime(50);
    }
}

void BitBangI2C::start() 
{    
    // take clock and data high
    sclHi();
    sdaHi();
    hiResWait(tBuf);
    
    // take data low
    sdaLo();
    hiResWait(tHdSta);
    
    // take clock low
    sclLo();
    hiResWait(tLow);
}

void BitBangI2C::stop() 
{
    // take SDA low
    sdaLo();

    // take SCL high
    sclHi();
    hiResWait(tSuSto);
    
    // take SDA high
    sdaHi();
    hiResWait(tBuf);
}

bool BitBangI2C::wait(uint32_t timeout_us)
{
    // set up a timer to monitor the timeout period    
    Timer t;
    t.start();

    // wait for an ACK
    for (;;)
    {
        // if SDA is low, it's an ACK
        if (!sdaPin.read())
            return true;
            
        // if we've reached the timeout, abort
        if (t.read_us() > timeout_us)
            return false;
    }
}

void BitBangI2C::reset() 
{
    // write out 9 '1' bits
    for (int i = 0 ; i < 9 ; ++i)
        writeBit(1);
        
    // issue a start sequence
    start();
    
    // take the clock high
    sclHi();
    
    // wait for a few clock cycles
    wait_us(4*clkPeriod_us);
}

int BitBangI2C::write(uint8_t addr, const uint8_t *data, size_t len, bool repeated)
{
    dprintf("i2c.write, addr=%02x [%s] %srepeat\r\n", 
        addr, dbgbytes(data, len), repeated ? "" : "no ");
    
    // send the start signal
    start();
    
    // send the address with the R/W bit set to WRITE (0)
    if (write(addr))
    {
        eprintf(". i2c.write, address write failed, addr=%02x [%s] %srepeat\r\n",
            addr, dbgbytes(data, len), repeated ? "": "no ");
        return -1;
    }
    
    // send the data bytes
    for (int i = 0 ; i < len ; ++i)
    {
        if (write(data[i]))
        {
            eprintf(". i2c.write, write failed at byte %d, addr=%02x [%s] %srepeat\r\n", 
                i, addr, dbgbytes(data, len), repeated ? "" : "no ");
            return -2;
        }
    }
    
    // send the stop, unless the start is to be repeated
    if (!repeated)
        stop();
        
    // success
    return 0;
}

int BitBangI2C::read(uint8_t addr, uint8_t *data, size_t len, bool repeated)
{
    dprintf("i2c.read, addr=%02x\r\n", addr);
    
    // send the start signal
    start();
    
    // send the address with the R/W bit set to READ (1)
    if (write(addr | 0x01))
    {
        eprintf(". i2c.read, read addr write failed, addr=%02x [%s] %srepeat\r\n",
            addr, dbgbytes(data, len), repeated ? "" : "no ");
        return -1;
    }
    
    // Read the data.  Send an ACK after each byte except the last,
    // where we send a NAK.
    for ( ; len != 0 ; --len, ++data)
        *data = read(len > 1);
        
    // send the stop signal, unless a repeated start is indicated
    if (!repeated)
        stop();
        
    // success
    return 0;
}

int BitBangI2C::write(uint8_t data) 
{
    // write the bits, most significant first
    for (int i = 0 ; i < 8 ; ++i, data <<= 1)
        writeBit(data & 0x80);

    // read and return the ACK bit
    return readBit();
}

int BitBangI2C::read(bool ack) 
{
    // read 8 bits, most significant first
    uint8_t data = 0;
    for (int i = 0 ; i < 8 ; ++i)
        data = (data << 1) | readBit();

    // switch to output mode and send the ACK bit
    writeBit(!ack);

    // return the data byte we read
    return data;
}

int BitBangI2C::readBit() 
{
    // take the clock high (actually, release it to the pull-up)
    sclHi();
    
    // Wait (within reason) for it to actually read as high.  The device
    // can intentionally pull the clock line low to tell us to wait while
    // it's working on preparing the data for us.
    Timer t;
    t.start();
    while (sclPin.read() == 0 && t.read_us() < 500000) ;
    
    // if the clock isn't high, we timed out
    if (sclPin.read() == 0)
    {
        eprintf("i2c.readBit, clock stretching timeout\r\n");
        return 0;
    }
    
    // wait until the clock interval is up
    while (t.read_us() < clkPeriod_us);
    
    // read the bit    
    bool bit = sdaPin.read();
    
    // take the clock low again
    sclLo();
    hiResWait(tLow);
    
    // return the bit
    return bit;
}
