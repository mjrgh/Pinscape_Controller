// CCD plunger sensor
//
// This file implements our generic plunger sensor interface for the 
// TAOS TSL1410R CCD array sensor.



// Number of pixels we read from the CCD on each frame.  Use the
// sample size from config.h.
const int npix = CCD_NPIXELS_SAMPLED;

// PlungerSensor interface implementation for the CCD
class PlungerSensor
{
public:
    PlungerSensor() : ccd(CCD_SO_PIN)
    {
    }
    
    // initialize
    void init()
    {
        // flush any random power-on values from the CCD's integration
        // capacitors, and start the first integration cycle
        ccd.clear();
    }
    
    // Perform a low-res scan of the sensor.  
    int lowResScan()
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
                return n*npix/nlpix;
            }
        }
        
        // didn't find a shadow - assume the whole array is in shadow (so
        // the edge is at the zero pixel point)
        return 0;
    }

    // Perform a high-res scan of the sensor.
    bool highResScan(int &pos)
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
    void sendExposureReport(USBJoystick &js)
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
    
private:
    // pixel buffer
    uint16_t pix[npix];
    
    // the low-level interface to the CCD hardware
    TSL1410R<CCD_SI_PIN, CCD_CLOCK_PIN> ccd;
};
