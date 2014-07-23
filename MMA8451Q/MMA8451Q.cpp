/* Copyright (c) 2010-2011 mbed.org, MIT License
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software
* and associated documentation files (the "Software"), to deal in the Software without
* restriction, including without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or
* substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
* BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
* DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "MMA8451Q.h"

#define REG_WHO_AM_I      0x0D
#define REG_CTRL_REG_1    0x2A
#define REG_CTRL_REG_2    0x2B
#define REG_CTRL_REG_3    0x2c
#define REG_CTRL_REG_4    0x2D
#define REG_CTRL_REG_5    0x2E
#define REG_OFF_X         0x2F
#define REG_OFF_Y         0x30
#define REG_OFF_Z         0x31
#define XYZ_DATA_CFG_REG  0x0E
#define REG_OUT_X_MSB     0x01
#define REG_OUT_Y_MSB     0x03
#define REG_OUT_Z_MSB     0x05

#define UINT14_MAX        16383

#define CTL_ACTIVE        0x01
#define FS_MASK           0x03
#define FS_2G             0x00
#define FS_4G             0x01
#define FS_8G             0x02

#define HPF_OUT_MASK      0x10

#define MODS1_MASK        0x02
#define MODS0_MASK        0x01
#define SMODS_MASK        0x18
#define MODS_MASK         0x03

#define DR_MASK           0x38
#define DR_800_HZ         0x00
#define DR_400_HZ         0x08
#define DR_200_HZ         0x10
#define DR_100_HZ         0x18
#define DR_50_HZ          0x20
#define DR_12_HZ          0x28
#define DR_6_HZ           0x30
#define DR_1_HZ           0x38

#define CTRL_REG3_IPOL_MASK  0x02
#define CTRL_REG3_PPOD_MASK  0x01

#define INT_EN_DRDY       0x01
#define INT_CFG_DRDY      0x01


MMA8451Q::MMA8451Q(PinName sda, PinName scl, int addr) : m_i2c(sda, scl), m_addr(addr) 
{
    // go to standby mode
    standby();
    
#if 0
    // reset all registers to power-on reset values
    uint8_t d0[2] = { REG_CTRL_REG_2, 0x40 };
    writeRegs(d0,2 );
    
    // wait for the reset bit to clear
    do {
        readRegs(REG_CTRL_REG_2, d0, 1);
    } while ((d0[0] & 0x40) != 0);
#endif
    
    // read the curent config register
    uint8_t d1[1];
    readRegs(XYZ_DATA_CFG_REG, d1, 1);
    
    // set 2g mode
    uint8_t d2[2] = { XYZ_DATA_CFG_REG, (d1[0] & ~FS_MASK) | FS_2G };
    writeRegs(d2, 2);
    
    // read the ctl2 register
    uint8_t d3[1];
    readRegs(REG_CTRL_REG_2, d3, 1);
    
    // set the high resolution mode
    uint8_t d4[2] = {REG_CTRL_REG_2, (d3[0] & ~MODS_MASK) | MODS1_MASK};
    writeRegs(d4, 2);
    
    // set 800 Hz mode
    uint8_t d5[1];
    readRegs(REG_CTRL_REG_1, d5, 1);
    uint8_t d6[2] = {REG_CTRL_REG_1, (d5[0] & ~DR_MASK) | DR_800_HZ};
    writeRegs(d6, 2);
    
    // enter active mode
    active();
}

MMA8451Q::~MMA8451Q() { }

void MMA8451Q::setInterruptMode(int pin)
{
    // go to standby mode
    standby();

    // set IRQ push/pull and active high
    uint8_t d1[1];
    readRegs(REG_CTRL_REG_3, d1, 1);
    uint8_t d2[2] = {
        REG_CTRL_REG_3, 
        (d1[0] & ~CTRL_REG3_PPOD_MASK) | CTRL_REG3_IPOL_MASK
    };
    writeRegs(d2, 2);
    
    // set pin 2 or pin 1
    readRegs(REG_CTRL_REG_5, d1, 1);
    uint8_t d3[2] = { 
        REG_CTRL_REG_5, 
        (d1[0] & ~INT_CFG_DRDY) | (pin == 1 ? INT_CFG_DRDY : 0)
    };
    writeRegs(d3, 2);
    
    // enable data ready interrupt
    readRegs(REG_CTRL_REG_4, d1, 1);
    uint8_t d4[2] = { REG_CTRL_REG_4, d1[0] | INT_EN_DRDY };
    writeRegs(d4, 2);
    
    // enter active mode
    active();
}

void MMA8451Q::standby()
{
    // read the current control register
    uint8_t d1[1];
    readRegs(REG_CTRL_REG_1, d1, 1);
    
    // write it back witht he Active bit cleared
    uint8_t d2[2] = { REG_CTRL_REG_1, d1[0] & ~CTL_ACTIVE };
    writeRegs(d2, 2);
}

void MMA8451Q::active()
{
    // read the current control register
    uint8_t d1[1];
    readRegs(REG_CTRL_REG_1, d1, 1);
    
    // write it back out with the Active bit set
    uint8_t d2[2] = { REG_CTRL_REG_1, d1[0] | CTL_ACTIVE };
    writeRegs(d2, 2);
}

uint8_t MMA8451Q::getWhoAmI() {
    uint8_t who_am_i = 0;
    readRegs(REG_WHO_AM_I, &who_am_i, 1);
    return who_am_i;
}

float MMA8451Q::getAccX() {
    return (float(getAccAxis(REG_OUT_X_MSB))/4096.0);
}

void MMA8451Q::getAccXY(float &x, float &y) 
{
    // read the X and Y output registers
    uint8_t res[4];
    readRegs(REG_OUT_X_MSB, res, 4);
    
    // translate the x value
    uint16_t acc = (res[0] << 8) | (res[1]);
    x = int16_t(acc)/(4*4096.0);
    
    // translate the y value
    acc = (res[2] << 9) | (res[3]);
    y = int16_t(acc)/(4*4096.0);
}

void MMA8451Q::getAccXYZ(float &x, float &y, float &z)
{
    // read the X, Y, and Z output registers
    uint8_t res[6];
    readRegs(REG_OUT_X_MSB, res, 6);
    
    // translate the x value
    uint16_t acc = (res[0] << 8) | (res[1]);
    x = int16_t(acc)/(4*4096.0);
    
    // translate the y value
    acc = (res[2] << 8) | (res[3]);
    y = int16_t(acc)/(4*4096.0);
    
    // translate the z value
    acc = (res[4] << 8) | (res[5]);
    z = int16_t(acc)/(4*4096.0);
}

float MMA8451Q::getAccY() {
    return (float(getAccAxis(REG_OUT_Y_MSB))/4096.0);
}

float MMA8451Q::getAccZ() {
    return (float(getAccAxis(REG_OUT_Z_MSB))/4096.0);
}

void MMA8451Q::getAccAllAxis(float * res) {
    res[0] = getAccX();
    res[1] = getAccY();
    res[2] = getAccZ();
}

int16_t MMA8451Q::getAccAxis(uint8_t addr) {
    int16_t acc;
    uint8_t res[2];
    readRegs(addr, res, 2);

    acc = (res[0] << 6) | (res[1] >> 2);
    if (acc > UINT14_MAX/2)
        acc -= UINT14_MAX;

    return acc;
}

void MMA8451Q::readRegs(int addr, uint8_t * data, int len) {
    char t[1] = {addr};
    m_i2c.write(m_addr, t, 1, true);
    m_i2c.read(m_addr, (char *)data, len);
}

void MMA8451Q::writeRegs(uint8_t * data, int len) {
    m_i2c.write(m_addr, (char *)data, len);
}
