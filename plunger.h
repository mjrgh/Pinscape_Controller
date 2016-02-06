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
    
    // Number of "pixels" in the sensor's range.  We use the term "pixels"
    // here for historical reasons, namely that the first sensor we implemented
    // was an imaging sensor that physically sensed the plunger position as
    // a pixel coordinate in the image.  But it's no longer the right word,
    // since we support sensor types that have nothing to do with imaging.
    // Even so, the function this serves is still applicable.  Abstractly,
    // it represents the physical resolution of the sensor in terms of
    // the number of quanta over the full range of travel of the plunger.
    // For sensors that inherently quantize the position reading at the 
    // physical level, such as imaging sensors and quadrature sensors, 
    // this should be set to the total number of position steps over the 
    // range of travel.  For devices with physically analog outputs, such 
    // as potentiometers or LVDTs, the reading still has to be digitized 
    // for us to be able to work with it, which means it has to be turned
    // into a value that's fundamentally an integer.  But this happens in
    // the ADC, so the quantization scale is hidden in the mbed libraries.
    // The normal KL25Z ADC configuration is 16-bit quantization, so the
    // quantization factor is usually 65535.  But you might prefer to set
    // this to the joystick maximum so that there are no more rounding
    // errors in scale conversions after the point of initial conversion.
    //
    // IMPORTANT!  This value MUST be initialized in the constructor for
    // each concrete subclass.
    int npix;
         
    // Initialize the physical sensor device.  This is called at startup
    // to set up the device for first use.
    virtual void init() = 0;

    // Take a high-resolution reading.  Sets pos to the current position,
    // on a scale from 0 to npix:  0 is the maximum forward plunger position,
    // and npix is the maximum retracted position, in terms of the sensor's
    // extremes.  This is a raw reading in terms of the sensor range; the
    // caller is responsible for applying calibration data and scaling the
    // result to the the joystick report range.
    //
    // Returns true on success, false on failure.  Return false if it wasn't
    // possible to take a good reading for any reason.
    virtual bool highResScan(int &pos) = 0;

    // Take a low-resolution reading.  This reports the result on the same
    // 0..npix scale as highResScan().  Returns true on success, false on
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
    virtual bool lowResScan(int &pos) = 0;
        
    // Send an exposure report to the joystick interface.  This is specifically
    // for image sensors, and should be omitted by other sensor types.  For
    // image sensors, this takes one exposure and sends all pixels to the host
    // through special joystick reports.  This is used for PC-side testing tools
    // to let the user check the sensor installation by directly viewing its
    // pixel output.
    virtual void sendExposureReport(class USBJoystick &js) { }
};

#endif /* PLUNGER_H */
