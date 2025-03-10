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
    PlungerSensorNull() : PlungerSensor(65535) { }
    
    virtual void init() { }
    virtual bool readRaw(PlungerReading &r) { return false; }
    virtual uint32_t getAvgScanTime() { return 0; }
};

#endif /* NULLSENSOR_H */
