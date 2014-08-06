/* Copyright 2014 M J Roberts, MIT License
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

//
// Pinscape Controller
//
// "Pinscape" is the name of my custom-built virtual pinball cabinet.  I wrote this
// software to perform a number of tasks that I needed for my cabinet.  It runs on a
// Freescale KL25Z microcontroller, which is a small and inexpensive device that
// attaches to the host PC via USB and can interface with numerous types of external
// hardware.
//
// I designed the software and hardware in this project especially for Pinscape, but 
// it uses standard interfaces in Windows and Visual Pinball, so it should be
// readily usable in anyone else's VP-based cabinet.  I've tried to document the
// hardware in enough detail for anyone else to duplicate the entire project, and
// the full software is open source.
//
// The device appears to the host computer as a USB joystick.  This works with the
// standard Windows joystick device drivers, so there's no need to install any
// software on the PC - Windows should recognize it as a joystick when you plug
// it in and shouldn't ask you to install anything.  If you bring up the control
// panel for USB Game Controllers, this device will appear as "Pinscape Controller".
// *Don't* do any calibration with the Windows control panel or third-part 
// calibration tools.  The device calibrates itself automatically for the
// accelerometer data, and has its own special calibration procedure for the
// plunger (see below).
//
// The controller provides the following functions.  It should be possible to use
// any subet of the features without using all of them.  External hardware for any
// particular function can simply be omitted if that feature isn't needed.
//
//  - Nudge sensing via the KL25Z's on-board accelerometer.  Nudge accelerations are
//    processed into a physics model of a rolling ball, and changes to the ball's
//    motion are sent to the host computer via the joystick interface.  This is designed
//    especially to work with Visuall Pinball's nudge handling to produce realistic 
//    on-screen results in VP.  By doing some physics modeling right on the device, 
//    rather than sending raw accelerometer data to VP, we can produce better results
//    using our awareness of the real physical parameters of a pinball cabinet.
//    VP's nudge handling has to be more generic, so it can't make the same sorts
//    of assumptions that we can about the dynamics of a real cabinet.
//
//    The nudge data reports are compatible with the built-in Windows USB joystick 
//    drivers and with VP's own joystick input scheme, so the nudge sensing is almost 
//    plug-and-play.  There are no Windiows drivers to install, and the only VP work 
//    needed is to customize a few global preference settings.
//
//  - Plunger position sensing via an attached TAOS TSL 1410R CCD linear array sensor.  
//    The sensor must be wired to a particular set of I/O ports on the KL25Z, and must 
//    be positioned adjacent to the plunger with proper lighting.  The physical and
//    electronic installation details are desribed in the project documentation.  We read 
//    the CCD to determine how far back the plunger is pulled, and report this to Visual 
//    Pinball via the joystick interface.  As with the nudge data, this is all nearly
//    plug-and-play, in that it works with the default Windows USB drivers and works 
//    with the existing VP handling for analog plunger input.  A few VP settings are
//    needed to tell VP to allow the plunger.
//
//    For best results, the plunger sensor should be calibrated.  The calibration
//    is stored in non-volatile memory on board the KL25Z, so it's only necessary
//    to do the calibration once, when you first install everything.  (You might
//    also want to re-calibrate if you physically remove and reinstall the CCD 
//    sensor or the mechanical plunger, since their alignment might change slightly 
//    when you put everything back together.)  To calibrate, you have to attach a
//    momentary switch (e.g., a push-button switch) between one of the KL25Z ground
//    pins (e.g., jumper J9 pin 12) and PTE29 (J10 pin 9).  Press and hold the
//    button for about two seconds - the LED on the KL25Z wlil flash blue while
//    you hold the button, and will turn solid blue when you've held it down long
//    enough to enter calibration mode.  This mode will last about 15 seconds.
//    Simply pull the plunger all the way back, hold it for a few moments, and
//    gradually return it to the starting position.  *Don't* release it - we want
//    to measure the maximum retracted position and the rest position, but NOT
//    the maximum forward position when the outer barrel spring is compressed.
//    After about 15 seconds, the device will save the new calibration settings
//    to its flash memory, and the LED will return to the regular "heartbeat" 
//    flashes.  If this is the first time you calibrated, you should observe the
//    color of the flashes change from yellow/green to blue/green to indicate
//    that the plunger has been calibrated.
//
//    Note that while Visual Pinball itself has good native support for analog 
//    plungers, most of the VP tables in circulation don't implement the necessary
//    scripting features to make this work properly.  Therefore, you'll have to do
//    a little scripting work for each table you download to add the required code
//    to that individual table.  The work has to be customized for each table, so
//    I haven't been able to automate this process, but I have tried to reduce it
//    to a relatively simple recipe that I've documented separately.
//
//  - In addition to the CCD sensor, a button should be attached (also described in 
//    the project documentation) to activate calibration mode for the plunger.  When 
//    calibration mode is activated, the software reads the plunger position for about 
//    10 seconds when to note the limits of travel, and uses these limits to ensure
//    accurate reports to VP that properly report the actual position of the physical
//    plunger.  The calibration is stored in non-volatile memory on the KL25Z, so it's
//    only necessary to calibrate once - the calibration will survive power cycling
//    and reboots of the PC.  It's only necessary to recalibrate if the CCD sensor or
//    the plunger are removed and reinstalled, since the relative alignment of the
//    parts could cahnge slightly when reinstalling.
//
//  - LedWiz emulation.  The KL25Z can appear to the PC as an LedWiz device, and will
//    accept and process LedWiz commands from the host.  The software can turn digital
//    output ports on and off, and can set varying PWM intensitiy levels on a subset
//    of ports.  (The KL25Z can only provide 6 PWM ports.  Intensity level settings on
//    other ports is ignored, so non-PWM ports can only be used for simple on/off
//    devices such as contactors and solenoids.)  The KL25Z can only supply 4mA on its
//    output ports, so external hardware is required to take advantage of the LedWiz
//    emulation.  Many different hardware designs are possible, but there's a simple
//    reference design in the documentation that uses a Darlington array IC to
//    increase the output from each port to 500mA (the same level as the LedWiz),
//    plus an extended design that adds an optocoupler and MOSFET to provide very
//    high power handling, up to about 45A or 150W, with voltages up to 100V.
//    That will handle just about any DC device directly (wtihout relays or other
//    amplifiers), and switches fast enough to support PWM devices.
//
//    The device can report any desired LedWiz unit number to the host, which makes
//    it possible to use the LedWiz emulation on a machine that also has one or more
//    actual LedWiz devices intalled.  The LedWiz design allows for up to 16 units
//    to be installed in one machine - each one is invidually addressable by its
//    distinct unit number.
//
//    The LedWiz emulation features are of course optional.  There's no need to 
//    build any of the external port hardware (or attach anything to the output 
//    ports at all) if the LedWiz features aren't needed.  Most people won't have
//    any use for the LedWiz features.  I built them mostly as a learning exercise,
//    but with a slight practical need for a handful of extra ports (I'm using the
//    cutting-edge 10-contactor setup, so my real LedWiz is full!).
//
// The on-board LED on the KL25Z flashes to indicate the current device status:
//
//    two short red flashes = the device is powered but hasn't successfully
//        connected to the host via USB (either it's not physically connected
//        to the USB port, or there was a problem with the software handshake
//        with the USB device driver on the computer)
//
//    short red flash = the host computer is in sleep/suspend mode
//
//    long red/green = the LedWiz unti number has been changed, so a reset
//        is needed.  You can simply unplug the device and plug it back in,
//        or presss and hold the reset button on the device for a few seconds.
//
//    long yellow/green = everything's working, but the plunger hasn't
//        been calibrated; follow the calibration procedure described above.
//        This flash mode won't appear if the CCD has been disabled.  Note
//        that the device can't tell whether a CCD is physically attached,
//        so you should use the config command to disable the CCD software 
//        features if you won't be attaching a CCD.
//
//    alternating blue/green = everything's working
//
// Software configuration: you can change option settings by sending special
// USB commands from the PC.  I've provided a Windows program for this purpose;
// refer to the documentation for details.  For reference, here's the format
// of the USB command for option changes:
//
//    length of report = 8 bytes
//    byte 0 = 65 (0x41)
//    byte 1 = 1 (0x01)
//    byte 2 = new LedWiz unit number, 0x01 to 0x0f
//    byte 3 = feature enable bit mask:
//             0x01 = enable CCD (default = on)

 
#include "mbed.h"
#include "math.h"
#include "USBJoystick.h"
#include "MMA8451Q.h"
#include "tsl1410r.h"
#include "FreescaleIAP.h"
#include "crc32.h"


// ---------------------------------------------------------------------------
//
// Configuration details
//

// Our USB device vendor ID, product ID, and version.  
// We use the vendor ID for the LedWiz, so that the PC-side software can
// identify us as capable of performing LedWiz commands.  The LedWiz uses
// a product ID value from 0xF0 to 0xFF; the last four bits identify the
// unit number (e.g., product ID 0xF7 means unit #7).  This allows multiple
// LedWiz units to be installed in a single PC; the software on the PC side
// uses the unit number to route commands to the devices attached to each
// unit.  On the real LedWiz, the unit number must be set in the firmware
// at the factory; it's not configurable by the end user.  Most LedWiz's
// ship with the unit number set to 0, but the vendor will set different
// unit numbers if requested at the time of purchase.  So if you have a
// single LedWiz already installed in your cabinet, and you didn't ask for
// a non-default unit number, your existing LedWiz will be unit 0.
//
// We use unit #7 by default.  There doesn't seem to be a requirement that
// unit numbers be contiguous (DirectOutput Framework and other software
// seem happy to have units 0 and 7 installed, without 1-6 existing).
// Marking this unit as #7 should work for almost everybody out of the box;
// the most common case seems to be to have a single LedWiz installed, and
// it's probably extremely rare to more than two.
//
// Note that the USB_PRODUCT_ID value set here omits the unit number.  We
// take the unit number from the saved configuration.  We provide a
// configuration command that can be sent via the USB connection to change
// the unit number, so that users can select the unit number without having
// to install a different version of the software.  We'll combine the base
// product ID here with the unit number to get the actual product ID that
// we send to the USB controller.
const uint16_t USB_VENDOR_ID = 0xFAFA;
const uint16_t USB_PRODUCT_ID = 0x00F0;
const uint16_t USB_VERSION_NO = 0x0006;
const uint8_t DEFAULT_LEDWIZ_UNIT_NUMBER = 0x07;

// On-board RGB LED elements - we use these for diagnostic displays.
DigitalOut ledR(LED1), ledG(LED2), ledB(LED3);

// calibration button - switch input and LED output
DigitalIn calBtn(PTE29);
DigitalOut calBtnLed(PTE23);

// LED-Wiz emulation output pin assignments.  The LED-Wiz protocol
// can support up to 32 outputs.  The KL25Z can physically provide
// about 48 (in addition to the ports we're already using for the
// CCD sensor and the calibration button), but to stay compatible
// with the LED-Wiz protocol we'll stop at 32.  
//
// The LED-Wiz protocol allows setting individual intensity levels
// on all outputs, with 48 levels of intensity.  This can be used
// to control lamp brightness and motor speeds, among other things.
// Unfortunately, the KL25Z only has 10 PWM channels, so while we 
// can support the full complement of 32 outputs, we can only provide 
// PWM dimming/speed control on 10 of them.  The remaining outputs 
// can only be switched fully on and fully off - we can't support
// dimming on these, so they'll ignore any intensity level setting 
// requested by the host.  Use these for devices that don't have any
// use for intensity settings anyway, such as contactors and knockers.
//
// The mapping between physical output pins on the KL25Z and the
// assigned LED-Wiz port numbers is essentially arbitrary - you can
// customize this by changing the entries in the array below if you
// wish to rearrange the pins for any reason.  Be aware that some
// of the physical outputs are already used for other purposes
// (e.g., some of the GPIO pins on header J10 are used for the
// CCD sensor - but you can of course reassign those as well by
// changing the corresponding declarations elsewhere in this module).
// The assignments we make here have two main objectives: first,
// to group the outputs on headers J1 and J2 (to facilitate neater
// wiring by keeping the output pins together physically), and
// second, to make the physical pin layout match the LED-Wiz port
// numbering order to the extent possible.  There's one big wrench
// in the works, though, which is the limited number and discontiguous
// placement of the KL25Z PWM-capable output pins.  This prevents
// us from doing the most obvious sequential ordering of the pins,
// so we end up with the outputs arranged into several blocks.
// Hopefully this isn't too confusing; for more detailed rationale,
// read on...
// 
// With the LED-Wiz, the host software configuration usually 
// assumes that each RGB LED is hooked up to three consecutive ports
// (for the red, green, and blue components, which need to be 
// physically wired to separate outputs to allow each color to be 
// controlled independently).  To facilitate this, we arrange the 
// PWM-enabled outputs so that they're grouped together in the 
// port numbering scheme.  Unfortunately, these outputs aren't
// together in a single group in the physical pin layout, so to
// group them logically in the LED-Wiz port numbering scheme, we
// have to break up the overall numbering scheme into several blocks.
// So our port numbering goes sequentially down each column of
// header pins, but there are several break points where we have
// to interrupt the obvious sequence to keep the PWM pins grouped
// logically.
//
// In the list below, "pin J1-2" refers to pin 2 on header J1 on
// the KL25Z, using the standard pin numbering in the KL25Z 
// documentation - this is the physical pin that the port controls.
// "LW port 1" means LED-Wiz port 1 - this is the LED-Wiz port
// number that you use on the PC side (in the DirectOutput config
// file, for example) to address the port.  PWM-capable ports are
// marked as such - we group the PWM-capable ports into the first
// 10 LED-Wiz port numbers.
// 
struct {
    PinName pin;
    bool isPWM;
} ledWizPortMap[32] = {
    { PTA1, true },      // pin J1-2,  LW port 1  (PWM capable - TPM 2.0 = channel 9)
    { PTA2, true },      // pin J1-4,  LW port 2  (PWM capable - TPM 2.1 = channel 10)
    { PTD4, true },      // pin J1-6,  LW port 3  (PWM capable - TPM 0.4 = channel 5)
    { PTA12, true },     // pin J1-8,  LW port 4  (PWM capable - TPM 1.0 = channel 7)
    { PTA4, true },      // pin J1-10, LW port 5  (PWM capable - TPM 0.1 = channel 2)
    { PTA5, true },      // pin J1-12, LW port 6  (PWM capable - TPM 0.2 = channel 3)
    { PTA13, true },     // pin J2-2,  LW port 7  (PWM capable - TPM 1.1 = channel 13)
    { PTD5, true },      // pin J2-4,  LW port 8  (PWM capable - TPM 0.5 = channel 6)
    { PTD0, true },      // pin J2-6,  LW port 9  (PWM capable - TPM 0.0 = channel 1)
    { PTD3, true },      // pin J2-10, LW port 10 (PWM capable - TPM 0.3 = channel 4)
    { PTC8, false },     // pin J1-14, LW port 11
    { PTC9, false },     // pin J1-16, LW port 12
    { PTC7, false },     // pin J1-1,  LW port 13
    { PTC0, false },     // pin J1-3,  LW port 14
    { PTC3, false },     // pin J1-5,  LW port 15
    { PTC4, false },     // pin J1-7,  LW port 16
    { PTC5, false },     // pin J1-9,  LW port 17
    { PTC6, false },     // pin J1-11, LW port 18
    { PTC10, false },    // pin J1-13, LW port 19
    { PTC11, false },    // pin J1-15, LW port 20
    { PTC12, false },    // pin J2-1,  LW port 21
    { PTC13, false },    // pin J2-3,  LW port 22
    { PTC16, false },    // pin J2-5,  LW port 23
    { PTC17, false },    // pin J2-7,  LW port 24
    { PTA16, false },    // pin J2-9,  LW port 25
    { PTA17, false },    // pin J2-11, LW port 26
    { PTE31, false },    // pin J2-13, LW port 27
    { PTD6, false },     // pin J2-17, LW port 29
    { PTD7, false },     // pin J2-19, LW port 30
    { PTE0, false },     // pin J2-18, LW port 31
    { PTE1, false }      // pin J2-20, LW port 32
};


// I2C address of the accelerometer (this is a constant of the KL25Z)
const int MMA8451_I2C_ADDRESS = (0x1d<<1);

// SCL and SDA pins for the accelerometer (constant for the KL25Z)
#define MMA8451_SCL_PIN   PTE25
#define MMA8451_SDA_PIN   PTE24

// Digital in pin to use for the accelerometer interrupt.  For the KL25Z,
// this can be either PTA14 or PTA15, since those are the pins physically
// wired on this board to the MMA8451 interrupt controller.
#define MMA8451_INT_PIN   PTA15

// Joystick axis report range - we report from -JOYMAX to +JOYMAX
#define JOYMAX 4096


// ---------------------------------------------------------------------------
//
// LedWiz emulation
//

static int pbaIdx = 0;

// LedWiz output pin interface.  We create a cover class to virtualize
// digital vs PWM outputs and give them a common interface.  The KL25Z
// unfortunately doesn't have enough hardware PWM channels to support 
// PWM on all 32 LedWiz outputs, so we provide as many PWM channels as
// we can (10), and fill out the rest of the outputs with plain digital
// outs.
class LwOut
{
public:
    virtual void set(float val) = 0;
};
class LwPwmOut: public LwOut
{
public:
    LwPwmOut(PinName pin) : p(pin) { }
    virtual void set(float val) { p = val; }
    PwmOut p;
};
class LwDigOut: public LwOut
{
public:
    LwDigOut(PinName pin) : p(pin) { }
    virtual void set(float val) { p = val; }
    DigitalOut p;
};

// output pin array
static LwOut *lwPin[32];

// initialize the output pin array
void initLwOut()
{
    for (int i = 0 ; i < sizeof(lwPin) / sizeof(lwPin[0]) ; ++i)
    {
        PinName p = ledWizPortMap[i].pin;
        lwPin[i] = (ledWizPortMap[i].isPWM
                    ? (LwOut *)new LwPwmOut(p) 
                    : (LwOut *)new LwDigOut(p));
    }
}

// on/off state for each LedWiz output
static uint8_t wizOn[32];

// profile (brightness/blink) state for each LedWiz output
static uint8_t wizVal[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static float wizState(int idx)
{
    if (wizOn[idx]) {
        // on - map profile brightness state to PWM level
        uint8_t val = wizVal[idx];
        if (val >= 1 && val <= 48)
            return 1.0 - val/48.0;
        else if (val >= 129 && val <= 132)
            return 0.0;
        else
            return 1.0;
    }
    else {
        // off
        return 1.0;
    }
}

static void updateWizOuts()
{
    for (int i = 0 ; i < 32 ; ++i)
        lwPin[i]->set(wizState(i));
}

// ---------------------------------------------------------------------------
//
// Non-volatile memory (NVM)
//

// Structure defining our NVM storage layout.  We store a small
// amount of persistent data in flash memory to retain calibration
// data when powered off.
struct NVM
{
    // checksum - we use this to determine if the flash record
    // has been properly initialized
    uint32_t checksum;

    // signature value
    static const uint32_t SIGNATURE = 0x4D4A522A;
    static const uint16_t VERSION = 0x0003;
    
    // Is the data structure valid?  We test the signature and 
    // checksum to determine if we've been properly stored.
    int valid() const
    {
        return (d.sig == SIGNATURE 
                && d.vsn == VERSION
                && d.sz == sizeof(NVM)
                && checksum == CRC32(&d, sizeof(d)));
    }
    
    // save to non-volatile memory
    void save(FreescaleIAP &iap, int addr)
    {
        // update the checksum and structure size
        checksum = CRC32(&d, sizeof(d));
        d.sz = sizeof(NVM);
        
        // erase the sector
        iap.erase_sector(addr);

        // save the data
        iap.program_flash(addr, this, sizeof(*this));
    }
    
    // stored data (excluding the checksum)
    struct
    {
        // Signature, structure version, and structure size - further verification 
        // that we have valid initialized data.  The size is a simple proxy for a
        // structure version, as the most common type of change to the structure as
        // the software evolves will be the addition of new elements.  We also
        // provide an explicit version number that we can update manually if we
        // make any changes that don't affect the structure size but would affect
        // compatibility with a saved record (e.g., swapping two existing elements).
        uint32_t sig;
        uint16_t vsn;
        int sz;
        
        // has the plunger been manually calibrated?
        int plungerCal;
        
        // plunger calibration min and max
        int plungerMin;
        int plungerZero;
        int plungerMax;
        
        // is the CCD enabled?
        int ccdEnabled;
        
        // LedWiz unit number
        uint8_t ledWizUnitNo;
    } d;
};


// ---------------------------------------------------------------------------
//
// Customization joystick subbclass
//

class MyUSBJoystick: public USBJoystick
{
public:
    MyUSBJoystick(uint16_t vendor_id, uint16_t product_id, uint16_t product_release) 
        : USBJoystick(vendor_id, product_id, product_release, true)
    {
        suspended_ = false;
    }
    
    // are we connected?
    int isConnected()  { return configured(); }
    
    // Are we in suspend mode?
    int isSuspended() const { return suspended_; }
    
protected:
    virtual void suspendStateChanged(unsigned int suspended)
        { suspended_ = suspended; }

    // are we suspended?
    int suspended_; 
};

// ---------------------------------------------------------------------------
//
// Some simple math service routines
//

inline float square(float x) { return x*x; }
inline float round(float x) { return x > 0 ? floor(x + 0.5) : ceil(x - 0.5); }

// ---------------------------------------------------------------------------
// 
// Accelerometer (MMA8451Q)
//

// The MMA8451Q is the KL25Z's on-board 3-axis accelerometer.
//
// This is a custom wrapper for the library code to interface to the
// MMA8451Q.  This class encapsulates an interrupt handler and 
// automatic calibration.
//
// We install an interrupt handler on the accelerometer "data ready" 
// interrupt to ensure that we fetch each sample immediately when it
// becomes available.  The accelerometer data rate is fiarly high
// (800 Hz), so it's not practical to keep up with it by polling.
// Using an interrupt handler lets us respond quickly and read
// every sample.
//
// We automatically calibrate the accelerometer so that it's not
// necessary to get it exactly level when installing it, and so
// that it's also not necessary to calibrate it manually.  There's
// lots of experience that tells us that manual calibration is a
// terrible solution, mostly because cabinets tend to shift slightly
// during use, requiring frequent recalibration.  Instead, we
// calibrate automatically.  We continuously monitor the acceleration
// data, watching for periods of constant (or nearly constant) values.
// Any time it appears that the machine has been at rest for a while
// (about 5 seconds), we'll average the readings during that rest
// period and use the result as the level rest position.  This is
// is ongoing, so we'll quickly find the center point again if the 
// machine is moved during play (by an especially aggressive bout
// of nudging, say).
//

// accelerometer input history item, for gathering calibration data
struct AccHist
{
    AccHist() { x = y = d = 0.0; xtot = ytot = 0.0; cnt = 0; }
    void set(float x, float y, AccHist *prv)
    {
        // save the raw position
        this->x = x;
        this->y = y;
        this->d = distance(prv);
    }
    
    // reading for this entry
    float x, y;
    
    // distance from previous entry
    float d;
    
    // total and count of samples averaged over this period
    float xtot, ytot;
    int cnt;

    void clearAvg() { xtot = ytot = 0.0; cnt = 0; }    
    void addAvg(float x, float y) { xtot += x; ytot += y; ++cnt; }
    float xAvg() const { return xtot/cnt; }
    float yAvg() const { return ytot/cnt; }
    
    float distance(AccHist *p)
        { return sqrt(square(p->x - x) + square(p->y - y)); }
};

// accelerometer wrapper class
class Accel
{
public:
    Accel(PinName sda, PinName scl, int i2cAddr, PinName irqPin)
        : mma_(sda, scl, i2cAddr), intIn_(irqPin)
    {
        // remember the interrupt pin assignment
        irqPin_ = irqPin;

        // reset and initialize
        reset();
    }
    
    void reset()
    {
        // clear the center point
        cx_ = cy_ = 0.0;
        
        // start the calibration timer
        tCenter_.start();
        iAccPrv_ = nAccPrv_ = 0;
        
        // reset and initialize the MMA8451Q
        mma_.init();
                
        // set the initial integrated velocity reading to zero
        vx_ = vy_ = 0;
        
        // set up our accelerometer interrupt handling
        intIn_.rise(this, &Accel::isr);
        mma_.setInterruptMode(irqPin_ == PTA14 ? 1 : 2);
        
        // read the current registers to clear the data ready flag
        mma_.getAccXYZ(ax_, ay_, az_);

        // start our timers
        tGet_.start();
        tInt_.start();
    }
    
    void get(int &x, int &y, int &rx, int &ry) 
    {
         // disable interrupts while manipulating the shared data
         __disable_irq();
         
         // read the shared data and store locally for calculations
         float ax = ax_, ay = ay_;
         float vx = vx_, vy = vy_;
         
         // reset the velocity sum for the next run
         vx_ = vy_ = 0;

         // get the time since the last get() sample
         float dt = tGet_.read_us()/1.0e6;
         tGet_.reset();
         
         // done manipulating the shared data
         __enable_irq();
         
         // adjust the readings for the integration time
         vx /= dt;
         vy /= dt;
         
         // add this sample to the current calibration interval's running total
         AccHist *p = accPrv_ + iAccPrv_;
         p->addAvg(ax, ay);

         // check for auto-centering every so often
         if (tCenter_.read_ms() > 1000)
         {
             // add the latest raw sample to the history list
             AccHist *prv = p;
             iAccPrv_ = (iAccPrv_ + 1) % maxAccPrv;
             p = accPrv_ + iAccPrv_;
             p->set(ax, ay, prv);

             // if we have a full complement, check for stability
             if (nAccPrv_ >= maxAccPrv)
             {
                 // check if we've been stable for all recent samples
                 static const float accTol = .01;
                 AccHist *p0 = accPrv_;
                 if (p0[0].d < accTol
                     && p0[1].d < accTol
                     && p0[2].d < accTol
                     && p0[3].d < accTol
                     && p0[4].d < accTol)
                 {
                     // Figure the new calibration point as the average of
                     // the samples over the rest period
                     cx_ = (p0[0].xAvg() + p0[1].xAvg() + p0[2].xAvg() + p0[3].xAvg() + p0[4].xAvg())/5.0;
                     cy_ = (p0[0].yAvg() + p0[1].yAvg() + p0[2].yAvg() + p0[3].yAvg() + p0[4].yAvg())/5.0;
                 }
             }
             else
             {
                // not enough samples yet; just up the count
                ++nAccPrv_;
             }
             
             // clear the new item's running totals
             p->clearAvg();
            
             // reset the timer
             tCenter_.reset();
         }
         
         // report our integrated velocity reading in x,y
         x = rawToReport(vx);
         y = rawToReport(vy);
         
         // apply a small dead zone near the center
         // if (abs(x) < 6) x = 0;
         // if (abs(y) < 6) y = 0;
         
         // report the calibrated instantaneous acceleration in rx,ry
         rx = int(round((ax - cx_)*JOYMAX));
         ry = int(round((ay - cy_)*JOYMAX));
         
#ifdef DEBUG_PRINTF
         if (x != 0 || y != 0)        
             printf("%f %f %d %d %f\r\n", vx, vy, x, y, dt);
#endif
     }    
    
private:
    // adjust a raw acceleration figure to a usb report value
    int rawToReport(float v)
    {
        // scale to the joystick report range and round to integer
        int i = int(round(v*JOYMAX));
        
        // if it's near the center, scale it roughly as 20*(i/20)^2,
        // to suppress noise near the rest position
        static const int filter[] = { 
            -18, -16, -14, -13, -11, -10, -8, -7, -6, -5, -4, -3, -2, -2, -1, -1, 0, 0, 0, 0,
            0,
            0, 0, 0, 0, 1, 1, 2, 2, 3, 4, 5, 6, 7, 8, 10, 11, 13, 14, 16, 18
        };
        return (i > 20 || i < -20 ? i : filter[i+20]);
    }

    // interrupt handler
    void isr()
    {
        // Read the axes.  Note that we have to read all three axes
        // (even though we only really use x and y) in order to clear
        // the "data ready" status bit in the accelerometer.  The
        // interrupt only occurs when the "ready" bit transitions from
        // off to on, so we have to make sure it's off.
        float x, y, z;
        mma_.getAccXYZ(x, y, z);
        
        // calculate the time since the last interrupt
        float dt = tInt_.read_us()/1.0e6;
        tInt_.reset();

        // integrate the time slice from the previous reading to this reading
        vx_ += (x + ax_ - 2*cx_)*dt/2;
        vy_ += (y + ay_ - 2*cy_)*dt/2;
        
        // store the updates
        ax_ = x;
        ay_ = y;
        az_ = z;
    }
    
    // underlying accelerometer object
    MMA8451Q mma_;
    
    // last raw acceleration readings
    float ax_, ay_, az_;
    
    // integrated velocity reading since last get()
    float vx_, vy_;
        
    // timer for measuring time between get() samples
    Timer tGet_;
    
    // timer for measuring time between interrupts
    Timer tInt_;

    // Calibration reference point for accelerometer.  This is the
    // average reading on the accelerometer when in the neutral position
    // at rest.
    float cx_, cy_;

    // timer for atuo-centering
    Timer tCenter_;

    // Auto-centering history.  This is a separate history list that
    // records results spaced out sparesely over time, so that we can
    // watch for long-lasting periods of rest.  When we observe nearly
    // no motion for an extended period (on the order of 5 seconds), we
    // take this to mean that the cabinet is at rest in its neutral 
    // position, so we take this as the calibration zero point for the
    // accelerometer.  We update this history continuously, which allows
    // us to continuously re-calibrate the accelerometer.  This ensures
    // that we'll automatically adjust to any actual changes in the
    // cabinet's orientation (e.g., if it gets moved slightly by an
    // especially strong nudge) as well as any systematic drift in the
    // accelerometer measurement bias (e.g., from temperature changes).
    int iAccPrv_, nAccPrv_;
    static const int maxAccPrv = 5;
    AccHist accPrv_[maxAccPrv];
    
    // interurupt pin name
    PinName irqPin_;
    
    // interrupt router
    InterruptIn intIn_;
};


// ---------------------------------------------------------------------------
//
// Clear the I2C bus for the MMA8451!.  This seems necessary some of the time
// for reasons that aren't clear to me.  Doing a hard power cycle has the same
// effect, but when we do a soft reset, the hardware sometimes seems to leave
// the MMA's SDA line stuck low.  Forcing a series of 9 clock pulses through
// the SCL line is supposed to clear this conidtion.
//
void clear_i2c()
{
    // assume a general-purpose output pin to the I2C clock
    DigitalOut scl(MMA8451_SCL_PIN);
    DigitalIn sda(MMA8451_SDA_PIN);
    
    // clock the SCL 9 times
    for (int i = 0 ; i < 9 ; ++i)
    {
        scl = 1;
        wait_us(20);
        scl = 0;
        wait_us(20);
    }
}

// ---------------------------------------------------------------------------
//
// Main program loop.  This is invoked on startup and runs forever.  Our
// main work is to read our devices (the accelerometer and the CCD), process
// the readings into nudge and plunger position data, and send the results
// to the host computer via the USB joystick interface.  We also monitor
// the USB connection for incoming LedWiz commands and process those into
// port outputs.
//
int main(void)
{
    // turn off our on-board indicator LED
    ledR = 1;
    ledG = 1;
    ledB = 1;
    
    // initialize the LedWiz ports
    initLwOut();
    
    // we don't need a reset yet
    bool needReset = false;
    
    // clear the I2C bus for the accelerometer
    clear_i2c();
    
    // set up a flash memory controller
    FreescaleIAP iap;
    
    // use the last sector of flash for our non-volatile memory structure
    int flash_addr = (iap.flash_size() - SECTOR_SIZE);
    NVM *flash = (NVM *)flash_addr;
    NVM cfg;
    
    // check for valid flash
    bool flash_valid = flash->valid();
                      
    // Number of pixels we read from the sensor on each frame.  This can be
    // less than the physical pixel count if desired; we'll read every nth
    // piexl if so.  E.g., with a 1280-pixel physical sensor, if npix is 320,
    // we'll read every 4th pixel.  It takes time to read each pixel, so the
    // fewer pixels we read, the higher the refresh rate we can achieve.
    // It's therefore better not to read more pixels than we have to.
    //
    // VP seems to have an internal resolution in the 8-bit range, so there's
    // no apparent benefit to reading more than 128-256 pixels when using VP.
    // Empirically, 160 pixels seems about right.  The overall travel of a
    // standard pinball plunger is about 3", so 160 pixels gives us resolution
    // of about 1/50".  This seems to take full advantage of VP's modeling
    // ability, and is probably also more precise than a human player's
    // perception of the plunger position.
    const int npix = 160;

    // if the flash is valid, load it; otherwise initialize to defaults
    if (flash_valid) {
        memcpy(&cfg, flash, sizeof(cfg));
        printf("Flash restored: plunger cal=%d, min=%d, zero=%d, max=%d\r\n", 
            cfg.d.plungerCal, cfg.d.plungerMin, cfg.d.plungerZero, cfg.d.plungerMax);
    }
    else {
        printf("Factory reset\r\n");
        cfg.d.sig = cfg.SIGNATURE;
        cfg.d.vsn = cfg.VERSION;
        cfg.d.plungerCal = 0;
        cfg.d.plungerZero = 0;
        cfg.d.plungerMin = 0;
        cfg.d.plungerMax = npix;
        cfg.d.ledWizUnitNo = DEFAULT_LEDWIZ_UNIT_NUMBER;
        cfg.d.ccdEnabled = true;
    }
    
    // Create the joystick USB client.  Note that we use the LedWiz unit
    // number from the saved configuration.
    MyUSBJoystick js(
        USB_VENDOR_ID, 
        USB_PRODUCT_ID | cfg.d.ledWizUnitNo,
        USB_VERSION_NO);

    // plunger calibration button debounce timer
    Timer calBtnTimer;
    calBtnTimer.start();
    int calBtnDownTime = 0;
    int calBtnLit = false;
    
    // Calibration button state:
    //  0 = not pushed
    //  1 = pushed, not yet debounced
    //  2 = pushed, debounced, waiting for hold time
    //  3 = pushed, hold time completed - in calibration mode
    int calBtnState = 0;
    
    // set up a timer for our heartbeat indicator
    Timer hbTimer;
    hbTimer.start();
    int hb = 0;
    uint16_t hbcnt = 0;
    
    // set a timer for accelerometer auto-centering
    Timer acTimer;
    acTimer.start();
    
    // create the accelerometer object
    Accel accel(MMA8451_SCL_PIN, MMA8451_SDA_PIN, MMA8451_I2C_ADDRESS, MMA8451_INT_PIN);
    
    // create the CCD array object
    TSL1410R ccd(PTE20, PTE21, PTB0);
    
    // last accelerometer report, in mouse coordinates
    int x = 0, y = 0, z = 0;
    
    // previous two plunger readings, for "debouncing" the results (z0 is
    // the most recent, z1 is the one before that)
    int z0 = 0, z1 = 0, z2 = 0;
    
    // Firing in progress: we set this when we detect the start of rapid 
    // plunger movement from a retracted position towards the rest position.
    // The actual plunger spring return speed seems to be too slow for VP, 
    // so when we detect the start of this motion, we immediately tell VP
    // to return the plunger to rest, then we monitor the real plunger 
    // until it atcually stops.
    bool firing = false;

    // start the first CCD integration cycle
    ccd.clear();

    // we're all set up - now just loop, processing sensor reports and 
    // host requests
    for (;;)
    {
        // Look for an incoming report.  Continue processing input as
        // long as there's anything pending - this ensures that we
        // handle input in as timely a fashion as possible by deferring
        // output tasks as long as there's input to process.
        HID_REPORT report;
        while (js.readNB(&report))
        {
            // all Led-Wiz reports are 8 bytes exactly
            if (report.length == 8)
            {
                uint8_t *data = report.data;
                if (data[0] == 64) 
                {
                    // LWZ-SBA - first four bytes are bit-packed on/off flags
                    // for the outputs; 5th byte is the pulse speed (0-7)
                    //printf("LWZ-SBA %02x %02x %02x %02x ; %02x\r\n",
                    //       data[1], data[2], data[3], data[4], data[5]);
    
                    // update all on/off states
                    for (int i = 0, bit = 1, ri = 1 ; i < 32 ; ++i, bit <<= 1)
                    {
                        if (bit == 0x100) {
                            bit = 1;
                            ++ri;
                        }
                        wizOn[i] = ((data[ri] & bit) != 0);
                    }
        
                    // update the physical outputs
                    updateWizOuts();
                    
                    // reset the PBA counter
                    pbaIdx = 0;
                }
                else if (data[0] == 65)
                {
                    // Private control message.  This isn't an LedWiz message - it's
                    // an extension for this device.  65 is an invalid PBA setting,
                    // and isn't used for any other LedWiz message, so we appropriate
                    // it for our own private use.  The first byte specifies the 
                    // message type.
                    if (data[1] == 1)
                    {
                        // Set Configuration:
                        //     data[2] = LedWiz unit number (0x00 to 0x0f)
                        //     data[3] = feature enable bit mask:
                        //               0x01 = enable CCD
                        
                        // we'll need a reset if the LedWiz unit number is changing
                        uint8_t newUnitNo = data[2] & 0x0f;
                        needReset |= (newUnitNo != cfg.d.ledWizUnitNo);
                        
                        // set the configuration parameters from the message
                        cfg.d.ledWizUnitNo = newUnitNo;
                        cfg.d.ccdEnabled = data[3] & 0x01;
                        
                        // save the configuration
                        cfg.save(iap, flash_addr);
                    }
                }
                else 
                {
                    // LWZ-PBA - full state dump; each byte is one output
                    // in the current bank.  pbaIdx keeps track of the bank;
                    // this is incremented implicitly by each PBA message.
                    //printf("LWZ-PBA[%d] %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
                    //       pbaIdx, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    
                    // update all output profile settings
                    for (int i = 0 ; i < 8 ; ++i)
                        wizVal[pbaIdx + i] = data[i];
    
                    // update the physical LED state if this is the last bank                    
                    if (pbaIdx == 24)
                        updateWizOuts();
    
                    // advance to the next bank
                    pbaIdx = (pbaIdx + 8) & 31;
                }
            }
        }
       
        // check for plunger calibration
        if (!calBtn)
        {
            // check the state
            switch (calBtnState)
            {
            case 0: 
                // button not yet pushed - start debouncing
                calBtnTimer.reset();
                calBtnDownTime = calBtnTimer.read_ms();
                calBtnState = 1;
                break;
                
            case 1:
                // pushed, not yet debounced - if the debounce time has
                // passed, start the hold period
                if (calBtnTimer.read_ms() - calBtnDownTime > 50)
                    calBtnState = 2;
                break;
                
            case 2:
                // in the hold period - if the button has been held down
                // for the entire hold period, move to calibration mode
                if (calBtnTimer.read_ms() - calBtnDownTime > 2050)
                {
                    // enter calibration mode
                    calBtnState = 3;
                    
                    // set extremes for the calibration data, so that the actual
                    // values we read will set new high/low water marks on the fly
                    cfg.d.plungerMax = 0;
                    cfg.d.plungerZero = npix;
                    cfg.d.plungerMin = npix;
                }
                break;
                
            case 3:
                // Already in calibration mode - pushing the button in this
                // state doesn't change the current state, but we won't leave
                // this state as long as it's held down.  We can simply do
                // nothing here.
                break;
            }
        }
        else
        {
            // Button released.  If we're in calibration mode, and
            // the calibration time has elapsed, end the calibration
            // and save the results to flash.
            //
            // Otherwise, return to the base state without saving anything.
            // If the button is released before we make it to calibration
            // mode, it simply cancels the attempt.
            if (calBtnState == 3
                && calBtnTimer.read_ms() - calBtnDownTime > 17500)
            {
                // exit calibration mode
                calBtnState = 0;
                
                // save the updated configuration
                cfg.d.plungerCal = 1;
                cfg.save(iap, flash_addr);
                
                // the flash state is now valid
                flash_valid = true;
            }
            else if (calBtnState != 3)
            {
                // didn't make it to calibration mode - cancel the operation
                calBtnState = 0;
            }
        }       
        
        // light/flash the calibration button light, if applicable
        int newCalBtnLit = calBtnLit;
        switch (calBtnState)
        {
        case 2:
            // in the hold period - flash the light
            newCalBtnLit = (((calBtnTimer.read_ms() - calBtnDownTime)/250) & 1);
            break;
            
        case 3:
            // calibration mode - show steady on
            newCalBtnLit = true;
            break;
            
        default:
            // not calibrating/holding - show steady off
            newCalBtnLit = false;
            break;
        }
        
        // light or flash the external calibration button LED, and 
        // do the same with the on-board blue LED
        if (calBtnLit != newCalBtnLit)
        {
            calBtnLit = newCalBtnLit;
            if (calBtnLit) {
                calBtnLed = 1;
                ledR = 1;
                ledG = 1;
                ledB = 1;
            }
            else {
                calBtnLed = 0;
                ledR = 1;
                ledG = 1;
                ledB = 0;
            }
        }
        
        // read the plunger sensor, if it's enabled
        if (cfg.d.ccdEnabled)
        {
            // start with the previous reading, in case we don't have a
            // clear result on this frame
            int znew = z;

            // read the array
            uint16_t pix[npix];
            ccd.read(pix, npix);
    
            // get the average brightness at each end of the sensor
            long avg1 = (long(pix[0]) + long(pix[1]) + long(pix[2]) + long(pix[3]) + long(pix[4]))/5;
            long avg2 = (long(pix[npix-1]) + long(pix[npix-2]) + long(pix[npix-3]) + long(pix[npix-4]) + long(pix[npix-5]))/5;
            
            // figure the midpoint in the brightness; multiply by 3 so that we can
            // compare sums of three pixels at a time to smooth out noise
            long midpt = (avg1 + avg2)/2 * 3;
            
            // Work from the bright end to the dark end.  VP interprets the
            // Z axis value as the amount the plunger is pulled: zero is the
            // rest position, and the axis maximum is fully pulled.  So we 
            // essentially want to report how much of the sensor is lit,
            // since this increases as the plunger is pulled back.
            int si = 1, di = 1;
            if (avg1 < avg2)
                si = npix - 2, di = -1;
    
            // If the bright end and dark end don't differ by enough, skip this
            // reading entirely - we must have an overexposed or underexposed frame.
            // Otherwise proceed with the scan.
            if (labs(avg1 - avg2) > 0x1000)
            {
                uint16_t *pixp = pix + si;           
                for (int n = 1 ; n < npix - 1 ; ++n, pixp += di)
                {
                    // if we've crossed the midpoint, report this position
                    if (long(pixp[-1]) + long(pixp[0]) + long(pixp[1]) < midpt)
                    {
                        // note the new position
                        int pos = n;
                        
                        // Calibrate, or apply calibration, depending on the mode.
                        // In either case, normalize to our range.  VP appears to
                        // ignore negative Z axis values.
                        if (calBtnState == 3)
                        {
                            // calibrating - note if we're expanding the calibration envelope
                            if (pos < cfg.d.plungerMin)
                                cfg.d.plungerMin = pos;
                            if (pos < cfg.d.plungerZero)
                                cfg.d.plungerZero = pos;
                            if (pos > cfg.d.plungerMax)
                                cfg.d.plungerMax = pos;
                                
                            // normalize to the full physical range while calibrating
                            znew = int(round(float(pos)/npix * JOYMAX));
                        }
                        else
                        {
                            // Running normally - normalize to the calibration range.  Note
                            // that values below the zero point are allowed - the zero point
                            // represents the park position, where the plunger sits when at
                            // rest, but a mechanical plunger has a smmall amount of travel
                            // in the "push" direction.  We represent forward travel with
                            // negative z values.
                            if (pos > cfg.d.plungerMax)
                                pos = cfg.d.plungerMax;
                            znew = int(round(float(pos - cfg.d.plungerZero)
                                / (cfg.d.plungerMax - cfg.d.plungerZero + 1) * JOYMAX));
                        }
                        
                        // done
                        break;
                    }
                }
            }
        
            // "Debounce" the plunger reading.  
            //
            // It takes us about 25ms to read the CCD and calculate the new
            // plunger position.  That's not quite fast enough to keep up with
            // very fast plunger motions.  And the single most important motion 
            // the plunger makes - releasing from a retracted position it to 
            // launch the ball - is just such a fast motion.  Our scan rate is
            // fast enough to capture one or two intermediate frames in a release
            // motion, but it's not nearly fast enough to get a clean reading on 
            // the instantaneous speed, let alone accelerations.
            //
            // Fortunately, we don't need to take speed readings at all.  VP has
            // its own internal simulated plunger model, which it uses to calculate
            // the speed and force of the plunger movement.  Our readings tell VP
            // where the plunger should be at any given moment, and VP makes its
            // model move in that direction, using the model parameters for speed
            // and acceleration.  So whatever speed we see physically is irrelevant;
            // the VP model plunger can only move at the speed set in its model.
            //
            // This works out great for our relatively slow scan rate.  We don't
            // have to take readings quickly enough to get instantaneous velocities;
            // we just need to know where the plunger is once in a while so that
            // VP can move its model plunger in the right direction for the right
            // distance, and VP figures out the appropriate speed for the required
            // travel.  
            //
            // But there is one complication.  We do scan fast enough to see *some* 
            // intermediate positions during a fast motion.  Suppose that on one
            // scan, the plunger is fully retracted.  Now suppose that the player
            // releases the plunger just after that scan, such that our next scan
            // catches the plunger *almost* back to the rest position, but not
            // quite - just a hair short.  If we send these two consecutive reports
            // to VP, VP will set its model plunger in motion with the *almost*
            // reading as the destination.  VP will step its physics model with
            // this new plunger destination until we send another reading.
            // Ddpending on how the timing of our next scan works out, it's
            // possible that the model plunger will have reached or almost reached
            // the destination by the time we send our next report - so VP might
            // be decelerating or stopping the model plunger as it approaches
            // this position.  Our next scan will probably find the plunger back
            // at the rest position, so we'll tell VP to continue moving the
            // plunger to the zero spot.  The problem that just happened is that
            // our intermediate *almost there* report might have robbed the
            // motion in the model of some energy that should have been there,
            // by causing it to decelerate briefly near the intermediate position.
            //
            // This is relatively easy to fix.  Because VP does all of the fast
            // motion modeling on its own anyway, there's no advantage to sending
            // VP intermediate positions during rapid motions - and there's the
            // disadvantage we just described.  So all we need to do is skip
            // reports while the plunger is moving rapidly - we just need to wait
            // for it to settle at a new position before sending an update.
            //
            // So: only report the latest reading if it's relatively close to the
            // previous reading, indicating we're moving slowly or at rest.  One
            // exception: if we see a reversal of direction, report the previous
            // reading, which is the peak in the previous direction.  This will
            // catch cases where the player is moving the plunger very rapidly
            // back and forth, as well as release motions where the plunger
            // briefly overshoots the rest position.
#if 1
            // Check to see if plunger firing is in progress.  If not, check
            // to see if it looks like we just started firing.
            const int restTol = JOYMAX/npix * 4;
            const int fireTol = JOYMAX/npix * 12;
            if (firing)
            {
                // Firing in progress - we've already told VP to send its
                // model plunger all the way back to the rest position, so
                // send no further reports until the mechanical plunger
                // actually comes to rest somewhere.
                if (abs(z0 - z2) < restTol && abs(znew - z2) < restTol)
                {
                    // the plunger is back at rest - firing is done
                    firing = false;
                    
                    // resume normal reporting
                    z = z2;
                }
            }
            else if (z0 < z2 && z1 < z2 && znew < z2
                     && (z0 < z2 - fireTol 
                         || z1 < z2 - fireTol
                         || znew < z2 - fireTol))
            {
                // Big jumps toward rest position in last two readings - 
                // firing has begun.  Report an immediate return to the
                // rest position, and send no further reports until the
                // physical plunger has come to rest.  This effectively
                // detaches VP's model plunger from the real world for
                // the duration of the spring return, letting VP evolve
                // its model without trying to synchronize with the
                // mechanical version.  The release motion is too fast
                // for that to work well; we can't take samples quickly
                // enough to get prcise velocity or acceleration
                // readings.  It's better to let VP figure the speed
                // and acceleration through modeling.  Plus, that lets
                // each virtual table set the desired parameters for its
                // virtual plunger, rather than imposing the actual
                // mechanical charateristics of the physical plunger on
                // every table.
                firing = true;
                z = 0;
            }
            else
            {
                // everything normal; report the 3rd recent position on
                // tape delay
                z = z2;
            }
        
            // shift in the new reading
            z2 = z1;
            z1 = z0;
            z0 = znew;
#endif


#if 0
            // check for the anomalous fast return case, where we get two
            // descending readings out of order
            if (znew < z1 
                && z0 < z1 
                && znew > z0
                && abs(znew - z1) > JOYMAX/npix*3 
                && abs(z0 - z1) > JOYMAX/npix*3)
            {
                // drop the middle reading - report nothing this round
                z0 = znew;
            }
            else
            {   
                // report the previous reading
                z = z0;
                
                // shift in the new reading
                z1 = z0;
                z0 = znew;
            }
#endif
#if 0
            static int insertion = -1;
            static int insertionList[] = { 0, 400, 800, 1200, 1600, 2000, 2400, 2800, 3200 };
            static int overcnt = 0;
            if (insertion >= 0)
                z = insertionList[insertion--];
            else if (znew > 3500 && z == 0)
                z = 3500, overcnt = 1;
            else if (znew > 3500)
                ++overcnt;
            else if (znew < 3500 && overcnt > 3)
                insertion = sizeof(insertionList)/sizeof(insertionList[0]) - 1, z = 3500, overcnt = 0;
            else
                overcnt = 0, z = 0;
#endif
#if 0
            if (znew != z) printf("%d\r\n", znew);
            z = znew;
#endif
#if 0
            // average the last three readings
            z = int(round(0.0f + znew + z0 + z1)/3.0f);
            
            // shift in the new reading
            z1 = z0;
            z0 = znew;
#endif
#if 0
            const int zTol = JOYMAX/npix*5;
            if (abs(znew - z0) < zTol && abs(z0 - z1) < zTol)
            {
                // slow or at rest - report the current reading
                z = znew;
            }
            else if ((z0 < z1 && znew > z0) || (z0 > z1 && znew < z0))
            {
                // direction reveersal - report the peak reading
                z = z0;
            }
                
            // in any case, remember this new reading, whether reporting it or not
            z1 = z0;
            z0 = znew;
#endif
        }

        // read the accelerometer
        int xa, ya, rxa, rya;
        accel.get(xa, ya, rxa, rya);
        
        // confine the results to our joystick axis range
        if (xa < -JOYMAX) xa = -JOYMAX;
        if (xa > JOYMAX) xa = JOYMAX;
        if (ya < -JOYMAX) ya = -JOYMAX;
        if (ya > JOYMAX) ya = JOYMAX;
        
        // store the updated accelerometer coordinates
        x = xa;
        y = ya;
        
        // Send the status report.
        //
        // $$$ button updates are for diagnostics, so we can see that the
        // device is sending data properly if the accelerometer gets stuck
        uint16_t btns = hb ? 0x5500 : 0xAA00;
        js.update(x, y, z, rxa, rya, btns);
        
#ifdef DEBUG_PRINTF
        if (x != 0 || y != 0)
            printf("%d,%d\r\n", x, y);
#endif

        // provide a visual status indication on the on-board LED
        if (calBtnState < 2 && hbTimer.read_ms() > 1000) 
        {
            if (js.isSuspended() || !js.isConnected())
            {
                // suspended - turn off the LED
                ledR = 1;
                ledG = 1;
                ledB = 1;

                // show a status flash every so often                
                if (hbcnt % 3 == 0)
                {
                    // disconnected = red/red flash; suspended = red
                    for (int n = js.isConnected() ? 1 : 2 ; n > 0 ; --n)
                    {
                        ledR = 0;
                        wait(0.05);
                        ledR = 1;
                        wait(0.25);
                    }
                }
            }
            else if (needReset)
            {
                // connected, need to reset due to changes in config parameters -
                // flash red/green
                hb = !hb;
                ledR = (hb ? 0 : 1);
                ledG = (hb ? 1 : 0);
                ledB = 0;
            }
            else if (cfg.d.ccdEnabled && !cfg.d.plungerCal)
            {
                // connected, plunger calibration needed - flash yellow/green
                hb = !hb;
                ledR = (hb ? 0 : 1);
                ledG = 0;
                ledB = 1;
            }
            else
            {
                // connected - flash blue/green
                hb = !hb;
                ledR = 1;
                ledG = (hb ? 0 : 1);
                ledB = (hb ? 1 : 0);
            }
            
            // reset the heartbeat timer
            hbTimer.reset();
            ++hbcnt;
        }
    }
}
