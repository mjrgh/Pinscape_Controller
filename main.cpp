/* Copyright 2014, 2015 M J Roberts, MIT License
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
// The Pinscape Controller
// A comprehensive input/output controller for virtual pinball machines
//
// This project implements an I/O controller for virtual pinball cabinets.  Its
// function is to connect Windows pinball software, such as Visual Pinball, with
// physical devices in the cabinet: buttons, sensors, and feedback devices that
// create visual or mechanical effects during play.  
//
// The software can perform several different functions, which can be used 
// individually or in any combination:
//
//  - Nudge sensing.  This uses the KL25Z's on-board accelerometer to sense the
//    motion of the cabinet when you nudge it.  Visual Pinball and other pinball 
//    emulators on the PC have native handling for this type of input, so that 
//    physical nudges on the cabinet turn into simulated effects on the virtual 
//    ball.  The KL25Z measures accelerations as analog readings and is quite 
//    sensitive, so the effect of a nudge on the simulation is proportional
//    to the strength of the nudge.  Accelerations are reported to the PC via a 
//    simulated joystick (using the X and Y axes); you just have to set some 
//    preferences in your  pinball software to tell it that an accelerometer 
//    is attached.
//
//  - Plunger position sensing, with mulitple sensor options.  To use this feature,
//    you need to choose a sensor and set it up, connect the sensor electrically to 
//    the KL25Z, and configure the Pinscape software on the KL25Z to let it know how 
//    the sensor is hooked up.  The Pinscape software monitors the sensor and sends
//    readings to Visual Pinball via the joystick Z axis.  VP and other PC software
//    have native support for this type of input; as with the nudge setup, you just 
//    have to set some options in VP to activate the plunger.
//
//    The Pinscape software supports optical sensors (the TAOS TSL1410R and TSL1412R 
//    linear sensor arrays) as well as slide potentiometers.  The specific equipment
//    that's supported, along with physical mounting and wiring details, can be found
//    in the Build Guide.
//
//    Note VP has built-in support for plunger devices like this one, but some VP
//    tables can't use it without some additional scripting work.  The Build Guide has 
//    advice on adjusting tables to add plunger support when necessary.
//
//    For best results, the plunger sensor should be calibrated.  The calibration
//    is stored in non-volatile memory on board the KL25Z, so it's only necessary
//    to do the calibration once, when you first install everything.  (You might
//    also want to re-calibrate if you physically remove and reinstall the CCD 
//    sensor or the mechanical plunger, since their alignment shift change slightly 
//    when you put everything back together.)  You can optionally install a
//    dedicated momentary switch or pushbutton to activate the calibration mode;
//    this is describe in the project documentation.  If you don't want to bother
//    with the extra button, you can also trigger calibration using the Windows 
//    setup software, which you can find on the Pinscape project page.
//
//    The calibration procedure is described in the project documentation.  Briefly,
//    when you trigger calibration mode, the software will scan the CCD for about
//    15 seconds, during which you should simply pull the physical plunger back
//    all the way, hold it for a moment, and then slowly return it to the rest
//    position.  (DON'T just release it from the retracted position, since that
//    let it shoot forward too far.  We want to measure the range from the park
//    position to the fully retracted position only.)
//
//  - Button input wiring.  24 of the KL25Z's GPIO ports are mapped as digital inputs
//    for buttons and switches.  You can wire each input to a physical pinball-style
//    button or switch, such as flipper buttons, Start buttons, coin chute switches,
//    tilt bobs, and service buttons.  Each button can be configured to be reported
//    to the PC as a joystick button or as a keyboard key (you can select which key
//    is used for each button).
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
//  - Enhanced LedWiz emulation with TLC5940 PWM controller chips.  You can attach
//    external PWM controller chips for controlling device outputs, instead of using
//    the limited LedWiz emulation through the on-board GPIO ports as described above. 
//    The software can control a set of daisy-chained TLC5940 chips, which provide
//    16 PWM outputs per chip.  Two of these chips give you the full complement
//    of 32 output ports of an actual LedWiz, and four give you 64 ports, which
//    should be plenty for nearly any virtual pinball project.  A private, extended
//    version of the LedWiz protocol lets the host control the extra outputs, up to
//    128 outputs per KL25Z (8 TLC5940s).  To take advantage of the extra outputs
//    on the PC side, you need software that knows about the protocol extensions,
//    which means you need the latest version of DirectOutput Framework (DOF).  VP
//    uses DOF for its output, so VP will be able to use the added ports without any
//    extra work on your part.  Older software (e.g., Future Pinball) that doesn't
//    use DOF will still be able to use the LedWiz-compatible protocol, so it'll be
//    able to control your first 32 ports (numbered 1-32 in the LedWiz scheme), but
//    older software won't be able to address higher-numbered ports.  That shouldn't
//    be a problem because older software wouldn't know what to do with the extra
//    devices anyway - FP, for example, is limited to a pre-defined set of outputs.
//    As long as you put the most common devices on the first 32 outputs, and use
//    higher numbered ports for the less common devices that older software can't
//    use anyway, you'll get maximum functionality out of software new and old.
//
//  - Night Mode control for output devices.  You can connect a switch or button
//    to the controller to activate "Night Mode", which disables feedback devices
//    that you designate as noisy.  You can designate outputs individually as being 
//    included in this set or not.  This is useful if you want to play a game on 
//    your cabinet late at night without waking the kids and annoying the neighbors.
//
//  - TV ON switch.  The controller can pulse a relay to turn on your TVs after
//    power to the cabinet comes on, with a configurable delay timer.  This feature
//    is for TVs that don't turn themselves on automatically when first plugged in.
//    To use this feature, you have to build some external circuitry to allow the
//    software to sense the power supply status, and you have to run wires to your
//    TV's on/off button, which requires opening the case on your TV.  The Build
//    Guide has details on the necessary circuitry and connections to the TV.
//
//
//
// STATUS LIGHTS:  The on-board LED on the KL25Z flashes to indicate the current 
// device status.  The flash patterns are:
//
//    two short red flashes = the device is powered but hasn't successfully
//        connected to the host via USB (either it's not physically connected
//        to the USB port, or there was a problem with the software handshake
//        with the USB device driver on the computer)
//
//    short red flash = the host computer is in sleep/suspend mode
//
//    long red/yellow = USB connection problem.  The device still has a USB
//        connection to the host, but data transmissions are failing.  This
//        condition shouldn't ever occur; if it does, it probably indicates
//        a bug in the device's USB software.  This display is provided to
//        flag any occurrences for investigation.  You'll probably need to
//        manually reset the device if this occurs.
//
//    long yellow/green = everything's working, but the plunger hasn't
//        been calibrated.  Follow the calibration procedure described in
//        the project documentation.  This flash mode won't appear if there's
//        no plunger sensor configured.
//
//    alternating blue/green = everything's working normally, and plunger
//        calibration has been completed (or there's no plunger attached)
//
//
// USB PROTOCOL:  please refer to USBProtocol.h for details on the USB
// message protocol.


#include "mbed.h"
#include "math.h"
#include "USBJoystick.h"
#include "MMA8451Q.h"
#include "tsl1410r.h"
#include "FreescaleIAP.h"
#include "crc32.h"
#include "TLC5940.h"
#include "74HC595.h"
#include "nvm.h"
#include "plunger.h"
#include "ccdSensor.h"
#include "potSensor.h"
#include "nullSensor.h"

#define DECL_EXTERNS
#include "config.h"


// ---------------------------------------------------------------------------
//
// Forward declarations
//
void setNightMode(bool on);
void toggleNightMode();

// ---------------------------------------------------------------------------
// utilities

// number of elements in an array
#define countof(x) (sizeof(x)/sizeof((x)[0]))

// floating point square of a number
inline float square(float x) { return x*x; }

// floating point rounding
inline float round(float x) { return x > 0 ? floor(x + 0.5) : ceil(x - 0.5); }


// --------------------------------------------------------------------------
// 
// USB product version number
//
const uint16_t USB_VERSION_NO = 0x0008;

// --------------------------------------------------------------------------
//
// Joystick axis report range - we report from -JOYMAX to +JOYMAX
//
#define JOYMAX 4096


// ---------------------------------------------------------------------------
//
// Wire protocol value translations.  These translate byte values from
// the USB protocol to local native format.
//

// unsigned 16-bit integer 
inline uint16_t wireUI16(const uint8_t *b)
{
    return b[0] | ((uint16_t)b[1] << 8);
}

inline int16_t wireI16(const uint8_t *b)
{
    return (int16_t)wireUI16(b);
}

inline uint32_t wireUI32(const uint8_t *b)
{
    return b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

inline int32_t wireI32(const uint8_t *b)
{
    return (int32_t)wireUI32(b);
}

inline PinName wirePinName(int c)
{
    static const PinName p[] =  {
        NC,    PTA1,  PTA2,  PTA4,  PTA5,  PTA12, PTA13, PTA16, PTA17, PTB0,    // 0-9
        PTB1,  PTB2,  PTB3,  PTB8,  PTB9,  PTB10, PTB11, PTB18, PTB19, PTC0,    // 10-19
        PTC1,  PTC2,  PTC3,  PTC4,  PTC5,  PTC6,  PTC7,  PTC8,  PTC9,  PTC10,   // 20-29
        PTC11, PTC12, PTC13, PTC16, PTC17, PTD0,  PTD1,  PTD2,  PTD3,  PTD4,    // 30-39
        PTD5,  PTD6,  PTD7,  PTE0,  PTE1,  PTE2,  PTE3,  PTE4,  PTE5,  PTE20,   // 40-49
        PTE21, PTE22, PTE23, PTE29, PTE30, PTE31                                // 50-55
    };
    return (c < countof(p) ? p[c] : NC);
}


// ---------------------------------------------------------------------------
//
// On-board RGB LED elements - we use these for diagnostic displays.
//
// Note that LED3 (the blue segment) is hard-wired on the KL25Z to PTD1,
// so PTD1 shouldn't be used for any other purpose (e.g., as a keyboard
// input or a device output).  This is kind of unfortunate in that it's 
// one of only two ports exposed on the jumper pins that can be muxed to 
// SPI0 SCLK.  This effectively limits us to PTC5 if we want to use the 
// SPI capability.
//
DigitalOut *ledR, *ledG, *ledB;

// Show the indicated pattern on the diagnostic LEDs.  0 is off, 1 is
// on, and -1 is no change (leaves the current setting intact).
void diagLED(int r, int g, int b)
{
    if (ledR != 0 && r != -1) ledR->write(!r);
    if (ledG != 0 && g != -1) ledG->write(!g);
    if (ledB != 0 && b != -1) ledB->write(!b);
}

// check an output port assignment to see if it conflicts with
// an on-board LED segment
struct LedSeg 
{ 
    bool r, g, b; 
    LedSeg() { r = g = b = false; } 

    void check(LedWizPortCfg &pc)
    {
        // if it's a GPIO, check to see if it's assigned to one of
        // our on-board LED segments
        int t = pc.typ;
        if (t == PortTypeGPIOPWM || t == PortTypeGPIODig)
        {
            // it's a GPIO port - check for a matching pin assignment
            PinName pin = wirePinName(pc.pin);
            if (pin == LED1)
                r = true;
            else if (pin == LED2)
                g = true;
            else if (pin == LED3)
                b = true;
        }
    }
};

// Initialize the diagnostic LEDs.  By default, we use the on-board
// RGB LED to display the microcontroller status.  However, we allow
// the user to commandeer the on-board LED as an LedWiz output device,
// which can be useful for testing a new installation.  So we'll check
// for LedWiz outputs assigned to the on-board LED segments, and turn
// off the diagnostic use for any so assigned.
void initDiagLEDs(Config &cfg)
{
    // run through the configuration list and cross off any of the
    // LED segments assigned to LedWiz ports
    LedSeg l;
    for (int i = 0 ; i < MAX_OUT_PORTS && cfg.outPort[i].typ != PortTypeDisabled ; ++i)
        l.check(cfg.outPort[i]);
    
    // check the special ports
    for (int i = 0 ; i < countof(cfg.specialPort) ; ++i)
        l.check(cfg.specialPort[i]);
    
    // We now know which segments are taken for LedWiz use and which
    // are free.  Create diagnostic ports for the ones not claimed for
    // LedWiz use.
    if (!l.r) ledR = new DigitalOut(LED1, 1);
    if (!l.g) ledG = new DigitalOut(LED2, 1);
    if (!l.b) ledB = new DigitalOut(LED3, 1);
}


// ---------------------------------------------------------------------------
//
// LedWiz emulation, and enhanced TLC5940 output controller
//
// There are two modes for this feature.  The default mode uses the on-board
// GPIO ports to implement device outputs - each LedWiz software port is
// connected to a physical GPIO pin on the KL25Z.  The KL25Z only has 10
// PWM channels, so in this mode only 10 LedWiz ports will be dimmable; the
// rest are strictly on/off.  The KL25Z also has a limited number of GPIO
// ports overall - not enough for the full complement of 32 LedWiz ports
// and 24 VP joystick inputs, so it's necessary to trade one against the
// other if both features are to be used.
//
// The alternative, enhanced mode uses external TLC5940 PWM controller
// chips to control device outputs.  In this mode, each LedWiz software
// port is mapped to an output on one of the external TLC5940 chips.
// Two 5940s is enough for the full set of 32 LedWiz ports, and we can
// support even more chips for even more outputs (although doing so requires
// breaking LedWiz compatibility, since the LedWiz USB protocol is hardwired
// for 32 outputs).  Every port in this mode has full PWM support.
//


// Current starting output index for "PBA" messages from the PC (using
// the LedWiz USB protocol).  Each PBA message implicitly uses the
// current index as the starting point for the ports referenced in
// the message, and increases it (by 8) for the next call.
static int pbaIdx = 0;

// Generic LedWiz output port interface.  We create a cover class to 
// virtualize digital vs PWM outputs, and on-board KL25Z GPIO vs external 
// TLC5940 outputs, and give them all a common interface.  
class LwOut
{
public:
    // Set the output intensity.  'val' is 0.0 for fully off, 1.0 for
    // fully on, and fractional values for intermediate intensities.
    virtual void set(float val) = 0;
};

// LwOut class for virtual ports.  This type of port is visible to
// the host software, but isn't connected to any physical output.
// This can be used for special software-only ports like the ZB
// Launch Ball output, or simply for placeholders in the LedWiz port
// numbering.
class LwVirtualOut: public LwOut
{
public:
    LwVirtualOut() { }
    virtual void set(float val) { }
};

// Active Low out.  For any output marked as active low, we layer this
// on top of the physical pin interface.  This simply inverts the value of
// the output value, so that 1.0 means fully off and 0.0 means fully on.
class LwInvertedOut: public LwOut
{
public:
    LwInvertedOut(LwOut *o) : out(o) { }
    virtual void set(float val) { out->set(1.0 - val); }
    
private:
    LwOut *out;
};


//
// The TLC5940 interface object.  We'll set this up with the port 
// assignments set in config.h.
//
TLC5940 *tlc5940 = 0;
void init_tlc5940(Config &cfg)
{
    if (cfg.tlc5940.nchips != 0)
    {
        tlc5940 = new TLC5940(cfg.tlc5940.sclk, cfg.tlc5940.sin, cfg.tlc5940.gsclk,
            cfg.tlc5940.blank, cfg.tlc5940.xlat, cfg.tlc5940.nchips);
    }
}

// LwOut class for TLC5940 outputs.  These are fully PWM capable.
// The 'idx' value in the constructor is the output index in the
// daisy-chained TLC5940 array.  0 is output #0 on the first chip,
// 1 is #1 on the first chip, 15 is #15 on the first chip, 16 is
// #0 on the second chip, 32 is #0 on the third chip, etc.
class Lw5940Out: public LwOut
{
public:
    Lw5940Out(int idx) : idx(idx) { prv = -1; }
    virtual void set(float val)
    {
        if (val != prv)
           tlc5940->set(idx, (int)((prv = val) * 4095.0f));
    }
    int idx;
    float prv;
};


// 74HC595 interface object.  Set this up with the port assignments in
// config.h.
HC595 *hc595 = 0;

// initialize the 74HC595 interface
void init_hc595(Config &cfg)
{
    if (cfg.hc595.nchips != 0)
    {
        hc595 = new HC595(cfg.hc595.nchips, cfg.hc595.sin, cfg.hc595.sclk, cfg.hc595.latch, cfg.hc595.ena);
        hc595->init();
        hc595->update();
    }
}

// LwOut class for 74HC595 outputs.  These are simple digial outs.
// The 'idx' value in the constructor is the output index in the
// daisy-chained 74HC595 array.  0 is output #0 on the first chip,
// 1 is #1 on the first chip, 7 is #7 on the first chip, 8 is
// #0 on the second chip, etc.
class Lw595Out: public LwOut
{
public:
    Lw595Out(int idx) : idx(idx) { prv = -1; }
    virtual void set(float val)
    {
        if (val != prv)
           hc595->set(idx, (prv = val) == 0.0 ? 0 : 1);
    }
    int idx;
    float prv;
};


// 
// Default LedWiz mode - using on-board GPIO ports.  In this mode, we
// assign a KL25Z GPIO port to each LedWiz output.  We have to use a
// mix of PWM-capable and Digital-Only ports in this configuration, 
// since the KL25Z hardware only has 10 PWM channels, which isn't
// enough to fill out the full complement of 32 LedWiz outputs.
//

// LwOut class for a PWM-capable GPIO port
class LwPwmOut: public LwOut
{
public:
    LwPwmOut(PinName pin) : p(pin) { prv = -1; }
    virtual void set(float val) 
    { 
        if (val != prv)
            p.write(prv = val); 
    }
    PwmOut p;
    float prv;
};

// LwOut class for a Digital-Only (Non-PWM) GPIO port
class LwDigOut: public LwOut
{
public:
    LwDigOut(PinName pin) : p(pin) { prv = -1; }
    virtual void set(float val) 
    {
         if (val != prv)
            p.write((prv = val) == 0.0 ? 0 : 1); 
    }
    DigitalOut p;
    float prv;
};

// Array of output physical pin assignments.  This array is indexed
// by LedWiz logical port number - lwPin[n] is the maping for LedWiz
// port n (0-based).  
//
// Each pin is handled by an interface object for the physical output 
// type for the port, as set in the configuration.  The interface 
// objects handle the specifics of addressing the different hardware
// types (GPIO PWM ports, GPIO digital ports, TLC5940 ports, and
// 74HC595 ports).
static int numOutputs;
static LwOut **lwPin;

// Special output ports:
//
//    [0] = Night Mode indicator light
//
static LwOut *specialPin[1];


// Number of LedWiz emulation outputs.  This is the number of ports
// accessible through the standard (non-extended) LedWiz protocol
// messages.  The protocol has a fixed set of 32 outputs, but we
// might have fewer actual outputs.  This is therefore set to the
// lower of 32 or the actual number of outputs.
static int numLwOutputs;

// Current absolute brightness level for an output.  This is a float
// value from 0.0 for fully off to 1.0 for fully on.  This is used
// for all extended ports (33 and above), and for any LedWiz port
// with wizVal == 255.
static float *outLevel;

// Day/night mode override for an output.  For each output, this is
// set to 1 if the output is enabled and 0 if the output is disabled
// by a global mode control, such as Night Mode (currently Night Mode
// is the only such global mode, but the idea could be extended to
// other similar controls if other needs emerge).  To get the final
// output level for each output, we simply multiply the outLevel value
// for the port by this override vlaue.
static uint8_t *modeLevel;

// create a single output pin
LwOut *createLwPin(LedWizPortCfg &pc, Config &cfg)
{
    // get this item's values
    int typ = pc.typ;
    int pin = pc.pin;
    int flags = pc.flags;
    int activeLow = flags & PortFlagActiveLow;

    // create the pin interface object according to the port type        
    LwOut *lwp;
    switch (typ)
    {
    case PortTypeGPIOPWM:
        // PWM GPIO port
        lwp = new LwPwmOut(wirePinName(pin));
        break;
    
    case PortTypeGPIODig:
        // Digital GPIO port
        lwp = new LwDigOut(wirePinName(pin));
        break;
    
    case PortTypeTLC5940:
        // TLC5940 port (if we don't have a TLC controller object, or it's not a valid
        // output port number on the chips we have, create a virtual port)
        if (tlc5940 != 0 && pin < cfg.tlc5940.nchips*16)
            lwp = new Lw5940Out(pin);
        else
            lwp = new LwVirtualOut();
        break;
    
    case PortType74HC595:
        // 74HC595 port (if we don't have an HC595 controller object, or it's not a valid
        // output number, create a virtual port)
        if (hc595 != 0 && pin < cfg.hc595.nchips*8)
            lwp = new Lw595Out(pin);
        else
            lwp = new LwVirtualOut();
        break;

    case PortTypeVirtual:
    default:
        // virtual or unknown
        lwp = new LwVirtualOut();
        break;
    }
    
    // if it's Active Low, layer on an inverter
    if (activeLow)
        lwp = new LwInvertedOut(lwp);

    // turn it off initially      
    lwp->set(0);
    
    // return the pin
    return lwp;
}

// initialize the output pin array
void initLwOut(Config &cfg)
{
    // Count the outputs.  The first disabled output determines the
    // total number of ports.
    numOutputs = MAX_OUT_PORTS;
    int i;
    for (i = 0 ; i < MAX_OUT_PORTS ; ++i)
    {
        if (cfg.outPort[i].typ == PortTypeDisabled)
        {
            numOutputs = i;
            break;
        }
    }
    
    // the real LedWiz protocol can access at most 32 ports, or the
    // actual number of outputs, whichever is lower
    numLwOutputs = (numOutputs < 32 ? numOutputs : 32);
    
    // allocate the pin array
    lwPin = new LwOut*[numOutputs];    
    
    // Allocate the current brightness array.  For these, allocate at
    // least 32, so that we have enough for all LedWiz messages, but
    // allocate the full set of actual ports if we have more than the
    // LedWiz complement.
    int minOuts = numOutputs < 32 ? 32 : numOutputs;
    outLevel = new float[minOuts];
    
    // Allocate the mode override array
    modeLevel = new uint8_t[minOuts];
    
    // start with all modeLevel values set to ON
    memset(modeLevel, 1, minOuts);
    
    // create the pin interface object for each port
    for (i = 0 ; i < numOutputs ; ++i)
        lwPin[i] = createLwPin(cfg.outPort[i], cfg);
        
    // create the pin interface for each special port
    for (i = 0 ; i < countof(cfg.specialPort) ; ++i)
        specialPin[i] = createLwPin(cfg.specialPort[i], cfg);
}

// LedWiz output states.
//
// The LedWiz protocol has two separate control axes for each output.
// One axis is its on/off state; the other is its "profile" state, which
// is either a fixed brightness or a blinking pattern for the light.
// The two axes are independent.
//
// Note that the LedWiz protocol can only address 32 outputs, so the
// wizOn and wizVal arrays have fixed sizes of 32 elements no matter
// how many physical outputs we're using.

// on/off state for each LedWiz output
static uint8_t wizOn[32];

// Profile (brightness/blink) state for each LedWiz output.  If the
// output was last updated through an LedWiz protocol message, it
// will have one of these values:
//
//   0-48 = fixed brightness 0% to 100%
//   129 = ramp up / ramp down
//   130 = flash on / off
//   131 = on / ramp down
//   132 = ramp up / on
//
// Special value 255:  If the output was updated through the 
// extended protocol, we'll set the wizVal entry to 255, which has 
// no meaning in the LedWiz protocol.  This tells us that the value 
// in outLevel[] was set directly from the extended protocol, so it 
// shouldn't be derived from wizVal[].
//
static uint8_t wizVal[32] = {
    48, 48, 48, 48, 48, 48, 48, 48,
    48, 48, 48, 48, 48, 48, 48, 48,
    48, 48, 48, 48, 48, 48, 48, 48,
    48, 48, 48, 48, 48, 48, 48, 48
};

// LedWiz flash speed.  This is a value from 1 to 7 giving the pulse
// rate for lights in blinking states.
static uint8_t wizSpeed = 2;

// Current LedWiz flash cycle counter.
static uint8_t wizFlashCounter = 0;

// Get the current brightness level for an LedWiz output.
static float wizState(int idx)
{
    // if the output was last set with an extended protocol message,
    // use the value set there, ignoring the output's LedWiz state
    if (wizVal[idx] == 255)
        return outLevel[idx];
    
    // if it's off, show at zero intensity
    if (!wizOn[idx])
        return 0;

    // check the state
    uint8_t val = wizVal[idx];
    if (val <= 48)
    {
        // PWM brightness/intensity level.  Rescale from the LedWiz
        // 0..48 integer range to our internal PwmOut 0..1 float range.
        // Note that on the actual LedWiz, level 48 is actually about
        // 98% on - contrary to the LedWiz documentation, level 49 is 
        // the true 100% level.  (In the documentation, level 49 is
        // simply not a valid setting.)  Even so, we treat level 48 as
        // 100% on to match the documentation.  This won't be perfectly
        // ocmpatible with the actual LedWiz, but it makes for such a
        // small difference in brightness (if the output device is an
        // LED, say) that no one should notice.  It seems better to
        // err in this direction, because while the difference in
        // brightness when attached to an LED won't be noticeable, the
        // difference in duty cycle when attached to something like a
        // contactor *can* be noticeable - anything less than 100%
        // can cause a contactor or relay to chatter.  There's almost
        // never a situation where you'd want values other than 0% and
        // 100% for a contactor or relay, so treating level 48 as 100%
        // makes us work properly with software that's expecting the
        // documented LedWiz behavior and therefore uses level 48 to
        // turn a contactor or relay fully on.
        return val/48.0f;
    }
    else if (val == 49)
    {
        // 49 is undefined in the LedWiz documentation, but actually
        // means 100% on.  The documentation says that levels 1-48 are
        // the full PWM range, but empirically it appears that the real
        // range implemented in the firmware is 1-49.  Some software on
        // the PC side (notably DOF) is aware of this and uses level 49
        // to mean "100% on".  To ensure compatibility with existing 
        // PC-side software, we need to recognize level 49.
        return 1.0f;
    }
    else if (val == 129)
    {
        //   129 = ramp up / ramp down
        return wizFlashCounter < 128 
            ? wizFlashCounter/128.0f 
            : (256 - wizFlashCounter)/128.0f;
    }
    else if (val == 130)
    {
        //   130 = flash on / off
        return wizFlashCounter < 128 ? 1.0f : 0.0f;
    }
    else if (val == 131)
    {
        //   131 = on / ramp down
        return wizFlashCounter < 128 ? 1.0f : (255 - wizFlashCounter)/128.0f;
    }
    else if (val == 132)
    {
        //   132 = ramp up / on
        return wizFlashCounter < 128 ? wizFlashCounter/128.0f : 1.0f;
    }
    else
    {
        // Other values are undefined in the LedWiz documentation.  Hosts
        // *should* never send undefined values, since whatever behavior an
        // LedWiz unit exhibits in response is accidental and could change
        // in a future version.  We'll treat all undefined values as equivalent 
        // to 48 (fully on).
        return 1.0f;
    }
}

// LedWiz flash timer pulse.  This fires periodically to update 
// LedWiz flashing outputs.  At the slowest pulse speed set via
// the SBA command, each waveform cycle has 256 steps, so we
// choose the pulse time base so that the slowest cycle completes
// in 2 seconds.  This seems to roughly match the real LedWiz
// behavior.  We run the pulse timer at the same rate regardless
// of the pulse speed; at higher pulse speeds, we simply use
// larger steps through the cycle on each interrupt.  Running
// every 1/127 of a second = 8ms seems to be a pretty light load.
Timeout wizPulseTimer;
#define WIZ_PULSE_TIME_BASE  (1.0f/127.0f)
static void wizPulse()
{
    // increase the counter by the speed increment, and wrap at 256
    wizFlashCounter += wizSpeed;
    wizFlashCounter &= 0xff;
    
    // if we have any flashing lights, update them
    int ena = false;
    for (int i = 0 ; i < numLwOutputs ; ++i)
    {
        if (wizOn[i])
        {
            uint8_t s = wizVal[i];
            if (s >= 129 && s <= 132)
            {
                lwPin[i]->set(wizState(i) * modeLevel[i]);
                ena = true;
            }
        }
    }    

    // Set up the next timer pulse only if we found anything flashing.
    // To minimize overhead from this feature, we only enable the interrupt
    // when we need it.  This eliminates any performance penalty to other
    // features when the host software doesn't care about the flashing 
    // modes.  For example, DOF never uses these modes, so there's no 
    // need for them when running Visual Pinball.
    if (ena)
        wizPulseTimer.attach(wizPulse, WIZ_PULSE_TIME_BASE);
}

// Update the physical outputs connected to the LedWiz ports.  This is 
// called after any update from an LedWiz protocol message.
static void updateWizOuts()
{
    // update each output
    int pulse = false;
    for (int i = 0 ; i < numLwOutputs ; ++i)
    {
        pulse |= (wizVal[i] >= 129 && wizVal[i] <= 132);
        lwPin[i]->set(wizState(i) * modeLevel[i]);
    }
    
    // if any outputs are set to flashing mode, and the pulse timer
    // isn't running, turn it on
    if (pulse)
        wizPulseTimer.attach(wizPulse, WIZ_PULSE_TIME_BASE);
        
    // flush changes to 74HC595 chips, if attached
    if (hc595 != 0)
        hc595->update();
}

// Update all physical outputs.  This is called after a change to a global
// setting that affects all outputs, such as engaging or canceling Night Mode.
static void updateAllOuts()
{
    // uddate each LedWiz output
    for (int i = 0 ; i < numLwOutputs ; ++i)
        lwPin[i]->set(wizState(i) * modeLevel[i]);
        
    // update each extended output
    for (int i = 33 ; i < numOutputs ; ++i)
        lwPin[i]->set(outLevel[i] * modeLevel[i]);
        
    // flush 74HC595 changes, if necessary
    if (hc595 != 0)
        hc595->update();
}

// ---------------------------------------------------------------------------
//
// Button input
//

// button state
struct ButtonState
{
    ButtonState()
    {
        di = NULL;
        on = 0;
        pressed = prev = 0;
        dbstate = 0;
        js = 0;
        keymod = 0;
        keycode = 0;
        special = 0;
        pulseState = 0;
        pulseTime = 0.0f;
    }
    
    // DigitalIn for the button
    DigitalIn *di;
    
    // current PHYSICAL on/off state, after debouncing
    uint8_t on;
    
    // current LOGICAL on/off state as reported to the host.
    uint8_t pressed;

    // previous logical on/off state, when keys were last processed for USB 
    // reports and local effects
    uint8_t prev;
    
    // Debounce history.  On each scan, we shift in a 1 bit to the lsb if
    // the physical key is reporting ON, and shift in a 0 bit if the physical
    // key is reporting OFF.  We consider the key to have a new stable state
    // if we have N consecutive 0's or 1's in the low N bits (where N is
    // a parameter that determines how long we wait for transients to settle).
    uint8_t dbstate;
    
    // joystick button mask for the button, if mapped as a joystick button
    uint32_t js;
    
    // keyboard modifier bits and scan code for the button, if mapped as a keyboard key
    uint8_t keymod;
    uint8_t keycode;
    
    // media control key code
    uint8_t mediakey;
    
    // special key code
    uint8_t special;
    
    // Pulse mode: a button in pulse mode transmits a brief logical button press and
    // release each time the attached physical switch changes state.  This is useful
    // for cases where the host expects a key press for each change in the state of
    // the physical switch.  The canonical example is the Coin Door switch in VPinMAME, 
    // which requires pressing the END key to toggle the open/closed state.  This
    // software design isn't easily implemented in a physical coin door, though -
    // the easiest way to sense a physical coin door's state is with a simple on/off
    // switch.  Pulse mode bridges that divide by converting a physical switch state
    // to on/off toggle key reports to the host.
    //
    // Pulse state:
    //   0 -> not a pulse switch - logical key state equals physical switch state
    //   1 -> off
    //   2 -> transitioning off-on
    //   3 -> on
    //   4 -> transitioning on-off
    //
    // Each state change sticks for a minimum period; when the timer expires,
    // if the underlying physical switch is in a different state, we switch
    // to the next state and restart the timer.  pulseTime is the amount of
    // time remaining before we can make another state transition.  The state
    // transitions require a complete cycle, 1 -> 2 -> 3 -> 4 -> 1...; this
    // guarantees that the parity of the pulse count always matches the 
    // current physical switch state when the latter is stable, which makes
    // it impossible to "trick" the host by rapidly toggling the switch state.
    // (On my original Pinscape cabinet, I had a hardware pulse generator
    // for coin door, and that *was* possible to trick by rapid toggling.
    // This software system can't be fooled that way.)
    uint8_t pulseState;
    float pulseTime;
    
} buttonState[MAX_BUTTONS];


// Button data
uint32_t jsButtons = 0;

// Keyboard report state.  This tracks the USB keyboard state.  We can
// report at most 6 simultaneous non-modifier keys here, plus the 8
// modifier keys.
struct
{
    bool changed;       // flag: changed since last report sent
    int nkeys;          // number of active keys in the list
    uint8_t data[8];    // key state, in USB report format: byte 0 is the modifier key mask,
                        // byte 1 is reserved, and bytes 2-7 are the currently pressed key codes
} kbState = { false, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };

// Media key state
struct
{
    bool changed;       // flag: changed since last report sent
    uint8_t data;       // key state byte for USB reports
} mediaState = { false, 0 };

// button scan interrupt ticker
Ticker buttonTicker;

// Button scan interrupt handler.  We call this periodically via
// a timer interrupt to scan the physical button states.  
void scanButtons()
{
    // scan all button input pins
    ButtonState *bs = buttonState;
    for (int i = 0 ; i < MAX_BUTTONS ; ++i, ++bs)
    {
        // if it's connected, check its physical state
        if (bs->di != NULL)
        {
            // Shift the new state into the debounce history.  Note that
            // the physical pin inputs are active low (0V/GND = ON), so invert 
            // the reading by XOR'ing the low bit with 1.  And of course we
            // only want the low bit (since the history is effectively a bit
            // vector), so mask the whole thing with 0x01 as well.
            uint8_t db = bs->dbstate;
            db <<= 1;
            db |= (bs->di->read() & 0x01) ^ 0x01;
            bs->dbstate = db;
            
            // if we have all 0's or 1's in the history for the required
            // debounce period, the key state is stable - check for a change
            // to the last stable state
            const uint8_t stable = 0x1F;   // 00011111b -> 5 stable readings
            db &= stable;
            if (db == 0 || db == stable)
                bs->on = db;
        }
    }
}

// Button state transition timer.  This is used for pulse buttons, to
// control the timing of the logical key presses generated by transitions
// in the physical button state.
Timer buttonTimer;

// initialize the button inputs
void initButtons(Config &cfg, bool &kbKeys)
{
    // presume we'll find no keyboard keys
    kbKeys = false;
    
    // create the digital inputs
    ButtonState *bs = buttonState;
    for (int i = 0 ; i < MAX_BUTTONS ; ++i, ++bs)
    {
        PinName pin = wirePinName(cfg.button[i].pin);
        if (pin != NC)
        {
            // set up the GPIO input pin for this button
            bs->di = new DigitalIn(pin);
            
            // if it's a pulse mode button, set the initial pulse state to Off
            if (cfg.button[i].flags & BtnFlagPulse)
                bs->pulseState = 1;
            
            // note if it's a keyboard key of some kind (including media keys)
            uint8_t val = cfg.button[i].val;
            switch (cfg.button[i].typ)
            {
            case BtnTypeJoystick:
                // joystick button - get the button bit mask
                bs->js = 1 << val;
                break;
                
            case BtnTypeKey:
                // regular keyboard key - note the scan code
                bs->keycode = val;
                kbKeys = true;
                break;
                
            case BtnTypeModKey:
                // keyboard mod key - note the modifier mask
                bs->keymod = val;
                kbKeys = true;
                break;
                
            case BtnTypeMedia:
                // media key - note the code
                bs->mediakey = val;
                kbKeys = true;
                break;
                
            case BtnTypeSpecial:
                // special key
                bs->special = val;
                break;
            }
        }
    }
    
    // start the button scan thread
    buttonTicker.attach_us(scanButtons, 1000);

    // start the button state transition timer
    buttonTimer.start();
}

// Process the button state.  This sets up the joystick, keyboard, and
// media control descriptors with the current state of keys mapped to
// those HID interfaces, and executes the local effects for any keys 
// mapped to special device functions (e.g., Night Mode).
void processButtons()
{
    // start with an empty list of USB key codes
    uint8_t modkeys = 0;
    uint8_t keys[7] = { 0, 0, 0, 0, 0, 0, 0 };
    int nkeys = 0;
    
    // clear the joystick buttons
    uint32_t newjs = 0;
    
    // start with no media keys pressed
    uint8_t mediakeys = 0;
    
    // calculate the time since the last run
    float dt = buttonTimer.read();
    buttonTimer.reset();

    // scan the button list
    ButtonState *bs = buttonState;
    for (int i = 0 ; i < MAX_BUTTONS ; ++i, ++bs)
    {
        // if it's a pulse-mode switch, get the virtual pressed state
        if (bs->pulseState != 0)
        {
            // deduct the time to the next state change
            bs->pulseTime -= dt;
            if (bs->pulseTime < 0)
                bs->pulseTime = 0;
                
            // if the timer has expired, check for state changes
            if (bs->pulseTime == 0)
            {
                const float pulseLength = 0.2;
                switch (bs->pulseState)
                {
                case 1:
                    // off - if the physical switch is now on, start a button pulse
                    if (bs->on) {
                        bs->pulseTime = pulseLength;
                        bs->pulseState = 2;
                        bs->pressed = 1;
                    }
                    break;
                    
                case 2:
                    // transitioning off to on - end the pulse, and start a gap
                    // equal to the pulse time so that the host can observe the
                    // change in state in the logical button
                    bs->pulseState = 3;
                    bs->pulseTime = pulseLength;
                    bs->pressed = 0;
                    break;
                    
                case 3:
                    // on - if the physical switch is now off, start a button pulse
                    if (!bs->on) {
                        bs->pulseTime = pulseLength;
                        bs->pulseState = 4;
                        bs->pressed = 1;
                    }
                    break;
                    
                case 4:
                    // transitioning on to off - end the pulse, and start a gap
                    bs->pulseState = 1;
                    bs->pulseTime = pulseLength;
                    bs->pressed = 0;
                    break;
                }
            }
        }
        else
        {
            // not a pulse switch - the logical state is the same as the physical state
            bs->pressed = bs->on;
        }

        // carry out any edge effects from buttons changing states
        if (bs->pressed != bs->prev)
        {
            // check for special key transitions
            switch (bs->special)
            {
            case 1:
                // night mode momentary switch - when the button transitions from
                // OFF to ON, invert night mode
                if (bs->pressed)
                    toggleNightMode();
                break;
                
            case 2:
                // night mode toggle switch - when the button changes state, change
                // night mode to match the new state
                setNightMode(bs->pressed);
                break;
            }
            
            // remember the new state for comparison on the next run
            bs->prev = bs->pressed;
        }

        // if it's pressed, add it to the appropriate key state list
        if (bs->pressed)
        {
            // OR in the joystick button bit, mod key bits, and media key bits
            newjs |= bs->js;
            modkeys |= bs->keymod;
            mediakeys |= bs->mediakey;
            
            // if it has a keyboard key, add the scan code to the active list
            if (bs->keycode != 0 && nkeys < 7)
                keys[nkeys++] = bs->keycode;
        }
    }

    // check for joystick button changes
    if (jsButtons != newjs)
        jsButtons = newjs;
    
    // Check for changes to the keyboard keys
    if (kbState.data[0] != modkeys
        || kbState.nkeys != nkeys
        || memcmp(keys, &kbState.data[2], 6) != 0)
    {
        // we have changes - set the change flag and store the new key data
        kbState.changed = true;
        kbState.data[0] = modkeys;
        if (nkeys <= 6) {
            // 6 or fewer simultaneous keys - report the key codes
            kbState.nkeys = nkeys;
            memcpy(&kbState.data[2], keys, 6);
        }
        else {
            // more than 6 simultaneous keys - report rollover (all '1' key codes)
            kbState.nkeys = 6;
            memset(&kbState.data[2], 1, 6);
        }
    }        
    
    // Check for changes to media keys
    if (mediaState.data != mediakeys)
    {
        mediaState.changed = true;
        mediaState.data = mediakeys;
    }
}

// ---------------------------------------------------------------------------
//
// Customization joystick subbclass
//

class MyUSBJoystick: public USBJoystick
{
public:
    MyUSBJoystick(uint16_t vendor_id, uint16_t product_id, uint16_t product_release,
        bool waitForConnect, bool enableJoystick, bool useKB) 
        : USBJoystick(vendor_id, product_id, product_release, waitForConnect, enableJoystick, useKB)
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

// I2C address of the accelerometer (this is a constant of the KL25Z)
const int MMA8451_I2C_ADDRESS = (0x1d<<1);

// SCL and SDA pins for the accelerometer (constant for the KL25Z)
#define MMA8451_SCL_PIN   PTE25
#define MMA8451_SDA_PIN   PTE24

// Digital in pin to use for the accelerometer interrupt.  For the KL25Z,
// this can be either PTA14 or PTA15, since those are the pins physically
// wired on this board to the MMA8451 interrupt controller.
#define MMA8451_INT_PIN   PTA15


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
    
    void get(int &x, int &y) 
    {
         // disable interrupts while manipulating the shared data
         __disable_irq();
         
         // read the shared data and store locally for calculations
         float ax = ax_, ay = ay_;
         float vx = vx_, vy = vy_;
         
         // reset the velocity sum for the next run
         vx_ = vy_ = 0;

         // get the time since the last get() sample
         float dt = tGet_.read_us()/1.0e6f;
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
             
             // If we haven't seen an interrupt in a while, do an explicit read to
             // "unstick" the device.  The device can become stuck - which is to say,
             // it will stop delivering data-ready interrupts - if we fail to service
             // one data-ready interrupt before the next one occurs.  Reading a sample
             // will clear up this overrun condition and allow normal interrupt
             // generation to continue.
             //
             // Note that this stuck condition *shouldn't* ever occur - if it does,
             // it means that we're spending a long period with interrupts disabled
             // (either in a critical section or in another interrupt handler), which
             // will likely cause other worse problems beyond the sticky accelerometer.
             // Even so, it's easy to detect and correct, so we'll do so for the sake
             // of making the system more fault-tolerant.
             if (tInt_.read() > 1.0f)
             {
                 printf("unwedging the accelerometer\r\n");
                float x, y, z;
                mma_.getAccXYZ(x, y, z);
             }
         }
         
         // report our integrated velocity reading in x,y
         x = rawToReport(vx);
         y = rawToReport(vy);
         
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
        float dt = tInt_.read();
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
// Clear the I2C bus for the MMA8451Q.  This seems necessary some of the time
// for reasons that aren't clear to me.  Doing a hard power cycle has the same
// effect, but when we do a soft reset, the hardware sometimes seems to leave
// the MMA's SDA line stuck low.  Forcing a series of 9 clock pulses through
// the SCL line is supposed to clear this condition.  I'm not convinced this
// actually works with the way this component is wired on the KL25Z, but it
// seems harmless, so we'll do it on reset in case it does some good.  What
// we really seem to need is a way to power cycle the MMA8451Q if it ever 
// gets stuck, but this is simply not possible in software on the KL25Z. 
// 
// If the accelerometer does get stuck, and a software reboot doesn't reset
// it, the only workaround is to manually power cycle the whole KL25Z by 
// unplugging both of its USB connections.
//
void clear_i2c()
{
    // set up general-purpose output pins to the I2C lines
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
// Simple binary (on/off) input debouncer.  Requires an input to be stable 
// for a given interval before allowing an update.
//
class Debouncer
{
public:
    Debouncer(bool initVal, float tmin)
    {
        t.start();
        this->stable = this->prv = initVal;
        this->tmin = tmin;
    }
    
    // Get the current stable value
    bool val() const { return stable; }

    // Apply a new sample.  This tells us the new raw reading from the
    // input device.
    void sampleIn(bool val)
    {
        // If the new raw reading is different from the previous
        // raw reading, we've detected an edge - start the clock
        // on the sample reader.
        if (val != prv)
        {
            // we have an edge - reset the sample clock
            t.reset();
            
            // this is now the previous raw sample for nxt time
            prv = val;
        }
        else if (val != stable)
        {
            // The new raw sample is the same as the last raw sample,
            // and different from the stable value.  This means that
            // the sample value has been the same for the time currently
            // indicated by our timer.  If enough time has elapsed to
            // consider the value stable, apply the new value.
            if (t.read() > tmin)
                stable = val;
        }
    }
    
private:
    // current stable value
    bool stable;

    // last raw sample value
    bool prv;
    
    // elapsed time since last raw input change
    Timer t;
    
    // Minimum time interval for stability, in seconds.  Input readings 
    // must be stable for this long before the stable value is updated.
    float tmin;
};


// ---------------------------------------------------------------------------
//
// Turn off all outputs and restore everything to the default LedWiz
// state.  This sets outputs #1-32 to LedWiz profile value 48 (full
// brightness) and switch state Off, sets all extended outputs (#33
// and above) to zero brightness, and sets the LedWiz flash rate to 2.
// This effectively restores the power-on conditions.
//
void allOutputsOff()
{
    // reset all LedWiz outputs to OFF/48
    for (int i = 0 ; i < numLwOutputs ; ++i)
    {
        outLevel[i] = 0;
        wizOn[i] = 0;
        wizVal[i] = 48;
        lwPin[i]->set(0);
    }
    
    // reset all extended outputs (ports >32) to full off (brightness 0)
    for (int i = 32 ; i < numOutputs ; ++i)
    {
        outLevel[i] = 0;
        lwPin[i]->set(0);
    }
    
    // restore default LedWiz flash rate
    wizSpeed = 2;
    
    // flush changes to hc595, if applicable
    if (hc595 != 0)
        hc595->update();
}

// ---------------------------------------------------------------------------
//
// TV ON timer.  If this feature is enabled, we toggle a TV power switch
// relay (connected to a GPIO pin) to turn on the cab's TV monitors shortly
// after the system is powered.  This is useful for TVs that don't remember
// their power state and don't turn back on automatically after being
// unplugged and plugged in again.  This feature requires external
// circuitry, which is built in to the expansion board and can also be
// built separately - see the Build Guide for the circuit plan.
//
// Theory of operation: to use this feature, the cabinet must have a 
// secondary PC-style power supply (PSU2) for the feedback devices, and
// this secondary supply must be plugged in to the same power strip or 
// switched outlet that controls power to the TVs.  This lets us use PSU2
// as a proxy for the TV power state - when PSU2 is on, the TV outlet is 
// powered, and when PSU2 is off, the TV outlet is off.  We use a little 
// latch circuit powered by PSU2 to monitor the status.  The latch has a 
// current state, ON or OFF, that we can read via a GPIO input pin, and 
// we can set the state to ON by pulsing a separate GPIO output pin.  As 
// long as PSU2 is powered off, the latch stays in the OFF state, even if 
// we try to set it by pulsing the SET pin.  When PSU2 is turned on after 
// being off, the latch starts receiving power but stays in the OFF state, 
// since this is the initial condition when the power first comes on.  So 
// if our latch state pin is reading OFF, we know that PSU2 is either off 
// now or *was* off some time since we last checked.  We use a timer to 
// check the state periodically.  Each time we see the state is OFF, we 
// try pulsing the SET pin.  If the state still reads as OFF, we know 
// that PSU2 is currently off; if the state changes to ON, though, we 
// know that PSU2 has gone from OFF to ON some time between now and the 
// previous check.  When we see this condition, we start a countdown
// timer, and pulse the TV switch relay when the countdown ends.
//
// This scheme might seem a little convoluted, but it neatly handles
// all of the different cases that can occur:
//
// - Most cabinets systems are set up with "soft" PC power switches, 
//   so that the PC goes into "Soft Off" mode (ACPI state S5, in Windows
//   parlance) when the user turns off the cabinet.  In this state, the
//   motherboard supplies power to USB devices, so the KL25Z continues
//   running without interruption.  The latch system lets us monitor
//   the power state even when we're never rebooted, since the latch
//   will turn off when PSU2 is off regardless of what the KL25Z is doing.
//
// - Some cabinet builders might prefer to use "hard" power switches,
//   cutting all power to the cabinet, including the PC motherboard (and
//   thus the KL25Z) every time the machine is turned off.  This also
//   applies to the "soft" switch case above when the cabinet is unplugged,
//   a power outage occurs, etc.  In these cases, the KL25Z will do a cold
//   boot when the PC is turned on.  We don't know whether the KL25Z
//   will power up before or after PSU2, so it's not good enough to 
//   observe the *current* state of PSU2 when we first check - if PSU2
//   were to come on first, checking the current state alone would fool
//   us into thinking that no action is required, because we would never
//   have known that PSU2 was ever off.  The latch handles this case by
//   letting us see that PSU2 *was* off before we checked.
//
// - If the KL25Z is rebooted while the main system is running, or the 
//   KL25Z is unplugged and plugged back in, we will correctly leave the 
//   TVs as they are.  The latch state is independent of the KL25Z's 
//   power or software state, so it's won't affect the latch state when
//   the KL25Z is unplugged or rebooted; when we boot, we'll see that 
//   the latch is already on and that we don't have to turn on the TVs.
//   This is important because TV ON buttons are usually on/off toggles,
//   so we don't want to push the button on a TV that's already on.
//   
//

// Current PSU2 state:
//   1 -> default: latch was on at last check, or we haven't checked yet
//   2 -> latch was off at last check, SET pulsed high
//   3 -> SET pulsed low, ready to check status
//   4 -> TV timer countdown in progress
//   5 -> TV relay on
//   
int psu2_state = 1;

// PSU2 power sensing circuit connections
DigitalIn *psu2_status_sense;
DigitalOut *psu2_status_set;

// TV ON switch relay control
DigitalOut *tv_relay;

// Timer interrupt
Ticker tv_ticker;
float tv_delay_time;
void TVTimerInt()
{
    // time since last state change
    static Timer tv_timer;

    // Check our internal state
    switch (psu2_state)
    {
    case 1:
        // Default state.  This means that the latch was on last
        // time we checked or that this is the first check.  In
        // either case, if the latch is off, switch to state 2 and
        // try pulsing the latch.  Next time we check, if the latch
        // stuck, it means that PSU2 is now on after being off.
        if (!psu2_status_sense->read())
        {
            // switch to OFF state
            psu2_state = 2;
            
            // try setting the latch
            psu2_status_set->write(1);
        }
        break;
        
    case 2:
        // PSU2 was off last time we checked, and we tried setting
        // the latch.  Drop the SET signal and go to CHECK state.
        psu2_status_set->write(0);
        psu2_state = 3;
        break;
        
    case 3:
        // CHECK state: we pulsed SET, and we're now ready to see
        // if that stuck.  If the latch is now on, PSU2 has transitioned
        // from OFF to ON, so start the TV countdown.  If the latch is
        // off, our SET command didn't stick, so PSU2 is still off.
        if (psu2_status_sense->read())
        {
            // The latch stuck, so PSU2 has transitioned from OFF
            // to ON.  Start the TV countdown timer.
            tv_timer.reset();
            tv_timer.start();
            psu2_state = 4;
        }
        else
        {
            // The latch didn't stick, so PSU2 was still off at
            // our last check.  Try pulsing it again in case PSU2
            // was turned on since the last check.
            psu2_status_set->write(1);
            psu2_state = 2;
        }
        break;
        
    case 4:
        // TV timer countdown in progress.  If we've reached the
        // delay time, pulse the relay.
        if (tv_timer.read() >= tv_delay_time)
        {
            // turn on the relay for one timer interval
            tv_relay->write(1);
            psu2_state = 5;
        }
        break;
        
    case 5:
        // TV timer relay on.  We pulse this for one interval, so
        // it's now time to turn it off and return to the default state.
        tv_relay->write(0);
        psu2_state = 1;
        break;
    }
}

// Start the TV ON checker.  If the status sense circuit is enabled in
// the configuration, we'll set up the pin connections and start the
// interrupt handler that periodically checks the status.  Does nothing
// if any of the pins are configured as NC.
void startTVTimer(Config &cfg)
{
    // only start the timer if the status sense circuit pins are configured
    if (cfg.TVON.statusPin != NC && cfg.TVON.latchPin != NC && cfg.TVON.relayPin != NC)
    {
        psu2_status_sense = new DigitalIn(cfg.TVON.statusPin);
        psu2_status_set = new DigitalOut(cfg.TVON.latchPin);
        tv_relay = new DigitalOut(cfg.TVON.relayPin);
        tv_delay_time = cfg.TVON.delayTime;
    
        // Set up our time routine to run every 1/4 second.  
        tv_ticker.attach(&TVTimerInt, 0.25);
    }
}

// ---------------------------------------------------------------------------
//
// In-memory configuration data structure.  This is the live version in RAM
// that we use to determine how things are set up.
//
// When we save the configuration settings, we copy this structure to
// non-volatile flash memory.  At startup, we check the flash location where
// we might have saved settings on a previous run, and it's valid, we copy 
// the flash data to this structure.  Firmware updates wipe the flash
// memory area, so you have to use the PC config tool to send the settings
// again each time the firmware is updated.
//
NVM nvm;

// For convenience, a macro for the Config part of the NVM structure
#define cfg (nvm.d.c)

// flash memory controller interface
FreescaleIAP iap;

// figure the flash address as a pointer along with the number of sectors
// required to store the structure
NVM *configFlashAddr(int &addr, int &numSectors)
{
    // figure how many flash sectors we span, rounding up to whole sectors
    numSectors = (sizeof(NVM) + SECTOR_SIZE - 1)/SECTOR_SIZE;

    // figure the address - this is the highest flash address where the
    // structure will fit with the start aligned on a sector boundary
    addr = iap.flash_size() - (numSectors * SECTOR_SIZE);
    
    // return the address as a pointer
    return (NVM *)addr;
}

// figure the flash address as a pointer
NVM *configFlashAddr()
{
    int addr, numSectors;
    return configFlashAddr(addr, numSectors);
}

// Load the config from flash
void loadConfigFromFlash()
{
    // We want to use the KL25Z's on-board flash to store our configuration
    // data persistently, so that we can restore it across power cycles.
    // Unfortunatly, the mbed platform doesn't explicitly support this.
    // mbed treats the on-board flash as a raw storage device for linker
    // output, and assumes that the linker output is the only thing
    // stored there.  There's no file system and no allowance for shared
    // use for other purposes.  Fortunately, the linker ues the space in
    // the obvious way, storing the entire linked program in a contiguous
    // block starting at the lowest flash address.  This means that the
    // rest of flash - from the end of the linked program to the highest
    // flash address - is all unused free space.  Writing our data there
    // won't conflict with anything else.  Since the linker doesn't give
    // us any programmatic access to the total linker output size, it's
    // safest to just store our config data at the very end of the flash
    // region (i.e., the highest address).  As long as it's smaller than
    // the free space, it won't collide with the linker area.
    
    // Figure how many sectors we need for our structure
    NVM *flash = configFlashAddr();
    
    // if the flash is valid, load it; otherwise initialize to defaults
    if (flash->valid()) 
    {
        // flash is valid - load it into the RAM copy of the structure
        memcpy(&nvm, flash, sizeof(NVM));
    }
    else 
    {
        // flash is invalid - load factory settings nito RAM structure
        cfg.setFactoryDefaults();
    }
}

void saveConfigToFlash()
{
    int addr, sectors;
    configFlashAddr(addr, sectors);
    nvm.save(iap, addr);
}

// ---------------------------------------------------------------------------
//
// NIGHT MODE flag.  When night mode is on, we disable all outputs
// marked as "noisemakers" in the output configuration flags.
int nightMode;

// Update the global output mode settings
static void globalOutputModeChange()
{
    // set the global modeLevel[] 
    for (int i = 0 ; i < numOutputs ; ++i)
    {
        // assume the port will be on
        uint8_t f = 1;
        
        // if night mode is in effect, and this is a noisemaker, disable it
        if (nightMode && (cfg.outPort[i].flags & PortFlagNoisemaker) != 0)
            f = 0;
            
        // set the final output port override value
        modeLevel[i] = f;
    }
    
    // update all outputs for the mode change
    updateAllOuts();
}

// Turn night mode on or off
static void setNightMode(bool on)
{
    nightMode = on;
    globalOutputModeChange();
    specialPin[0]->set(on ? 255.0 : 0.0);
}

// Toggle night mode
static void toggleNightMode()
{
    setNightMode(!nightMode);
}


// ---------------------------------------------------------------------------
//
// Plunger Sensor
//

// the plunger sensor interface object
PlungerSensor *plungerSensor = 0;

// Create the plunger sensor based on the current configuration.  If 
// there's already a sensor object, we'll delete it.
void createPlunger()
{
    // delete any existing sensor object
    if (plungerSensor != 0)
        delete plungerSensor;
        
    // create the new sensor object according to the type
    switch (cfg.plunger.sensorType)
    {
    case PlungerType_TSL1410RS:
        // pins are: SI, CLOCK, AO
        plungerSensor = new PlungerSensorTSL1410R(cfg.plunger.sensorPin[0], cfg.plunger.sensorPin[1], cfg.plunger.sensorPin[2], NC);
        break;
        
    case PlungerType_TSL1410RP:
        // pins are: SI, CLOCK, AO1, AO2
        plungerSensor = new PlungerSensorTSL1410R(cfg.plunger.sensorPin[0], cfg.plunger.sensorPin[1], cfg.plunger.sensorPin[2], cfg.plunger.sensorPin[3]);
        break;
        
    case PlungerType_TSL1412RS:
        // pins are: SI, CLOCK, AO1, AO2
        plungerSensor = new PlungerSensorTSL1412R(cfg.plunger.sensorPin[0], cfg.plunger.sensorPin[1], cfg.plunger.sensorPin[2], NC);
        break;
    
    case PlungerType_TSL1412RP:
        // pins are: SI, CLOCK, AO1, AO2
        plungerSensor = new PlungerSensorTSL1412R(cfg.plunger.sensorPin[0], cfg.plunger.sensorPin[1], cfg.plunger.sensorPin[2], cfg.plunger.sensorPin[3]);
        break;
    
    case PlungerType_Pot:
        // pins are: AO
        plungerSensor = new PlungerSensorPot(cfg.plunger.sensorPin[0]);
        break;
    
    case PlungerType_None:
    default:
        plungerSensor = new PlungerSensorNull();
        break;
    }
}

// ---------------------------------------------------------------------------
//
// Reboot - resets the microcontroller
//
void reboot(USBJoystick &js)
{
    // disconnect from USB
    js.disconnect();
    
    // wait a few seconds to make sure the host notices the disconnect
    wait(5);
    
    // reset the device
    NVIC_SystemReset();
    while (true) { }
}

// ---------------------------------------------------------------------------
//
// Translate joystick readings from raw values to reported values, based
// on the orientation of the controller card in the cabinet.
//
void accelRotate(int &x, int &y)
{
    int tmp;
    switch (cfg.orientation)
    {
    case OrientationFront:
        tmp = x;
        x = y;
        y = tmp;
        break;
    
    case OrientationLeft:
        x = -x;
        break;
    
    case OrientationRight:
        y = -y;
        break;
    
    case OrientationRear:
        tmp = -x;
        x = -y;
        y = tmp;
        break;
    }
}

// ---------------------------------------------------------------------------
//
// Device status.  We report this on each update so that the host config
// tool can detect our current settings.  This is a bit mask consisting
// of these bits:
//    0x0001  -> plunger sensor enabled
//    0x8000  -> RESERVED - must always be zero
//
// Note that the high bit (0x8000) must always be 0, since we use that
// to distinguish special request reply packets.
uint16_t statusFlags;
    
// flag: send a pixel dump after the next read
bool reportPix = false;


// ---------------------------------------------------------------------------
//
// Calibration button state:
//  0 = not pushed
//  1 = pushed, not yet debounced
//  2 = pushed, debounced, waiting for hold time
//  3 = pushed, hold time completed - in calibration mode
int calBtnState = 0;

// calibration button debounce timer
Timer calBtnTimer;

// calibration button light state
int calBtnLit = false;
    

// ---------------------------------------------------------------------------
//
// Handle a configuration variable update.  'data' is the USB message we
// received from the host.
//
void configVarMsg(uint8_t *data)
{
    switch (data[1])
    {
    case 1:
        // USB identification (Vendor ID, Product ID)
        cfg.usbVendorID = wireUI16(data+2);
        cfg.usbProductID = wireUI16(data+4);
        break;
        
    case 2:
        // Pinscape Controller unit number - note that data[2] contains
        // the nominal unit number, 1-16
        if (data[2] >= 1 && data[2] <= 16)
            cfg.psUnitNo = data[2];
        break;
        
    case 3:
        // Enable/disable joystick
        cfg.joystickEnabled = data[2];
        break;
        
    case 4:
        // Accelerometer orientation
        cfg.orientation = data[2];
        break;

    case 5:
        // Plunger sensor type
        cfg.plunger.sensorType = data[2];
        break;
        
    case 6:
        // Set plunger pin assignments
        cfg.plunger.sensorPin[0] = wirePinName(data[2]);
        cfg.plunger.sensorPin[1] = wirePinName(data[3]);
        cfg.plunger.sensorPin[2] = wirePinName(data[4]);
        cfg.plunger.sensorPin[3] = wirePinName(data[5]);
        break;
        
    case 7:
        // Plunger calibration button and indicator light pin assignments
        cfg.plunger.cal.btn = wirePinName(data[2]);
        cfg.plunger.cal.led = wirePinName(data[3]);
        break;
        
    case 8:
        // ZB Launch Ball setup
        cfg.plunger.zbLaunchBall.port = (int)(unsigned char)data[2];
        cfg.plunger.zbLaunchBall.btn = (int)(unsigned char)data[3];
        cfg.plunger.zbLaunchBall.pushDistance = (float)wireUI16(data+4) / 1000.0;
        break;
        
    case 9:
        // TV ON setup
        cfg.TVON.statusPin = wirePinName(data[2]);
        cfg.TVON.latchPin = wirePinName(data[3]);
        cfg.TVON.relayPin = wirePinName(data[4]);
        cfg.TVON.delayTime = (float)wireUI16(data+5) / 100.0;
        break;
        
    case 10:
        // TLC5940NT PWM controller chip setup
        cfg.tlc5940.nchips = (int)(unsigned char)data[2];
        cfg.tlc5940.sin = wirePinName(data[3]);
        cfg.tlc5940.sclk = wirePinName(data[4]);
        cfg.tlc5940.xlat = wirePinName(data[5]);
        cfg.tlc5940.blank = wirePinName(data[6]);
        cfg.tlc5940.gsclk = wirePinName(data[7]);
        break;
        
    case 11:
        // 74HC595 shift register chip setup
        cfg.hc595.nchips = (int)(unsigned char)data[2];
        cfg.hc595.sin = wirePinName(data[3]);
        cfg.hc595.sclk = wirePinName(data[4]);
        cfg.hc595.latch = wirePinName(data[5]);
        cfg.hc595.ena = wirePinName(data[6]);
        break;
        
    case 12:
        // button setup
        {
            // get the button number
            int idx = data[2];
            
            // if it's in range, set the button data
            if (idx > 0 && idx <= MAX_BUTTONS)
            {
                // adjust to an array index
                --idx;
                
                // set the values
                cfg.button[idx].pin = data[3];
                cfg.button[idx].typ = data[4];
                cfg.button[idx].val = data[5];
                cfg.button[idx].flags = data[6];
            }
        }
        break;
        
    case 13:
        // LedWiz output port setup
        {
            // get the port number
            int idx = data[2];
            
            // if it's in range, set the port data
            if (idx > 0 && idx <= MAX_OUT_PORTS)
            {
                // adjust to an array index
                --idx;
                
                // set the values
                cfg.outPort[idx].typ = data[3];
                cfg.outPort[idx].pin = data[4];
                cfg.outPort[idx].flags = data[5];
            }
            else if (idx == 254)
            {
                // special ports
                idx -= 254;
                cfg.specialPort[idx].typ = data[3];
                cfg.specialPort[idx].pin = data[4];
                cfg.specialPort[idx].flags = data[5];
            }
        }
        break;

    case 14:
        // engage/cancel Night Mode
        setNightMode(data[2]);
        break;
    }
}

// ---------------------------------------------------------------------------
//
// Handle an input report from the USB host.  Input reports use our extended
// LedWiz protocol.
//
void handleInputMsg(LedWizMsg &lwm, USBJoystick &js, int &z)
{
    // LedWiz commands come in two varieties:  SBA and PBA.  An
    // SBA is marked by the first byte having value 64 (0x40).  In
    // the real LedWiz protocol, any other value in the first byte
    // means it's a PBA message.  However, *valid* PBA messages
    // always have a first byte (and in fact all 8 bytes) in the
    // range 0-49 or 129-132.  Anything else is invalid.  We take
    // advantage of this to implement private protocol extensions.
    // So our full protocol is as follows:
    //
    // first byte =
    //   0-48     -> LWZ-PBA
    //   64       -> LWZ SBA 
    //   65       -> private control message; second byte specifies subtype
    //   129-132  -> LWZ-PBA
    //   200-228  -> extended bank brightness set for outputs N to N+6, where
    //               N is (first byte - 200)*7
    //   other    -> reserved for future use
    //
    uint8_t *data = lwm.data;
    if (data[0] == 64) 
    {
        // LWZ-SBA - first four bytes are bit-packed on/off flags
        // for the outputs; 5th byte is the pulse speed (1-7)
        //printf("LWZ-SBA %02x %02x %02x %02x ; %02x\r\n",
        //       data[1], data[2], data[3], data[4], data[5]);

        // update all on/off states
        for (int i = 0, bit = 1, ri = 1 ; i < numLwOutputs ; ++i, bit <<= 1)
        {
            // figure the on/off state bit for this output
            if (bit == 0x100) {
                bit = 1;
                ++ri;
            }
            
            // set the on/off state
            wizOn[i] = ((data[ri] & bit) != 0);
            
            // If the wizVal setting is 255, it means that this
            // output was last set to a brightness value with the
            // extended protocol.  Return it to LedWiz control by
            // rescaling the brightness setting to the LedWiz range
            // and updating wizVal with the result.  If it's any
            // other value, it was previously set by a PBA message,
            // so simply retain the last setting - in the normal
            // LedWiz protocol, the "profile" (brightness) and on/off
            // states are independent, so an SBA just turns an output
            // on or off but retains its last brightness level.
            if (wizVal[i] == 255)
                wizVal[i] = (uint8_t)round(outLevel[i]*48);
        }
        
        // set the flash speed - enforce the value range 1-7
        wizSpeed = data[5];
        if (wizSpeed < 1)
            wizSpeed = 1;
        else if (wizSpeed > 7)
            wizSpeed = 7;

        // update the physical outputs
        updateWizOuts();
        if (hc595 != 0)
            hc595->update();
        
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
        switch (data[1])
        {
        case 0:
            // No Op
            break;
            
        case 1:
            // 1 = Old Set Configuration:
            //     data[2] = LedWiz unit number (0x00 to 0x0f)
            //     data[3] = feature enable bit mask:
            //               0x01 = enable plunger sensor
            {
    
                // get the new LedWiz unit number - this is 0-15, whereas we
                // we save the *nominal* unit number 1-16 in the config                
                uint8_t newUnitNo = (data[2] & 0x0f) + 1;
    
                // we'll need a reset if the LedWiz unit number is changing
                bool needReset = (newUnitNo != cfg.psUnitNo);
                
                // set the configuration parameters from the message
                cfg.psUnitNo = newUnitNo;
                cfg.plunger.enabled = data[3] & 0x01;
                
                // update the status flags
                statusFlags = (statusFlags & ~0x01) | (data[3] & 0x01);
                
                // if the plunger is no longer enabled, use 0 for z reports
                if (!cfg.plunger.enabled)
                    z = 0;
                
                // save the configuration
                saveConfigToFlash();
                
                // reboot if necessary
                if (needReset)
                    reboot(js);
            }
            break;
            
        case 2:
            // 2 = Calibrate plunger
            // (No parameters)
            
            // enter calibration mode
            calBtnState = 3;
            calBtnTimer.reset();
            cfg.plunger.cal.reset(plungerSensor->npix);
            break;
            
        case 3:
            // 3 = pixel dump
            // (No parameters)
            reportPix = true;
            
            // show purple until we finish sending the report
            diagLED(1, 0, 1);
            break;
            
        case 4:
            // 4 = hardware configuration query
            // (No parameters)
            js.reportConfig(
                numOutputs, 
                cfg.psUnitNo - 1,   // report 0-15 range for unit number (we store 1-16 internally)
                cfg.plunger.cal.zero, cfg.plunger.cal.max);
            break;
            
        case 5:
            // 5 = all outputs off, reset to LedWiz defaults
            allOutputsOff();
            break;
            
        case 6:
            // 6 = Save configuration to flash.
            saveConfigToFlash();
            
            // Reboot the microcontroller.  Nearly all config changes
            // require a reset, and a reset only takes a few seconds, 
            // so we don't bother tracking whether or not a reboot is
            // really needed.
            reboot(js);
            break;
        }
    }
    else if (data[0] == 66)
    {
        // Extended protocol - Set configuration variable.
        // The second byte of the message is the ID of the variable
        // to update, and the remaining bytes give the new value,
        // in a variable-dependent format.
        configVarMsg(data);
    }
    else if (data[0] >= 200 && data[0] <= 228)
    {
        // Extended protocol - Extended output port brightness update.  
        // data[0]-200 gives us the bank of 7 outputs we're setting:
        // 200 is outputs 0-6, 201 is outputs 7-13, 202 is 14-20, etc.
        // The remaining bytes are brightness levels, 0-255, for the
        // seven outputs in the selected bank.  The LedWiz flashing 
        // modes aren't accessible in this message type; we can only 
        // set a fixed brightness, but in exchange we get 8-bit 
        // resolution rather than the paltry 0-48 scale that the real
        // LedWiz uses.  There's no separate on/off status for outputs
        // adjusted with this message type, either, as there would be
        // for a PBA message - setting a non-zero value immediately
        // turns the output, overriding the last SBA setting.
        //
        // For outputs 0-31, this overrides any previous PBA/SBA
        // settings for the port.  Any subsequent PBA/SBA message will
        // in turn override the setting made here.  It's simple - the
        // most recent message of either type takes precedence.  For
        // outputs above the LedWiz range, PBA/SBA messages can't
        // address those ports anyway.
        int i0 = (data[0] - 200)*7;
        int i1 = i0 + 7 < numOutputs ? i0 + 7 : numOutputs; 
        for (int i = i0 ; i < i1 ; ++i)
        {
            // set the brightness level for the output
            float b = data[i-i0+1]/255.0;
            outLevel[i] = b;
            
            // if it's in the basic LedWiz output set, set the LedWiz
            // profile value to 255, which means "use outLevel"
            if (i < 32) 
                wizVal[i] = 255;
                
            // set the output
            lwPin[i]->set(b * modeLevel[i]);
        }
        
        // update 74HC595 outputs, if attached
        if (hc595 != 0)
            hc595->update();
    }
    else 
    {
        // Everything else is LWZ-PBA.  This is a full "profile"
        // dump from the host for one bank of 8 outputs.  Each
        // byte sets one output in the current bank.  The current
        // bank is implied; the bank starts at 0 and is reset to 0
        // by any LWZ-SBA message, and is incremented to the next
        // bank by each LWZ-PBA message.  Our variable pbaIdx keeps
        // track of our notion of the current bank.  There's no direct
        // way for the host to select the bank; it just has to count
        // on us staying in sync.  In practice, the host will always
        // send a full set of 4 PBA messages in a row to set all 32
        // outputs.
        //
        // Note that a PBA implicitly overrides our extended profile
        // messages (message prefix 200-219), because this sets the
        // wizVal[] entry for each output, and that takes precedence
        // over the extended protocol settings.
        //
        //printf("LWZ-PBA[%d] %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
        //       pbaIdx, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

        // Update all output profile settings
        for (int i = 0 ; i < 8 ; ++i)
            wizVal[pbaIdx + i] = data[i];

        // Update the physical LED state if this is the last bank.
        // Note that hosts always send a full set of four PBA
        // messages, so there's no need to do a physical update
        // until we've received the last bank's PBA message.
        if (pbaIdx == 24)
        {
            updateWizOuts();
            if (hc595 != 0)
                hc595->update();
            pbaIdx = 0;
        }
        else
            pbaIdx += 8;
    }
}


// ---------------------------------------------------------------------------
//
// Pre-connection diagnostic flasher
//
void preConnectFlasher()
{
    diagLED(1, 0, 0);
    wait(0.05);
    diagLED(0, 0, 0);
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
    printf("\r\nPinscape Controller starting\r\n");
    // memory config debugging: {int *a = new int; printf("Stack=%lx, heap=%lx, free=%ld\r\n", (long)&a, (long)a, (long)&a - (long)a);}
    
    // clear the I2C bus (for the accelerometer)
    clear_i2c();

    // load the saved configuration
    loadConfigFromFlash();
    
    // initialize the diagnostic LEDs
    initDiagLEDs(cfg);

    // set up the pre-connected ticker
    Ticker preConnectTicker;
    preConnectTicker.attach(preConnectFlasher, 3);

    // start the TV timer, if applicable
    startTVTimer(cfg);
    
    // we're not connected/awake yet
    bool connected = false;
    time_t connectChangeTime = time(0);

    // create the plunger sensor interface
    createPlunger();

    // set up the TLC5940 interface and start the TLC5940 clock, if applicable
    init_tlc5940(cfg);

    // enable the 74HC595 chips, if present
    init_hc595(cfg);
    
    // Initialize the LedWiz ports.  Note that it's important to wait until
    // after initializing the various off-board output port controller chip
    // sybsystems (TLC5940, 74HC595), since pins attached to peripheral
    // controllers will need to address their respective controller objects,
    // which don't exit until we initialize those subsystems.
    initLwOut(cfg);
    
    // start the TLC5940 clock
    if (tlc5940 != 0)
        tlc5940->start();
        
    // initialize the button input ports
    bool kbKeys = false;
    initButtons(cfg, kbKeys);
    
    // Create the joystick USB client.  Note that we use the LedWiz unit
    // number from the saved configuration.
    MyUSBJoystick js(cfg.usbVendorID, cfg.usbProductID, USB_VERSION_NO, true, cfg.joystickEnabled, kbKeys);
    
    // we're now connected - kill the pre-connect ticker
    preConnectTicker.detach();
        
    // Last report timer for the joytick interface.  We use the joystick timer 
    // to throttle the report rate, because VP doesn't benefit from reports any 
    // faster than about every 10ms.
    Timer jsReportTimer;
    jsReportTimer.start();
    
    // Time since we successfully sent a USB report.  This is a hacky workaround
    // for sporadic problems in the USB stack that I haven't been able to figure
    // out.  If we go too long without successfully sending a USB report, we'll
    // try resetting the connection.
    Timer jsOKTimer;
    jsOKTimer.start();
    
    // set the initial status flags
    statusFlags = (cfg.plunger.enabled ? 0x01 : 0x00);

    // initialize the calibration buttons, if present
    DigitalIn *calBtn = (cfg.plunger.cal.btn == NC ? 0 : new DigitalIn(cfg.plunger.cal.btn));
    DigitalOut *calBtnLed = (cfg.plunger.cal.led == NC ? 0 : new DigitalOut(cfg.plunger.cal.led));

    // initialize the calibration button 
    calBtnTimer.start();
    calBtnState = 0;
    
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
    
    // last accelerometer report, in joystick units (we report the nudge
    // acceleration via the joystick x & y axes, per the VP convention)
    int x = 0, y = 0;
    
    // last plunger report position, in 'npix' normalized pixel units
    int pos = 0;
    
    // last plunger report, in joystick units (we report the plunger as the
    // "z" axis of the joystick, per the VP convention)
    int z = 0;
    
    // most recent prior plunger readings, for tracking release events(z0 is
    // reading just before the last one we reported, z1 is the one before that, 
    // z2 the next before that)
    int z0 = 0, z1 = 0, z2 = 0;
    
    // Simulated "bounce" position when firing.  We model the bounce off of
    // the barrel spring when the plunger is released as proportional to the
    // distance it was retracted just before being released.
    int zBounce = 0;
    
    // Simulated Launch Ball button state.  If a "ZB Launch Ball" port is
    // defined for our LedWiz port mapping, any time that port is turned ON,
    // we'll simulate pushing the Launch Ball button if the player pulls 
    // back and releases the plunger, or simply pushes on the plunger from
    // the rest position.  This allows the plunger to be used in lieu of a
    // physical Launch Ball button for tables that don't have plungers.
    //
    // States:
    //   0 = default
    //   1 = cocked (plunger has been pulled back about 1" from state 0)
    //   2 = uncocked (plunger is pulled back less than 1" from state 1)
    //   3 = launching, plunger is forward beyond park position
    //   4 = launching, plunger is behind park position
    //   5 = pressed and holding (plunger has been pressed forward beyond 
    //       the park position from state 0)
    int lbState = 0;
    
    // button bit for ZB launch ball button
    const uint32_t lbButtonBit = (1 << (cfg.plunger.zbLaunchBall.btn - 1));
    
    // Time since last lbState transition.  Some of the states are time-
    // sensitive.  In the "uncocked" state, we'll return to state 0 if
    // we remain in this state for more than a few milliseconds, since
    // it indicates that the plunger is being slowly returned to rest
    // rather than released.  In the "launching" state, we need to release 
    // the Launch Ball button after a moment, and we need to wait for 
    // the plunger to come to rest before returning to state 0.
    Timer lbTimer;
    lbTimer.start();
    
    // Launch Ball simulated push timer.  We start this when we simulate
    // the button push, and turn off the simulated button when enough time
    // has elapsed.
    Timer lbBtnTimer;
    
    // Simulated button states.  This is a vector of button states
    // for the simulated buttons.  We combine this with the physical
    // button states on each USB joystick report, so we will report
    // a button as pressed if either the physical button is being pressed
    // or we're simulating a press on the button.  This is used for the
    // simulated Launch Ball button.
    uint32_t simButtons = 0;
    
    // Firing in progress: we set this when we detect the start of rapid 
    // plunger movement from a retracted position towards the rest position.
    //
    // When we detect a firing event, we send VP a series of synthetic
    // reports simulating the idealized plunger motion.  The actual physical
    // motion is much too fast to report to VP; in the time between two USB
    // reports, the plunger can shoot all the way forward, rebound off of
    // the barrel spring, bounce back part way, and bounce forward again,
    // or even do all of this more than once.  This means that sampling the 
    // physical motion at the USB report rate would create a misleading 
    // picture of the plunger motion, since our samples would catch the 
    // plunger at random points in this oscillating motion.  From the 
    // user's perspective, the physical action that occurred is simply that 
    // the plunger was released from a particular distance, so it's this 
    // high-level event that we want to convey to VP.  To do this, we
    // synthesize a series of reports to convey an idealized version of
    // the release motion that's perfectly synchronized to the VP reports.  
    // Essentially we pretend that our USB position samples are exactly 
    // aligned in time with (1) the point of retraction just before the 
    // user released the plunger, (2) the point of maximum forward motion 
    // just after the user released the plunger (the point of maximum 
    // compression as the plunger bounces off of the barrel spring), and 
    // (3) the plunger coming to rest at the park position.  This series
    // of reports is synthetic in the sense that it's not what we actually
    // see on the CCD at the times of these reports - the true plunger
    // position is oscillating at high speed during this period.  But at
    // the same time it conveys a more faithful picture of the true physical
    // motion to VP, and allows VP to reproduce the true physical motion 
    // more faithfully in its simulation model, by correcting for the
    // relatively low sampling rate in the communication path between the
    // real plunger and VP's model plunger.
    //
    // If 'firing' is non-zero, it's the index of our current report in
    // the synthetic firing report series.
    int firing = 0;

    // start the first CCD integration cycle
    plungerSensor->init();
    
    // we're all set up - now just loop, processing sensor reports and 
    // host requests
    for (;;)
    {
        // Process incoming reports on the joystick interface.  This channel
        // is used for LedWiz commands are our extended protocol commands.
        LedWizMsg lwm;
        while (js.readLedWizMsg(lwm))
            handleInputMsg(lwm, js, z);
       
        // check for plunger calibration
        if (calBtn != 0 && !calBtn->read())
        {
            // check the state
            switch (calBtnState)
            {
            case 0: 
                // button not yet pushed - start debouncing
                calBtnTimer.reset();
                calBtnState = 1;
                break;
                
            case 1:
                // pushed, not yet debounced - if the debounce time has
                // passed, start the hold period
                if (calBtnTimer.read_ms() > 50)
                    calBtnState = 2;
                break;
                
            case 2:
                // in the hold period - if the button has been held down
                // for the entire hold period, move to calibration mode
                if (calBtnTimer.read_ms() > 2050)
                {
                    // enter calibration mode
                    calBtnState = 3;
                    calBtnTimer.reset();
                    
                    // reset the plunger calibration limits
                    cfg.plunger.cal.reset(plungerSensor->npix);
                }
                break;
                
            case 3:
                // Already in calibration mode - pushing the button here
                // doesn't change the current state, but we won't leave this
                // state as long as it's held down.  So nothing changes here.
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
            if (calBtnState == 3 && calBtnTimer.read_ms() > 15000)
            {
                // exit calibration mode
                calBtnState = 0;
                
                // save the updated configuration
                cfg.plunger.cal.calibrated = 1;
                saveConfigToFlash();
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
            newCalBtnLit = ((calBtnTimer.read_ms()/250) & 1);
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
                if (calBtnLed != 0)
                    calBtnLed->write(1);
                diagLED(0, 0, 1);       // blue
            }
            else {
                if (calBtnLed != 0)
                    calBtnLed->write(0);
                diagLED(0, 0, 0);       // off
            }
        }
 
        // If the plunger is enabled, and we're not already in a firing event,
        // and the last plunger reading had the plunger pulled back at least
        // a bit, watch for plunger release events until it's time for our next
        // USB report.
        if (!firing && cfg.plunger.enabled && z >= JOYMAX/6)
        {
            // monitor the plunger until it's time for our next report
            while (jsReportTimer.read_ms() < 15)
            {
                // do a fast low-res scan; if it's at or past the zero point,
                // start a firing event
                int pos0;
                if (plungerSensor->lowResScan(pos0) && pos0 <= cfg.plunger.cal.zero)
                    firing = 1;
            }
        }

        // read the plunger sensor, if it's enabled
        if (cfg.plunger.enabled)
        {
            // start with the previous reading, in case we don't have a
            // clear result on this frame
            int znew = z;
            if (plungerSensor->highResScan(pos))
            {
                // We got a new reading.  If we're in calibration mode, use it
                // to figure the new calibration, otherwise adjust the new reading
                // for the established calibration.
                if (calBtnState == 3)
                {
                    // Calibration mode.  If this reading is outside of the current
                    // calibration bounds, expand the bounds.
                    if (pos < cfg.plunger.cal.min)
                        cfg.plunger.cal.min = pos;
                    if (pos < cfg.plunger.cal.zero)
                        cfg.plunger.cal.zero = pos;
                    if (pos > cfg.plunger.cal.max)
                        cfg.plunger.cal.max = pos;
                        
                    // normalize to the full physical range while calibrating
                    znew = int(round(float(pos)/plungerSensor->npix * JOYMAX));
                }
                else
                {
                    // Not in calibration mode, so normalize the new reading to the 
                    // established calibration range.  
                    //
                    // Note that negative values are allowed.  Zero represents the
                    // "park" position, where the plunger sits when at rest.  A mechanical 
                    // plunger has a small amount of travel in the "push" direction,
                    // since the barrel spring can be compressed slightly.  Negative
                    // values represent travel in the push direction.
                    if (pos > cfg.plunger.cal.max)
                        pos = cfg.plunger.cal.max;
                    znew = int(round(float(pos - cfg.plunger.cal.zero)
                        / (cfg.plunger.cal.max - cfg.plunger.cal.zero + 1) * JOYMAX));
                }
            }

            // If we're not already in a firing event, check to see if the
            // new position is forward of the last report.  If it is, a firing
            // event might have started during the high-res scan.  This might
            // seem unlikely given that the scan only takes about 5ms, but that
            // 5ms represents about 25-30% of our total time between reports,
            // there's about a 1 in 4 chance that a release starts during a
            // scan.  
            if (!firing && z0 > 0 && znew < z0)
            {
                // The plunger has moved forward since the previous report.
                // Watch it for a few more ms to see if we can get a stable
                // new position.
                int pos0;
                if (plungerSensor->lowResScan(pos0))
                {
                    int pos1 = pos0;
                    Timer tw;
                    tw.start();
                    while (tw.read_ms() < 6)
                    {
                        // read the new position
                        int pos2;
                        if (plungerSensor->lowResScan(pos2))
                        {
                            // If it's stable over consecutive readings, stop looping.
                            // (Count it as stable if the position is within about 1/8".
                            // pos1 and pos2 are reported in pixels, so they range from
                            // 0 to npix.  The overall travel of a standard plunger is
                            // about 3.2", so we have (npix/3.2) pixels per inch, hence
                            // 1/8" is (npix/3.2)*(1/8) pixels.)
                            if (abs(pos2 - pos1) < int(plungerSensor->npix/(3.2*8)))
                                break;
        
                            // If we've crossed the rest position, and we've moved by
                            // a minimum distance from where we starting this loop, begin
                            // a firing event.  (We require a minimum distance to prevent
                            // spurious firing from random analog noise in the readings
                            // when the plunger is actually just sitting still at the 
                            // rest position.  If it's at rest, it's normal to see small
                            // random fluctuations in the analog reading +/- 1% or so
                            // from the 0 point, especially with a sensor like a
                            // potentionemeter that reports the position as a single 
                            // analog voltage.)  Note that we compare the latest reading
                            // to the first reading of the loop - we don't require the
                            // threshold motion over consecutive readings, but any time
                            // over the stability wait loop.
                            if (pos1 < cfg.plunger.cal.zero
                                && abs(pos2 - pos0) > int(plungerSensor->npix/(3.2*8)))
                            {
                                firing = 1;
                                break;
                            }
                                                    
                            // the new reading is now the prior reading
                            pos1 = pos2;
                        }
                    }
                }
            }
            
            // Check for a simulated Launch Ball button press, if enabled
            if (cfg.plunger.zbLaunchBall.port != 0)
            {
                const int cockThreshold = JOYMAX/3;
                const int pushThreshold = int(-JOYMAX/3 * cfg.plunger.zbLaunchBall.pushDistance);
                int newState = lbState;
                switch (lbState)
                {
                case 0:
                    // Base state.  If the plunger is pulled back by an inch
                    // or more, go to "cocked" state.  If the plunger is pushed
                    // forward by 1/4" or more, go to "pressed" state.
                    if (znew >= cockThreshold)
                        newState = 1;
                    else if (znew <= pushThreshold)
                        newState = 5;
                    break;
                    
                case 1:
                    // Cocked state.  If a firing event is now in progress,
                    // go to "launch" state.  Otherwise, if the plunger is less
                    // than 1" retracted, go to "uncocked" state - the player
                    // might be slowly returning the plunger to rest so as not
                    // to trigger a launch.
                    if (firing || znew <= 0)
                        newState = 3;
                    else if (znew < cockThreshold)
                        newState = 2;
                    break;
                    
                case 2:
                    // Uncocked state.  If the plunger is more than an inch
                    // retracted, return to cocked state.  If we've been in
                    // the uncocked state for more than half a second, return
                    // to the base state.  This allows the user to return the
                    // plunger to rest without triggering a launch, by moving
                    // it at manual speed to the rest position rather than
                    // releasing it.
                    if (znew >= cockThreshold)
                        newState = 1;
                    else if (lbTimer.read_ms() > 500)
                        newState = 0;
                    break;
                    
                case 3:
                    // Launch state.  If the plunger is no longer pushed
                    // forward, switch to launch rest state.
                    if (znew >= 0)
                        newState = 4;
                    break;    
                    
                case 4:
                    // Launch rest state.  If the plunger is pushed forward
                    // again, switch back to launch state.  If not, and we've
                    // been in this state for at least 200ms, return to the
                    // default state.
                    if (znew <= pushThreshold)
                        newState = 3;
                    else if (lbTimer.read_ms() > 200)
                        newState = 0;                    
                    break;
                    
                case 5:
                    // Press-and-Hold state.  If the plunger is no longer pushed
                    // forward, AND it's been at least 50ms since we generated
                    // the simulated Launch Ball button press, return to the base 
                    // state.  The minimum time is to ensure that VP has a chance
                    // to see the button press and to avoid transient key bounce
                    // effects when the plunger position is right on the threshold.
                    if (znew > pushThreshold && lbTimer.read_ms() > 50)
                        newState = 0;
                    break;
                }
                
                // change states if desired
                if (newState != lbState)
                {
                    // If we're entering Launch state OR we're entering the
                    // Press-and-Hold state, AND the ZB Launch Ball LedWiz signal 
                    // is turned on, simulate a Launch Ball button press.
                    if (((newState == 3 && lbState != 4) || newState == 5)
                        && wizOn[cfg.plunger.zbLaunchBall.port-1])
                    {
                        lbBtnTimer.reset();
                        lbBtnTimer.start();
                        simButtons |= lbButtonBit;
                    }
                    
                    // if we're switching to state 0, release the button
                    if (newState == 0)
                        simButtons &= ~(1 << (cfg.plunger.zbLaunchBall.btn - 1));
                    
                    // switch to the new state
                    lbState = newState;
                    
                    // start timing in the new state
                    lbTimer.reset();
                }
                
                // If the Launch Ball button press is in effect, but the
                // ZB Launch Ball LedWiz signal is no longer turned on, turn
                // off the button.
                //
                // If we're in one of the Launch states (state #3 or #4),
                // and the button has been on for long enough, turn it off.
                // The Launch mode is triggered by a pull-and-release gesture.
                // From the user's perspective, this is just a single gesture
                // that should trigger just one momentary press on the Launch
                // Ball button.  Physically, though, the plunger usually
                // bounces back and forth for 500ms or so before coming to
                // rest after this gesture.  That's what the whole state
                // #3-#4 business is all about - we stay in this pair of
                // states until the plunger comes to rest.  As long as we're
                // in these states, we won't send duplicate button presses.
                // But we also don't want the one button press to continue 
                // the whole time, so we'll time it out now.
                //
                // (This could be written as one big 'if' condition, but
                // I'm breaking it out verbosely like this to make it easier
                // for human readers such as myself to comprehend the logic.)
                if ((simButtons & lbButtonBit) != 0)
                {
                    int turnOff = false;
                    
                    // turn it off if the ZB Launch Ball signal is off
                    if (!wizOn[cfg.plunger.zbLaunchBall.port-1])
                        turnOff = true;
                        
                    // also turn it off if we're in state 3 or 4 ("Launch"),
                    // and the button has been on long enough
                    if ((lbState == 3 || lbState == 4) && lbBtnTimer.read_ms() > 250)
                        turnOff = true;
                        
                    // if we decided to turn off the button, do so
                    if (turnOff)
                    {
                        lbBtnTimer.stop();
                        simButtons &= ~lbButtonBit;
                    }
                }
            }
                
            // If a firing event is in progress, generate synthetic reports to 
            // describe an idealized version of the plunger motion to VP rather 
            // than reporting the actual physical plunger position.
            //
            // We use the synthetic reports during a release event because the
            // physical plunger motion when released is too fast for VP to track.
            // VP only syncs its internal physics model with the outside world 
            // about every 10ms.  In that amount of time, the plunger moves
            // fast enough when released that it can shoot all the way forward,
            // bounce off of the barrel spring, and rebound part of the way
            // back.  The result is the classic analog-to-digital problem of
            // sample aliasing.  If we happen to time our sample during the
            // release motion so that we catch the plunger at the peak of a
            // bounce, the digital signal incorrectly looks like the plunger
            // is moving slowly forward - VP thinks we went from fully
            // retracted to half retracted in the sample interval, whereas
            // we actually traveled all the way forward and half way back,
            // so the speed VP infers is about 1/3 of the actual speed.
            //
            // To correct this, we take advantage of our ability to sample 
            // the CCD image several times in the course of a VP report.  If
            // we catch the plunger near the origin after we've seen it
            // retracted, we go into Release Event mode.  During this mode,
            // we stop reporting the true physical plunger position, and
            // instead report an idealized pattern: we report the plunger
            // immediately shooting forward to a position in front of the
            // park position that's in proportion to how far back the plunger
            // was just before the release, and we then report it stationary
            // at the park position.  We continue to report the stationary
            // park position until the actual physical plunger motion has
            // stabilized on a new position.  We then exit Release Event
            // mode and return to reporting the true physical position.
            if (firing)
            {
                // Firing in progress.  Keep reporting the park position
                // until the physical plunger position comes to rest.
                const int restTol = JOYMAX/24;
                if (firing == 1)
                {
                    // For the first couple of frames, show the plunger shooting
                    // forward past the zero point, to simulate the momentum carrying
                    // it forward to bounce off of the barrel spring.  Show the 
                    // bounce as proportional to the distance it was retracted
                    // in the prior report.
                    z = zBounce = -z0/6;
                    ++firing;
                }
                else if (firing == 2)
                {
                    // second frame - keep the bounce a little longer
                    z = zBounce;
                    ++firing;
                }
                else if (firing > 4
                    && abs(znew - z0) < restTol
                    && abs(znew - z1) < restTol 
                    && abs(znew - z2) < restTol)
                {
                    // The physical plunger has come to rest.  Exit firing
                    // mode and resume reporting the actual position.
                    firing = false;
                    z = znew;
                }
                else
                {
                    // until the physical plunger comes to rest, simply 
                    // report the park position
                    z = 0;
                    ++firing;
                }
            }
            else
            {
                // not in firing mode - report the true physical position
                z = znew;
            }

            // shift the new reading into the recent history buffer
            z2 = z1;
            z1 = z0;
            z0 = znew;
        }

        // process button updates
        processButtons();
        
        // send a keyboard report if we have new data
        if (kbState.changed)
        {
            // send a keyboard report
            js.kbUpdate(kbState.data);
            kbState.changed = false;
        }
        
        // likewise for the media controller
        if (mediaState.changed)
        {
            // send a media report
            js.mediaUpdate(mediaState.data);
            mediaState.changed = false;
        }
        
        // flag:  did we successfully send a joystick report on this round?
        bool jsOK = false;

        // If it's been long enough since our last USB status report,
        // send the new report.  We throttle the report rate because
        // it can overwhelm the PC side if we report too frequently.
        // VP only wants to sync with the real world in 10ms intervals,
        // so reporting more frequently creates I/O overhead without 
        // doing anything to improve the simulation.
        if (cfg.joystickEnabled && jsReportTimer.read_ms() > 10)
        {
            // read the accelerometer
            int xa, ya;
            accel.get(xa, ya);
            
            // confine the results to our joystick axis range
            if (xa < -JOYMAX) xa = -JOYMAX;
            if (xa > JOYMAX) xa = JOYMAX;
            if (ya < -JOYMAX) ya = -JOYMAX;
            if (ya > JOYMAX) ya = JOYMAX;
            
            // store the updated accelerometer coordinates
            x = xa;
            y = ya;
            
            // Report the current plunger position UNLESS the ZB Launch Ball 
            // signal is on, in which case just report a constant 0 value.  
            // ZB Launch Ball turns off the plunger position because it
            // tells us that the table has a Launch Ball button instead of
            // a traditional plunger.
            int zrep = (cfg.plunger.zbLaunchBall.port != 0 && wizOn[cfg.plunger.zbLaunchBall.port-1] ? 0 : z);
            
            // rotate X and Y according to the device orientation in the cabinet
            accelRotate(x, y);

            // send the joystick report
            jsOK = js.update(x, y, zrep, jsButtons | simButtons, statusFlags);
            
            // we've just started a new report interval, so reset the timer
            jsReportTimer.reset();
        }

        // If we're in pixel dump mode, report all pixel exposure values
        if (reportPix)
        {
            // send the report            
            plungerSensor->sendExposureReport(js);

            // we have satisfied this request
            reportPix = false;
        }
        
        // If joystick reports are turned off, send a generic status report
        // periodically for the sake of the Windows config tool.
        if (!cfg.joystickEnabled && jsReportTimer.read_ms() > 200)
        {
            jsOK = js.updateStatus(0);
            jsReportTimer.reset();
        }

        // if we successfully sent a joystick report, reset the watchdog timer
        if (jsOK) 
        {
            jsOKTimer.reset();
            jsOKTimer.start();
        }

#ifdef DEBUG_PRINTF
        if (x != 0 || y != 0)
            printf("%d,%d\r\n", x, y);
#endif

        // check for connection status changes
        int newConnected = js.isConnected() && !js.isSuspended();
        if (newConnected != connected)
        {
            // give it a few seconds to stabilize
            time_t tc = time(0);
            if (tc - connectChangeTime > 3)
            {
                // note the new status
                connected = newConnected;
                connectChangeTime = tc;
                
                // if we're no longer connected, turn off all outputs
                if (!connected)
                    allOutputsOff();
            }
        }
        
        // provide a visual status indication on the on-board LED
        if (calBtnState < 2 && hbTimer.read_ms() > 1000) 
        {
            if (!newConnected)
            {
                // suspended - turn off the LED
                diagLED(0, 0, 0);

                // show a status flash every so often                
                if (hbcnt % 3 == 0)
                {
                    // disconnected = short red/red flash
                    // suspended = short red flash
                    for (int n = js.isConnected() ? 1 : 2 ; n > 0 ; --n)
                    {
                        diagLED(1, 0, 0);
                        wait(0.05);
                        diagLED(0, 0, 0);
                        wait(0.25);
                    }
                }
            }
            else if (jsOKTimer.read() > 5)
            {
                // USB freeze - show red/yellow.
                //  Our outgoing joystick messages aren't going through, even though we
                // think we're still connected.  This indicates that one or more of our
                // USB endpoints have stopped working, which can happen as a result of
                // bugs in the USB HAL or latency responding to a USB IRQ.  Show a
                // distinctive diagnostic flash to signal the error.  I haven't found a 
                // way to recover from this class of error other than rebooting the MCU, 
                // so the goal is to fix the HAL so that this error never happens.  This
                // flash pattern is thus for debugging purposes only; hopefully it won't
                // ever occur in a real installation.
                static bool dumped;
                if (!dumped) {
                    // If we haven't already, dump the USB HAL status to the debug console,
                    // in case it helps identify the reason for the endpoint failure.
                    extern void USBDeviceStatusDump(void);
                    USBDeviceStatusDump();
                    dumped = true;
                }
                jsOKTimer.stop();
                hb = !hb;
                diagLED(1, hb, 0);
            }
            else if (cfg.plunger.enabled && !cfg.plunger.cal.calibrated)
            {
                // connected, plunger calibration needed - flash yellow/green
                hb = !hb;
                diagLED(hb, 1, 0);
            }
            else
            {
                // connected - flash blue/green
                hb = !hb;
                diagLED(0, hb, !hb);
            }
            
            // reset the heartbeat timer
            hbTimer.reset();
            ++hbcnt;
        }
    }
}
