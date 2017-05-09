// VL6180X Time of Flight sensor interface

#include "mbed.h"
#include "VL6180X.h"

VL6180X::VL6180X(PinName sda, PinName scl, uint8_t addr, PinName gpio0, 
    bool internalPullups)
    : i2c(sda, scl, internalPullups), gpio0Pin(gpio0)
{
    // remember the address
    this->addr = addr;
    
    // start in single-shot distance mode
    distMode = 0;
    rangeStarted = false;
    
    // initially reset the sensor by holding GPIO0/CE low
    gpio0Pin.mode(PullNone);
    gpio0Pin.output();
    gpio0Pin.write(0);
}

VL6180X::~VL6180X() 
{
}

bool VL6180X::init()
{
    // hold reset low for 10ms
    gpio0Pin.output();
    gpio0Pin.write(0);
    wait_us(10000);
    
    // release reset and allow 10ms for the sensor to reboot
    gpio0Pin.input();
    wait_us(10000);
        
    // reset the I2C bus
    i2c.reset();
    
    // check that the sensor's reset register reads as '1'
    Timer t;
    t.start();
    while (readReg8(VL6180X_SYSTEM_FRESH_OUT_OF_RESET) != 1)
    {
        if (t.read_us() > 1000000)
            return false;
    }
    
    // clear reset flag
    writeReg8(VL6180X_SYSTEM_FRESH_OUT_OF_RESET, 0);
        
    // give the device 50ms before sending the startup sequence
    wait_ms(50);
    
    // Send the mandatory initial register assignments, per the manufacturer's app notes:
    // http://www.st.com/st-web-ui/static/active/en/resource/technical/document/application_note/DM00122600.pdf
    writeReg8(0x0207, 0x01);
    writeReg8(0x0208, 0x01);
    writeReg8(0x0096, 0x00);
    writeReg8(0x0097, 0xfd);
    writeReg8(0x00e3, 0x00);
    writeReg8(0x00e4, 0x04);
    writeReg8(0x00e5, 0x02);
    writeReg8(0x00e6, 0x01);
    writeReg8(0x00e7, 0x03);
    writeReg8(0x00f5, 0x02);
    writeReg8(0x00d9, 0x05);
    writeReg8(0x00db, 0xce);
    writeReg8(0x00dc, 0x03);
    writeReg8(0x00dd, 0xf8);
    writeReg8(0x009f, 0x00);
    writeReg8(0x00a3, 0x3c);
    writeReg8(0x00b7, 0x00);
    writeReg8(0x00bb, 0x3c);
    writeReg8(0x00b2, 0x09);
    writeReg8(0x00ca, 0x09);
    writeReg8(0x0198, 0x01);
    writeReg8(0x01b0, 0x17);
    writeReg8(0x01ad, 0x00);
    writeReg8(0x00ff, 0x05);
    writeReg8(0x0100, 0x05);
    writeReg8(0x0199, 0x05);
    writeReg8(0x01a6, 0x1b);
    writeReg8(0x01ac, 0x3e);
    writeReg8(0x01a7, 0x1f);
    writeReg8(0x0030, 0x00);    
    
    // allow time to settle
    wait_us(1000);
        
    // start the sample timer 
    sampleTimer.start();

    // success
    return true;
}
 
void VL6180X::setDefaults()
{
    writeReg8(VL6180X_SYSTEM_GROUPED_PARAMETER_HOLD, 0x01);         // set parameter hold while updating settings
    
    writeReg8(VL6180X_SYSTEM_INTERRUPT_CONFIG_GPIO, 4);             // Enable interrupts from range only
    writeReg8(VL6180X_SYSTEM_MODE_GPIO1, 0x00);                     // Disable GPIO1
    writeReg8(VL6180X_SYSRANGE_VHV_REPEAT_RATE, 0xFF);              // Set auto calibration period (Max = 255)/(OFF = 0)
    writeReg8(VL6180X_SYSRANGE_INTERMEASUREMENT_PERIOD, 0x09);      // Set default ranging inter-measurement period to 100ms
    writeReg8(VL6180X_SYSRANGE_MAX_CONVERGENCE_TIME, 63);           // Max range convergence time 63ms
    writeReg8(VL6180X_SYSRANGE_RANGE_CHECK_ENABLES, 0x00);          // S/N disable, ignore disable, early convergence test disable
    writeReg16(VL6180X_SYSRANGE_EARLY_CONVERGENCE_ESTIMATE, 0x00);  // abort range measurement if convergence rate below this value
    writeReg8(VL6180X_READOUT_AVERAGING_SAMPLE_PERIOD, averagingSamplePeriod);  // Sample averaging period (1.3ms + N*64.5us)
    writeReg8(VL6180X_SYSRANGE_THRESH_LOW, 0x00);                   // low threshold
    writeReg8(VL6180X_SYSRANGE_THRESH_HIGH, 0xff);                  // high threshold

    writeReg8(VL6180X_SYSTEM_GROUPED_PARAMETER_HOLD, 0x00);         // end parameter hold

    // perform a single calibration; wait until it's done (within reason)
    Timer t;
    t.start();
    writeReg8(VL6180X_SYSRANGE_VHV_RECALIBRATE, 0x01);
    while (readReg8(VL6180X_SYSRANGE_VHV_RECALIBRATE) != 0)
    {
        // if we've been waiting too long, abort
        if (t.read_us() > 100000)
            break;
    }
}

void VL6180X::getID(struct VL6180X_ID &id)
{
    id.model = readReg8(VL6180X_IDENTIFICATION_MODEL_ID);
    id.modelRevMajor = readReg8(VL6180X_IDENTIFICATION_MODEL_REV_MAJOR) & 0x07;
    id.modelRevMinor = readReg8(VL6180X_IDENTIFICATION_MODEL_REV_MINOR) & 0x07;
    id.moduleRevMajor = readReg8(VL6180X_IDENTIFICATION_MODULE_REV_MAJOR) & 0x07;
    id.moduleRevMinor = readReg8(VL6180X_IDENTIFICATION_MODULE_REV_MINOR) & 0x07;
    
    uint16_t date = readReg16(VL6180X_IDENTIFICATION_DATE);
    uint16_t time = readReg16(VL6180X_IDENTIFICATION_TIME) * 2;
    id.manufDate.year = 2010 + ((date >> 12) & 0x0f);
    id.manufDate.month = (date >> 8) & 0x0f;
    id.manufDate.day = (date >> 3) & 0x1f;
    id.manufDate.phase = uint8_t(date & 0x07);
    id.manufDate.hh = time/3600;
    id.manufDate.mm = (time % 3600) / 60;
    id.manufDate.ss = time % 60;
}

void VL6180X::continuousDistanceMode(bool on)
{
    if (distMode != on)
    {
        // remember the new mode
        distMode = on;
        
        // Set continuous or single-shot mode.  If starting continuous
        // mode, set bits 0x01 (range mode = continuous) + 0x02 (start
        // collecting samples now).  If ending the mode, set all bits
        // to zero to select single-shot mode without starting a reading.
        if (on)
        {
            writeReg8(VL6180X_SYSTEM_INTERRUPT_CONFIG_GPIO, 4);   // Enable interrupts for ranging only
            writeReg8(VL6180X_SYSALS_INTERMEASUREMENT_PERIOD, 0); // minimum measurement interval (10ms)
            writeReg8(VL6180X_SYSRANGE_START, 0x03);
        }
        else
            writeReg8(VL6180X_SYSRANGE_START, 0x00);
    }
}

bool VL6180X::rangeReady()
{
    // check if the status register says a sample is ready (bits 0-2/0x07)
    // or an error has occurred (bits 6-7/0xC0)
    return ((readReg8(VL6180X_RESULT_INTERRUPT_STATUS_GPIO) & 0xC7) != 0);
}
 
void VL6180X::startRangeReading()
{
    // start a new range reading if one isn't already in progress
    if (!rangeStarted)
    {
        tSampleStart = sampleTimer.read_us();
        writeReg8(VL6180X_SYSTEM_INTERRUPT_CLEAR, 0x07);
        writeReg8(VL6180X_SYSRANGE_START, 0x00);
        writeReg8(VL6180X_SYSRANGE_START, 0x01);
        rangeStarted = true;
    }
}

int VL6180X::getRange(uint8_t &distance, uint32_t &tMid, uint32_t &dt, uint32_t timeout_us)
{
    // start a reading if one isn't already in progress
    startRangeReading();
    
    // we're going to wait until this reading ends, so consider the
    // 'start' command consumed, no matter what happens next
    rangeStarted = false;
    
    // wait for the sample
    Timer t;
    t.start();
    for (;;)
    {
        // check for a sample
        if (rangeReady())
            break;
            
        // if we've exceeded the timeout, return failure
        if (t.read_us() > timeout_us)
        {
            writeReg8(VL6180X_SYSRANGE_START, 0x00);
            return -1;
        }
    }
    
    // check for errors
    uint8_t err = (readReg8(VL6180X_RESULT_RANGE_STATUS) >> 4) & 0x0F;
    
    // read the distance
    distance = readReg8(VL6180X_RESULT_RANGE_VAL);
    
    // Read the convergence time, and compute the overall sample time.
    // Per the data sheet, the total execution time is the sum of the
    // fixed 3.2ms pre-calculation time, the convergence time, and the
    // readout averaging time.  We can query the convergence time for
    // each reading from the sensor.  The averaging time is a controlled
    // by the READOUT_AVERAGING_SAMPLE_PERIOD setting, which we set to
    // our constant value averagingSamplePeriod.
    dt = 
        3200                                                // fixed 3.2ms pre-calculation period
        + readReg32(VL6180X_RESULT_RANGE_RETURN_CONV_TIME)  // convergence time
        + (1300 + 48*averagingSamplePeriod);                // readout averaging period
        
    // figure the midpoint of the sample time - the starting time
    // plus half the collection time
    tMid = tSampleStart + dt/2;
    
    // clear the data-ready interrupt
    writeReg8(VL6180X_SYSTEM_INTERRUPT_CLEAR, 0x07);

    // return the error code
    return err;
}

void VL6180X::getRangeStats(VL6180X_RangeStats &stats)
{
    stats.returnRate = readReg16(VL6180X_RESULT_RANGE_RETURN_RATE);
    stats.refReturnRate = readReg16(VL6180X_RESULT_RANGE_REFERENCE_RATE);
    stats.returnCnt = readReg32(VL6180X_RESULT_RANGE_RETURN_SIGNAL_COUNT);
    stats.refReturnCnt = readReg32(VL6180X_RESULT_RANGE_REFERENCE_SIGNAL_COUNT);
    stats.ambCnt = readReg32(VL6180X_RESULT_RANGE_RETURN_AMB_COUNT);
    stats.refAmbCnt = readReg32(VL6180X_RESULT_RANGE_REFERENCE_AMB_COUNT);
    stats.convTime = readReg32(VL6180X_RESULT_RANGE_RETURN_CONV_TIME);
    stats.refConvTime = readReg32(VL6180X_RESULT_RANGE_REFERENCE_CONV_TIME);
}
     
uint8_t VL6180X::readReg8(uint16_t registerAddr)
{
    // write the request - MSB+LSB of register address
    uint8_t data_write[2];
    data_write[0] = (registerAddr >> 8) & 0xFF;
    data_write[1] = registerAddr & 0xFF;
    if (i2c.write(addr << 1, data_write, 2, false))
        return 0x00;

    // read the result
    uint8_t data_read[1];
    if (i2c.read(addr << 1, data_read, 1))
        return 0x00;
    
    // return the result
    return data_read[0];
}
 
uint16_t VL6180X::readReg16(uint16_t registerAddr)
{
    // write the request - MSB+LSB of register address
    uint8_t data_write[2];
    data_write[0] = (registerAddr >> 8) & 0xFF;
    data_write[1] = registerAddr & 0xFF;
    if (i2c.write(addr << 1, data_write, 2, false))
        return 0;
    
    // read the result
    uint8_t data_read[2];
    if (i2c.read(addr << 1, data_read, 2))
        return 00;
    
    // return the result
    return (data_read[0] << 8) | data_read[1];
}
 
uint32_t VL6180X::readReg32(uint16_t registerAddr)
{
    // write the request - MSB+LSB of register address
    uint8_t data_write[2];
    data_write[0] = (registerAddr >> 8) & 0xFF;
    data_write[1] = registerAddr & 0xFF;
    if (i2c.write(addr << 1, data_write, 2, false))
        return 0;
    
    // read the result
    uint8_t data_read[4];
    if (i2c.read(addr << 1, data_read, 4))
        return 0;
    
    // return the result
    return (data_read[0] << 24) | (data_read[1] << 16) | (data_read[2] << 8) | data_read[1];
}
 
void VL6180X::writeReg8(uint16_t registerAddr, uint8_t data)
{
    uint8_t data_write[3];
    data_write[0] = (registerAddr >> 8) & 0xFF;
    data_write[1] = registerAddr & 0xFF;
    data_write[2] = data & 0xFF; 
    i2c.write(addr << 1, data_write, 3);
}
 
void VL6180X::writeReg16(uint16_t registerAddr, uint16_t data)
{
    uint8_t data_write[4];
    data_write[0] = (registerAddr >> 8) & 0xFF;
    data_write[1] = registerAddr & 0xFF;
    data_write[2] = (data >> 8) & 0xFF;
    data_write[3] = data & 0xFF; 
    i2c.write(addr << 1, data_write, 4); 
}
