// TLC59116 interface
//
// The TLC59116 is a 16-channel constant-current PWM controller chip with
// an I2C interface.
//
// Up to 14 of these chips can be connected to a single bus.  Each chip needs
// a unique address, configured via four pin inputs.  (The I2C address is 7
// bits, but the high-order 3 bits are fixed in the hardware, leaving 4 bits
// to configure per chip.  Two of the possible 16 addresses are reserved by
// the chip hardware as broadcast addresses, leaving room for 14 unique chip
// addresses per bus.)
//
// EXTERNAL PULL-UP RESISTORS ARE REQUIRED ON SDA AND SCL.  The internal 
// pull-ups in the KL25Z GPIO ports will only work if the bus speed is 
// limited to 100kHz.  Higher speeds require external pull-ups.  Because
// of the relatively high data rate required, we use the maximum 1MHz bus 
// speed, requiring external pull-ups.  These are typically 2.2K.
//
// This chip is similar to the TLC5940, but has a more modern design with 
// several advantages, including a standardized and much more robust data 
// interface (I2C) and glitch-free startup.  The only downside vs the TLC5940 
// is that it's only available in an SMD package, whereas the TLC5940 is 
// available in easy-to-solder DIP format.  The DIP 5940 is longer being 
// manufactured, but it's still easy to find old stock; when those run out,
// though, and the choice is between SMD 5940 and 59116, the 59116 will be
// the clear winner.
//

#ifndef _TLC59116_H_
#define _TLC59116_H_

#include "mbed.h"
#include "BitBangI2C.h"

// Which I2C class are we using?  We use this to switch between
// BitBangI2C and MbedI2C for testing and debugging.
#define I2C_Type BitBangI2C

// register constants
struct TLC59116R
{
    // control register bits
    static const uint8_t CTL_AIALL = 0x80;         // auto-increment mode, all registers
    static const uint8_t CTL_AIPWM = 0xA0;         // auto-increment mode, PWM registers only
    static const uint8_t CTL_AICTL = 0xC0;         // auto-increment mode, control registers only
    static const uint8_t CTL_AIPWMCTL = 0xE0;      // auto-increment mode, PWM + control registers only

    // register addresses
    static const uint8_t REG_MODE1 = 0x00;         // MODE1
    static const uint8_t REG_MODE2 = 0x01;         // MODE2
    static const uint8_t REG_PWM0 = 0x02;          // PWM 0
    static const uint8_t REG_PWM1 = 0x03;          // PWM 1
    static const uint8_t REG_PWM2 = 0x04;          // PWM 2
    static const uint8_t REG_PWM3 = 0x05;          // PWM 3
    static const uint8_t REG_PWM4 = 0x06;          // PWM 4
    static const uint8_t REG_PWM5 = 0x07;          // PWM 5
    static const uint8_t REG_PWM6 = 0x08;          // PWM 6
    static const uint8_t REG_PWM7 = 0x09;          // PWM 7
    static const uint8_t REG_PWM8 = 0x0A;          // PWM 8
    static const uint8_t REG_PWM9 = 0x0B;          // PWM 9
    static const uint8_t REG_PWM10 = 0x0C;         // PWM 10
    static const uint8_t REG_PWM11 = 0x0D;         // PWM 11
    static const uint8_t REG_PWM12 = 0x0E;         // PWM 12
    static const uint8_t REG_PWM13 = 0x0F;         // PWM 13
    static const uint8_t REG_PWM14 = 0x10;         // PWM 14
    static const uint8_t REG_PWM15 = 0x11;         // PWM 15
    static const uint8_t REG_GRPPWM = 0x12;        // Group PWM duty cycle
    static const uint8_t REG_GRPFREQ = 0x13;       // Group frequency register
    static const uint8_t REG_LEDOUT0 = 0x14;       // LED driver output status register 0
    static const uint8_t REG_LEDOUT1 = 0x15;       // LED driver output status register 1
    static const uint8_t REG_LEDOUT2 = 0x16;       // LED driver output status register 2
    static const uint8_t REG_LEDOUT3 = 0x17;       // LED driver output status register 3
    
    // MODE1 bits
    static const uint8_t MODE1_AI2 = 0x80;         // auto-increment mode enable
    static const uint8_t MODE1_AI1 = 0x40;         // auto-increment bit 1
    static const uint8_t MODE1_AI0 = 0x20;         // auto-increment bit 0
    static const uint8_t MODE1_OSCOFF = 0x10;      // oscillator off
    static const uint8_t MODE1_SUB1 = 0x08;        // subaddress 1 enable
    static const uint8_t MODE1_SUB2 = 0x04;        // subaddress 2 enable
    static const uint8_t MODE1_SUB3 = 0x02;        // subaddress 3 enable
    static const uint8_t MODE1_ALLCALL = 0x01;     // all-call enable
    
    // MODE2 bits
    static const uint8_t MODE2_EFCLR = 0x80;       // clear error status flag
    static const uint8_t MODE2_DMBLNK = 0x20;      // group blinking mode
    static const uint8_t MODE2_OCH = 0x08;         // outputs change on ACK (vs Stop command)
    
    // LEDOUTn states
    static const uint8_t LEDOUT_OFF = 0x00;        // driver is off
    static const uint8_t LEDOUT_ON = 0x01;         // fully on
    static const uint8_t LEDOUT_PWM = 0x02;        // individual PWM control via PWMn register
    static const uint8_t LEDOUT_GROUP = 0x03;      // PWM control + group dimming/blinking via PWMn + GRPPWM
};
   

// Individual unit object.  We create one of these for each unit we
// find on the bus.  This keeps track of the state of each output on
// a unit so that we can update outputs in batches, to reduce the 
// amount of time we spend in I2C communications during rapid updates.
struct TLC59116Unit
{
    TLC59116Unit()
    {
        // start inactive, since we haven't been initialized yet
        active = false;
        
        // set all brightness levels to 0 intially
        memset(bri, 0, sizeof(bri));
        
        // mark all outputs as dirty to force an update after initializing
        dirty = 0xFFFF;
    }
    
    // initialize
    void init(int addr, I2C_Type &i2c)
    {        
        // set all output drivers to individual PWM control
        const uint8_t all_pwm = 
            TLC59116R::LEDOUT_PWM 
            | (TLC59116R::LEDOUT_PWM << 2)
            | (TLC59116R::LEDOUT_PWM << 4)
            | (TLC59116R::LEDOUT_PWM << 6);
        static const uint8_t buf[] = { 
            TLC59116R::REG_LEDOUT0 | TLC59116R::CTL_AIALL,
            all_pwm, 
            all_pwm, 
            all_pwm, 
            all_pwm 
        };
        int err = i2c.write(addr << 1, buf, sizeof(buf));

        // turn on the oscillator
        static const uint8_t buf2[] = { 
            TLC59116R::REG_MODE1, 
            TLC59116R::MODE1_AI2 | TLC59116R::MODE1_ALLCALL 
        };
        err |= i2c.write(addr << 1, buf2, sizeof(buf));
        
        // mark the unit as active if the writes succeeded
        active = !err;
    }
    
    // Set an output
    void set(int idx, int val)
    {
        // validate the index
        if (idx >= 0 && idx <= 15)
        {
            // record the new brightness
            bri[idx] = val;
            
            // set the dirty bit
            dirty |= 1 << idx;
        }
    }
    
    // Get an output's current value
    int get(int idx) const
    {
        return idx >= 0 && idx <= 15 ? bri[idx] : -1;
    }
    
    // Send I2C updates
    void send(int addr, I2C_Type &i2c)
    {
        // Scan all outputs.  I2C sends are fairly expensive, so we
        // minimize the send time by using the auto-increment mode.
        // Optimizing this is a bit tricky.  Suppose that the outputs
        // are in this state, where c represents a clean output and D
        // represents a dirty output:
        //
        //    cccDcDccc...
        //
        // Clearly we want to start sending at the first dirty output
        // so that we don't waste time sending the three clean bytes
        // ahead of it.  However, do we send output[3] as one chunk
        // and then send output[5] as a separate chunk, or do we send
        // outputs [3],[4],[5] as a single block to take advantage of
        // the auto-increment mode?  Based on I2C bus timing parameters,
        // the answer is that it's cheaper to send this as a single
        // contiguous block [3],[4],[5].  The reason is that the cost
        // of starting a new block is a Stop/Start sequence plus another
        // register address byte; the register address byte costs the
        // same as a data byte, so the extra Stop/Start of the separate
        // chunk approach makes the single continguous send cheaper. 
        // But how about this one?:
        //
        //   cccDccDccc...
        //
        // This one is cheaper to send as two separate blocks.  The
        // break costs us a Start/Stop plus a register address byte,
        // but the Start/Stop is only about 25% of the cost of a data
        // byte, so Start/Stop+Register Address is cheaper than sending
        // the two clean data bytes sandwiched between the dirty bytes.
        //
        // So: we want to look for sequences of contiguous dirty bytes
        // and send those as a chunk.  We furthermore will allow up to
        // one clean byte in the midst of the dirty bytes.
        uint8_t buf[17];
        int n = 0;
        for (int i = 0, bit = 1 ; i < 16 ; ++i, bit <<= 1)
        {
            // If this one is dirty, include it in the set of outputs to
            // send to the chip.  Also include this one if it's clean
            // and the outputs on both sides are dirty - see the notes
            // above about optimizing for the case where we have one clean
            // output surrounded by dirty outputs.
            if ((dirty & bit) != 0)
            {
                // it's dirty - add it to the dirty set under construction
                buf[++n] = bri[i];
            }
            else if (n != 0 && n < 15 && (dirty & (bit << 1)) != 0)
            {
                // this one is clean, but the one before and the one after
                // are both dirty, so keep it in the set anyway to take
                // advantage of the auto-increment mode for faster sends
                buf[++n] = bri[i];
            }
            else
            {
                // This one is clean, and it's not surrounded by dirty
                // outputs.  If the set of dirty outputs so far has any
                // members, send them now.
                if (n != 0)
                {
                    // set the starting register address, including the
                    // auto-increment flag, and write the block
                    buf[0] = (TLC59116R::REG_PWM0 + i - n) | TLC59116R::CTL_AIALL;
                    i2c.write(addr << 1, buf, n + 1);
                    
                    // empty the set
                    n = 0;
                }
            }
        }
        
        // if we finished the loop with dirty outputs to send, send them
        if (n != 0)
        {
            // fill in the starting register address, and write the block
            buf[0] = (TLC59116R::REG_PWM15 + 1 - n) | TLC59116R::CTL_AIALL;
            i2c.write(addr << 1, buf, n + 1);
        }
        
        // all outputs are now clean
        dirty = 0;
    }
    
    // Is the unit active?  If we have trouble writing a unit,
    // we can mark it inactive so that we know to stop wasting
    // time writing to it, and so that we can re-initialize it
    // if it comes back on later bus scans.
    bool active;
    
    // Output states.  This records the latest brightness level
    // for each output as set by the client.  We don't actually
    // send these values to the physical unit until the client 
    // tells us to do an I2C update.
    uint8_t bri[16];
    
    // Dirty output mask.  Whenever the client changes an output,
    // we record the new brightness in bri[] and set the 
    // corresponding bit here to 1.  We use these bits to determine
    // which outputs to send during each I2C update.
    uint16_t dirty;
};

// TLC59116 public interface.  This provides control over a collection
// of units connected on a common I2C bus.
class TLC59116
{
public:
    // Initialize.  The address given is the configurable part
    // of the address, 0x0000 to 0x000F.
    TLC59116(PinName sda, PinName scl, PinName reset)
        : i2c(sda, scl, true), reset(reset)
    {
        // Use the fastest I2C speed possible, since we want to be able
        // to rapidly update many outputs at once.  The TLC59116 can run 
        // I2C at up to 1MHz.
        i2c.frequency(1000000);
        
        // assert !RESET until we're ready to go
        this->reset.write(0);
        
        // there are no units yet
        memset(units, 0, sizeof(units));
        nextUpdate = 0;
    }
    
    void init()
    {
        // un-assert reset
        reset.write(1);
        wait_us(10000);
        
        // scan the bus for new units
        scanBus();
    }
    
    // scan the bus
    void scanBus()
    {
        // scan each possible address
        for (int i = 0 ; i < 16 ; ++i)
        {
            // Address 8 and 11 are reserved - skip them
            if (i == 8 || i == 11)
                continue;
                
            // Try reading register REG_MODE1
            int addr = I2C_BASE_ADDR | i;
            TLC59116Unit *u = units[i];
            if (readReg8(addr, TLC59116R::REG_MODE1) >= 0)
            {
                // success - if the slot wasn't already populated, allocate
                // a unit entry for it
                if (u == 0)
                    units[i] = u = new TLC59116Unit();
                    
                // if the unit isn't already marked active, initialize it
                if (!u->active)
                    u->init(addr, i2c);
            }
            else
            {
                // failed - if the unit was previously active, mark it
                // as inactive now
                if (u != 0)
                    u->active = false;
            }
        }
    }
    
    // set an output
    void set(int unit, int output, int val)
    {
        if (unit >= 0 && unit <= 15)
        {
            TLC59116Unit *u = units[unit];
            if (u != 0)
                u->set(output, val);
        }
    }
    
    // get an output's current value
    int get(int unit, int output)
    {
        if (unit >= 0 && unit <= 15)
        {
            TLC59116Unit *u = units[unit];
            if (u != 0)
                return u->get(output);
        }
        
        return -1;
    }
    
    // Send I2C updates to the next unit.  The client must call this 
    // periodically to send pending updates.  We only update one unit on 
    // each call to ensure that the time per cycle is relatively constant
    // (rather than scaling with the number of chips).
    void send()
    {
        // look for a dirty unit
        for (int i = 0, n = nextUpdate ; i < 16 ; ++i, ++n)
        {
            // wrap the unit number
            n &= 0x0F;
            
            // if this unit is populated and dirty, it's the one to update
            TLC59116Unit *u = units[n];
            if (u != 0 && u->dirty != 0)
            {
                // it's dirty - update it 
                u->send(I2C_BASE_ADDR | n, i2c);
                
                // We only update one on each call, so we're done.
                // Remember where to pick up again on the next update() 
                // call, and return.
                nextUpdate = n + 1;
                return;
            }
        }
    }
    
    // Enable/disable all outputs
    void enable(bool f)
    {
        // visit each populated unit
        for (int i = 0 ; i < 16 ; ++i)
        {
            // if this unit is populated, enable/disable it
            TLC59116Unit *u = units[i];
            if (u != 0)
            {
                // read the current MODE1 register
                int m = readReg8(I2C_BASE_ADDR | i, TLC59116R::REG_MODE1);
                if (m >= 0)
                {
                    // Turn the oscillator off to disable, on to enable. 
                    // Note that the bit is kind of backwards:  SETTING the 
                    // OSC bit turns the oscillator OFF.
                    if (f)
                        m &= ~TLC59116R::MODE1_OSCOFF; // enable - clear the OSC bit
                    else
                        m |= TLC59116R::MODE1_OSCOFF;  // disable - set the OSC bit
                        
                    // update MODE1
                    writeReg8(I2C_BASE_ADDR | i, TLC59116R::REG_MODE1, m);
                }
            }
        }
    }
    
protected:
    // TLC59116 base I2C address.  These chips use an address of
    // the form 110xxxx, where the the low four bits are set by
    // external pins on the chip.  The top three bits are always
    // the same, so we construct the full address by combining 
    // the upper three fixed bits with the four-bit unit number.
    //
    // Note that addresses 1101011 (0x6B) and 1101000 (0x68) are
    // reserved (for SWRSTT and ALLCALL, respectively), and can't
    // be used for configured device addresses.
    static const uint8_t I2C_BASE_ADDR = 0x60;
    
    // Units.  We populate this with active units we find in
    // bus scans.  Note that units 8 and 11 can't be used because
    // of the reserved ALLCALL and SWRST addresses, but we allocate
    // the slots anyway to keep indexing simple.
    TLC59116Unit *units[16];
    
    // next unit to update
    int nextUpdate;

    // read 8-bit register; returns the value read on success, -1 on failure
    int readReg8(int addr, uint16_t registerAddr)
    {
        // write the request - register address + auto-inc mode
        uint8_t data_write[1];
        data_write[0] = registerAddr | TLC59116R::CTL_AIALL;
        if (i2c.write(addr << 1, data_write, 1, true))
            return -1;
    
        // read the result
        uint8_t data_read[1];
        if (i2c.read(addr << 1, data_read, 1))
            return -1;
        
        // return the result
        return data_read[0];
    }
 
    // write 8-bit register; returns true on success, false on failure
    bool writeReg8(int addr, uint16_t registerAddr, uint8_t data)
    {
        uint8_t data_write[2];
        data_write[0] = registerAddr | TLC59116R::CTL_AIALL;
        data_write[1] = data;
        return !i2c.write(addr << 1, data_write, 2);
    }
 
    // I2C bus interface
    I2C_Type i2c;
    
    // reset pin (active low)
    DigitalOut reset;
};

#endif
