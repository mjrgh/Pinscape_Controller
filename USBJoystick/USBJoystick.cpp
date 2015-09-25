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

#include "config.h"  // Pinscape configuration

// Length of our joystick reports.  Important: This must be kept in sync 
// with the actual joystick report format sent in update().
const int reportLen = 14;

#ifdef ENABLE_JOYSTICK
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
   
   // important: keep reportLen in sync with the actual byte length of
   // the reports we build here
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
    
    // start at the second byte
    int ofs = 2;
    
    // in the first report, add the total pixel count as the next two bytes
    if (idx == 0)
    {
        put(ofs, npix);
        ofs += 2;
    }
        
    // now fill out the remaining words with exposure values
    report.length = reportLen;
    for ( ; ofs + 1 < report.length ; ofs += 2)
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

#else /* ENABLE_JOYSTICK */

bool USBJoystick::updateStatus(uint32_t status)
{
   HID_REPORT report;

   // Fill the report according to the Joystick Descriptor
#define put(idx, val) (report.data[idx] = (val) & 0xff, report.data[(idx)+1] = ((val) >> 8) & 0xff)
   memset(report.data, 0, reportLen);
   put(0, status);
   report.length = reportLen;
 
   // send the report
   return sendTO(&report, 100);
}

#endif /* ENABLE_JOYSTICK */
 
 
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
#ifdef ENABLE_JOYSTICK
    // Joystick reports are enabled.  Use the full joystick report
    // format.
    static uint8_t reportDescriptor[] = 
    {         
         USAGE_PAGE(1), 0x01,            // Generic desktop
         USAGE(1), 0x04,                 // Joystick
         COLLECTION(1), 0x01,            // Application
         
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
             REPORT_COUNT(1), 0x08,      // output report count - 8-byte LedWiz format
             0x09, 0x01,                 // usage
             0x91, 0x01,                 // Output (array)

         END_COLLECTION(0)

      };
#else /* defined(ENABLE_JOYSTICK) */

    // Joystick reports are disabled.  We still want to appear
    // as a USB device for the LedWiz output emulation, but we
    // don't want to appear as a joystick.
     
    static uint8_t reportDescriptor[] = 
    {         
         USAGE_PAGE(1), 0x01,            // Generic desktop
         USAGE(1), 0x00,                 // Undefined

         COLLECTION(1), 0x01,            // Application
         
             // input report (device to host)
             USAGE_PAGE(1), 0x06,        // generic device controls - for config status
             USAGE(1), 0x00,             // undefined device control
             LOGICAL_MINIMUM(1), 0x00,   // 8-bit values
             LOGICAL_MAXIMUM(1), 0xFF,
             REPORT_SIZE(1), 0x08,       // 8 bits per report
             REPORT_COUNT(1), reportLen, // standard report length (same as if we were in joystick mode)
             INPUT(1), 0x02,             // Data, Variable, Absolute

             // output report (host to device)
             REPORT_SIZE(1), 0x08,       // 8 bits per report
             REPORT_COUNT(1), 0x08,      // output report count (LEDWiz messages)
             0x09, 0x01,                 // usage
             0x91, 0x01,                 // Output (array)

         END_COLLECTION(0)
      };
 
#endif /* defined(ENABLE_JOYSTICK) */

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
