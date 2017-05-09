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
#define BBI2C_DEBUG 0
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
BitBangI2C::BitBangI2C(PinName sda, PinName scl, bool internalPullup) :
    sdaPin(sda, internalPullup), sclPin(scl, internalPullup)
{
    // set the default frequency to 100kHz
    frequency(100000);
    
    // we're initially in a stop
    inStop = true;
}

void BitBangI2C::frequency(uint32_t freq)
{
    // figure the clock time per cycle
    clkPeriod_us = 1000000/freq;

    // Figure wait times according to frequency
    if (freq <= 100000)
    {
        // standard mode I2C bus - up to 100kHz
        
        // nanosecond parameters
        tLow = calcHiResWaitTime(4700);
        tHigh = calcHiResWaitTime(4000);
        tHdSta = calcHiResWaitTime(4000);
        tSuSta = calcHiResWaitTime(4700);
        tSuSto = calcHiResWaitTime(4000);
        tAck = calcHiResWaitTime(300);
        tSuDat = calcHiResWaitTime(250);
        tBuf = calcHiResWaitTime(4700);
    }
    else if (freq <= 400000)
    {
        // fast mode I2C - up to 400kHz

        // nanosecond parameters
        tLow = calcHiResWaitTime(1300);
        tHigh = calcHiResWaitTime(600);
        tHdSta = calcHiResWaitTime(600);
        tSuSta = calcHiResWaitTime(600);
        tSuSto = calcHiResWaitTime(600);
        tAck = calcHiResWaitTime(100);
        tSuDat = calcHiResWaitTime(100);
        tBuf = calcHiResWaitTime(1300);
    }
    else
    {
        // fast mode plus - up to 1MHz

        // nanosecond parameters
        tLow = calcHiResWaitTime(500);
        tHigh = calcHiResWaitTime(260);
        tHdSta = calcHiResWaitTime(260);
        tSuSta = calcHiResWaitTime(260);
        tSuSto = calcHiResWaitTime(260);
        tAck = calcHiResWaitTime(50);
        tSuDat = calcHiResWaitTime(50);
        tBuf = calcHiResWaitTime(500);
    }
}

void BitBangI2C::start() 
{    
    // check to see if we're starting after a stop, or if this is a
    // repeated start
    if (inStop)
    {
        // in a stop - make sure we waited for the minimum hold time
        hiResWait(tBuf);
    }
    else
    {
        // repeated start - take data high
        sdaHi();
        hiResWait(tSuDat);
        
        // take clock high
        sclHi();
        
        // wait for the minimum setup period
        hiResWait(tSuSta);
    }
    
    // take data low
    sdaLo();
    
    // wait for the setup period and take clock low
    hiResWait(tHdSta);
    sclLo();

    // wait for the low period
    hiResWait(tLow);    
    
    // no longer in a stop
    inStop = false;
}

void BitBangI2C::stop() 
{
    // if we're not in a stop, enter one
    if (!inStop)
    {
        // take SDA low
        sdaLo();

        // take SCL high
        sclHi();
        hiResWait(tSuSto);

        // take SDA high
        sdaHi();
        
        // we're in a stop
        inStop = true;
    }
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
        
    // release SDA so the device can control it
    sdaHi();
        
    // read the ACK bit
    int ack = readBit();
    
    // take SDA low again
    sdaLo();
            
    // return success if ACK was 0
    return ack;
}

int BitBangI2C::read(bool ack) 
{
    // take SDA high before reading
    sdaHi();

    // read 8 bits, most significant first
    uint8_t data = 0;
    for (int i = 0 ; i < 8 ; ++i)
        data = (data << 1) | readBit();

    // switch to output mode and send the ACK bit
    writeBit(!ack);

    // release SDA
    sdaHi();

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
    int t = 0;
    do
    {
        // if the clock is high, we're ready to go
        if (sclPin.read())
        {
            // wait for the data setup time
            hiResWait(tSuDat);
            
            // read the bit    
            bool bit = sdaPin.read();
            
            // take the clock low again
            sclLo();
            hiResWait(tLow);
            
            // return the bit
            return bit;
        }
    }
    while (t++ < 100000);

    // we timed out
    eprintf("i2c.readBit, clock stretching timeout\r\n");
    return 0;
}
