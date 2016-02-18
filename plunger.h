// Plunger Sensor Interface
//
// This module defines the abstract interface to the plunger sensors.
// We support several different physical sensor types, so we need a
// common interface for use in the main code.
//

#ifndef PLUNGER_H
#define PLUNGER_H

class PlungerSensor
{
public:

    PlungerSensor() { }
    virtual ~PlungerSensor() { }
    
    // Initialize the physical sensor device.  This is called at startup
    // to set up the device for first use.
    virtual void init() = 0;

    // Read the sensor position.  Sets 'pos' to the current plunger
    // position registered on the sensor, normalized to a 16-bit unsigned
    // integer (0x0000 to 0xFFFF).  0x0000 represents the maximum forward
    // position, and 0xFFFF represents the maximum retracted position.
    // The result returned by this routing isn't calibrated; it simply
    // reflects the raw sensor reading.
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
    virtual bool read(uint16_t &pos) = 0;
    
    // $$$ DEPRECATED - left in during transition to new design
    bool lowResScan(float &pos) 
    {
        uint16_t fpos;
        if (read(fpos)) 
        {
            pos = fpos / 65535.0;
            return true;
        }
        else 
            return false;
    }
    bool highResScan(float &pos) { return lowResScan(pos); }

    // Send an exposure report to the host, via the joystick interface.  This
    // is for image sensors, and can be omitted by other sensor types.  For
    // image sensors, this takes one exposure and sends all pixels to the host
    // through special joystick reports.  This is used by tools on the host PC
    // to let the user view the low-level sensor pixel data, which can be
    // helpful during installation to adjust the sensor positioning and light
    // source.
    //
    // Mode bits:
    //   0x01  -> send processed pixels (default is raw pixels)
    //   0x02  -> low res scan (default is high res scan)
    //
    // If processed mode is selected, the sensor should apply any pixel
    // processing it normally does when taking a plunger position reading,
    // such as exposure correction, noise reduction, etc.  In raw mode, we
    // simply send the pixels as read from the sensor.  Both modes are useful
    // in setting up the physical sensor.
    virtual void sendExposureReport(class USBJoystick &js, uint8_t mode) { }
};

#endif /* PLUNGER_H */
