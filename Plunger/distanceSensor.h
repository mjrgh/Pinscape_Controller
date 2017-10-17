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
// minimum standards.  However, this sensor is inexpensive and easier to
// set up than most of the better options, so it might be attractive to
// some cab builders despite the quality tradeoffs.

#ifndef _DISTANCESENSOR_H_
#define _DISTANCESENSOR_H_

#include "plunger.h"
#include "VL6180X.h"

// Base class for distance sensors
class PlungerSensorDistance: public PlungerSensor
{
public:
    PlungerSensorDistance(int nativeScale) : PlungerSensor(nativeScale)
    {
        totalTime = 0;
        nRuns = 0;
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

    // scan time statistics
    uint64_t totalTime;       // total time consumed by all reads so far
    uint32_t nRuns;           // number of runs so far
};

// PlungerSensor interface implementation for VL6180X sensors.  
//
// The VL6180X reports distances in millimeter quanta, so the native
// sensor units are millimeters.  A physical plunger has about 3" of
// total travel, but leave a little extra padding for measurement
// inaccuracies and other unusual situations, so'll use an actual
// native scale of 150mm.
class PlungerSensorVL6180X: public PlungerSensorDistance
{
public:
    PlungerSensorVL6180X(PinName sda, PinName scl, PinName gpio0)
        : PlungerSensorDistance(150),
          sensor(sda, scl, I2C_ADDRESS, gpio0, true)
    {
    }
    
    // fixed I2C bus address for the VL6180X
    static const int I2C_ADDRESS = 0x29;
    
    virtual void init()
    {
        // initialize the sensor and set the default configuration
        sensor.init();
        sensor.setDefaults();
        
        // start a reading
        sensor.startRangeReading();
    }
    
    virtual bool ready()
    {
        // make sure a reading has been initiated
        sensor.startRangeReading();
        
        // check if a reading is ready
        return sensor.rangeReady();
    }
    
    virtual bool readRaw(PlungerReading &r)
    {
        // if we have a new reading ready, collect it
        if (sensor.rangeReady())
        {
            // Get the range reading.  Note that we already know that the
            // sensor has a reading ready, so it shouldn't be possible to 
            // time out on the read.  (The sensor could have timed out on 
            // convergence, but if it did, that's in the past already so 
            // it's not something we have to wait for now.)
            uint8_t d;
            uint32_t t, dt;
            lastErr = sensor.getRange(d, t, dt, 100);
            
            // if we got a reading, update the last reading
            if (lastErr == 0)
            {
                // save the new reading
                last.pos = d;
                last.t = t;
            
                // collect scan time statistics
                collectScanTimeStats(dt);
            }
    
            // start a new reading
            sensor.startRangeReading();
        }
        
        // return the most recent reading
        r = last;
        return lastErr == 0;
    }
    
protected:
    // underlying sensor interface
    VL6180X sensor;
    
    // last reading and error status
    PlungerReading last;
    int lastErr;
};


#endif
