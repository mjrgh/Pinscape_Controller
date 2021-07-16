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
// from the target.  At the basic physics level, the apparent brightness
// of a point light source varies with the inverse of the square of the
// distance between source and observer.  Our setup isn't quite as simple
// as that idealized model: we have a reflector instead of a point source,
// so there are other factors that could vary by distance, especially the
// cross-section of the target (the portion of the target within the spread
// angle of the source light).  These other factors might not even have
// simple polynomial relationships to distance.  Even so, the general idea
// that the reflected brightness varies inversely with the distance should
// hold, at least within a limited distance range.  Assuming we can hold
// all of the other quantities constant (brightness of the light source,
// reflectivity of the target, etc), then, the reflected brightness should
// serve as a proxy for the distance.  It's obviously not possible to
// compute an absolute distance (in millimeters from the sensor, say) from
// the brightness reading alone, since that depends upon knowing the actual
// values of all of the other quantities that assuming are held constant.
// But we don't have to know those variables individually; we can roll them
// into a proportionality constant that we can compute via calibration, by
// taking brightness readings at known distances and then solving for the
// constant.
//
// The VCNL4010 data sheet doesn't provide any specifications of how the
// brightness reading relates to distance - it can't, for all of the reasons
// mentioned above.  But it at least provides a rough plot of readings taken
// for a particular test configuration.  That plot suggests that the power
// law observed in the test configuration is roughly
//
//   Brightness ~ 1/Distance^3.2
// 
// over most of the range from 10mm to 100mm.  In my own testing, the best
// fit was more like 1/r^2.  I suspect that the power law depends quite a
// lot on the size and shape of the reflector.  Vishay's test setup uses a
// 3cm x 3cm square reflector, whereas my plunger test rig has about a 2.5cm
// circular reflector, which is about as big as you can make the reflector
// for a pin cab plunger without conflicting with the flipper switches.  I
// don't know if the difference in observed power law is due to the
// reflector geometry or other factors.  We might need to revisit the
// formula I used for the distance conversion as we gain experience from
// different users setting up the sensor.  A possible future enhancement
// would be to do a more detailed calibration as follows:
//
//   - Ask the user to pull back the plunger slowly at a very steady rate,
//     maybe 3-5 seconds per pull
//
//   - Collect frequent readings throughout this period, say every 50ms
//     (so around 60-100 readings per pull)
//
//   - Do a best-fit calculation on the data to solve for the exponent X
//     and proportionality constant C in (Brightness = C/Distance^X),
//     assuming that the distances are uniformly distributed over the
//     pull range (because the user was pulling at constant speed).
//
//   - Save the exponent as config.plunger.cal.raw1 (perhaps as a 4.4 bit
//     fixed-point value, such that X = raw1/16.0f)
//
// Alternatively, we could let the user provide the power law exponent
// manually, as a configuration parameter, and add a Config Tool command
// to collect the same calibration data described above and do the best-fit
// analysis.  It might be preferable to do it that way - the user could
// experiment with different values manually to find one that provides the
// best subjective feel, and they could use the analysis tool to suggest
// the best value based on data collection.  The reason I like the manual
// approach is that the actual distance/brightness relationship isn't as
// uniform as a simple power law, so even the best-fit power law will be
// imperfect.  What looks best subjectively might not match the mathematical
// best fit, because divergence from the fit might be more noticeable to
// the eye in some regions than in others.  A manual fit would allow the
// user to tweak it until it looked best in the whatever region they find
// most noticeable.
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
