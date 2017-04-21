// AEDR-8300-1K2 optical encoder / generic quadrature sensor plunger 
// implementation
//
// This class implements the Pinscape plunger interface for the 
// AEDR-8300-1K2 optical encoder in particular, and quadrature sensors 
// in general.  The code was written specifically for the AEDR-8300-1K2,
// but it should work with any other quadrature sensor that's electrically 
// compatible and that doesn't exceed the maximum interrupt rate we can 
// handle on the KL25Z.  To be electrically compatible, the device must 
// be 3.3V compatible, have logic type outputs (basically square waves
// for the signals), and provide two outputs 90 degrees out of phase.  
// The maximum interrupt rate that the KL25Z can handle (with our 
// FastInterruptIn class) is about 150 kHz.
//
// A quadrature sensor works by detecting transitions along a bar-coded 
// scale.  Most position encoders (including the AEDR-8300) are optical,
// but the same principle can be used with other technologies, such as
// magnetic pole strips.  Whatever the underlying physical "bar" type,
// the device detects transitions between the bars and the spaces between
// the bars and relays them to the microcontroller via its outputs.  A
// quadrature device actually consists of two such sensors, slightly offset
// from each other relative to the direction of motion of the scale, so 
// that their bar transitions are 90 degrees out of phase.  The phase
// shift in the two signals is what allows the microcontroller to sense
// the direction of motion.  The controller figures the current position
// by counting bar transitions (incrementing the count when moving in one
// direction and decrement it in the other direction), so it knows the 
// location at any given time as an offset in units of bar widths from the
// starting position.  The position reading is always relative, because
// we can only count up or down from the initial point.
//
// In many applications involving quadrature sensors, the relative 
// quadrature reading is augmented with a separate sensor for absolute 
// positioning.  This is usually something simple and low-res, like an 
// end-of-stroke switch or a zero-crossing switch.  The idea is that you 
// use the low-res absolute sensor to tell when you're at a known reference 
// point, and then use the high-res quadrature data to get the precise 
// location relative to the reference point.  To keep things simple, we 
// don't use any such supplemental absolute sensor.  It's not really 
// necessary for a plunger, because a plunger has the special property 
// that it always returns to the same point when not being manipulated.  
// It's almost as good as having a sensor at the park position, because
// even though we can't know for sure the plunger is there at any given
// time, it's a good bet that that's where it is at startup and any time
// we haven't seen any motion in a while.  Note that we could easily add 
// support in the software for some kind of absolute sensing if it became 
// desirable; the only challenge is the complexity it would add to the
// physical system.
//
// The AEDR-8300 lets us collect some very precise data on the 
// instantaneous speed of the plunger thanks to its high resolution and
// real-time position updates.  The shortest observed time between pulses 
// (so far, with my test rig) is 19us.  Pulses are generated at 4 per
// bar, with bars at 75 per inch, yielding 300 pulses per inch.  The 19us
// pulse time translates to an instantaneous plunger speed of 0.175 
// inches/millisecond, or 4.46 mm/ms, or 4.46 m/s, or 9.97 mph.
//
// The peak interrupt rate of 19us is well within the KL25Z's comfort
// zone, as long as we take reasonable measures to minimize latency.  In
// particular, we have to elevate the GPIO port IRQ priority above all
// other hardware interrupts.  That's vital because there are some
// relatively long-running interrupt handlers in the system, particularly
// the USB handlers and the microsecond timer.  It's also vital to keep
// other GPIO interrupt handlers very fast, since the ports all share
// a priority level and thus can't preempt one another.  Fortunately, the
// rest of the Pinscape system make very little use of GPIO interrupts;
// the only current use is in the IR receiver, and that code is designed 
// to do minimal work in IRQ context.
//
// We use our custom FastInterruptIn class instead of the original mbed
// InterruptIn.  FastInterruptIn gives us a modest speed improvement: it
// has a measured overhead time per interrupt of about 6.5us compared with
// the mbed libary's 8.9us, which gives us a maximum interrupt rate of
// about 159kHz vs mbed's 112kHz.  The AEDR-8300's maximum 19us is well
// within both limits, but FastInterruptIn gives us a little more headroom
// for substituting other sensors with higher pulse rates.
//

#ifndef _QUADSENSOR_H_
#define _QUADSENSOR_H_

#include "FastInterruptIn.h"

class PlungerSensorQuad: public PlungerSensor
{
public:
    // Construct.
    //
    // 'dpi' is the approximate number of dots per inch of linear travel
    // that the sensor can distinguish.  This is equivalent to the number
    // of pulses it generates per inch.  This doesn't have to be exact,
    // since the main loop rescales it anyway via calibration.  But it's
    // helpful to have the approximate figure so that we can scale the
    // raw data readings appropriately for the interface datatypes.
    //
    // For the native scale, we'll assume a 4" range at our dpi rating.
    // The actual plunger travel is constrainted to about a 3" range, but
    // we want to leave a little extra padding to reduce the chances of
    // going out of range in unusual situations.
    PlungerSensorQuad(int dpi, PinName pinA, PinName pinB) 
        : PlungerSensor(dpi*4),
          chA(pinA), chB(pinB)
    {   
        // Use 1" as the reference park position
        parkPos = dpi;
        
        // start at the park position
        pos = parkPos;
          
        // get the initial pin states
        st = (chA.read() ? 0x01 : 0x00) 
             | (chB.read() ? 0x02 : 0x00);
        
        // set up the interrupt handlers
        chA.rise(&PlungerSensorQuad::aUp, this);
        chA.fall(&PlungerSensorQuad::aDown, this);
        chB.rise(&PlungerSensorQuad::bUp, this);
        chB.fall(&PlungerSensorQuad::bDown, this);

        // start our sample timer with an arbitrary zero point of now
        timer.start();
    }
    
    // Auto-zero.  Return to the park position
    virtual void autoZero()
    {
        pos = parkPos;
    }
        
    // Begin calibration.  We can assume that the plunger is at the
    // park position when calibration starts.
    virtual void beginCalibration()
    {
        pos = parkPos;
    }
    
    // read the sensor
    virtual bool readRaw(PlungerReading &r)
    {
        // Get the current position in native units
        r.pos = pos;
        
        // Set the timestamp on the reading to right now.  Our internal
        // position counter reflects the position in real time, since it's
        // updated in the interrupt handlers for the change signals from
        // the sensor.
        r.t = timer.read_us();
        
        // success
        return true;
    }
    
    // figure the average scan time in microseconds
    virtual uint32_t getAvgScanTime() 
    { 
        // we're updated by interrupts rather than scanning, so our
        // "scan time" is exactly zero
        return 0;
    }
        
private:
    // interrupt inputs for our channel pins
    FastInterruptIn chA, chB;
    
    // current position - this is the cumulate counter for all
    // transitions so far
    int pos;
    
    // Park position.  This is essentially arbitrary, since our readings
    // are entirely relative, but for interface purposes we have to keep 
    // our raw readings positive.  We need an initial park position that's
    // non-zero so that plunger motion forward of the park position remains
    // positive.
    int parkPos;
    
    // Channel state on last read.  This is a bit vector combining
    // the two channel states:
    //   0x01 = channel A state
    //   0x02 = channel B state
    uint8_t st;
    
    // interrupt handlers
    static void aUp(void *obj) { 
        PlungerSensorQuad *self = (PlungerSensorQuad *)obj; 
        self->transition(self->st | 0x01); 
    }
    static void aDown(void *obj) { 
        PlungerSensorQuad *self = (PlungerSensorQuad *)obj; 
        self->transition(self->st & 0x02); 
    }
    static void bUp(void *obj) {
        PlungerSensorQuad *self = (PlungerSensorQuad *)obj; 
        self->transition(self->st | 0x02); 
    }
    static void bDown(void *obj) { 
        PlungerSensorQuad *self = (PlungerSensorQuad *)obj; 
        self->transition(self->st & 0x01); 
    }
    
    // Transition handler.  The interrupt handlers call this, so
    // it's critical that this run as fast as possible.  The observed
    // peak interrupt rate is one interrupt per 19us.  Fortunately, 
    // our work here is simple:  we just have to count the pulse in 
    // the appropriate direction according to the state transition 
    // that the pulse represents.  We can do this with a simple table 
    // lookup.
    inline void transition(int stNew)
    {
        // Transition matrix: this gives the direction of motion
        // when we switch from state dir[n] to state dir[n][m].
        // The state number is formed by the two-bit number B:A,
        // where each bit is 1 if the channel pulse is on and 0
        // if the channel pulse is off.  E.g., if chA is OFF and 
        // chB is ON, B:A = 1:0, so the state number is 0b10 = 2.
        // Slots with 'NV' are Not Valid: it's impossible to make 
        // this transition (unless we missed an interrupt).  'NC'
        // means No Change; these are the slots on the matrix
        // diagonal, which represent the same state on both input
        // and output.  Like NV transitions, NC transitions should
        // never happen, in this case because no interrupt should
        // be generated when nothing has changed.
        const int NV = 0, NC = 0;
        static const int dir[][4] = {
            { NC,  1, -1, NV },
            { -1, NC, NV,  1 },
            {  1, NV, NC, -1 },
            { NV, -1,  1, NC }
        };
        
        // increment or decrement the position counter by one notch, 
        // according to the direction of motion implied by the transition
        pos += dir[st][stNew];

        // the new state is now the current state
        st = stNew;
    }
    
    // timer for input timestamps
    Timer timer;
};

#endif
