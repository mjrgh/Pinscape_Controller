/* USBJoystick.h */
/* USB device example: Joystick*/
/* Copyright (c) 2011 ARM Limited. All rights reserved. */
/* Modified Mouse code for Joystick - WH 2012 */
 
#ifndef USBJOYSTICK_H
#define USBJOYSTICK_H
 
#include "USBHID.h"
#include "circbuf.h"

// Bufferd incoming LedWiz message structure
struct LedWizMsg
{
    uint8_t data[8];
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
 
 
class USBJoystick: public USBHID 
{
public:
    // Length of our joystick reports.  Important: This must be kept in sync 
    // with the actual joystick report format sent in update().
    static const int reportLen = 14;


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
     * Write the plunger status report header.
     *
     * Note that we automatically add the "calibration mode" bit to the flags,
     * so the caller doesn't have to include this.  The caller only has to
     * include the sensor-specific flags.
     *
     * @param npix number of pixels in the sensor (0 for non-imaging sensors)
     * @param pos the decoded plunger position, or -1 if none detected
     * @param flags (see USBProtocol.h, message type 2A, "byte 7" bit flags)
     * @param avgScanTime average sensor scan time in microseconds
     * @param processingTime time in microseconds to process the current frame
     */
    bool sendPlungerStatus(int npix, int flags, int dir, 
        uint32_t avgScanTime, uint32_t processingTime);
        
    /**
      * Send a secondary plunger status report header.
      *
      * @param nativeScale upper bound of the sensor's native reading scale
      * @param jitterLo low end of jitter filter window (in sensor native scale units)
      * @param jitterHi high end of jitter filter window
      * @param rawPos raw position reading, before applying jitter filter
      * @param axcTime auto-exposure time in microseconds
      */
    bool sendPlungerStatus2(
        int nativeScale, int jitterLo, int jitterHi, int rawPos, int axcTime);
        
    /**
     * Send a barcode plunger status report header.
     *
     * @param nbits number of bits in bar code
     * @param codetype bar code type (1=Gray code/Manchester bit coding)
     * @param pixofs pixel offset of first bit
     * @param raw raw bar code bits
     * @param mask mask of successfully read bar code bits
     */
    bool sendPlungerStatusBarcode(
        int nbits, int codetype, int startOfs, int pixPerBit, int raw, int mask);
    
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
     * @param sbxpbx true if this firmware version supports SBX/PBX protocol extensions
     * @param newAccelFeatures true if this firmware version supports the new accelerometer
     *        features (adjustable dynamic range, adjustable auto-centering mode time,
     *        auto-centering mode on/off)
     * @param flashStatusFeature true if this firmware versions upports the flash write
     *        success flags in the status bits
     * @param freeHeapBytes number of free bytes in the malloc heap
     */
    bool reportConfig(int numOutputs, int unitNo, 
        int plungerZero, int plungerMax, int plunterRlsTime, 
        bool configured, bool sbxpbx, bool newAccelFeatures, bool flashStatusFeature,
        size_t freeHeapBytes);
        
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
     * Write an IR raw sensor input report.  This reports a set of raw
     * timing reports for input read from the IR sensor, for learning
     * remote purposes.
     *
     * @param n number of items to report, up to maxRawIR
     * @param data items to report; each is a timing reading, in 2us
     *        increments, with the low bit in each report set to 0 for
     *        a "space" (IR off) or 1 for a "mark" (IR on)
     */
    bool reportRawIR(int n, const uint16_t *data);
    
    /**
     * Maximum number of raw IR readings that can be sent in one report
     * via reportRawIR().
     */
    static const int maxRawIR = (reportLen - 3)/2;
    
    /**
     * Write an IR input report.  This reports a decoded command read in
     * learning mode to the host.
     *
     * @param pro protocol ID (see IRProtocolID.h)
     * @param flags bit flags: 0x02 = protocol uses dittos
     * @param code decoded command code
     */
    bool reportIRCode(uint8_t pro, uint8_t flags, uint64_t code);

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
    CircBuf<LedWizMsg, 16> lwbuf;
     
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
