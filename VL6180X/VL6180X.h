// VL6180X Time of Flight sensor interface

#ifndef _VL6180X_H_
#define _VL6180X_H_

#include "mbed.h"
#include "BitBangI2C.h"


class MyI2C: public I2C
{
public:
    MyI2C(PinName sda, PinName scl) : I2C(sda, scl) { }
    
    int write(int addr, const uint8_t *data, size_t len, bool repeated = false)
    {
        return I2C::write(addr, (const char *)data, len, repeated);
    }
    int read(int addr, uint8_t *data, size_t len, bool repeated = false)
    {
        return I2C::read(addr, (char *)data, len, repeated);
    }
    
    void reset() { }
};

#define VL6180X_IDENTIFICATION_MODEL_ID              0x0000
#define VL6180X_IDENTIFICATION_MODEL_REV_MAJOR       0x0001
#define VL6180X_IDENTIFICATION_MODEL_REV_MINOR       0x0002
#define VL6180X_IDENTIFICATION_MODULE_REV_MAJOR      0x0003
#define VL6180X_IDENTIFICATION_MODULE_REV_MINOR      0x0004
#define VL6180X_IDENTIFICATION_DATE                  0x0006  // NB - 16-bit value
#define VL6180X_IDENTIFICATION_TIME                  0x0008  // NB - 16-bit value
 
#define VL6180X_SYSTEM_MODE_GPIO0                    0x0010
#define VL6180X_SYSTEM_MODE_GPIO1                    0x0011
#define VL6180X_SYSTEM_HISTORY_CTRL                  0x0012
#define VL6180X_SYSTEM_INTERRUPT_CONFIG_GPIO         0x0014
#define VL6180X_SYSTEM_INTERRUPT_CLEAR               0x0015
#define VL6180X_SYSTEM_FRESH_OUT_OF_RESET            0x0016
#define VL6180X_SYSTEM_GROUPED_PARAMETER_HOLD        0x0017
 
#define VL6180X_SYSRANGE_START                       0x0018
#define VL6180X_SYSRANGE_THRESH_HIGH                 0x0019
#define VL6180X_SYSRANGE_THRESH_LOW                  0x001A
#define VL6180X_SYSRANGE_INTERMEASUREMENT_PERIOD     0x001B
#define VL6180X_SYSRANGE_MAX_CONVERGENCE_TIME        0x001C
#define VL6180X_SYSRANGE_CROSSTALK_COMPENSATION_RATE 0x001E
#define VL6180X_SYSRANGE_CROSSTALK_VALID_HEIGHT      0x0021
#define VL6180X_SYSRANGE_EARLY_CONVERGENCE_ESTIMATE  0x0022
#define VL6180X_SYSRANGE_PART_TO_PART_RANGE_OFFSET   0x0024
#define VL6180X_SYSRANGE_RANGE_IGNORE_VALID_HEIGHT   0x0025
#define VL6180X_SYSRANGE_RANGE_IGNORE_THRESHOLD      0x0026
#define VL6180X_SYSRANGE_MAX_AMBIENT_LEVEL_MULT      0x002C
#define VL6180X_SYSRANGE_RANGE_CHECK_ENABLES         0x002D
#define VL6180X_SYSRANGE_VHV_RECALIBRATE             0x002E
#define VL6180X_SYSRANGE_VHV_REPEAT_RATE             0x0031
 
#define VL6180X_SYSALS_START                         0x0038
#define VL6180X_SYSALS_THRESH_HIGH                   0x003A
#define VL6180X_SYSALS_THRESH_LOW                    0x003C
#define VL6180X_SYSALS_INTERMEASUREMENT_PERIOD       0x003E
#define VL6180X_SYSALS_ANALOGUE_GAIN                 0x003F
#define VL6180X_SYSALS_INTEGRATION_PERIOD            0x0040
 
#define VL6180X_RESULT_RANGE_STATUS                  0x004D
#define VL6180X_RESULT_ALS_STATUS                    0x004E
#define VL6180X_RESULT_INTERRUPT_STATUS_GPIO         0x004F
#define VL6180X_RESULT_ALS_VAL                       0x0050
#define VL6180X_RESULT_HISTORY_BUFFER                0x0052 
#define VL6180X_RESULT_RANGE_VAL                     0x0062
#define VL6180X_RESULT_RANGE_RAW                     0x0064
#define VL6180X_RESULT_RANGE_RETURN_RATE             0x0066
#define VL6180X_RESULT_RANGE_REFERENCE_RATE          0x0068
#define VL6180X_RESULT_RANGE_RETURN_SIGNAL_COUNT     0x006C
#define VL6180X_RESULT_RANGE_REFERENCE_SIGNAL_COUNT  0x0070
#define VL6180X_RESULT_RANGE_RETURN_AMB_COUNT        0x0074
#define VL6180X_RESULT_RANGE_REFERENCE_AMB_COUNT     0x0078
#define VL6180X_RESULT_RANGE_RETURN_CONV_TIME        0x007C
#define VL6180X_RESULT_RANGE_REFERENCE_CONV_TIME     0x0080
 
#define VL6180X_READOUT_AVERAGING_SAMPLE_PERIOD      0x010A
#define VL6180X_FIRMWARE_BOOTUP                      0x0119
#define VL6180X_FIRMWARE_RESULT_SCALER               0x0120
#define VL6180X_I2C_SLAVE_DEVICE_ADDRESS             0x0212
#define VL6180X_INTERLEAVED_MODE_ENABLE              0x02A3
 
// gain settings
enum VL6180X_ALS_Gain 
{
    GAIN_20 = 0,    // 20
    GAIN_10,        // 10.32
    GAIN_5,         // 5.21
    GAIN_2_5,       // 2.60
    GAIN_1_67,      // 1.72
    GAIN_1_25,      // 1.28
    GAIN_1,         // 1.01
    GAIN_40,        // 40 
};

// identification
struct VL6180X_ID 
{
    uint8_t model;              // model number
    uint8_t modelRevMajor;      // model revision number major...
    uint8_t modelRevMinor;      // ...and minor
    uint8_t moduleRevMajor;     // module revision number major...
    uint8_t moduleRevMinor;     // ... and minior
    struct
    {
        uint8_t month;          // month 1..12
        uint8_t day;            // day of month 1..31
        uint16_t year;          // calendar year, 4-digit (e.g., 2016)
        uint8_t phase;          // manufacturing phase, 0..7
        uint8_t hh;             // hour, 0..23
        uint8_t mm;             // minute, 0..59
        uint8_t ss;             // second, 0..59
    } manufDate;                // manufacturing date and time
};

// range statistics
struct VL6180X_RangeStats
{
    uint16_t returnRate;        // return signal rate
    uint16_t refReturnRate;     // reference return rate
    uint32_t returnCnt;         // return signal count
    uint32_t refReturnCnt;      // reference return count
    uint32_t ambCnt;            // ambient count
    uint32_t refAmbCnt;         // reference ambient count
    uint32_t convTime;          // convergence time
    uint32_t refConvTime;       // reference convergence time
};

class VL6180X
{
public:
    // Set up the interface with the given I2C pins, I2C address, and
    // the GPIO0 pin (for resetting the sensor at startup).
    //
    // Note that the power-on default I2C address is always 0x29.  The
    // address can be changed during a session, but it's not saved 
    // persistently; it always resets to 0x29 on the next power cycle.
    VL6180X(PinName sda, PinName scl, uint8_t addr, PinName gpio0);
    
    // destruction
    ~VL6180X();

    // Send the required initialization sequence.  Returns true on
    // success, false on failure.
    bool init();

    // set up default settings
    void setDefaults();
    
    // Start a distance reading, returning immediately without waiting
    // for the reading to finish.  The caller can poll for the finished
    // reading via distanceReady().
    void startRangeReading();

    // Get TOF range distance in mm.  Returns 0 on success, a device
    // "range error code" (>0) on failure, or -1 on timeout.
    int getRange(uint8_t &distance, uint32_t timeout_us);
    
    // get range statistics
    void getRangeStats(VL6180X_RangeStats &stats);

    // set continuous distance mode
    void continuousDistanceMode(bool on);
    
    // is a sample ready?
    bool rangeReady();

    // get ambient light level in lux
    float getAmbientLight(VL6180X_ALS_Gain gain);

    // get identification data
    void getID(VL6180X_ID &id);

    // Change the address of the device.  Returns the new address.
    uint8_t changeAddress(uint8_t newAddress);
 

protected:
    // I2C interface to device
    BitBangI2C i2c;
    
    // GPIO0 pin for hard reset
    DigitalInOut gpio0Pin;
    
    // device address
    uint8_t addr;
    
    // current distance mode: 0=single shot, 1=continuous
    bool distMode;

    // read registers
    uint8_t readReg8(uint16_t regAddr);
    uint16_t readReg16(uint16_t regAddr);
    uint32_t readReg32(uint16_t regAddr);

    // write registers
    void writeReg8(uint16_t regAddr, uint8_t data);
    void writeReg16(uint16_t regAddr, uint16_t data);
};
 
#endif
