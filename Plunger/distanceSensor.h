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
// VCNL4010: An IR proximity sensor.  This sensor shines an IR light at a
// target and measures the intensity of the reflected light.  This doesn't
// measure distance per se, but since the intensity of a light source
// falls off as the square of the distance, we can use the reflected
// intensity as a proxy for the distance by calculating 1/sqrt(intensity).
// The main reason to support this type of sensor is that it's used in the
// VirtuaPin v3 plunger kit, and several people have requested support so
// that they can move re-flash that kit using the Pinscape software and
// continue using their existing plunger sensor.  Many people might also
// consider this sensor for new DIY builds, since it produces pretty good
// results.  It's not as accurate as a potentiometer or quadrature sensor,
// but it yields low-noise results with good enough precision for smooth
// on-screen animation (maybe around 1mm precision).  Its main drawback
// is that it's relatively slow (250 Hz maximum sampling rate), but it's
// still fast enough to be usable.  It has several virtues that might more
// than orffset its technical limitations for many paople: it's easy to
// set up physically, it's completely non-contact, and it's cheap (under
// $10 for the Adafruit breakout board).
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
//
//

#ifndef _DISTANCESENSOR_H_
#define _DISTANCESENSOR_H_

#include "plunger.h"
#include "VL6180X.h"
#include "VCNL4010.h"


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


// PlungerSensor interface implementation for VCNL4010 IR proximity sensors
//
// Our hardware interface for this sensor reports distances in abstract
// units that fit a 16-bit int, so the native distance scale is 0..65535.
// (The sensor itself doesn't have a native distance scale per se, since
// it reports results in terms of the intensity of the reflected light.
// This is related to the distance by an inverse square law, so since we
// have to do some math on the raw readings anyway to convert them to
// distances, we can choose whatever units we want for the conversion.
// We choose units that are convenient for our purposes at the joystick
// layer, given the 16-bit field we use to report the position back to
// the PC.)
//
// The iredCurrent parameter sets the brightness of the sensor's IR LED,
// which serves as the light source for the reflected light intensity
// readings used for proximity measurements.  This is given in units of
// 10mA, so 1 means 10mA, 2 means 20mA, etc.  Valid values are from 1
// (10mA) to 20 (200mA).
//
class PlungerSensorVCNL4010: public PlungerSensorDistance
{
public:
    PlungerSensorVCNL4010(PinName sda, PinName scl, int iredCurrent)
        : PlungerSensorDistance(65535),
          sensor(sda, scl, true, iredCurrent)
    {
    }
    
    virtual void init()
    {
        // initialize the sensor
        sensor.init();
        
        // start a reading
        sensor.startProxReading();
    }
    
    virtual bool ready()
    {
        // check if a reading is ready
        return sensor.proxReady();
    }
    
    virtual bool readRaw(PlungerReading &r)
    {
        // if we have a new reading ready, collect it
        if (sensor.proxReady())
        {
            // Get the proximity count reading.  Note that we already know 
            // that the sensor has a reading ready, so it shouldn't be
            // possible to time out on the read.
            int rawCount;
            uint32_t t, dt;
            lastErr = sensor.getProx(rawCount, t, dt, 100);
            
            // if we got a reading, update the last reading
            if (lastErr == 0)
            {
                // run the proximity count through the jitter filter
                int filteredCount = jitterFilter(rawCount);
                
                // convert the count to a distance, using the filtered count
                int dist = sensor.countToDistance(filteredCount);
            
                // save the new reading
                last.pos = dist;
                last.t = t;
                lastFilteredCount = filteredCount;
                lastRawCount = rawCount;
            
                // collect scan time statistics
                collectScanTimeStats(dt);
            }
        }
        
        // return the most recent reading
        r = last;
        return lastErr == 0;
    }
    
    // The VCNL4010 applies jitter filtering to the physical sensor reading
    // instead of to the distance reading.  This produces much better results
    // for this sensor because the sensor's distance resolution gets lower
    // at longer distances, so the conversion to distance tends to amplify
    // noise quite a bit at the distant end.  It's therefore important to
    // do the noise reduction in the brightness domain, before that
    // amplification takes place.
    virtual int postJitterFilter(int pos) { return pos; }
    
    // Send a status report for the config tool sensor viewer
    virtual void sendStatusReport(class USBJoystick &js, uint8_t flags)
    {
        // send the common status report
        PlungerSensor::sendStatusReport(js, flags);

        // send the extra VCNL4010 sensor status report
        js.sendPlungerStatusVCNL4010(lastFilteredCount, lastRawCount);
    }

    // Restore the saved calibration data from the configuration.  The
    // main loop calls this at initialization time to pass us saved
    // private configuration data.  The VCNL4010 uses this to store the
    // minimum proximity count reading observed during calibration, which
    // it uses to figure the scaling factor for the 1/sqrt(intensity)
    // distance calculation.
    virtual void restoreCalibration(Config &config) 
    {
        // restore the saved minimum count reading
        sensor.restoreCalibration(config);
    }
    
    // Begin calibration.  The main loop calls this when the user
    // initiates a calibration cycle.  The VCNL4010 code uses this to
    // reset its internal record of the proximity minimum.
    virtual void beginCalibration(Config &)
    {
        sensor.beginCalibration();
    }
    
    // End calibration.  The main loop calls this when a calibration
    // cycle finishes.  The VCNL4010 code uses this to save the minimum
    // count value observed during the calibration interval, and to
    // calculate the new scaling factor for the 1/sqrt(intensity)
    // distance calculation.
    virtual void endCalibration(Config &config)
    {
        // let the sensor figure the new scaling factor
        sensor.endCalibration(config);
    }
  
        
protected:
    // underlying sensor interface
    VCNL4010 sensor;
    
    // last reading and error status
    PlungerReading last;
    int lastFilteredCount;
    int lastRawCount;
    int lastErr;
};

#endif
