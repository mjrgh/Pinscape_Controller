// Null plunger sensor
//
// This file defines a class that provides the plunger sensor interface
// that the main program expects, but with no physical sensor underneath.

#ifndef NULLSENSOR_H
#define NULLSENSOR_H

#include "plunger.h"

class PlungerSensorNull: public PlungerSensor
{
public:
    PlungerSensorNull() { }
    
    virtual void init() { }
    virtual bool lowResScan(float &pos) { return false; }
    virtual bool highResScan(float &pos) { return false; }
};

#endif /* NULLSENSOR_H */
