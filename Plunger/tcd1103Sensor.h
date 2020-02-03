// Toshiba TCD1103 linear image sensors
//
// This sensor is similar to the original TSL1410R in both its electronic
// interface and the theory of operation.  The details of the electronics
// are different enough that we can't reuse the same code at the hardware
// interface level, but the principle of operation is similar: the sensor
// provides a serial interface to a file of pixels transferred as analog
// voltage levels representing the charge collected.  
//
// As with the TSL1410R, we position the sensor so that the pixel row is
// aligned with the plunger axis, and we detect the plunger position by
// looking for a dark/light edge at the end of the plunger.  However, 
// the optics for this sensor are very different because of the sensor's
// size.  The TSL1410R is by some magical coincidence the same size as
// the plunger travel range, so we set that sensor up so that the plunger
// is backlit with respect to the sensor, and simply casts a shadow on
// the sensor.  The TCD1103, in contrast, has a pixel array that's only 
// 8mm long, so we can't use the direct shadow approach.  Instead, we
// have to use a lens to focus an image of the plunger on the sensor.
// With a focused image, we can front-light the plunger and take a picture
// of the plunger itself rather than of an occluded back-light.
//
// Even though we use "edge sensing", this class isn't based on the
// PlungerSensorEdgePos class.  Our sensing algorithm is a little different,
// and much simpler, because we're working with a proper image of the
// plunger, rather than an image of its shadow.  The shadow tends to be
// rather fuzzy, and the TSL14xx sensors were pretty noisy, so we had to
// work fairly hard to distinguish an edge in the image from a noise spike.
// This sensor has very low noise, and the focused image produces a sharp
// edge, so we can use a more straightforward algorithm that just looks
// for the first bright spot.
//
// The TCD1103 uses a negative image: brighter pixels are represented by 
// lower numbers.  The electronics of the sensor are such that the dynamic
// range for the pixel analag voltage signal (which is what our pixel 
// elements represent) is only about 1V, or about 30% of the 3.3V range of
// the ADC.  Dark pixels read at about 2V (about 167 after 8-bit ADC 
// quantization), and saturated pixels read at 1V (78 on the ADC).  So our
// effective dynamic range after quantization is about 100 steps.  That 
// would be pretty terrible if the goal were to take pictures for an art
// gallery, and there are things we could do in the electronic interface 
// to improve it.  In particular, we could use an op-amp to expand the 
// voltage range on the ADC input and remove the DC offset, so that the
// signal going into the ADC covers the ADC's full 0V - 3.3V range.  That
// technique is actually used in some other projects using this sensor
// where the goal is to yield pictures as the end result.  But it's
// pretty complicated to set up and fine-tune to get the voltage range
// expansion just right, and we really don't need it; the edge detection
// works fine with what we get directly from the sensor.



#include "plunger.h"
#include "TCD1103.h"

template <bool invertedLogicGates>
class PlungerSensorImageInterfaceTCD1103: public PlungerSensorImageInterface
{
public:
    PlungerSensorImageInterfaceTCD1103(PinName fm, PinName os, PinName icg, PinName sh)
        : PlungerSensorImageInterface(1500), sensor(fm, os, icg, sh)
    {
    }

    // is the sensor ready?
    virtual bool ready() { return sensor.ready(); }
    
    virtual void init() { }
    
    // get the average sensor scan time
    virtual uint32_t getAvgScanTime() { return sensor.getAvgScanTime(); }
    
    virtual void readPix(uint8_t* &pix, uint32_t &t)
    {        
        // get the image array from the last capture
        sensor.getPix(pix, t);
    }
    
    virtual void releasePix() { sensor.releasePix(); }
    
    virtual void setMinIntTime(uint32_t us) { sensor.setMinIntTime(us); }

    // the low-level interface to the TSL14xx sensor
    TCD1103<invertedLogicGates> sensor;
};

template<bool invertedLogicGates>
class PlungerSensorTCD1103: public PlungerSensorImage<int>
{
public:
    PlungerSensorTCD1103(PinName fm, PinName os, PinName icg, PinName sh)
        : PlungerSensorImage(sensor, 1500, 1499, true), sensor(fm, os, icg, sh)
    {
    }
    
protected:
    // Process an image.  This seeks the first dark-to-light edge in the image.
    // We assume that the background (open space behind the plunger) has a
    // dark (minimally reflective) backdrop, and that the tip of the plunger
    // has a bright white strip right at the end.  So the end of the plunger
    // should be easily identifiable in the image as the first bright edge
    // we see starting at the "far" end.
    virtual bool process(const uint8_t *pix, int n, int &pos, int& /*processResult*/)
    {
        // Scan the pixel array to determine the actual dynamic range 
        // of this image.  That will let us determine what consistutes
        // "bright" when we're looking for the bright spot.
        uint8_t pixMin = 255, pixMax = 0;
        const uint8_t *p = pix;
        for (int i = n; i != 0; --i)
        {
            uint8_t c = *p++;
            if (c < pixMin) pixMin = c;
            if (c > pixMax) pixMax = c;
        }
        
        // Figure the threshold brightness for the bright spot as halfway
        // between the min and max.
        uint8_t threshold = (pixMin + pixMax)/2;
        
        // Scan for the first bright-enough pixel.  Remember that we're
        // working with a negative image, so "brighter" is "less than".
        p = pix;
        for (int i = n; i != 0; --i, ++p)
        {
            if (*p < threshold)
            {
                // got it - report this position
                pos = p - pix;
                return true;
            }
        }
        
        // no edge found - report failure
        return false;
    }
    
    // Use a fixed orientation for this sensor.  The shadow-edge sensors
    // try to infer the direction by checking which end of the image is
    // brighter, which works well for the shadow sensors because the back
    // end of the image will always be in shadow.  But for this sensor,
    // we're taking an image of the plunger (not its shadow), and the
    // back end of the plunger is the part with the spring, which has a
    // fuzzy and complex reflectivity pattern because of the spring.
    // So for this sensor, it's better to insist that the user sets it
    // up in a canonical orientation.  That's a reasaonble expectation
    // for this sensor anyway, because the physical installation won't
    // be as ad hoc as the TSL1410R setup, which only required that you
    // mounted the sensor itself.  In this case, you have to build a
    // circuit board and mount a lens on it, so it's reasonable to
    // expect that everyone will be using the mounting apparatus plans
    // that we'll detail in the build guide.  In any case, we'll just
    // make it clear in the instructions that you have to mount the
    // sensor in a certain orientation.
    virtual int getOrientation() const { return 1; }

    // the hardware sensor interface
    PlungerSensorImageInterfaceTCD1103<invertedLogicGates> sensor;
};
