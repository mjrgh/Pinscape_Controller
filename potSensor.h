// Potentiometer plunger sensor
//
// This file implements our generic plunger sensor interface for a
// potentiometer.

#include "FastAnalogIn.h"

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
const int npix = 4096;

class PlungerSensor
{
public:
    PlungerSensor() : pot(POT_PIN)
    {
        pot.enable();
    }
    
    void init() 
    {
    }
    
    int lowResScan()
    {
        return int(pot.read() * npix);
    }
        
    bool highResScan(int &pos)
    {
        pos = int(pot.read() * npix);
        return true;
    }
    
    void sendExposureReport(USBJoystick &) 
    { 
    }
    
private:
    FastAnalogIn pot;
};
