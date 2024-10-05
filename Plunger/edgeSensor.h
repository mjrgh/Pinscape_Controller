// Edge position sensor - 2D optical
//
// This class implements our plunger sensor interface using edge
// detection on a 2D optical sensor.  With this setup, a 2D optical
// sensor is placed close to the plunger, parallel to the rod, with a 
// light source opposite the plunger.  This makes the plunger cast a
// shadow on the sensor.  We figure the plunger position by detecting
// where the shadow is, by finding the edge between the bright and
// dark regions in the image.
//
// This class is designed to work with any type of 2D optical sensor.
// We have subclasses for the TSL1410R and TSL1412S sensors, but other
// similar sensors could be supported as well by adding interfaces for
// the physical electronics.  For the edge detection, we just need an 
// array of pixel readings.

#ifndef _EDGESENSOR_H_
#define _EDGESENSOR_H_

#include "plunger.h"


// Assembler routine to scan for an edge using the Steepest Slope algorithm
extern "C" int edgeScanBySlope(const uint8_t *pix, int npix, const uint8_t **edgePtr, int dir);

// PlungerSensor interface implementation for edge detection setups.
// This is a generic base class for image-based sensors where we detect
// the plunger position by finding the edge of the shadow it casts on
// the detector.
//
// Edge sensors use the image pixel span as the native position scale,
// since a position reading is the pixel offset of the shadow edge.
class PlungerSensorEdgePos: public PlungerSensorImage<int>
{
public:
    PlungerSensorEdgePos(PlungerSensorImageInterface &sensor, int npix, int scanMode)
        : PlungerSensorImage(sensor, npix, npix - 1)
    {
        // select the scan mode
        setScanMode(scanMode);

        // initialize scan method variables
        prvRawResult0 = 0;
        prvRawResult1 = 0;

        // initialization for variables used only in old scan methods
        // midptIdx = 0;
        // memset(midpt, 0, sizeof(midpt));
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
    //
    bool process(const uint8_t *pix, int n, int &pos, int &processResult)
    {
        // call the selected scan method implementation
        return (this->*scanMethodFunc)(pix, n, pos, processResult);
    }

    virtual void onConfigChange(int varno, Config &cfg)
    {
        switch (varno)
        {
        case 5:
            // Plunger sensor type and sensor-specific parameter "param1".
            // We use param1 to select the scan mode.  Update to the new mode.
            setScanMode(cfg.plunger.param1);
            break;
        }

        // inherit the default handling
        PlungerSensorImage::onConfigChange(varno, cfg);
    }

    // Set the scan mode
    void setScanMode(int mode)
    {
        switch (mode)
        {
        case 0:
        default:
            scanMethodFunc = &PlungerSensorEdgePos::scanBySteadySlope;
            break;

        case 1:
            scanMethodFunc = &PlungerSensorEdgePos::scanBySteepestSlope;
            break;

        case 2:
            scanMethodFunc = &PlungerSensorEdgePos::scanBySlopeAcrossGap;
            break;
        }
    }

protected:
    
    // "Steepest Slope" scanning method.  This is the method used for
    // many years in the v2 firmware.
    //
    // This scans the whole image and looks for the position with the 
    // highest brightness difference across adjacent pixels.  We average
    // over a window of several pixels on each side of each position,
    // to smooth out pixel noise.  This should avoid treating a single
    // noisy pixel as having a steep slope adjacent to it.
    //
    // This algorithm proved to be much better than the original v1
    // firmware method, which simply looked for a single pixel that was
    // dark enough to count as shadow.  The v1 method was too easily
    // fooled by noise.  This algorithm takes more context into account,
    // since it looks for an edge by the difference in brightness at
    // adjacent pixels.
    //
    // This method is compute-intensive method, because it scans the whole
    // sensor, and computes an average of a few pixels at every position.
    // An assembly language implemementation is necessary to make it fast
    // enough on the KL25Z.  The algorithm has a fixed execution time because
    // it always does one full pass over the whole pixel array.
    virtual bool scanBySteepestSlope(const uint8_t *pix, int n, int &pos, int& /*processResult*/)
    {        
        // Get the levels at each end by averaging across several pixels.
        // Compute just the sums: don't bother dividing by the count, since 
        // the sums are equivalent to the averages as long as we know 
        // everything is multiplied by the number of samples.
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4]);
        int b = (int(pix[n-1]) + pix[n-2] + pix[n-3] + pix[n-4] + pix[n-5]);
        
        // Figure the sensor orientation based on the relative brightness
        // levels at the opposite ends of the image.  We're going to scan
        // across the image from each side - 'bi' is the starting index
        // scanning from the bright side, 'di' is the starting index on
        // the dark side.  'binc' and 'dinc' are the pixel increments
        // for the respective indices.
        if (a > b + 50)
        {
            // left end is brighter - standard orientation
            dir = 1;
        }
        else if (b > a + 50)
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
        if (edgeScanBySlope(pix, n, &edgep, dir))
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

    // "Steepest Slope Across a Gap" scanning method.
    //
    // This is a refinement of the "Steepest Slope" scanning method that
    // scans for the steepest slope (biggest difference in brightness)
    // across a gap, rather than between immediately adjacent pixels.
    // The gap represents the expected fuzziness in the edge.  In the
    // reference hardware configuration for the TSL14xx sensors, there
    // are no optics involved; the sensor sits close to the plunger,
    // with a light source on the other side arranged so that the plunger
    // casts a shadow on the sensor.  Since this is an unfocused shadow,
    // it has a penumbra.  What's more, the mechanical plunger can move
    // quickly enough that it can cause significant motion blur by
    // changing position by as much as 200 pixels in the course of a
    // single 2.5ms exposure, which makes the sloping region even wider.
    //
    // To account for motion blur in addition to a stationary penumbra,
    // the algorithm uses the last two position readings to estimate the
    // current speed during the new exposure.  This is imperfect, since
    // it doesn't take into account acceleration, but the 2.5 ms exposure
    // time is short enough that the previous speed is still a pretty
    // fair estimate of the current speed.  When the speed is zero, we
    // still use a gap of a few pixels to approximate the penumbra of
    // a stationary shadow.
    //
    // This scan algorithm was added when the new plunger speed reporting
    // in the HID joystick interface was added, because it became apparent
    // once we started reporting the speed that the basic "Steepest Slope"
    // algorithm could read images with heavy motion blur.  It tended to
    // find false edges and thus yield inaccurate position readings when
    // the plunger is moving at speeds near the peak speeds it tends to
    // hit when pulled back all the way and released to be driven forward
    // by the spring.  This algorithm is much better able to read these
    // motion-blurred images, since it compensates for the expected blur
    // by increasing the gap size.
    bool scanBySlopeAcrossGap(const uint8_t *pix, int n, int &pos, int& /*processResult*/)
    {
        // Get the levels at each end
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4]);
        int b = (int(pix[n-1]) + pix[n-2] + pix[n-3] + pix[n-4] + pix[n-5]);
        
        // Figure the sensor orientation based on the relative brightness
        // levels at the opposite ends of the image.  We're going to scan
        // across the image from each side - 'bi' is the starting index
        // scanning from the bright side, 'di' is the starting index on
        // the dark side.  'binc' and 'dinc' are the pixel increments
        // for the respective indices.
        if (a > b + 50)
        {
            // left end is brighter - standard orientation
            dir = 1;
        }
        else if (b > a + 50)
        {
           // right end is brighter - reverse orientation
            dir = -1;
        }
        else
        {
            // We can't detect the orientation from this image
            return false;
        }

        // Calculate the expected gap size based on the previous delta.
        // Each exposure takes almost the full time between frames, so
        // there will be motion blur in each frame equal to the distance
        // the plunger moves over the course of the frame.  At ~2.5ms per
        // image, the speed doesn't change much from one frame to the
        // next, so the trailing speed is a pretty good approximation of
        // the new speed and thus of the expected motion blur.  Sizing
        // the gap to the expected motion blur improves our chances of
        // identifying the position in a frame with fast motion.
        int prvDelta = abs(prvRawResult0 - prvRawResult1);
        const int gapSize = prvDelta < 3 ? 3 : prvDelta > 175 ? 175 : prvDelta;

        // Initialize a pair of rolling-average windows.  This sensor tends
        // to have a bit of per-pixel noise, so if we looked at the slope
        // from one pixel to the next, we'd see a lot of steep edges from
        // the noise alone.  Averaging a few pixels smooths out that
        // high-frequency noise.  We use two windows because we're looking
        // for the edge of the shadow, so we want to know where the average
        // suddenly changes across a small gap.  The standard physical setup
        // with this sensor doesn't use focusing optics, so the shadow is a
        // little fuzzy, crossing a few pixels; the gap is meant to
        // approximate the fuzzy extent of the shadow.
        const int windowSize = 8;  // must be power of two
        uint8_t window1[windowSize], window2[windowSize];
        unsigned int sum1 = 0, sum2 = 0;
        int iPix1 = dir < 0 ? n - 1 : 0;
        for (int i = 0 ; i < windowSize ; ++i, iPix1 += dir)
            sum1 += (window1[i] = pix[iPix1]);

        int iGap = iPix1 + dir*gapSize/2;
        int iPix2 = iPix1 + dir*gapSize;
        for (int i = 0 ; i < windowSize ; ++i, iPix2 += dir)
            sum2 += (window2[i] = pix[iPix2]);

        // search for the steepest bright-to-dark gradient
        int steepestSlope = 0;
        int steepestIdx = 0;
        for (int i = windowSize*2 + gapSize, wi = 0 ; i < n ; ++i, iPix1 += dir, iPix2 += dir, iGap += dir)
        {
            // compute the slope at the current gap
            int slope = sum1 - sum2;

            // record the steepest slope
            if (slope > steepestSlope)
            {
                steepestSlope = slope;
                steepestIdx = iGap;
            }

            // move to the next pixel in each window
            sum1 -= window1[wi];
            sum1 += (window1[wi] = pix[iPix1]);
            sum2 -= window2[wi];
            sum2 += (window2[wi] = pix[iPix2]);

            // advance and wrap the window index
            wi += 1;
            wi &= ~windowSize;
        }

        // Reject the reading if the steepest slope is too shallow, which
        // indicates that the contrast is too low to take a reading.
        if (steepestSlope < 8*windowSize)
            return false;  

        // return the best slope point
        pos = steepestIdx;

        // update the previous results
        prvRawResult1 = prvRawResult0;
        prvRawResult0 = pos;

        // if the sensor orientation is reversed, figure the index from
        // the other end of the array
        if (dir < 0)
            pos = n - pos;

        // success            
        return true;
    }

    // Previous raw results, to estimate the plunger speed expected
    // during the new frame.  A moving plunger causes motion blur,
    // which makes the shadow gap wider.  We can compensate for the
    // blur by looking for a shadow blur of the expected size, if we
    // know the speed.  The exposure time is quick enough that the
    // speed doesn't change very much from one frame to the next, so
    // the trailing speed from the last two frames gives us a decent
    // estimate for the new frame's speed.
    int prvRawResult0, prvRawResult1;

    // "Steady slope" edge scan algorithm.  This algorithm searches
    // for a region where the pixels for a monotonic slope from bright
    // to dark.
    //
    // This algorithm is similar to the "steepest slope" searches, but
    // instead of measuring the steepness of the slope, it merely looks
    // for consistency of slope.  This makes it adapt automatically to
    // the width of the blurring from the stationary penumbra and motion,
    // so it doesn't need to guess at the current speed (as the "gap"
    // modification of the steepest-slope search does).
    //
    // This method also looks for a flat shadow section after the sloping
    // region, to confirm that the slope is really the border between the
    // two regions and not a local dip (due to sensor noise, say).
    bool scanBySteadySlope(const uint8_t *pix, int nPixels, int &pos, int& /*processResult*/)
    {
        // Get the levels at each end
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4])/5;
        int b = (int(pix[nPixels-1]) + pix[nPixels-2] + pix[nPixels-3] + pix[nPixels-4] + pix[nPixels-5])/5;

        // Figure the sensor orientation based on the relative brightness
        // levels at the opposite ends of the image.  We're going to scan
        // across the image from each side - 'bi' is the starting index
        // scanning from the bright side, 'di' is the starting index on
        // the dark side.  'binc' and 'dinc' are the pixel increments
        // for the respective indices.
        int dir;
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
            // We can't detect the orientation from this image
            return false;
        }

        // figure the midpoint brightness
        int midpt = ((a + b)/2);

        // Figure the bright and dark thresholds at the quarter points
        int brightThreshold = ((a > b ? a : b) + midpt)/2;
        int darkThreshold = ((a < b ? a : b) + midpt)/2;

        // rolling-average window size
        const int windowShift = 3;
        const int windowSize = (1 << windowShift);  // must be power of two
        const int windowMask = windowSize - 1;

        // Search for the starting point.  The core algorithm searches for
        // the shadow from the bright side, so if the plunger is all the way
        // back, we'd have to scan the entire sensor length if we started at
        // the bright end.  We can save a lot of time by skipping most of
        // the bright section, by doing a binary search for a point where
        // the brightness dips below the bright threshold.
        int leftIdx = 0;
        int rightIdx = nPixels - 1;
        int leftAvg = (pix[leftIdx] + pix[leftIdx+1] + pix[leftIdx+2] + pix[leftIdx+3])/4;
        int rightAvg = (pix[rightIdx] + pix[rightIdx-1] + pix[rightIdx-2] + pix[rightIdx-3])/4;
        for (int i = 0 ; i < 8 ; ++i)
        {
            // find the halfway point in this division
            int centerIdx = (leftIdx + rightIdx)/2;
            int centerAvg = (pix[centerIdx-1] + pix[centerIdx] + pix[centerIdx+1] + pix[centerIdx+2])/4;

            // move the bounds towards the dark region
            if (dir < 0 ? centerAvg < brightThreshold : centerAvg > brightThreshold)
            {
                // center is in same region as left side, so move right
                leftIdx = centerIdx - windowSize;
                leftAvg = (pix[leftIdx] + pix[leftIdx+1] + pix[leftIdx+2] + pix[leftIdx+3])/4;
            }
            else
            {
                // center is in same region as right side, so move left
                rightIdx = centerIdx + windowSize;
                rightAvg = (pix[rightIdx] + pix[rightIdx-1] + pix[rightIdx-2] + pix[rightIdx-3])/4;
            }
        }

        // We sometimes land with the range exactly starting or ending at
        // the transition point, so make sure we have enough runway on either
        // side to detect the steady state and slope we look for in the loop.
        leftIdx = (leftIdx > windowSize) ? leftIdx - windowSize : 0;
        rightIdx = (rightIdx < nPixels - windowSize) ? rightIdx + windowSize : nPixels - 1;

        // Adjust the points for the window sum.  The window is an average
        // over windowSize pixels, but to save work in the loop, we don't
        // divide by the number of samples, so the value we actually work
        // with is (average * N) == (average * windowSize).  So all of our
        // reference points have to be likewise adjusted.
        midpt <<= windowShift;
        darkThreshold <<= windowShift;

        // initialize the rolling-average window, starting at the bright end
        // of the region we narrowed down to with the binary search
        int iPix = dir < 0 ? rightIdx : leftIdx;
        int nScan = (dir < 0 ? iPix - windowSize : nPixels - iPix - windowSize);
        uint8_t window[windowSize];
        unsigned int sum = 0;
        for (int i = 0 ; i < windowSize ; ++i, iPix += dir)
            sum += (window[i] = pix[iPix]);

        // search for a monotonic falling edge
        int prv = sum;
        int edgeStart = -1;
        int edgeMid = -1;
        int nShadow = 0;
        int edgeFound = -1;
        for (int i = windowSize, wi = 0 ; i < nScan ; ++i, iPix += dir)
        {
            // advance the rolling window
            sum -= window[wi];
            sum += (window[wi] = pix[iPix]);

            // advance and wrap the window index
            wi += 1;
            wi &= windowMask;

            // check for a falling edge
            if (sum < prv)
            {
                // dropping - start or continue the falling edge
                if (edgeStart < 0)
                    edgeStart = iPix;
            }
            else if (sum > prv)
            {
                // rising - cancel the falling edge
                edgeStart = -1;
            }

            // are we in an edge?
            if (edgeStart >= 0)
            {
                // check for a midpoint crossover, which we'll take as the edge position
                if (prv > midpt && sum <= midpt)
                    edgeMid = iPix;

                // if we've reached the dark threshold, count it as a potential match
                if (sum < darkThreshold)
                    edgeFound = edgeMid;
            }

            // If we're above the midpoint, cancel any match position.  We must
            // have encountered a dark patch where the brightness dipped briefly
            // but didn't actually cross into the shadow zone.
            if (sum > midpt)
            {
                edgeFound = -1;
                nShadow = 0;
            }

            // if we have a potential match, check if we're still in shadow
            if (edgeFound && sum < darkThreshold)
            {
                // count the dark region
                ++nShadow;

                // if we've seen enough contiguous shadow, declare success
                if (nShadow > 10)
                {
                    pos = edgeFound;
                    return true;
                }
            }

            // remember the previous item
            prv = sum;
        }

        // no edge found
        return false;
    }

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
    virtual int getOrientation() const { return dir; }
    int dir;

    // Scan method function
    bool (PlungerSensorEdgePos::*scanMethodFunc)(const uint8_t *pix, int n, int &pos, int &processResult);


    // --------------------------------------------------------------------
    //
    // Experimental scan methods, not included in the build.  These are
    // mostly experimental methods that didn't pan out (they performed
    // poorly compared with the other methods).  They're preserved here
    // mostly as a record of what's been tried, so that if someone comes
    // up with one of these ideas again, they can quickly test it out to
    // re-evaluate it and/or try new variations on the same idea, without
    // having to re-code the whole algorithm.
    //

#if 0  // NOT INCLUDED IN BUILD

    // One-way scan.  This is the original algorithm from the v1 software,
    // with some slight improvements.  We start at the brighter end of the
    // sensor and scan until we find a pixel darker than a threshold level 
    // (halfway between the respective brightness levels at the bright and 
    // dark ends of the sensor).  The original v1 algorithm simply stopped
    // there.  This version is slightly improved: it scans for a few more 
    // pixels to make sure that the majority of the adjacent pixels are 
    // also in shadow, to help reject false edges from sensor noise or 
    // optical shadows that make one pixel read darker than it should.
    bool scanByOneWayScan(const uint8_t *pix, int n, int &pos, int& /*processResult*/)
    {        
        // Get the levels at each end
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4]);
        int b = (int(pix[n-1]) + pix[n-2] + pix[n-3] + pix[n-4] + pix[n-5]);
        
        // Figure the sensor orientation based on the relative brightness
        // levels at the opposite ends of the image.  We're going to scan
        // across the image from each side - 'bi' is the starting index
        // scanning from the bright side, 'di' is the starting index on
        // the dark side.  'binc' and 'dinc' are the pixel increments
        // for the respective indices.
        int bi;
        if (a > b + 50)
        {
            // left end is brighter - standard orientation
            dir = 1;
            bi = 4;
        }
        else if (b > a + 50)
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
            a = (a+b)/10;
            
            // Check if we seem to be fully exposed or fully covered.
            pos = a < sum ? 0 : n;
            
            // stop here with a successful reading
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
        int mid = (a+b)/10;

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

    // "Meet in the middle" scan method.
    //
    // This approach sets up with pointers to both ends of the pixel
    // array, and scans inward from each end.  For the scan from the 
    // dark end, we stop when we reach a pixel that's brighter than
    // the average dark level by 2/3 of the gap between the dark and
    // bright levels.  For the scan from the bright end, we stop when
    // we reach a pixel that's darker by 2/3 of the gap.  Each time we
    // stop, we look to see if the other scan has reached the same 
    // stopping point.  If so, the two scans converged on a common
    // point, which we take to be the edge between the dark and bright
    // sections.  If the two scans haven't converged yet, we switch to
    // the other scan and continue it.  We repeat this process until
    // the two converge.  The benefit of this approach vs the original
    // one-way scan is that it's more tolerant of noise, since both
    // scans have to converge at the same point; the original one-way
    // scan stopped as soon as it found a pixel matching the shadow
    // criteria, which made it easy to fool with a single noisy pixel
    // reading darker than it should.  The two-ended scan avoids
    // that by noticing that the other end hasn't reached the same
    // point yet, implying that the pixel crossing the threshold is
    // just a noise blip that we should ignore.
    //
    // Despite the improvement over the original one-way scan, this
    // method still doesn't consider enough context, so it's still
    // easily fooled by noise.
    bool scanByMeetInTheMiddle(const uint8_t *pix, int n, int &pos, int& /*processResult*/)
    {        
        // Get the levels at each end
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4]);
        int b = (int(pix[n-1]) + pix[n-2] + pix[n-3] + pix[n-4] + pix[n-5]);
        
        // Figure the sensor orientation based on the relative brightness
        // levels at the opposite ends of the image.  We're going to scan
        // across the image from each side - 'bi' is the starting index
        // scanning from the bright side, 'di' is the starting index on
        // the dark side.  'binc' and 'dinc' are the pixel increments
        // for the respective indices.
        int bi, di;
        int binc, dinc;
        if (a > b + 50)
        {
            // left end is brighter - standard orientation
            dir = 1;
            bi = 4, di = n - 5;
            binc = 1, dinc = -1;
        }
        else if (b > a + 50)
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
        int mid = (a+b)/10;
        int delta6 = abs(a-b)/30;
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

    // "Total bright pixel count" scan method.
    //
    // This method simply adds up the total number of pixels above a
    // threshold brightness, without worrying about whether they're
    // contiguous with other pixels on the same side of the edge.  Since
    // we know there's always exactly one actual shadow edge from the
    // physical plunger, all of the dark pixels should in principle be
    // on one side, and all of the light pixels should be on the other
    // side.  There might be some noise that creates isolated pixels that
    // don't match their neighbors, but these should average out.  The
    // virtue of this approach (apart from its simplicity) is that it
    // should be immune to false edges - local spikes due to noise - that
    // might fool the algorithms that explicitly look for edges.  In
    // practice, though, it seems to be even more sensitive to noise
    // than the other algorithms, probably because it treats every pixel
    // as independent and thus doesn't have any sort of inherent noise
    // reduction from considering relationships among pixels.
    bool scanByTotalBrightPixelCount(const uint8_t *pix, int n, int &pos, int& /*processResult*/)
    {        
        // Get the levels at each end
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4]);
        int b = (int(pix[n-1]) + pix[n-2] + pix[n-3] + pix[n-4] + pix[n-5]);
        
        // Figure the sensor orientation based on the relative brightness
        // levels at the opposite ends of the image.  We're going to scan
        // across the image from each side - 'bi' is the starting index
        // scanning from the bright side, 'di' is the starting index on
        // the dark side.  'binc' and 'dinc' are the pixel increments
        // for the respective indices.
        if (a > b + 50)
        {
            // left end is brighter - standard orientation
            dir = 1;
        }
        else if (b > a + 50)
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
        int mid = (a+b)/10;

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

#endif // 0 - disabled scan methods
};


#endif /* _EDGESENSOR_H_ */
