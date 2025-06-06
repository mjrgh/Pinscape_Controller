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



// Maximum report sizes
const int MAX_REPORT_JS_TX = USBJoystick::reportLen;
const int MAX_REPORT_JS_RX = 8;
const int MAX_REPORT_KB_TX = 8;
const int MAX_REPORT_KB_RX = 4;

bool USBJoystick::update(int16_t x, int16_t y, int16_t z, int16_t z0, int16_t vx, int16_t vy, int16_t vz, uint32_t buttons, uint16_t status) 
{
   _x = x;
   _y = y;
   _z = z;
   _z0 = z0;
   _vx = vx;
   _vy = vy;
   _vz = vz;
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
#define putbe(idx, val) (report.data[(idx)+1] = (val) & 0xff, report.data[idx] = ((val) >> 8) & 0xff)
#define putl(idx, val) (put(idx, val), put((idx)+2, (val) >> 16))
#define putlbe(idx, val) (putbe((idx)+2, val), putbe(idx, (val) >> 16))
#define put64(idx, val) (putl(idx, val), putl((idx)+4, (val) >> 32))
   put(0, _status);
   put(2, 0);  // second word of status - zero in high bit identifies as normal joystick report
   put(4, _buttonsLo);
   put(6, _buttonsHi);

   switch (axisFormat)
   {
   case AXIS_FORMAT_XYZ:
      // accelerations/plunger position only, on X/Y/Z; RX/RY/RZ unused
      put(8, _x);
      put(10, _y);
      put(12, _z);
      put(14, 0);
      put(16, 0);
      put(18, 0);
      put(20, 0);
      break;

   case AXIS_FORMAT_RXRYRZ:
      // accelerations/plunger position only, on RX/RY/RZ; X/Y/Z unused
      put(8, 0);
      put(10, 0);
      put(12, 0);
      put(14, _x);
      put(16, _y);
      put(18, _z);
      put(20, 0);
      break;

   case AXIS_FORMAT_XYZ_RXRYRZ:
      // accelerations/plunger position on X/Y/Z, velocities/plunger speed on RX/RY/RZ,
      // processed plunger position (with firing event detection) on slider1
      put(8, _x);
      put(10, _y);
      put(12, _z0);
      put(14, _vx);
      put(16, _vy);
      put(18, _vz);
      put(20, _z);
      break;
   }
   
   // important: keep reportLen in sync with the actual byte length of
   // the reports we build here
   report.length = reportLen;
 
   // send the report
   return sendTO(&report, 100);
}

bool USBJoystick::kbUpdate(uint8_t data[8])
{
    // set up the report
    HID_REPORT report;
    report.data[0] = REPORT_ID_KB;      // report ID = keyboard
    memcpy(&report.data[1], data, 8);   // copy the kb report data
    report.length = 9;                  // length = ID prefix + kb report length
    
    // send it to endpoint 4 (the keyboard interface endpoint)
    return writeTO(EP4IN, report.data, report.length, MAX_PACKET_SIZE_EPINT, 100);
}

bool USBJoystick::mediaUpdate(uint8_t data)
{
    // set up the report
    HID_REPORT report;
    report.data[0] = REPORT_ID_MEDIA;   // report ID = media
    report.data[1] = data;              // key pressed bits
    report.length = 2;
    
    // send it
    return writeTO(EP4IN, report.data, report.length, MAX_PACKET_SIZE_EPINT, 100);
}
 
bool USBJoystick::sendPlungerStatus(int npix, int plungerPos, int flags, 
    uint32_t avgScanTime, uint32_t processingTime, int16_t speed)
{
    HID_REPORT report;
    
    // Set the special status bits to indicate it's an extended
    // exposure report.
    put(0, 0x87FF);
    
    // start at the second byte
    int ofs = 2;
    
    // write the report subtype (0) to byte 2
    report.data[ofs++] = 0;

    // write the number of pixels to bytes 3-4
    put(ofs, static_cast<uint16_t>(npix));
    ofs += 2;
    
    // write the detected plunger position to bytes 5-6
    put(ofs, static_cast<uint16_t>(plungerPos));
    ofs += 2;
    
    // Add the calibration mode flag if applicable
    extern bool plungerCalMode;
    if (plungerCalMode) flags |= 0x04;

    // add the speed-reported flag
    flags |= 0x08;

    // write the flags to byte 7
    report.data[ofs++] = flags;
    
    // write the average scan time in 10us intervals to bytes 8-10
    uint32_t t = uint32_t(avgScanTime / 10);
    report.data[ofs++] = t & 0xff;
    report.data[ofs++] = (t >> 8) & 0xff;
    report.data[ofs++] = (t >> 16) & 0xff;
    
    // write the processing time to bytes 11-13
    t = uint32_t(processingTime / 10);
    report.data[ofs++] = t & 0xff;
    report.data[ofs++] = (t >> 8) & 0xff;
    report.data[ofs++] = (t >> 16) & 0xff;

    // add the speed at bytes 14-15
    put(ofs, static_cast<uint16_t>(speed));
    
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

bool USBJoystick::sendPlungerStatus2(
    int nativeScale, 
    int jitterLo, int jitterHi, int rawPos,
    int axcTime)
{
    HID_REPORT report;
    memset(report.data, 0, sizeof(report.data));
    
    // Set the special status bits to indicate it's an extended
    // exposure report.
    put(0, 0x87FF);
    
    // start at the second byte
    int ofs = 2;
    
    // write the report subtype (1) to byte 2
    report.data[ofs++] = 1;

    // write the native scale to bytes 3:4
    put(ofs, uint16_t(nativeScale));
    ofs += 2;
    
    // limit the jitter filter bounds to the native scale
    if (jitterLo < 0) 
        jitterLo = 0;
    else if (jitterLo > nativeScale) 
        jitterLo = nativeScale;
    if (jitterHi < 0)
        jitterHi = 0;
    else if (jitterHi > nativeScale)
        jitterHi = nativeScale;
    
    // write the jitter filter window bounds to 5:6 and 7:8
    put(ofs, uint16_t(jitterLo));
    ofs += 2;
    put(ofs, uint16_t(jitterHi));
    ofs += 2;
    
    // add the raw position
    put(ofs, uint16_t(rawPos));
    ofs += 2;
    
    // add the auto-exposure time
    put(ofs, uint16_t(axcTime));
    ofs += 2;
    
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

bool USBJoystick::sendPlungerStatusBarcode(
        int nbits, int codetype, int startOfs, int pixPerBit, int raw, int mask)
{
    HID_REPORT report;
    memset(report.data, 0, sizeof(report.data));
    
    // Set the special status bits to indicate it's an extended
    // status report for barcode sensors
    put(0, 0x87FF);
    
    // start at the second byte
    int ofs = 2;
    
    // write the report subtype (2) to byte 2
    report.data[ofs++] = 2;

    // write the bit count and code type
    report.data[ofs++] = nbits;
    report.data[ofs++] = codetype;
    
    // write the bar code starting pixel offset
    put(ofs, uint16_t(startOfs));
    ofs += 2;
    
    // write the pixel width per bit
    report.data[ofs++] = pixPerBit;
    
    // write the raw bar code and success bit mask
    put(ofs, uint16_t(raw));
    ofs += 2;
    put(ofs, uint16_t(mask));
    ofs += 2;
    
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

bool USBJoystick::sendPlungerStatusQuadrature(int chA, int chB)
{
    HID_REPORT report;
    memset(report.data, 0, sizeof(report.data));
    
    // set the status bits to indicate that it's an extended
    // status report for quadrature sensors
    put(0, 0x87FF);
    int ofs = 2;
    
    // write the report subtype (3)
    report.data[ofs++] = 3;
    
    // write the channel "A" and channel "B" values
    report.data[ofs++] = static_cast<uint8_t>(chA);
    report.data[ofs++] = static_cast<uint8_t>(chB);
    
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

bool USBJoystick::sendPlungerStatusVCNL4010(int filteredProxCount, int rawProxCount)
{
    HID_REPORT report;
    memset(report.data, 0, sizeof(report.data));
    
    // set the status bits to indicate that it's an extended
    // status report for quadrature sensors
    put(0, 0x87FF);
    int ofs = 2;
    
    // write the report subtype (4)
    report.data[ofs++] = 4;
    
    // write the filtered and raw proximity count from the sensor
    put(ofs, static_cast<uint16_t>(filteredProxCount));
    put(ofs + 2, static_cast<uint16_t>(rawProxCount));
    
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}


bool USBJoystick::sendPlungerPix(int &idx, int npix, const uint8_t *pix)
{
    HID_REPORT report;
    
    // Set the special status bits to indicate it's an exposure report.
    // The high 5 bits of the status word are set to 10000, and the
    // low 11 bits are the current pixel index.
    uint16_t s = idx | 0x8000;
    put(0, s);
    
    // start at the second byte
    int ofs = 2;
    
    // now fill out the remaining bytes with exposure values
    report.length = reportLen;
    for ( ; ofs < report.length ; ++ofs)
        report.data[ofs] = (idx < npix ? pix[idx++] : 0);
    
    // send the report
    return sendTO(&report, 100);
}

bool USBJoystick::reportID(int index)
{
    HID_REPORT report;

    // initially fill the report with zeros
    memset(report.data, 0, sizeof(report.data));
    
    // Set the special status bits to indicate that it's an ID report
    uint16_t s = 0x9000;
    put(0, s);
    
    // add the requested ID index
    report.data[2] = (uint8_t)index;
    
    // figure out which ID we're reporting
    switch (index)
    {
    case 1:
        // KL25Z CPU ID
        putbe(3, SIM->UIDMH);
        putlbe(5, SIM->UIDML);
        putlbe(9, SIM->UIDL);
        break;
        
    case 2:
        // OpenSDA ID.  Copy the low-order 80 bits of the OpenSDA ID.
        // (The stored value is 128 bits = 16 bytes; we only want the last
        // 80 bits = 10 bytes.  So skip ahead 16 and back up 10 to get
        // the starting point.)
        extern const char *getOpenSDAID();
        memcpy(&report.data[3], getOpenSDAID() + 16 - 10, 10);
        break;
    }
    
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

bool USBJoystick::reportBuildInfo(const char *date)
{
    HID_REPORT report;

    // initially fill the report with zeros
    memset(report.data, 0, sizeof(report.data));
    
    // Set the special status bits to indicate that it's a build
    // info report
    uint16_t s = 0xA000;
    put(0, s);
    
    // Parse the date.  This is given in the standard __DATE__ " " __TIME
    // macro format, "Mon dd yyyy hh:mm:ss" (e.g., "Feb 16 2016 12:15:06").
    static const char mon[][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" 
    };
    long dd = (atol(date + 7) * 10000L) // YYYY0000
        + (atol(date + 4));             // 000000DD
    for (int i = 0 ; i < 12 ; ++i)
    {
        if (memcmp(mon[i], date, 3) == 0)
        {
            dd += (i+1)*100;         // 0000MM00
            break;
        }
    }
    
    // parse the time into a long formatted as decimal HHMMSS (e.g.,
    // "12:15:06" turns into 121506 decimal)
    long tt = (atol(date+12)*10000)
        + (atol(date+15)*100)
        + (atol(date+18));
    
    // store the build date and time
    putl(2, dd);
    putl(6, tt);
    
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

bool USBJoystick::reportConfigVar(const uint8_t *data)
{
    HID_REPORT report;

    // initially fill the report with zeros
    memset(report.data, 0, sizeof(report.data));
    
    // Set the special status bits to indicate that it's a config 
    // variable report
    uint16_t s = 0x9800;
    put(0, s);
    
    // Copy the variable data (7 bytes, starting with the variable ID)
    memcpy(report.data + 2, data, 7);
    
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

bool USBJoystick::reportConfig(
    int numOutputs, int unitNo, 
    int plungerZero, int plungerMax, int plungerRlsTime,
    bool configured, bool sbxpbx, bool newAccelFeatures, 
    bool flashStatusFeature, bool reportTimingFeatures,
    bool chimeLogicFeature, bool velocityFeatures, 
    size_t freeHeapBytes)
{
    HID_REPORT report;

    // initially fill the report with zeros
    memset(report.data, 0, sizeof(report.data));
    
    // Set the special status bits to indicate that it's a config report.
    uint16_t s = 0x8800;
    put(0, s);
    
    // write the number of configured outputs
    put(2, numOutputs);
    
    // write the unit number
    put(4, unitNo);
    
    // write the plunger zero and max values
    put(6, plungerZero);
    put(8, plungerMax);
    report.data[10] = uint8_t(plungerRlsTime);
    
    // write the status bits: 
    //  0x01  -> configuration loaded
    //  0x02  -> SBX/PBX protocol extensions supported
    //  0x04  -> new accelerometer features supported
    //  0x08  -> flash status feature supported
    //  0x10  -> joystick report timing features supported
    //  0x20  -> chime logic feature supported
    //  0x40  -> accelerometer velocity, plunger speed, X/Y/Z+RX/RY/RZ features supported, and adjustable PWM
    report.data[11] = 
        (configured ? 0x01 : 0x00)
        | (sbxpbx ? 0x02 : 0x00)
        | (newAccelFeatures ? 0x04 : 0x00)
        | (flashStatusFeature ? 0x08 : 0x00)
        | (reportTimingFeatures ? 0x10 : 0x00)
        | (chimeLogicFeature ? 0x20 : 0x00)
        | (velocityFeatures ? 0x40 : 0x00);
    
    // write the free heap space
    put(12, freeHeapBytes);

    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

// report physical button status
bool USBJoystick::reportButtonStatus(int numButtons, const uint8_t *state)
{
    // initially fill the report with zeros
    HID_REPORT report;
    memset(report.data, 0, sizeof(report.data));
    
    // set the special status bits to indicate that it's a buton report
    uint16_t s = 0xA100;
    put(0, s);
    
    // write the number of buttons
    report.data[2] = uint8_t(numButtons);
    
    // Write the buttons - these are packed into ceil(numButtons/8) bytes.
    size_t btnBytes = (numButtons+7)/8;
    if (btnBytes + 3 > reportLen) btnBytes = reportLen - 3;
    memcpy(&report.data[3], state, btnBytes);
    
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

// report raw IR timing codes (for learning mode)
bool USBJoystick::reportRawIR(int n, const uint16_t *data)
{
    // initially fill the report with zeros
    HID_REPORT report;
    memset(report.data, 0, sizeof(report.data));
    
    // set the special status bits to indicate that it's an IR report
    uint16_t s = 0xA200;
    put(0, s);
    
    // limit the number of items reported to the available space
    if (n > maxRawIR)
        n = maxRawIR;
    
    // write the number of codes
    report.data[2] = uint8_t(n);
    
    // write the codes
    for (int i = 0, ofs = 3 ; i < n ; ++i, ofs += 2)
        put(ofs, data[i]);
    
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

bool USBJoystick::reportRawBytes(const uint8_t *data, size_t len)
{
    HID_REPORT report;
    report.length = reportLen;
    memcpy(report.data, data, len < reportLen ? len : reportLen);
    return sendTO(&report, 100);
}

// report a decoded IR command
bool USBJoystick::reportIRCode(uint8_t pro, uint8_t flags, uint64_t code)
{
    // initially fill the report with zeros
    HID_REPORT report;
    memset(report.data, 0, sizeof(report.data));
    
    // set the special status bits to indicate that it's an IR report
    uint16_t s = 0xA200;
    put(0, s);
    
    // set the raw count to 0xFF to flag that it's a decoded command
    report.data[2] = 0xFF;
    
    // write the data
    report.data[3] = pro;
    report.data[4] = flags;
    put64(5, code);
        
    // send the report
    report.length = reportLen;
    return sendTO(&report, 100);
}

bool USBJoystick::move(int16_t x, int16_t y) 
{
     _x = x;
     _y = y;
     return update();
}

bool USBJoystick::setZ(int16_t z) 
{
    _z = z;
    return update();
}
 
bool USBJoystick::buttons(uint32_t buttons) 
{
     _buttonsLo = (uint16_t)(buttons & 0xffff);
     _buttonsHi = (uint16_t)((buttons >> 16) & 0xffff);
     return update();
}

bool USBJoystick::updateStatus(uint32_t status)
{
   HID_REPORT report;

   // Fill the report according to the Joystick Descriptor
   memset(report.data, 0, reportLen);
   put(0, status);
   report.length = reportLen;
 
   // send the report
   return sendTO(&report, 100);
}

void USBJoystick::_init() {
 
   _x = 0;                       
   _y = 0;     
   _z = 0;
   _vx = 0;
   _vy = 0;
   _vz = 0;
   _buttonsLo = 0x0000;
   _buttonsHi = 0x0000;
   _status = 0;
}
 
 
// --------------------------------------------------------------------------
//
// USB HID Report Descriptor - Joystick
//
static const uint8_t reportDescriptorJS[] = 
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
        USAGE(1), 0x33,             // RX axis ("X rotation")
        USAGE(1), 0x34,             // RY axis
        USAGE(1), 0x35,             // RZ axis
        USAGE(1), 0x36,             // Slider
        LOGICAL_MINIMUM(2), 0x00,0xF0,   // each value ranges -4096
        LOGICAL_MAXIMUM(2), 0x00,0x10,   // ...to +4096
        REPORT_SIZE(1), 0x10,       // 16 bits per report
        REPORT_COUNT(1), 0x07,      // 6 reports (X, Y, Z, RX, RY, RZ, Slider1)
        INPUT(1), 0x02,             // Data, Variable, Absolute
         
        // output report (host to device)
        REPORT_SIZE(1), 0x08,       // 8 bits per report
        REPORT_COUNT(1), 0x08,      // output report count - 8-byte LedWiz format
        0x09, 0x01,                 // usage
        0x91, 0x01,                 // Output (array)

    END_COLLECTION(0)
};

// 
// USB HID Report Descriptor - Keyboard/Media Control
//
static const uint8_t reportDescriptorKB[] = 
{
    USAGE_PAGE(1), 0x01,                    // Generic Desktop
    USAGE(1), 0x06,                         // Keyboard
    COLLECTION(1), 0x01,                    // Application
        REPORT_ID(1), REPORT_ID_KB,

        USAGE_PAGE(1), 0x07,                    // Key Codes
        USAGE_MINIMUM(1), 0xE0,
        USAGE_MAXIMUM(1), 0xE7,
        LOGICAL_MINIMUM(1), 0x00,
        LOGICAL_MAXIMUM(1), 0x01,
        REPORT_SIZE(1), 0x01,
        REPORT_COUNT(1), 0x08,
        INPUT(1), 0x02,                         // Data, Variable, Absolute
        REPORT_COUNT(1), 0x01,
        REPORT_SIZE(1), 0x08,
        INPUT(1), 0x01,                         // Constant

        REPORT_COUNT(1), 0x05,
        REPORT_SIZE(1), 0x01,
        USAGE_PAGE(1), 0x08,                    // LEDs
        USAGE_MINIMUM(1), 0x01,
        USAGE_MAXIMUM(1), 0x05,
        OUTPUT(1), 0x02,                        // Data, Variable, Absolute
        REPORT_COUNT(1), 0x01,
        REPORT_SIZE(1), 0x03,
        OUTPUT(1), 0x01,                        // Constant

        REPORT_COUNT(1), 0x06,
        REPORT_SIZE(1), 0x08,
        LOGICAL_MINIMUM(1), 0x00,
        LOGICAL_MAXIMUM(1), 0xA4,
        USAGE_PAGE(1), 0x07,                    // Key Codes
        USAGE_MINIMUM(1), 0x00,
        USAGE_MAXIMUM(1), 0xA4,
        INPUT(1), 0x00,                         // Data, Array
    END_COLLECTION(0),

    // Media Control
    USAGE_PAGE(1), 0x0C,
    USAGE(1), 0x01,
    COLLECTION(1), 0x01,
        REPORT_ID(1), REPORT_ID_MEDIA,
        USAGE_PAGE(1), 0x0C,
        LOGICAL_MINIMUM(1), 0x00,
        LOGICAL_MAXIMUM(1), 0x01,
        REPORT_SIZE(1), 0x01,
        REPORT_COUNT(1), 0x07,
        USAGE(1), 0xE2,             // Mute -> 0x01
        USAGE(1), 0xE9,             // Volume Up -> 0x02
        USAGE(1), 0xEA,             // Volume Down -> 0x04
        USAGE(1), 0xB5,             // Next Track -> 0x08
        USAGE(1), 0xB6,             // Previous Track -> 0x10
        USAGE(1), 0xB7,             // Stop -> 0x20
        USAGE(1), 0xCD,             // Play / Pause -> 0x40
        INPUT(1), 0x02,             // Input (Data, Variable, Absolute) -> 0x80
        REPORT_COUNT(1), 0x01,
        INPUT(1), 0x01,
    END_COLLECTION(0),
};

// 
// USB HID Report Descriptor - LedWiz only, with no joystick or keyboard
// input reporting
//
static const uint8_t reportDescriptorLW[] = 
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
        REPORT_COUNT(1), USBJoystick::reportLen, // standard report length (same as if we were in joystick mode)
        INPUT(1), 0x02,             // Data, Variable, Absolute

        // output report (host to device)
        REPORT_SIZE(1), 0x08,       // 8 bits per report
        REPORT_COUNT(1), 0x08,      // output report count (LEDWiz messages)
        0x09, 0x01,                 // usage
        0x91, 0x01,                 // Output (array)

    END_COLLECTION(0)
};


const uint8_t *USBJoystick::reportDesc(int idx, uint16_t &len) 
{    
    switch (idx)
    {
    case 0:
        // If the joystick is enabled, this is the joystick.
        // Otherwise, it's the plain LedWiz control interface.
        if (enableJoystick)
        {
            len = sizeof(reportDescriptorJS);
            return reportDescriptorJS;
        }
        else
        {
            len = sizeof(reportDescriptorLW);
            return reportDescriptorLW;
        }
        
    case 1:
        // This is the keyboard, if enabled.
        if (useKB)
        {
            len = sizeof(reportDescriptorKB);
            return reportDescriptorKB;
        }
        else
        {
            len = 0;
            return 0;
        }
        
    default:
        // Unknown interface ID
        len = 0;
        return 0;
    }
} 
 
 const uint8_t *USBJoystick::stringImanufacturerDesc() {
    static const uint8_t stringImanufacturerDescriptor[] = {
        0x0E,                                            /* bLength */
        STRING_DESCRIPTOR,                               /* bDescriptorType 0x03 (String Descriptor) */
        'm',0,'j',0,'r',0,'n',0,'e',0,'t',0              /* bString iManufacturer - mjrnet */
    };
    return stringImanufacturerDescriptor;
}

const uint8_t *USBJoystick::stringIserialDesc() 
{
    // set up a buffer with space for the length prefix byte, descriptor type
    // byte, and serial number (as a wide-character string)
    const int numChars = 3 + 16 + 1 + 1 + 3;
    static uint8_t buf[2 + numChars*2];
    uint8_t *dst = buf;
    
    // store a placeholder for the length, followed by the descriptor type byte
    *dst++ = 0;
    *dst++ = STRING_DESCRIPTOR;

    // Create an ASCII version of our unique serial number string:
    //
    //   PSCxxxxxxxxxxxxxxxxivvv
    //
    // where:
    //   
    //   xxx... = decimal representation of low 64 bits of CPU ID (16 hex digits)
    //   i      = interface type:  first character is J if joystick is enabled,
    //             L = LedWiz/control interface only, no input
    //             J = Joystick + LedWiz
    //             K = Keyboard + LedWiz
    //             C = Joystick + Keyboard + LedWiz ("C" for combo)
    //   vvv    = version suffix
    //
    // The suffix for the interface type resolves a problem on some Windows systems
    // when switching between interface types.  Windows can cache device information
    // that includes the interface descriptors, and it won't recognize a change in
    // the interfaces once the information is cached, causing connection failures.
    // The cache key includes the device serial number, though, so this can be 
    // resolved by changing the serial number when the interface setup changes.
    char xbuf[numChars + 1];
    uint32_t x = SIM->UIDML;
    static char ifcCode[] = "LJKC";
    sprintf(xbuf, "PSC%08lX%08lX%c009",
        SIM->UIDML, 
        SIM->UIDL, 
        ifcCode[(enableJoystick ? 0x01 : 0x00) | (useKB ? 0x02 : 0x00)]);

    // copy the ascii bytes into the descriptor buffer, converting to unicode
    // 16-bit little-endian characters
    for (char *src = xbuf ; *src != '\0' && dst < buf + sizeof(buf) ; )
    {
        *dst++ = *src++;
        *dst++ = '\0';
    }
    
    // store the final length (in bytes) in the length prefix byte
    buf[0] = dst - buf;
    
    // return the buffer    
    return buf;
}

const uint8_t *USBJoystick::stringIproductDesc() {
    static const uint8_t stringIproductDescriptor[] = {
        0x28,                                                       /*bLength*/
        STRING_DESCRIPTOR,                                          /*bDescriptorType 0x03*/
        'P',0,'i',0,'n',0,'s',0,'c',0,'a',0,'p',0,'e',0,
        ' ',0,'C',0,'o',0,'n',0,'t',0,'r',0,'o',0,'l',0,
        'l',0,'e',0,'r',0                                           /*String iProduct */
    };
    return stringIproductDescriptor;
}

#define DEFAULT_CONFIGURATION (1)

const uint8_t *USBJoystick::configurationDesc() 
{
    int rptlen0 = reportDescLength(0);
    int rptlen1 = reportDescLength(1);
    if (useKB)
    {
        const int cfglenKB = 
            ((1 * CONFIGURATION_DESCRIPTOR_LENGTH)
             + (2 * INTERFACE_DESCRIPTOR_LENGTH)
             + (2 * HID_DESCRIPTOR_LENGTH)
             + (4 * ENDPOINT_DESCRIPTOR_LENGTH));
        static uint8_t configurationDescriptorWithKB[] = 
        {
            CONFIGURATION_DESCRIPTOR_LENGTH,// bLength
            CONFIGURATION_DESCRIPTOR,       // bDescriptorType
            LSB(cfglenKB),                  // wTotalLength (LSB)
            MSB(cfglenKB),                  // wTotalLength (MSB)
            0x02,                           // bNumInterfaces - TWO INTERFACES (JOYSTICK + KEYBOARD)
            DEFAULT_CONFIGURATION,          // bConfigurationValue
            0x00,                           // iConfiguration
            C_RESERVED | C_SELF_POWERED,    // bmAttributes
            C_POWER(0),                     // bMaxPower
        
            // ***** INTERFACE 0 - JOYSTICK/LEDWIZ ******
            INTERFACE_DESCRIPTOR_LENGTH,    // bLength
            INTERFACE_DESCRIPTOR,           // bDescriptorType
            0x00,                           // bInterfaceNumber
            0x00,                           // bAlternateSetting
            0x02,                           // bNumEndpoints
            HID_CLASS,                      // bInterfaceClass
            HID_SUBCLASS_NONE,              // bInterfaceSubClass
            HID_PROTOCOL_NONE,              // bInterfaceProtocol
            0x00,                           // iInterface
        
            HID_DESCRIPTOR_LENGTH,          // bLength
            HID_DESCRIPTOR,                 // bDescriptorType
            LSB(HID_VERSION_1_11),          // bcdHID (LSB)
            MSB(HID_VERSION_1_11),          // bcdHID (MSB)
            0x00,                           // bCountryCode
            0x01,                           // bNumDescriptors
            REPORT_DESCRIPTOR,              // bDescriptorType
            LSB(rptlen0),                   // wDescriptorLength (LSB)
            MSB(rptlen0),                   // wDescriptorLength (MSB)
        
            ENDPOINT_DESCRIPTOR_LENGTH,     // bLength
            ENDPOINT_DESCRIPTOR,            // bDescriptorType
            PHY_TO_DESC(EPINT_IN),          // bEndpointAddress - EPINT == EP1
            E_INTERRUPT,                    // bmAttributes
            LSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (LSB)
            MSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (MSB)
            1,                              // bInterval (milliseconds)
        
            ENDPOINT_DESCRIPTOR_LENGTH,     // bLength
            ENDPOINT_DESCRIPTOR,            // bDescriptorType
            PHY_TO_DESC(EPINT_OUT),         // bEndpointAddress - EPINT == EP1
            E_INTERRUPT,                    // bmAttributes
            LSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (LSB)
            MSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (MSB)
            1,                              // bInterval (milliseconds)
            
            // ****** INTERFACE 1 - KEYBOARD ******
            INTERFACE_DESCRIPTOR_LENGTH,    // bLength
            INTERFACE_DESCRIPTOR,           // bDescriptorType
            0x01,                           // bInterfaceNumber
            0x00,                           // bAlternateSetting
            0x02,                           // bNumEndpoints
            HID_CLASS,                      // bInterfaceClass
            HID_SUBCLASS_BOOT,              // bInterfaceSubClass
            HID_PROTOCOL_KB,                // bInterfaceProtocol
            0x00,                           // iInterface
        
            HID_DESCRIPTOR_LENGTH,          // bLength
            HID_DESCRIPTOR,                 // bDescriptorType
            LSB(HID_VERSION_1_11),          // bcdHID (LSB)
            MSB(HID_VERSION_1_11),          // bcdHID (MSB)
            0x00,                           // bCountryCode
            0x01,                           // bNumDescriptors
            REPORT_DESCRIPTOR,              // bDescriptorType
            LSB(rptlen1),                   // wDescriptorLength (LSB)
            MSB(rptlen1),                   // wDescriptorLength (MSB)
        
            ENDPOINT_DESCRIPTOR_LENGTH,     // bLength
            ENDPOINT_DESCRIPTOR,            // bDescriptorType
            PHY_TO_DESC(EP4IN),             // bEndpointAddress
            E_INTERRUPT,                    // bmAttributes
            LSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (LSB)
            MSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (MSB)
            1,                              // bInterval (milliseconds)
        
            ENDPOINT_DESCRIPTOR_LENGTH,     // bLength
            ENDPOINT_DESCRIPTOR,            // bDescriptorType
            PHY_TO_DESC(EP4OUT),            // bEndpointAddress
            E_INTERRUPT,                    // bmAttributes
            LSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (LSB)
            MSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (MSB)
            1,                              // bInterval (milliseconds)

        };

        // Keyboard + joystick interfaces
        return configurationDescriptorWithKB;
    }
    else
    {
        // No keyboard - joystick interface only
        const int cfglenNoKB = 
            ((1 * CONFIGURATION_DESCRIPTOR_LENGTH)
              + (1 * INTERFACE_DESCRIPTOR_LENGTH)
              + (1 * HID_DESCRIPTOR_LENGTH)
              + (2 * ENDPOINT_DESCRIPTOR_LENGTH));
        static uint8_t configurationDescriptorNoKB[] = 
        {
            CONFIGURATION_DESCRIPTOR_LENGTH,// bLength
            CONFIGURATION_DESCRIPTOR,       // bDescriptorType
            LSB(cfglenNoKB),                // wTotalLength (LSB)
            MSB(cfglenNoKB),                // wTotalLength (MSB)
            0x01,                           // bNumInterfaces
            DEFAULT_CONFIGURATION,          // bConfigurationValue
            0x00,                           // iConfiguration
            C_RESERVED | C_SELF_POWERED,    // bmAttributes
            C_POWER(0),                     // bMaxPower
        
            INTERFACE_DESCRIPTOR_LENGTH,    // bLength
            INTERFACE_DESCRIPTOR,           // bDescriptorType
            0x00,                           // bInterfaceNumber
            0x00,                           // bAlternateSetting
            0x02,                           // bNumEndpoints
            HID_CLASS,                      // bInterfaceClass
            HID_SUBCLASS_NONE,              // bInterfaceSubClass
            HID_PROTOCOL_NONE,              // bInterfaceProtocol (keyboard)
            0x00,                           // iInterface
        
            HID_DESCRIPTOR_LENGTH,          // bLength
            HID_DESCRIPTOR,                 // bDescriptorType
            LSB(HID_VERSION_1_11),          // bcdHID (LSB)
            MSB(HID_VERSION_1_11),          // bcdHID (MSB)
            0x00,                           // bCountryCode
            0x01,                           // bNumDescriptors
            REPORT_DESCRIPTOR,              // bDescriptorType
            (uint8_t)(LSB(rptlen0)),        // wDescriptorLength (LSB)
            (uint8_t)(MSB(rptlen0)),        // wDescriptorLength (MSB)
        
            ENDPOINT_DESCRIPTOR_LENGTH,     // bLength
            ENDPOINT_DESCRIPTOR,            // bDescriptorType
            PHY_TO_DESC(EPINT_IN),          // bEndpointAddress
            E_INTERRUPT,                    // bmAttributes
            LSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (LSB)
            MSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (MSB)
            1,                              // bInterval (milliseconds)
        
            ENDPOINT_DESCRIPTOR_LENGTH,     // bLength
            ENDPOINT_DESCRIPTOR,            // bDescriptorType
            PHY_TO_DESC(EPINT_OUT),         // bEndpointAddress
            E_INTERRUPT,                    // bmAttributes
            LSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (LSB)
            MSB(MAX_PACKET_SIZE_EPINT),     // wMaxPacketSize (MSB)
            1                               // bInterval (milliseconds)
        };

        return configurationDescriptorNoKB;
    }
}

// Set the configuration.  We need to set up the endpoints for
// our active interfaces.
bool USBJoystick::USBCallback_setConfiguration(uint8_t configuration) 
{
    // we only have one valid configuration
    if (configuration != DEFAULT_CONFIGURATION)
        return false;
        
    // Configure endpoint 1 - we use this in all cases, for either
    // the combined joystick/ledwiz interface or just the ledwiz interface
    addEndpoint(EPINT_IN, MAX_REPORT_JS_TX + 1);
    addEndpoint(EPINT_OUT, MAX_REPORT_JS_RX + 1);
    readStart(EPINT_OUT, MAX_REPORT_JS_TX + 1);
    
    // if the keyboard is enabled, configure endpoint 4 for the kb interface
    if (useKB)
    {
        addEndpoint(EP4IN, MAX_REPORT_KB_TX + 1);
        addEndpoint(EP4OUT, MAX_REPORT_KB_RX + 1);
        readStart(EP4OUT, MAX_REPORT_KB_TX + 1);
    }

    // success
    return true;
}

// Handle incoming messages on the joystick/LedWiz interface = endpoint 1.
// This interface receives LedWiz protocol commands and commands using our
// custom LedWiz protocol extensions.
//
// We simply queue the messages in our circular buffer for processing in 
// the main loop.  The circular buffer object is designed for safe access
// from the interrupt handler using the rule that only the interrupt 
// handler can change the write pointer, and only the regular code can
// change the read pointer.
bool USBJoystick::EP1_OUT_callback()
{
    // Read this message
    union {
        LedWizMsg msg;
        uint8_t buf[MAX_HID_REPORT_SIZE];
    } buf;
    uint32_t bytesRead = 0;
    USBDevice::readEP(EP1OUT, buf.buf, &bytesRead, MAX_HID_REPORT_SIZE);
    
    // if it's the right length, queue it to our circular buffer
    if (bytesRead == 8)
        lwbuf.write(buf.msg);

    // start the next read
    return readStart(EP1OUT, MAX_HID_REPORT_SIZE);
}

// Handle incoming messages on the keyboard interface = endpoint 4.
// The host uses this to send updates for the keyboard indicator LEDs
// (caps lock, num lock, etc).  We don't do anything with these, but
// we have to read them to keep the pipe open.
bool USBJoystick::EP4_OUT_callback() 
{
    // read this message
    uint32_t bytesRead = 0;
    uint8_t led[MAX_HID_REPORT_SIZE];
    USBDevice::readEP(EP4OUT, led, &bytesRead, MAX_HID_REPORT_SIZE);

    // start the next read
    return readStart(EP4OUT, MAX_HID_REPORT_SIZE);
}

