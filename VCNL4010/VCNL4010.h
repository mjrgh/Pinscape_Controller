// Vishay VCNL4010 Proximity Sensor interface
//
// OVERVIEW
//
// The Vishay VCNL4010 is an IR proximity sensor chip with an I2C interface
// and an effective operating range of about 2mm to 100mm.  The VirtuaPin
// plunger kit v3 is based on this sensor, so it's in fairly widespread
// use among pin cab builders.
//
// Like all proximity sensors, this chip is designed for sensing *proxmity*,
// not distance.  Proximity sensing is answering the yes/no question "is
// there an object within my detection range?".  However, many types of
// proximity sensors, including this one, don't answer the proximity
// question directly, but rather report an analog quantity that correlates
// with distance to a detected object.  For a proximity reading, we'd
// compare that analog quantitiy to a threshold level to determine whether
// or not an object is in range.  But we can "abuse" the analog reading by
// interpreting it as a continuous value instead of merely being on one side
// or the other of a cutoff point.  Since the analog value varies (in some
// way) with the distance to the detected object, we can re-interpret the
// reported analog quantity as a continuous distance value, as long as we
// know the mathematical relationship between the distance to the target
// and the sensor's reported reading.
//
// In the case of IR proximity sensors like this one, the analog quantity
// that the sensor reports is the intensity of light reflected from the
// target object.  This type of sensor projects an IR light source in the
// direction of the target, and measures the intensity of light reflected
// from the target.  At the basic physics level, the intensity of a light
// source varies with the inverse of the square of the distance, so assuming
// that we can hold all other quantities constant (brightness of the light
// source, reflectivity of the target, etc), the intensity measurement can
// be used as a proxy for the distance.  It's obviously not possible to
// compute an absolute distance (in millimeters from the sensor, say) from
// this information alone, since there are many other variables involved
// apart from the distance. But we can at least compute relative distances
// for different intensities.  And if we can compute relative distances,
// and we can calibrate the ends of our scale to the known geometry of a
// pinball plunger, we can get the effective absolute distance readings.
//
//
// SENSOR INTIALIZATION
//
// Initializing the VCNL4010 from the software side is just a matter of
// programming the registers that control sample rate and sample collection
// policy.  From experience with other plunger sensors, we know that good
// plunger motion tracking without aliasing requires samples at very short
// intervals - ideally 2.5ms or less  The VCNL4010's fastest sampling rate
// for proximity is 250 samples/second, or 4ms intervals, so it's not quite
// as fast as we'd like.  But it's still usable.  In addition, we'll use the
// "on demand" mode to collect readings (rather than its interrupt mode),
// since the upper software layers poll the sensor by design.
//
//
// I2C INFORMATION
//
// This chip has an I2C interface with an immutable I2C address of 0010 011x.
// In 8-bit address terms, this is 0x26 write, 0x27 read; or, if you prefer
// the 7-bit notation, it's address 0x13.
//

#ifndef _VCNL4010_H_
#define _VCNL4010_H_

#include "mbed.h"
#include "BitBangI2C.h"
#include "config.h"

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
    VCNL4010(PinName sda, PinName scl, bool internalPullups, int iredCurrent);
    
    // initialize the chip
    void init();

    // Start a distance reading, returning immediately without waiting
    // for the reading to finish.  The caller can poll for the finished
    // reading via proxReady().
    void startProxReading();
    
    // Is a proximity reading ready?
    bool proxReady();
    
    // Read the proximity value.  Note that this returns the "brightness"
    // value from the sensor, not a distance reading.  This must be converted
    // into a distance reading via countToDistance().
    int getProx(int &proxCount, uint32_t &tMid, uint32_t &dt, uint32_t timeout_us);

    // convert from raw sensor count values to distance units
    int countToDistance(int count);
        
    // This chip has a fixed I2C address of 0x26 write, 0x27 read
    static const uint8_t I2C_ADDR = 0x26;

    // Restore the saved calibration data from the configuration
    virtual void restoreCalibration(Config &config);

    // Begin calibration    
    virtual void beginCalibration();
    
    // End calibration
    virtual void endCalibration(Config &config);
  
protected:
    // I2C read/write
    uint8_t readReg(uint8_t regAddr);
    void writeReg(uint8_t regAddr, uint8_t data);

    // I2C interface to device
    BitBangI2C i2c;
    
    // IR LED current setting (from configuration)
    int iredCurrent;
    
    // sample timer
    Timer sampleTimer;

    // time (from Timer t) of start of last range sample
    uint32_t tSampleStart;
    
    // last raw proximity reading
    uint16_t lastProxCount;
    
    // flag: calibration is in progress
    bool calibrating;
        
    // minimum and maximum observed proximity counts during calibration
    uint16_t minProxCount;
    uint16_t maxProxCount;
    
    // proximity count observed at "park" position during calibration
    uint16_t parkProxCount;

    // Calculate the scaling factor for count -> distance conversions.
    // This uses the data collected during calibration to figure the
    // conversion factors.
    void calcScalingFactor();
    
    // DC Offset for converting from count to distance.  Per the Vishay
    // application notes, the sensor brightness signal contains a fixed
    // component that comes from a combination of physical factors such
    // as internal reflections, ambient light, ADC artifacts, and sensor
    // noise.  This must be subtracted from the reported proximity count
    // to get a measure of the actual reflected brightness level.  The
    // DC offset is a function of the overall setup, so it has to be
    // determined through calibration.
    int dcOffset;

    // Scaling factor and offset for converting from count to distance.
    // We calculate these based on the counts collected at known points
    // during calibration.
    float scalingFactor;
    float scalingOffset;
};

#endif // _VCNL4010_H_
