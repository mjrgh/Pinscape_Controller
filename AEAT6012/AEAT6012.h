// AEAT-6012-A06 interface
//
// The Broadcom AEAT-6012-A06 is a magnetic absolute rotary position encoder
// with a 12-bit digital position output.  It encodes the rotational angle of
// magnet, which is meant to be attached to a shaft that's positioned 
// perpendicular to the sensor.  The 12-bit reporting means that the angle
// is resolved to one part in 4096 around one 360 degree rotation of the
// shaft, so each increment represents 360/4096 = 0.088 degrees of arc.
//
// For Pinscape purposes, we can use this sensor to track the position of
// a plunger by mechanically translating the linear motion of the plunger
// to rotational motion around a fixed point somewhere off the axis of the
// plunger:
//
//    =X=======================|===   <- plunger, X = connector attachment point
//      \
//       \                            <- connector between plunger and shaft
//        \
//         *                          <- rotating shaft, at a fixed position
//
// As the plunger moves, the angle of the connector relative to the fixed
// shaft position changes in a predictable way, so by measuring the rotational
// position of the shaft at any given time, we can infer the plunger's
// linear position.  The relationship between the plunger position and shaft
// angle isn't precisely linear - it's sinusoidal.  So we need to apply a
// little trigonometry to recover the linear position from the angle.
//
// The AEAT-6012-A06 has an extremely simple electronic interface.  It uses
// a three-wire serial protocol: CS (chip select), CLK (data clock), and
// DO (digital data out).  The data transmission is one-way - device to
// host - and simply consists of the 12-bit position reading.  There aren't
// any "commands" or other fancy business to deal with.  Between readings,
// CS is held high; to intiate a reading, hold CS low, then toggle CLK to
// clock out the bits.  The bits are clocked out from MSb to LSb.  After
// clocking out the 12 bits, we take CS high again to reset the cycle.
// There are also some timing requirements spelled out in the data sheet
// that we have to observe for minimum clock pulse time, time before DO 
// is valid, etc.
//
// There's a 10-bit variant of the sensor (AEAT-6010) that's otherwise 
// identical, so we've made the data size a parameter so that the code can
// be re-used for both sensor types (as well as any future variations with
// other resolutions).

template<int nBits> class AEAT601X
{
public:
    AEAT601X(PinName csPin, PinName clkPin, PinName doPin) :
        cs(csPin), clk(clkPin), DO(doPin)
    {
        // hold CS and CLK high between readings
        cs = 1;
        clk = 1;
    }
    
    // take a reading, returning an unsigned integer result from 0 to 2^bits-1
    int readAngle()
    {
        // Note on timings: the data sheet lists a number of minimum timing
        // parameters for the serial protocol.  The parameters of interest
        // here are all sub-microsecond, from 100ns to 500ns.  The mbed 
        // library doesn't have a nanosecond "wait", just the microsecond
        // wait, so we can't wait for precisely the minimum times.  But we
        // don't have to; the parameters are all minimum waits to ensure
        // that the sensor is ready for the next operation, so it's okay
        // to wait longer than the minimum.  And since we only have to move
        // a small number of bits (10-12 for the current sensor generation),
        // we don't have to be ruthlessly efficient about it; we can afford
        // to putter around for a leisurely microsecond at each step.  The 
        // total delay time for the 12-bit sensor even with the microsecond
        // delays only amounts to 25us, which is negligible for the plunger
        // read operation.
        
        // hold CS low for at least t[CLKFE] = 500ns per data sheet
        cs = 0;
        wait_us(1);
        
        // clock in the bits
        int result = 0;
        for (int i = 0; i < nBits; ++i)
        {
            // take clock low for >= T[CLK/2] = 500ns
            clk = 0;
            wait_us(1);
            
            // take clock high
            clk = 1;
            
            // wait for the data to become valid, T[DOvalid] = 375ns
            wait_us(1);
            
            // read the bit
            result <<= 1;
            result |= (DO ? 1 : 0);
        }
        
        // done - leave CS high between readings
        cs = 1;
        
        // The orientation in our mounting design reads the angle in the
        // reverse of the direction we want, so flip it.
        result = 4095 - result;
        
        // return the result
        return result;
    }
    
protected:
    // CS (chip select) pin
    DigitalOut cs;
    
    // CLK (serial clock) pin
    DigitalOut clk;
    
    // DO (serial data) pin
    DigitalIn DO;
};
