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
 
bool USBJoystick::update(int16_t x, int16_t y, int16_t z, uint32_t buttons, uint16_t status) 
{
   _x = x;
   _y = y;
   _z = z;
   _buttonsLo = (uint16_t)(buttons & 0xffff);
   _buttonsHi = (uint16_t)((buttons >> 16) & 0xffff);
   _status = status;
 
   // send the report
   return update();
}
 
const int reportLen = 14;
bool USBJoystick::update() 
{
   HID_REPORT report;

   // Fill the report according to the Joystick Descriptor
#define put(idx, val) (report.data[idx] = (val) & 0xff, report.data[(idx)+1] = ((val) >> 8) & 0xff)
   put(0, _status);
   put(2, 0);  // second byte of status isn't used in normal reports
   put(4, _buttonsLo);
   put(6, _buttonsHi);
   put(8, _x);
   put(10, _y);
   put(12, _z);
   report.length = reportLen;
 
   // send the report
   return sendTO(&report, 100);
}

bool USBJoystick::updateExposure(int &idx, int npix, const uint16_t *pix)
{
    HID_REPORT report;
    
    // Set the special status bits to indicate it's an exposure report.
    // The high 5 bits of the status word are set to 10000, and the
    // low 11 bits are the current pixel index.
    uint16_t s = idx | 0x8000;
    put(0, s);
        
    // now fill out the remaining words with exposure values
    report.length = reportLen;
    for (int ofs = 2 ; ofs + 1 < report.length ; ofs += 2)
    {
        uint16_t p = (idx < npix ? pix[idx++] : 0);
        put(ofs, p);
    }
    
    // send the report
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
 
bool USBJoystick::buttons(uint32_t buttons) {
     _buttonsLo = (uint16_t)(buttons & 0xffff);
     _buttonsHi = (uint16_t)((buttons >> 16) & 0xffff);
     return update();
}
 
 
void USBJoystick::_init() {
 
   _x = 0;                       
   _y = 0;     
   _z = 0;
   _buttonsLo = 0x0000;
   _buttonsHi = 0x0000;
   _status = 0;
}
 
 
uint8_t * USBJoystick::reportDesc() 
{    
    static uint8_t reportDescriptor[] = 
    {         
         USAGE_PAGE(1), 0x01,            // Generic desktop
         USAGE(1), 0x04,                 // Joystick

         COLLECTION(1), 0x01,            // Application
         
         // NB - the canonical joystick has a nested collection at this
         // point.  We remove the inner collection to enable the LedWiz 
         // emulation.  The LedWiz API implementation on the PC side
         // appears to use the collection structure as part of the
         // device signature, and the real LedWiz descriptor has just
         // one top-level collection.  The built-in Windows HID drivers 
         // don't appear to care whether this collection is present or
         // not for the purposes of recognizing a joystick, so it seems
         // to make everyone happy to leave it out.  
         //
         // All of the reference material for USB joystick device builders 
         // does use the inner collection, so it's possible that omitting 
         // it will create an incompatibility with some non-Windows hosts.  
         // But that seems largely moot in that VP only runs on Windows. 
         //  If you're you're trying to adapt this code for a different 
         // device and run into problems connecting to a non-Windows host, 
         // try restoring the inner collection.  You probably won't 
         // care about LedWiz compatibility in such a situation so there
         // should be no reason not to return to the standard structure.
       //  COLLECTION(1), 0x00,          // Physical
           
             // input report (device to host)

             USAGE_PAGE(1), 0x06,        // generic device controls - for config status
             USAGE(1), 0x00,             // undefined device control
             LOGICAL_MINIMUM(1), 0x00,   // 8-bit values
             LOGICAL_MAXIMUM(1), 0xFF,
             REPORT_SIZE(1), 0x08,       // 8 bits per report
             REPORT_COUNT(1), 0x04,      // 4 reports (4 bytes)
             INPUT(1), 0x02,             // Data, Variable, Absolute

             USAGE_PAGE(1), 0x09,        // Buttons
             USAGE_MINIMUM(1), 0x01,     // { buttons }
             USAGE_MAXIMUM(1), 0x20,     // {  1-32   }
             LOGICAL_MINIMUM(1), 0x00,   // 1-bit buttons - 0...
             LOGICAL_MAXIMUM(1), 0x01,   // ...to 1
             REPORT_SIZE(1), 0x01,       // 1 bit per report
             REPORT_COUNT(1), 0x20,      // 32 reports
             UNIT_EXPONENT(1), 0x00,     // Unit_Exponent (0)
             UNIT(1), 0x00,              // Unit (None)                                           
             INPUT(1), 0x02,             // Data, Variable, Absolute
           
             USAGE_PAGE(1), 0x01,        // Generic desktop
             USAGE(1), 0x30,             // X axis
             USAGE(1), 0x31,             // Y axis
             USAGE(1), 0x32,             // Z axis
             LOGICAL_MINIMUM(2), 0x00,0xF0,   // each value ranges -4096
             LOGICAL_MAXIMUM(2), 0x00,0x10,   // ...to +4096
             REPORT_SIZE(1), 0x10,       // 16 bits per report
             REPORT_COUNT(1), 0x03,      // 3 reports (X, Y, Z)
             INPUT(1), 0x02,             // Data, Variable, Absolute
             
             // output report (host to device)
             REPORT_SIZE(1), 0x08,       // 8 bits per report
             REPORT_COUNT(1), 0x08,      // output report count (LEDWiz messages)
             0x09, 0x01,                 // usage
             0x91, 0x01,                 // Output (array)

     //    END_COLLECTION(0),
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
        0x28,                                                       /*bLength*/
        STRING_DESCRIPTOR,                                          /*bDescriptorType 0x03*/
        'P',0,'i',0,'n',0,'s',0,'c',0,'a',0,'p',0,'e',0,
        ' ',0,'C',0,'o',0,'n',0,'t',0,'r',0,'o',0,'l',0,
        'l',0,'e',0,'r',0                                           /*String iProduct */
    };
    return stringIproductDescriptor;
}
