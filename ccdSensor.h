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
    PlungerSensorCCD(int nPix, PinName si, PinName clock, PinName ao1, PinName ao2) 
        : ccd(nPix, si, clock, ao1, ao2)
    {
    }
    
    // initialize
    virtual void init()
    {
        // flush any random power-on values from the CCD's integration
        // capacitors, and start the first integration cycle
        ccd.clear();
    }
    
    // Perform a low-res scan of the sensor.  
    virtual bool lowResScan(int &pos)
    {
        // read the pixels at low resolution
        const int nlpix = 32;
        uint16_t pix[nlpix];
        ccd.read(pix, nlpix);
    
        // determine which end is brighter
        uint16_t p1 = pix[0];
        uint16_t p2 = pix[nlpix-1];
        int si = 1, di = 1;
        if (p1 < p2)
            si = nlpix, di = -1;
        
        // figure the shadow edge threshold - just use the midpoint 
        // of the levels at the bright and dark ends
        uint16_t shadow = uint16_t((long(p1) + long(p2))/2);
        
        // find the current tip position
        for (int n = 0 ; n < nlpix ; ++n, si += di)
        {
            // check to see if we found the shadow
            if (pix[si] <= shadow)
            {
                // got it - normalize it to normal 'npix' resolution and
                // return the result
                pos = n*npix/nlpix;
                return true;
            }
        }
        
        // didn't find a shadow - return failure
        return false;
    }

    // Perform a high-res scan of the sensor.
    virtual bool highResScan(int &pos)
    {
        // read the array
        ccd.read(pix, npix);

        // get the brightness at each end of the sensor
        long b1 = pix[0];
        long b2 = pix[npix-1];
        
        // Work from the bright end to the dark end.  VP interprets the
        // Z axis value as the amount the plunger is pulled: zero is the
        // rest position, and the axis maximum is fully pulled.  So we 
        // essentially want to report how much of the sensor is lit,
        // since this increases as the plunger is pulled back.
        int si = 0, di = 1;
        long hi = b1;
        if (b1 < b2)
            si = npix - 1, di = -1, hi = b2;

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
            for (int n = 0 ; n < npix ; ++n, pixp += di)
            {
                // if we've crossed the midpoint, report this position
                if (long(*pixp) < midpt)
                {
                    // note the new position
                    pos = n;
                    return true;
                }
            }
        }
        
        // we didn't find a shadow - return no reading
        return false;
    }
    
    // send an exposure report to the joystick interface
    virtual void sendExposureReport(USBJoystick &js)
    {
        // send reports for all pixels
        int idx = 0;
        while (idx < npix)
        {
            js.updateExposure(idx, npix, pix);
            wait_ms(1);
        }
            
        // The pixel dump requires many USB reports, since each report
        // can only send a few pixel values.  An integration cycle has
        // been running all this time, since each read starts a new
        // cycle.  Our timing is longer than usual on this round, so
        // the integration won't be comparable to a normal cycle.  Throw
        // this one away by doing a read now, and throwing it away - that 
        // will get the timing of the *next* cycle roughly back to normal.
        ccd.read(pix, npix);
    }
    
protected:
    // pixel buffer
    uint16_t *pix;
    
    // the low-level interface to the CCD hardware
    TSL1410R ccd;
};


// TSL1410R sensor 
class PlungerSensorTSL1410R: public PlungerSensorCCD
{
public:
    PlungerSensorTSL1410R(PinName si, PinName clock, PinName ao1, PinName ao2) 
        : PlungerSensorCCD(1280, si, clock, ao1, ao2)
    {
        // This sensor is 1x1280 pixels at 400dpi.  Sample every 8th
        // pixel -> 160 pixels at 50dpi == 0.5mm spatial resolution.
        npix = 160;
        pix = pixbuf;
    }
    
    uint16_t pixbuf[160];
};

// TSL1412R
class PlungerSensorTSL1412R: public PlungerSensorCCD
{
public:
    PlungerSensorTSL1412R(PinName si, PinName clock, PinName ao1, PinName ao2)
        : PlungerSensorCCD(1536, si, clock, ao1, ao2)
    {
        // This sensor is 1x1536 pixels at 400dpi.  Sample every 8th
        // pixel -> 192 pixels at 50dpi == 0.5mm spatial resolution.
        npix = 192;
        pix = pixbuf;
    }
    
    uint16_t pixbuf[192];
};

