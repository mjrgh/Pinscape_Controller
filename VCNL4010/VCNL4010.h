// Vishay VCNL4010 Proximity Sensor interface
//
// OVERVIEW
//
// The Vishay VCNL4010 is an IR proximity sensor chip with an I2C interface
// and an effective operating range of about 2mm to 100mm.  The VirtuaPin
// plunger kit v3 is based on this sensor, so it's in fairly common use
// among pin cab builders.
//
// The Vishay chip reports its distance readings using a "count" value,
// which corresponds to the intensity of the IR signal reflected from the
// target.  The reflected light intensity follows the usual inverse square 
// law for distance, so the count is proportional to 1/D^2, hence the
// distance for a given count N is proportional to 1/sqrt(N).  That gives
// us the *relative* distance, not an exact distance, because the reflected
// light intensity also depends upon the brightness of the light source,
// the reflectivity of the target, and possibly other factors.  Assuming
// that all of theose other factors are constant for a given installation,
// there exists some constant F such that D = F/sqrt(N) yields the distance
// in a unit system of our choosing.  Determining F would require calibrating
// by taking the count at one or more known distances.
//
// For our purposes, we don't need to know the distance in hard units like
// millimeters.  It's sufficient to convert the count to a linear distance
// reading on an arbitrary scale.  So all that's really important is the
// 1/sqrt(N) part.  We can apply any proportionality constant we want at
// that point, to get it to an abstract distance range that's convenient
// for us to work with numerically.  We'd ideally like our abstract scale
// to fit a 16-bit int, since that's what the upper software layers use to
// report the position via the USB joystick interface.  The raw count
// readings from the chip are constrained by the chip's design to be in
// 0..65535, and for practical purposes have a lower limit of about 5 (with
// the target at 90mm and the LED current is set to the minimum 10mA).  A
// proportionality constant of 146540 gives us a value of 65534 for a count
// of 5.
//
//
// SENSOR INTIALIZATION
//
// Initializing the VCNL4010 from the software side is just a matter of
// programming the registers that control sample rate and sample collection
// policy.  From experience with other plunger sensors, we know that good
// plunger motion tracking without aliasing requires samples at 2.5ms
// intervals or faster.  The VCNL4010's fastest sampling rate for proximity
// is 250 samples/second, or 4ms intervals.  This is slower than we'd prefer,
// but it's the best the sensor can do, so we'll have to use that setting.
// For sample collection, we'll initiate "on demand" readings as the upper
// software layers poll the sensor, so there's no need to use the self-timer
// or interrupt modes on the sensor.
//
//
// I2C INFORMATION
//
// The sensor has an I2C interface wtih a fixed I2C address of 0010 011x
// (in 8-bit address terms, this is 0x26 write, 0x27 read).
//

#ifndef _VCNL4010_H_
#define _VCNL4010_H_

#include "mbed.h"
#include "BitBangI2C.h"


class VCNL4010
{
public:
    // Set up the interface with the given I2C pins.
    //
    // If 'internalPullups' is true, we'll set the I2C SDA/SCL pins to 
    // enable the internal pullup resistors.  Set this to false if you're
    // using your own external pullup resistors on the lines.  External
    // pullups are better if you're attaching more than one device to the
    // same physical I2C bus; the internal pullups are fine if there's only
    // one I2C device (in this case the VCNL4010) connected to these pins.
    VCNL4010(PinName sda, PinName scl, bool internalPullups);
    
    // initialize the chip
    void init();

    // Start a distance reading, returning immediately without waiting
    // for the reading to finish.  The caller can poll for the finished
    // reading via distanceReady().
    void startProxReading();
    
    // Is a proximity reading ready?
    bool proxReady();

    // Read the proximity value
    int getProx(
        uint8_t &distance, uint32_t &tMid, uint32_t &dt, 
        uint32_t timeout_us);

    // This chip has a fixed I2C address of 0x26 write, 0x27 read
    static const uint8_t I2C_ADDR = 0x26;
    
protected:
    // I2C read/write
    uint8_t readReg(uint8_t regAddr);
    void writeReg(uint8_t regAddr, uint8_t data);

    // I2C interface to device
    BitBangI2C i2c;
    
    // sample timer
    Timer sampleTimer;

    // time (from Timer t) of start of last range sample
    uint32_t tSampleStart;
};

#endif // _VCNL4010_H_
