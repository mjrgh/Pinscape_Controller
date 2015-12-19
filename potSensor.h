// Potentiometer plunger sensor
//
// This file implements our generic plunger sensor interface for a
// potentiometer.

#include "FastAnalogIn.h"

class PlungerSensorPot: public PlungerSensor
{
public:
    PlungerSensorPot(PinName ao) : pot(ao)
    {
    }
    
    virtual void init() 
    {
        // The potentiometer doesn't have pixels, but we still need an
        // integer range for normalizing our digitized voltage level values.
        // The number here is fairly arbitrary; the higher it is, the finer
        // the digitized steps.  A 40" 1080p HDTV has about 55 pixels per inch
        // on its physical display, so if the on-screen plunger is displayed
        // at roughly the true physical size, it's about 3" on screen or about
        // 165 pixels.  So the minimum quantization size here should be about
        // the same.  For the pot sensor, this is just a scaling factor, 
        // so higher values don't cost us anything (unlike the CCD, where the
        // read time is proportional to the number of pixels we sample).
        npix = 4096;
    }
    
    virtual bool highResScan(int &pos)
    {
        // Take a few readings and use the average, to reduce the effect
        // of analog voltage fluctuations.  The voltage range on the ADC
        // is 0-3.3V, and empirically it looks like we can expect random
        // voltage fluctuations of up to 50 mV, which is about 1.5% of
        // the overall range.  We try to quantize at about the mm level
        // (in terms of the plunger motion range), which is about 1%.
        // So 1.5% noise is big enough to be visible in the joystick
        // reports.  Averaging several readings should help smooth out
        // random noise in the readings.
        pos = int((pot.read() + pot.read() + pot.read())/3.0 * npix);
        return true;
    }
    
    virtual bool lowResScan(int &pos)
    {
        // Use an average of several readings.  Note that even though this
        // is nominally a "low res" scan, we can still afford to take an
        // average.  The point of the low res interface is speed, and since
        // we only have one analog value to read, we can afford to take
        // several samples here even in the low res case.
        pos = int((pot.read() + pot.read() + pot.read())/3.0 * npix);
        return true;
    }
        
private:
    AnalogIn pot;
};
