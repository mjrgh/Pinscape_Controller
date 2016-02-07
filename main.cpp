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
//    of ports.  The KL25Z hardware is limited to 10 PWM ports.  Ports beyond the
//    10 PWM ports are simple digital on/off ports.  Intensity level settings on 
//    digital ports is ignored, so such ports can only be used for devices such as 
//    contactors and solenoids that don't need differeing intensities.
//
//    Note that the KL25Z can only supply or sink 4mA on its output ports, so external 
//    amplifier hardware is required to use the LedWiz emulation.  Many different 
//    hardware designs are possible, but there's a simple reference design in the 
//    documentation that uses a Darlington array IC to increase the output from 
//    each port to 500mA (the same level as the LedWiz), plus an extended design 
//    that adds an optocoupler and MOSFET to provide very high power handling, up 
//    to about 45A or 150W, with voltages up to 100V.  That will handle just about 
//    any DC device directly (wtihout relays or other amplifiers), and switches fast 
//    enough to support PWM devices.  For example, you can use it to drive a motor at
//    different speeds via the PWM intensity.
//
//    The Controller device can report any desired LedWiz unit number to the host, 
//    which makes it possible for one or more Pinscape Controller units to coexist
//    with one more more real LedWiz units in the same machine.  The LedWiz design 
//    allows for up to 16 units to be installed in one machine.  Each device needs
//    to have a distinct LedWiz Unit Number, which allows software on the PC to
//    address each device independently.
//
//    The LedWiz emulation features are of course optional.  There's no need to 
//    build any of the external port hardware (or attach anything to the output 
//    ports at all) if the LedWiz features aren't needed.
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
// Extended verison of Timer class.  This adds the ability to interrogate
// the running state.
//
class Timer2: public Timer
{
public:
    Timer2() : running(false) { }

    void start() { running = true; Timer::start(); }
    void stop()  { running = false; Timer::stop(); }
    
    bool isRunning() const { return running; }
    
private:
    bool running;
};

// --------------------------------------------------------------------------
// 
// USB product version number
//
const uint16_t USB_VERSION_NO = 0x0009;

// --------------------------------------------------------------------------
//
// Joystick axis report range - we report from -JOYMAX to +JOYMAX
//
#define JOYMAX 4096


// ---------------------------------------------------------------------------
//
// Wire protocol value translations.  These translate byte values to and
// from the USB protocol to local native format.
//

// unsigned 16-bit integer 
inline uint16_t wireUI16(const uint8_t *b)
{
    return b[0] | ((uint16_t)b[1] << 8);
}
inline void ui16Wire(uint8_t *b, uint16_t val)
{
    b[0] = (uint8_t)(val & 0xff);
    b[1] = (uint8_t)((val >> 8) & 0xff);
}

inline int16_t wireI16(const uint8_t *b)
{
    return (int16_t)wireUI16(b);
}
inline void i16Wire(uint8_t *b, int16_t val)
{
    ui16Wire(b, (uint16_t)val);
}

inline uint32_t wireUI32(const uint8_t *b)
{
    return b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
inline void ui32Wire(uint8_t *b, uint32_t val)
{
    b[0] = (uint8_t)(val & 0xff);
    b[1] = (uint8_t)((val >> 8) & 0xff);    
    b[2] = (uint8_t)((val >> 16) & 0xff);    
    b[3] = (uint8_t)((val >> 24) & 0xff);    
}

inline int32_t wireI32(const uint8_t *b)
{
    return (int32_t)wireUI32(b);
}

static const PinName pinNameMap[] =  {
    NC,    PTA1,  PTA2,  PTA4,  PTA5,  PTA12, PTA13, PTA16, PTA17, PTB0,    // 0-9
    PTB1,  PTB2,  PTB3,  PTB8,  PTB9,  PTB10, PTB11, PTB18, PTB19, PTC0,    // 10-19
    PTC1,  PTC2,  PTC3,  PTC4,  PTC5,  PTC6,  PTC7,  PTC8,  PTC9,  PTC10,   // 20-29
    PTC11, PTC12, PTC13, PTC16, PTC17, PTD0,  PTD1,  PTD2,  PTD3,  PTD4,    // 30-39
    PTD5,  PTD6,  PTD7,  PTE0,  PTE1,  PTE2,  PTE3,  PTE4,  PTE5,  PTE20,   // 40-49
    PTE21, PTE22, PTE23, PTE29, PTE30, PTE31                                // 50-55
};
inline PinName wirePinName(int c)
{
    return (c < countof(pinNameMap) ? pinNameMap[c] : NC);
}
inline void pinNameWire(uint8_t *b, PinName n)
{
    b[0] = 0; // presume invalid -> NC
    for (int i = 0 ; i < countof(pinNameMap) ; ++i)
    {
        if (pinNameMap[i] == n)
        {
            b[0] = i;
            return;
        }
    }
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
    // Set the output intensity.  'val' is 0 for fully off, 255 for
    // fully on, with values in between signifying lower intensity.
    virtual void set(uint8_t val) = 0;
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
    virtual void set(uint8_t ) { }
};

// Active Low out.  For any output marked as active low, we layer this
// on top of the physical pin interface.  This simply inverts the value of
// the output value, so that 255 means fully off and 0 means fully on.
class LwInvertedOut: public LwOut
{
public:
    LwInvertedOut(LwOut *o) : out(o) { }
    virtual void set(uint8_t val) { out->set(255 - val); }
    
private:
    LwOut *out;
};

// Gamma correction table for 8-bit input values
static const uint8_t gamma[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1, 
      1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2, 
      2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   5,   5,   5, 
      5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10, 
     10,  10,  11,  11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16, 
     17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,  24,  25, 
     25,  26,  27,  27,  28,  29,  29,  30,  31,  32,  32,  33,  34,  35,  35,  36, 
     37,  38,  39,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  50, 
     51,  52,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  66,  67,  68, 
     69,  70,  72,  73,  74,  75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89, 
     90,  92,  93,  95,  96,  98,  99, 101, 102, 104, 105, 107, 109, 110, 112, 114, 
    115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142, 
    144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175, 
    177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213, 
    215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
};

// Gamma-corrected out.  This is a filter object that we layer on top
// of a physical pin interface.  This applies gamma correction to the
// input value and then passes it along to the underlying pin object.
class LwGammaOut: public LwOut
{
public:
    LwGammaOut(LwOut *o) : out(o) { }
    virtual void set(uint8_t val) { out->set(gamma[val]); }
    
private:
    LwOut *out;
};

// Noisy output.  This is a filter object that we layer on top of
// a physical pin output.  This filter disables the port when night
// mode is engaged.
class LwNoisyOut: public LwOut
{
public:
    LwNoisyOut(LwOut *o) : out(o) { }
    virtual void set(uint8_t val) { out->set(nightMode ? 0 : val); }
    
    static bool nightMode;

private:
    LwOut *out;
};

// global night mode flag
bool LwNoisyOut::nightMode = false;


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

// Conversion table for 8-bit DOF level to 12-bit TLC5940 level
static const uint16_t dof_to_tlc[] = {
       0,   16,   32,   48,   64,   80,   96,  112,  128,  145,  161,  177,  193,  209,  225,  241, 
     257,  273,  289,  305,  321,  337,  353,  369,  385,  401,  418,  434,  450,  466,  482,  498, 
     514,  530,  546,  562,  578,  594,  610,  626,  642,  658,  674,  691,  707,  723,  739,  755, 
     771,  787,  803,  819,  835,  851,  867,  883,  899,  915,  931,  947,  964,  980,  996, 1012, 
    1028, 1044, 1060, 1076, 1092, 1108, 1124, 1140, 1156, 1172, 1188, 1204, 1220, 1237, 1253, 1269, 
    1285, 1301, 1317, 1333, 1349, 1365, 1381, 1397, 1413, 1429, 1445, 1461, 1477, 1493, 1510, 1526, 
    1542, 1558, 1574, 1590, 1606, 1622, 1638, 1654, 1670, 1686, 1702, 1718, 1734, 1750, 1766, 1783, 
    1799, 1815, 1831, 1847, 1863, 1879, 1895, 1911, 1927, 1943, 1959, 1975, 1991, 2007, 2023, 2039, 
    2056, 2072, 2088, 2104, 2120, 2136, 2152, 2168, 2184, 2200, 2216, 2232, 2248, 2264, 2280, 2296, 
    2312, 2329, 2345, 2361, 2377, 2393, 2409, 2425, 2441, 2457, 2473, 2489, 2505, 2521, 2537, 2553, 
    2569, 2585, 2602, 2618, 2634, 2650, 2666, 2682, 2698, 2714, 2730, 2746, 2762, 2778, 2794, 2810, 
    2826, 2842, 2858, 2875, 2891, 2907, 2923, 2939, 2955, 2971, 2987, 3003, 3019, 3035, 3051, 3067, 
    3083, 3099, 3115, 3131, 3148, 3164, 3180, 3196, 3212, 3228, 3244, 3260, 3276, 3292, 3308, 3324, 
    3340, 3356, 3372, 3388, 3404, 3421, 3437, 3453, 3469, 3485, 3501, 3517, 3533, 3549, 3565, 3581, 
    3597, 3613, 3629, 3645, 3661, 3677, 3694, 3710, 3726, 3742, 3758, 3774, 3790, 3806, 3822, 3838, 
    3854, 3870, 3886, 3902, 3918, 3934, 3950, 3967, 3983, 3999, 4015, 4031, 4047, 4063, 4079, 4095
};

// Conversion table for 8-bit DOF level to 12-bit TLC5940 level, with 
// gamma correction.  Note that the output layering scheme can handle
// this without a separate table, by first applying gamma to the DOF
// level to produce an 8-bit gamma-corrected value, then convert that
// to the 12-bit TLC5940 value.  But we get better precision by doing
// the gamma correction in the 12-bit TLC5940 domain.  We can only
// get the 12-bit domain by combining both steps into one layering
// object, though, since the intermediate values in the layering system
// are always 8 bits.
static const uint16_t dof_to_gamma_tlc[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1, 
      2,   2,   2,   3,   3,   4,   4,   5,   5,   6,   7,   8,   8,   9,  10,  11, 
     12,  13,  15,  16,  17,  18,  20,  21,  23,  25,  26,  28,  30,  32,  34,  36, 
     38,  40,  43,  45,  48,  50,  53,  56,  59,  62,  65,  68,  71,  75,  78,  82, 
     85,  89,  93,  97, 101, 105, 110, 114, 119, 123, 128, 133, 138, 143, 149, 154, 
    159, 165, 171, 177, 183, 189, 195, 202, 208, 215, 222, 229, 236, 243, 250, 258, 
    266, 273, 281, 290, 298, 306, 315, 324, 332, 341, 351, 360, 369, 379, 389, 399, 
    409, 419, 430, 440, 451, 462, 473, 485, 496, 508, 520, 532, 544, 556, 569, 582, 
    594, 608, 621, 634, 648, 662, 676, 690, 704, 719, 734, 749, 764, 779, 795, 811, 
    827, 843, 859, 876, 893, 910, 927, 944, 962, 980, 998, 1016, 1034, 1053, 1072, 1091, 
    1110, 1130, 1150, 1170, 1190, 1210, 1231, 1252, 1273, 1294, 1316, 1338, 1360, 1382, 1404, 1427, 
    1450, 1473, 1497, 1520, 1544, 1568, 1593, 1617, 1642, 1667, 1693, 1718, 1744, 1770, 1797, 1823, 
    1850, 1877, 1905, 1932, 1960, 1988, 2017, 2045, 2074, 2103, 2133, 2162, 2192, 2223, 2253, 2284, 
    2315, 2346, 2378, 2410, 2442, 2474, 2507, 2540, 2573, 2606, 2640, 2674, 2708, 2743, 2778, 2813, 
    2849, 2884, 2920, 2957, 2993, 3030, 3067, 3105, 3143, 3181, 3219, 3258, 3297, 3336, 3376, 3416, 
    3456, 3496, 3537, 3578, 3619, 3661, 3703, 3745, 3788, 3831, 3874, 3918, 3962, 4006, 4050, 4095
};


// LwOut class for TLC5940 outputs.  These are fully PWM capable.
// The 'idx' value in the constructor is the output index in the
// daisy-chained TLC5940 array.  0 is output #0 on the first chip,
// 1 is #1 on the first chip, 15 is #15 on the first chip, 16 is
// #0 on the second chip, 32 is #0 on the third chip, etc.
class Lw5940Out: public LwOut
{
public:
    Lw5940Out(int idx) : idx(idx) { prv = 0; }
    virtual void set(uint8_t val)
    {
        if (val != prv)
           tlc5940->set(idx, dof_to_tlc[prv = val]);
    }
    int idx;
    uint8_t prv;
};

// LwOut class for TLC5940 gamma-corrected outputs.
class Lw5940GammaOut: public LwOut
{
public:
    Lw5940GammaOut(int idx) : idx(idx) { prv = 0; }
    virtual void set(uint8_t val)
    {
        if (val != prv)
           tlc5940->set(idx, dof_to_gamma_tlc[prv = val]);
    }
    int idx;
    uint8_t prv;
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
    Lw595Out(int idx) : idx(idx) { prv = 0; }
    virtual void set(uint8_t val)
    {
        if (val != prv)
           hc595->set(idx, (prv = val) == 0 ? 0 : 1);
    }
    int idx;
    uint8_t prv;
};



// Conversion table - 8-bit DOF output level to PWM float level
// (normalized to 0.0..1.0 scale)
static const float pwm_level[] = {
    0.000000, 0.003922, 0.007843, 0.011765, 0.015686, 0.019608, 0.023529, 0.027451, 
    0.031373, 0.035294, 0.039216, 0.043137, 0.047059, 0.050980, 0.054902, 0.058824, 
    0.062745, 0.066667, 0.070588, 0.074510, 0.078431, 0.082353, 0.086275, 0.090196, 
    0.094118, 0.098039, 0.101961, 0.105882, 0.109804, 0.113725, 0.117647, 0.121569, 
    0.125490, 0.129412, 0.133333, 0.137255, 0.141176, 0.145098, 0.149020, 0.152941, 
    0.156863, 0.160784, 0.164706, 0.168627, 0.172549, 0.176471, 0.180392, 0.184314, 
    0.188235, 0.192157, 0.196078, 0.200000, 0.203922, 0.207843, 0.211765, 0.215686, 
    0.219608, 0.223529, 0.227451, 0.231373, 0.235294, 0.239216, 0.243137, 0.247059, 
    0.250980, 0.254902, 0.258824, 0.262745, 0.266667, 0.270588, 0.274510, 0.278431, 
    0.282353, 0.286275, 0.290196, 0.294118, 0.298039, 0.301961, 0.305882, 0.309804, 
    0.313725, 0.317647, 0.321569, 0.325490, 0.329412, 0.333333, 0.337255, 0.341176, 
    0.345098, 0.349020, 0.352941, 0.356863, 0.360784, 0.364706, 0.368627, 0.372549, 
    0.376471, 0.380392, 0.384314, 0.388235, 0.392157, 0.396078, 0.400000, 0.403922, 
    0.407843, 0.411765, 0.415686, 0.419608, 0.423529, 0.427451, 0.431373, 0.435294, 
    0.439216, 0.443137, 0.447059, 0.450980, 0.454902, 0.458824, 0.462745, 0.466667, 
    0.470588, 0.474510, 0.478431, 0.482353, 0.486275, 0.490196, 0.494118, 0.498039, 
    0.501961, 0.505882, 0.509804, 0.513725, 0.517647, 0.521569, 0.525490, 0.529412, 
    0.533333, 0.537255, 0.541176, 0.545098, 0.549020, 0.552941, 0.556863, 0.560784, 
    0.564706, 0.568627, 0.572549, 0.576471, 0.580392, 0.584314, 0.588235, 0.592157, 
    0.596078, 0.600000, 0.603922, 0.607843, 0.611765, 0.615686, 0.619608, 0.623529, 
    0.627451, 0.631373, 0.635294, 0.639216, 0.643137, 0.647059, 0.650980, 0.654902, 
    0.658824, 0.662745, 0.666667, 0.670588, 0.674510, 0.678431, 0.682353, 0.686275, 
    0.690196, 0.694118, 0.698039, 0.701961, 0.705882, 0.709804, 0.713725, 0.717647, 
    0.721569, 0.725490, 0.729412, 0.733333, 0.737255, 0.741176, 0.745098, 0.749020, 
    0.752941, 0.756863, 0.760784, 0.764706, 0.768627, 0.772549, 0.776471, 0.780392, 
    0.784314, 0.788235, 0.792157, 0.796078, 0.800000, 0.803922, 0.807843, 0.811765, 
    0.815686, 0.819608, 0.823529, 0.827451, 0.831373, 0.835294, 0.839216, 0.843137, 
    0.847059, 0.850980, 0.854902, 0.858824, 0.862745, 0.866667, 0.870588, 0.874510, 
    0.878431, 0.882353, 0.886275, 0.890196, 0.894118, 0.898039, 0.901961, 0.905882, 
    0.909804, 0.913725, 0.917647, 0.921569, 0.925490, 0.929412, 0.933333, 0.937255, 
    0.941176, 0.945098, 0.949020, 0.952941, 0.956863, 0.960784, 0.964706, 0.968627, 
    0.972549, 0.976471, 0.980392, 0.984314, 0.988235, 0.992157, 0.996078, 1.000000
};

// LwOut class for a PWM-capable GPIO port
class LwPwmOut: public LwOut
{
public:
    LwPwmOut(PinName pin, uint8_t initVal) : p(pin)
    {
         prv = initVal ^ 0xFF;
         set(initVal);
    }
    virtual void set(uint8_t val) 
    { 
        if (val != prv)
            p.write(pwm_level[prv = val]); 
    }
    PwmOut p;
    uint8_t prv;
};

// LwOut class for a Digital-Only (Non-PWM) GPIO port
class LwDigOut: public LwOut
{
public:
    LwDigOut(PinName pin, uint8_t initVal) : p(pin, initVal ? 1 : 0) { prv = initVal; }
    virtual void set(uint8_t val) 
    {
         if (val != prv)
            p.write((prv = val) == 0 ? 0 : 1); 
    }
    DigitalOut p;
    uint8_t prv;
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
const int SPECIAL_PIN_NIGHTMODE = 0;


// Number of LedWiz emulation outputs.  This is the number of ports
// accessible through the standard (non-extended) LedWiz protocol
// messages.  The protocol has a fixed set of 32 outputs, but we
// might have fewer actual outputs.  This is therefore set to the
// lower of 32 or the actual number of outputs.
static int numLwOutputs;

// Current absolute brightness level for an output.  This is a DOF
// brightness level value, from 0 for fully off to 255 for fully on.  
// This is used for all extended ports (33 and above), and for any 
// LedWiz port with wizVal == 255.
static uint8_t *outLevel;

// create a single output pin
LwOut *createLwPin(LedWizPortCfg &pc, Config &cfg)
{
    // get this item's values
    int typ = pc.typ;
    int pin = pc.pin;
    int flags = pc.flags;
    int noisy = flags & PortFlagNoisemaker;
    int activeLow = flags & PortFlagActiveLow;
    int gamma = flags & PortFlagGamma;

    // create the pin interface object according to the port type        
    LwOut *lwp;
    switch (typ)
    {
    case PortTypeGPIOPWM:
        // PWM GPIO port
        lwp = new LwPwmOut(wirePinName(pin), activeLow ? 255 : 0);
        break;
    
    case PortTypeGPIODig:
        // Digital GPIO port
        lwp = new LwDigOut(wirePinName(pin), activeLow ? 255 : 0);
        break;
    
    case PortTypeTLC5940:
        // TLC5940 port (if we don't have a TLC controller object, or it's not a valid
        // output port number on the chips we have, create a virtual port)
        if (tlc5940 != 0 && pin < cfg.tlc5940.nchips*16)
        {
            // If gamma correction is to be used, and we're not inverting the output,
            // use the combined TLC4950 + Gamma output class.  Otherwise use the plain 
            // TLC5940 output.  We skip the combined class if the output is inverted
            // because we need to apply gamma BEFORE the inversion to get the right
            // results, but the combined class would apply it after because of the
            // layering scheme - the combined class is a physical device output class,
            // and a physical device output class is necessarily at the bottom of 
            // the stack.  We don't have a combined inverted+gamma+TLC class, because
            // inversion isn't recommended for TLC5940 chips in the first place, so
            // it's not worth the extra memory footprint to have a dedicated table
            // for this unlikely case.
            if (gamma && !activeLow)
            {
                // use the gamma-corrected 5940 output mapper
                lwp = new Lw5940GammaOut(pin);
                
                // DON'T apply further gamma correction to this output
                gamma = false;
            }
            else
            {
                // no gamma - use the plain (linear) 5940 output class
                lwp = new Lw5940Out(pin);
            }
        }
        else
        {
            // no TLC5940 chips, or invalid port number - use a virtual out
            lwp = new LwVirtualOut();
        }
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
    case PortTypeDisabled:
    default:
        // virtual or unknown
        lwp = new LwVirtualOut();
        break;
    }
    
    // If it's Active Low, layer on an inverter.  Note that an inverter
    // needs to be the bottom-most layer, since all of the other filters
    // assume that they're working with normal (non-inverted) values.
    if (activeLow)
        lwp = new LwInvertedOut(lwp);
        
    // If it's a noisemaker, layer on a night mode switch.  Note that this
    // needs to be 
    if (noisy)
        lwp = new LwNoisyOut(lwp);
        
    // If it's gamma-corrected, layer on a gamma corrector
    if (gamma)
        lwp = new LwGammaOut(lwp);

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
    outLevel = new uint8_t[minOuts];
    
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

// LedWiz "Profile State" (the LedWiz brightness level or blink mode)
// for each LedWiz output.  If the output was last updated through an 
// LedWiz protocol message, it will have one of these values:
//
//   0-48 = fixed brightness 0% to 100%
//   49  = fixed brightness 100% (equivalent to 48)
//   129 = ramp up / ramp down
//   130 = flash on / off
//   131 = on / ramp down
//   132 = ramp up / on
//
// If the output was last updated through an extended protocol message,
// it will have the special value 255.  This means that we use the
// outLevel[] value for the port instead of an LedWiz setting.
//
// (Note that value 49 isn't documented in the LedWiz spec, but real
// LedWiz units treat it as equivalent to 48, and some PC software uses
// it, so we need to accept it for compatibility.)
static uint8_t wizVal[32] = {
    48, 48, 48, 48, 48, 48, 48, 48,
    48, 48, 48, 48, 48, 48, 48, 48,
    48, 48, 48, 48, 48, 48, 48, 48,
    48, 48, 48, 48, 48, 48, 48, 48
};

// LedWiz flash speed.  This is a value from 1 to 7 giving the pulse
// rate for lights in blinking states.
static uint8_t wizSpeed = 2;

// Current LedWiz flash cycle counter.  This runs from 0 to 255
// during each cycle.
static uint8_t wizFlashCounter = 0;

// translate an LedWiz brightness level (0-49) to a DOF brightness
// level (0-255)
static const uint8_t lw_to_dof[] = {
       0,    5,   11,   16,   21,   27,   32,   37, 
      43,   48,   53,   58,   64,   69,   74,   80, 
      85,   90,   96,  101,  106,  112,  117,  122, 
     128,  133,  138,  143,  149,  154,  159,  165, 
     170,  175,  181,  186,  191,  197,  202,  207, 
     213,  218,  223,  228,  234,  239,  244,  250, 
     255,  255
};

// Translate an LedWiz output (ports 1-32) to a DOF brightness level.
static uint8_t wizState(int idx)
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
    if (val <= 49)
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
        //
        // Note that value 49 is undefined in the LedWiz documentation,
        // but real LedWiz units treat it as 100%, equivalent to 48.
        // Some software on the PC side uses this, so we need to treat
        // it the same way for compatibility.
        return lw_to_dof[val];
    }
    else if (val == 129)
    {
        // 129 = ramp up / ramp down
        return wizFlashCounter < 128 
            ? wizFlashCounter*2 + 1
            : (255 - wizFlashCounter)*2;
    }
    else if (val == 130)
    {
        // 130 = flash on / off
        return wizFlashCounter < 128 ? 255 : 0;
    }
    else if (val == 131)
    {
        // 131 = on / ramp down
        return wizFlashCounter < 128 ? 255 : (255 - wizFlashCounter)*2;
    }
    else if (val == 132)
    {
        // 132 = ramp up / on
        return wizFlashCounter < 128 ? wizFlashCounter*2 : 255;
    }
    else
    {
        // Other values are undefined in the LedWiz documentation.  Hosts
        // *should* never send undefined values, since whatever behavior an
        // LedWiz unit exhibits in response is accidental and could change
        // in a future version.  We'll treat all undefined values as equivalent 
        // to 48 (fully on).
        return 255;
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
                lwPin[i]->set(wizState(i));
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
        lwPin[i]->set(wizState(i));
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
        lwPin[i]->set(wizState(i));
        
    // update each extended output
    for (int i = 33 ; i < numOutputs ; ++i)
        lwPin[i]->set(outLevel[i]);
        
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
    for (int i = numLwOutputs ; i < numOutputs ; ++i)
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
// This scheme might seem a little convoluted, but it handles a number
// of tricky but likely scenarios:
//
// - Most cabinets systems are set up with "soft" PC power switches, 
//   so that the PC goes into "Soft Off" mode when the user turns off
//   the cabinet by pushing the power button or using the Shut Down
//   command from within Windows.  In Windows parlance, this "soft off"
//   condition is called ACPI State S5.  In this state, the main CPU
//   power is turned off, but the motherboard still provides power to
//   USB devices.  This means that the KL25Z keeps running.  Without
//   the external power sensing circuit, the only hint that we're in 
//   this state is that the USB connection to the host goes into Suspend
//   mode, but that could mean other things as well.  The latch circuit
//   lets us tell for sure that we're in this state.
//
// - Some cabinet builders might prefer to use "hard" power switches,
//   cutting all power to the cabinet, including the PC motherboard (and
//   thus the KL25Z) every time the machine is turned off.  This also
//   applies to the "soft" switch case above when the cabinet is unplugged,
//   a power outage occurs, etc.  In these cases, the KL25Z will do a cold
//   boot when the PC is turned on.  We don't know whether the KL25Z
//   will power up before or after PSU2, so it's not good enough to 
//   observe the current state of PSU2 when we first check.  If PSU2
//   were to come on first, checking only the current state would fool
//   us into thinking that no action is required, because we'd only see
//   that PSU2 is turned on any time we check.  The latch handles this 
//   case by letting us see that PSU2 was indeed off some time before our
//   first check.
//
// - If the KL25Z is rebooted while the main system is running, or the 
//   KL25Z is unplugged and plugged back in, we'll correctly leave the 
//   TVs as they are.  The latch state is independent of the KL25Z's 
//   power or software state, so it's won't affect the latch state when
//   the KL25Z is unplugged or rebooted; when we boot, we'll see that 
//   the latch is already on and that we don't have to turn on the TVs.
//   This is important because TV ON buttons are usually on/off toggles,
//   so we don't want to push the button on a TV that's already on.
//   

// Current PSU2 state:
//   1 -> default: latch was on at last check, or we haven't checked yet
//   2 -> latch was off at last check, SET pulsed high
//   3 -> SET pulsed low, ready to check status
//   4 -> TV timer countdown in progress
//   5 -> TV relay on
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
        // if it stuck.  If the latch is now on, PSU2 has transitioned
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
        tv_delay_time = cfg.TVON.delayTime/100.0;
    
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
// Night mode setting updates
//

// Turn night mode on or off
static void setNightMode(bool on)
{
    // set the new night mode flag in the noisy output class
    LwNoisyOut::nightMode = on;

    // update the special output pin that shows the night mode state
    specialPin[SPECIAL_PIN_NIGHTMODE]->set(on ? 255 : 0);

    // update all outputs for the mode change
    updateAllOuts();
}

// Toggle night mode
static void toggleNightMode()
{
    setNightMode(!LwNoisyOut::nightMode);
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
// Configuration variable get/set message handling
//

// Handle SET messages - write configuration variables from USB message data
#define if_msg_valid(test)  if (test)
#define v_byte(var, ofs)   cfg.var = data[ofs]
#define v_ui16(var, ofs)   cfg.var = wireUI16(data+ofs)
#define v_pin(var, ofs)    cfg.var = wirePinName(data[ofs])
#define v_func configVarSet
#include "cfgVarMsgMap.h"

// redefine everything for the SET messages
#undef if_msg_valid
#undef v_byte
#undef v_ui16
#undef v_pin
#undef v_func

// Handle GET messages - read variable values and return in USB message daa
#define if_msg_valid(test)
#define v_byte(var, ofs)   data[ofs] = cfg.var
#define v_ui16(var, ofs)   ui16Wire(data+ofs, cfg.var)
#define v_pin(var, ofs)    pinNameWire(data+ofs, cfg.var)
#define v_func  configVarGet
#include "cfgVarMsgMap.h"


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
                wizVal[i] = (uint8_t)round(outLevel[i]/255.0 * 48.0);
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
            cfg.plunger.cal.begin();
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
                cfg.plunger.cal.zero, cfg.plunger.cal.max,
                nvm.valid());
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
            
        case 7:
            // 7 = Device ID report
            // (No parameters)
            js.reportID();
            break;
            
        case 8:
            // 8 = Engage/disengage night mode.
            //     data[2] = 1 to engage, 0 to disengage
            setNightMode(data[2]);
            break;
        }
    }
    else if (data[0] == 66)
    {
        // Extended protocol - Set configuration variable.
        // The second byte of the message is the ID of the variable
        // to update, and the remaining bytes give the new value,
        // in a variable-dependent format.
        configVarSet(data);
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
            uint8_t b = data[i-i0+1];
            outLevel[i] = b;
            
            // if it's in the basic LedWiz output set, set the LedWiz
            // profile value to 255, which means "use outLevel"
            if (i < 32) 
                wizVal[i] = 255;
                
            // set the output
            lwPin[i]->set(b);
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

    // load the saved configuration (or set factory defaults if no flash
    // configuration has ever been saved)
    loadConfigFromFlash();
    
    // initialize the diagnostic LEDs
    initDiagLEDs(cfg);

    // set up the pre-connected ticker
    Ticker preConnectTicker;
    preConnectTicker.attach(preConnectFlasher, 3);

    // we're not connected/awake yet
    bool connected = false;
    Timer connectChangeTimer;

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
        
    // start the TV timer, if applicable
    startTVTimer(cfg);
    
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
    
    // last plunger report position, on the 0.0..1.0 normalized scale
    float pos = 0;
    
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
    
    Timer dbgTimer; dbgTimer.start(); // $$$  plunger debug report timer
    
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
                    
                    // begin the plunger calibration limits
                    cfg.plunger.cal.begin();
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
 
        // If the plunger is enabled, and we're not in calibration mode, and 
        // we're not already in a firing event, and the last plunger reading had 
        // the plunger pulled back at least a bit, watch for plunger release 
        // events until it's time for our next USB report.
        if (!firing && calBtnState != 3 && cfg.plunger.enabled && z >= JOYMAX/6)
        {
            // monitor the plunger until it's time for our next report
            for (int i = 0 ; i < 20 && jsReportTimer.read_ms() < 12 ; ++i)
            {
                // do a fast low-res scan; if it's at or past the zero point,
                // start a firing event
                float pos0;
                if (plungerSensor->lowResScan(pos0) && pos0 <= cfg.plunger.cal.zero)
                {
                    firing = 1;
                    break;
                }
            }
        }

        // read the plunger sensor, if it's enabled and we're not in firing mode
        if (cfg.plunger.enabled && !firing)
        {
            // start with the previous reading, in case we don't have a
            // clear result on this frame
            int znew = z;
            if (plungerSensor->highResScan(pos))
            {
                // We have a new reading.  If we're in calibration mode, use it
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
                    znew = int(round(pos * JOYMAX));
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
                    znew = int(round(
                        (pos - cfg.plunger.cal.zero)
                        / (cfg.plunger.cal.max - cfg.plunger.cal.zero) 
                        * JOYMAX));
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
                float pos0;
                if (plungerSensor->lowResScan(pos0))
                {
                    int pos1 = pos0;
                    Timer tw;
                    tw.start();
                    while (tw.read_ms() < 6)
                    {
                        // read the new position
                        float pos2;
                        if (plungerSensor->lowResScan(pos2))
                        {
                            // If it's stable over consecutive readings, stop looping.
                            // Count it as stable if the position is within about 1/8".
                            // The overall travel of a standard plunger is about 3.2", 
                            // so on our normalized 0.0..1.0 scale, 1.0 equals 3.2",
                            // thus 1" = .3125 and 1/8" = .0391.
                            if (fabs(pos2 - pos1) < .0391f)
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
                            if (pos1 < cfg.plunger.cal.zero && fabs(pos2 - pos0) > .0391f)
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
                const int pushThreshold = int(-JOYMAX/3.0 * cfg.plunger.zbLaunchBall.pushDistance/1000.0);
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
        bool newConnected = js.isConnected() && !js.isSuspended();
        if (newConnected != connected)
        {
            // give it a few seconds to stabilize
            connectChangeTimer.start();
            if (connectChangeTimer.read() > 3)
            {
                // note the new status
                connected = newConnected;
                
                // done with the change timer for this round - reset it for next time
                connectChangeTimer.stop();
                connectChangeTimer.reset();
                
                // adjust to the new status
                if (connected)
                {
                    // We're newly connected.  This means we just powered on, we were
                    // just plugged in to the PC USB port after being unplugged, or the
                    // PC just came out of sleep/suspend mode and resumed the connection.
                    // In any of these cases, we can now assume that the PC power supply
                    // is on (the PC must be on for the USB connection to be running, and
                    // if the PC is on, its power supply is on).  This also means that 
                    // power to any external output controller chips (TLC5940, 74HC595)
                    // is now on, because those have to be powered from the PC power
                    // supply to allow for a reliable data connection to the KL25Z.
                    // We can thus now set clear initial output state in those chips and
                    // enable their outputs.
                    if (tlc5940 != 0)
                    {
                        tlc5940->update(true);
                        tlc5940->enable(true);
                    }
                    if (hc595 != 0)
                    {
                        hc595->update(true);
                        hc595->enable(true);
                    }
                }
                else
                {
                    // We're no longer connected.  Turn off all outputs.
                    allOutputsOff();
                    
                    // The KL25Z runs off of USB power, so we might (depending on the PC
                    // and OS configuration) continue to receive power even when the main
                    // PC power supply is turned off, such as in soft-off or suspend/sleep
                    // mode.  Any external output controller chips (TLC5940, 74HC595) might
                    // be powered from the PC power supply directly rather than from our
                    // USB power, so they might be powered off even when we're still running.
                    // To ensure cleaner startup when the power comes back on, globally
                    // disable the outputs.  The global disable signals come from GPIO lines
                    // that remain powered as long as the KL25Z is powered, so these modes
                    // will apply smoothly across power state transitions in the external
                    // hardware.  That is, when the external chips are powered up, they'll
                    // see the global disable signals as stable voltage inputs immediately,
                    // which will cause them to suppress any output triggering.  This ensures
                    // that we don't fire any solenoids or flash any lights spuriously when
                    // the power first comes on.
                    if (tlc5940 != 0)
                        tlc5940->enable(false);
                    if (hc595 != 0)
                        hc595->enable(false);
                }
            }
        }

    // $$$
        if (dbgTimer.read() > 10) {
            dbgTimer.reset();
            if (plungerSensor != 0 && (cfg.plunger.sensorType == PlungerType_TSL1410RS || cfg.plunger.sensorType == PlungerType_TSL1410RP))
            {
                PlungerSensorTSL1410R *ps = (PlungerSensorTSL1410R *)plungerSensor;
                printf("average plunger read time: %f ms (total=%f, n=%d)\r\n", ps->ccd.totalTime*1000.0 / ps->ccd.nRuns, ps->ccd.totalTime, ps->ccd.nRuns);
            }
        }
    // end $$$
        
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
                // Our outgoing joystick messages aren't going through, even though we
                // think we're still connected.  This indicates that one or more of our
                // USB endpoints have stopped working, which can happen as a result of
                // bugs in the USB HAL or latency responding to a USB IRQ.  Show a
                // distinctive diagnostic flash to signal the error.  I haven't found a 
                // way to recover from this class of error other than rebooting the MCU, 
                // so the goal is to fix the HAL so that this error never happens.  
                //
                // NOTE!  This diagnostic code *hopefully* shouldn't occur.  It happened
                // in the past due to a number of bugs in the mbed KL25Z USB HAL that
                // I've since fixed.  I think I found all of the cases that caused it,
                // but I'm leaving the diagnostics here in case there are other bugs
                // still lurking that can trigger the same symptoms.
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
