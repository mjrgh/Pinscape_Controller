// Base class for TSL14xx-based plunger sensors.
//
// This provides a common base class for plunger sensors based on
// AMS/TAOS TSL14xx sensors (TSL1410R, TSL1412S, TSL1401CL).  The sensors
// in this series all work the same way, differing mostly in the number
// of pixels.  However, we have two fundamentally different ways of using
// these image sensors to detect position: sensing the position of the
// shadow cast by the plunger on the sensor, and optically reading a bar
// code telling us the location of the sensor along a scale.  This class
// provides the low-level pixel-sensor interface; subclasses provide the
// image analysis that figures the position from the captured image.


#ifndef _TSL14XXSENSOR_H_
#define _TSL14XXSENSOR_H_

#include "plunger.h"
#include "TSL14xx.h"

class PlungerSensorTSL14xx: public PlungerSensor
{
public:
    PlungerSensorTSL14xx(int nativePix, PinName si, PinName clock, PinName ao)
        : sensor(nativePix, si, clock, ao)
    {
        // Figure the scaling factor for converting native pixel readings
        // to our normalized 0..65535 range.  The effective calculation we
        // need to perform is (reading*65535)/(npix-1).  Division is slow
        // on the M0+, and floating point is dreadfully slow, so recast the
        // per-reading calculation as a multiply (which, unlike DIV, is fast
        // on KL25Z - the device has a single-cycle 32-bit hardware multiply).
        // How do we turn a divide into a multiply?  By calculating the
        // inverse!  How do we calculate a meaningful inverse of a large
        // integer using integers?  By doing our calculations in fixed-point
        // integers, which is to say, using hardware integers but treating
        // all values as multiplied by a scaling factor.  We'll use 64K as
        // the scaling factor, since we can divide the scaling factor back
        // out by using an arithmetic shift (also fast on M0+).  
        native_npix = nativePix;
        scaling_factor = (65535U*65536U) / (nativePix - 1);
        
        // start with no additional integration time for automatic 
        // exposure control
        axcTime = 0;
    }
        
    // is the sensor ready?
    virtual bool ready() { return sensor.ready(); }
    
    // read the plunger position
    virtual bool read(PlungerReading &r)
    {
        // start reading the next pixel array - this also waits for any
        // previous read to finish, ensuring that we have stable pixel
        // data in the capture buffer
        sensor.startCapture(axcTime);
        
        // get the image array from the last capture
        uint8_t *pix;
        uint32_t tpix;
        sensor.getPix(pix, tpix);
        
        // process the pixels
        int pixpos;
        if (process(pix, native_npix, pixpos))
        {            
            // Normalize to the 16-bit range by applying the scaling
            // factor.  The scaling factor is 65535/npix expressed as
            // a fixed-point number with 64K scale, so multiplying the
            // pixel reading by this will give us the result with 64K
            // scale: so shift right 16 bits to get the final answer.
            // (The +32768 is added for rounding: it's equal to 0.5 
            // at our 64K scale.)
            r.pos = uint16_t((scaling_factor*uint32_t(pixpos) + 32768) >> 16);
            r.t = tpix;
            
            // success
            return true;
        }
        else
        {
            // no position found
            return false;
        }
    }
        
    virtual void init()
    {
        sensor.clear();
    }
    
    // Send a status report to the joystick interface.
    // See plunger.h for details on the arguments.
    virtual void sendStatusReport(USBJoystick &js, uint8_t flags, uint8_t extraTime)
    {
        // To get the requested timing for the cycle we report, we need to run
        // an extra cycle.  Right now, the sensor is integrating from whenever
        // the last start() call was made.  
        //
        // 1. Call startCapture() to end that previous cycle.  This will collect
        // dits pixels into one DMA buffer (call it EVEN), and start a new 
        // integration cycle.  
        // 
        // 2. We know a new integration has just started, so we can control its
        // time.  Wait for the cycle we just started to finish, since that sets
        // the minimum time.
        //
        // 3. The integration cycle we started in step 1 has now been running the
        // minimum time - namely, one read cycle.  Pause for our extraTime delay
        // to add the requested added time.
        //
        // 4. Start the next cycle.  This will make the pixels we started reading
        // in step 1 available via getPix(), and will end the integration cycle
        // we started in step 1 and start reading its pixels into the internal
        // DMA buffer.  
        //
        // 5. This is where it gets tricky!  The pixels we want are the ones that 
        // started integrating in step 1, which are the ones we're reading via DMA 
        // now.  The pixels available via getPix() are the ones from the cycle we 
        // *ended* in step 1 - we don't want these.  So we need to start a *third*
        // cycle in order to get the pixels from the second cycle.
        
        sensor.startCapture(axcTime);       // transfer pixels from period A, begin integration period B
        sensor.wait();                      // wait for scan of A to complete, as minimum integration B time
        wait_us(long(extraTime) * 100);     // add extraTime (0.1ms == 100us increments) to integration B time
        sensor.startCapture(axcTime);       // transfer pixels from integration period B, begin period C; period A pixels now available
        sensor.startCapture(axcTime);       // trnasfer pixels from integration period C, begin period D; period B pixels now available
        
        // get the pixel array
        uint8_t *pix;
        uint32_t t;
        sensor.getPix(pix, t);

        // start a timer to measure the processing time
        Timer pt;
        pt.start();

        // process the pixels and read the position
        int pos;
        int n = native_npix;
        if (!process(pix, n, pos))
            pos = 0xFFFF;
        
        // note the processing time
        uint32_t processTime = pt.read_us();
        
        // if a low-res scan is desired, reduce to a subset of pixels
        if (flags & 0x01)
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
            
            // rescale the position for the reduced resolution
            if (pos != 0xFFFF)
                pos = pos * (lowResPix-1) / (n-1);

            // update the pixel count to the reduced array size
            n = lowResPix;
        }
        
        // send the sensor status report
        js.sendPlungerStatus(n, pos, getOrientation(), sensor.getAvgScanTime(), processTime);
        
        // If we're not in calibration mode, send the pixels
        extern bool plungerCalMode;
        if (!plungerCalMode)
        {
            // send the pixels in report-sized chunks until we get them all
            int idx = 0;
            while (idx < n)
                js.sendPlungerPix(idx, n, pix);
        }
            
        // It takes us a while to send all of the pixels, since we have
        // to break them up into many USB reports.  This delay means that
        // the sensor has been sitting there integrating for much longer
        // than usual, so the next frame read will be overexposed.  To
        // mitigate this, make sure we don't have a capture running,
        // then clear the sensor and start a new capture.
        sensor.wait();
        sensor.clear();
        sensor.startCapture(axcTime);
    }
    
    // get the average sensor scan time
    virtual uint32_t getAvgScanTime() { return sensor.getAvgScanTime(); }
    
protected:
    // Analyze the image and find the plunger position.  If successful,
    // fills in 'pixpos' with the plunger position using the 0..65535
    // scale and returns true.  If no position can be detected from the
    // image data, returns false.
    virtual bool process(const uint8_t *pix, int npix, int &pixpos) = 0;
    
    // Get the currently detected sensor orientation, if applicable.
    // Returns 1 for standard orientation, -1 for reversed orientation,
    // or 0 for orientation unknown or not applicable.  Edge sensors can
    // automatically detect orientation by observing which side of the
    // image is in shadow.  Bar code sensors generally can't detect
    // orientation.
    virtual int getOrientation() const { return 0; }
    
    // the low-level interface to the TSL14xx sensor
    TSL14xx sensor;
    
    // number of pixels
    int native_npix;

    // Scaling factor for converting a native pixel reading to the normalized
    // 0..65535 plunger reading scale.  This value contains 65535*65536/npix,
    // which is equivalent to 65535/npix as a fixed-point number with a 64K
    // scale.  To apply this, multiply a pixel reading by this value and
    // shift right by 16 bits.
    uint32_t scaling_factor;
    
    // Automatic exposure control time, in microseconds.  This is an amount
    // of time we add to each integration cycle to compensate for low light
    // levels.  By default, this is always zero; the base class doesn't have
    // any logic for determining proper exposure, because that's a function
    // of the type of image we're looking for.  Subclasses can add logic in
    // the process() function to check exposure level and adjust this value
    // if the image looks over- or under-exposed.
    uint32_t axcTime;
};

#endif
