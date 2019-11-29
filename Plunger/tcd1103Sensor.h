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
// aligned with the plunger axis, with a backlight, and we detect the plunger 
// position by looking for an edge between a light area (where the backlight
// is unobstructed) and a dark area (where the plunger rod is blocking the
// backlight).  The optical sensor area of the TSL1410R is large enough to
// cover the entire plunger travel distance, so the physical setup for that
// sensor is a simple matter of placing the sensor near the plunger, so that
// the plunger casts a shadow on the sensor.  The TCD1103, in contrast, has a 
// small optical sensor area, about 8mm long, so in this case we have to use
// a lens to reduce the image of the plunger by about 10X (from the 80mm
// plunger travel distance to the 8mm sensor size).  This makes the physical
// setup more complex, but it has the advantage of giving us a focused image,
// allowing for better precision in detecting the edge.  With the unfocused
// image used in the TSL1410R setup, the shadow was blurry over about 1/50".
// With a lens to focus the image, we could potentially get as good as 
// single-pixel resolution, which would give us about 1/500" resolution on 
// this 1500-pixel sensor.
//

#include "edgeSensor.h"
#include "TCD1103.h"

template <bool invertedLogicGates>
class PlungerSensorImageInterfaceTCD1103: public PlungerSensorImageInterface
{
public:
    // Note that the TCD1103 has 1500 actual image pixels, but the serial
    // interface provides 32 dummy elements on the front end (before the
    // first image pixel) and 14 dummy elements on the back end (after the
    // last image pixel), for a total of 1546 serial outputs.
    PlungerSensorImageInterfaceTCD1103(PinName fm, PinName os, PinName icg, PinName sh)
        : PlungerSensorImageInterface(1546), sensor(fm, os, icg, sh)
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
class PlungerSensorTCD1103: public PlungerSensorEdgePos
{
public:
    // Note that the TCD1103 has 1500 actual image pixels, but the serial
    // interface provides 32 dummy elements on the front end (before the
    // first image pixel) and 14 dummy elements on the back end (after the
    // last image pixel), for a total of 1546 serial outputs.
    PlungerSensorTCD1103(PinName fm, PinName os, PinName icg, PinName sh)
        : PlungerSensorEdgePos(sensor, 1546), sensor(fm, os, icg, sh)
    {
    }
    
protected:
    PlungerSensorImageInterfaceTCD1103<invertedLogicGates> sensor;
};
