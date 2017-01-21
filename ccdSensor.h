// CCD plunger sensor
//
// This class implements our generic plunger sensor interface for the 
// TAOS TSL1410R and TSL1412R linear sensor arrays.  Physically, these
// sensors are installed with their image window running parallel to
// the plunger rod, spanning the travel range of the plunger tip.
// A light source is positioned on the opposite side of the rod, so
// that the rod casts a shadow on the sensor.  We sense the position
// by looking for the edge of the shadow.

#include "plunger.h"

// Scan method - select a method listed below.  Method 2 (find the point
// with maximum brightness slop) seems to work the best so far.
#define SCAN_METHOD 2
//
//
//  0 = One-way scan.  This is the original algorithim from the v1 software, 
//      with some slight improvements.  We start at the brighter end of the
//      sensor and scan until we find a pixel darker than a threshold level 
//      (halfway between the respective brightness levels at the bright and 
//      dark ends of the sensor).  The original v1 algorithm simply stopped
//      there.  This version is slightly improved: it scans for a few more 
//      pixels to make sure that the majority of the adjacent pixels are 
//      also in shadow, to help reject false edges from sensor noise or 
//      optical shadows that make one pixel read darker than it should.
//
//  1 = Meet in the middle.  We start two scans concurrently, one from 
//      the dark end of the sensor and one from the bright end.  For
//      the scan from the dark end, we stop when we reach a pixel that's
//      brighter than the average dark level by 2/3 of the gap between 
//      the dark and bright levels.  For the scan from the bright end,
//      we stop when we reach a pixel that's darker by 2/3 of the gap.
//      Each time we stop, we look to see if the other scan has reached
//      the same place.  If so, the two scans converged on a common
//      point, which we take to be the edge between the dark and bright
//      sections.  If the two scans haven't converged yet, we switch to
//      the other scan and continue it.  We repeat this process until
//      the two converge.  The benefit of this approach vs the older
//      one-way scan is that it's much more tolerant of noise, and the
//      degree of noise tolerance is dictated by how noisy the signal
//      actually is.  The dynamic degree of tolerance is good because
//      higher noise tolerance tends to result in reduced resolution.
//
//  2 = Maximum dL/ds (highest first derivative of luminance change per
//      distance, or put another way, the steepest rate of change in
//      brightness).  This scans the whole image and looks for the 
//      position with the highest dL/ds value.  We average over a window
//      of several pixels, to smooth out pixel noise; this should avoid
//      treating a single spiky pixel as having a steep slope adjacent 
//      to it.  The advantage I see in this approach is that it looks for
//      the *strongest* edge, which should make it less likely to be fooled
//      by noise that creates a false edge.  Algorithms 1 and 2 have
//      basically fixed thresholds for what constitutes an edge, but this
//      approach is more dynamic in that it evaluates each edge-like region
//      and picks the one with the highest contrast.  The one fixed feature
//      of this algorithm is the width of the edge, since that's limited
//      by the pixel window; but we only deal with one type of image, so
//      it should be possible to adjust the light source and sensor position
//      to always yield an image with a narrow enough edge region.
//
//      The max dL/ds method is the most compute-intensive method, because
//      of the pixel window averaging.  An assembly language implemementation
//      seems to be needed to make it fast enough on the KL25Z.  This method
//      has a fixed run time because it always does exactly one pass over
//      the whole pixel array.
//
//  3 = Total bright pixel count.  This simply adds up the total number
//      of pixels above a threshold brightness, without worrying about 
//      whether they're contiguous with other pixels on the same side
//      of the edge.  Since we know there's always exactly one edge,
//      all of the dark pixels should in principle be on one side, and
//      all of the light pixels should be on the other side.  There
//      might be some noise that creates isolated pixels that don't
//      match their neighbors, but these should average out.  The virtue
//      of this approach (apart from its simplicity) is that it should
//      be immune to false edges - local spikes due to noise - that
//      might fool the algorithms that explicitly look for edges.  In
//      practice, though, it seems to be even more sensitive to noise
//      than the other algorithms, probably because it treats every pixel
//      as independent and thus doesn't have any sort of inherent noise
//      reduction from considering relationships among pixels.
//

extern "C" int ccdScanMode2(
    const uint8_t *pix, int npix, const uint8_t **edgePtr, int dir);

// PlungerSensor interface implementation for the CCD
class PlungerSensorCCD: public PlungerSensor
{
public:
    PlungerSensorCCD(int nativePix, PinName si, PinName clock, PinName ao1, PinName ao2)
        : ccd(nativePix, si, clock, ao1, ao2)
    {
        // we don't know the direction yet
        dir = 0;
        
        // set the midpoint history arbitrarily to the absolute halfway point
        memset(midpt, 127, sizeof(midpt));
        midptIdx = 0;
    }
    
    // initialize
    virtual void init()
    {
        // flush any random power-on values from the CCD's integration
        // capacitors, and start the first integration cycle
        ccd.clear();
    }
    
    
    virtual bool read(PlungerReading &r)
    {
        // start reading the next pixel array - this also waits for any
        // previous read to finish, ensuring that we have stable pixel
        // data in the capture buffer
        ccd.startCapture();
        
        // get the image array from the last capture
        uint8_t *pix;
        int n;
        uint32_t tpix;
        ccd.getPix(pix, n, tpix);
        
        // process the pixels and look for the edge position
        int pixpos;
        if (process(pix, n, pixpos))
        {            
            // Normalize to the 16-bit range.  Our reading from the 
            // sensor is a pixel position, 0..n-1.  To rescale to the
            // normalized range, figure pixpos*65535/(n-1).
            r.pos = uint16_t(((pixpos << 16) - pixpos) / (n-1));
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
        
    // Process an image - scan for the shadow edge to determine the plunger
    // position.
    //
    // If we detect the plunger position, we set 'pos' to the pixel location
    // of the edge and return true; otherwise we return false.  The 'pos'
    // value returned, if any, is adjusted for sensor orientation so that
    // it reflects the logical plunger position (i.e., distance retracted,
    // where 0 is always the fully forward position and 'n' is fully
    // retracted).

#if SCAN_METHOD == 0
    // Scan method 0: one-way scan; original method used in v1 firmware.
    bool process(uint8_t *pix, int n, int &pos)
    {        
        // Get the levels at each end
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4])/5;
        int b = (int(pix[n-1]) + pix[n-2] + pix[n-3] + pix[n-4] + pix[n-5])/5;
        
        // Figure the sensor orientation based on the relative brightness
        // levels at the opposite ends of the image.  We're going to scan
        // across the image from each side - 'bi' is the starting index
        // scanning from the bright side, 'di' is the starting index on
        // the dark side.  'binc' and 'dinc' are the pixel increments
        // for the respective indices.
        int bi;
        if (a > b+10)
        {
            // left end is brighter - standard orientation
            dir = 1;
            bi = 4;
        }
        else if (b > a+10)
        {
           // right end is brighter - reverse orientation
            dir = -1;
            bi = n - 5;
        }
        else if (dir != 0)
        {
            // We don't have enough contrast to detect the orientation
            // from this image, so either the image is too overexposed
            // or underexposed to be useful, or the entire sensor is in
            // light or darkness.  We'll assume the latter: the plunger
            // is blocking the whole window or isn't in the frame at
            // all.  We'll also assume that the exposure level is
            // similar to that in recent frames where we *did* detect
            // the direction.  This means that if the new exposure level
            // (which is about the same over the whole array) is less
            // than the recent midpoint, we must be entirely blocked
            // by the plunger, so it's all the way forward; if the
            // brightness is above the recent midpoint, we must be
            // entirely exposed, so the plunger is all the way back.

            // figure the average of the recent midpoint brightnesses            
            int sum = 0;
            for (int i = 0 ; i < countof(midpt) ; sum += midpt[i++]) ;
            sum /= countof(midpt);
            
            // Figure the average of our two ends.  We have very
            // little contrast overall, so we already know that the
            // two ends are about the same, but we can't expect the
            // lighting to be perfectly uniform.  Averaging the ends
            // will smooth out variations due to light source placement,
            // sensor noise, etc.
            a = (a+b)/2;
            
            // Check if we seem to be fully exposed or fully covered
            pos = a < sum ? 0 : n;
            return true;
        }
        else
        {
            // We can't detect the orientation from this image, and 
            // we don't know it from previous images, so we have nothing
            // to go on.  Give up and return failure.
            return false;
        }
            
        // Figure the crossover brightness levels for detecting the edge.
        // The midpoint is the brightness level halfway between the bright
        // and dark regions we detected at the opposite ends of the sensor.
        // To find the edge, we'll look for a brightness level slightly 
        // *past* the midpoint, to help reject noise - the bright region
        // pixels should all cluster close to the higher level, and the
        // shadow region should all cluster close to the lower level.
        // We'll define "close" as within 1/3 of the gap between the 
        // extremes.
        int mid = (a+b)/2;

        // Scan from the bright side looking, for a pixel that drops below the
        // midpoint brightess.  To reduce false positives from noise, check to
        // see if the majority of the next few pixels stay in shadow - if not,
        // consider the dark pixel to be some kind of transient noise, and
        // continue looking for a more solid edge.
        for (int i = 5 ; i < n-5 ; ++i, bi += dir)
        {
            // check to see if we found a dark pixel
            if (pix[bi] < mid)
            {
                // make sure we have a sustained edge
                int ok = 0;
                int bi2 = bi + dir;
                for (int j = 0 ; j < 5 ; ++j, bi2 += dir)
                {
                    // count this pixel if it's darker than the midpoint
                    if (pix[bi2] < mid)
                        ++ok;
                }
                
                // if we're clearly in the dark section, we have our edge
                if (ok > 3)
                {
                    // Success.  Since we found an edge in this scan, save the
                    // midpoint brightness level in our history list, to help
                    // with any future frames with insufficient contrast.
                    midpt[midptIdx++] = mid;
                    midptIdx %= countof(midpt);
                    
                    // return the detected position
                    pos = i;
                    return true;
                }
            }
        }
        
        // no edge found
        return false;
    }
#endif // SCAN_METHOD 0
    
#if SCAN_METHOD == 1
    // Scan method 1: meet in the middle.
    bool process(uint8_t *pix, int n, int &pos)
    {        
        // Get the levels at each end
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4])/5;
        int b = (int(pix[n-1]) + pix[n-2] + pix[n-3] + pix[n-4] + pix[n-5])/5;
        
        // Figure the sensor orientation based on the relative brightness
        // levels at the opposite ends of the image.  We're going to scan
        // across the image from each side - 'bi' is the starting index
        // scanning from the bright side, 'di' is the starting index on
        // the dark side.  'binc' and 'dinc' are the pixel increments
        // for the respective indices.
        int bi, di;
        int binc, dinc;
        if (a > b+10)
        {
            // left end is brighter - standard orientation
            dir = 1;
            bi = 4, di = n - 5;
            binc = 1, dinc = -1;
        }
        else if (b > a+10)
        {
            // right end is brighter - reverse orientation
            dir = -1;
            bi = n - 5, di = 4;
            binc = -1, dinc = 1;
        }
        else
        {
            // can't detect direction
            return false;
        }
            
        // Figure the crossover brightness levels for detecting the edge.
        // The midpoint is the brightness level halfway between the bright
        // and dark regions we detected at the opposite ends of the sensor.
        // To find the edge, we'll look for a brightness level slightly 
        // *past* the midpoint, to help reject noise - the bright region
        // pixels should all cluster close to the higher level, and the
        // shadow region should all cluster close to the lower level.
        // We'll define "close" as within 1/3 of the gap between the 
        // extremes.
        int mid = (a+b)/2;
        int delta6 = abs(a-b)/6;
        int crossoverHi = mid + delta6;
        int crossoverLo = mid - delta6;

        // Scan inward from the each end, looking for edges.  Each time we
        // find an edge from one direction, we'll see if the scan from the
        // other direction agrees.  If it does, we have a winner.  If they
        // don't agree, we must have found some noise in one direction or the
        // other, so switch sides and continue the scan.  On each continued
        // scan, if the stopping point from the last scan *was* noise, we'll
        // start seeing the expected non-edge pixels again as we move on,
        // so we'll effectively factor out the noise.  If what stopped us
        // *wasn't* noise but was a legitimate edge, we'll see that we're
        // still in the region that stopped us in the first place and just
        // stop again immediately.  
        //
        // The two sides have to converge, because they march relentlessly
        // towards each other until they cross.  Even if we have a totally
        // random bunch of pixels, the two indices will eventually meet and
        // we'll declare that to be the edge position.  The processing time
        // is linear in the pixel count - it's equivalent to one pass over
        // the pixels.  The measured time for 1280 pixels is about 1.3ms,
        // which is about half the DMA transfer time.  Our goal is always
        // to complete the processing in less than the DMA transfer time,
        // since that's as fast as we can possibly go with the physical
        // sensor.  Since our processing time is overlapped with the DMA
        // transfer, the overall frame rate is limited by the *longer* of
        // the two times, not the sum of the two times.  So as long as the
        // processing takes less time than the DMA transfer, we're not 
        // contributing at all to the overall frame rate limit - it's like
        // we're not even here.
        for (;;)
        {
            // scan from the bright side
            for (bi += binc ; bi >= 5 && bi <= n-6 ; bi += binc)
            {
                // if we found a dark pixel, consider it to be an edge
                if (pix[bi] < crossoverLo)
                    break;
            }
            
            // if we reached an extreme, return failure
            if (bi < 5 || bi > n-6)
                return false;
            
            // if the two directions crossed, we have a winner
            if (binc > 0 ? bi >= di : bi <= di)
            {
                pos = (dir == 1 ? bi : n - bi);
                return true;
            }
            
            // they haven't converged yet, so scan from the dark side
            for (di += dinc ; di >= 5 && di <= n-6 ; di += dinc)
            {
                // if we found a bright pixel, consider it to be an edge
                if (pix[di] > crossoverHi)
                    break;
            }
            
            // if we reached an extreme, return failure
            if (di < 5 || di > n-6)
                return false;
            
            // if they crossed now, we have a winner
            if (binc > 0 ? bi >= di : bi <= di)
            {
                pos = (dir == 1 ? di : n - di);
                return true;
            }
        }
    }
#endif // SCAN METHOD 1

#if SCAN_METHOD == 2
    // Scan method 2: scan for steepest brightness slope.
    bool process(uint8_t *pix, int n, int &pos)
    {        
        // Get the levels at each end
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4])/5;
        int b = (int(pix[n-1]) + pix[n-2] + pix[n-3] + pix[n-4] + pix[n-5])/5;
        
        // Figure the sensor orientation based on the relative brightness
        // levels at the opposite ends of the image.  We're going to scan
        // across the image from each side - 'bi' is the starting index
        // scanning from the bright side, 'di' is the starting index on
        // the dark side.  'binc' and 'dinc' are the pixel increments
        // for the respective indices.
        if (a > b + 10)
        {
            // left end is brighter - standard orientation
            dir = 1;
        }
        else if (b > a + 10)
        {
            // right end is brighter - reverse orientation
            dir = -1;
        }
        else
        {
            // can't determine direction
            return false;
        }

        // scan for the steepest edge using the assembly language 
        // implementation (since the C++ version is too slow)
        const uint8_t *edgep = 0;
        if (ccdScanMode2(pix, n, &edgep, dir))
        {
            // edgep has the pixel array pointer; convert it to an offset
            pos = edgep - pix;
            
            // if the sensor orientation is reversed, figure the index from
            // the other end of the array
            if (dir < 0)
                pos = n - pos;
                
            // success
            return true;
        }
        else
        {
            // no edge found
            return false;
        }

    }
#endif // SCAN_METHOD 2

#if SCAN_METHOD == 3
    // Scan method 0: one-way scan; original method used in v1 firmware.
    bool process(uint8_t *pix, int n, int &pos)
    {        
        // Get the levels at each end
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4])/5;
        int b = (int(pix[n-1]) + pix[n-2] + pix[n-3] + pix[n-4] + pix[n-5])/5;
        
        // Figure the sensor orientation based on the relative brightness
        // levels at the opposite ends of the image.  We're going to scan
        // across the image from each side - 'bi' is the starting index
        // scanning from the bright side, 'di' is the starting index on
        // the dark side.  'binc' and 'dinc' are the pixel increments
        // for the respective indices.
        if (a > b+10)
        {
            // left end is brighter - standard orientation
            dir = 1;
        }
        else if (b > a+10)
        {
           // right end is brighter - reverse orientation
            dir = -1;
        }
        else
        {
            // We can't detect the orientation from this image
            return false;
        }
            
        // Figure the crossover brightness levels for detecting the edge.
        // The midpoint is the brightness level halfway between the bright
        // and dark regions we detected at the opposite ends of the sensor.
        // To find the edge, we'll look for a brightness level slightly 
        // *past* the midpoint, to help reject noise - the bright region
        // pixels should all cluster close to the higher level, and the
        // shadow region should all cluster close to the lower level.
        // We'll define "close" as within 1/3 of the gap between the 
        // extremes.
        int mid = (a+b)/2;

        // Count pixels brighter than the brightness midpoint.  We assume
        // that all of the bright pixels are contiguously within the bright
        // region, so we simply have to count them up.  Even if we have a
        // few noisy pixels in the dark region above the midpoint, these
        // should on average be canceled out by anomalous dark pixels in
        // the bright region.
        int bcnt = 0;
        for (int i = 0 ; i < n ; ++i)
        {
            if (pix[i] > mid)
                ++bcnt;
        }
        
        // The position is simply the size of the bright region
        pos = bcnt;
        if (dir < 1)
            pos = n - pos;
        return true;
    }
#endif // SCAN_METHOD 3
    
    
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
        
        ccd.startCapture();                 // read pixels from period A, begin integration period B
        ccd.wait();                         // wait for scan of A to complete, as minimum integration B time
        wait_us(long(extraTime) * 100);     // add extraTime (0.1ms == 100us increments) to integration B time
        ccd.startCapture();                 // read pixels from integration period B, begin period C; period A pixels now available
        ccd.startCapture();                 // read pixels from integration period C, begin period D; period B pixels now available
        
        // get the pixel array
        uint8_t *pix;
        int n;
        uint32_t t;
        ccd.getPix(pix, n, t);

        // start a timer to measure the processing time
        Timer pt;
        pt.start();

        // process the pixels and read the position
        int pos;
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
        
        // send the sensor status report report
        js.sendPlungerStatus(n, pos, dir, ccd.getAvgScanTime(), processTime);
        
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
        ccd.wait();
        ccd.clear();
        ccd.startCapture();
    }
    
    // get the average sensor scan time
    virtual uint32_t getAvgScanTime() { return ccd.getAvgScanTime(); }
    
protected:
    // Sensor orientation.  +1 means that the "tip" end - which is always
    // the brighter end in our images - is at the 0th pixel in the array.
    // -1 means that the tip is at the nth pixel in the array.  0 means
    // that we haven't figured it out yet.  We automatically infer this
    // from the relative light levels at each end of the array when we
    // successfully find a shadow edge.  The reason we save the information
    // is that we might occasionally get frames that are fully in shadow
    // or fully in light, and we can't infer the direction from such
    // frames.  Saving the information from past frames gives us a fallback 
    // when we can't infer it from the current frame.  Note that we update
    // this each time we can infer the direction, so the device will adapt
    // on the fly even if the user repositions the sensor while the software
    // is running.
    int dir;

    // History of midpoint brightness levels for the last few successful
    // scans.  This is a circular buffer that we write on each scan where
    // we successfully detect a shadow edge.  (It's circular, so we
    // effectively discard the oldest element whenever we write a new one.)
    //
    // We use the history in cases where we have too little contrast to
    // detect an edge.  In these cases, we assume that the entire sensor
    // is either in shadow or light, which can happen if the plunger is at
    // one extreme or the other such that the edge of its shadow is out of 
    // the frame.  (Ideally, the sensor should be positioned so that the
    // shadow edge is always in the frame, but it's not always possible
    // to do this given the constrained space within a cabinet.)  The
    // history helps us decide which case we have - all shadow or all
    // light - by letting us compare our average pixel level in this
    // frame to the range in recent frames.  This assumes that the
    // exposure level is fairly consistent from frame to frame, which 
    // is usually true because the sensor and light source are both
    // fixed in place.
    // 
    // We always try first to infer the bright and dark levels from the 
    // image, since this lets us adapt automatically to different exposure 
    // levels.  The exposure level can vary by integration time and the 
    // intensity and positioning of the light source, and we want
    // to be as flexible as we can about both.
    uint8_t midpt[10];
    uint8_t midptIdx;
    
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
