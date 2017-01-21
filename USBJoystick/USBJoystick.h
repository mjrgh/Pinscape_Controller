/* USBJoystick.h */
/* USB device example: Joystick*/
/* Copyright (c) 2011 ARM Limited. All rights reserved. */
/* Modified Mouse code for Joystick - WH 2012 */
 
#ifndef USBJOYSTICK_H
#define USBJOYSTICK_H
 
#include "USBHID.h"

// Bufferd incoming LedWiz message structure
struct LedWizMsg
{
    uint8_t data[8];
};

// Circular buffer for incoming reports.  We write reports in the IRQ
// handler, and we read reports in the main loop in normal application
// (non-IRQ) context.  
// 
// The design is organically safe for IRQ threading; there are no critical 
// sections.  The IRQ context has exclusive access to the write pointer, 
// and the application context has exclusive access to the read pointer, 
// so there are no test-and-set or read-and-modify race conditions.
template<class T, int cnt> class CircBuf
{
public:
    CircBuf() 
    {
        iRead = iWrite = 0;
    }

    // Read an item from the buffer.  Returns true if an item was available,
    // false if the buffer was empty.  (Called in the main loop, in application
    // context.)
    bool read(T &result) 
    {
        if (iRead != iWrite)
        {
            //{uint8_t *d = buf[iRead].data; printf("circ read [%02x %02x %02x %02x %02x %02x %02x %02x]\r\n", d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7]);}
            memcpy(&result, &buf[iRead], sizeof(T));
            iRead = advance(iRead);
            return true;
        }
        else
            return false;
    }
    
    // Write an item to the buffer.  (Called in the IRQ handler, in interrupt
    // context.)
    bool write(const T &item)
    {
        int nxt = advance(iWrite);
        if (nxt != iRead)
        {
            memcpy(&buf[iWrite], &item, sizeof(T));
            iWrite = nxt;
            return true;
        }
        else
            return false;
    }

private:
    int advance(int i)
    {
        ++i;
        return i < cnt ? i : 0;
    } 
    
    int iRead;
    int iWrite;
    T buf[cnt];
};

// interface IDs
const uint8_t IFC_ID_JS = 0;        // joystick + LedWiz interface
const uint8_t IFC_ID_KB = 1;        // keyboard interface

// keyboard interface report IDs 
const uint8_t REPORT_ID_KB = 1;
const uint8_t REPORT_ID_MEDIA = 2;

/* Common usage */
enum JOY_BUTTON {
     JOY_B0 = 0x0001,
     JOY_B1 = 0x0002,
     JOY_B2 = 0x0004,
     JOY_B3 = 0x0008,
     JOY_B4 = 0x0010,
     JOY_B5 = 0x0020,
     JOY_B6 = 0x0040,
     JOY_B7 = 0x0080,
     JOY_B8 = 0x0100,
     JOY_B9 = 0x0200,
     JOY_B10 = 0x0400,
     JOY_B11 = 0x0800,
     JOY_B12 = 0x1000,
     JOY_B13 = 0x2000,
     JOY_B14 = 0x4000,
     JOY_B15 = 0x8000
};
 
/* X, Y and T limits */
/* These values do not directly map to screen pixels */
/* Zero may be interpreted as meaning 'no movement' */
#define JX_MIN_ABS    (-127)     /*!< The maximum value that we can move to the left on the x-axis */
#define JY_MIN_ABS    (-127)     /*!< The maximum value that we can move up on the y-axis */
#define JZ_MIN_ABS    (-127)     /*!< The minimum value for the Z axis */
#define JX_MAX_ABS    (127)      /*!< The maximum value that we can move to the right on the x-axis */
#define JY_MAX_ABS    (127)      /*!< The maximum value that we can move down on the y-axis */
#define JZ_MAX_ABS    (127)      /*!< The maximum value for the Z axis */
 
/**
 *
 * USBJoystick example
 * @code
 * #include "mbed.h"
 * #include "USBJoystick.h"
 *
 * USBJoystick joystick;
 *
 * int main(void)
 * {
 *   while (1)
 *   {
 *      joystick.move(20, 0);
 *      wait(0.5);
 *   }
 * }
 *
 * @endcode
 *
 *
 * @code
 * #include "mbed.h"
 * #include "USBJoystick.h"
 * #include <math.h>
 *
 * USBJoystick joystick;
 *
 * int main(void)
 * {   
 *   while (1) {
 *       // Basic Joystick
 *       joystick.update(tx, y, z, buttonBits);
 *       wait(0.001);
 *   }
 * }
 * @endcode
 */
 
 
class USBJoystick: public USBHID {
   public:
 
        /**
         *   Constructor
         *
         * @param vendor_id Your vendor_id (default: 0x1234)
         * @param product_id Your product_id (default: 0x0002)
         * @param product_release Your product_release (default: 0x0001)
         */
         USBJoystick(uint16_t vendor_id, uint16_t product_id, uint16_t product_release, 
             int waitForConnect, bool enableJoystick, bool useKB)
             : USBHID(16, 64, vendor_id, product_id, product_release, false)
         { 
             _init();
             this->useKB = useKB;
             this->enableJoystick = enableJoystick;
             connect(waitForConnect);
         };

         /* read a report from the LedWiz buffer */
         bool readLedWizMsg(LedWizMsg &msg)
         {
             return lwbuf.read(msg);
         }
         
         /* get the idle time settings, in milliseconds */
         uint32_t getKbIdle() const { return kbIdleTime * 4UL; }
         uint32_t getMediaIdle() const { return mediaIdleTime * 4UL; }
         

         /**
          * Send a keyboard report.  The argument gives the key state, in the standard
          * 6KRO USB keyboard report format: byte 0 is the modifier key bit mask, byte 1
          * is reserved (must be 0), and bytes 2-6 are the currently pressed keys, as
          * USB key codes.
          */
         bool kbUpdate(uint8_t data[8]);
         
         /**
          * Send a media key update.  The argument gives the bit mask of media keys
          * currently pressed.  See the HID report descriptor for the order of bits.
          */
         bool mediaUpdate(uint8_t data);
         
         /**
         * Update the joystick status
         *
         * @param x x-axis position
         * @param y y-axis position
         * @param z z-axis position
         * @param buttons buttons state, as a bit mask (combination with '|' of JOY_Bn values)
         * @returns true if there is no error, false otherwise
         */
         bool update(int16_t x, int16_t y, int16_t z, uint32_t buttons, uint16_t status);
         
         /**
         * Update just the status
         */
         bool updateStatus(uint32_t stat);
         
         /**
         * Write the plunger status report.
         *
         * @param npix number of pixels in the sensor (0 for non-imaging sensors)
         * @param edgePos the pixel position of the detected edge in this image, or -1 if none detected
         * @param dir sensor orientation (1 = standard, -1 = reversed, 0 = unknown)
         * @param avgScanTime average sensor scan time in microseconds
         * @param processingTime time in microseconds to process the current frame
         */
         bool sendPlungerStatus(int npix, int edgePos, int dir, uint32_t avgScanTime, uint32_t processingTime);
         
         /**
         * Write an exposure report.  We'll fill out a report with as many pixels as
         * will fit in the packet, send the report, and update the index to the next
         * pixel to send.  The caller should call this repeatedly to send reports for
         * all pixels.
         *
         * @param idx current index in pixel array, updated to point to next pixel to send
         * @param npix number of pixels in the overall array
         * @param pix pixel array
         */
         bool sendPlungerPix(int &idx, int npix, const uint8_t *pix);
         
         /**
         * Write a configuration report.
         *
         * @param numOutputs the number of configured output channels
         * @param unitNo the device unit number
         * @param plungerZero plunger zero calibration point
         * @param plungerMax plunger max calibration point
         * @param plungerRlsTime measured plunger release time, in milliseconds
         * @param configured true if a configuration has been saved to flash from the host
         * @param freeHeapBytes number of free bytes in the malloc heap
         */
         bool reportConfig(int numOutputs, int unitNo, 
            int plungerZero, int plungerMax, int plunterRlsTime, 
            bool configured, size_t freeHeapBytes);
            
         /**
         * Write a configuration variable query report.
         *
         * @param data the 7-byte data variable buffer, starting with the variable ID byte
         */
         bool reportConfigVar(const uint8_t *data);
         
         /**
         * Write a device ID report.
         */
         bool reportID(int index);
         
         /**
         * Write a build data report
         *
         * @param date build date plus time, in __DATE__ " " __TIME__ macro format ("Mon dd, yyyy hh:mm:ss")
         */
         bool reportBuildInfo(const char *date);
         
         /**
          * Write a physical button status report.
          *
          * @param numButtons the number of buttons
          * @param state the button states, 1 bit per button, 8 buttons per byte,
          *        starting with button 0 in the low-order bit (0x01) of the 
          *        first byte
          */
         bool reportButtonStatus(int numButtons, const uint8_t *state);
 
         /**
         * Send a joystick report to the host
         *
         * @returns true if there is no error, false otherwise
         */
         bool update();
         
         /**
         * Move the cursor to (x, y)
         *
         * @param x x-axis position
         * @param y y-axis position
         * @returns true if there is no error, false otherwise
         */
         bool move(int16_t x, int16_t y);
         
         /**
         * Set the z position
         *
         * @param z z-axis osition
         */
         bool setZ(int16_t z);
         
         /**
         * Press one or several buttons
         *
         * @param buttons button state, as a bitwise combination of JOY_Bn values
         * @returns true if there is no error, false otherwise
         */
         bool buttons(uint32_t buttons);

         /* USB descriptor overrides */
         virtual const uint8_t *configurationDesc();
         virtual const uint8_t *reportDesc(int idx, uint16_t &len);
 
         /* USB descriptor string overrides */
         virtual const uint8_t *stringImanufacturerDesc();
         virtual const uint8_t *stringIserialDesc();
         virtual const uint8_t *stringIproductDesc();
         
         /* set/get idle time */
         virtual void setIdleTime(int ifc, int rptid, int t)
         {
             // Remember the new value if operating on the keyboard.  Remember
             // separate keyboard and media control idle times, in case the
             // host wants separate report rates.
             if (ifc == IFC_ID_KB)
             {
                if (rptid == REPORT_ID_KB)
                    kbIdleTime = t;
                else if (rptid == REPORT_ID_MEDIA)
                    mediaIdleTime = t;
            }
         }
         virtual uint8_t getIdleTime(int ifc, int rptid)
         {
             // Return the kb idle time if the kb interface is the one requested.
             if (ifc == IFC_ID_KB)
             {
                 if (rptid == REPORT_ID_KB)
                    return kbIdleTime;
                 if (rptid == REPORT_ID_MEDIA)
                    return mediaIdleTime;
             }
             
             // we don't use idle times for other interfaces or report types
             return 0;
         }
         
         /* callback overrides */
         virtual bool USBCallback_setConfiguration(uint8_t configuration);
         virtual bool USBCallback_setInterface(uint16_t interface, uint8_t alternate)
            { return interface == 0 || interface == 1; }
            
         virtual bool EP1_OUT_callback();
         virtual bool EP4_OUT_callback();
         
     private:

         // Incoming LedWiz message buffer.  Each LedWiz message is exactly 8 bytes.
         CircBuf<LedWizMsg, 64> lwbuf;
         
         bool enableJoystick;
         bool useKB;
         uint8_t kbIdleTime;
         uint8_t mediaIdleTime;
         int16_t _x;                       
         int16_t _y;     
         int16_t _z;
         uint16_t _buttonsLo;
         uint16_t _buttonsHi;
         uint16_t _status;

         void _init();                 
};
 
#endif
