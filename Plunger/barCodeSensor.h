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
// Gray codes are ideal for this type of application, because they have the
// property that any two adjacent code values differ in exactly one bit.
// This is perfectly suited to an optical sensor scanning a moving target.
// For one thing, if we're halfway between two positions, the single-bit
// difference between adjacent codes means that exactly one bit will be
// ambiguous, so even if we get it wrong because of the ambiguous optical
// data, we'll still be +/- 1 position from the true position.  The other
// good feature is that any motion blur in images taken during rapid motion
// will likewise create ambiguity in the least significant bits, so we'll
// gracefully lose precision as motion blur increases but still have the
// correct values for the most significant bits, which is to say that we'll
// know our true position at reduced precision during motion.
//
// We use the Manchester-type optical coding because it has good properties
// for noisy images.  In particular, we evaluate each bit based only on
// the light levels of nearby pixels.  This insulates us from non-uniformity
// in the light level across the image.  We don't have to care if the pixels
// in a bit are above or below the average or median across the whole image;
// we only have to compare them to the immediately adjacent few pixels. 
// This gives us highly stable readings even with poor lighting conditions.
// That's desirable because it simplifies the requirements for the physical
// sensor installation.
// 

#ifndef _BARCODESENSOR_H_
#define _BARCODESENSOR_H_

#include "plunger.h"
#include "tsl14xxSensor.h"

// Base class for bar-code sensors
class PlungerSensorBarCode
{
public:
    bool process(const uint8_t *pix, int npix, int &pos)
    {
        // $$$ to be written
        return false;
    }
};

// PlungerSensor interface implementation for edge detection setups
class PlungerSensorBarCodeTSL14xx: public PlungerSensorTSL14xx, public PlungerSensorBarCode
{
public:
    PlungerSensorBarCodeTSL14xx(int nativePix, PinName si, PinName clock, PinName ao)
        : PlungerSensorTSL14xx(nativePix, si, clock, ao),
        PlungerSensorBarCode()
    {
    }
    
protected:
    // process the image through the bar code reader
    virtual bool process(const uint8_t *pix, int npix, int &pos)
    {
        // adjust the exposure
        adjustExposure(pix, npix);
        
        // do the standard bar code processing
        return PlungerSensorBarCode::process(pix, npix, pos);
    }
    
    // adjust the exposure
    void adjustExposure(const uint8_t *pix, int npix)
    {
        // Count the number of pixels near total darkness and
        // total saturation
        int nDark = 0, nSat = 0;
        for (int i = 0 ; i < npix ; ++i)
        {
            int pi = pix[i];
            if (pi < 10)
                ++nDark;
            else if (pi > 244)
                ++nSat;
        }
        
        // If more than 30% of pixels are near total darkness, increase
        // the exposure time.  If more than 30% are near total saturation,
        // decrease the exposure time.
        int pct30 = uint32_t(npix * 19661) >> 16;
        int pct50 = uint32_t(npix) >> 1;
        if (nDark > pct50 && nSat < pct30)
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
        else if (nSat > pct50 && nDark < pct30)
        {
            // very overexposed - decrease exposure time a lot
            if (axcTime > 50)
                axcTime -= 50;
            else
                axcTime = 0;
        }
        else if (nSat > pct30 && nDark < pct30)
        {
            // overexposed - decrease exposure time a little
            if (axcTime > 10)
                axcTime -= 10;
            else
                axcTime = 0;
        }
    }
};


// TSL1401CL
class PlungerSensorTSL1401CL: public PlungerSensorBarCodeTSL14xx
{
public:
    PlungerSensorTSL1401CL(PinName si, PinName clock, PinName a0)
        : PlungerSensorBarCodeTSL14xx(128, si, clock, a0)
    {
    }
};

#endif
