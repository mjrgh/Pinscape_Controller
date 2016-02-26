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
        
        // no hysteresis zone yet
        hyst1 = hyst2 = 0xFFFF;
    }
    
    // initialize
    virtual void init()
    {
        // flush any random power-on values from the CCD's integration
        // capacitors, and start the first integration cycle
        ccd.clear();
    }
    
    // Read the plunger position
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
        if (process(pix, n, pixpos, 0))
        {
            // Success.  Apply hysteresis.
            if (pixpos == hyst1)
            {
                // this is the same as the last reading, so no adjustment
                // is needed to the position OR the hysteresis range
            }
            else if (pixpos == hyst2)
            {
                // We're at the "jitter" end of the current hysteresis
                // range.  Eliminate the jitter by returning the "stable"
                // reading instead.
                pixpos = hyst1;
            }
            else if (hyst2 == 0xFFFF && (pixpos == hyst1+1 || pixpos == hyst1-1))
            {
                // There's no hysteresis range yet, and we're exactly one
                // pixel off from the previous reading.  Treat this new
                // reading as jitter.  We now know that the jitter will
                // occur in this direction from the last reading, so set
                // it as the "jitter" end of the hysteresis range.  Then
                // report the last stable reading to eliminate the jitter.
                hyst2 = pixpos;
                pixpos = hyst1;
            }
            else
            {
                // We're not inside the previous hysteresis range, so we're
                // breaking free of the hysteresis band.  Reset the hysteresis
                // to an empty range starting at the current position, and
                // report the new position as detected.
                hyst1 = pixpos;
                hyst2 = 0xFFFF;
            }
            
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
        
    // Process an image.  Applies noise reduction and looks for edges.
    // If we detect the plunger position, we set 'pos' to the pixel location
    // of the edge and return true; otherwise we return false.  The 'pos'
    // value returned, if any, is adjusted for sensor orientation so that
    // it reflects the logical plunger position.
    //
    // 'visMode' is the visualization mode.  If non-zero, we replace the
    // pixels in the 'pix' array with a new version for visual presentation
    // to the user, as an aid to setup and debugging.  The visualization
    // modes are:
    //
    //   0 = No visualization
    //   1 = High contrast: we set each pixel to white or black according
    //       to whether it's brighter or dimmer than the midpoint brightness 
    //       we use to seek the shadow edge.  This mode makes the edge 
    //       positions visually apparent.
    //   2 = Edge mode: we set all pixels to white except for detected edges,
    //       which we set to black.
    //
    // The 'pix' array is overwritten with the processed pixels.  If visMode
    // is 0, this reflects only the basic preprocessing we do in an edge
    // scan, such as noise reduction.  For other visualization modes, the
    // pixels are replaced by the visualization results.
    bool process(uint8_t *pix, int &n, int &pos, int visMode)
    {
        // Get the levels at each end
        int a = (int(pix[0]) + pix[1] + pix[2] + pix[3] + pix[4])/5;
        int b = (int(pix[n-1]) + pix[n-2] + pix[n-3] + pix[n-4] + pix[n-5])/5;
        
        // Figure the sensor orientation based on the relative 
        // brightness levels at the opposite ends of the image
        int pi;
        if (a > b+10)
        {
            // left end is brighter - standard orientation
            dir = 1;
            pi = 5;
        }
        else if (b > a+10)
        {
           // right end is brighter - reverse orientation
            dir = -1;
            pi = n - 6;
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
            sum /= 10;
            
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
            
        // figure the midpoint brigthness
        int mid = (a+b)/2;
            
        // Scan from the bright side looking for an edge
        for (int i = 5 ; i < n-5 ; ++i, pi += dir)
        {
            // check to see if we found a dark pixel
            if (pix[pi] < mid)
            {
                // make sure we have a sustained edge
                int ok = 0;
                int pi2 = pi + dir;
                for (int j = 0 ; j < 5 ; ++j, pi2 += dir)
                {
                    // count this pixel if it's darker than the midpoint
                    if (pix[pi2] < mid)
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


#if 0
    bool process3(uint8_t *pix, int &n, int &pos, int visMode)
    {
        // First, reduce the pixel array resolution to 1/4 of the 
        // native sensor resolution.  The native 400 dpi is higher
        // than we need for good results, so we can afford to cut 
        // this down a bit.  Reducing the resolution  gives us
        // a little simplistic noise reduction (by averaging adjacent
        // pixels), and it speeds up the rest of the edge finder by
        // making the data set smaller.
        //
        // While we're scanning, collect the brightness range of the
        // reduced pixel set.
        register int src, dst;
        int lo = pix[0], hi = pix[0];
        for (src = 0, dst = 0 ; src < n ; )
        {
            // compute the average of this pixel group
            int p = (int(pix[src++]) + pix[src++] + pix[src++] + pix[src++]) / 4;
            
            // note if it's the new high or low point
            if (p > hi)
                hi = p;
            else if (p < lo)
                lo = p;
            
            // Store the result back into the original array.  Note
            // that there's no risk of overwriting anything we still
            // need, since the pixel set is shrinking, so the write 
            // pointer is always behind the read pointer.
            pix[dst++] = p;
        }
        
        // set the new array size
        n = dst;

        // figure the midpoint brightness
        int mid = (hi + lo)/2;
        
        // Look at the first few pixels on the left and right sides
        // to try to detect the sensor orientation. 
        int left = pix[0] + pix[1] + pix[2] + pix[3];
        int right = pix[n-1] + pix[n-2] + pix[n-3] + pix[n-4];
        if (left > right + 40)
        {
            // left side is brighter - standard orientation
            dir = 1;
        }
        else if (right > left + 40)
        {
            // right side is brighter - reversed orientation
            dir = -1;
        }
        
        // scan for edges according to the direction
        bool found = false;
        if (dir == 0)
        {
        }
        else
        {
            // scan from the bright end to the dark end
            int stop;
            if (dir == 1)
            {
                src = 0;
                stop = n;
            }
            else
            {
                src = n - 1;
                stop = -1;
            }

            // scan through the pixels
            for ( ; src != stop ; src += dir)
            {
                // if this pixel is darker than the midpoint, we might 
                // have an edge
                if (pix[src] < mid)
                {
                    // make sure it's not just noise by checking the next
                    // few to make sure they're also darker
                    if (dir > 0)
                        dst = src + 10 > n ? n : src + 10;
                    else
                        dst = src - 10 < 0 ? -1 : src - 10;
                    int i, nok;
                    for (nok = 0, i = src ; i != dst ; i += dir)
                    {
                        if (pix[i] < mid)
                            ++nok;
                    }
                    if (nok > 6)
                    {
                        // we have a winner
                        pos = src;
                        found = true;
                        break;
                    }
                }
            }
        }

        // return the result
        return found;        
    }
#endif

#if 0
    bool process2(uint8_t *pix, int n, int &pos, int visMode)
    {
        // find the high and low brightness levels, and sum
        // all pixels (for the running averages)
        register int i;
        long sum = 0;
        int lo = 255, hi = 0;
        for (i = 0 ; i < n ; ++i)
        {
            int p = pix[i];
            sum += p;
            if (p > hi) hi = p;
            if (p < lo) lo = p;
        }
        
        // Figure the midpoint brightness
        int mid = (lo + hi)/2;
        
        // Scan for edges.  An edge is where adjacent pixels are
        // on opposite sides of the brightness midpoint.  For each
        // edge, we'll compute the "steepness" as the difference
        // between the average brightness on each side.  We'll
        // keep only the steepest edge.
        register int bestSteepness = -1;
        register int bestPos = -1;
        register int sumLeft = 0;
        register int prv = pix[0], nxt = pix[1];
        for (i = 1 ; i < n ; prv = nxt, nxt = pix[++i])
        {
            // figure the new sums left and right of the i:i+1 boundary
            sumLeft += prv;
            
            // if this is an edge, check if it's the best edge
            if (((mid - prv) & 0x80) ^ ((mid - nxt) & 0x80))
            {
                // compute the steepness
                int steepness = sumLeft/i - (sum - sumLeft)/(n-i);
                if (steepness > bestSteepness)
                {
                    bestPos = i;
                    bestSteepness = steepness;
                }
            }
        }
        
        // if we found a position, return it
        if (bestPos >= 0)
        {
            pos = bestPos;
            return true;
        }
        else
        {
            return false;
        }
    }
#endif

#if 0
    bool process1(uint8_t *pix, int n, int &pos, int visMode)
    {
        // presume failure
        bool ret = false;
        
        // apply noise reduction
        noiseReduction(pix, n);
        
        // make a histogram of brightness values
        uint8_t hist[256];
        memset(hist, 0, sizeof(hist));
        for (int i = 0 ; i < n ; ++i)
        {
            // get this pixel brightness, and count it in the histogram,
            // stopping if we hit the maximum count of 255
            int b = pix[i];
            if (hist[b] < 255)
                ++hist[b];
        }
        
        // Find the high and low bounds.  To avoid counting outliers that
        // might be noise, we'll scan in from each end of the brightness 
        // range until we find a few pixels at or outside that level.
        int cnt, lo, hi;
        const int mincnt = 10;
        for (cnt = 0, lo = 0 ; lo < 255 ; ++lo)
        {
            cnt += hist[lo];
            if (cnt >= mincnt)
                break;
        }
        for (cnt = 0, hi = 255 ; hi >= 0 ; --hi)
        {
            cnt += hist[hi];
            if (cnt >= mincnt)
                break;
        }
        
        // figure the inferred midpoint brightness level
        uint8_t m = uint8_t((int(lo) + int(hi))/2);
        
        // Try finding an edge with the inferred brightness range
        if (findEdge(pix, n, m, pos, false))
        {
            // Found it!  This image has sufficient contrast to find
            // an edge, so save the midpoint brightness for next time in
            // case the next image isn't as clear.
            midpt[midptIdx] = m;
            midptIdx = (midptIdx + 1) % countof(midpt);
            
            // Infer the sensor orientation.  If pixels at the bottom 
            // of the array are brighter than pixels at the top, it's in the
            // standard orientation, otherwise it's the reverse orientation.
            int a = int(pix[0]) + int(pix[1]) + int(pix[2]);
            int b = int(pix[n-1]) + int(pix[n-2]) + int(pix[n-3]);
            dir = (a > b ? 1 : -1);
            
            // if we're in the reversed orientation, mirror the position
            if (dir < 0)
                pos = n-1 - pos;
                
            // success
            ret = true;
        }
        else
        {
            // We didn't find a clear edge using the inferred exposure
            // level.  This might be because the image is entirely in or out
            // of shadow, with the plunger's shadow's edge out of the frame.
            // Figure the average of the recent history of successful frames
            // so that we can check to see if we have a low-contrast image
            // that's entirely above or below the recent midpoints.
            int avg = 0;
            for (int i = 0 ; i < countof(midpt) ; avg += midpt[i++]) ;
            avg /= countof(midpt);
            
            // count how many we have above and below the midpoint
            int nBelow = 0, nAbove = 0;
            for (int i = 0 ; i < avg ; nBelow += hist[i++]) ;
            for (int i = avg + 1 ; i < 255 ; nAbove += hist[i++]) ;
            
            // check if we're mostly above or below (we don't require *all*,
            // to allow for some pixel noise remaining)
            if (nBelow < 50)
            {
                // everything's bright -> we're in full light -> fully retracted
                pos = n - 1;
                ret = true;
            }
            else if (nAbove < 50)
            {
                // everything's dark -> we're in full shadow -> fully forward
                pos = 0;
                ret = true;
            }
            
            // for visualization purposes, use the previous average as the midpoint
            m = avg;
        }
        
        // If desired, apply the visualization mode to the pixels
        switch (visMode)
        {
        case 2:
            // High contrast mode.  Peg each pixel to the white or black according
            // to which side of the midpoint it's on.
            for (int i = 0 ; i < n ; ++i)
                pix[i] = (pix[i] < m ? 0 : 255);
            break;
            
        case 3:
            // Edge mode.  Re-run the edge analysis in visualization mode.
            {
                int dummy;
                findEdge(pix, n, m, dummy, true);
            }
            break;
        }
        
        // return the result
        return ret;
    }
    
    // Apply noise reduction to the pixel array.  We use a simple rank
    // selection median filter, which is fast and seems to produce pretty
    // good results with data from this sensor type.  The filter looks at
    // a small window around each pixel; if a given pixel is the outlier
    // within its window (i.e., it has the maximum or minimum brightness 
    // of all the pixels in the window), we replace it with the median
    // brightness of the pixels in the window.  This works particularly
    // well with the structure of the image we expect to capture, since
    // the image should have stretches of roughly uniform brightness -
    // part fully exposed and part in the plunger's shadow.  Spiky
    // variations in isolated pixels are almost guaranteed to be noise.
    void noiseReduction(uint8_t *pix, int n)
    {
        // set up a rolling window of pixels
        uint8_t w[7] = { pix[0], pix[1], pix[2], pix[3], pix[4], pix[5], pix[6] };
        int a = 0;
        
        // run through the pixels
        for (int i = 0 ; i < n ; ++i)
        {
            // set up a sorting array for the current window
            uint8_t tmp[7] = { w[0], w[1], w[2], w[3], w[4], w[5], w[6] };
            
            // sort it (using a Bose-Nelson sorting network for N=7)
#define SWAP(x, y) { \
                const int a = tmp[x], b = tmp[y]; \
                if (a > b) tmp[x] = b, tmp[y] = a; \
            }
            SWAP(1, 2);
            SWAP(0, 2);
            SWAP(0, 1);
            SWAP(3, 4);
            SWAP(5, 6);
            SWAP(3, 5);
            SWAP(4, 6);
            SWAP(4, 5);
            SWAP(0, 4);
            SWAP(0, 3);
            SWAP(1, 5);
            SWAP(2, 6);
            SWAP(2, 5);
            SWAP(1, 3);
            SWAP(2, 4);
            SWAP(2, 3);            

            // if the current pixel is at one of the extremes, replace it
            // with the median, otherwise leave it unchanged
            if (pix[i] == tmp[0] || pix[i] == tmp[6])
                pix[i] = tmp[3];
                
            // update our rolling window, if we're not at the start or
            // end of the overall pixel array
            if (i >= 3 && i < n-4)
            {
                w[a] = pix[i+4];
                a = (a + 1) % 7;
            }
        }
    }
    
    // Find an edge in the image.  'm' is the midpoint brightness level
    // in the array.  On success, fills in 'pos' with the pixel position
    // of the edge and returns true.  Returns false if no clear, unique 
    // edge can be detected.
    //
    // If 'vis' is true, we'll update the pixel array with a visualization
    // of the edges, for display in the config tool.
    bool findEdge(uint8_t *pix, int n, uint8_t m, int &pos, bool vis)
    {
        // Scan for edges.  An edge is a transition where two adajacent
        // pixels are on opposite sides of the brightness midpoint.
        int nEdges = 0;
        int edgePos = 0;
        uint8_t prv = pix[0], nxt = pix[1];
        for (int i = 1 ; i < n-1 ; prv = nxt, nxt = pix[++i])
        {
            // presume we'll show a non-edge (white) pixel in the visualization
            uint8_t vispix = 255;
            
            // if the two are on opposite sides of the midpoint, we have
            // an edge
            if ((prv < m && nxt > m) || (prv > m && nxt < m))
            {
                // count the edge and note its position
                ++nEdges;
                edgePos = i;
                
                // color edges black in the visualization
                vispix = 0;
            }
            
            // if in visualization mode, substitute the visualization pixel
            if (vis)
                pix[i] = vispix;
        }
        
        // check for a unique edge
        if (nEdges == 1)
        {
            // success
            pos = edgePos;
            return true;
        }
        else
        {
            // failure
            return false;
        }
    }
#endif
    
    // Send an exposure report to the joystick interface.
    // See plunger.h for details on the flags and visualization modes.
    virtual void sendExposureReport(USBJoystick &js, uint8_t flags, uint8_t visMode)
    {
        // start a capture
        ccd.startCapture();
        
        // get the stable pixel array
        uint8_t *pix;
        int n;
        uint32_t t;
        ccd.getPix(pix, n, t);
        
        // Apply processing if desired.  For visualization mode 0, apply no
        // processing at all.  For all others it through the pixel processor.
        int pos = 0xffff;
        uint32_t processTime = 0;
        if (visMode != 0)
        {
            // count the processing time
            Timer pt;
            pt.start();

            // do the processing
            process(pix, n, pos, visMode);
            
            // note the processing time
            processTime = pt.read_us();
        }
        
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
                // Combine these pixels - the best way to do this differs
                // by visualization mode...
                int a = 0;
                switch (visMode)
                {
                case 0:
                case 1:
                    // Raw or noise-reduced pixels.  This mode shows basically
                    // a regular picture, so reduce the resolution by averaging
                    // the grouped pixels.
                    for (int j = 0 ; j < group ; ++j)
                        a += pix[src++];
                        
                    // we have the sum, so get the average
                    a /= group;
                    break;
                    
                case 2:
                    // High contrast mode.  To retain the high contrast, take a
                    // majority vote of the pixels.  Start by counting the white
                    // pixels.
                    for (int j = 0 ; j < group ; ++j)
                        a += (pix[src++] > 127);
                        
                    // If half or more are white, make the combined pixel white;
                    // otherwise make it black.
                    a = (a >= n/2 ? 255 : 0);
                    break;
                    
                case 3:
                    // Edge mode.  Edges are shown as black.  To retain every
                    // detected edge in the result image, show the combined pixel
                    // as an edge if ANY pixel within the group is an edge.
                    a = 255;
                    for (int j = 0 ; j < group ; ++j)
                    {
                        if (pix[src++] < 127)
                            a = 0;
                    }
                    break;
                }
                    
                // store the down-res'd pixel in the array
                pix[dst] = uint8_t(a);
            }
            
            // update the pixel count to the number we stored
            n = dst;
            
            // if we have a valid position, rescale it to the reduced pixel count
            if (pos != 0xffff)
                pos = pos / group;
        }
        
        // send reports for all pixels
        int idx = 0;
        while (idx < n)
            js.updateExposure(idx, n, pix);
            
        // send a special final report with additional data
        js.updateExposureExt(pos, dir, ccd.getAvgScanTime(), processTime);
        
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
    
    // Hysteresis data.  We apply a very mild hysteresis to the position
    // reading to reduce jitter that can occur with this sensor design.
    // In practice, the shadow edge isn't perfectly sharp, so we usually
    // have several pixels in a fuzzy zone between solid shadow and solid
    // bright.  The sensor scan time isn't perfectly uniform either, so
    // exposure levels vary (exposure level is a function of the integration
    // time, and the way we're set up, integration time is a function of 
    // the sample scan time).  Plus there's some random noise in the sensor
    // pixels and in the ADC sampling process.  All of these variables
    // add up to some slight frame-to-frame variation in precisely where
    // we detect the shadow edge, even when the plunger is perfectly still.
    // This manifests as jitter: the sensed position wiggles back and forth
    // in response to the noise factors.  In testing, it appears that the
    // typical jitter is one pixel - it looks like we basicaly have trouble
    // deciding whether the edge is at pixel A or pixel A+1, and we jump
    // back and forth randomly as the noise varies.  To eliminate the
    // visible effects, we apply hysteresis customized for this magnitude
    // of jitter.  If two consecutive readings are only one pixel apart,
    // we'll stick with the first reading.  Furthermore, we'll set the
    // hysteresis bounds to exactly these two pixels.  'hyst1' is the 
    // last reported pixel position; 'hyst2' is the adjacent position
    // in the hysteresis range, if there is one, or 0xFFFF if not.
    uint16_t hyst1, hyst2;
    
    // History of midpoint brightness levels for the last few successful
    // scans.  This is a circular buffer that we write on each scan where
    // we successfully detect a shadow edge.  (It's circular, so we
    // effectively discard the oldest element whenever we write a new one.)
    //
    // The history is useful in cases where we have too little contrast
    // to detect an edge.  In these cases, we assume that the entire sensor
    // is either in shadow or light, which can happen if the plunger is at
    // one extreme or the other such that the edge of its shadow is out of 
    // the frame.  (Ideally, the sensor should be positioned so that the
    // shadow edge is always in the frame, but it's not always possible
    // to do this given the constrained space within a cabinet.)  The
    // history helps us decide which case we have - all shadow or all
    // light - by letting us compare our average pixel level in this
    // frame to the range in recent frames.  This assumes that the
    // exposure varies minimally from frame to frame, which is usually
    // true because the physical installation (the light source and 
    // sensor positions) are usually static.
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
