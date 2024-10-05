// Base class for TSL14xx-based plunger sensors.
//
// This provides a common base class for plunger sensors based on
// AMS/TAOS TSL14xx sensors (TSL1410R, TSL1412S, TSL1401CL).  The sensors
// in this series all work the same way, differing mostly in the number
// of pixels.  However, we have two fundamentally different ways of using
// these image sensors to detect position: sensing the position of the
// shadow cast by the plunger on the sensor, and optically reading a bar
// code telling us the location of the sensor along a scale.  This class
// provides the low-level pixel-sensor interface; subclasses provide the
// image analysis that figures the position from the captured image.


#ifndef _TSL14XXSENSOR_H_
#define _TSL14XXSENSOR_H_

#include "plunger.h"
#include "edgeSensor.h"
#include "barCodeSensor.h"
#include "TSL14xx.h"

class PlungerSensorTSL14xx: public PlungerSensorImageInterface
{
public:
    PlungerSensorTSL14xx(int nativePix, PinName si, PinName clock, PinName ao)
        : PlungerSensorImageInterface(nativePix), sensor(nativePix, si, clock, ao)
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
    TSL14xx sensor;
};


// -------------------------------------------------------------------------
//
// Concrete TSL14xx sensor types
//


// TSL1410R sensor - edge detection sensor
class PlungerSensorTSL1410R: public PlungerSensorEdgePos
{
public:
    PlungerSensorTSL1410R(PinName si, PinName clock, PinName ao, int scanMode)
        : PlungerSensorEdgePos(sensor, 1280, scanMode), sensor(1280, si, clock, ao)
    {
    }
    
protected:
    PlungerSensorTSL14xx sensor;
};

// TSL1412R - edge detection sensor
class PlungerSensorTSL1412R: public PlungerSensorEdgePos
{
public:
    PlungerSensorTSL1412R(PinName si, PinName clock, PinName ao, int scanMode)
        : PlungerSensorEdgePos(sensor, 1536, scanMode), sensor(1536, si, clock, ao)
    {
    }
    
protected:
    PlungerSensorTSL14xx sensor;
};

// TSL1401CL - bar code sensor
class PlungerSensorTSL1401CL: public PlungerSensorBarCode<7, 0, 1, 16>
{
public:
    PlungerSensorTSL1401CL(PinName si, PinName clock, PinName ao)
        : PlungerSensorBarCode(sensor, 128), sensor(128, si, clock, ao)
    {
    }
    
protected:
    PlungerSensorTSL14xx sensor;
};

#endif
