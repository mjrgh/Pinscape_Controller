// VCNL4010 IR proximity sensor

#include "mbed.h"
#include "math.h"
#include "VCNL4010.h"


VCNL4010::VCNL4010(PinName sda, PinName scl, bool internalPullups, int iredCurrent)
    : i2c(sda, scl, internalPullups)
{
    // Calculate the scaling factor with a minimum proximitiy count of 5.
    // In actual practice, the minimum will usually be a lot higher, but
    // this is a safe default that gives us valid distance calculations
    // across almost the whole possible range of count values.  (Why not
    // zero?  Because of the inverse relationship between distance and
    // brightness == proximity count.  1/0 isn't meaningful, so we have
    // to use a non-zero minimum in the scaling calculation.  5 is so
    // low that it'll probably never actually happen in real readings,
    // but still gives us a reasonable scaled range.)
    calibrating = false;
    minProxCount = 100;
    maxProxCount = 65535;
    parkProxCount = 20000;
    dcOffset = 0;
    lastProxCount = 0;
    calcScalingFactor();
    
    // remember the desired IRED current setting
    this->iredCurrent = iredCurrent;
}

// Initialize the sensor device
void VCNL4010::init()
{
    // debugging instrumentation
    printf("VCNL4010 initializing\r\n");
    
    // reset the I2C bus
    i2c.reset();

    // Set the proximity sampling rate to the fastest available rate of
    // 250 samples/second (4ms/sample).  This isn't quite fast enough for
    // perfect plunger motion tracking - a minimum sampling frequency of
    // 400/s is needed to avoid aliasing during the bounce-back phase of
    // release motions.  But the plunger-independent part of the code
    // does some data processing to tolerate aliasing for even slower
    // sensors than this one, so this isn't a showstopper.  Apart from
    // the potential for aliasing during fast motion, 250/s is plenty
    // fast enough for responsive input and smooth animation.
    writeReg(0x82, 0x07);
    
    // Set the current for the IR LED (the light source for proximity
    // measurements).  This is in units of 10mA, up to 200mA.  If the
    // parameter is zero in the configuration, apply a default.  Make
    // sure it's in range (1..20).
    //
    // Note that the nominal current level isn't the same as the actual
    // current load on the sensor's power supply.  The nominal current
    // set here is the instantaneous current the chip uses to generate
    // IR pulses.  The pulses have a low duty cycle, so the continuous
    // current drawn on the chip's power inputs is much lower.  The
    // data sheet says that the total continuous power supply current
    // drawn with the most power-hungry settings (IRED maxed out at
    // 200mA, sampling frequency maxed at 250 Hz) is only 4mA.  So
    // there's no need to worry about blowing a fuse on the USB port
    // or frying the KL25Z 3.3V regulator - the chip draws negligible
    // power in those terms, even at the maximum IRED setting.
    uint8_t cur = static_cast<uint8_t>(iredCurrent);
    cur = (cur == 0 ? 10 : cur < 1 ? 1 : cur > 20 ? 20 : cur);
    writeReg(0x83, cur);

    // disable self-timed measurements - we'll start measurements on demand
    writeReg(0x80, 0x00);
    
    // start the sample timer, which we use to gather timing statistics 
    sampleTimer.start();

    // debugging instrumentation
    printf("VCNL4010 initialization done\r\n");
}

// Start a proximity measurement.  This initiates a proximity reading
// in the chip, and returns immediately, allowing the KL25Z to tend to
// other tasks while waiting for the reading to complete.  proxReady()
// can be used to poll for completion.
void VCNL4010::startProxReading()
{
    // set the prox_od (initiate proximity on demand) bit (0x08) in
    // the command register, if it's not already set
    uint8_t b = readReg(0x80);
    if ((b & 0x08) == 0)
    {
        tSampleStart = sampleTimer.read_us();
        writeReg(0x80, b | 0x08);
    }
}

// Check if a proximity sample is ready.  Implicitly starts a new reading
// if one isn't already either completed or in progress.  Returns true if
// a reading is ready, false if not.
bool VCNL4010::proxReady()
{
    // read the command register to get the status bits
    uint8_t b = readReg(0x80);
    
    // if the prox_data_rdy bit (0x20) is set, a reading is ready
    if ((b & 0x20) != 0)
        return true;
        
    // Not ready.  Since the caller is polling, they must expect a reading
    // to be in progress; if not, start one now.  A reading in progress is
    // indicated and initiated by the prox_od bit 
    if ((b & 0x08) == 0)
    {
        tSampleStart = sampleTimer.read_us();
        writeReg(0x80, b | 0x08);
    }
        
    // a reading is available if the prox_data_rdy (0x08) is set
    return (b & 0x20) != 0;
}

// Read the current proximity reading.  If a reading isn't ready, 
// we'll block until one is, up to the specified timeout interval.
// Returns zero if a reading was successfully retrieved, or a
// non-zero error code if a timeout or error occurs.
//
// Note that the returned proximity count value is the raw reading
// from the sensor, which indicates the intensity of the reflected
// light detected on the sensor, on an abstract scale from 0 to
// 65535.  The proximity count is inversely related to the distance
// to the target, but the relationship also depends upon many other
// factors, such as the size and reflectivity of the target, ambient
// light, and internal reflections within the sensor itself and
// within the overall apparatus.
int VCNL4010::getProx(int &proxCount,
    uint32_t &tMid, uint32_t &dt, uint32_t timeout_us)
{
    // If the chip isn't responding, try resetting it.  I2C will
    // generally report 0xFF on all byte reads when a device isn't
    // responding to commands, since the pull-up resistors on SDA
    // will make all data bits look like '1' on read.  It's
    // conceivable that a device could lock up while holding SDA
    // low, too, so a value of 0x00 could also be reported.  So to
    // sense if the device is answering, we should try reading a
    // register that, when things are working properly, should
    // always hold a value that's not either 0x00 or 0xFF.  For
    // the VCNL4010, we can read the product ID register, which
    // should report ID value 0x21 per the data sheet.  The low
    // nybble is a product revision number, so we shouldn't
    // insist on the value 0x21 - it could be 0x22 or 0x23, etc,
    // in future revisions of this chip.  But in any case, the
    // register should definitely not be 0x00 or 0xFF, so it's
    // a good solid test.
    uint8_t prodId = readReg(0x81);
    if (prodId == 0x00 || prodId == 0xFF)
    {
        // try resetting the chip
        init();
        
        // check if that cleared the problem; if not, give up and
        // return an error
        prodId = readReg(0x81);
        if (prodId == 0x00 || prodId == 0xFF)
            return 1;
    }
    
    // wait for the sample
    Timer t;
    t.start();
    for (;;)
    {
        // check for a sample
        if (proxReady())
            break;
            
        // if we've exceeded the timeout, return failure
        if (t.read_us() > timeout_us)
            return -1;
    }
    
    // figure the time since we initiated the reading
    dt = sampleTimer.read_us() - tSampleStart;
    
    // figure the midpoint time
    tMid = tSampleStart + dt/2;

    // read the result from the sensor, as a 16-bit proximity count value    
    int N = (static_cast<int>(readReg(0x87)) << 8) | readReg(0x88);
    
    // remember the last raw reading
    lastProxCount = N;
    
    // start a new reading, so that the sensor is collecting the next
    // reading concurrently with the time-consuming floating-point math
    // we're about to do
    startProxReading();
    
    // if calibration is in progress, note the new min/max proximity
    // count readings, if applicable
    if (calibrating) 
    {
        if (N < minProxCount)
            minProxCount = N;
        if (N > maxProxCount)
            maxProxCount = N;
    }
    
    // report the raw count back to the caller
    proxCount = N;
    
    // success
    return 0;
}

// Restore the saved calibration data from the configuration
void VCNL4010::restoreCalibration(Config &config)
{
    // remember the calibrated minimum proximity count
    this->minProxCount = config.plunger.cal.raw0;
    this->maxProxCount = config.plunger.cal.raw1;
    this->parkProxCount = config.plunger.cal.raw2;
    
    // figure the scaling factor for distance calculations
    calcScalingFactor();
}

// Begin calibration    
void VCNL4010::beginCalibration()
{
    // reset the min/max proximity count to the last reading
    calibrating = true;
    minProxCount = lastProxCount;
    maxProxCount = lastProxCount;
    parkProxCount = lastProxCount;
}

// End calibration
void VCNL4010::endCalibration(Config &config)
{
    // save the proximity count range data from the calibration in the
    // caller's configuration, so that we can restore the scaling
    // factor calculation on the next boot
    config.plunger.cal.raw0 = minProxCount;
    config.plunger.cal.raw1 = maxProxCount;
    config.plunger.cal.raw2 = parkProxCount;
    
    // calculate the new scaling factor for conversions to distance
    calcScalingFactor();
    
    // Set the new calibration range in distance units.  The range
    // in distance units is fixed, since we choose the scaling factor
    // specifically to cover the fixed range.
    config.plunger.cal.zero = 10922;
    config.plunger.cal.min = 0;
    config.plunger.cal.max = 65535;
    
    // we're no longer calibrating
    calibrating = false;
}

// Power law function for the relationship between sensor count
// readings and distance.  For our distance calculations, we use
// this relationship:
//
//    distance = <scaling factor> * 1/power(count - <DC offset>) + <scaling offset>
//
// where all of the constants in <angle brackets> are determined
// through calibration.
//
// We use the square root of the count as our power law relation.
// This was determined empirically (based on observation).  This is
// also the power law we'd expect from a naive application of physics,
// on the principle that the observed brightness of a point light
// source varies inversely with the square of the distance.
//
// The VCNL4010 data sheet doesn't specify a formulaic relationship,
// which isn't surprising given that the relationship is undoubtedly
// much more complex than just a power law equation, and also because
// Vishay doesn't market this chip as a distance sensor in the first
// place.  It's a *proximity* sensor, which means it's only meant to
// answer a yes/no question, "is an object within range?", and not
// the quantitative question "how far?".  So there's no reason for
// Vishay to specify a precise relationship between distance and
// brightness; all we have to know is that there's some kind of
// inverse relationship, since beyond that, everything's just
// relative.  The data sheet does at least offer a (low-res) graph
// of the distance-vs-proximity-count relationship under one set of
// test conditions, and interestingly, that graph suggests a rather
// different power law, more like ~1/distance^3.1.  The graph also
// makes it clear that the response isn't uniform - it doesn't
// follow *any* power law exactly, but is something more complex
// than that.  This is another non-surprise, given that environmental
// factors will inevitably confound the readings to some degree. 
//
// At any rate, in the data I've gathered, it seems that a simple 1/R^2
// power law is pretty close to reality, so I'm using that.  (Brightness
// varies with 1/R^2, so distance varies with 1/sqrt(brightness).)  If
// this turns out to produce noticeably non-linear results in other
// people's installations, we might have to revisit this with something
// more customized to the local setup.  For example, we could gather
// calibration data points across the whole plunger travel range and
// then do a best-fit calculation to determine the best exponent
// (which would still assume that there's *some* 1/R^x relationship
// for some exponent x, but it wouldn't assume it's necessarily R^2.)
// 
static inline float power(int x)
{
    return sqrtf(static_cast<float>(x)); 
}

// convert from a raw sensor count value to distance units, using our
// current calibration data
int VCNL4010::countToDistance(int count)
{
    // remove the DC offset from teh signal
    count -= dcOffset;
    
    // if the adjusted count (excess of DC offset) is zero or negative,
    // peg it to the minimum end = maximum retraction point
    if (count <= 0)
        return 65535;

    // figure the distance based on our inverse power curve
    float d = scalingFactor/power(count) + scalingOffset;
    
    // constrain it to the valid range and convert to int for return
    return d < 0.0f ? 0 : d > 65535.0f ? 65535 : static_cast<int>(d);
}

// Calculate the scaling factors for our power-law formula for
// converting proximity count (brightness) readings to distances.
// We call this upon completing a new calibration pass, and during
// initialization, when loading saved calibration data.
void VCNL4010::calcScalingFactor()
{
    // Don't let the minimum go below 100.  The inverse relationship makes
    // the calculation meaningless at zero and unstable at very small
    // count values, so we need a reasonable floor to keep things in a
    // usable range.  In practice, the minimum observed value will usually
    // be quite a lot higher (2000 to 20000 in my testing), which the
    // Vishay application note attributes to stray reflections from the
    // chip's mounting apparatus, ambient light, and noise within the
    // detector itself.  But just in case, set a floor that will ensure
    // reasonable calculations.
    if (minProxCount < 100)
        minProxCount = 100;
        
    // Set a ceiling of 65535, since the sensor can't go higher
    if (maxProxCount > 65535)
        maxProxCount = 65535;
        
    // Figure the scaling factor and offset over the range from the park
    // position to the maximum retracted position, which corresponds to
    // the minimum count (lowest intensity reflection) we've observed.
    //
    // Do all calculations with the counts *after* subtracting out the
    // signal's DC offset, which is the brightness level registered on the
    // sensor when there's no reflective target in range.  We can't directly
    // measure the DC offset in a plunger setup, since that would require
    // removing the plunger entirely, but we can guess that the minimum
    // reading observed during calibration is approximately equal to the
    // DC offset.  The minimum brightness occurs when the plunger is at the
    // most distance point in its travel range from the sensor, which is
    // when it's pulled all the way back.  The plunger travel distance is
    // just about at the limit of the VCNL4010's sensitivity, so the inverse
    // curve should be very nearly flat at this point, thus this is a very
    // close approximation of the true DC offset.
    const int dcOffsetDelta = 50;
    dcOffset = minProxCount > dcOffsetDelta ? minProxCount - dcOffsetDelta : 0;
    int park = parkProxCount - dcOffset;
    float parkInv = 1.0f/power(park);
    scalingFactor = 54612.5f / (1.0f/power(minProxCount - dcOffset) - parkInv);
    scalingOffset = 10922.5f - (scalingFactor * parkInv);
}

// Read an I2C register on the device
uint8_t VCNL4010::readReg(uint8_t registerAddr)
{
    // write the request
    uint8_t data_write[1] = { registerAddr };
    if (i2c.write(I2C_ADDR, data_write, 1, false))
        return 0x00;

    // read the result
    uint8_t data_read[1];
    if (i2c.read(I2C_ADDR, data_read, 1))
        return 0x00;
    
    // return the result
    return data_read[0];
}

// Write to an I2C register on the device 
void VCNL4010::writeReg(uint8_t registerAddr, uint8_t data)
{
    // set up the write: register number, data byte
    uint8_t data_write[2] = { registerAddr, data };
    i2c.write(I2C_ADDR, data_write, 2);
}
