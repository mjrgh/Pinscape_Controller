// VCNL4010 IR proximity sensor

#include "mbed.h"
#include "math.h"
#include "VCNL4010.h"

VCNL4010::VCNL4010(PinName sda, PinName scl, bool internalPullups)
    : i2c(sda, scl, internalPullups)
{
}

void VCNL4010::init()
{
    printf("VCNL4010 initializing\r\n");
    
    // reset the I2C bus
    i2c.reset();

    // Set the proximity sampling rate to the fastest available rate of
    // 250 samples/second (4ms/sample).  This isn't really fast enough for
    // good plunger motion tracking - a minimum sampling frequency of 400/s
    // is needed to avoid aliasing during the bounce-back phase of release
    // motions - but it's as fast as this device can go.
    writeReg(0x82, 0x07);
    
    // Set the current for the IR LED (the light source for proximity
    // measurements).  From the data sheet, it appears that higher current
    // settings yield slightly more linear response curves, but with
    // diminishing returns above 100mA.  Assuming that the installation
    // will be powering the sensor from the KL25Z 3.3V regulator, we'd
    // like to keep the current as small as possible, though, to avoid
    // putting too much load on the regulator (since it has to provide
    // power to teh KL25Z itself as well).  I'm going to try 50mA as a
    // compromise.  It might be worth experimenting with different values
    // to see if they make a different to signal quality.
    //
    // The LED current in milliamps is 10mA times the numeric value we
    // set in the register, up to a maximum of 20 for 200mA.
    writeReg(0x83, 5);

    // disable self-timed measurements - we'll start measurements on demand
    writeReg(0x80, 0x00);
    
    // start the sample timer, which we use to gather timing statistics 
    sampleTimer.start();

    printf("VCNL4010 initialization done\r\n");
}

// Start a proximity measurement
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

int VCNL4010::getProx(uint8_t &distance, uint32_t &tMid, uint32_t &dt, uint32_t timeout_us)
{
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
    int N = (readReg(0x87) << 8) | readReg(0x88);
    
    // start a new reading, so that the sensor is collecting the next
    // reading concurrently with the time-consuming floating-point math
    // we're about to do
    startProxReading();

    // Figure the distance in abstract units.  The raw count data from the
    // sensor is proportional to the intensity of the reflected light from
    // the target, which is proportional to the inverse of the square of
    // the distance.  So the distance is proportional to the inverse of
    // the square root of the count.  The proportionality factor is chosen
    // to normalize the result to a range of 0..65535.
    distance = static_cast<int>(146540.0f / sqrtf(static_cast<float>(N)));

    // success
    return 0;
}

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
 
void VCNL4010::writeReg(uint8_t registerAddr, uint8_t data)
{
    // set up the write: register number, data byte
    uint8_t data_write[2] = { registerAddr, data };
    i2c.write(I2C_ADDR, data_write, 2);
}
