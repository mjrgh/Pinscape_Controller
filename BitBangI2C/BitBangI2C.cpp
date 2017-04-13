// Bit Bang BitBangI2C implementation
//
// This implements an I2C interface that can operate on any GPIO ports,
// whether or not connected to I2C hardware on the MCU.  We simply send
// and receive bits using DigitalInOut ports instead of MCU I2C hardware.
//
// Electrically, the I2C bus is designed as a a pair of open-collector 
// lines with pull-up resistors.  Any device can pull a line low by
// shorting it to ground, but no one can pull a line high: instead, you
// *allow* a line to go high by releasing it, which is to say putting
// your connection to it in a high-Z state.  On an MCU, we put a GPIO
// in in high-Z state by setting its direction to INPUT mode.  So our
// GPIO write strategy is like this:
//
//   - take a pin low (0):   
//        pin.input(); 
//        pin.write(0);
//
//   - take a pin high (1):
//        pin.output();
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
#endif

// --------------------------------------------------------------------------
//
// Bit-bang I2C implementation
//
BitBangI2C::BitBangI2C(PinName sda, PinName scl) :
    sclPin(scl), sdaPin(sda)
{
    // set the default frequency to 100kHz
    setFrequency(100000);
    
    // start with pins in high Z state
    this->sda(1);
    this->scl(1);
}

void BitBangI2C::setFrequency(uint32_t freq)
{
    // figure the wait time per half-bit in microseconds for this frequency,
    // with a minimum of 1us
    clkPeriod_us = 500000/freq;
    if (clkPeriod_us < 1)
        clkPeriod_us = 1;
}

void BitBangI2C::start() 
{    
    // take clock high
    scl(1);
    wait_us(clkPeriod_us);
    
    // take data high
    sda(1);
    wait_us(clkPeriod_us);
    
    // take data low
    sda(0);
    wait_us(clkPeriod_us);
    
    // take clock low
    scl(0);
    wait_us(clkPeriod_us);
}

void BitBangI2C::stop() 
{
    // take SCL high
    scl(1);
    wait_us(clkPeriod_us);
    
    // take SDA high
    sda(1);
    wait_us(clkPeriod_us);
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
        
        // wait briefly
        wait_us(20);
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
    scl(1);
    wait_us(clkPeriod_us);
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

void BitBangI2C::writeBit(int bit) 
{
    // express the bit on the SDA line
    if (bit)
        sdaPin.input();
    else
    {
        sdaPin.output();
        sdaPin.write(0);
    }
    
    // clock it
    wait_us(clkPeriod_us);
    sclPin.input();
    
    wait_us(clkPeriod_us);
    sclPin.output();
    sclPin.write(0);
    
    wait_us(clkPeriod_us);
}

int BitBangI2C::readBit() 
{
    // take the clock high (actually, release it to the pull-up)
    scl(1);
    
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
    scl(0);
    wait_us(clkPeriod_us);
    
    // return the bit
    return bit;
}

// --------------------------------------------------------------------------
//
// Low-level line controls.  
// 
// - To take a line LOW, we pull it to ground by setting the GPIO pin 
//   direction to OUTPUT and writing a 0.  
//
// - To take a line HIGH, we simply put it in high-Z state by setting 
//   its GPIO pin direction to INPUT.
//
void BitBangI2C::setLevel(DigitalInOut pin, int level)
{
    if (level)
    {
        // set HIGH - release line by setting pin to INPUT mode
        pin.input();
    }
    else
    {
        // set LOW - pull line low by setting as OUTPUT and writing 0
        pin.output();
        pin.write(0);
    }
}
