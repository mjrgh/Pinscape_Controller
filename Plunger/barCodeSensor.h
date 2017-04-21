// Plunger sensor type for bar-code based absolute position encoders.
// This type of sensor uses an optical sensor that moves with the plunger
// along a guide rail with printed bar codes along its length that encode
// the absolute position at each point.  We figure the plunger position
// by reading the bar code and decoding it into a position figure.
//
// The bar code has to be encoded in a specific format that we recognize.
// We use a 10-bit reflected Gray code, optically encoded using a Manchester-
// type of coding.  Each bit is represented as a fixed-width area on the
// bar, half white and half black.  The bit value is encoded in the order
// of the colors: Black/White is '0', and White/Black is '1'.
//
// Gray codes are ideal for this type of application.  Gray codes are
// defined such that each code point differs in exactly one bit from each
// adjacent code point.  This provides natural error correction when used
// as a position scale, since any single-bit error will yield a code point 
// reading that's only one spot off from the true position.  So a bit read
// error acts like a reduction in precision.  Likewise, any time the sensor
// is halfway between two code points, only one bit will be ambiguous, so
// the reading will come out as one of points on either side of the true
// position.  Finally, motion blur will have the same effect, of creating
// ambiguity in the least significant bits, and thus giving us a reading
// that's correct to as many bits as we can read with teh blur.
//
// We use the Manchester-type optical coding because it has good properties
// for low-contrast images, and doesn't require uniform lighting.  Each bit's
// pixel span contains equal numbers of light and dark pixels, so each bit
// provides its own local level reference.  This means we don't care about
// lighting uniformity over the whole image, because we don't need a global
// notion of light and dark, just a local one over a single bit at a time.
// 

#ifndef _BARCODESENSOR_H_
#define _BARCODESENSOR_H_

#include "plunger.h"
#include "tsl14xxSensor.h"

// Base class for bar-code sensors
//
// This is a template class with template parameters for the bar
// code pixel structure.  The bar code layout is fixed for a given 
// sensor type.  We can assume fixed pixel sizes because we don't 
// have to process arbitrary images.  We only have to read scales 
// specially prepared for this application, so we can count on them
// being printed at an exact size relative to the sensor pixels.
//
// nBits = Number of bits in the code 
//
// leftBarWidth = Width in pixels of delimiting left bar.  The code is 
// delimited by a black bar on the "left" end, nearest pixel 0.  This 
// gives the pixel width of the bar.
//
// leftBarMaxOfs = Maximum offset of the delimiting bar from the left
// edge of the sensor (pixel 0), in pixels
//
// bitWidth = Width of each bit in pixels.  This is the width of the
// full bit, including both "half bits" - it's the full white/black or 
// black/white pattern area.

template <int nBits, int leftBarWidth, int leftBarMaxOfs, int bitWidth>
class PlungerSensorBarCode
{
public:
    // process the image    
    bool process(const uint8_t *pix, int npix, int &pos)
    {
#if 0 // $$$

        // scan from the left edge until we find the fixed '0' start bit
        for (int i = 0 ; i < leftBarMaxOfs ; ++i, ++pix)
        {
            // check for the '0' bit
            if (readBit8(pix) == 0)
            {
                // got it - skip the start bit
                pix += bitWidth;
                
                // read the gray code bits
                int gray = 0;
                for (int j = 0 ; j < nBits ; ++j, pix += bitWidth)
                {
                    // read the bit; return failure if we can't decode a bit
                    int bit = readBit8(pix);
                    if (bit < 0)
                        return false;
                        
                    // shift it into the code
                    gray = (gray << 1) | bit;
                }
            }
            
            // convert the gray code to binary
            int bin = grayToBin(gray);
            
            // compute the parity of the binary value
            int parity = 0;
            for (int j = 0 ; j < nBits ; ++j)
                parity ^= bin >> j;
                
            // figure the bit required for odd parity
            int odd = (parity & 0x01) ^ 0x01;
            
            // read the check bit
            int bit = readBit8(pix);
            if (pix < 0)
                return false;
                
            // check that it matches the expected parity
            if (bit != odd)
                return false;
                
            // success
            pos = bin;
            return true;
        }
        
        // no code found
        return false;

#else
        int barStart = leftBarMaxOfs/2;
        if (leftBarWidth != 0) // $$$
        {
            // Find the black bar on the left side (nearest pixel 0) that
            // delimits the start of the bar code.  To find it, first figure
            // the average brightness over the left margin up to the maximum
            // allowable offset, then look for the bar by finding the first
            // bar-width run of pixels that are darker than the average.
            int lsum = 0;
            for (int x = 1 ; x <= leftBarMaxOfs ; ++x)
                lsum += pix[x];
            int lavg = lsum / leftBarMaxOfs;
    
            // now find the first dark edge
            for (int x = 0 ; x < leftBarMaxOfs ; ++x)
            {
                // if this pixel is dark, and one of the next two is dark,
                // take it as the edge
                if (pix[x] < lavg && (pix[x+1] < lavg || pix[x+2] < lavg))
                {
                    // move past the delimier
                    barStart = x + leftBarWidth;
                    break;
                }
            }
        }
        else
        {
            barStart = 4; // $$$ should be configurable via config tool
        }

        // Scan the bits
        int barcode = 0;
        for (int bit = 0, x0 = barStart; bit < nBits ; ++bit, x0 += bitWidth)
        {
            // figure the extent of this bit
            int x1 = x0 + bitWidth / 2;
            int x2 = x0 + bitWidth;
            if (x1 > npix) x1 = npix;
            if (x2 > npix) x2 = npix;

            // get the average of the pixels over the bit
            int sum = 0;
            for (int x = x0 ; x < x2 ; ++x)
                sum += pix[x];
            int avg = sum / bitWidth;

            // Scan the left and right sections.  Classify each
            // section according to whether the majority of its
            // pixels are above or below the local average.
            int lsum = 0, rsum = 0;
            for (int x = x0 + 1 ; x < x1 - 1 ; ++x)
                lsum += (pix[x] < avg ? 0 : 1);
            for (int x = x1 + 1 ; x < x2 - 1 ; ++x)
                rsum += (pix[x] < avg ? 0 : 1);
                
            // if we don't have a winner, fail
            if (lsum == rsum)
                return false;

            // black/white = 0, white/black = 1
            barcode = (barcode << 1) | (lsum < rsum ? 0 : 1);
        }

        // decode the Gray code value to binary
        pos = grayToBin(barcode);
        
        // success
        return true;
#endif
    }
    
    // read a bar starting at the given pixel
    int readBit8(const uint8_t *pix)
    {
        // pull out the pixels for the bar
        uint8_t s[8];
        memcpy(s, pix, 8);
        
        // sort them in brightness order (using an 8-element network sort)
#define SWAP(a, b) if (s[a] > s[b]) { uint8_t tmp = s[a]; s[a] = s[b]; s[b] = tmp; }
        SWAP(0, 1);
        SWAP(2, 3);
        SWAP(0, 2);
        SWAP(1, 3);
        SWAP(1, 2);
        SWAP(4, 5);
        SWAP(6, 7);
        SWAP(4, 6);
        SWAP(5, 7);
        SWAP(5, 6);
        SWAP(0, 4);
        SWAP(1, 5);
        SWAP(1, 4);
        SWAP(2, 6);
        SWAP(3, 7);
        SWAP(3, 6);
        SWAP(2, 4);
        SWAP(3, 5);
        SWAP(3, 4);
#undef SWAP
        
        // figure the median brightness
        int median = (int(s[3]) + s[4] + 1) / 2;
        
        // count pixels below the median on each side
        int ldark = 0, rdark = 0;
        for (int i = 0 ; i < 3 ; ++i)
        {
            if (pix[i] < median)
                ldark++;
        }
        for (int i = 4 ; i < 8 ; ++i)
        {
            if (pix[i] < median)
                rdark++;
        }
        
        // we need >=3 dark + >=3 light or vice versa
        if (ldark >= 3 && rdark <= 1)
        {
            // dark + light = '0' bit
            return 0;
        }
        if (ldark <= 1 && rdark >= 3)
        {
            // light + dark = '1' bit
            return 1;
        }
        else
        {
            // ambiguous bit
            return -1;
        }
    }

    // convert a reflected Gray code value (up to 16 bits) to binary
    int grayToBin(int grayval)
    {
        int temp = grayval ^ (grayval >> 8);
        temp ^= (temp >> 4);
        temp ^= (temp >> 2);
        temp ^= (temp >> 1);
        return temp;
    }
};

// Auto-exposure counter
class BarCodeExposureCounter
{
public:
    BarCodeExposureCounter()
    {
        nDark = 0;
        nBright = 0;
        nZero = 0;
        nSat = 0;
    }
    
    inline void count(int pix)
    {
        if (pix <= 2)
            ++nZero;
        else if (pix < 12)
            ++nDark;
        else if (pix >= 253)
            ++nSat;
        else if (pix > 200)
            ++nBright;
    }
    
    int nDark;      // dark pixels
    int nBright;    // bright pixels
    int nZero;      // pixels at zero brightness
    int nSat;       // pixels at full saturation
};

// PlungerSensor interface implementation for bar code readers.
//
// Bar code readers are image sensors, so we have a pixel size for
// the sensor.  However, this isn't the scale for the readings.  The
// scale for the readings is determined by the number of bits in the
// bar code, since an n-bit bar code can encode 2^n distinct positions.
//
template <int nBits, int leftBarWidth, int leftBarMaxOfs, int bitWidth>
class PlungerSensorBarCodeTSL14xx: public PlungerSensorTSL14xxSmall,
    PlungerSensorBarCode<nBits, leftBarWidth, leftBarMaxOfs, bitWidth>
{
public:
    PlungerSensorBarCodeTSL14xx(int nativePix, PinName si, PinName clock, PinName ao)
        : PlungerSensorTSL14xxSmall(nativePix, (1 << nBits) - 1, si, clock, ao)
    {
        // the native scale is the number of positions we can
        // encode in the bar code
        nativeScale = 1023;
    }
    
protected:
    
    // process the image through the bar code reader
    virtual bool process(const uint8_t *pix, int npix, int &pos)
    {
        // adjust the exposure
        adjustExposure(pix, npix);
        
        // do the standard bar code processing
        return PlungerSensorBarCode<nBits, leftBarWidth, leftBarMaxOfs, bitWidth>
            ::process(pix, npix, pos);
    }
    
    // bar code sensor orientation is fixed
    virtual int getOrientation() const { return 1; }
    
    // adjust the exposure
    void adjustExposure(const uint8_t *pix, int npix)
    {
#if 1
        // The Manchester code has a nice property for auto exposure
        // control: each bit area has equal numbers of white and black
        // pixels.  So we know exactly how the overall population of
        // pixels has to look: the bit area will be 50% black and 50%
        // white, and the margins will be all white.  For maximum
        // contrast, target an exposure level where the black pixels
        // are all below the middle brightness level and the white
        // pixels are all above.  Start by figuring the number of
        // pixels above and below.
        int nDark = 0;
        for (int i = 0 ; i < npix ; ++i)
        {
            if (pix[i] < 200)
                ++nDark;
        }
        
        // Figure the percentage of black pixels: the left bar is
        // all black pixels, and 50% of each bit is black pixels.
        int targetDark = leftBarWidth + (nBits * bitWidth)/2;
        
        // Increase exposure time if too many pixels are below the
        // halfway point; decrease it if too many pixels are above.
        int d = nDark - targetDark;
        if (d > 5 || d < -5)
        {
            axcTime += d;
        }
        
        
#elif 0 //$$$
        // Count exposure levels of pixels in the left and right margins
        BarCodeExposureCounter counter;
        for (int i = 0 ; i < leftBarMaxOfs/2 ; ++i)
        {
            // count the pixels at the left and right margins
            counter.count(pix[i]);
            counter.count(pix[npix - i - 1]);
        }
        
        // The margin is all white, so try to get all of these pixels
        // in the bright range, but not saturated.  That should give us
        // the best overall contrast throughout the image.
        if (counter.nSat > 0)
        {
            // overexposed - reduce exposure time
            if (axcTime > 5)
                axcTime -= 5;
            else
                axcTime = 0;
        }
        else if (counter.nBright < leftBarMaxOfs)
        {
            // they're not all in the bright range - increase exposure time
            axcTime += 5;
        }

#else // $$$
        // Count the number of pixels near total darkness and
        // total saturation
        int nZero = 0, nDark = 0, nBri = 0, nSat = 0;
        for (int i = 0 ; i < npix ; ++i)
        {
            int pi = pix[i];
            if (pi <= 2)
                ++nZero;
            else if (pi < 12)
                ++nDark;
            else if (pi >= 254)
                ++nSat;
            else if (pi > 242)
                ++nBri;
        }
        
        // If more than 30% of pixels are near total darkness, increase
        // the exposure time.  If more than 30% are near total saturation,
        // decrease the exposure time.
        int pct5 = uint32_t(npix * 3277) >> 16;
        int pct30 = uint32_t(npix * 19661) >> 16;
        int pct50 = uint32_t(npix) >> 1;
        if (nSat == 0)
        {
            // no saturated pixels - increase exposure time
            axcTime += 5;
        }
        else if (nSat > pct5)
        {
            if (axcTime > 5)
                axcTime -= 5;
            else
                axcTime = 0;
        }
        else if (nZero == 0)
        {
            // no totally dark pixels - decrease exposure time
            if (axcTime > 5)
                axcTime -= 5;
            else
                axcTime = 0;
        }
        else if (nZero > pct5)
        {
            axcTime += 5;
        }
        else if (nZero > pct30 || (nDark > pct50 && nSat < pct30))
        {
            // very dark - increase exposure time a lot
            if (axcTime < 450)
                axcTime += 50;
        }
        else if (nDark > pct30 && nSat < pct30)
        {
            // dark - increase exposure time a bit
            if (axcTime < 490)
                axcTime += 10;
        }
        else if (nSat > pct30 || (nBri > pct50 && nDark < pct30))
        {
            // very overexposed - decrease exposure time a lot
            if (axcTime > 50)
                axcTime -= 50;
            else
                axcTime = 0;
        }
        else if (nBri > pct30 && nDark < pct30)
        {
            // overexposed - decrease exposure time a little
            if (axcTime > 10)
                axcTime -= 10;
            else
                axcTime = 0;
        }
#endif
        
        // don't allow the exposure time to go over 2.5ms
        if (int(axcTime) < 0)
            axcTime = 0;
        if (axcTime > 2500)
            axcTime = 2500;
    }
};

// TSL1401CL - 128-bit image sensor, used as a bar code reader
class PlungerSensorTSL1401CL: public PlungerSensorBarCodeTSL14xx<
    10,  // number of bits in code
    0,   // left delimiter bar width in pixels (0 for none)
    24,  // maximum left margin width in pixels
    12>  // pixel width of each bit
{
public:
    PlungerSensorTSL1401CL(PinName si, PinName clock, PinName a0)
        : PlungerSensorBarCodeTSL14xx(128, si, clock, a0)
    {
    }
};

#endif
