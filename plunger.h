// Plunger Sensor Interface
//
// This module defines the abstract interface to the plunger sensors.
// We support several different physical sensor types, so we need a
// common interface for use in the main code.

#ifndef PLUNGER_H
#define PLUNGER_H

// Plunger reading with timestamp
struct PlungerReading
{
    // Raw sensor reading, normalied to 0x0000..0xFFFF range
    int pos;
    
    // Rimestamp of reading, in microseconds, relative to an arbitrary
    // zero point.  Note that a 32-bit int can only represent about 71.5
    // minutes worth of microseconds, so this value is only meaningful
    // to compute a delta from other recent readings.  As long as two
    // readings are within 71.5 minutes of each other, the time difference
    // calculated from the timestamps using 32-bit math will be correct
    // *even if a rollover occurs* between the two readings, since the
    // calculation is done mod 2^32-1.
    uint32_t t;
};

class PlungerSensor
{
public:
    PlungerSensor() { }

    // ---------- Abstract sensor interface ----------
    
    // Initialize the physical sensor device.  This is called at startup
    // to set up the device for first use.
    virtual void init() = 0;

    // Read the sensor position, if possible.  Returns true on success,
    // false if it wasn't possible to take a reading.  On success, fills
    // in 'r' with the current reading.
    //
    // r.pos is set to the current raw sensor reading, normalized to the
    // range 0x0000..0xFFFF.  r.t is set to the timestamp of the reading,
    // in 
    //
    // Also sets 't' to the microsecond timestamp of the reading, if a
    // reading was successfully taken.  This timestamp is relative to an
    // arbitrary zero point and rolls over when it overflows its 32-bit
    // container (every 71.58 minutes).  Callers can use this to calculate
    // the interval between two readings (e.g., to figure the average 
    // velocity of the plunger between readings).
    //
    // Timing requirements:  for best results, readings should be taken
    // in well under 5ms.  There are two factors that go into this limit.
    // The first is the joystick report rate in the main loop.  We want
    // to send those as fast as possible to avoid any perceptible control 
    // input lag in the pinball emulation.  "As fast as possible" is about
    // every 10ms - that's VP's approximate sampling rate, so any faster
    // wouldn't do any good, and could even slow things down by adding CPU 
    // load in the Windows drivers handling the extra messages.  The second
    // is the speed of the plunger motion.  During release events, the
    // plunger moves in a sinusoidal pattern (back and forth) as it reaches
    // the extreme of its travel and bounces back off the spring.  To
    // resolve this kind of cyclical motion accurately, we have to take
    // samples much faster than the cycle period - otherwise we encounter
    // an effect known as aliasing, where we mistake a bounce for a small
    // forward motion.  Tests with real plungers indicate that the bounce
    // period is on the order of 10ms, so ideally we'd like to take
    // samples much faster than that.
    //
    // Returns true on success, false on failure.  Returning false means
    // that it wasn't possible to take a valid reading.
    virtual bool read(PlungerReading &r) = 0;
    
    // Send an exposure report to the host, via the joystick interface.  This
    // is for image sensors, and can be omitted by other sensor types.  For
    // image sensors, this takes one exposure and sends all pixels to the host
    // through special joystick reports.  This is used by tools on the host PC
    // to let the user view the low-level sensor pixel data, which can be
    // helpful during installation to adjust the sensor positioning and light
    // source.
    //
    // Flag bits:
    //   0x01  -> low res scan (default is high res scan)
    //
    // Visualization modes:
    //   0  -> raw pixels
    //   1  -> processed pixels (noise reduction, etc)
    //   2  -> exaggerated contrast mode
    //   3  -> edge visualization
    //
    // If processed mode is selected, the sensor should apply any pixel
    // processing it normally does when taking a plunger position reading,
    // such as exposure correction, noise reduction, etc.  In raw mode, we
    // simply send the pixels as read from the sensor.  Both modes are useful
    // in setting up the physical sensor.
    virtual void sendExposureReport(class USBJoystick &js, uint8_t flags, uint8_t visMode) { }
        
protected:
};

#endif /* PLUNGER_H */
