// Plunger sensor type for bar-code based absolute position encoders.
// This type of sensor uses an optical sensor that moves with the plunger
// along a guide rail with printed bar codes along its length that encode
// the absolute position at each point.  We figure the plunger position
// by reading the bar code and decoding it into a position figure.
//
// The bar code has to be encoded in a specific format that we recognize.
// We use a reflected Gray code, optically encoded in black/white pixel
// patterns.  Each bit is represented by a fixed-width area.  Half the
// pixels in every bit are white, and half are black.  A '0' bit is
// represented by black pixels in the left half and white pixels in the
// right half, and a '1' bit is white on the left and black on the right.
// To read a bit, we identify the set of pixels covering the bit's fixed
// area in the code, then we see if the left or right half is brighter.
//
// (This optical encoding scheme is based on Manchester coding, which is 
// normally used in the context of serial protocols, but translates to 
// bar codes straightforwardly.  Replace the serial protocol's time
// dimension with the spatial dimension across the bar, and replace the
// high/low wire voltage levels with white/black pixels.)
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
// that's correct to as many bits as we can make out.
//
// The half-and-half optical coding also has good properties for our
// purposes.  The fixed-width bit regions require essentially no CPU work
// to find the bits, which is good because we're using a fairly slow CPU.
// The half white/half black coding of each pixel makes every pixel 
// self-relative in terms of brightness, so we don't need to figure the 
// black and white thresholds globally for the whole image.  That makes 
// the physical device engineering and installation easier because the 
// software can tolerate a fairly wide range of lighting conditions.
// 

#ifndef _BARCODESENSOR_H_
#define _BARCODESENSOR_H_

#include "plunger.h"

// Gray code to binary mapping for our special coding.  This is a custom
// 7-bit code, minimum run length 6, 110 positions populated.  The minimum
// run length is the minimum number of consecutive code points where each
// bit must remain fixed.  For out optical coding, this defines the smallest
// "island" size for a black or white bar horizontally.  Small features are
// prone to light scattering that makes them appear gray on the sensor.
// Larger features are less subject to scatter, making them easier to 
// distinguish by brightness level.
static const uint8_t grayToBin[] = {
   0,   1,  83,   2,  71, 100,  84,   3,  69, 102,  82, 128,  70, 101,  57,   4,    // 0-15
  35,  50,  36,  37,  86,  87,  85, 128,  34, 103,  21, 104, 128, 128,  20,   5,    // 16-31
  11, 128,  24,  25,  98,  99,  97,  40,  68,  67,  81,  80,  55,  54,  56,  41,    // 32-47
  10,  51,  23,  38, 128,  52, 128,  39,   9,  66,  22, 128,   8,  53,   7,   6,    // 48-63
  47,  14,  60, 128,  72,  15,  59,  16,  46,  91,  93,  92,  45, 128,  58,  17,    // 64-79
  48,  49,  61,  62,  73,  88,  74,  75,  33,  90, 106, 105,  32,  89,  19,  18,    // 80-95
  12,  13,  95,  26, 128,  28,  96,  27, 128, 128,  94,  79,  44,  29,  43,  42,    // 96-111
 128,  64, 128,  63, 110, 128, 109,  76, 128,  65, 107,  78,  31,  30, 108,  77     // 112-127
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

struct BarCodeProcessResult
{
    int pixofs;
    int raw;
    int mask;
};

template <int nBits, int leftBarWidth, int leftBarMaxOfs, int bitWidth>
class PlungerSensorBarCode: public PlungerSensorImage<BarCodeProcessResult>
{
public:
    PlungerSensorBarCode(PlungerSensorImageInterface &sensor, int npix) 
        : PlungerSensorImage(sensor, npix, (1 << nBits) - 1)
    {
        startOfs = 0;
    }

    // process a configuration change
    virtual void onConfigChange(int varno, Config &cfg)
    {
        // check for bar-code variables
        switch (varno)
        {
        case 20:
            // bar code offset
            startOfs = cfg.plunger.barCode.startPix;
            break;
        }
        
        // do the generic work
        PlungerSensorImage::onConfigChange(varno, cfg);
    }

protected:
    // process the image    
    virtual bool process(const uint8_t *pix, int npix, int &pos, BarCodeProcessResult &res)
    {
        // adjust auto-exposure
        adjustExposure(pix, npix);
        
        // clear the result descriptor
        res.pixofs = 0;
        res.raw = 0;
        res.mask = 0;
        
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
            // start at the fixed pixel offset
            barStart = startOfs;
        }

        // Start with zero in the barcode and success mask.  The mask
        // indicates which bits we were able to read successfully: a
        // '1' bit in the mask indicates that the corresponding bit
        // position in 'barcode' was successfully read, a '0' bit means
        // that the image was too fuzzy to read.
        int barcode = 0, mask = 0;

        // Scan the bits
        for (int bit = 0, x0 = barStart; bit < nBits ; ++bit, x0 += bitWidth)
        {
#if 0
            // Figure the extent of this bit.  The last bit is double
            // the width of the other bits, to give us a better chance
            // of making out the small features of the last bit.
            int w = bitWidth;
            if (bit == nBits - 1) w *= 2;
#else
            // width of the bit
            const int w = bitWidth;
#endif

            // figure the bit's internal pixel layout
            int halfBitWidth = w / 2;
            int x1 = x0 + halfBitWidth;     // midpoint
            int x2 = x0 + w;                // right edge
            
            // make sure we didn't go out of bounds
            if (x1 > npix) x1 = npix;
            if (x2 > npix) x2 = npix;

#if 0
            // get the average of the pixels over the bit
            int sum = 0;
            for (int x = x0 ; x < x2 ; ++x)
                sum += pix[x];
            int avg = sum / w;
            // Scan the left and right sections.  Classify each
            // section according to whether the majority of its
            // pixels are above or below the local average.
            int lsum = 0, rsum = 0;
            for (int x = x0 + 1 ; x < x1 - 1 ; ++x)
                lsum += (pix[x] < avg ? 0 : 1);
            for (int x = x1 + 1 ; x < x2 - 1 ; ++x)
                rsum += (pix[x] < avg ? 0 : 1);
#else
            // Sum the pixel readings in each half-bit.  Ignore
            // the first and last bit of each section, since these
            // could be contaminated with scattered light from the
            // adjacent half-bit.  On the right half, hew to the 
            // right side if the overall pixel width is odd. 
            int lsum = 0, rsum = 0;
            for (int x = x0 + 1 ; x < x1 - 1 ; ++x)
                lsum += pix[x];
            for (int x = x2 - halfBitWidth + 1 ; x < x2 - 1 ; ++x)
                rsum += pix[x];
#endif
                
            // shift a zero bit into the code and success mask
            barcode <<= 1;
            mask <<= 1;

            // Brightness difference required per pixel.  Higher values
            // require greater contrast to make a reading, which reduces
            // spurious readings at the cost of reducing the overall 
            // success rate.  The right level depends on the quality of
            // the optical system.  Setting this to zero makes us maximally
            // tolerant of low-contrast images, allowing for the simplest
            // optical system.  Our simple optical system suffers from
            // poor focus, which in turn causes poor contrast in small
            // features.
            const int minDelta = 2;

            // see if we could tell the difference in brightness
            int delta = lsum - rsum;
            if (delta < 0) delta = -delta;
            if (delta > minDelta * w/2)
            {
                // got it - black/white = 0, white/black = 1
                if (lsum > rsum) barcode |= 1;
                mask |= 1;
            }
        }

        // decode the Gray code value to binary
        pos = grayToBin[barcode];
        
        // set the results descriptor structure
        res.pixofs = barStart;
        res.raw = barcode;
        res.mask = mask;
    
        // return success if we decoded all bits, and the Gray-to-binary
        // mapping was populated
        return pos != (1 << nBits) && mask == ((1 << nBits) - 1);
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

    // bar code sensor orientation is fixed
    virtual int getOrientation() const { return 1; }
    
    // send extra status report headers
    virtual void extraStatusHeaders(USBJoystick &js, BarCodeProcessResult &res)
    {
        // Send the bar code status report.  We use coding type 1 (Gray code,
        // Manchester pixel coding).
        js.sendPlungerStatusBarcode(nBits, 1, res.pixofs, bitWidth, res.raw, res.mask);
    }
    
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
        // are all below a certain brightness level and the white
        // pixels are all above.  Start by figuring the number of
        // pixels above and below.
        const int medianTarget = 160;
        int nBelow = 0;
        for (int i = 0 ; i < npix ; ++i)
        {
            if (pix[i] < medianTarget)
                ++nBelow;
        }
        
        // Figure the desired number of black pixels: the left bar is
        // all black pixels, and 50% of each bit is black pixels.
        int targetBelow = leftBarWidth + (nBits * bitWidth)/2;
        
        // Increase exposure time if too many pixels are below the
        // halfway point; decrease it if too many pixels are above.
        int d = nBelow - targetBelow;
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

#if 0
    // convert a reflected Gray code value (up to 16 bits) to binary
    static inline int grayToBin(int grayval)
    {
        int temp = grayval ^ (grayval >> 8);
        temp ^= (temp >> 4);
        temp ^= (temp >> 2);
        temp ^= (temp >> 1);
        return temp;
    }
#endif

    // bar code starting pixel offset
    int startOfs;
};

#endif
