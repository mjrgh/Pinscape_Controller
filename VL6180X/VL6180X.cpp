// VL6180X Time of Flight sensor interface

#include "mbed.h"
#include "VL6180X.h"

VL6180X::VL6180X(PinName sda, PinName scl, uint8_t addr, PinName gpio0)
    : i2c(sda, scl), gpio0Pin(gpio0)
{
    // remember the address
    this->addr = addr;
    
    // start in single-shot distance mode
    distMode = 0;
    
    // initially reset the sensor
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
    
    // release reset to allow the sensor to reboot
    gpio0Pin.input();
    wait_us(10000);
    
    // reset the I2C bus
    i2c.reset();
    
    // check that the sensor's reset register reads as '1'
    Timer t;
    t.start();
    while (readReg8(VL6180X_SYSTEM_FRESH_OUT_OF_RESET) != 1)
    {
        if (t.read_us() > 10000000)
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
    
    // success
    return true;
}
 
void VL6180X::setDefaults()
{
    writeReg8(VL6180X_SYSTEM_GROUPED_PARAMETER_HOLD, 0x01);         // set parameter hold while updating settings
    
    writeReg8(VL6180X_SYSTEM_INTERRUPT_CONFIG_GPIO, (4<<3) | 4);    // Enable interrupts from range and ambient integrator
    writeReg8(VL6180X_SYSTEM_MODE_GPIO1, 0x10);                     // Set GPIO1 low when sample complete
    writeReg8(VL6180X_SYSRANGE_VHV_REPEAT_RATE, 0xFF);              // Set auto calibration period (Max = 255)/(OFF = 0)
    writeReg8(VL6180X_SYSRANGE_INTERMEASUREMENT_PERIOD, 0x09);      // Set default ranging inter-measurement period to 100ms
    writeReg8(VL6180X_SYSRANGE_MAX_CONVERGENCE_TIME, 0x32);         // Max range convergence time 48ms
    writeReg8(VL6180X_SYSRANGE_RANGE_CHECK_ENABLES, 0x11);          // S/N enable, ignore disable, early convergence test enable
    writeReg16(VL6180X_SYSRANGE_EARLY_CONVERGENCE_ESTIMATE, 0x7B);  // abort range measurement if convergence rate below this value

    writeReg8(VL6180X_SYSALS_INTERMEASUREMENT_PERIOD, 0x0A);        // Set default ALS inter-measurement period to 100ms
    writeReg8(VL6180X_SYSALS_ANALOGUE_GAIN, 0x46);                  // Set the ALS gain
    writeReg16(VL6180X_SYSALS_INTEGRATION_PERIOD, 0x63);            // ALS integration time 100ms
    
    writeReg8(VL6180X_READOUT_AVERAGING_SAMPLE_PERIOD, 0x30);       // Sample averaging period (1.3ms + N*64.5us)
    writeReg8(VL6180X_FIRMWARE_RESULT_SCALER, 0x01);

    writeReg8(VL6180X_SYSTEM_GROUPED_PARAMETER_HOLD, 0x00);         // end parameter hold

    // perform a single calibration; wait until it's done (within reason)
    Timer t;
    t.start();
    writeReg8(VL6180X_SYSRANGE_VHV_RECALIBRATE, 0x01);
    while (readReg8(VL6180X_SYSRANGE_VHV_RECALIBRATE) != 0)
    {
        // if we've been waiting too long, abort
        if (t.read_us() > 1000000)
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
 
 
uint8_t VL6180X::changeAddress(uint8_t newAddress)
{  
    // do nothing if the address is the same or it's out of range
    if (newAddress == addr || newAddress > 127)
        return addr;

    // set the new address    
    writeReg8(VL6180X_I2C_SLAVE_DEVICE_ADDRESS, newAddress);
    
    // read it back and store it
    addr = readReg8(VL6180X_I2C_SLAVE_DEVICE_ADDRESS); 
    
    // return the new address
    return addr;
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
    return (readReg8(VL6180X_RESULT_INTERRUPT_STATUS_GPIO) & 0x07) == 4;
}
 
void VL6180X::startRangeReading()
{
    writeReg8(VL6180X_SYSRANGE_START, 0x01);
}

int VL6180X::getRange(uint8_t &distance, uint32_t timeout_us)
{
    if (!rangeReady())
        writeReg8(VL6180X_SYSRANGE_START, 0x01);
    
    // wait for the sample
    Timer t;
    t.start();
    for (;;)
    {
        // if the GPIO pin is high, the sample is ready
        if (rangeReady())
            break;
            
        // if we've exceeded the timeout, return failure
        if (t.read_us() > timeout_us)
            return -1;
    }
    
    // check for errors
    uint8_t err = (readReg8(VL6180X_RESULT_RANGE_STATUS) >> 4) & 0x0F;
    
    // read the distance
    distance = readReg8(VL6180X_RESULT_RANGE_VAL);
    
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
     
float VL6180X::getAmbientLight(VL6180X_ALS_Gain gain)
{
    // set the desired gain
    writeReg8(VL6180X_SYSALS_ANALOGUE_GAIN, (0x40 | gain));
    
    // start the integration
    writeReg8(VL6180X_SYSALS_START, 0x01);

    // give it time to integrate
    wait_ms(100);
    
    // clear the data-ready interrupt
    writeReg8(VL6180X_SYSTEM_INTERRUPT_CLEAR, 0x07);

    // retrieve the raw sensor reading om the sensoe
    unsigned int alsRaw = readReg16(VL6180X_RESULT_ALS_VAL);
    
    // get the integration period
    unsigned int tIntRaw = readReg16(VL6180X_SYSALS_INTEGRATION_PERIOD);
    float alsIntegrationPeriod = 100.0 / tIntRaw ;
    
    // get the actual gain at the user's gain setting
    float trueGain = 0.0;
    switch (gain)
    {
    case GAIN_20:   trueGain = 20.0; break;
    case GAIN_10:   trueGain = 10.32; break;
    case GAIN_5:    trueGain = 5.21; break;
    case GAIN_2_5:  trueGain = 2.60; break;
    case GAIN_1_67: trueGain = 1.72; break;
    case GAIN_1_25: trueGain = 1.28; break;
    case GAIN_1:    trueGain = 1.01; break;
    case GAIN_40:   trueGain = 40.0; break;
    default:        trueGain = 1.0;  break;
    }
    
    // calculate the lux (see the manufacturer's app notes)
    return alsRaw  * 0.32f / trueGain * alsIntegrationPeriod;
}
 
uint8_t VL6180X::readReg8(uint16_t registerAddr)
{
    // write the request - MSB+LSB of register address
    uint8_t data_write[2];
    data_write[0] = (registerAddr >> 8) & 0xFF;
    data_write[1] = registerAddr & 0xFF;
    if (i2c.write(addr << 1, data_write, 2, true))
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
    if (i2c.write(addr << 1, data_write, 2, true))
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
