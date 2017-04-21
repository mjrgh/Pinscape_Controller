// Plunger Sensor Interface
//
// This module defines the abstract interface to the plunger sensors.
// We support several different physical sensor types, so we need a
// common interface for use in the main code.
//
// In case it's helpful in developing code for new sensor types, I've
// measured the maximum instantaneous speed of a plunger at .175 inches
// per millisecond, or 4.46 mm/ms.  (I measured that with an AEDR-8300;
// see that code for more details.)
//

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
    PlungerSensor(int nativeScale)
    {
        // use the joystick scale as our native scale by default
        this->nativeScale = nativeScale;
        
        // figure the scaling factor
        scalingFactor = (65535UL*65536UL) / nativeScale;
        
        // presume no jitter filter
        jfWindow = 0;
        
        // initialize the jitter filter
        jfLo = jfHi = jfLast = 0;
    }

    // ---------- Abstract sensor interface ----------
    
    // Initialize the physical sensor device.  This is called at startup
    // to set up the device for first use.
    virtual void init() { }
    
    // Auto-zero the plunger.  Relative sensor types, such as quadrature
    // sensors, can lose sync with the absolute position over time if they
    // ever miss any motion.  We can automatically correct for this by
    // resetting to the park position after periods of inactivity.  It's
    // usually safe to assume that the plunger is at the park position if it 
    // hasn't moved in a long time, since the spring always returns it to 
    // that position when it isn't being manipulated.  The main loop monitors
    // for motion, and calls this after a long enough time goes by without
    // seeing any movement.  Sensor types that are inherently absolute
    // (TSL1410, potentiometers) shouldn't do anything here.
    virtual void autoZero() { }

    // Is the sensor ready to take a reading?  The optical sensor requires
    // a fairly long time (2.5ms) to transfer the data for each reading, but 
    // this is done via DMA, so we can carry on other work while the transfer
    // takes place.  This lets us poll the sensor to see if it's still busy
    // working on the current reading's data transfer.
    virtual bool ready() { return true; }
    
    // Read the sensor position, if possible.  Returns true on success,
    // false if it wasn't possible to take a reading.  On success, fills
    // in 'r' with the current reading and timestamp and returns true.
    // Returns false if a reading couldn't be taken.
    //
    // r.pos is set to the normalized position reading, and r.t is set to
    // the timestamp of the reading.
    //
    // The normalized position is the sensor reading, corrected for jitter,
    // and adjusted to the abstract 0x0000..0xFFFF range.
    // 
    // The timestamp is the time the sensor reading was taken, relative to
    // an arbitrary zero point.  The arbitrary zero point makes this useful
    // only for calculating the time between readings.  Note that the 32-bit
    // timestamp rolls over about every 71 minutes, so it should only be
    // used for time differences between readings taken fairly close together.
    // In practice, the higher level code only uses this for a few consecutive
    // readings to calculate (nearly) instantaneous velocities, so the time
    // spans are only tens of milliseconds.
    //
    // Timing requirements:  for best results, readings should be taken
    // in well under 5ms.  The release motion of the physical plunger
    // takes from 30ms to 50ms, so we need to collect samples much faster
    // than that to avoid aliasing during the bounce.
    bool read(PlungerReading &r)
    {
        // get the raw reading
        if (readRaw(r))
        {
            // process it through the jitter filter
            //$$$ r.pos = jitterFilter(r.pos);
            
            // adjust to the abstract scale via the scaling factor
            r.pos = uint16_t(uint32_t((scalingFactor * r.pos) + 32768) >> 16);
            
            // success
            return true;
        }
        else
        {
            // no reading is available
            return false;
        }
    }

    // Get a raw plunger reading.  This gets the raw sensor reading with
    // timestamp, without jitter filtering and without any scale adjustment.
    virtual bool readRaw(PlungerReading &r) = 0;
    
    // Begin calibration.  The main loop calls this when the user activates
    // calibration mode.  Sensors that work in terms of relative positions,
    // such as quadrature-based sensors, can use this to set the reference
    // point for the park position internally.
    virtual void beginCalibration() { }
    
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
        if (readRaw(r))
        {
            // success - apply the jitter filter
            pos = jitterFilter(r.pos);
        }
        
        // Send the common status information, indicating 0 pixels, standard
        // sensor orientation, and zero processing time.  Non-imaging sensors 
        // usually don't have any way to detect the orientation, so assume
        // normal orientation (flag 0x01).  Also assume zero analysis time,
        // as most non-image sensors don't have to do anything CPU-intensive
        // with the raw readings (all they usually have to do is scale the
        // value to the abstract reporting range).
        js.sendPlungerStatus(0, pos, 0x01, getAvgScanTime(), 0);
        js.sendPlungerStatus2(nativeScale, jfLo, jfHi, r.pos, 0);
    }
    
    // Get the average sensor scan time in microseconds
    virtual uint32_t getAvgScanTime() = 0;
        
    // Apply the jitter filter
    int jitterFilter(int pos)
    {
        // Check to see where the new reading is relative to the
        // current window
        if (pos < jfLo)
        {
            // the new position is below the current window, so move
            // the window down such that the new point is at the bottom 
            // of the window
            jfLo = pos;
            jfHi = pos + jfWindow;
            jfLast = pos;
            return pos;
        }
        else if (pos > jfHi)
        {
            // the new position is above the current window, so move
            // the window up such that the new point is at the top of
            // the window
            jfHi = pos;
            jfLo = pos - jfWindow;
            jfLast = pos;
            return pos;
        }
        else
        {
            // the new position is inside the current window, so repeat
            // the last reading
            return jfLast;
        }
    }
    
    // Set the jitter filter window size.  This is specified in native
    // sensor units.
    void setJitterWindow(int w)
    {
        // set the new window size
        jfWindow = w;
        
        // reset the running window
        jfHi = jfLo = jfLast;
    }
        
protected:
    // Native scale of the device.  This is the scale used for the position
    // reading in status reports.  This lets us report the position in the
    // same units the sensor itself uses, to avoid any rounding error 
    // converting to an abstract scale.
    //
    // Image edge detection sensors use the pixel size of the image, since
    // the position is determined by the pixel position of the shadow in
    // the image.  Quadrature sensors and other sensors that report the
    // distance in terms of physical distance units should use the number
    // of quanta in the approximate total plunger travel distance of 3".
    // For example, the VL6180X uses millimeter quanta, so can report
    // about 77 quanta over 3"; a quadrature sensor that reports at 1/300"
    // intervals has about 900 quanta over 3".  Absolute encoders (e.g., 
    // bar code sensors) should use the bar code range.
    //
    // Sensors that are inherently analog (e.g., potentiometers, analog
    // distance sensors) can quantize on any arbitrary scale.  In most cases,
    // it's best to use the same 0..65535 scale used for the regular plunger
    // reports.
    uint16_t nativeScale;
    
    // Scaling factor to convert native readings to abstract units on the
    // 0x0000..0xFFFF scale used in the higher level sensor-independent
    // code.  Multiply a raw sensor position reading by this value to
    // get the equivalent value on the abstract scale.  This is expressed 
    // as a fixed-point real number with a scale of 65536: calculate it as
    //
    //   (65535U*65536U) / (nativeScale - 1);
    uint32_t scalingFactor;
    
    // Jitter filtering
    int jfWindow;                // window size, in native sensor units
    int jfLo, jfHi;              // bounds of current window
    int jfLast;                  // last filtered reading
};

#endif /* PLUNGER_H */
