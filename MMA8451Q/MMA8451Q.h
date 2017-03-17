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

#ifndef MMA8451Q_H
#define MMA8451Q_H

#include "mbed.h"

/**
* MMA8451Q accelerometer example
*
* @code
* #include "mbed.h"
* #include "MMA8451Q.h"
* 
* #define MMA8451_I2C_ADDRESS (0x1d<<1)
* 
* int main(void) {
* 
* MMA8451Q acc(P_E25, P_E24, MMA8451_I2C_ADDRESS);
* PwmOut rled(LED_RED);
* PwmOut gled(LED_GREEN);
* PwmOut bled(LED_BLUE);
* 
*     while (true) {       
*         rled = 1.0 - abs(acc.getAccX());
*         gled = 1.0 - abs(acc.getAccY());
*         bled = 1.0 - abs(acc.getAccZ());
*         wait(0.1);
*     }
* }
* @endcode
*/
class MMA8451Q
{
public:
  /**
  * MMA8451Q constructor
  *
  * @param sda SDA pin
  * @param sdl SCL pin
  * @param addr addr of the I2C peripheral
  */
  MMA8451Q(PinName sda, PinName scl, int addr);

  /**
  * MMA8451Q destructor
  */
  ~MMA8451Q();
  
  /**
  *  Reset the accelerometer hardware and set our initial parameters
  */
  void init();

  /**
   * Enter standby mode
   */
  void standby();
  
  /**
   * Enter active mode
   */
  void active();
  
  /**
   * Get the value of the WHO_AM_I register
   *
   * @returns WHO_AM_I value
   */
  uint8_t getWhoAmI();

  /**
   * Get X axis acceleration
   *
   * @returns X axis acceleration
   */
  float getAccX();

  /**
   * Get Y axis acceleration
   *
   * @returns Y axis acceleration
   */
  float getAccY();
  
  /**
   *  Read an X,Y pair
   */
  void getAccXY(float &x, float &y);
  
  /**
   *  Read X,Y,Z as floats.  This is the second most efficient way 
   *  to fetch all three axes (after the integer version), since it
   *  fetches all axes in a single I2C transaction.
   */
  void getAccXYZ(float &x, float &y, float &z);
  
  /**
   *  Read X,Y,Z as integers.  This reads the three axes in a single
   *  I2C transaction and returns them in the native integer scale,
   *  so it's the most efficient way to read the current 3D status.
   *  Each axis value is represented an an integer using the device's 
   *  native 14-bit scale, so each is in the range -8192..+8191.
   */
  void getAccXYZ(int &x, int &y, int &z);

  /**
   * Get Z axis acceleration
   *
   * @returns Z axis acceleration
   */
  float getAccZ();

  /**
   * Get XYZ axis acceleration
   *
   * @param res array where acceleration data will be stored
   */
  void getAccAllAxis(float * res);
  
  /**
   * Set interrupt mode.  'pin' is 1 for INT1_ACCEL (PTA14) and 2 for INT2_ACCEL (PTA15).
   * The caller is responsible for setting up an interrupt handler on the corresponding
   * PTAxx pin.
   */
  void setInterruptMode(int pin);
  
  /**
   * Set the hardware dynamic range, in G.  Valid ranges are 2, 4, and 8.
   */
  void setRange(int g);
  
  /**
   * Disable interrupts.
   */
  void clearInterruptMode();
  
  /**
   * Is a sample ready?
   */
  bool sampleReady();
  
  /**
   * Get the number of FIFO samples available
   */
  int getFIFOCount();

private:
  I2C m_i2c;
  int m_addr;
  void readRegs(int addr, uint8_t * data, int len);
  void writeRegs(uint8_t * data, int len);
  int16_t getAccAxis(uint8_t addr);
  
  // Translate a 14-bit register value to a signed integer.  The
  // most significant 8 bits are in the first byte, and the least
  // significant 6 bits are in the second byte.  To adjust to a
  // regular integer, left-justify the 14 bits in an int16_t, then
  // divide by 4 to shift out the unused low two bits.  Note that
  // we have to divide rather than right-shift (>>) to ensure proper
  // filling of the sign bits.  The compiler should convert the
  // divide-by-4 to an arithmetic shift right by 2, so this should
  // still be efficient.
  inline int xlat14(const uint8_t *buf)
  {
      // Store the 16 bits left-justified in an int16_t, then cast
      // to a regular int to sign-extend to the full int width. 
      // Divide the result by 4 to shift out the unused 2 bits
      // at the right end.
      return int(int16_t((buf[0] << 8) | buf[1])) / 4;
  }

};

#endif
