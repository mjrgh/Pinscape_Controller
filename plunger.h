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
    
    // Is the sensor ready to take a reading?  The optical sensor requires
    // a fairly long time (2.5ms) to transfer the data for each reading, but 
    // this is done via DMA, so we can carry on other work while the transfer
    // takes place.  This lets us poll the sensor to see if it's still busy
    // working on the current reading's data transfer.
    virtual bool ready() const { return true; }

    // Read the sensor position, if possible.  Returns true on success,
    // false if it wasn't possible to take a reading.  On success, fills
    // in 'r' with the current reading.
    //
    // r.pos is set to the current raw sensor reading, normalized to the
    // range 0x0000..0xFFFF.  r.t is set to the timestamp of the reading,
    // in microseconds.
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
    
    // Send a sensor status report to the host, via the joystick interface.
    // This provides some common information for all sensor types, and also
    // includes a full image snapshot of the current sensor pixels for
    // imaging sensor types.
    //
    // The default implementation here sends the common information
    // packet, with the pixel size set to 0.
    //
    // 'flags' is a combination of bit flags:
    //   0x01  -> low-res scan (default is high res scan)
    //
    // Low-res scan mode means that the sensor should send a scaled-down
    // image, at a reduced size determined by the sensor subtype.  The
    // default if this flag isn't set is to send the full image, at the
    // sensor's native pixel size.  The low-res version is a reduced size
    // image in the normal sense of scaling down a photo image, keeping the
    // image intact but at reduced resolution.  Note that low-res mode
    // doesn't affect the ongoing sensor operation at all.  It only applies
    // to this single pixel report.  The purpose is simply to reduce the USB 
    // transmission time for the image, to allow for a faster frame rate for 
    // displaying the sensor image in real time on the PC.  For a high-res
    // sensor like the TSL1410R, sending the full pixel array by USB takes 
    // so long that the frame rate is way below regular video rates.
    //
    // 'exposureTime' is the amount of extra time to add to the exposure,
    // in 100us (.1ms) increments.  For imaging sensors, the frame we report
    // is exposed for the minimum exposure time plus this added time.  This
    // allows the host to take readings at different exposure levels for
    // calibration purposes.  Non-imaging sensors ignore this.
    virtual void sendStatusReport(
        class USBJoystick &js, uint8_t flags, uint8_t exposureTime)
    {
        // read the current position
        int pos = 0xFFFF;
        PlungerReading r;
        if (read(r))
        {
            // success - scale it to 0..4095 (the generic scale used
            // for non-imaging sensors)
            pos = int(r.pos*4095L / 65535L);
        }
        
        // Send the common status information, indicating 0 pixels, standard
        // sensor orientation, and zero processing time.  Non-imaging sensors 
        // usually don't have any way to detect the orientation, so they have 
        // to rely on being installed in a pre-determined direction.  Non-
        // imaging sensors usually have negligible analysis time (the only
        // "analysis" is usually nothing more than a multiply to rescale an 
        // ADC sample), so there's no point in collecting actual timing data; 
        // just report zero.
        js.sendPlungerStatus(0, pos, 1, getAvgScanTime(), 0);
    }
    
    // Get the average sensor scan time in microseconds
    virtual uint32_t getAvgScanTime() = 0;
        
protected:
};

#endif /* PLUNGER_H */
