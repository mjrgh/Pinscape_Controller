// CCD plunger sensor
//
// This class implements our generic plunger sensor interface for the 
// TAOS TSL1410R and TSL1412R linear sensor arrays.  Physically, these
// sensors are installed with their image window running parallel to
// the plunger rod, spanning the travel range of the plunger tip.
// A light source is positioned on the opposite side of the rod, so
// that the rod casts a shadow on the sensor.  We sense the position
// by looking for the edge of the shadow.
//
// These sensors can take an image quickly, but it takes a significant
// amount of time to transfer the image data from the sensor to the 
// microcontroller, since each pixel's analog voltage level must be
// sampled serially.  It takes about 20us to sample a pixel accurately.
// The TSL1410R has 1280 pixels, and the 1412R has 1536.  Sampling 
// every pixel would thus take about 25ms or 30ms respectively.
// This is too slow for a responsive feel in the UI, and much too
// slow to track the plunger release motion in real time.  To improve
// on the read speed, we only sample a subset of pixels for each
// reading - for higher speed at the expense of spatial resolution.
// The sensor's native resolution is much higher than we need, so
// this is a perfectly equitable trade.

#include "plunger.h"


// PlungerSensor interface implementation for the CCD
class PlungerSensorCCD: public PlungerSensor
{
public:
    PlungerSensorCCD(
        int nativePix, int highResPix, int lowResPix, 
        PinName si, PinName clock, PinName ao1, PinName ao2) 
        : ccd(nativePix, si, clock, ao1, ao2)
    {
        this->highResPix = highResPix;
        this->lowResPix = lowResPix;
        this->pix = new uint16_t[highResPix];
    }
    
    // initialize
    virtual void init()
    {
        // flush any random power-on values from the CCD's integration
        // capacitors, and start the first integration cycle
        ccd.clear();
    }
    
    // Perform a low-res scan of the sensor.  
    virtual bool lowResScan(float &pos)
    {
        // If we haven't sensed the direction yet, do a high-res scan
        // first, so that we can get accurate direction data.  Return
        // the result of the high-res scan if successful, since it 
        // provides even better data than the caller requested from us.
        // This will take longer than the caller wanted, but it should
        // only be necessary to do this once after the system stabilizes
        // after startup, so the timing difference won't affect normal
        // operation.
        if (dir == 0)
            return highResScan(pos);
        
        // read the pixels at low resolution
        ccd.read(pix, lowResPix);
        
        // set the loop variables for the sensor orientation
        int si = 0;
        if (dir < 0)
            si = lowResPix - 1;
            
        // Figure the shadow edge threshold.  Use the midpoint between
        // the average levels of a few pixels at each end.
        uint16_t shadow = uint16_t(
            (long(pix[0]) + long(pix[1]) + long(pix[2])
             + long(pix[lowResPix-1]) + long(pix[lowResPix-2]) + long(pix[lowResPix-3])
            )/6);
        
        // find the current tip position
        for (int n = 0 ; n < lowResPix ; ++n, si += dir)
        {
            // check to see if we found the shadow
            if (pix[si] <= shadow)
            {
                // got it - normalize to the 0.0..1.0 range and return success
                pos = float(n)/lowResPix;
                return true;
            }
        }
        
        // didn't find a shadow - return failure
        return false;
    }

    // Perform a high-res scan of the sensor.
    virtual bool highResScan(float &pos)
    {
        // read the array
        ccd.read(pix, highResPix);

        // Sense the orientation of the sensor if we haven't already.  If 
        // that fails, we must not have enough contrast to find a shadow edge 
        // in the image, so there's no point in looking any further - just
        // return failure.
        if (dir == 0 && !senseOrientation(highResPix))
            return false;
            
        // Get the average brightness for a few pixels at each end.
        long b1 = (long(pix[0]) + long(pix[1]) + long(pix[2]) + long(pix[3]) + long(pix[4])) / 5;
        long b2 = (long(pix[highResPix-1]) + long(pix[highResPix-2]) + long(pix[highResPix-3])
            + long(pix[highResPix-4]) + long(pix[highResPix-5])) / 5;

        // Work from the bright end to the dark end.  VP interprets the
        // Z axis value as the amount the plunger is pulled: zero is the
        // rest position, and the axis maximum is fully pulled.  So we 
        // essentially want to report how much of the sensor is lit,
        // since this increases as the plunger is pulled back.
        int si = 0;
        long hi = b1;
        if (dir < 0)
            si = highResPix - 1, hi = b2;

        // Figure the shadow threshold.  In practice, the portion of the
        // sensor that's not in shadow has all pixels consistently near
        // saturation; the first drop in brightness is pretty reliably the
        // start of the shadow.  So set the threshold level to be closer
        // to the bright end's brightness level, so that we detect the leading
        // edge if the shadow isn't perfectly sharp.  Use the point 1/3 of
        // the way down from the high top the low side, so:
        //
        //   threshold = lo + (hi - lo)*2/3
        //             = lo + hi*2/3 - lo*2/3
        //             = lo - lo*2/3 + hi*2/3
        //             = lo*1/3 + hi*2/3
        //             = (lo + hi*2)/3
        //
        // Now, 'lo' is always one of b1 or b2, and 'hi' is the other
        // one, so we can rewrite this as:
        long midpt = (b1 + b2 + hi)/3;
        
        // If we have enough contrast, proceed with the scan.
        //
        // If the bright end and dark end don't differ by enough, skip this
        // reading entirely.  Either we have an overexposed or underexposed frame,
        // or the sensor is misaligned and is either fully in or out of shadow
        // (it's supposed to be mounted such that the edge of the shadow always
        // falls within the sensor, for any possible plunger position).
        if (labs(b1 - b2) > 0x1000)
        {
            uint16_t *pixp = pix + si;           
            for (int n = 0 ; n < highResPix ; ++n, pixp += dir)
            {
                // if we've crossed the midpoint, report this position
                if (long(*pixp) < midpt)
                {
                    // normalize to the 0.0..1.0 range and return success
                    pos = float(n)/highResPix;
                    return true;
                }
            }
        }
        
        // we didn't find a shadow - return no reading
        return false;
    }
    
    // Infer the sensor orientation from the image data.  This determines 
    // which end of the array has the brighter pixels.  In some cases it 
    // might not be possible to tell: if the light source is turned off,
    // or if the plunger is all the way to one extreme so that the entire
    // pixel array is in shadow or in full light.  To sense the direction
    // we need to have a sufficient difference in brightness between the
    // two ends of the array to be confident that one end is in shadow
    // and the other isn't.  On success, sets member variable 'dir' and
    // returns true; on failure (i.e., we don't have sufficient contrast
    // to sense the orientation), returns false and leaves 'dir' unset.
    bool senseOrientation(int n)
    {
        // get the total brightness for the first few pixels at
        // each end of the array (a proxy for the average - just
        // save time by skipping the divide-by-N)
        long a = long(pix[0]) + long(pix[1]) + long(pix[2]) 
            + long(pix[3]) + long(pix[4]);
        long b = long(pix[n-1]) + long(pix[n-2]) + long(pix[n-3])
            + long(pix[n-4]) + long(pix[n-5]);
            
        // if the difference is too small, we can't tell
        const long minPct = 10;
        const long minDiff = 65535*5*minPct/100;
        if (labs(a - b) < minDiff)
            return false;
            
        // we now know the orientation - set the 'dir' member variable
        // for future use
        if (a > b)
            dir = 1;
        else
            dir = -1;
            
        // success
        return true;
    }
    
    // send an exposure report to the joystick interface
    virtual void sendExposureReport(USBJoystick &js)
    {
        // Read a fresh high-res scan, then do another right away.  This
        // gives us the shortest possible exposure for the sample we report,
        // which helps ensure that the user inspecting the data sees something
        // close to what we see when we calculate the plunger position.
        ccd.read(pix, highResPix);
        ccd.read(pix, highResPix);
        
        // send reports for all pixels
        int idx = 0;
        while (idx < highResPix)
            js.updateExposure(idx, highResPix, pix);
            
        // The pixel dump requires many USB reports, since each report
        // can only send a few pixel values.  An integration cycle has
        // been running all this time, since each read starts a new
        // cycle.  Our timing is longer than usual on this round, so
        // the integration won't be comparable to a normal cycle.  Throw
        // this one away by doing a read now, and throwing it away - that 
        // will get the timing of the *next* cycle roughly back to normal.
        ccd.read(pix, highResPix);
    }
    
protected:
    // pixel buffer - we allocate this to be big enough for a high-res scan
    uint16_t *pix;

    // number of pixels in a high-res scan
    int highResPix;
    
    // number of pixels in a low-res scan
    int lowResPix;
    
    // Sensor orientation.  +1 means that the "tip" end - which is always
    // the brighter end in our images - is at the 0th pixel in the array.
    // -1 means that the tip is at the nth pixel in the array.  0 means
    // that we haven't figured it out yet.
    int dir;
    
public:
    // the low-level interface to the CCD hardware
    TSL1410R ccd;
};


// TSL1410R sensor 
class PlungerSensorTSL1410R: public PlungerSensorCCD
{
public:
    PlungerSensorTSL1410R(PinName si, PinName clock, PinName ao1, PinName ao2) 
        : PlungerSensorCCD(1280, 320, 64, si, clock, ao1, ao2)
    {
    }
};

// TSL1412R
class PlungerSensorTSL1412R: public PlungerSensorCCD
{
public:
    PlungerSensorTSL1412R(PinName si, PinName clock, PinName ao1, PinName ao2)
        : PlungerSensorCCD(1536, 384, 64, si, clock, ao1, ao2)
    {
    }
};
