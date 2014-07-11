/* Copyright (c) 2010-2011 mbed.org, MIT License
* Modified Mouse code for Joystick - WH 2012
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
 
#include "stdint.h"
#include "USBJoystick.h"
 
bool USBJoystick::update(int16_t x, int16_t y, int16_t z, uint16_t buttons) 
{
   _x = x;
   _y = y;
   _z = z;
   _buttons = buttons;     
 
   // send the report
   return update();
}
 
bool USBJoystick::update() {
   HID_REPORT report;
 
   // Fill the report according to the Joystick Descriptor
   report.data[0] = _buttons & 0xff;
   report.data[1] = (_buttons >> 8) & 0xff;
   report.data[2] = _x & 0xff;            
   report.data[3] = _y & 0xff;            
   report.data[4] = _z & 0xff;
   report.length = 5; 
 
   return send(&report);
}
 
bool USBJoystick::move(int16_t x, int16_t y) {
     _x = x;
     _y = y;
     return update();
}

bool USBJoystick::setZ(int16_t z) {
    _z = z;
    return update();
}
 
bool USBJoystick::buttons(uint16_t buttons) {
     _buttons = buttons;
     return update();
}
 
 
void USBJoystick::_init() {
 
   _x = 0;                       
   _y = 0;     
   _z = 0;
   _buttons = 0x0000;
}
 
 
uint8_t * USBJoystick::reportDesc() 
{    
    static uint8_t reportDescriptor[] = 
    {         
         USAGE_PAGE(1), 0x01,            // Generic desktop
         USAGE(1), 0x04,                 // Joystick

         COLLECTION(1), 0x01,            // Application
      //     COLLECTION(1), 0x00,          // Physical
           
             USAGE_PAGE(1), 0x09,        // Buttons
             USAGE_MINIMUM(1), 0x01,     // { buttons }
             USAGE_MAXIMUM(1), 0x10,     // {  1-16   }
             LOGICAL_MINIMUM(1), 0x00,   // 1-bit buttons - 0...
             LOGICAL_MAXIMUM(1), 0x01,   // ...to 1
             REPORT_SIZE(1), 0x01,       // 1 bit per report
             REPORT_COUNT(1), 0x10,      // 16 reports
             UNIT_EXPONENT(1), 0x00,     // Unit_Exponent (0)
             UNIT(1), 0x00,              // Unit (None)                                           
             INPUT(1), 0x02,             // Data, Variable, Absolute
           
             USAGE_PAGE(1), 0x01,        // Generic desktop
             USAGE(1), 0x30,             // X
             USAGE(1), 0x31,             // Y
             USAGE(1), 0x32,             // Z
             LOGICAL_MINIMUM(1), 0x81,   // each value ranges -127...
             LOGICAL_MAXIMUM(1), 0x7f,   // ...to 127
             REPORT_SIZE(1), 0x08,       // 8 bits per report
             REPORT_COUNT(1), 0x03,      // 3 reports
             INPUT(1), 0x02,             // Data, Variable, Absolute
        
             REPORT_COUNT(1), 0x08,      // input report count (LEDWiz messages)
             0x09, 0x01,                 // usage
             0x91, 0x01,                 // Output (array)

     //      END_COLLECTION(0),
         END_COLLECTION(0)
      };
 
      reportLength = sizeof(reportDescriptor);
      return reportDescriptor;
}
 
 uint8_t * USBJoystick::stringImanufacturerDesc() {
    static uint8_t stringImanufacturerDescriptor[] = {
        0x10,                                            /*bLength*/
        STRING_DESCRIPTOR,                               /*bDescriptorType 0x03*/
        'm',0,'j',0,'r',0,'c',0,'o',0,'r',0,'p',0        /*bString iManufacturer - mjrcorp*/
    };
    return stringImanufacturerDescriptor;
}

uint8_t * USBJoystick::stringIserialDesc() {
    static uint8_t stringIserialDescriptor[] = {
        0x16,                                                           /*bLength*/
        STRING_DESCRIPTOR,                                              /*bDescriptorType 0x03*/
        '0',0,'1',0,'2',0,'3',0,'4',0,'5',0,'6',0,'7',0,'8',0,'9',0,    /*bString iSerial - 0123456789*/
    };
    return stringIserialDescriptor;
}

uint8_t * USBJoystick::stringIproductDesc() {
    static uint8_t stringIproductDescriptor[] = {
        0x1E,                                                       /*bLength*/
        STRING_DESCRIPTOR,                                          /*bDescriptorType 0x03*/
        'P',0,'i',0,'n',0,'M',0,'a',0,'s',0,'t',0,'e',0,'r',0,
        ' ',0,'2',0,'0',0,'0',0,'0',0                               /*String iProduct - PinMaster 2000*/
    };
    return stringIproductDescriptor;
}
