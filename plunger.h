// Plunger Sensor Interface
//
// This module defines the abstract interface to the plunger sensors.
// We support several different physical sensor types, so we need a
// common interface for use in the main code.
//

#ifndef PLUNGER_H
#define PLUNGER_H

class PlungerSensor
{
public:

    PlungerSensor() { }
    virtual ~PlungerSensor() { }
    
    // Initialize the physical sensor device.  This is called at startup
    // to set up the device for first use.
    virtual void init() = 0;

    // Take a high-resolution reading.  Sets pos to the current position,
    // on a scale from 0.0 to 1.0:  0.0 is the maximum forward plunger position,
    // and 1.0 is the maximum retracted position, in terms of the sensor's
    // extremes.  This is a raw reading in terms of the sensor range; the
    // caller is responsible for applying calibration data and scaling the
    // result to the the joystick report range.
    //
    // Returns true on success, false on failure.  Return false if it wasn't
    // possible to take a good reading for any reason.
    virtual bool highResScan(float &pos) = 0;

    // Take a low-resolution reading.  This reports the result on the same
    // 0.0 to 1.0 scale as highResScan().  Returns true on success, false on
    // failure.
    //
    // The difference between the high-res and low-res scans is the amount 
    // of time it takes to complete the reading.  The high-res scan is allowed
    // to take about 10ms; a low-res scan take less than 1ms.  For many
    // sensors, either of these time scales would yield identical resolution;
    // if that's the case, simply take a reading the same way in both functions.
    // The distinction is for the benefit of sensors that need significantly
    // longer to read at higher resolutions, such as image sensors that have
    // to sample pixels serially.
    virtual bool lowResScan(float &pos) = 0;
        
    // Send an exposure report to the joystick interface.  This is specifically
    // for image sensors, and should be omitted by other sensor types.  For
    // image sensors, this takes one exposure and sends all pixels to the host
    // through special joystick reports.  This is used for PC-side testing tools
    // to let the user check the sensor installation by directly viewing its
    // pixel output.
    //
    // Mode bits:
    //   0x01  -> send processed pixels (default is raw pixels)
    //   0x02  -> low res scan (default is high res scan)
    //
    // If processed mode is selected, the sensor should apply any pixel
    // processing it normally does when taking a plunger position reading,
    // such as exposure correction, noise reduction, etc.  In raw mode, we
    // simply send the pixels as read from the sensor.  Both modes are useful
    // in setting up the physical sensor.
    virtual void sendExposureReport(class USBJoystick &js, uint8_t mode) { }
};

#endif /* PLUNGER_H */
