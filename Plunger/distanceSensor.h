// Plunger sensor type for distance sensors.
//
// This type of sensor measures the distance to a target by sending
// optical or sound signals and watching for the reflection.  There are
// many types of these sensors, including sensors that measure the 
// intensity of reflected sound or light signals, sensors that measure
// the round-trip time of "pings", and sensors that measure optical
// parallax.
//
// The basic installation for this type of sensor involves placing the
// sensor itself in a fixed location at one end of the plunger, pointing
// down the length of the plunger, and placing a reflective target at
// the end of the plunger.  The target can simply be an ordinary plunger
// tip, if the sensor is at the far end of the plunger facing forward
// (facing the front of the cabinet).  Alternatively, the target can
// be a disk or similar object attached to the end of the plunger, and
// the sensor can be placed at the front of the machine facing the target.
// In either case, the sensor measures the distance to the target at any
// given time, and we interpret that into the plunger position.
//
// Here are the specific sensor types we currently support:
//
// VL6180X: This is an optical (IR) "time of flight" sensor that measures
// the distance to the target by sending optical pings and timing the 
// return signal, converting the result to distance via the known speed 
// of light.  This sensor has nominal 1mm precision, although its true
// precision in testing is closer to 5mm.  Sample times are around 16ms.
// This makes the sensor acceptable but not great by Pinscape standards;
// we generally consider 2.5ms read times and .25mm precision to be the
// minimum standards.  However, this sensor is very inexpensive and easier
// to set up than most of the better options, so it might be attractive to
// some cab builders despite the quality tradeoffs.

#ifndef _DISTANCESENSOR_H_
#define _DISTANCESENSOR_H_

#include "plunger.h"
#include "VL6180X.h"

// Base class for distance sensors
class PlungerSensorDistance: public PlungerSensor
{
public:
    PlungerSensorDistance()
    {
        // start the sample timer
        t.start();
    }

    // get the average scan time
    virtual uint32_t getAvgScanTime() { return uint32_t(totalTime / nRuns); }

protected:
    // collect scan time statistics
    void collectScanTimeStats(uint32_t dt)
    {
        totalTime += dt;
        nRuns += 1;
    }

    // sample timer
    Timer t;
    
    // scan time statistics
    uint32_t tStart;          // time (on this->t) of start of current scan
    uint64_t totalTime;       // total time consumed by all reads so far
    uint32_t nRuns;           // number of runs so far
};

// PlungerSensor interface implementation for VL6180X sensors.  
class PlungerSensorVL6180X: public PlungerSensorDistance
{
public:
    PlungerSensorVL6180X(PinName sda, PinName scl, PinName gpio0)
        : sensor(sda, scl, I2C_ADDRESS, gpio0)
    {
    }
    
    static const int I2C_ADDRESS = 0x28;
    
    virtual void init()
    {
        // reboot and initialize the sensor
        sensor.init();
        
        // set the default configuration
        sensor.setDefaults();
        
        // start the first reading
        tStart = t.read_us();
        sensor.startRangeReading();
    }
    
    virtual bool ready()
    {
        return sensor.rangeReady();
    }
    
    virtual bool read(PlungerReading &r)
    {
        // get the range reading
        uint8_t d;
        int err = sensor.getRange(d, 25000);
        
        // start a new reading
        sensor.startRangeReading();
        tStart = t.read_us();

        // use the current timestamp
        r.t = t.read_us();
        
        // Normalize the reading to the 0..65536 scale.  We assume
        // that the sensor is mounted at the front of the cabinet
        // facing towards the back, so the plunger moves towards 
        // the sensor as its retracted.  The maximum distance that
        // a standard plunger e-ring can get from the front wall of
        // the cabinet is about 5" or 127mm, so we'll use 127mm as
        // our normalized zero point (the maximum forward position).  
        // We'll assume that the sensor is set up so that the target
        // at the e-ring gets no closer than 1cm = 10mm, so this is
        // our maximum retracted position, or 65535 on the normalized
        // scale.  So our scale maps from 10mm..127mm to 65535..0.
        if (d > 127)
            d = 127;
        else if (d < 10)
            d = 10;
        r.pos = (117 - (d - 10)) * (65535/117);
        
        // collect scan time statistics
        if (err == 0)
            collectScanTimeStats(uint32_t(r.t - tStart));

        // return the status ('err' is zero on success)
        return err == 0;
    }
    
protected:
    // underlying sensor interface
    VL6180X sensor;
};


#endif
