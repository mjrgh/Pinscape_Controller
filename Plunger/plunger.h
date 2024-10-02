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

#include "config.h"

// Plunger reading with timestamp
struct PlungerReading
{
    // Raw sensor reading, normalied to 0x0000..0xFFFF range
    int pos;
    
    // Timestamp of reading, in microseconds, relative to an arbitrary
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
        
        // presume normal orientation
        reverseOrientation = false;
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
        // fail if the hardware scan isn't ready
        if (!ready())
            return false;
        
        // get the raw reading
        if (readRaw(r))
        {
            // adjust for orientation
            r.pos = applyOrientation(r.pos);

            // process it through the jitter filter
            r.pos = postJitterFilter(r.pos);
            
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
    
    // Restore the saved calibration data from the configuration.  The main 
    // loop calls this at startup to let us initialize internals from the
    // saved calibration data.  This is called even if the plunger isn't 
    // calibrated, which is flagged in the config.
    virtual void restoreCalibration(Config &) { }
    
    // Begin calibration.  The main loop calls this when the user activates
    // calibration mode.  Sensors that work in terms of relative positions,
    // such as quadrature-based sensors, can use this to set the reference
    // point for the park position internally.
    virtual void beginCalibration(Config &) { }
    
    // End calibration.  The main loop calls this when calibration mode is
    // completed.
    virtual void endCalibration(Config &) { }
    
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
    virtual void sendStatusReport(class USBJoystick &js, uint8_t flags, int16_t speed)
    {
        // read the current position
        int pos = 0xFFFF;
        PlungerReading r;
        if (readRaw(r))
        {
            // adjust for reverse orientation
            r.pos = applyOrientation(r.pos);

            // success - apply the jitter filter
            pos = postJitterFilter(r.pos);
        }
        
        // Send the common status information, indicating 0 pixels, standard
        // sensor orientation, and zero processing time.  Non-imaging sensors 
        // usually don't have any way to detect the orientation, so assume
        // normal orientation (flag 0x01).  Also assume zero analysis time,
        // as most non-image sensors don't have to do anything CPU-intensive
        // with the raw readings (all they usually have to do is scale the
        // value to the abstract reporting range).
        js.sendPlungerStatus(0, pos, 0x01, getAvgScanTime(), 0, speed);
        js.sendPlungerStatus2(nativeScale, jfLo, jfHi, r.pos, 0);
    }
    
    // Set extra image integration time, in microseconds.  This is only 
    // meaningful for image-type sensors.  This allows the PC client to
    // manually adjust the exposure time for testing and debugging
    // purposes.
    virtual void setExtraIntegrationTime(uint32_t us) { }
    
    // Get the average sensor scan time in microseconds
    virtual uint32_t getAvgScanTime() = 0;
    
    // Apply the orientation filter.  The position is in unscaled
    // native sensor units.
    int applyOrientation(int pos)
    {
        return (reverseOrientation ? nativeScale - pos : pos);
    }
    
    // Post-filter a raw reading through the mitter filter.  Most plunger
    // sensor subclasses can use this default implementation, since the
    // jitter filter is usually applied to the raw position reading.
    // However, for some sensor types, it might be better to apply the
    // jitter filtering to the underlying physical sensor reading, before
    // readRaw() translates the reading into distance units.  In that
    // case, the subclass can override this to simply return the argument
    // unchanged.  This allows subclasses to use jitterFilter() if desired
    // on their physical sensor readings.  It's not either/or, though; a
    // subclass that overrides jitter post-filtering isn't could use an
    // entirely different noise filtering algorithm on its sensor data.
    virtual int postJitterFilter(int pos) { return jitterFilter(pos); }
        
    // Apply the jitter filter.  The position is in unscaled native 
    // sensor units.
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
            
            // figure the new position as the centerpoint of the new window
            jfLast = pos = (jfHi + jfLo)/2;
            return pos;
        }
        else if (pos > jfHi)
        {
            // the new position is above the current window, so move
            // the window up such that the new point is at the top of
            // the window
            jfHi = pos;
            jfLo = pos - jfWindow;

            // figure the new position as the centerpoint of the new window
            jfLast = pos = (jfHi + jfLo)/2;
            return pos;
        }
        else
        {
            // the new position is inside the current window, so repeat
            // the last reading
            return jfLast;
        }
    }
    
    // Process a configuration variable change.  'varno' is the
    // USB protocol variable number being updated; 'cfg' is the
    // updated configuration.
    virtual void onConfigChange(int varno, Config &cfg)
    {
        switch (varno)
        {
        case 19:
            // Plunger filters - jitter window and reverse orientation.
            setJitterWindow(cfg.plunger.jitterWindow);
            setReverseOrientation((cfg.plunger.reverseOrientation & 0x01) != 0);
            break;
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
    
    // Set reverse orientation
    void setReverseOrientation(bool f) { reverseOrientation = f; }
        
protected:
    // Native scale of the device.  This is the scale used for the position
    // reading in status reports.  This lets us report the position in the
    // same units the sensor itself uses, to avoid any rounding error 
    // converting to an abstract scale.
    //
    // The nativeScale value is the number of units in the range of raw
    // sensor readings returned from readRaw().  Raw readings thus have a
    // valid range of 0 to nativeScale-1.
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
    
    // Reverse the raw reading orientation.  If set, raw readings will be
    // switched to the opposite orientation.  This allows flipping the sensor
    // orientation virtually to correct for installing the physical device
    // backwards.
    bool reverseOrientation;
};


// --------------------------------------------------------------------------
//
// Generic image sensor interface for image-based plungers.
//
// This interface is designed to allow the underlying sensor code to work
// asynchronously to transfer pixels from the sensor into memory using
// multiple buffers arranged in a circular list.  We have a "ready" state,
// which lets the sensor tell us when a buffer is available, and we have
// the notion of "ownership" of the buffer.  When the client is done with
// a frame, it must realease the frame back to the sensor so that the sensor
// can use it for a subsequent frame transfer.
//
class PlungerSensorImageInterface
{
public:
    PlungerSensorImageInterface(int npix)
    {
        native_npix = npix;
    }
    
    // initialize the sensor
    virtual void init() = 0;

    // is the sensor ready?
    virtual bool ready() = 0;
    
    // Read the image.  This retrieves a pointer to the current frame
    // buffer, which is in memory space managed by the sensor.  This
    // MUST only be called when ready() returns true.  The buffer is
    // locked for the client's use until the client calls releasePix().
    // The client MUST call releasePix() when done with the buffer, so
    // that the sensor can reuse it for another frame.
    virtual void readPix(uint8_t* &pix, uint32_t &t) = 0;
    
    // Release the current frame buffer back to the sensor.  
    virtual void releasePix() = 0;
    
    // get the average sensor pixel scan time (the time it takes on average
    // to read one image frame from the sensor)
    virtual uint32_t getAvgScanTime() = 0;
    
    // Set the minimum integration time (microseconds)
    virtual void setMinIntTime(uint32_t us) = 0;
    
protected:
    // number of pixels on sensor
    int native_npix;
};


// ----------------------------------------------------------------------------
//
// Plunger base class for image-based sensors
//
template<typename ProcessResult>
class PlungerSensorImage: public PlungerSensor
{
public:
    PlungerSensorImage(PlungerSensorImageInterface &sensor, 
        int npix, int nativeScale, bool negativeImage = false) :
        PlungerSensor(nativeScale), 
        sensor(sensor),
        native_npix(npix),
        negativeImage(negativeImage),
        axcTime(0),
        extraIntTime(0)
    {
    }
    
    // initialize the sensor
    virtual void init() { sensor.init(); }

    // is the sensor ready?
    virtual bool ready() { return sensor.ready(); }
    
    // get the pixel transfer time
    virtual uint32_t getAvgScanTime() { return sensor.getAvgScanTime(); }

    // set extra integration time
    virtual void setExtraIntegrationTime(uint32_t us) { extraIntTime = us; }
    
    // read the plunger position
    virtual bool readRaw(PlungerReading &r)
    {
        // read pixels from the sensor
        uint8_t *pix;
        uint32_t tpix;
        sensor.readPix(pix, tpix);
        
        // process the pixels
        int pixpos;
        ProcessResult res;
        bool ok = process(pix, native_npix, pixpos, res);
        
        // release the buffer back to the sensor
        sensor.releasePix();
        
        // adjust the exposure time
        sensor.setMinIntTime(axcTime + extraIntTime);

        // if we successfully processed the frame, read the position
        if (ok)
        {            
            r.pos = pixpos;
            r.t = tpix;
        }
        
        // return the result
        return ok;
    }

    // Send a status report to the joystick interface.
    // See plunger.h for details on the arguments.
    virtual void sendStatusReport(USBJoystick &js, uint8_t flags, int16_t speed)
    {
        // start a timer to measure the processing time
        Timer pt;
        pt.start();

        // get pixels
        uint8_t *pix;
        uint32_t t;
        sensor.readPix(pix, t);

        // process the pixels and read the position
        int pos, rawPos;
        int n = native_npix;
        ProcessResult res;
        if (process(pix, n, rawPos, res))
        {
            // success - apply the post jitter filter
            pos = postJitterFilter(rawPos);
        }
        else
        {
            // report 0xFFFF to indicate that the position wasn't read
            pos = 0xFFFF;
            rawPos = 0xFFFF;
        }
        
        // adjust the exposure time
        sensor.setMinIntTime(axcTime + extraIntTime);

        // note the processing time
        uint32_t processTime = pt.read_us();
        
        // If a low-res scan is desired, reduce to a subset of pixels.  Ignore
        // this for smaller sensors (below 512 pixels)
        if ((flags & 0x01) && n >= 512)
        {
            // figure how many sensor pixels we combine into each low-res pixel
            const int group = 8;
            int lowResPix = n / group;
            
            // combine the pixels
            int src, dst;
            for (src = dst = 0 ; dst < lowResPix ; ++dst)
            {
                // average this block of pixels
                int a = 0;
                for (int j = 0 ; j < group ; ++j)
                    a += pix[src++];
                        
                // we have the sum, so get the average
                a /= group;

                // store the down-res'd pixel in the array
                pix[dst] = uint8_t(a);
            }
            
            // update the pixel count to the reduced array size
            n = lowResPix;
        }
        
        // figure the report flags
        int jsflags = 0;
        
        // add flags for the detected orientation: 0x01 for normal orientation,
        // 0x02 for reversed orientation; no flags if orientation is unknown
        int dir = getOrientation();
        if (dir == 1) 
            jsflags |= 0x01; 
        else if (dir == -1)
            jsflags |= 0x02;
            
        // send the sensor status report headers
        js.sendPlungerStatus(n, pos, jsflags, sensor.getAvgScanTime(), processTime, speed);
        js.sendPlungerStatus2(nativeScale, jfLo, jfHi, rawPos, axcTime);
        
        // send any extra status headers for subclasses
        extraStatusHeaders(js, res);
        
        // If we're not in calibration mode, send the pixels
        extern bool plungerCalMode;
        if (!plungerCalMode)
        {
            // If the sensor uses a negative image format (brighter pixels are
            // represented by lower numbers in the pixel array), invert the scale
            // back to a normal photo-positive scale, so that the client doesn't
            // have to know these details.
            if (negativeImage)
            {
                // Invert the photo-negative 255..0 scale to a normal,
                // photo-positive 0..255 scale.  This is just a matter of
                // calculating pos_pixel = 255 - neg_pixel for each pixel.
                //
                // There's a shortcut we can use here to make this loop go a
                // lot faster than the naive approach.  Note that 255 decimal
                // is 1111111 binary.  Subtracting any number in (0..255) from
                // 255 is the same as inverting the bits in the other number.
                // That is, 255 - X == ~X for all X in 0..255.  That's useful
                // because it means that we can compute (255-X) as a purely
                // bitwise operation, which means that we can perform it on
                // blocks of bytes instead of individual bytes.  On ARM, we
                // can perform bitwise operations four bytes at a time via
                // DWORD instructions.  This lets us compute (255-X) for N
                // bytes using N/4 loop iterations.
                //
                // One other small optimization we can apply is to notice that
                // ~X == X ^ ~0, and that X ^= ~0 can be performed with a
                // single ARM instruction.  So we can make the ARM C++ compiler
                // translate the loop body into just three instructions:  XOR 
                // with immediate data and auto-increment pointer, decrement 
                // the counter, and jump if not zero.  That's as fast we could
                // do it in hand-written assembly.  I clocked this loop at 
                // 60us for the 1536-pixel TCD1103 array.
                //
                // Note two important constraints:
                //
                //  - 'pix' must be aligned on a DWORD (4-byte) boundary.
                //    This is REQUIRED, because the XOR in the loop uses a
                //    DWORD memory operand, which will halt the MCU with a
                //    bus error if the pointer isn't DWORD-aligned.
                //
                //  - 'n' must be a multiple of 4 bytes.  This isn't strictly
                //    required, but if it's not observed, the last (N - N/4)
                //    bytes won't be inverted.
                //
                // The only sensor that uses a negative image is the TCD1103.
                // Its buffer is DWORD-aligned because it's allocated via
                // malloc(), which always does worst-case alignment.  Its
                // buffer is 1546 bytes long, which violates the multiple-of-4
                // rule, but inconsequentially, as the last 14 bytes represent
                // dummy pixels that can be ignored (so it's okay that we'll 
                // miss inverting the last two bytes).
                //
                uint32_t *pix32 = reinterpret_cast<uint32_t*>(pix);
                for (int i = n/4; i != 0; --i)
                    *pix32++ ^= 0xFFFFFFFF;
            }            

            // send the pixels in report-sized chunks until we get them all
            int idx = 0;
            while (idx < n)
                js.sendPlungerPix(idx, n, pix);
        }
        
        // release the pixel buffer
        sensor.releasePix();
    }
    
protected:
    // process an image to read the plunger position
    virtual bool process(const uint8_t *pix, int npix, int &rawPos, ProcessResult &res) = 0;
    
    // send extra status headers, following the standard headers (types 0 and 1)
    virtual void extraStatusHeaders(USBJoystick &js, ProcessResult &res) { }
    
    // get the detected orientation
    virtual int getOrientation() const { return 0; }
    
    // underlying hardware sensor interface
    PlungerSensorImageInterface &sensor;
    
    // number of pixels
    int native_npix;
    
    // Does the sensor report a "negative" image?  This is like a photo
    // negative, where brighter pixels are represented by lower numbers in
    // the pixel array.
    bool negativeImage;
    
    // Auto-exposure time.  This is for use by process() in the subclass.
    // On each frame processing iteration, it can adjust this to optimize
    // the image quality.
    uint32_t axcTime;
    
    // Extra exposure time.  This is for use by the PC side, mostly for
    // debugging use to allow the PC user to manually adjust the exposure
    // when inspecting captured frames.
    uint32_t extraIntTime;
};


#endif /* PLUNGER_H */
