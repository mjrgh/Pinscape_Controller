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
    PlungerSensorCCD(int nativePix, PinName si, PinName clock, PinName ao1, PinName ao2)
        : ccd(nativePix, si, clock, ao1, ao2)
    {
        // we don't know the direction yet
        dir = 0;
        
        // we don't have contrast information from any prior images yet,
        // so just peg the low and high brightness levels at the extremes
        lastLo = 0x00;
        lastHi = 0xff;
    }
    
    // initialize
    virtual void init()
    {
        // flush any random power-on values from the CCD's integration
        // capacitors, and start the first integration cycle
        ccd.clear();
    }
    
    // Perform a high-res scan of the sensor.
    virtual bool read(uint16_t &pos)
    {
        // start reading the next pixel array
        ccd.startCapture();
        
        // get the last image array
        uint8_t *pix;
        int n;
        ccd.getPix(pix, n);
        
        // process it
        int pixpos;
        process(pix, n, pixpos);
        
        // if we found the position, return it
        if (pixpos >= 0)
        {
            // adjust for the reversed orientation if necessary
            if (dir < 0)
                pixpos = n - pixpos;
                
            // normalize to the 16-bit range
            pos = uint16_t(((pixpos << 16) - pixpos) / n);
            
            // success
            return true;
        }
        else
        {
            // no position found
            return false;
        }
    }
        
    // Process an image.  Applies noise reduction and looks for edges.
    // If we detect the plunger position, we set 'pos' to the pixel location
    // of the edge; otherwise we set pos to -1.  If 'copy is true, we copy
    // the noise-reduced pixel array back to the caller's pixel array, 
    // otherwise we leave it unchanged.
    void process(uint8_t *pix, int n, int &pos, bool copy = false)
    {
        // allocate a working buffer
        uint8_t *tmp = (uint8_t *)malloc(n+2);
        printf("processing - tmp=%lx\r\n", tmp);
        
        // find the low and high pixel values
        int lo = 256, hi = 0;
        for (int i = 0 ; i < n ; ++i)
        {
            if (pix[i] < lo) lo = pix[i];
            if (pix[i] > hi) hi = pix[i];
        }
        
        for (int pass = 1 ; pass <= 2 ; ++pass)
        {
            // peg the pixels to their ranges with these parameters
            pegPixels(pix, tmp, n, lo, hi);
        
            // count the edges
            int nEdges;
            findEdges(tmp, n, pos, nEdges);
            
            // if we found one edge, stop
            if (nEdges == 1)
            {
                // If this is the first pass, we appear to have a good
                // contrast level.  Note this for future use, in case the
                // next image has insufficient contrast.
                lastLo = lo;
                lastHi = hi;
                
                // This also tells us the orientation.  If the bright
                // end is at the 0th pixel, we're installed in the standard
                // orientation (dir = 1), otherwise we're installed in the
                // reverse orientation (dir = -1).
                dir = (pix[0] == 255 ? 1 : -1);
                
                // use this result
                break;
            }
            
            // we have other than one edge, so presume failure
            pos = -1;
        
            // On the first pass, if we didn't find any edges, or we
            // found more than one, make another pass with the brightness
            // range from the *previous* image.  This helps us deal with
            // images where the plunger is positioned so that entire sensor
            // is entirely convered or uncovered, in which case we won't
            // the image won't contain any natural contrast that would
            // allow us to infer the exposure level automatically.  In
            // these cases, we assume that the new image has a similar
            // exposure level to the prior image.
            if (pass == 1)
            {
                // first pass - try again with the previous exposure levels
                lo = lastLo;
                hi = lastHi;
            }
            else if (nEdges == 0)
            {
                // There are no edges, and we're on the second pass, so
                // we're already using an exposure range that worked on a
                // previous exposure.  If the sensor is reading in the
                // low end of the range, it must be entirely covered, which
                // means that the plunger is all the way forward.  If it's
                // at the high end of the range, the sensor must be entirely
                // exposed, so the plunger is pulled all the way back.
                // Report the end based on the known orientation.
                if (dir != 0)
                {
                    if (tmp[0] == 0)
                    {
                        // all dark - fully covered, plunger is all the way forward
                        pos = dir > 0 ? 0 : n - 1;
                    }
                    else if (tmp[0] == 255)
                    {
                        // all bright - fully exposed, plunger is all the way back
                        pos = dir > 0 ? n - 1 : 0;
                    }
                }
            }
        }
        
        // if desired, copy the processed pixels back to the caller's array
        if (copy)
            memcpy(pix, tmp, n);
            
        // done with the temp array
        delete [] tmp;
    }
    
    // Peg each pixel to its third of the range
    void pegPixels(const uint8_t *pix, uint8_t *tmp, int n, int lo, int hi)
    {
        // Figure the thresholds for the top third and bottom third
        // of the brightness range.
        int third = (hi - lo)/3;
        int midHi = hi - third;
        int midLo = lo + third;
        
        // Peg each pixel to its third of the range
        for (int i = 0 ; i < n ; ++i)
            tmp[i] = (pix[i] < midLo ? 0 : pix[i] > midHi ? 255 : 127);
            
        // Set up a circular buffer for a rolling 5-sample window.  To
        // simplify the loop, fill in two fake pixels before the first
        // one simply by repeating the first pixel in those slots.
        uint8_t t[5] = { tmp[0], tmp[0], tmp[0], tmp[1], tmp[2] };
        int a = 0;
        
        // Likewise, fill in two fake pixels at the end by copying the
        // actual last pixel.
        tmp[n] = tmp[n+1] = tmp[n-1];
        int s = t[0] + t[1] + t[2] + t[3] + t[4];
        
        // Run through the array and peg each pixel to the consensus
        // of its two neighbors to either side.  This smooths out noise
        // by eliminating lone flipped pixels.
        for (int i = 1 ; i < n ; ++i)
        {
            // apply the consensus vote to this pixel
            tmp[i] = (s < 85*5 ? 0 : s > 382*5 ? 255 : 127);
            
            // update the rolling window with the next sample
            s -= t[0];
            s += (t[a] = pix[i+3]);
            a = (a + 1) % 5;
        }
    }
    
    // Find the edge(s)
    void findEdges(const uint8_t *pix, int n, int &edgePos, int &nEdges)
    {
        // we don't have any edges yet
        nEdges = 0;
        edgePos = -1;
        
        // loop over the pixels looking for edges
        int prv = pix[0], cur = pix[1], nxt = pix[2];
        for (int i = 1 ; i < n - 1 ; prv = cur, cur = nxt, nxt = pix[++i + 1])
        {
            if (cur != prv)
            {
                ++nEdges;
                edgePos = i;
            }
        }
    }
    
    // Send an exposure report to the joystick interface.
    // 
    // Mode bits:
    //    0x01  -> send processed pixels (default is raw pixels)
    //    0x02  -> low res scan (default is high res scan)
    virtual void sendExposureReport(USBJoystick &js, uint8_t mode)
    {
        // do a scan
        ccd.startCapture();
        
        // get the last pixel array
        uint8_t *pix;
        int n;
        ccd.getPix(pix, n);
        
        // apply processing if desired
        int pos = -1;
        if (mode & 0x01)
            process(pix, n, pos, true);
        
        // if a low-res scan is desired, reduce to a subset of pixels
        if (mode & 0x02)
        {
            int lowResPix = 128;
            int skip = n / lowResPix;
            int src, dst;
            for (src = skip, dst = 1 ; dst < lowResPix ; ++dst, src += skip)
                pix[dst] = pix[src];
            n = dst;
        }
        
        // send reports for all pixels
        int idx = 0;
        while (idx < n)
            js.updateExposure(idx, n, pix);
            
        // The pixel dump requires many USB reports, since each report
        // can only send a few pixel values.  An integration cycle has
        // been running all this time, since each read starts a new
        // cycle.  Our timing is longer than usual on this round, so
        // the integration won't be comparable to a normal cycle.  Throw
        // this one away by doing a read now, and throwing it away - that 
        // will get the timing of the *next* cycle roughly back to normal.
        ccd.startCapture();
    }
    
protected:
    // Sensor orientation.  +1 means that the "tip" end - which is always
    // the brighter end in our images - is at the 0th pixel in the array.
    // -1 means that the tip is at the nth pixel in the array.  0 means
    // that we haven't figured it out yet.
    int dir;
    
    // High and low brightness levels from last successful image.  We keep
    // track of these so that we can apply them to any images we take with
    // insufficient contrast to detect an edge.  We assume in these cases
    // that the plunger is positioned so that the sensor is entirely in
    // shadow or entirely in light.  We assume that the exposure level is
    // roughly the same as the previous frame where we did find an edge,
    // so we use the last frame's levels to determine whether the uniform
    // brightness we're seeing is shadow or light.
    uint8_t lastLo, lastHi;
    
public:
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
    }
};

// TSL1412R
class PlungerSensorTSL1412R: public PlungerSensorCCD
{
public:
    PlungerSensorTSL1412R(PinName si, PinName clock, PinName ao1, PinName ao2)
        : PlungerSensorCCD(1536, si, clock, ao1, ao2)
    {
    }
};
