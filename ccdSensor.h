// CCD plunger sensor
//
// This file implements our generic plunger sensor interface for the 
// TAOS TSL1410R CCD array sensor.



// Number of pixels we read from the CCD on each frame.  This can be
// less than the actual sensor size if desired; if so, we'll read every
// nth pixel.  E.g., with a 1280-pixel physical sensor, if npix is 320,
// we'll read every 4th pixel.  Reading a pixel is fairly time-consuming,
// because it requires waiting for the pixel's electric charge to
// stabilize on the CCD output, for the charge to transfer to the KL25Z 
// input, and then for the KL25Z analog voltage sampler to get a stable
// reading.  This all takes about 15us per pixel, which adds up to
// a relatively long time in such a large array.  However, we can skip
// a pixel without waiting for all of that charge stabilization time,
// so we can get higher frame rates by only sampling a subset of the
// pixels.  The array is so dense (400dpi) that we can still get
// excellent resolution by reading a fraction of the total pixels.
//
// Empirically, 160 pixels seems to be the point of diminishing returns
// for resolution - going higher will only improve the apparent smoothness
// slightly, if at all.  160 pixels gives us 50dpi on the sensor, which 
// is roughly the same as the physical pixel pitch of a typical cabinet 
// playfield monitor.  (1080p HDTV displayed 1920x1080 pixels, and a 40" 
// TV is about 35" wide, so the dot pitch is about 55dpi across the width 
// of the TV.  If on-screen plunger is displayed at roughly the true
// physical size, it's about 3" on the screen, or about 165 pixels.  So at
// 160 pixels on the sensor, one pixel on the sensor translates to almost
// exactly one on-screen pixel on the TV, which makes the animated motion 
// on-screen about as smooth as it can be.  Looked at another way, 50dpi
// means that we're measuring the physical shooter rod position in about
// half-millimeter increments, which is probably better than I can 
// discern by feel or sight.
const int npix = 160;


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
        ccd.read(pix, npix, ccdReadCB, 0, 3);

        // get the average brightness at each end of the sensor
        long avg1 = (long(pix[0]) + long(pix[1]) + long(pix[2]) + long(pix[3]) + long(pix[4]))/5;
        long avg2 = (long(pix[npix-1]) + long(pix[npix-2]) + long(pix[npix-3]) + long(pix[npix-4]) + long(pix[npix-5]))/5;
        
        // Work from the bright end to the dark end.  VP interprets the
        // Z axis value as the amount the plunger is pulled: zero is the
        // rest position, and the axis maximum is fully pulled.  So we 
        // essentially want to report how much of the sensor is lit,
        // since this increases as the plunger is pulled back.
        int si = 1, di = 1;
        long avgHi = avg1;
        if (avg1 < avg2)
            si = npix - 2, di = -1, avgHi = avg2;

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
        // Then multiply the whole thing by 3 to factor out the averaging
        // of each three adjacent pixels that we do in the loop (to save a
        // little time on a mulitply on each loop):
        //
        //    threshold' = lo + 2*hi
        //
        // Now, 'lo' is always one of avg1 or avg2, and 'hi' is the other
        // one, so we can rewrite this as hi + avg1 + avg2.  We also already
        // pulled out 'hi' as avgHi, so we finally come to the final
        // simplified expression:
        long midpt = avg1 + avg2 + avgHi;
        
        // If we have enough contrast, proceed with the scan.
        //
        // If the bright end and dark end don't differ by enough, skip this
        // reading entirely.  Either we have an overexposed or underexposed frame,
        // or the sensor is misaligned and is either fully in or out of shadow
        // (it's supposed to be mounted such that the edge of the shadow always
        // falls within the sensor, for any possible plunger position).
        if (labs(avg1 - avg2) > 0x1000)
        {
            uint16_t *pixp = pix + si;           
            for (int n = 1 ; n < npix - 1 ; ++n, pixp += di)
            {
                // if we've crossed the midpoint, report this position
                if (long(pixp[-1]) + long(pixp[0]) + long(pixp[1]) < midpt)
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
            js.updateExposure(idx, npix, pix);
            
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
