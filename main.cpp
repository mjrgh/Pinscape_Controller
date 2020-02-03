/* Copyright 2014, 2016 M J Roberts, MIT License
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
// This project implements an I/O controller for virtual pinball cabinets.  The
// controller's function is to connect Visual Pinball (and other Windows pinball 
// emulators) with physical devices in the cabinet:  buttons, sensors, and 
// feedback devices that create visual or mechanical effects during play.  
//
// The controller can perform several different functions, which can be used 
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
//  - Plunger position sensing, with multiple sensor options.  To use this feature,
//    you need to choose a sensor and set it up, connect the sensor electrically to 
//    the KL25Z, and configure the Pinscape software on the KL25Z to let it know how 
//    the sensor is hooked up.  The Pinscape software monitors the sensor and sends
//    readings to Visual Pinball via the joystick Z axis.  VP and other PC software
//    have native support for this type of input; as with the nudge setup, you just 
//    have to set some options in VP to activate the plunger.
//
//    We support several sensor types:
//
//    - AEDR-8300-1K2 optical encoders.  These are quadrature encoders with
//      reflective optical sensing and built-in lighting and optics.  The sensor
//      is attached to the plunger so that it moves with the plunger, and slides
//      along a guide rail with a reflective pattern of regularly spaces bars 
//      for the encoder to read.  We read the plunger position by counting the
//      bars the sensor passes as it moves across the rail.  This is the newest
//      option, and it's my current favorite because it's highly accurate,
//      precise, and fast, plus it's relatively inexpensive.
//
//    - Slide potentiometers.  There are slide potentioneters available with a
//      long enough travel distance (at least 85mm) to cover the plunger travel.
//      Attach the plunger to the potentiometer knob so that the moving the
//      plunger moves the pot knob.  We sense the position by simply reading
//      the analog voltage on the pot brush.  A pot with a "linear taper" (that
//      is, the resistance varies linearly with the position) is required.
//      This option is cheap, easy to set up, and works well.
//
//    - VL6108X time-of-flight distance sensor.  This is an optical distance
//      sensor that measures the distance to a nearby object (within about 10cm)
//      by measuring the travel time for reflected pulses of light.  It's fairly
//      cheap and easy to set up, but I don't recommend it because it has very
//      low precision.
//
//    - TSL1410R/TSL1412R linear array optical sensors.  These are large optical
//      sensors with the pixels arranged in a single row.  The pixel arrays are
//      large enough on these to cover the travel distance of the plunger, so we
//      can set up the sensor near the plunger in such a way that the plunger 
//      casts a shadow on the sensor.  We detect the plunger position by finding
//      the edge of the sahdow in the image.  The optics for this setup are very
//      simple since we don't need any lenses.  This was the first sensor we
//      supported, and works very well, but unfortunately the sensor is difficult
//      to find now since it's been discontinued by the manufacturer.
//
//    The v2 Build Guide has details on how to build and configure all of the
//    sensor options.
//
//    Visual Pinball has built-in support for plunger devices like this one, but 
//    some older VP tables (particularly for VP 9) can't use it without some
//    modifications to their scripting.  The Build Guide has advice on how to
//    fix up VP tables to add plunger support when necessary.
//
//  - Button input wiring.  You can assign GPIO ports as inputs for physical
//    pinball-style buttons, such as flipper buttons, a Start button, coin
//    chute switches, tilt bobs, and service panel buttons.  You can configure
//    each button input to report a keyboard key or joystick button press to
//    the PC when the physical button is pushed.
//
//  - LedWiz emulation.  The KL25Z can pretend to be an LedWiz device.  This lets
//    you connect feedback devices (lights, solenoids, motors) to GPIO ports on the 
//    KL25Z, and lets PC software (such as Visual Pinball) control them during game 
//    play to create a more immersive playing experience.  The Pinscape software
//    presents itself to the host as an LedWiz device and accepts the full LedWiz
//    command set, so software on the PC designed for real LedWiz'es can control
//    attached devices without any modifications.
//
//    Even though the software provides a very thorough LedWiz emulation, the KL25Z
//    GPIO hardware design imposes some serious limitations.  The big one is that
//    the KL25Z only has 10 PWM channels, meaning that only 10 ports can have
//    varying-intensity outputs (e.g., for controlling the brightness level of an
//    LED or the speed or a motor).  You can control more than 10 output ports, but
//    only 10 can have PWM control; the rest are simple "digital" ports that can only
//    be switched fully on or fully off.  The second limitation is that the KL25Z
//    just doesn't have that many GPIO ports overall.  There are enough to populate
//    all 32 button inputs OR all 32 LedWiz outputs, but not both.  The default is
//    to assign 24 buttons and 22 LedWiz ports; you can change this balance to trade
//    off more outputs for fewer inputs, or vice versa.  The third limitation is that
//    the KL25Z GPIO pins have *very* tiny amperage limits - just 4mA, which isn't
//    even enough to control a small LED.  So in order to connect any kind of feedback
//    device to an output, you *must* build some external circuitry to boost the
//    current handing.  The Build Guide has a reference circuit design for this
//    purpose that's simple and inexpensive to build.
//
//  - Enhanced LedWiz emulation with TLC5940 and/or TLC59116 PWM controller chips. 
//    You can attach external PWM chips for controlling device outputs, instead of
//    using (or in addition to) the on-board GPIO ports as described above.  The 
//    software can control a set of daisy-chained TLC5940 or TLC59116 chips.  Each
//    chip provides 16 PWM outputs, so you just need two of them to get the full 
//    complement of 32 output ports of a real LedWiz.  You can hook up even more, 
//    though.  Four chips gives you 64 ports, which should be plenty for nearly any 
//    virtual pinball project.  
//
//    The Pinscape Expansion Board project (which appeared in early 2016) provides
//    a reference hardware design, with EAGLE circuit board layouts, that takes full
//    advantage of the TLC5940 capability.  It lets you create a customized set of
//    outputs with full PWM control and power handling for high-current devices
//    built in to the boards.
//
//    To accommodate the larger supply of ports possible with the external chips,
//    the controller software provides a custom, extended version of the LedWiz 
//    protocol that can handle up to 128 ports.  Legacy PC software designed only
//    for the original LedWiz obviously can't use the extended protocol, and thus 
//    can't take advantage of its extra capabilities, but the latest version of 
//    DOF (DirectOutput Framework) *does*  know the new language and can take full
//    advantage.  Older software will still work, though - the new extensions are 
//    all backwards compatible, so old software that only knows about the original 
//    LedWiz protocol will still work, with the limitation that it can only access 
//    the first 32 ports.  In addition, we provide a replacement LEDWIZ.DLL that 
//    creates virtual LedWiz units representing additional ports beyond the first
//    32.  This allows legacy LedWiz client software to address all ports by
//    making them think that you have several physical LedWiz units installed.
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
//    software to sense the power supply status.  The Build Guide has details 
//    on the necessary circuitry.  You can use this to switch your TV on via a
//    hardwired connection to the TV's "on" button, which requires taking the
//    TV apart to gain access to its internal wiring, or optionally via the IR
//    remote control transmitter feature below.
//
//  - Infrared (IR) remote control receiver and transmitter.  You can attach an
//    IR LED and/or an IR sensor (we recommend the TSOP384xx series) to make the
//    KL25Z capable of sending and/or receiving IR remote control signals.  This
//    can be used with the TV ON feature above to turn your TV(s) on when the
//    system power comes on by sending the "on" command to them via IR, as though
//    you pressed the "on" button on the remote control.  The sensor lets the
//    Pinscape software learn the IR codes from your existing remotes, in the
//    same manner as a handheld universal remote control, and the IR LED lets
//    it transmit learned codes.  The sensor can also be used to receive codes
//    during normal operation and turn them into PC keystrokes; this lets you
//    access extra commands on the PC without adding more buttons to your
//    cabinet.  The IR LED can also be used to transmit other codes when you
//    press selected cabinet buttons, allowing you to assign cabinet buttons
//    to send IR commands to your cabinet TV or other devices.
//
//
//
// STATUS LIGHTS:  The on-board LED on the KL25Z flashes to indicate the current 
// device status.  The flash patterns are:
//
//    short yellow flash = waiting to connect
//
//    short red flash = the connection is suspended (the host is in sleep
//        or suspend mode, the USB cable is unplugged after a connection
//        has been established)
//
//    two short red flashes = connection lost (the device should immediately
//        go back to short-yellow "waiting to reconnect" mode when a connection
//        is lost, so this display shouldn't normally appear)
//
//    long red/yellow = USB connection problem.  The device still has a USB
//        connection to the host (or so it appears to the device), but data 
//        transmissions are failing.
//
//    medium blue flash = TV ON delay timer running.  This means that the
//        power to the secondary PSU has just been turned on, and the TV ON
//        timer is waiting for the configured delay time before pulsing the
//        TV power button relay.  This is only shown if the TV ON feature is
//        enabled.
//
//    long yellow/green = everything's working, but the plunger hasn't
//        been calibrated.  Follow the calibration procedure described in
//        the project documentation.  This flash mode won't appear if there's
//        no plunger sensor configured.
//
//    alternating blue/green = everything's working normally, and plunger
//        calibration has been completed (or there's no plunger attached)
//
//    fast red/purple = out of memory.  The controller halts and displays
//        this diagnostic code until you manually reset it.  If this happens,
//        it's probably because the configuration is too complex, in which
//        case the same error will occur after the reset.  If it's stuck
//        in this cycle, you'll have to restore the default configuration
//        by re-installing the controller software (the Pinscape .bin file).
//
//
// USB PROTOCOL:  Most of our USB messaging is through standard USB HID
// classes (joystick, keyboard).  We also accept control messages on our
// primary HID interface "OUT endpoint" using a custom protocol that's
// not defined in any USB standards (we do have to provide a USB HID
// Report Descriptor for it, but this just describes the protocol as
// opaque vendor-defined bytes).  The control protocol incorporates the 
// LedWiz protocol as a subset, and adds our own private extensions.
// For full details, see USBProtocol.h.


#include "mbed.h"
#include "math.h"
#include "diags.h"
#include "pinscape.h"
#include "NewMalloc.h"
#include "USBJoystick.h"
#include "MMA8451Q.h"
#include "FreescaleIAP.h"
#include "crc32.h"
#include "TLC5940.h"
#include "TLC59116.h"
#include "74HC595.h"
#include "nvm.h"
#include "TinyDigitalIn.h"
#include "IRReceiver.h"
#include "IRTransmitter.h"
#include "NewPwm.h"

// plunger sensors
#include "plunger.h"
#include "edgeSensor.h"
#include "potSensor.h"
#include "quadSensor.h"
#include "nullSensor.h"
#include "barCodeSensor.h"
#include "distanceSensor.h"
#include "tsl14xxSensor.h"
#include "rotarySensor.h"
#include "tcd1103Sensor.h"


#define DECL_EXTERNS
#include "config.h"


// --------------------------------------------------------------------------
// 
// OpenSDA module identifier.  This is for the benefit of the Windows
// configuration tool.  When the config tool installs a .bin file onto
// the KL25Z, it will first find the sentinel string within the .bin file,
// and patch the "\0" bytes that follow the sentinel string with the 
// OpenSDA module ID data.  This allows us to report the OpenSDA 
// identifiers back to the host system via USB, which in turn allows the 
// config tool to figure out which OpenSDA MSD (mass storage device - a 
// virtual disk drive) correlates to which Pinscape controller USB 
// interface.  
// 
// This is only important if multiple Pinscape devices are attached to 
// the same host.  There doesn't seem to be any other way to figure out 
// which OpenSDA MSD corresponds to which KL25Z USB interface; the OpenSDA 
// MSD doesn't report the KL25Z CPU ID anywhere, and the KL25Z doesn't
// have any way to learn about the OpenSDA module it's connected to.  The
// only way to pass this information to the KL25Z side that I can come up 
// with is to have the Windows host embed it in the .bin file before 
// downloading it to the OpenSDA MSD.
//
// We initialize the const data buffer (the part after the sentinel string)
// with all "\0" bytes, so that's what will be in the executable image that
// comes out of the mbed compiler.  If you manually install the resulting
// .bin file onto the KL25Z (via the Windows desktop, say), the "\0" bytes
// will stay this way and read as all 0's at run-time.  Since a real TUID
// would never be all 0's, that tells us that we were never patched and
// thus don't have any information on the OpenSDA module.
//
const char *getOpenSDAID()
{
    #define OPENSDA_PREFIX "///Pinscape.OpenSDA.TUID///"
    static const char OpenSDA[] = OPENSDA_PREFIX "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0///";
    const size_t OpenSDA_prefix_length = sizeof(OPENSDA_PREFIX) - 1;
    
    return OpenSDA + OpenSDA_prefix_length;
}

// --------------------------------------------------------------------------
//
// Build ID.  We use the date and time of compiling the program as a build
// identifier.  It would be a little nicer to use a simple serial number
// instead, but the mbed platform doesn't have a way to automate that.  The
// timestamp is a pretty good proxy for a serial number in that it will
// naturally increase on each new build, which is the primary property we
// want from this.
//
// As with the embedded OpenSDA ID, we store the build timestamp with a
// sentinel string prefix, to allow automated tools to find the static data
// in the .bin file by searching for the sentinel string.  In contrast to 
// the OpenSDA ID, the value we store here is for tools to extract rather 
// than store, since we automatically populate it via the preprocessor 
// macros.
//
const char *getBuildID()
{
    #define BUILDID_PREFIX "///Pinscape.Build.ID///"
    static const char BuildID[] = BUILDID_PREFIX __DATE__ " " __TIME__ "///";
    const size_t BuildID_prefix_length = sizeof(BUILDID_PREFIX) - 1;
    
    return BuildID + BuildID_prefix_length;
}

// --------------------------------------------------------------------------
// Main loop iteration timing statistics.  Collected only if 
// ENABLE_DIAGNOSTICS is set in diags.h.
#if ENABLE_DIAGNOSTICS
  uint64_t mainLoopIterTime, mainLoopIterCheckpt[15], mainLoopIterCount;
  uint64_t mainLoopMsgTime, mainLoopMsgCount;
  Timer mainLoopTimer;
#endif


// ---------------------------------------------------------------------------
//
// Forward declarations
//
void setNightMode(bool on);
void toggleNightMode();

// ---------------------------------------------------------------------------
// utilities

// int/float point square of a number
inline int square(int x) { return x*x; }
inline float square(float x) { return x*x; }

// floating point rounding
inline float round(float x) { return x > 0 ? floor(x + 0.5) : ceil(x - 0.5); }


// --------------------------------------------------------------------------
// 
// Extended verison of Timer class.  This adds the ability to interrogate
// the running state.
//
class ExtTimer: public Timer
{
public:
    ExtTimer() : running(false) { }

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
const uint16_t USB_VERSION_NO = 0x000A;

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

// Convert "wire" (USB) pin codes to/from PinName values.
// 
// The internal mbed PinName format is 
//
//   ((port) << PORT_SHIFT) | (pin << 2)    // MBED FORMAT
//
// where 'port' is 0-4 for Port A to Port E, and 'pin' is
// 0 to 31.  E.g., E31 is (4 << PORT_SHIFT) | (31<<2).
//
// We remap this to our more compact wire format where each
// pin name fits in 8 bits:
//
//   ((port) << 5) | pin)                   // WIRE FORMAT
//
// E.g., E31 is (4 << 5) | 31.
//
// Wire code FF corresponds to PinName NC (not connected).
//
inline PinName wirePinName(uint8_t c)
{
    if (c == 0xFF)
        return NC;                                  // 0xFF -> NC
    else 
        return PinName(
            (int(c & 0xE0) << (PORT_SHIFT - 5))      // top three bits are the port
            | (int(c & 0x1F) << 2));                // bottom five bits are pin
}
inline void pinNameWire(uint8_t *b, PinName n)
{
    *b = PINNAME_TO_WIRE(n);
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

// Power on timer state for diagnostics.  We flash the blue LED when
// nothing else is going on.  State 0-1 = off, 2-3 = on blue.  Also
// show red when transmitting an LED signal, indicated by state 4.
uint8_t powerTimerDiagState = 0;

// Show the indicated pattern on the diagnostic LEDs.  0 is off, 1 is
// on, and -1 is no change (leaves the current setting intact).
static uint8_t diagLEDState = 0;
void diagLED(int r, int g, int b)
{
    // remember the new state
    diagLEDState = r | (g << 1) | (b << 2);
    
    // if turning everything off, use the power timer state instead, 
    // applying it to the blue LED
    if (diagLEDState == 0)
    {
        b = (powerTimerDiagState == 2 || powerTimerDiagState == 3);
        r = (powerTimerDiagState == 4);
    }
        
    // set the new state
    if (ledR != 0 && r != -1) ledR->write(!r);
    if (ledG != 0 && g != -1) ledG->write(!g);
    if (ledB != 0 && b != -1) ledB->write(!b);
}

// update the LEDs with the current state
void diagLED(void)
{
    diagLED(
        diagLEDState & 0x01,
        (diagLEDState >> 1) & 0x01,
        (diagLEDState >> 2) & 0x01);
}

// check an output port or pin assignment to see if it conflicts with
// an on-board LED segment
struct LedSeg 
{ 
    bool r, g, b; 
    LedSeg() { r = g = b = false; } 

    // check an output port to see if it conflicts with one of the LED ports
    void check(LedWizPortCfg &pc)
    {
        // if it's a GPIO, check to see if it's assigned to one of
        // our on-board LED segments
        int t = pc.typ;
        if (t == PortTypeGPIOPWM || t == PortTypeGPIODig)
            check(pc.pin);
    }

    // check a pin to see if it conflicts with one of the diagnostic LED ports    
    void check(uint8_t pinId)
    {
        PinName pin = wirePinName(pinId);
        if (pin == LED1)
            r = true;
        else if (pin == LED2)
            g = true;
        else if (pin == LED3)
            b = true;
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
        
    // check the button inputs
    for (int i = 0 ; i < countof(cfg.button) ; ++i)
        l.check(cfg.button[i].pin);
        
    // check plunger inputs
    if (cfg.plunger.enabled && cfg.plunger.sensorType != PlungerType_None)
    {
        for (int i = 0 ; i < countof(cfg.plunger.sensorPin) ; ++i)
            l.check(cfg.plunger.sensorPin[i]);
    }
    
    // check the TV ON pin assignments
    l.check(cfg.TVON.statusPin);
    l.check(cfg.TVON.latchPin);
    l.check(cfg.TVON.relayPin);

    // check  the TLC5940 pins
    if (cfg.tlc5940.nchips != 0)
    {
        l.check(cfg.tlc5940.sin);
        l.check(cfg.tlc5940.sclk);
        l.check(cfg.tlc5940.xlat);
        l.check(cfg.tlc5940.blank);
        l.check(cfg.tlc5940.gsclk);
    }
    
    // check 74HC595 pin assignments
    if (cfg.hc595.nchips != 0)
    {
        l.check(cfg.hc595.sin);
        l.check(cfg.hc595.sclk);
        l.check(cfg.hc595.latch);
        l.check(cfg.hc595.ena);
    }
    
    // check TLC59116 pin assignments
    if (cfg.tlc59116.chipMask != 0)
    {
        l.check(cfg.tlc59116.sda);
        l.check(cfg.tlc59116.scl);
        l.check(cfg.tlc59116.reset);
    }
    
    // check the IR remove control hardware
    l.check(cfg.IR.sensor);
    l.check(cfg.IR.emitter);
    
    // We now know which segments are taken for other uses and which
    // are free.  Create diagnostic ports for the ones not claimed for
    // other purposes.
    if (!l.r) ledR = new DigitalOut(LED1, 1);
    if (!l.g) ledG = new DigitalOut(LED2, 1);
    if (!l.b) ledB = new DigitalOut(LED3, 1);
}


// ---------------------------------------------------------------------------
//
// LedWiz emulation
//

// LedWiz output states.
//
// The LedWiz protocol has two separate control axes for each output.
// One axis is its on/off state; the other is its "profile" state, which
// is either a fixed brightness or a blinking pattern for the light.
// The two axes are independent.
//
// Even though the original LedWiz protocol can only access 32 ports, we
// maintain LedWiz state for every port, even if we have more than 32.  Our
// extended protocol allows the client to send LedWiz-style messages that
// control any set of ports.  A replacement LEDWIZ.DLL can make a single
// Pinscape unit look like multiple virtual LedWiz units to legacy clients,
// allowing them to control all of our ports.  The clients will still be
// using LedWiz-style states to control the ports, so we need to support
// the LedWiz scheme with separate on/off and brightness control per port.

// On/off state for each LedWiz output
static uint8_t *wizOn;

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
// (Note that value 49 isn't documented in the LedWiz spec, but real
// LedWiz units treat it as equivalent to 48, and some PC software uses
// it, so we need to accept it for compatibility.)
static uint8_t *wizVal;

// Current actual brightness for each output.  This is a simple linear
// value on a 0..255 scale.  This is EITHER the linear brightness computed 
// from the LedWiz setting for the port, OR the 0..255 value set explicitly 
// by the extended protocol:
//
// - If the last command that updated the port was an extended protocol 
//   SET BRIGHTNESS command, this is the value set by that command.  In
//   addition, wizOn[port] is set to 0 if the brightness is 0, 1 otherwise;
//   and wizVal[port] is set to the brightness rescaled to the 0..48 range
//   if the brightness is non-zero.
//
// - If the last command that updated the port was an LedWiz command
//   (SBA/PBA/SBX/PBX), this contains the brightness value computed from
//   the combination of wizOn[port] and wizVal[port].  If wizOn[port] is 
//   zero, this is simply 0, otherwise it's wizVal[port] rescaled to the
//   0..255 range.
//
// - For a port set to wizOn[port]=1 and wizVal[port] in 129..132, this is
//   also updated continuously to reflect the current flashing brightness
//   level.
//
static uint8_t *outLevel;


// LedWiz flash speed.  This is a value from 1 to 7 giving the pulse
// rate for lights in blinking states.  The LedWiz API doesn't document
// what the numbers mean in real time units, but by observation, the
// "speed" setting represents the period of the flash cycle in 0.25s
// units, so speed 1 = 0.25 period = 4Hz, speed 7 = 1.75s period = 0.57Hz.
// The period is the full cycle time of the flash waveform.
//
// Each bank of 32 lights has its independent own pulse rate, so we need 
// one entry per bank.  Each bank has 32 outputs, so we need a total of
// ceil(number_of_physical_outputs/32) entries.  Note that we could allocate 
// this dynamically once we know the number of actual outputs, but the 
// upper limit is low enough that it's more efficient to use a fixed array
// at the maximum size.
static const int MAX_LW_BANKS = (MAX_OUT_PORTS+31)/32;
static uint8_t wizSpeed[MAX_LW_BANKS];

// Current starting output index for "PBA" messages from the PC (using
// the LedWiz USB protocol).  Each PBA message implicitly uses the
// current index as the starting point for the ports referenced in
// the message, and increases it (by 8) for the next call.
static int pbaIdx = 0;


// ---------------------------------------------------------------------------
//
// Output Ports
//
// There are two way to connect outputs.  First, you can use the on-board
// GPIO ports to implement device outputs: each LedWiz software port is
// connected to a physical GPIO pin on the KL25Z.  This has some pretty
// strict limits, though.  The KL25Z only has 10 PWM channels, so only 10
// GPIO LedWiz ports can be made dimmable; the rest are strictly on/off.  
// The KL25Z also simply doesn't have enough exposed GPIO ports overall to 
// support all of the features the software supports.  The software allows 
// for up to 128 outputs, 48 button inputs, plunger input (requiring 1-5 
// GPIO pins), and various other external devices.  The KL25Z only exposes
// about 50 GPIO pins.  So if you want to do everything with GPIO ports,
// you have to ration pins among features.
//
// To overcome some of these limitations, we also support several external
// peripheral controllers that allow adding many more outputs, using only
// a small number of GPIO pins to interface with the peripherals:
//
// - TLC5940 PWM controller chips.  Each TLC5940 provides 16 ports with
//   12-bit PWM, and multiple TLC5940 chips can be daisy-chained.  The
//   chips connect via 5 GPIO pins, and since they're daisy-chainable,
//   one set of 5 pins can control any number of the chips.  So this chip
//   effectively converts 5 GPIO pins into almost any number of PWM outputs.
//
// - TLC59116 PWM controller chips.  These are similar to the TLC5940 but
//   a newer generation with an improved design.  These use an I2C bus,
//   allowing up to 14 chips to be connected via 3 GPIO pins.
//
// - 74HC595 shift register chips.  These provide 8 digital (on/off only)
//   outputs per chip.  These need 4 GPIO pins, and like the other can be
//   daisy chained to add more outputs without using more GPIO pins.  These
//   are advantageous for outputs that don't require PWM, since the data
//   transfer sizes are so much smaller.  The expansion boards use these
//   for the chime board outputs.
//
// Direct GPIO output ports and peripheral controllers can be mixed and
// matched in one system.  The assignment of pins to ports and the 
// configuration of peripheral controllers is all handled in the software
// setup, so a physical system can be expanded and updated at any time.
//
// To handle the diversity of output port types, we start with an abstract
// base class for outputs.  Each type of physical output interface has a
// concrete subclass.  During initialization, we create the appropriate
// subclass for each software port, mapping it to the assigned GPIO pin 
// or peripheral port.   Most of the rest of the software only cares about
// the abstract interface, so once the subclassed port objects are set up,
// the rest of the system can control the ports without knowing which types
// of physical devices they're connected to.


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
    // underlying physical output
    LwOut *out;
};

// Global ZB Launch Ball state
bool zbLaunchOn = false;

// ZB Launch Ball output.  This is layered on a port (physical or virtual)
// to track the ZB Launch Ball signal.
class LwZbLaunchOut: public LwOut
{
public:
    LwZbLaunchOut(LwOut *o) : out(o) { }
    virtual void set(uint8_t val)
    {
        // update the global ZB Launch Ball state
        zbLaunchOn = (val != 0);
        
        // pass it along to the underlying port, in case it's a physical output
        out->set(val);
    }
        
private:
    // underlying physical or virtual output
    LwOut *out;
};


// Gamma correction table for 8-bit input values
static const uint8_t dof_to_gamma_8bit[] = {
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
    virtual void set(uint8_t val) { out->set(dof_to_gamma_8bit[val]); }
    
private:
    LwOut *out;
};

// Global night mode flag.  To minimize overhead when reporting
// the status, we set this to the status report flag bit for
// night mode, 0x02, when engaged.
static uint8_t nightMode = 0x00;

// Noisy output.  This is a filter object that we layer on top of
// a physical pin output.  This filter disables the port when night
// mode is engaged.
class LwNoisyOut: public LwOut
{
public:
    LwNoisyOut(LwOut *o) : out(o) { }
    virtual void set(uint8_t val) { out->set(nightMode ? 0 : val); }
    
private:
    LwOut *out;
};

// Night Mode indicator output.  This is a filter object that we
// layer on top of a physical pin output.  This filter ignores the
// host value and simply shows the night mode status.
class LwNightModeIndicatorOut: public LwOut
{
public:
    LwNightModeIndicatorOut(LwOut *o) : out(o) { }
    virtual void set(uint8_t)
    {
        // ignore the host value and simply show the current 
        // night mode setting
        out->set(nightMode ? 255 : 0);
    }

private:
    LwOut *out;
};


// Flipper Logic output.  This is a filter object that we layer on
// top of a physical pin output.
//
// A Flipper Logic output is effectively a digital output from the
// client's perspective, in that it ignores the intensity level and
// only pays attention to the ON/OFF state.  0 is OFF and any other
// level is ON.
//
// In terms of the physical output, though, we do use varying power.
// It's just that the varying power isn't under the client's control;
// we control it according to our flipperLogic settings:
//
// - When the software port transitions from OFF (0 brightness) to ON
//   (any non-zero brightness level), we set the physical port to 100%
//   power and start a timer.
//
// - When the full power time in our flipperLogic settings elapses,
//   if the software port is still ON, we reduce the physical port to
//   the PWM level in our flipperLogic setting.
//
class LwFlipperLogicOut: public LwOut
{
public:
    // Set up the output.  'params' is the flipperLogic value from
    // the configuration.
    LwFlipperLogicOut(LwOut *o, uint8_t params)
        : out(o), params(params)
    {
        // initially OFF
        state = 0;
    }
    
    virtual void set(uint8_t level)
    {
        // remember the new nominal level set by the client
        val = level;
        
        // update the physical output according to our current timing state
        switch (state)
        {
        case 0:
            // We're currently off.  If the new level is non-zero, switch
            // to state 1 (initial full-power interval) and set the requested
            // level.  If the new level is zero, we're switching from off to
            // off, so there's no change.
            if (level != 0)
            {
                // switch to state 1 (initial full-power interval)
                state = 1;
                
                // set the requested output level - there's no limit during
                // the initial full-power interval, so set the exact level
                // requested
                out->set(level);

                // add myself to the pending timer list
                pending[nPending++] = this;
                
                // note the starting time
                t0 = timer.read_us();
            }
            break;
            
        case 1:
            // Initial full-power interval.  If the new level is non-zero,
            // simply apply the new level as requested, since there's no
            // limit during this period.  If the new level is zero, shut
            // off the output and cancel the pending timer.
            out->set(level);
            if (level == 0)
            {
                // We're switching off.  In state 1, we have a pending timer,
                // so we need to remove it from the list.
                for (int i = 0 ; i < nPending ; ++i)
                {
                    // is this us?
                    if (pending[i] == this)
                    {
                        // remove myself by replacing the slot with the
                        // last list entry
                        pending[i] = pending[--nPending];
                        
                        // no need to look any further
                        break;
                    }
                }
                
                // switch to state 0 (off)
                state = 0;
            }
            break;
            
        case 2: 
            // Hold interval.  If the new level is zero, switch to state
            // 0 (off).  If the new level is non-zero, stay in the hold
            // state, and set the new level, applying the hold power setting
            // as the upper bound.
            if (level == 0)
            {
                // switching off - turn off the physical output
                out->set(0);
                
                // go to state 0 (off)
                state = 0;
            }
            else
            {
                // staying on - set the new physical output power to the
                // lower of the requested power and the hold power
                uint8_t hold = holdPower();
                out->set(level < hold ? level : hold);
            }
            break;
        }
    }
    
    // Class initialization
    static void classInit(Config &cfg)
    {
        // Count the Flipper Logic outputs in the configuration.  We
        // need to allocate enough pending timer list space to accommodate
        // all of these outputs.
        int n = 0;
        for (int i = 0 ; i < MAX_OUT_PORTS ; ++i)
        {
            // if this port is active and marked as Flipper Logic, count it
            if (cfg.outPort[i].typ != PortTypeDisabled
                && (cfg.outPort[i].flags & PortFlagFlipperLogic) != 0)
                ++n;
        }
        
        // allocate space for the pending timer list
        pending = new LwFlipperLogicOut*[n];
        
        // there's nothing in the pending list yet
        nPending = 0;
        
        // Start our shared timer.  The epoch is arbitrary, since we only
        // use it to figure elapsed times.
        timer.start();
    }

    // Check for ports with pending timers.  The main routine should
    // call this on each iteration to process our state transitions.
    static void poll()
    {
        // note the current time
        uint32_t t = timer.read_us();
        
        // go through the timer list
        for (int i = 0 ; i < nPending ; )
        {
            // get the port
            LwFlipperLogicOut *port = pending[i];
            
            // assume we'll keep it
            bool remove = false;
            
            // check if the port is still on
            if (port->state != 0)
            {
                // it's still on - check if the initial full power time has elapsed
                if (uint32_t(t - port->t0) > port->fullPowerTime_us())
                {
                    // done with the full power interval - switch to hold state
                    port->state = 2;

                    // set the physical port to the hold power setting or the
                    // client brightness setting, whichever is lower
                    uint8_t hold = port->holdPower();
                    uint8_t val = port->val;                    
                    port->out->set(val < hold ? val : hold);
                    
                    // we're done with the timer
                    remove = true;
                }
            }
            else
            {
                // the port was turned off before the timer expired - remove
                // it from the timer list
                remove = true;
            }
            
            // if desired, remove the port from the timer list
            if (remove)
            {
                // Remove the list entry by overwriting the slot with
                // the last entry in the list.
                pending[i] = pending[--nPending];
                
                // Note that we don't increment the loop counter, since
                // we now need to revisit this same slot.
            }
            else
            {
                // we're keeping this item; move on to the next one
                ++i;
            }
        }
    }

protected:
    // underlying physical output
    LwOut *out;
    
    // Timestamp on 'timer' of start of full-power interval.  We set this
    // to the current 'timer' timestamp when entering state 1.
    uint32_t t0;

    // Nominal output level (brightness) last set by the client.  During
    // the initial full-power interval, we replicate the requested level
    // exactly on the physical output.  During the hold interval, we limit
    // the physical output to the hold power, but use the caller's value
    // if it's lower.
    uint8_t val;
    
    // Current port state:
    //
    //  0 = off
    //  1 = on at initial full power
    //  2 = on at hold power
    uint8_t state;
    
    // Configuration parameters.  The high 4 bits encode the initial full-
    // power time in 50ms units, starting at 0=50ms.  The low 4 bits encode
    // the hold power (applied after the initial time expires if the output
    // is still on) in units of 6.66%.  The resulting percentage is used
    // for the PWM duty cycle of the physical output.
    uint8_t params;
    
    // Figure the initial full-power time in microseconds: 50ms * (1+N),
    // where N is the high 4 bits of the parameter byte.
    inline uint32_t fullPowerTime_us() const { return 50000*(1 + ((params >> 4) & 0x0F)); }
    
    // Figure the hold power PWM level (0-255) 
    inline uint8_t holdPower() const { return (params & 0x0F) * 17; }
    
    // Timer.  This is a shared timer for all of the FL ports.  When we
    // transition from OFF to ON, we note the current time on this timer
    // (which runs continuously).  
    static Timer timer;
    
    // Flipper logic pending timer list.  Whenever a flipper logic output
    // transitions from OFF to ON, we add it to this list.  We scan the
    // list in our polling routine to find ports that have reached the
    // expiration of their initial full-power intervals.
    static LwFlipperLogicOut **pending;
    static uint8_t nPending;
};

// Flipper Logic statics
Timer LwFlipperLogicOut::timer;
LwFlipperLogicOut **LwFlipperLogicOut::pending;
uint8_t LwFlipperLogicOut::nPending;

// Chime Logic.  This is a filter output that we layer on a physical
// output to set a minimum and maximum ON time for the output. 
class LwChimeLogicOut: public LwOut
{
public:
    // Set up the output.  'params' encodes the minimum and maximum time.
    LwChimeLogicOut(LwOut *o, uint8_t params)
        : out(o), params(params)
    {
        // initially OFF
        state = 0;
    }
    
    virtual void set(uint8_t level)
    {
        // update the physical output according to our current timing state
        switch (state)
        {
        case 0:
            // We're currently off.  If the new level is non-zero, switch
            // to state 1 (initial minimum interval) and set the requested
            // level.  If the new level is zero, we're switching from off to
            // off, so there's no change.
            if (level != 0)
            {
                // switch to state 1 (initial minimum interval, port is
                // logically on)
                state = 1;
                
                // set the requested output level
                out->set(level);

                // add myself to the pending timer list
                pending[nPending++] = this;
                
                // note the starting time
                t0 = timer.read_us();
            }
            break;
            
        case 1:   // min ON interval, port on
        case 2:   // min ON interval, port off
            // We're in the initial minimum ON interval.  If the new power
            // level is non-zero, pass it through to the physical port, since
            // the client is allowed to change the power level during the
            // initial ON interval - they just can't turn it off entirely.
            // Set the state to 1 to indicate that the logical port is on.
            //
            // If the new level is zero, leave the underlying port at its 
            // current power level, since we're not allowed to turn it off
            // during this period.  Set the state to 2 to indicate that the
            // logical port is off even though the physical port has to stay
            // on for the remainder of the interval.
            if (level != 0)
            {
                // client is leaving the port on - pass through the new 
                // power level and set state 1 (logically on)
                out->set(level);
                state = 1;
            }
            else
            {
                // Client is turning off the port - leave the underlying port 
                // on at its current level and set state 2 (logically off).
                // When the minimum ON time expires, the polling routine will
                // see that we're logically off and will pass that through to
                // the underlying physical port.  Until then, though, we have
                // to leave the physical port on to satisfy the minimum ON
                // time requirement.
                state = 2;
            }
            break;
            
        case 3: 
            // We're after the minimum ON interval and before the maximum
            // ON time limit.  We can set any new level, including fully off.  
            // Pass the new power level through to the port.
            out->set(level);
            
            // if the port is now off, return to state 0 (OFF)
            if (level == 0)
            {
                // return to the OFF state
                state = 0;
                
                // If we have a timer pending, remove it.  A timer will be
                // pending if we have a non-infinite maximum on time for the
                // port.
                for (int i = 0 ; i < nPending ; ++i)
                {
                    // is this us?
                    if (pending[i] == this)
                    {
                        // remove myself by replacing the slot with the
                        // last list entry
                        pending[i] = pending[--nPending];
                        
                        // no need to look any further
                        break;
                    }
                }
            }
            break;
            
        case 4:
            // We're after the maximum ON time.  The physical port stays off
            // during this interval, so we don't pass any changes through to
            // the physical port.  When the client sets the level to 0, we
            // turn off the logical port and reset to state 0.
            if (level == 0)
                state = 0;
            break;
        }
    }
    
    // Class initialization
    static void classInit(Config &cfg)
    {
        // Count the Minimum On Time outputs in the configuration.  We
        // need to allocate enough pending timer list space to accommodate
        // all of these outputs.
        int n = 0;
        for (int i = 0 ; i < MAX_OUT_PORTS ; ++i)
        {
            // if this port is active and marked as Flipper Logic, count it
            if (cfg.outPort[i].typ != PortTypeDisabled
                && (cfg.outPort[i].flags & PortFlagChimeLogic) != 0)
                ++n;
        }
        
        // allocate space for the pending timer list
        pending = new LwChimeLogicOut*[n];
        
        // there's nothing in the pending list yet
        nPending = 0;
        
        // Start our shared timer.  The epoch is arbitrary, since we only
        // use it to figure elapsed times.
        timer.start();
    }

    // Check for ports with pending timers.  The main routine should
    // call this on each iteration to process our state transitions.
    static void poll()
    {
        // note the current time
        uint32_t t = timer.read_us();
        
        // go through the timer list
        for (int i = 0 ; i < nPending ; )
        {
            // get the port
            LwChimeLogicOut *port = pending[i];
            
            // assume we'll keep it
            bool remove = false;
            
            // check our state
            switch (port->state)
            {
            case 1:  // initial minimum ON time, port logically on
            case 2:  // initial minimum ON time, port logically off
                // check if the minimum ON time has elapsed
                if (uint32_t(t - port->t0) > port->minOnTime_us())
                {
                    // This port has completed its initial ON interval, so
                    // it advances to the next state. 
                    if (port->state == 1)
                    {
                        // The port is logically on, so advance to state 3.
                        // The underlying port is already at its proper level, 
                        // since we pass through non-zero power settings to the 
                        // underlying port throughout the initial minimum time.
                        // The timer stays active into state 3.
                        port->state = 3;
                        
                        // Special case: maximum on time 0 means "infinite".
                        // There's no need for a timer in this case; we'll
                        // just stay in state 3 until the client turns the
                        // port off.
                        if (port->maxOnTime_us() == 0)
                            remove = true;
                    }
                    else
                    {
                        // The port was switched off by the client during the
                        // minimum ON period.  We haven't passed the OFF state
                        // to the underlying port yet, because the port has to
                        // stay on throughout the minimum ON period.  So turn
                        // the port off now.
                        port->out->set(0);
                        
                        // return to state 0 (OFF)
                        port->state = 0;

                        // we're done with the timer
                        remove = true;
                    }
                }
                break;
                
            case 3:  // between minimum ON time and maximum ON time
                // check if the maximum ON time has expired
                if (uint32_t(t - port->t0) > port->maxOnTime_us())
                {
                    // The maximum ON time has expired.  Turn off the physical
                    // port.
                    port->out->set(0);
                    
                    // Switch to state 4 (logically ON past maximum time)
                    port->state = 4;
                    
                    // Remove the timer on this port.  This port simply stays
                    // in state 4 until the client turns off the port.
                    remove = true;
                }
                break;                
            }
            
            // if desired, remove the port from the timer list
            if (remove)
            {
                // Remove the list entry by overwriting the slot with
                // the last entry in the list.
                pending[i] = pending[--nPending];
                
                // Note that we don't increment the loop counter, since
                // we now need to revisit this same slot.
            }
            else
            {
                // we're keeping this item; move on to the next one
                ++i;
            }
        }
    }

protected:
    // underlying physical output
    LwOut *out;
    
    // Timestamp on 'timer' of start of full-power interval.  We set this
    // to the current 'timer' timestamp when entering state 1.
    uint32_t t0;

    // Current port state:
    //
    //  0 = off
    //  1 = in initial minimum ON interval, logical port is on
    //  2 = in initial minimum ON interval, logical port is off
    //  3 = in interval between minimum and maximum ON times
    //  4 = after the maximum ON interval
    //
    // The "logical" on/off state of the port is the state set by the 
    // client.  The "physical" state is the state of the underlying port.
    // The relationships between logical and physical port state, and the 
    // effects of updates by the client, are as follows:
    //
    //    State | Logical | Physical | Client set on | Client set off
    //    -----------------------------------------------------------
    //      0   |   Off   |   Off    | phys on, -> 1 |   no effect
    //      1   |   On    |   On     |   no effect   |     -> 2
    //      2   |   Off   |   On     |     -> 1      |   no effect
    //      3   |   On    |   On     |   no effect   | phys off, -> 0
    //      4   |   On    |   On     |   no effect   | phys off, -> 0
    //      
    // The polling routine makes the following transitions when the current
    // time limit expires:
    //
    //   1: at end of minimum ON, -> 3 (or 4 if max == infinity)
    //   2: at end of minimum ON, port off, -> 0
    //   3: at end of maximum ON, port off, -> 4
    //
    uint8_t state;
    
    // Configuration parameters byte.  This encodes the minimum and maximum
    // ON times.
    uint8_t params;
    
    // Timer.  This is a shared timer for all of the minimum ON time ports.
    // When we transition from OFF to ON, we note the current time on this 
    // timer to establish the start of our minimum ON period.
    static Timer timer;

    // translaton table from timing parameter in config to minimum ON time
    static const uint32_t paramToTime_us[];
    
    // Figure the minimum ON time.  The minimum ON time is given by the
    // low-order 4 bits of the parameters byte, which serves as an index
    // into our time table.
    inline uint32_t minOnTime_us() const { return paramToTime_us[params & 0x0F]; }
    
    // Figure the maximum ON time.  The maximum time is the high 4 bits
    // of the parameters byte.  This is an index into our time table, but
    // 0 has the special meaning "infinite".
    inline uint32_t maxOnTime_us() const { return paramToTime_us[((params >> 4) & 0x0F)]; }

    // Pending timer list.  Whenever one of our ports transitions from OFF
    // to ON, we add it to this list.  We scan this list in our polling
    // routine to find ports that have reached the ends of their initial
    // ON intervals.
    static LwChimeLogicOut **pending;
    static uint8_t nPending;
};

// Min Time Out statics
Timer LwChimeLogicOut::timer;
LwChimeLogicOut **LwChimeLogicOut::pending;
uint8_t LwChimeLogicOut::nPending;
const uint32_t LwChimeLogicOut::paramToTime_us[] = {
    0,          // for the max time, this means "infinite"
    1000, 
    2000,
    5000, 
    10000, 
    20000, 
    40000, 
    80000, 
    100000, 
    200000, 
    300000, 
    400000, 
    500000, 
    600000, 
    700000, 
    800000
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
        tlc5940 = new TLC5940(
            wirePinName(cfg.tlc5940.sclk), 
            wirePinName(cfg.tlc5940.sin),
            wirePinName(cfg.tlc5940.gsclk),
            wirePinName(cfg.tlc5940.blank), 
            wirePinName(cfg.tlc5940.xlat), 
            cfg.tlc5940.nchips);
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
    Lw5940Out(uint8_t idx) : idx(idx) { prv = 0; }
    virtual void set(uint8_t val)
    {
        if (val != prv)
           tlc5940->set(idx, dof_to_tlc[prv = val]);
    }
    uint8_t idx;
    uint8_t prv;
};

// LwOut class for TLC5940 gamma-corrected outputs.
class Lw5940GammaOut: public LwOut
{
public:
    Lw5940GammaOut(uint8_t idx) : idx(idx) { prv = 0; }
    virtual void set(uint8_t val)
    {
        if (val != prv)
           tlc5940->set(idx, dof_to_gamma_tlc[prv = val]);
    }
    uint8_t idx;
    uint8_t prv;
};

//
// TLC59116 interface object
//
TLC59116 *tlc59116 = 0;
void init_tlc59116(Config &cfg)
{
    // Create the interface if any chips are enabled
    if (cfg.tlc59116.chipMask != 0)
    {
        // set up the interface
        tlc59116 = new TLC59116(
            wirePinName(cfg.tlc59116.sda),
            wirePinName(cfg.tlc59116.scl),
            wirePinName(cfg.tlc59116.reset));
            
        // initialize the chips
        tlc59116->init();
    }
}

// LwOut class for TLC59116 outputs.  The 'addr' value in the constructor
// is low 4 bits of the chip's I2C address; this is the part of the address
// that's configurable per chip.  'port' is the output number on the chip
// (0-15).
//
// Note that we don't need a separate gamma-corrected subclass for this
// output type, since there's no loss of precision with the standard layered
// gamma (it emits 8-bit values, and we take 8-bit inputs).
class Lw59116Out: public LwOut
{
public:
    Lw59116Out(uint8_t addr, uint8_t port) : addr(addr), port(port) { prv = 0; }
    virtual void set(uint8_t val)
    {
        if (val != prv)
            tlc59116->set(addr, port, prv = val);
    }
    
protected:
    uint8_t addr;
    uint8_t port;
    uint8_t prv;
};


//
// 74HC595 interface object.  Set this up with the port assignments in
// config.h.
//
HC595 *hc595 = 0;

// initialize the 74HC595 interface
void init_hc595(Config &cfg)
{
    if (cfg.hc595.nchips != 0)
    {
        hc595 = new HC595(
            wirePinName(cfg.hc595.nchips), 
            wirePinName(cfg.hc595.sin), 
            wirePinName(cfg.hc595.sclk), 
            wirePinName(cfg.hc595.latch), 
            wirePinName(cfg.hc595.ena));
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
    Lw595Out(uint8_t idx) : idx(idx) { prv = 0; }
    virtual void set(uint8_t val)
    {
        if (val != prv)
           hc595->set(idx, (prv = val) == 0 ? 0 : 1);
    }
    uint8_t idx;
    uint8_t prv;
};



// Conversion table - 8-bit DOF output level to PWM duty cycle,
// normalized to 0.0 to 1.0 scale.
static const float dof_to_pwm[] = {
    0.000000f, 0.003922f, 0.007843f, 0.011765f, 0.015686f, 0.019608f, 0.023529f, 0.027451f, 
    0.031373f, 0.035294f, 0.039216f, 0.043137f, 0.047059f, 0.050980f, 0.054902f, 0.058824f, 
    0.062745f, 0.066667f, 0.070588f, 0.074510f, 0.078431f, 0.082353f, 0.086275f, 0.090196f, 
    0.094118f, 0.098039f, 0.101961f, 0.105882f, 0.109804f, 0.113725f, 0.117647f, 0.121569f, 
    0.125490f, 0.129412f, 0.133333f, 0.137255f, 0.141176f, 0.145098f, 0.149020f, 0.152941f, 
    0.156863f, 0.160784f, 0.164706f, 0.168627f, 0.172549f, 0.176471f, 0.180392f, 0.184314f, 
    0.188235f, 0.192157f, 0.196078f, 0.200000f, 0.203922f, 0.207843f, 0.211765f, 0.215686f, 
    0.219608f, 0.223529f, 0.227451f, 0.231373f, 0.235294f, 0.239216f, 0.243137f, 0.247059f, 
    0.250980f, 0.254902f, 0.258824f, 0.262745f, 0.266667f, 0.270588f, 0.274510f, 0.278431f, 
    0.282353f, 0.286275f, 0.290196f, 0.294118f, 0.298039f, 0.301961f, 0.305882f, 0.309804f, 
    0.313725f, 0.317647f, 0.321569f, 0.325490f, 0.329412f, 0.333333f, 0.337255f, 0.341176f, 
    0.345098f, 0.349020f, 0.352941f, 0.356863f, 0.360784f, 0.364706f, 0.368627f, 0.372549f, 
    0.376471f, 0.380392f, 0.384314f, 0.388235f, 0.392157f, 0.396078f, 0.400000f, 0.403922f, 
    0.407843f, 0.411765f, 0.415686f, 0.419608f, 0.423529f, 0.427451f, 0.431373f, 0.435294f, 
    0.439216f, 0.443137f, 0.447059f, 0.450980f, 0.454902f, 0.458824f, 0.462745f, 0.466667f, 
    0.470588f, 0.474510f, 0.478431f, 0.482353f, 0.486275f, 0.490196f, 0.494118f, 0.498039f, 
    0.501961f, 0.505882f, 0.509804f, 0.513725f, 0.517647f, 0.521569f, 0.525490f, 0.529412f, 
    0.533333f, 0.537255f, 0.541176f, 0.545098f, 0.549020f, 0.552941f, 0.556863f, 0.560784f, 
    0.564706f, 0.568627f, 0.572549f, 0.576471f, 0.580392f, 0.584314f, 0.588235f, 0.592157f, 
    0.596078f, 0.600000f, 0.603922f, 0.607843f, 0.611765f, 0.615686f, 0.619608f, 0.623529f, 
    0.627451f, 0.631373f, 0.635294f, 0.639216f, 0.643137f, 0.647059f, 0.650980f, 0.654902f, 
    0.658824f, 0.662745f, 0.666667f, 0.670588f, 0.674510f, 0.678431f, 0.682353f, 0.686275f, 
    0.690196f, 0.694118f, 0.698039f, 0.701961f, 0.705882f, 0.709804f, 0.713725f, 0.717647f, 
    0.721569f, 0.725490f, 0.729412f, 0.733333f, 0.737255f, 0.741176f, 0.745098f, 0.749020f, 
    0.752941f, 0.756863f, 0.760784f, 0.764706f, 0.768627f, 0.772549f, 0.776471f, 0.780392f, 
    0.784314f, 0.788235f, 0.792157f, 0.796078f, 0.800000f, 0.803922f, 0.807843f, 0.811765f, 
    0.815686f, 0.819608f, 0.823529f, 0.827451f, 0.831373f, 0.835294f, 0.839216f, 0.843137f, 
    0.847059f, 0.850980f, 0.854902f, 0.858824f, 0.862745f, 0.866667f, 0.870588f, 0.874510f, 
    0.878431f, 0.882353f, 0.886275f, 0.890196f, 0.894118f, 0.898039f, 0.901961f, 0.905882f, 
    0.909804f, 0.913725f, 0.917647f, 0.921569f, 0.925490f, 0.929412f, 0.933333f, 0.937255f, 
    0.941176f, 0.945098f, 0.949020f, 0.952941f, 0.956863f, 0.960784f, 0.964706f, 0.968627f, 
    0.972549f, 0.976471f, 0.980392f, 0.984314f, 0.988235f, 0.992157f, 0.996078f, 1.000000f
};


// Conversion table for 8-bit DOF level to pulse width, with gamma correction
// pre-calculated.  The values are normalized duty cycles from 0.0 to 1.0.
// Note that we could use the layered gamma output on top of the regular 
// LwPwmOut class for this instead of a separate table, but we get much better 
// precision with a dedicated table, because we apply gamma correction to the
// actual duty cycle values (as 'float') rather than the 8-bit DOF values.
static const float dof_to_gamma_pwm[] = {
    0.000000f, 0.000000f, 0.000001f, 0.000004f, 0.000009f, 0.000017f, 0.000028f, 0.000042f,
    0.000062f, 0.000086f, 0.000115f, 0.000151f, 0.000192f, 0.000240f, 0.000296f, 0.000359f,
    0.000430f, 0.000509f, 0.000598f, 0.000695f, 0.000803f, 0.000920f, 0.001048f, 0.001187f,
    0.001337f, 0.001499f, 0.001673f, 0.001860f, 0.002059f, 0.002272f, 0.002498f, 0.002738f,
    0.002993f, 0.003262f, 0.003547f, 0.003847f, 0.004162f, 0.004494f, 0.004843f, 0.005208f,
    0.005591f, 0.005991f, 0.006409f, 0.006845f, 0.007301f, 0.007775f, 0.008268f, 0.008781f,
    0.009315f, 0.009868f, 0.010442f, 0.011038f, 0.011655f, 0.012293f, 0.012954f, 0.013637f,
    0.014342f, 0.015071f, 0.015823f, 0.016599f, 0.017398f, 0.018223f, 0.019071f, 0.019945f,
    0.020844f, 0.021769f, 0.022720f, 0.023697f, 0.024701f, 0.025731f, 0.026789f, 0.027875f,
    0.028988f, 0.030129f, 0.031299f, 0.032498f, 0.033726f, 0.034983f, 0.036270f, 0.037587f,
    0.038935f, 0.040313f, 0.041722f, 0.043162f, 0.044634f, 0.046138f, 0.047674f, 0.049243f,
    0.050844f, 0.052478f, 0.054146f, 0.055847f, 0.057583f, 0.059353f, 0.061157f, 0.062996f,
    0.064870f, 0.066780f, 0.068726f, 0.070708f, 0.072726f, 0.074780f, 0.076872f, 0.079001f,
    0.081167f, 0.083371f, 0.085614f, 0.087895f, 0.090214f, 0.092572f, 0.094970f, 0.097407f,
    0.099884f, 0.102402f, 0.104959f, 0.107558f, 0.110197f, 0.112878f, 0.115600f, 0.118364f,
    0.121170f, 0.124019f, 0.126910f, 0.129844f, 0.132821f, 0.135842f, 0.138907f, 0.142016f,
    0.145170f, 0.148367f, 0.151610f, 0.154898f, 0.158232f, 0.161611f, 0.165037f, 0.168509f,
    0.172027f, 0.175592f, 0.179205f, 0.182864f, 0.186572f, 0.190327f, 0.194131f, 0.197983f,
    0.201884f, 0.205834f, 0.209834f, 0.213883f, 0.217982f, 0.222131f, 0.226330f, 0.230581f,
    0.234882f, 0.239234f, 0.243638f, 0.248094f, 0.252602f, 0.257162f, 0.261774f, 0.266440f,
    0.271159f, 0.275931f, 0.280756f, 0.285636f, 0.290570f, 0.295558f, 0.300601f, 0.305699f,
    0.310852f, 0.316061f, 0.321325f, 0.326645f, 0.332022f, 0.337456f, 0.342946f, 0.348493f,
    0.354098f, 0.359760f, 0.365480f, 0.371258f, 0.377095f, 0.382990f, 0.388944f, 0.394958f,
    0.401030f, 0.407163f, 0.413356f, 0.419608f, 0.425921f, 0.432295f, 0.438730f, 0.445226f,
    0.451784f, 0.458404f, 0.465085f, 0.471829f, 0.478635f, 0.485504f, 0.492436f, 0.499432f,
    0.506491f, 0.513614f, 0.520800f, 0.528052f, 0.535367f, 0.542748f, 0.550194f, 0.557705f,
    0.565282f, 0.572924f, 0.580633f, 0.588408f, 0.596249f, 0.604158f, 0.612133f, 0.620176f,
    0.628287f, 0.636465f, 0.644712f, 0.653027f, 0.661410f, 0.669863f, 0.678384f, 0.686975f,
    0.695636f, 0.704366f, 0.713167f, 0.722038f, 0.730979f, 0.739992f, 0.749075f, 0.758230f,
    0.767457f, 0.776755f, 0.786126f, 0.795568f, 0.805084f, 0.814672f, 0.824334f, 0.834068f,
    0.843877f, 0.853759f, 0.863715f, 0.873746f, 0.883851f, 0.894031f, 0.904286f, 0.914616f,
    0.925022f, 0.935504f, 0.946062f, 0.956696f, 0.967407f, 0.978194f, 0.989058f, 1.000000f
};

// Polled-update PWM output list
//
// This is a workaround for a KL25Z hardware bug/limitation.  The bug (more
// about this below) is that we can't write to a PWM output "value" register
// more than once per PWM cycle; if we do, outputs after the first are lost.
// The value register controls the duty cycle, so it's what you have to write
// if you want to update the brightness of an output.
//
// The symptom of the problem, if it's not worked around somehow, is that 
// an output will get "stuck" due to a missed write.  This is especially
// noticeable during a series of updates such as a fade.  If the last
// couple of updates in a fade are lost, the output will get stuck at some
// value above or below the desired final value.  The stuck setting will
// persist until the output is deliberately changed again later.
//
// Our solution:  Simply repeat all PWM updates periodically.  This way, any
// lost write will *eventually* take hold on one of the repeats.  Repeats of
// the same value won't change anything and thus won't be noticeable.  We do
// these periodic updates during the main loop, which makes them very low 
// overhead (there's no interrupt overhead; we just do them when convenient 
// in the main loop), and also makes them very frequent.  The frequency 
// is crucial because it ensures that updates will never be lost for long 
// enough to become noticeable.
//
// The mbed library has its own, different solution to this bug, but the
// mbed solution isn't really a solution at all because it creates a separate 
// problem of its own.  The mbed approach is to reset the TPM "count" register
// on every value register write.   The count reset truncates the current
// PWM cycle, which bypasses the hardware problem.  Remember, the hardware
// problem is that you can only write once per cycle; the mbed "solution" gets
// around that by making sure the cycle ends immediately after the write.
// The problem with this approach is that the truncated cycle causes visible 
// flicker if the output is connected to an LED.  This is particularly 
// noticeable during fades, when we're updating the value register repeatedly 
// and rapidly: an attempt to fade from fully on to fully off causes rapid 
// fluttering and flashing rather than a smooth brightness fade.  That's why
// I had to come up with something different - the mbed solution just trades
// one annoying bug for another that's just as bad.
//
// The hardware bug, by the way, is a case of good intentions gone bad.  
// The whole point of the staging register is to make things easier for
// us software writers.  In most PWM hardware, software has to coordinate
// with the PWM duty cycle when updating registers to avoid a glitch that
// you'd get by scribbling to the duty cycle register mid-cycle.  The
// staging register solves this by letting the software write an update at
// any time, knowing that the hardware will apply the update at exactly the
// end of the cycle, ensuring glitch-free updates.  It's a great design,
// except that it doesn't quite work.  The problem is that they implemented
// this clever staging register as a one-element FIFO that refuses any more
// writes when full.  That is, writing a value to the FIFO fills it; once
// full, it ignores writes until it gets emptied out.  How's it emptied out?
// By the hardware moving the staged value to the real register.  Sadly, they
// didn't provide any way for the software to clear the register, and no way
// to even tell that it's full.  So we don't have glitches on write, but we're
// back to the original problem that the software has to be aware of the PWM
// cycle timing, because the only way for the software to know that a write
// actually worked is to know that it's been at least one PWM cycle since the
// last write.  That largely defeats the whole purpose of the staging register,
// since the whole point was to free software writers of these timing
// considerations.  It's still an improvement over no staging register at
// all, since we at least don't have to worry about glitches, but it leaves
// us with this somewhat similar hassle.
//
// So here we have our list of PWM outputs that need to be polled for updates.
// The KL25Z hardware only has 10 PWM channels, so we only need a fixed set
// of polled items.
static int numPolledPwm;
static class LwPwmOut *polledPwm[10];

// LwOut class for a PWM-capable GPIO port.
class LwPwmOut: public LwOut
{
public:
    LwPwmOut(PinName pin, uint8_t initVal) : p(pin)
    {
        // add myself to the list of polled outputs for periodic updates
        if (numPolledPwm < countof(polledPwm))
            polledPwm[numPolledPwm++] = this;
            
        // IMPORTANT:  Do not set the PWM period (frequency) here explicitly.  
        // We instead want to accept the current setting for the TPM unit
        // we're assigned to.  The KL25Z hardware can only set the period at
        // the TPM unit level, not per channel, so if we changed the frequency
        // here, we'd change it for everything attached to our TPM unit.  LW
        // outputs don't care about frequency other than that it's fast enough
        // that attached LEDs won't flicker.  Some other PWM users (IR remote,
        // TLC5940) DO care about exact frequencies, because they use the PWM
        // as a signal generator rather than merely for brightness control.
        // If we changed the frequency here, we could clobber one of those
        // carefully chosen frequencies and break the other subsystem.  So
        // we need to be the "free variable" here and accept whatever setting
        // is currently on our assigned unit.  To minimize flicker, the main()
        // entrypoint sets a default PWM rate of 1kHz on all channels.  All
        // of the other subsystems that might set specific frequencies will
        // set much high frequencies, so that should only be good for us.
            
        // set the initial brightness value
        set(initVal);
    }

    virtual void set(uint8_t val) 
    {
        // save the new value
        this->val = val;
        
        // commit it to the hardware
        commit();
    }

    // handle periodic update polling
    void poll()
    {
        commit();
    }

protected:
    virtual void commit()
    {
        // write the current value to the PWM controller if it's changed
        p.glitchFreeWrite(dof_to_pwm[val]);
    }
    
    NewPwmOut p;
    uint8_t val;
};

// Gamma corrected PWM GPIO output.  This works exactly like the regular
// PWM output, but translates DOF values through the gamma-corrected
// table instead of the regular linear table.
class LwPwmGammaOut: public LwPwmOut
{
public:
    LwPwmGammaOut(PinName pin, uint8_t initVal)
        : LwPwmOut(pin, initVal)
    {
    }
    
protected:
    virtual void commit()
    {
        // write the current value to the PWM controller if it's changed
        p.glitchFreeWrite(dof_to_gamma_pwm[val]);
    }
};

// poll the PWM outputs
Timer polledPwmTimer;
uint64_t polledPwmTotalTime, polledPwmRunCount;
void pollPwmUpdates()
{
    // If it's been long enough since the last update, do another update.
    // Note that the time limit is fairly arbitrary: it has to be at least
    // 1.5X the PWM period, so that we can be sure that at least one PWM
    // period has elapsed since the last update, but there's no hard upper
    // bound.  Instead, it only has to be short enough that fades don't
    // become noticeably chunky.  The competing interest is that we don't 
    // want to do this more often than necessary to provide incremental
    // benefit, because the polling adds overhead to the main loop and
    // takes time away from other tasks we could be performing.  The
    // shortest time with practical benefit is probably around 50-60Hz,
    // since that gives us "video rate" granularity in fades.  Anything
    // faster wouldn't probably make fades look any smoother to a human 
    // viewer.
    if (polledPwmTimer.read_us() >= 15000)
    {
        // time the run for statistics collection
        IF_DIAG(
          Timer t; 
          t.start();
        )
        
        // poll each output
        for (int i = numPolledPwm ; i > 0 ; )
            polledPwm[--i]->poll();
        
        // reset the timer for the next cycle
        polledPwmTimer.reset();
        
        // collect statistics
        IF_DIAG(
          polledPwmTotalTime += t.read_us();
          polledPwmRunCount += 1;
        )
    }
}

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

// create a single output pin
LwOut *createLwPin(int portno, LedWizPortCfg &pc, Config &cfg)
{
    // get this item's values
    int typ = pc.typ;
    int pin = pc.pin;
    int flags = pc.flags;
    int noisy = flags & PortFlagNoisemaker;
    int activeLow = flags & PortFlagActiveLow;
    int gamma = flags & PortFlagGamma;
    int flipperLogic = flags & PortFlagFlipperLogic;
    int chimeLogic = flags & PortFlagChimeLogic;
    
    // cancel gamma on flipper logic ports
    if (flipperLogic)
        gamma = false;

    // create the pin interface object according to the port type        
    LwOut *lwp;
    switch (typ)
    {
    case PortTypeGPIOPWM:
        // PWM GPIO port - assign if we have a valid pin
        if (pin != 0)
        {
            // If gamma correction is to be used, and we're not inverting the output,
            // use the combined Pwmout + Gamma output class; otherwise use the plain
            // PwmOut class.  We can't use the combined class for inverted outputs
            // because we have to apply gamma correction before the inversion.
            if (gamma && !activeLow)
            {
                // use the gamma-corrected PwmOut type
                lwp = new LwPwmGammaOut(wirePinName(pin), 0);
                
                // don't apply further gamma correction to this output
                gamma = false;
            }
            else
            {
                // no gamma correction - use the standard PwmOut class
                lwp = new LwPwmOut(wirePinName(pin), activeLow ? 255 : 0);
            }
        }
        else
            lwp = new LwVirtualOut();
        break;
    
    case PortTypeGPIODig:
        // Digital GPIO port
        if (pin != 0)
            lwp = new LwDigOut(wirePinName(pin), activeLow ? 255 : 0);
        else
            lwp = new LwVirtualOut();
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
        // 74HC595 port (if we don't have an HC595 controller object, or it's not 
        // a valid output number, create a virtual port)
        if (hc595 != 0 && pin < cfg.hc595.nchips*8)
            lwp = new Lw595Out(pin);
        else
            lwp = new LwVirtualOut();
        break;
        
    case PortTypeTLC59116:
        // TLC59116 port.  The pin number in the config encodes the chip address
        // in the high 4 bits and the output number on the chip in the low 4 bits.
        // There's no gamma-corrected version of this output handler, so we don't
        // need to worry about that here; just use the layered gamma as needed.
        if (tlc59116 != 0)
            lwp = new Lw59116Out((pin >> 4) & 0x0F, pin & 0x0F);
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
        
    // Layer on Flipper Logic if desired
    if (flipperLogic)
        lwp = new LwFlipperLogicOut(lwp, pc.flipperLogic);
        
    // Layer on Chime Logic if desired.  Note that Chime Logic and
    // Flipper Logic are mutually exclusive, and Flipper Logic takes
    // precedence, so ignore the Chime Logic bit if both are set.
    if (chimeLogic && !flipperLogic)
        lwp = new LwChimeLogicOut(lwp, pc.flipperLogic);
        
    // If it's a noisemaker, layer on a night mode switch
    if (noisy)
        lwp = new LwNoisyOut(lwp);
        
    // If it's gamma-corrected, layer on a gamma corrector
    if (gamma)
        lwp = new LwGammaOut(lwp);
        
    // If this is the ZB Launch Ball port, layer a monitor object.  Note
    // that the nominal port numbering in the config starts at 1, but we're
    // using an array index, so test against portno+1.
    if (portno + 1 == cfg.plunger.zbLaunchBall.port)
        lwp = new LwZbLaunchOut(lwp);
        
    // If this is the Night Mode indicator port, layer a night mode object.
    if (portno + 1 == cfg.nightMode.port)
        lwp = new LwNightModeIndicatorOut(lwp);

    // turn it off initially      
    lwp->set(0);
    
    // return the pin
    return lwp;
}

// initialize the output pin array
void initLwOut(Config &cfg)
{
    // Initialize the Flipper Logic and Chime Logic outputs
    LwFlipperLogicOut::classInit(cfg);
    LwChimeLogicOut::classInit(cfg);

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
    
    // allocate the pin array
    lwPin = new LwOut*[numOutputs];
    
    // Allocate the current brightness array
    outLevel = new uint8_t[numOutputs];
    
    // allocate the LedWiz output state arrays
    wizOn = new uint8_t[numOutputs];
    wizVal = new uint8_t[numOutputs];
    
    // initialize all LedWiz outputs to off and brightness 48
    memset(wizOn, 0, numOutputs);
    memset(wizVal, 48, numOutputs);
    
    // set all LedWiz virtual unit flash speeds to 2
    for (i = 0 ; i < countof(wizSpeed) ; ++i)
        wizSpeed[i] = 2;
    
    // create the pin interface object for each port
    for (i = 0 ; i < numOutputs ; ++i)
        lwPin[i] = createLwPin(i, cfg.outPort[i], cfg);
}

// Translate an LedWiz brightness level (0..49) to a DOF brightness
// level (0..255).  Note that brightness level 49 isn't actually valid,
// according to the LedWiz API documentation, but many clients use it
// anyway, and the real LedWiz accepts it and seems to treat it as 
// equivalent to 48.
static const uint8_t lw_to_dof[] = {
       0,    5,   11,   16,   21,   27,   32,   37, 
      43,   48,   53,   58,   64,   69,   74,   80, 
      85,   90,   96,  101,  106,  112,  117,  122, 
     128,  133,  138,  143,  149,  154,  159,  165, 
     170,  175,  181,  186,  191,  197,  202,  207, 
     213,  218,  223,  228,  234,  239,  244,  250, 
     255,  255
};

// Translate a DOF brightness level (0..255) to an LedWiz brightness
// level (1..48)
static const uint8_t dof_to_lw[] = {
     1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  3,  3,
     3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  6,  6,
     6,  6,  6,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,  8,  9,  9,
     9,  9,  9, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 12, 12,
    12, 12, 12, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 15, 15,
    15, 15, 15, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 18, 18, 18,
    18, 18, 18, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 21, 21, 21,
    21, 21, 21, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 24, 24, 24,
    24, 24, 24, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 27, 27, 27,
    27, 27, 27, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 30, 30, 30,
    30, 30, 30, 31, 31, 31, 31, 31, 32, 32, 32, 32, 32, 33, 33, 33,
    33, 33, 34, 34, 34, 34, 34, 34, 35, 35, 35, 35, 35, 36, 36, 36,
    36, 36, 37, 37, 37, 37, 37, 37, 38, 38, 38, 38, 38, 39, 39, 39,
    39, 39, 40, 40, 40, 40, 40, 40, 41, 41, 41, 41, 41, 42, 42, 42,
    42, 42, 43, 43, 43, 43, 43, 43, 44, 44, 44, 44, 44, 45, 45, 45,
    45, 45, 46, 46, 46, 46, 46, 46, 47, 47, 47, 47, 47, 48, 48, 48
};

// LedWiz flash cycle tables.  For efficiency, we use a lookup table
// rather than calculating these on the fly.  The flash cycles are
// generated by the following formulas, where 'c' is the current
// cycle counter, from 0 to 255:
//
//  mode 129 = sawtooth = (c < 128 ? c*2 + 1 : (255-c)*2)
//  mode 130 = flash on/off = (c < 128 ? 255 : 0)
//  mode 131 = on/ramp down = (c < 128 ? 255 : (255-c)*2)
//  mode 132 = ramp up/on = (c < 128 ? c*2 : 255)
//
// To look up the current output value for a given mode and a given
// cycle counter 'c', index the table with ((mode-129)*256)+c.
static const uint8_t wizFlashLookup[] = {
    // mode 129 = sawtooth = (c < 128 ? c*2 + 1 : (255-c)*2)
    0x01, 0x03, 0x05, 0x07, 0x09, 0x0b, 0x0d, 0x0f, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1b, 0x1d, 0x1f,
    0x21, 0x23, 0x25, 0x27, 0x29, 0x2b, 0x2d, 0x2f, 0x31, 0x33, 0x35, 0x37, 0x39, 0x3b, 0x3d, 0x3f,
    0x41, 0x43, 0x45, 0x47, 0x49, 0x4b, 0x4d, 0x4f, 0x51, 0x53, 0x55, 0x57, 0x59, 0x5b, 0x5d, 0x5f,
    0x61, 0x63, 0x65, 0x67, 0x69, 0x6b, 0x6d, 0x6f, 0x71, 0x73, 0x75, 0x77, 0x79, 0x7b, 0x7d, 0x7f,
    0x81, 0x83, 0x85, 0x87, 0x89, 0x8b, 0x8d, 0x8f, 0x91, 0x93, 0x95, 0x97, 0x99, 0x9b, 0x9d, 0x9f,
    0xa1, 0xa3, 0xa5, 0xa7, 0xa9, 0xab, 0xad, 0xaf, 0xb1, 0xb3, 0xb5, 0xb7, 0xb9, 0xbb, 0xbd, 0xbf,
    0xc1, 0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf, 0xd1, 0xd3, 0xd5, 0xd7, 0xd9, 0xdb, 0xdd, 0xdf,
    0xe1, 0xe3, 0xe5, 0xe7, 0xe9, 0xeb, 0xed, 0xef, 0xf1, 0xf3, 0xf5, 0xf7, 0xf9, 0xfb, 0xfd, 0xff,
    0xfe, 0xfc, 0xfa, 0xf8, 0xf6, 0xf4, 0xf2, 0xf0, 0xee, 0xec, 0xea, 0xe8, 0xe6, 0xe4, 0xe2, 0xe0,
    0xde, 0xdc, 0xda, 0xd8, 0xd6, 0xd4, 0xd2, 0xd0, 0xce, 0xcc, 0xca, 0xc8, 0xc6, 0xc4, 0xc2, 0xc0,
    0xbe, 0xbc, 0xba, 0xb8, 0xb6, 0xb4, 0xb2, 0xb0, 0xae, 0xac, 0xaa, 0xa8, 0xa6, 0xa4, 0xa2, 0xa0,
    0x9e, 0x9c, 0x9a, 0x98, 0x96, 0x94, 0x92, 0x90, 0x8e, 0x8c, 0x8a, 0x88, 0x86, 0x84, 0x82, 0x80,
    0x7e, 0x7c, 0x7a, 0x78, 0x76, 0x74, 0x72, 0x70, 0x6e, 0x6c, 0x6a, 0x68, 0x66, 0x64, 0x62, 0x60,
    0x5e, 0x5c, 0x5a, 0x58, 0x56, 0x54, 0x52, 0x50, 0x4e, 0x4c, 0x4a, 0x48, 0x46, 0x44, 0x42, 0x40,
    0x3e, 0x3c, 0x3a, 0x38, 0x36, 0x34, 0x32, 0x30, 0x2e, 0x2c, 0x2a, 0x28, 0x26, 0x24, 0x22, 0x20,
    0x1e, 0x1c, 0x1a, 0x18, 0x16, 0x14, 0x12, 0x10, 0x0e, 0x0c, 0x0a, 0x08, 0x06, 0x04, 0x02, 0x00,

    // mode 130 = flash on/off = (c < 128 ? 255 : 0)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // mode 131 = on/ramp down = c < 128 ? 255 : (255 - c)*2
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xfe, 0xfc, 0xfa, 0xf8, 0xf6, 0xf4, 0xf2, 0xf0, 0xee, 0xec, 0xea, 0xe8, 0xe6, 0xe4, 0xe2, 0xe0,
    0xde, 0xdc, 0xda, 0xd8, 0xd6, 0xd4, 0xd2, 0xd0, 0xce, 0xcc, 0xca, 0xc8, 0xc6, 0xc4, 0xc2, 0xc0,
    0xbe, 0xbc, 0xba, 0xb8, 0xb6, 0xb4, 0xb2, 0xb0, 0xae, 0xac, 0xaa, 0xa8, 0xa6, 0xa4, 0xa2, 0xa0,
    0x9e, 0x9c, 0x9a, 0x98, 0x96, 0x94, 0x92, 0x90, 0x8e, 0x8c, 0x8a, 0x88, 0x86, 0x84, 0x82, 0x80,
    0x7e, 0x7c, 0x7a, 0x78, 0x76, 0x74, 0x72, 0x70, 0x6e, 0x6c, 0x6a, 0x68, 0x66, 0x64, 0x62, 0x60,
    0x5e, 0x5c, 0x5a, 0x58, 0x56, 0x54, 0x52, 0x50, 0x4e, 0x4c, 0x4a, 0x48, 0x46, 0x44, 0x42, 0x40,
    0x3e, 0x3c, 0x3a, 0x38, 0x36, 0x34, 0x32, 0x30, 0x2e, 0x2c, 0x2a, 0x28, 0x26, 0x24, 0x22, 0x20,
    0x1e, 0x1c, 0x1a, 0x18, 0x16, 0x14, 0x12, 0x10, 0x0e, 0x0c, 0x0a, 0x08, 0x06, 0x04, 0x02, 0x00,

    // mode 132 = ramp up/on = c < 128 ? c*2 : 255
    0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e,
    0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3a, 0x3c, 0x3e,
    0x40, 0x42, 0x44, 0x46, 0x48, 0x4a, 0x4c, 0x4e, 0x50, 0x52, 0x54, 0x56, 0x58, 0x5a, 0x5c, 0x5e,
    0x60, 0x62, 0x64, 0x66, 0x68, 0x6a, 0x6c, 0x6e, 0x70, 0x72, 0x74, 0x76, 0x78, 0x7a, 0x7c, 0x7e,
    0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8c, 0x8e, 0x90, 0x92, 0x94, 0x96, 0x98, 0x9a, 0x9c, 0x9e,
    0xa0, 0xa2, 0xa4, 0xa6, 0xa8, 0xaa, 0xac, 0xae, 0xb0, 0xb2, 0xb4, 0xb6, 0xb8, 0xba, 0xbc, 0xbe,
    0xc0, 0xc2, 0xc4, 0xc6, 0xc8, 0xca, 0xcc, 0xce, 0xd0, 0xd2, 0xd4, 0xd6, 0xd8, 0xda, 0xdc, 0xde,
    0xe0, 0xe2, 0xe4, 0xe6, 0xe8, 0xea, 0xec, 0xee, 0xf0, 0xf2, 0xf4, 0xf6, 0xf8, 0xfa, 0xfc, 0xfe,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

// LedWiz flash cycle timer.  This runs continuously.  On each update,
// we use this to figure out where we are on the cycle for each bank.
Timer wizCycleTimer;

// timing statistics for wizPulse()
uint64_t wizPulseTotalTime, wizPulseRunCount;

// LedWiz flash timer pulse.  The main loop calls this on each cycle
// to update outputs using LedWiz flash modes.  We do one bank of 32
// outputs on each cycle.
static void wizPulse()
{
    // current bank
    static int wizPulseBank = 0;

    // start a timer for statistics collection
    IF_DIAG(
      Timer t;
      t.start();
    )

    // Update the current bank's cycle counter: figure the current
    // phase of the LedWiz pulse cycle for this bank.
    //
    // The LedWiz speed setting gives the flash period in 0.25s units
    // (speed 1 is a flash period of .25s, speed 7 is a period of 1.75s).
    //
    // What we're after here is the "phase", which is to say the point
    // in the current cycle.  If we assume that the cycle has been running
    // continuously since some arbitrary time zero in the past, we can
    // figure where we are in the current cycle by dividing the time since
    // that zero by the cycle period and taking the remainder.  E.g., if
    // the cycle time is 5 seconds, and the time since t-zero is 17 seconds,
    // we divide 17 by 5 to get a remainder of 2.  That says we're 2 seconds
    // into the current 5-second cycle, or 2/5 of the way through the
    // current cycle.
    //
    // We do this calculation on every iteration of the main loop, so we 
    // want it to be very fast.  To streamline it, we'll use some tricky
    // integer arithmetic.  The result will be the same as the straightforward
    // remainder and fraction calculation we just explained, but we'll get
    // there by less-than-obvious means.
    //
    // Rather than finding the phase as a continuous quantity or floating
    // point number, we'll quantize it.  We'll divide each cycle into 256 
    // time units, or quanta.  Each quantum is 1/256 of the cycle length,
    // so for a 1-second cycle (LedWiz speed 4), each quantum is 1/256 of 
    // a second, or about 3.9ms.  If we express the time since t-zero in
    // these units, the time period of one cycle is exactly 256 units, so
    // we can calculate our point in the cycle by taking the remainder of
    // the time (in our funny units) divided by 256.  The special thing
    // about making the cycle time equal to 256 units is that "x % 256" 
    // is exactly the same as "x & 255", which is a much faster operation
    // than division on ARM M0+: this CPU has no hardware DIVIDE operation,
    // so an integer division takes about 5us.  The bit mask operation, in 
    // contrast, takes only about 60ns - about 100x faster.  5us doesn't
    // sound like much, but we do this on every main loop, so every little
    // bit counts.  
    //
    // The snag is that our system timer gives us the elapsed time in
    // microseconds.  We still need to convert this to our special quanta
    // of 256 units per cycle.  The straightforward way to do that is by
    // dividing by (microseconds per quantum).  E.g., for LedWiz speed 4,
    // we decided that our quantum was 1/256 of a second, or 3906us, so
    // dividing the current system time in microseconds by 3906 will give
    // us the time in our quantum units.  But now we've just substituted
    // one division for another!
    //
    // This is where our really tricky integer math comes in.  Dividing
    // by X is the same as multiplying by 1/X.  In integer math, 1/3906
    // is zero, so that won't work.  But we can get around that by doing
    // the integer math as "fixed point" arithmetic instead.  It's still
    // actually carried out as integer operations, but we'll scale our
    // integers by a scaling factor, then take out the scaling factor
    // later to get the final result.  The scaling factor we'll use is
    // 2^24.  So we're going to calculate (time * 2^24/3906), then divide
    // the result by 2^24 to get the final answer.  I know it seems like 
    // we're substituting one division for another yet again, but this 
    // time's the charm, because dividing by 2^24 is a bit shift operation,
    // which is another single-cycle operation on M0+.  You might also
    // wonder how all these tricks don't cause overflows or underflows
    // or what not.  Well, the multiply by 2^24/3906 will cause an
    // overflow, but we don't care, because the overflow will all be in
    // the high-order bits that we're going to discard in the final 
    // remainder calculation anyway.
    //
    // Each entry in the array below represents 2^24/N for the corresponding
    // LedWiz speed, where N is the number of time quanta per cycle at that
    // speed.  The time quanta are chosen such that 256 quanta add up to 
    // approximately (LedWiz speed setting * 0.25s).
    // 
    // Note that the calculation has an implicit bit mask (result & 0xFF)
    // to get the final result mod 256.  But we don't have to actually
    // do that work because we're using 32-bit ints and a 2^24 fixed
    // point base (X in the narrative above).  The final shift right by
    // 24 bits to divide out the base will leave us with only 8 bits in
    // the result, since we started with 32.
    static const uint32_t inv_us_per_quantum[] = { // indexed by LedWiz speed
        0, 17172, 8590, 5726, 4295, 3436, 2863, 2454
    };
    int counter = ((wizCycleTimer.read_us() * inv_us_per_quantum[wizSpeed[wizPulseBank]]) >> 24);
        
    // get the range of 32 output sin this bank
    int fromPort = wizPulseBank*32;
    int toPort = fromPort+32;
    if (toPort > numOutputs)
        toPort = numOutputs;
        
    // update all outputs set to flashing values
    for (int i = fromPort ; i < toPort ; ++i)
    {
        // Update the port only if the LedWiz SBA switch for the port is on
        // (wizOn[i]) AND the port is a PBA flash mode in the range 129..132.
        // These modes and only these modes have the high bit (0x80) set, so
        // we can test for them simply by testing the high bit.
        if (wizOn[i])
        {
            uint8_t val = wizVal[i];
            if ((val & 0x80) != 0)
            {
                // ook up the value for the mode at the cycle time
                lwPin[i]->set(outLevel[i] = wizFlashLookup[((val-129) << 8) + counter]);
            }
        }
    }
        
    // flush changes to 74HC595 chips, if attached
    if (hc595 != 0)
        hc595->update();
        
    // switch to the next bank
    if (++wizPulseBank >= MAX_LW_BANKS)
        wizPulseBank = 0;

    // collect timing statistics
    IF_DIAG(
      wizPulseTotalTime += t.read_us();
      wizPulseRunCount += 1;
    )
}

// Update a port to reflect its new LedWiz SBA+PBA setting.
static void updateLwPort(int port)
{
    // check if the SBA switch is on or off
    if (wizOn[port])
    {
        // It's on.  If the port is a valid static brightness level,
        // set the output port to match.  Otherwise leave it as is:
        // if it's a flashing mode, the flash mode pulse will update
        // it on the next cycle.
        int val = wizVal[port];
        if (val <= 49)
            lwPin[port]->set(outLevel[port] = lw_to_dof[val]);
    }
    else
    {
        // the port is off - set absolute brightness zero
        lwPin[port]->set(outLevel[port] = 0);
    }
}

// Turn off all outputs and restore everything to the default LedWiz
// state.  This sets all outputs to LedWiz profile value 48 (full
// brightness) and switch state Off, and sets the LedWiz flash rate 
// to 2.  This effectively restores the power-on conditions.
//
void allOutputsOff()
{
    // reset all outputs to OFF/48
    for (int i = 0 ; i < numOutputs ; ++i)
    {
        outLevel[i] = 0;
        wizOn[i] = 0;
        wizVal[i] = 48;
        lwPin[i]->set(0);
    }
    
    // restore default LedWiz flash rate
    for (int i = 0 ; i < countof(wizSpeed) ; ++i)
        wizSpeed[i] = 2;
        
    // flush changes to hc595, if applicable
    if (hc595 != 0)
        hc595->update();
}

// Cary out an SBA or SBX message.  portGroup is 0 for ports 1-32,
// 1 for ports 33-64, etc.  Original protocol SBA messages always
// address port group 0; our private SBX extension messages can 
// address any port group.
void sba_sbx(int portGroup, const uint8_t *data)
{
    // update all on/off states in the group
    for (int i = 0, bit = 1, imsg = 1, port = portGroup*32 ; 
         i < 32 && port < numOutputs ; 
         ++i, bit <<= 1, ++port)
    {
        // figure the on/off state bit for this output
        if (bit == 0x100) {
            bit = 1;
            ++imsg;
        }
        
        // set the on/off state
        bool on = wizOn[port] = ((data[imsg] & bit) != 0);
        
        // set the output port brightness to match the new setting
        updateLwPort(port);
    }
    
    // set the flash speed for the port group
    if (portGroup < countof(wizSpeed))
        wizSpeed[portGroup] = (data[5] < 1 ? 1 : data[5] > 7 ? 7 : data[5]);

    // update 74HC959 outputs
    if (hc595 != 0)
        hc595->update();
}

// Carry out a PBA or PBX message.
void pba_pbx(int basePort, const uint8_t *data)
{
    // update each wizVal entry from the brightness data
    for (int i = 0, port = basePort ; i < 8 && port < numOutputs ; ++i, ++port)
    {
        // get the value
        uint8_t v = data[i];
        
        // Validate it.  The legal values are 0..49 for brightness
        // levels, and 128..132 for flash modes.  Set anything invalid
        // to full brightness (48) instead.  Note that 49 isn't actually
        // a valid documented value, but in practice some clients send
        // this to mean 100% brightness, and the real LedWiz treats it
        // as such.
        if ((v > 49 && v < 129) || v > 132)
            v = 48;
        
        // store it
        wizVal[port] = v;
        
        // update the port
        updateLwPort(port);
    }

    // update 74HC595 outputs
    if (hc595 != 0)
        hc595->update();
}

// ---------------------------------------------------------------------------
//
// IR Remote Control transmitter & receiver
//

// receiver
IRReceiver *ir_rx;

// transmitter
IRTransmitter *ir_tx;

// Mapping from IR commands slots in the configuration to "virtual button"
// numbers on the IRTransmitter's "virtual remote".  To minimize RAM usage, 
// we only create virtual buttons on the transmitter object for code slots 
// that are configured for transmission, which includes slots used for TV
// ON commands and slots that can be triggered by button presses.  This
// means that virtual button numbers won't necessarily match the config
// slot numbers.  This table provides the mapping:
// IRConfigSlotToVirtualButton[n] = ir_tx virtual button number for 
// configuration slot n
uint8_t IRConfigSlotToVirtualButton[MAX_IR_CODES];

// IR transmitter virtual button number for ad hoc IR command.  We allocate 
// one virtual button for sending ad hoc IR codes, such as through the USB
// protocol.
uint8_t IRAdHocBtn;

// Staging area for ad hoc IR commands.  It takes multiple messages
// to fill out an IR command, so we store the partial command here
// while waiting for the rest.
static struct
{
    uint8_t protocol;       // protocol ID
    uint64_t code;          // code
    uint8_t dittos : 1;     // using dittos?
    uint8_t ready : 1;      // do we have a code ready to transmit?    
} IRAdHocCmd;


// IR mode timer.  In normal mode, this is the time since the last
// command received; we use this to handle commands with timed effects,
// such as sending a key to the PC.  In learning mode, this is the time
// since we activated learning mode, which we use to automatically end
// learning mode if a decodable command isn't received within a reasonable
// amount of time.
Timer IRTimer;

// IR Learning Mode.  The PC enters learning mode via special function 65 12.
// The states are:
//
//   0 -> normal operation (not in learning mode)
//   1 -> learning mode; reading raw codes, no command read yet
//   2 -> learning mode; command received, awaiting auto-repeat
//   3 -> learning mode; done, command and repeat mode decoded
//
// When we enter learning mode, we reset IRTimer to keep track of how long
// we've been in the mode.  This allows the mode to time out if no code is
// received within a reasonable time.
uint8_t IRLearningMode = 0;

// Learning mode command received.  This stores the first decoded command
// when in learning mode.  For some protocols, we can't just report the
// first command we receive, because we need to wait for an auto-repeat to
// determine what format the remote uses for repeats.  This stores the first
// command while we await a repeat.  This is necessary for protocols that 
// have "dittos", since some remotes for such protocols use the dittos and 
// some don't; the only way to find out is to read a repeat code and see if 
// it's a ditto or just a repeat of the full code.
IRCommand learnedIRCode;

// IR command received, as a config slot index, 1..MAX_IR_CODES.
// When we receive a command that matches one of our programmed commands, 
// we note the slot here.  We also reset the IR timer so that we know how 
// long it's been since the command came in.  This lets us handle commands 
// with timed effects, such as PC key input.  Note that this is a 1-based 
// index; 0 represents no command.
uint8_t IRCommandIn = 0;

// "Toggle bit" of last command.  Some IR protocols have a toggle bit
// that distinguishes an auto-repeating key from a key being pressed
// several times in a row.  This records the toggle bit of the last
// command we received.
uint8_t lastIRToggle = 0;

// Are we in a gap between successive key presses?  When we detect that a 
// key is being pressed multiple times rather than auto-repeated (which we 
// can detect via a toggle bit in some protocols), we'll briefly stop sending 
// the associated key to the PC, so that the PC likewise recognizes the 
// distinct key press.  
uint8_t IRKeyGap = false;


// initialize
void init_IR(Config &cfg, bool &kbKeys)
{
    PinName pin;
    
    // start the IR timer
    IRTimer.start();
    
    // if there's a transmitter, set it up
    if ((pin = wirePinName(cfg.IR.emitter)) != NC)
    {
        // no virtual buttons yet
        int nVirtualButtons = 0;
        memset(IRConfigSlotToVirtualButton, 0xFF, sizeof(IRConfigSlotToVirtualButton));
        
        // assign virtual buttons slots for TV ON codes
        for (int i = 0 ; i < MAX_IR_CODES ; ++i)
        {
            if ((cfg.IRCommand[i].flags & IRFlagTVON) != 0)
                IRConfigSlotToVirtualButton[i] = nVirtualButtons++;
        }
            
        // assign virtual buttons for codes that can be triggered by 
        // real button inputs
        for (int i = 0 ; i < MAX_BUTTONS ; ++i)
        {
            // get the button
            ButtonCfg &b = cfg.button[i];
            
            // check the unshifted button
            int c = b.IRCommand - 1;
            if (c >= 0 && c < MAX_IR_CODES 
                && IRConfigSlotToVirtualButton[c] == 0xFF)
                IRConfigSlotToVirtualButton[c] = nVirtualButtons++;
                
            // check the shifted button
            c = b.IRCommand2 - 1;
            if (c >= 0 && c < MAX_IR_CODES 
                && IRConfigSlotToVirtualButton[c] == 0xFF)
                IRConfigSlotToVirtualButton[c] = nVirtualButtons++;
        }
        
        // allocate an additional virtual button for transmitting ad hoc
        // codes, such as for the "send code" USB API function
        IRAdHocBtn = nVirtualButtons++;
            
        // create the transmitter
        ir_tx = new IRTransmitter(pin, nVirtualButtons);
        
        // program the commands into the virtual button slots
        for (int i = 0 ; i < MAX_IR_CODES ; ++i)
        {
            // if this slot is assigned to a virtual button, program it
            int vb = IRConfigSlotToVirtualButton[i];
            if (vb != 0xFF)
            {
                IRCommandCfg &cb = cfg.IRCommand[i];
                uint64_t code = cb.code.lo | (uint64_t(cb.code.hi) << 32);
                bool dittos = (cb.flags & IRFlagDittos) != 0;
                ir_tx->programButton(vb, cb.protocol, dittos, code);
            }
        }
    }

    // if there's a receiver, set it up
    if ((pin = wirePinName(cfg.IR.sensor)) != NC)
    {
        // create the receiver
        ir_rx = new IRReceiver(pin, 32);
        
        // connect the transmitter (if any) to the receiver, so that
        // the receiver can suppress reception of our own transmissions
        ir_rx->setTransmitter(ir_tx);
        
        // enable it
        ir_rx->enable();
        
        // Check the IR command slots to see if any slots are configured
        // to send a keyboard key on receiving an IR command.  If any are,
        // tell the caller that we need a USB keyboard interface.
        for (int i = 0 ; i < MAX_IR_CODES ; ++i)
        {
            IRCommandCfg &cb = cfg.IRCommand[i];
            if (cb.protocol != 0
                && (cb.keytype == BtnTypeKey || cb.keytype == BtnTypeMedia))
            {
                kbKeys = true;
                break;
            }
        }
    }
}

// Press or release a button with an assigned IR function.  'cmd'
// is the command slot number (1..MAX_IR_CODES) assigned to the button.
void IR_buttonChange(uint8_t cmd, bool pressed)
{
    // only proceed if there's an IR transmitter attached
    if (ir_tx != 0)
    {
        // adjust the command slot to a zero-based index
        int slot = cmd - 1;
        
        // press or release the virtual button
        ir_tx->pushButton(IRConfigSlotToVirtualButton[slot], pressed);
    }
}

// Process IR input and output
void process_IR(Config &cfg, USBJoystick &js)
{
    // check for transmitter tasks, if there's a transmitter
    if (ir_tx != 0)
    {
        // If we're not currently sending, and an ad hoc IR command
        // is ready to send, send it.
        if (!ir_tx->isSending() && IRAdHocCmd.ready)
        {
            // program the command into the transmitter virtual button
            // that we reserved for ad hoc commands
            ir_tx->programButton(IRAdHocBtn, IRAdHocCmd.protocol,
                IRAdHocCmd.dittos, IRAdHocCmd.code);
                
            // send the command - just pulse the button to send it once
            ir_tx->pushButton(IRAdHocBtn, true);
            ir_tx->pushButton(IRAdHocBtn, false);
            
            // we've sent the command, so clear the 'ready' flag
            IRAdHocCmd.ready = false;
        }
    }
    
    // check for receiver tasks, if there's a receiver
    if (ir_rx != 0)
    {
        // Time out any received command
        if (IRCommandIn != 0)
        {
            // Time out commands after 200ms without a repeat signal.
            // Time out the inter-key gap after 50ms.
            uint32_t t = IRTimer.read_us();
            if (t > 200000)
                IRCommandIn = 0;
            else if (t > 50000)
                IRKeyGap = false;
        }
    
        // Check if we're in learning mode
        if (IRLearningMode != 0)
        {
            // Learning mode.  Read raw inputs from the IR sensor and 
            // forward them to the PC via USB reports, up to the report
            // limit.
            const int nmax = USBJoystick::maxRawIR;
            uint16_t raw[nmax];
            int n;
            for (n = 0 ; n < nmax && ir_rx->processOne(raw[n]) ; ++n) ;
            
            // if we read any raw samples, report them
            if (n != 0)
                js.reportRawIR(n, raw);
                
            // check for a command
            IRCommand c;
            if (ir_rx->readCommand(c))
            {
                // check the current learning state
                switch (IRLearningMode)
                {
                case 1:
                    // Initial state, waiting for the first decoded command.
                    // This is it.
                    learnedIRCode = c;
                    
                    // Check if we need additional information.  If the
                    // protocol supports dittos, we have to wait for a repeat
                    // to see if the remote actually uses the dittos, since
                    // some implementations of such protocols use the dittos
                    // while others just send repeated full codes.  Otherwise,
                    // all we need is the initial code, so we're done.
                    IRLearningMode = (c.hasDittos ? 2 : 3);
                    break;
                    
                case 2:
                    // Code received, awaiting auto-repeat information.  If
                    // the protocol has dittos, check to see if we got a ditto:
                    //
                    // - If we received a ditto in the same protocol as the
                    //   prior command, the remote uses dittos.
                    //
                    // - If we received a repeat of the prior command (not a
                    //   ditto, but a repeat of the full code), the remote
                    //   doesn't use dittos even though the protocol supports
                    //   them.
                    //
                    // - Otherwise, it's not an auto-repeat at all, so we
                    //   can't decide one way or the other on dittos: start
                    //   over.
                    if (c.proId == learnedIRCode.proId
                        && c.hasDittos
                        && c.ditto)
                    {
                        // success - the remote uses dittos
                        IRLearningMode = 3;
                    }
                    else if (c.proId == learnedIRCode.proId
                        && c.hasDittos
                        && !c.ditto
                        && c.code == learnedIRCode.code)
                    {
                        // success - it's a repeat of the last code, so
                        // the remote doesn't use dittos even though the
                        // protocol supports them
                        learnedIRCode.hasDittos = false;
                        IRLearningMode = 3;
                    }
                    else
                    {
                        // It's not a ditto and not a full repeat of the
                        // last code, so it's either a new key, or some kind
                        // of multi-code key encoding that we don't recognize.
                        // We can't use this code, so start over.
                        IRLearningMode = 1;
                    }
                    break;
                }
                
                // If we ended in state 3, we've successfully decoded
                // the transmission.  Report the decoded data and terminate
                // learning mode.
                if (IRLearningMode == 3)
                {
                    // figure the flags: 
                    //   0x02 -> dittos
                    uint8_t flags = 0;
                    if (learnedIRCode.hasDittos)
                        flags |= 0x02;
                        
                    // report the code
                    js.reportIRCode(learnedIRCode.proId, flags, learnedIRCode.code);
                        
                    // exit learning mode
                    IRLearningMode = 0;
                }
            }
            
            // time out of IR learning mode if it's been too long
            if (IRLearningMode != 0 && IRTimer.read_us() > 10000000L)
            {
                // report the termination by sending a raw IR report with
                // zero data elements
                js.reportRawIR(0, 0);
                
                
                // cancel learning mode
                IRLearningMode = 0;
            }
        }
        else
        {
            // Not in learning mode.  We don't care about the raw signals;
            // just run them through the protocol decoders.
            ir_rx->process();
            
            // Check for decoded commands.  Keep going until all commands
            // have been read.
            IRCommand c;
            while (ir_rx->readCommand(c))
            {
                // We received a decoded command.  Determine if it's a repeat,
                // and if so, try to determine whether it's an auto-repeat (due
                // to the remote key being held down) or a distinct new press 
                // on the same key as last time.  The distinction is significant
                // because it affects the auto-repeat behavior of the PC key
                // input.  An auto-repeat represents a key being held down on
                // the remote, which we want to translate to a (virtual) key 
                // being held down on the PC keyboard; a distinct key press on
                // the remote translates to a distinct key press on the PC.
                //
                // It can only be a repeat if there's a prior command that
                // hasn't timed out yet, so start by checking for a previous
                // command.
                bool repeat = false, autoRepeat = false;
                if (IRCommandIn != 0)
                {
                    // We have a command in progress.  Check to see if the
                    // new command is a repeat of the previous command.  Check
                    // first to see if it's a "ditto", which explicitly represents
                    // an auto-repeat of the last command.
                    IRCommandCfg &cmdcfg = cfg.IRCommand[IRCommandIn - 1];
                    if (c.ditto)
                    {
                        // We received a ditto.  Dittos are always auto-
                        // repeats, so it's an auto-repeat as long as the
                        // ditto is in the same protocol as the last command.
                        // If the ditto is in a new protocol, the ditto can't
                        // be for the last command we saw, because a ditto
                        // never changes protocols from its antecedent.  In
                        // such a case, we must have missed the antecedent
                        // command and thus don't know what's being repeated.
                        repeat = autoRepeat = (c.proId == cmdcfg.protocol);
                    }
                    else
                    {
                        // It's not a ditto.  The new command is a repeat if
                        // it matches the protocol and command code of the 
                        // prior command.
                        repeat = (c.proId == cmdcfg.protocol 
                                  && uint32_t(c.code) == cmdcfg.code.lo
                                  && uint32_t(c.code >> 32) == cmdcfg.code.hi);
                                  
                        // If the command is a repeat, try to determine whether
                        // it's an auto-repeat or a new press on the same key.
                        // If the protocol uses dittos, it's definitely a new
                        // key press, because an auto-repeat would have used a
                        // ditto.  For a protocol that doesn't use dittos, both
                        // an auto-repeat and a new key press just send the key
                        // code again, so we can't tell the difference based on
                        // that alone.  But if the protocol has a toggle bit, we
                        // can tell by the toggle bit value: a new key press has
                        // the opposite toggle value as the last key press, while 
                        // an auto-repeat has the same toggle.  Note that if the
                        // protocol doesn't use toggle bits, the toggle value
                        // will always be the same, so we'll simply always treat
                        // any repeat as an auto-repeat.  Many protocols simply
                        // provide no way to distinguish the two, so in such
                        // cases it's consistent with the native implementations
                        // to treat any repeat as an auto-repeat.
                        autoRepeat = 
                            repeat 
                            && !(cmdcfg.flags & IRFlagDittos)
                            && c.toggle == lastIRToggle;
                    }
                }
                
                // Check to see if it's a repeat of any kind
                if (repeat)
                {
                    // It's a repeat.  If it's not an auto-repeat, it's a
                    // new distinct key press, so we need to send the PC a
                    // momentary gap where we're not sending the same key,
                    // so that the PC also recognizes this as a distinct
                    // key press event.
                    if (!autoRepeat)
                        IRKeyGap = true;
                        
                    // restart the key-up timer
                    IRTimer.reset();
                }
                else if (c.ditto)
                {
                    // It's a ditto, but not a repeat of the last command.
                    // But a ditto doesn't contain any information of its own
                    // on the command being repeated, so given that it's not
                    // our last command, we can't infer what command the ditto
                    // is for and thus can't make sense of it.  We have to
                    // simply ignore it and wait for the sender to start with
                    // a full command for a new key press.
                    IRCommandIn = 0;
                }
                else
                {
                    // It's not a repeat, so the last command is no longer
                    // in effect (regardless of whether we find a match for
                    // the new command).
                    IRCommandIn = 0;
                    
                    // Check to see if we recognize the new command, by
                    // searching for a match in our learned code list.
                    for (int i = 0 ; i < MAX_IR_CODES ; ++i)
                    {
                        // if the protocol and command code from the code
                        // list both match the input, it's a match
                        IRCommandCfg &cmdcfg = cfg.IRCommand[i];
                        if (cmdcfg.protocol == c.proId 
                            && cmdcfg.code.lo == uint32_t(c.code)
                            && cmdcfg.code.hi == uint32_t(c.code >> 32))
                        {
                            // Found it!  Make this the last command, and 
                            // remember the starting time.
                            IRCommandIn = i + 1;
                            lastIRToggle = c.toggle;
                            IRTimer.reset();
                            
                            // no need to keep searching
                            break;
                        }
                    }
                }
            }
        }
    }
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
        physState = logState = prevLogState = 0;
        virtState = 0;
        dbState = 0;
        pulseState = 0;
        pulseTime = 0;
    }
    
    // "Virtually" press or un-press the button.  This can be used to
    // control the button state via a software (virtual) source, such as
    // the ZB Launch Ball feature.
    //
    // To allow sharing of one button by multiple virtual sources, each
    // virtual source must keep track of its own state internally, and 
    // only call this routine to CHANGE the state.  This is because calls
    // to this routine are additive: turning the button ON twice will
    // require turning it OFF twice before it actually turns off.
    void virtPress(bool on)
    {
        // Increment or decrement the current state
        virtState += on ? 1 : -1;
    }
    
    // DigitalIn for the button, if connected to a physical input
    TinyDigitalIn di;
    
    // Time of last pulse state transition.
    //
    // Each state change sticks for a minimum period; when the timer expires,
    // if the underlying physical switch is in a different state, we switch
    // to the next state and restart the timer.  pulseTime is the time remaining
    // remaining before we can make another state transition, in microseconds.
    // The state transitions require a complete cycle, 1 -> 2 -> 3 -> 4 -> 1...; 
    // this guarantees that the parity of the pulse count always matches the 
    // current physical switch state when the latter is stable, which makes
    // it impossible to "trick" the host by rapidly toggling the switch state.
    // (On my original Pinscape cabinet, I had a hardware pulse generator
    // for coin door, and that *was* possible to trick by rapid toggling.
    // This software system can't be fooled that way.)
    uint32_t pulseTime;
    
    // Config key index.  This points to the ButtonCfg structure in the
    // configuration that contains the PC key mapping for the button.
    uint8_t cfgIndex;
    
    // Virtual press state.  This is used to simulate pressing the button via
    // software inputs rather than physical inputs.  To allow one button to be
    // controlled by mulitple software sources, each source should keep track
    // of its own virtual state for the button independently, and then INCREMENT
    // this variable when the source's state transitions from off to on, and
    // DECREMENT it when the source's state transitions from on to off.  That
    // will make the button's pressed state the logical OR of all of the virtual
    // and physical source states.
    uint8_t virtState;
    
    // Debounce history.  On each scan, we shift in a 1 bit to the lsb if
    // the physical key is reporting ON, and shift in a 0 bit if the physical
    // key is reporting OFF.  We consider the key to have a new stable state
    // if we have N consecutive 0's or 1's in the low N bits (where N is
    // a parameter that determines how long we wait for transients to settle).
    uint8_t dbState;
    
    // current PHYSICAL on/off state, after debouncing
    uint8_t physState : 1;
    
    // current LOGICAL on/off state as reported to the host.
    uint8_t logState : 1;

    // Previous logical on/off state, when keys were last processed for USB 
    // reports and local effects.  This lets us detect edges (transitions)
    // in the logical state, for effects that are triggered when the state
    // changes rather than merely by the button being on or off.
    uint8_t prevLogState : 1;
    
    // Pulse state
    // 
    // A button in pulse mode (selected via the config flags for the button) 
    // transmits a brief logical button press and release each time the attached 
    // physical switch changes state.  This is useful for cases where the host 
    // expects a key press for each change in the state of the physical switch.  
    // The canonical example is the Coin Door switch in VPinMAME, which requires 
    // pressing the END key to toggle the open/closed state.  This software design 
    // isn't easily implemented in a physical coin door, though; the simplest
    // physical sensor for the coin door state is a switch that's on when the 
    // door is open and off when the door is closed (or vice versa, but in either 
    // case, the switch state corresponds to the current state of the door at any
    // given time, rather than pulsing on state changes).  The "pulse mode"
    // option bridges this gap by generating a toggle key event each time
    // there's a change to the physical switch's state.
    //
    // Pulse state:
    //   0 -> not a pulse switch - logical key state equals physical switch state
    //   1 -> off
    //   2 -> transitioning off-on
    //   3 -> on
    //   4 -> transitioning on-off
    uint8_t pulseState : 3;         // 5 states -> we need 3 bits

} __attribute__((packed));

ButtonState *buttonState;       // live button slots, allocated on startup
int8_t nButtons;                // number of live button slots allocated
int8_t zblButtonIndex = -1;     // index of ZB Launch button slot; -1 if unused

// Shift button state
struct
{
    int8_t index;               // buttonState[] index of shift button; -1 if none
    uint8_t state;              // current state, for "Key OR Shift" mode:
                                //   0 = not shifted
                                //   1 = shift button down, no key pressed yet
                                //   2 = shift button down, key pressed
                                //   3 = released, sending pulsed keystroke
    uint32_t pulseTime;         // time remaining in pulsed keystroke (state 3)
}
__attribute__((packed)) shiftButton;

// Button data
uint32_t jsButtons = 0;

// Keyboard report state.  This tracks the USB keyboard state.  We can
// report at most 6 simultaneous non-modifier keys here, plus the 8
// modifier keys.
struct
{
    bool changed;       // flag: changed since last report sent
    uint8_t nkeys;      // number of active keys in the list
    uint8_t data[8];    // key state, in USB report format: byte 0 is the modifier key mask,
                        // byte 1 is reserved, and bytes 2-7 are the currently pressed key codes
} kbState = { false, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };

// Media key state
struct
{
    bool changed;       // flag: changed since last report sent
    uint8_t data;       // key state byte for USB reports
} mediaState = { false, 0 };

// button scan interrupt timer
Timeout scanButtonsTimeout;

// Button scan interrupt handler.  We call this periodically via
// a timer interrupt to scan the physical button states.  
void scanButtons()
{
    // schedule the next interrupt
    scanButtonsTimeout.attach_us(&scanButtons, 1000);
    
    // scan all button input pins
    ButtonState *bs = buttonState, *last = bs + nButtons;
    for ( ; bs < last ; ++bs)
    {
        // Shift the new state into the debounce history
        uint8_t db = (bs->dbState << 1) | bs->di.read();
        bs->dbState = db;
        
        // If we have all 0's or 1's in the history for the required
        // debounce period, the key state is stable, so apply the new
        // physical state.  Note that the pins are active low, so the
        // new button on/off state is the inverse of the GPIO state.
        const uint8_t stable = 0x1F;   // 00011111b -> low 5 bits = last 5 readings
        db &= stable;
        if (db == 0 || db == stable)
            bs->physState = !db;
    }
}

// Button state transition timer.  This is used for pulse buttons, to
// control the timing of the logical key presses generated by transitions
// in the physical button state.
Timer buttonTimer;

// Count a button during the initial setup scan
void countButton(uint8_t typ, uint8_t shiftTyp, bool &kbKeys)
{
    // count it
    ++nButtons;
    
    // if it's a keyboard key or media key, note that we need a USB 
    // keyboard interface
    if (typ == BtnTypeKey || typ == BtnTypeMedia
        || shiftTyp == BtnTypeKey || shiftTyp == BtnTypeMedia)
        kbKeys = true;
}

// initialize the button inputs
void initButtons(Config &cfg, bool &kbKeys)
{
    // presume no shift key
    shiftButton.index = -1;
    shiftButton.state = 0;
    
    // Count up how many button slots we'll need to allocate.  Start
    // with assigned buttons from the configuration, noting that we
    // only need to create slots for buttons that are actually wired.
    nButtons = 0;
    for (int i = 0 ; i < MAX_BUTTONS ; ++i)
    {
        // it's valid if it's wired to a real input pin
        if (wirePinName(cfg.button[i].pin) != NC)
            countButton(cfg.button[i].typ, cfg.button[i].typ2, kbKeys);
    }
    
    // Count virtual buttons

    // ZB Launch
    if (cfg.plunger.zbLaunchBall.port != 0)
    {
        // valid - remember the live button index
        zblButtonIndex = nButtons;
        
        // count it
        countButton(cfg.plunger.zbLaunchBall.keytype, BtnTypeNone, kbKeys);
    }

    // Allocate the live button slots
    ButtonState *bs = buttonState = new ButtonState[nButtons];
    
    // Configure the physical inputs
    for (int i = 0 ; i < MAX_BUTTONS ; ++i)
    {
        PinName pin = wirePinName(cfg.button[i].pin);
        if (pin != NC)
        {
            // point back to the config slot for the keyboard data
            bs->cfgIndex = i;

            // set up the GPIO input pin for this button
            bs->di.assignPin(pin);
            
            // if it's a pulse mode button, set the initial pulse state to Off
            if (cfg.button[i].flags & BtnFlagPulse)
                bs->pulseState = 1;
                
            // If this is the shift button, note its buttonState[] index.
            // We have to figure the buttonState[] index separately from
            // the config index, because the indices can differ if some
            // config slots are left unused.
            if (cfg.shiftButton.idx == i+1)
                shiftButton.index = bs - buttonState;
                
            // advance to the next button
            ++bs;
        }
    }
    
    // Configure the virtual buttons.  These are buttons controlled via
    // software triggers rather than physical GPIO inputs.  The virtual
    // buttons have the same control structures as regular buttons, but
    // they get their configuration data from other config variables.
    
    // ZB Launch Ball button
    if (cfg.plunger.zbLaunchBall.port != 0)
    {
        // Point back to the config slot for the keyboard data.
        // We use a special extra slot for virtual buttons, 
        // so we also need to set up the slot data by copying
        // the ZBL config data to our virtual button slot.
        bs->cfgIndex = ZBL_BUTTON_CFG;
        cfg.button[ZBL_BUTTON_CFG].pin = PINNAME_TO_WIRE(NC);
        cfg.button[ZBL_BUTTON_CFG].typ = cfg.plunger.zbLaunchBall.keytype;
        cfg.button[ZBL_BUTTON_CFG].val = cfg.plunger.zbLaunchBall.keycode;
        
        // advance to the next button
        ++bs;
    }
    
    // start the button scan thread
    scanButtonsTimeout.attach_us(scanButtons, 1000);

    // start the button state transition timer
    buttonTimer.start();
}

// Media key mapping.  This maps from an 8-bit USB media key
// code to the corresponding bit in our USB report descriptor.
// The USB key code is the index, and the value at the index
// is the report descriptor bit.  See joystick.cpp for the
// media descriptor details.  Our currently mapped keys are:
//
//    0xE2 -> Mute -> 0x01
//    0xE9 -> Volume Up -> 0x02
//    0xEA -> Volume Down -> 0x04
//    0xB5 -> Next Track -> 0x08
//    0xB6 -> Previous Track -> 0x10
//    0xB7 -> Stop -> 0x20
//    0xCD -> Play / Pause -> 0x40
//
static const uint8_t mediaKeyMap[] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 00-0F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 10-1F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 20-2F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 30-3F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 40-4F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 50-5F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 60-6F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 70-7F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 80-8F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 90-9F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // A0-AF
     0,  0,  0,  0,  0,  8, 16, 32,  0,  0,  0,  0,  0,  0,  0,  0, // B0-BF
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 64,  0,  0, // C0-CF
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // D0-DF
     0,  0,  1,  0,  0,  0,  0,  0,  0,  2,  4,  0,  0,  0,  0,  0, // E0-EF
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0  // F0-FF
};
 
// Keyboard key/joystick button state.  processButtons() uses this to 
// build the set of key presses to report to the PC based on the logical
// states of the button iputs.
struct KeyState
{
    KeyState()
    {
        // zero all members
        memset(this, 0, sizeof(*this));
    }
    
    // Keyboard media keys currently pressed.  This is a bit vector in
    // the format used in our USB keyboard reports (see USBJoystick.cpp).
    uint8_t mediakeys;
         
    // Keyboard modifier (shift) keys currently pressed.  This is a bit 
    // vector in the format used in our USB keyboard reports (see
    // USBJoystick.cpp).
    uint8_t modkeys;
     
    // Regular keyboard keys currently pressed.  Each element is a USB
    // key code, or 0 for empty slots.  Note that the USB report format
    // theoretically allows a flexible size limit, but the Windows KB
    // drivers have a fixed limit of 6 simultaneous keys (and won't
    // accept reports with more), so there's no point in making this
    // flexible; we'll just use the fixed size dictated by Windows.
    uint8_t keys[7];
     
    // number of valid entries in keys[] array
    int nkeys;
     
    // Joystick buttons pressed, as a bit vector.  Bit n (1 << n)
    // represents joystick button n, n in 0..31, with 0 meaning 
    // unpressed and 1 meaning pressed.
    uint32_t js;
    
    
    // Add a key press.  'typ' is the button type code (ButtonTypeXxx),
    // and 'val' is the value (the meaning of which varies by type code).
    void addKey(uint8_t typ, uint8_t val)
    {
        // add the key according to the type
        switch (typ)
        {
        case BtnTypeJoystick:
            // joystick button
            js |= (1 << (val - 1));
            break;
            
        case BtnTypeKey:
            // Keyboard key.  The USB keyboard report encodes regular
            // keys and modifier keys separately, so we need to check
            // which type we have.  Note that past versions mapped the 
            // Keyboard Volume Up, Keyboard Volume Down, and Keyboard 
            // Mute keys to the corresponding Media keys.  We no longer
            // do this; instead, we have the separate BtnTypeMedia for
            // explicitly using media keys if desired.
            if (val >= 0xE0 && val <= 0xE7)
            {
                // It's a modifier key.  These are represented in the USB 
                // reports with a bit mask.  We arrange the mask bits in
                // the same order as the scan codes, so we can figure the
                // appropriate bit with a simple shift.
                modkeys |= (1 << (val - 0xE0));
            }
            else
            {
                // It's a regular key.  Make sure it's not already in the 
                // list, and that the list isn't full.  If neither of these 
                // apply, add the key to the key array.
                if (nkeys < 7)
                {
                    bool found = false;
                    for (int i = 0 ; i < nkeys ; ++i)
                    {
                        if (keys[i] == val)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        keys[nkeys++] = val;
                }
            }
            break;

        case BtnTypeMedia:
            // Media control key.  The media keys are mapped in the USB
            // report to bits, whereas the key codes are specified in the
            // config with their USB usage numbers.  E.g., the config val
            // for Media Next Track is 0xB5, but we encode this in the USB
            // report as bit 0x08.  The mediaKeyMap[] table translates
            // from the USB usage number to the mask bit.  If the key isn't
            // among the subset we support, the mapped bit will be zero, so
            // the "|=" will have no effect and the key will be ignored.
            mediakeys |= mediaKeyMap[val];
            break;                                
        }
    }
};


// Process the button state.  This sets up the joystick, keyboard, and
// media control descriptors with the current state of keys mapped to
// those HID interfaces, and executes the local effects for any keys 
// mapped to special device functions (e.g., Night Mode).
void processButtons(Config &cfg)
{
    // key state
    KeyState ks;
    
    // calculate the time since the last run
    uint32_t dt = buttonTimer.read_us();
    buttonTimer.reset();
    
    // check the shift button state
    if (shiftButton.index != -1)
    {
        // get the shift button's physical state object
        ButtonState *sbs = &buttonState[shiftButton.index];
        
        // figure what to do based on the shift button mode in the config
        switch (cfg.shiftButton.mode)
        {
        case 0:
        default:
            // "Shift OR Key" mode.  The shift button doesn't send its key
            // immediately when pressed.  Instead, we wait to see what 
            // happens while it's down.  Check the current cycle state.
            switch (shiftButton.state)
            {
            case 0:
                // Not shifted.  Check if the button is now down: if so,
                // switch to state 1 (shift button down, no key pressed yet).
                if (sbs->physState)
                    shiftButton.state = 1;
                break;
                
            case 1:
                // Shift button down, no key pressed yet.  If the button is
                // now up, it counts as an ordinary button press instead of
                // a shift button press, since the shift function was never
                // used.  Return to unshifted state and start a timed key 
                // pulse event.
                if (!sbs->physState)
                {
                    shiftButton.state = 3;
                    shiftButton.pulseTime = 50000+dt;  // 50 ms left on the key pulse
                }
                break;
                
            case 2:
                // Shift button down, other key was pressed.  If the button is
                // now up, simply clear the shift state without sending a key
                // press for the shift button itself to the PC.  The shift
                // function was used, so its ordinary key press function is
                // suppressed.
                if (!sbs->physState)
                    shiftButton.state = 0;
                break;
                
            case 3:
                // Sending pulsed keystroke.  Deduct the current time interval
                // from the remaining pulse timer.  End the pulse if the time
                // has expired.
                if (shiftButton.pulseTime > dt)
                    shiftButton.pulseTime -= dt;
                else
                    shiftButton.state = 0;
                break;
            }
            break;
            
        case 1:
            // "Shift AND Key" mode.  In this mode, the shift button acts
            // like any other button and sends its mapped key immediately.
            // The state cycle in this case simply matches the physical
            // state: ON -> cycle state 1, OFF -> cycle state 0.
            shiftButton.state = (sbs->physState ? 1 : 0);
            break;
        }
    }

    // scan the button list
    ButtonState *bs = buttonState;
    for (int i = 0 ; i < nButtons ; ++i, ++bs)
    {
        // get the config entry for the button
        ButtonCfg *bc = &cfg.button[bs->cfgIndex];

        // Check the button type:
        //   - shift button
        //   - pulsed button
        //   - regular button
        if (shiftButton.index == i)
        {
            // This is the shift button.  The logical state handling
            // depends on the mode.
            switch (cfg.shiftButton.mode)
            {
            case 0:
            default:
                // "Shift OR Key" mode.  The logical state is ON only
                // during the timed pulse when the key is released, which
                // is signified by shift button state 3.
                bs->logState = (shiftButton.state == 3);
                break;
                
            case 1:
                // "Shif AND Key" mode.  The shift button acts like any
                // other button, so it's logically on when physically on.
                bs->logState = bs->physState;
                break;
            }
        }        
        else if (bs->pulseState != 0)
        {
            // if the timer has expired, check for state changes
            if (bs->pulseTime > dt)
            {
                // not expired yet - deduct the last interval
                bs->pulseTime -= dt;
            }
            else
            {
                // pulse time expired - check for a state change
                const uint32_t pulseLength = 200000UL;  // 200 milliseconds
                switch (bs->pulseState)
                {
                case 1:
                    // off - if the physical switch is now on, start a button pulse
                    if (bs->physState) 
                    {
                        bs->pulseTime = pulseLength;
                        bs->pulseState = 2;
                        bs->logState = 1;
                    }
                    break;
                    
                case 2:
                    // transitioning off to on - end the pulse, and start a gap
                    // equal to the pulse time so that the host can observe the
                    // change in state in the logical button
                    bs->pulseState = 3;
                    bs->pulseTime = pulseLength;
                    bs->logState = 0;
                    break;
                    
                case 3:
                    // on - if the physical switch is now off, start a button pulse
                    if (!bs->physState) 
                    {
                        bs->pulseTime = pulseLength;
                        bs->pulseState = 4;
                        bs->logState = 1;
                    }
                    break;
                    
                case 4:
                    // transitioning on to off - end the pulse, and start a gap
                    bs->pulseState = 1;
                    bs->pulseTime = pulseLength;
                    bs->logState = 0;
                    break;
                }
            }
        }
        else
        {
            // not a pulse switch - the logical state is the same as the physical state
            bs->logState = bs->physState;
        }
        
        // Determine if we're going to use the shifted version of the
        // button.  We're using the shifted version if...
        // 
        //  - the shift button is down, AND
        //  - this button isn't itself the shift button, AND
        //  - this button has some kind of shifted meaning
        //
        // A "shifted meaning" means that we have any of the following 
        // assigned to the shifted version of the button: a key assignment, 
        // (in typ2,key2), an IR command (in IRCommand2), or Night mode.
        //
        // The test for Night Mode is a bit tricky.  The shifted version of 
        // the button is the Night Mode toggle if the button matches the 
        // Night Mode button index, AND its flags are set with "toggle mode
        // ON" (bit 0x02 is on) and "switch mode OFF" (bit 0x01 is off).
        // So (button flags) & 0x03 must equal 0x02.
        bool useShift = 
            (shiftButton.state != 0
             && shiftButton.index != i
             && (bc->typ2 != BtnTypeNone
                 || bc->IRCommand2 != 0
                 || (cfg.nightMode.btn == i+1 && (cfg.nightMode.flags & 0x03) == 0x02)));
                 
        // If we're using the shift function, and no other button has used
        // the shift function yet (shift state 1: "shift button is down but
        // no one has used the shift function yet"), then we've "consumed"
        // the shift button press (so go to shift state 2: "shift button has
        // been used by some other button press that has a shifted meaning").
        if (useShift && shiftButton.state == 1 && bs->logState)
            shiftButton.state = 2;

        // carry out any edge effects from buttons changing states
        if (bs->logState != bs->prevLogState)
        {
            // check to see if this is the Night Mode button
            if (cfg.nightMode.btn == i + 1)
            {
                // Check the switch type in the config flags.  If flag 0x01 is 
                // set, it's a persistent on/off switch, so the night mode 
                // state simply tracks the current state of the switch.  
                // Otherwise, it's a momentary button, so each button push 
                // (i.e., each transition from logical state OFF to ON) toggles 
                // the night mode state.
                //
                // Note that the "shift" flag (0x02) has no effect in switch
                // mode.  Shifting only works for toggle mode.
                if ((cfg.nightMode.flags & 0x01) != 0)
                {
                    // It's an on/off switch.  Night mode simply tracks the
                    // current switch state.
                    setNightMode(bs->logState);
                }
                else if (bs->logState)
                {
                    // It's a momentary toggle switch.  Toggle the night mode 
                    // state on each distinct press of the button: that is,
                    // whenever the button's logical state transitions from 
                    // OFF to ON.
                    //
                    // The "shift" flag (0x02) tells us whether night mode is
                    // assigned to the shifted or unshifted version of the
                    // button.
                    bool pressed;
                    if (shiftButton.index == i)
                    {
                        // This button is both the Shift button AND the Night
                        // Mode button.  This is a special case in that the
                        // Shift status is irrelevant, because it's obviously
                        // identical to the Night Mode status.  So it doesn't
                        // matter whether or not the Night Mode button has the
                        // shifted flags; the raw button state is all that
                        // counts in this case.
                        pressed = true;
                    }
                    else if ((cfg.nightMode.flags & 0x02) != 0)
                    {
                        // Shift bit is set - night mode is assigned to the
                        // shifted version of the button.  This is a Night
                        // Mode toggle only if the Shift button is pressed.
                        pressed = (shiftButton.state != 0);
                    }
                    else
                    {
                        // No shift bit - night mode is assigned to the
                        // regular unshifted button.  The button press only
                        // applies if the Shift button is NOT pressed.
                        pressed = (shiftButton.state == 0);
                    }
                    
                    // if it's pressed (even after considering the shift mode),
                    // toggle night mode
                    if (pressed)
                        toggleNightMode();
                }
            }
            
            // press or release IR virtual keys on key state changes
            uint8_t irc = useShift ? bc->IRCommand2 : bc->IRCommand;
            if (irc != 0)
                IR_buttonChange(irc, bs->logState);
            
            // remember the new state for comparison on the next run
            bs->prevLogState = bs->logState;
        }

        // if it's pressed, physically or virtually, add it to the appropriate 
        // key state list
        if (bs->logState || bs->virtState)
        {
            // Get the key type and code.  Start by assuming that we're
            // going to use the normal unshifted meaning.
            uint8_t typ, val;
            if (useShift)
            {
                typ = bc->typ2;
                val = bc->val2;
            }
            else
            {
                typ = bc->typ;
                val = bc->val;
            }
        
            // We've decided on the meaning of the button, so process
            // the keyboard or joystick event.
            ks.addKey(typ, val);
        }
    }
    
    // If an IR input command is in effect, add the IR command's
    // assigned key, if any.  If we're in an IR key gap, don't include
    // the IR key.
    if (IRCommandIn != 0 && !IRKeyGap)
    {
        IRCommandCfg &irc = cfg.IRCommand[IRCommandIn - 1];
        ks.addKey(irc.keytype, irc.keycode);
    }
    
    // We're finished building the new key state.  Update the global
    // key state variables to reflect the new state. 
    
    // set the new joystick buttons (no need to check for changes, as we
    // report these on every joystick report whether they changed or not)
    jsButtons = ks.js;
    
    // check for keyboard key changes (we only send keyboard reports when
    // something changes)
    if (kbState.data[0] != ks.modkeys
        || kbState.nkeys != ks.nkeys
        || memcmp(ks.keys, &kbState.data[2], 6) != 0)
    {
        // we have changes - set the change flag and store the new key data
        kbState.changed = true;
        kbState.data[0] = ks.modkeys;
        if (ks.nkeys <= 6) {
            // 6 or fewer simultaneous keys - report the key codes
            kbState.nkeys = ks.nkeys;
            memcpy(&kbState.data[2], ks.keys, 6);
        }
        else {
            // more than 6 simultaneous keys - report rollover (all '1' key codes)
            kbState.nkeys = 6;
            memset(&kbState.data[2], 1, 6);
        }
    }        
    
    // check for media key changes (we only send media key reports when
    // something changes)
    if (mediaState.data != ks.mediakeys)
    {
        // we have changes - set the change flag and store the new key data
        mediaState.changed = true;
        mediaState.data = ks.mediakeys;
    }
}

// Send a button status report
void reportButtonStatus(USBJoystick &js)
{
    // start with all buttons off
    uint8_t state[(MAX_BUTTONS+7)/8];
    memset(state, 0, sizeof(state));

    // pack the button states into bytes, one bit per button
    ButtonState *bs = buttonState;
    for (int i = 0 ; i < nButtons ; ++i, ++bs)
    {
        // get the physical state
        int b = bs->physState;
        
        // pack it into the appropriate bit
        int idx = bs->cfgIndex;
        int si = idx / 8;
        int shift = idx & 0x07;
        state[si] |= b << shift;
    }
    
    // send the report
    js.reportButtonStatus(MAX_BUTTONS, state);
}

// ---------------------------------------------------------------------------
//
// Customization joystick subbclass
//

class MyUSBJoystick: public USBJoystick
{
public:
    MyUSBJoystick(uint16_t vendor_id, uint16_t product_id, uint16_t product_release,
        bool waitForConnect, bool enableJoystick, int axisFormat, bool useKB) 
        : USBJoystick(vendor_id, product_id, product_release, waitForConnect, enableJoystick, axisFormat, useKB)
    {
        sleeping_ = false;
        reconnectPending_ = false;
        timer_.start();
    }
    
    // show diagnostic LED feedback for connect state
    void diagFlash()
    {
        if (!configured() || sleeping_)
        {
            // flash once if sleeping or twice if disconnected
            for (int j = isConnected() ? 1 : 2 ; j > 0 ; --j)
            {
                // short red flash
                diagLED(1, 0, 0);
                wait_us(50000);
                diagLED(0, 0, 0);
                wait_us(50000);
            }
        }
    }
    
    // are we connected?
    int isConnected()  { return configured(); }
    
    // Are we in sleep mode?  If true, this means that the hardware has
    // detected no activity on the bus for 3ms.  This happens when the
    // cable is physically disconnected, the computer is turned off, or
    // the connection is otherwise disabled.
    bool isSleeping() const { return sleeping_; }

    // If necessary, attempt to recover from a broken connection.
    //
    // This is a hack, to work around an apparent timing bug in the
    // KL25Z USB implementation that I haven't been able to solve any
    // other way.
    //
    // The issue: when we have an established connection, and the
    // connection is broken by physically unplugging the cable or by
    // rebooting the PC, the KL25Z sometimes fails to reconnect when
    // the physical connection is re-established.  The failure is 
    // sporadic; I'd guess it happens about 25% of the time, but I 
    // haven't collected any real statistics on it.  
    //
    // The proximate cause of the failure is a deadlock in the SETUP
    // protocol between the host and device that happens around the
    // point where the PC is requesting the configuration descriptor.
    // The exact point in the protocol where this occurs varies slightly;
    // it can occur a message or two before or after the Get Config
    // Descriptor packet.  No matter where it happens, the nature of
    // the deadlock is the same: the PC thinks it sees a STALL on EP0
    // from the device, so it terminates the connection attempt, which
    // stops further traffic on the cable.  The KL25Z USB hardware sees
    // the lack of traffic and triggers a SLEEP interrupt (a misnomer
    // for what should have been called a BROKEN CONNECTION interrupt).
    // Both sides simply stop talking at this point, so the connection
    // is effectively dead.  
    //
    // The strange thing is that, as far as I can tell, the KL25Z isn't
    // doing anything to trigger the STALL on its end.  Both the PC
    // and the KL25Z are happy up until the very point of the failure 
    // and show no signs of anything wrong in the protocol exchange.
    // In fact, every detail of the protocol exchange up to this point
    // is identical to every successful exchange that does finish the
    // whole setup process successfully, on both the KL25Z and Windows
    // sides of the connection.  I can't find any point of difference
    // between successful and unsuccessful sequences that suggests why
    // the fateful message fails.  This makes me suspect that whatever
    // is going wrong is inside the KL25Z USB hardware module, which 
    // is a pretty substantial black box - it has a lot of internal 
    // state that's inaccessible to the software.  Further bolstering 
    // this theory is a little experiment where I found that I could 
    // reproduce the exact sequence of events of a failed reconnect 
    // attempt in an *initial* connection, which is otherwise 100% 
    // reliable, by inserting a little bit of artifical time padding 
    // (200us per event) into the SETUP interrupt handler.  My
    // hypothesis is that the STALL event happens because the KL25Z
    // USB hardware is too slow to respond to a message.  I'm not 
    // sure why this would only happen after a disconnect and not
    // during the initial connection; maybe there's some reset work
    // in the hardware that takes a substantial amount of time after
    // a disconnect.
    //
    // The solution: the problem happens during the SETUP exchange,
    // after we've been assigned a bus address.  It only happens on
    // some percentage of connection requests, so if we can simply
    // start over when the failure occurs, we'll eventually succeed
    // simply because not every attempt fails.  The ideal would be
    // to get the success rate up to 100%, but I can't figure out how
    // to fix the underlying problem, so this is the next best thing.
    //
    // We can detect when the failure occurs by noticing when a SLEEP
    // interrupt happens while we have an assigned bus address.
    //
    // To start a new connection attempt, we have to make the *host*
    // try again.  The logical connection is initiated solely by the
    // host.  Fortunately, it's easy to get the host to initiate the
    // process: if we disconnect on the device side, it effectively
    // makes the device look to the PC like it's electrically unplugged.
    // When we reconnect on the device side, the PC thinks a new device
    // has been plugged in and initiates the logical connection setup.
    // We have to remain disconnected for some minimum interval before
    // the host notices; the exact minimum is unclear, but 5ms seems 
    // reliable in practice.
    // 
    // Here's the full algorithm:
    //
    // 1. In the SLEEP interrupt handler, if we have a bus address,
    // we disconnect the device.  This happens in ISR context, so we
    // can't wait around for 5ms.  Instead, we simply set a flag noting
    // that the connection has been broken, and we note the time and
    // return.
    //
    // 2. In our main loop, whenever we find that we're disconnected,
    // we call recoverConnection().  The main loop's job is basically a
    // bunch of device polling.  We're just one more device to poll, so
    // recoverConnection() will be called soon after a disconnect, and
    // then will be called in a loop for as long as we're disconnected.
    //
    // 3. In recoverConnection(), we check the flag we set in the SLEEP
    // handler.  If set, we wait until 5ms has elapsed from the SLEEP
    // event time that we noted, then we'll reconnect and clear the flag.
    // This gives us the required 5ms (or longer) delay between the
    // disconnect and reconnect, ensuring that the PC will notice and
    // will start over with the connection protocol.
    //
    // 4. The main loop keeps calling recoverConnection() in a loop for
    // as long as we're disconnected, so if the new connection attempt
    // triggered in step 3 fails, the SLEEP interrupt will happen again,
    // we'll disconnect again, the flag will get set again, and 
    // recoverConnection() will reconnect again after another suitable
    // delay.  This will repeat until the connection succeeds or hell
    // freezes over.  
    //
    // Each disconnect happens immediately when a reconnect attempt 
    // fails, and an entire successful connection only takes about 25ms, 
    // so our loop can retry at more than 30 attempts per second.  
    // In my testing, lost connections almost always reconnect in
    // less than second with this code in place.
    void recoverConnection()
    {
        // if a reconnect is pending, reconnect
        if (reconnectPending_)
        {
            // Loop until we reach 5ms after the last sleep event.
            for (bool done = false ; !done ; )
            {
                // If we've reached the target time, reconnect.  Do the
                // time check and flag reset atomically, so that we can't
                // have another sleep event sneak in after we've verified
                // the time.  If another event occurs, it has to happen
                // before we check, in which case it'll update the time
                // before we check it, or after we clear the flag, in
                // which case it will reset the flag and we'll do another
                // round the next time we call this routine.
                __disable_irq();
                if (uint32_t(timer_.read_us() - lastSleepTime_) > 5000)
                {
                    connect(false);
                    reconnectPending_ = false;
                    done = true;
                }
                __enable_irq();
            }
        }
    }
    
protected:
    // Handle a USB SLEEP interrupt.  This interrupt signifies that the
    // USB hardware module hasn't seen any token traffic for 3ms, which 
    // means that we're either physically or logically disconnected. 
    //
    // Important: this runs in ISR context.
    //
    // Note that this is a specialized sense of "sleep" that's unrelated 
    // to the similarly named power modes on the PC.  This has nothing
    // to do with suspend/sleep mode on the PC, and it's not a low-power
    // mode on the KL25Z.  They really should have called this interrupt 
    // DISCONNECT or BROKEN CONNECTION.)
    virtual void sleepStateChanged(unsigned int sleeping)
    { 
        // note the new state
        sleeping_ = sleeping;
        
        // If we have a non-zero bus address, we have at least a partial
        // connection to the host (we've made it at least as far as the
        // SETUP stage).  Explicitly disconnect, and the pending reconnect
        // flag, and remember the time of the sleep event.
        if (USB0->ADDR != 0x00)
        {
            disconnect();
            lastSleepTime_ = timer_.read_us();
            reconnectPending_ = true;
        }
    }
    
    // is the USB connection asleep?
    volatile bool sleeping_; 
    
    // flag: reconnect pending after sleep event
    volatile bool reconnectPending_;
    
    // time of last sleep event while connected
    volatile uint32_t lastSleepTime_;
    
    // timer to keep track of interval since last sleep event
    Timer timer_;
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
// We collect data at the device's maximum rate of 800kHz (one sample 
// every 1.25ms).  To keep up with the high data rate, we use the 
// device's internal FIFO, and drain the FIFO by polling on each 
// iteration of our main application loop.  In the past, we used an
// interrupt handler to read the device immediately on the arrival of
// each sample, but this created too much latency for the IR remote
// receiver, due to the relatively long time it takes to transfer the
// accelerometer readings via I2C.  The device's on-board FIFO can
// store up to 32 samples, which gives us up to about 40ms between
// polling iterations before the buffer overflows.  Our main loop runs
// in under 2ms, so we can easily keep the FIFO far from overflowing.
//
// The MMA8451Q has three range modes, +/- 2G, 4G, and 8G.  The ADC
// sample is the same bit width (14 bits) in all modes, so the higher
// dynamic range modes trade physical precision for range.  For our
// purposes, precision is more important than range, so we use the
// +/-2G mode.  Further, our joystick range is calibrated for only
// +/-1G.  This was unintentional on my part; I didn't look at the
// MMA8451Q library closely enough to realize it was normalizing to
// actual "G" units, and assumed that it was normalizing to a -1..+1 
// scale.  In practice, a +/-1G scale seems perfectly adequate for
// virtual pinball use, so I'm sticking with that range for now.  But
// there might be some benefit in renormalizing to a +/-2G range, in
// that it would allow for higher dynamic range for very hard nudges.
// Everyone would have to tweak their nudge sensitivity in VP if I
// made that change, though, so I'm keeping it as is for now; it would
// be best to make it a config option ("accelerometer high dynamic range") 
// rather than change it across the board.
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
    AccHist() { x = y = dsq = 0; xtot = ytot = 0; cnt = 0; }
    void set(int x, int y, AccHist *prv)
    {
        // save the raw position
        this->x = x;
        this->y = y;
        this->dsq = distanceSquared(prv);
    }
    
    // reading for this entry
    int x, y;
    
    // (distance from previous entry) squared
    int dsq;
    
    // total and count of samples averaged over this period
    int xtot, ytot;
    int cnt;

    void clearAvg() { xtot = ytot = 0; cnt = 0; }    
    void addAvg(int x, int y) { xtot += x; ytot += y; ++cnt; }
    int xAvg() const { return xtot/cnt; }
    int yAvg() const { return ytot/cnt; }
    
    int distanceSquared(AccHist *p)
        { return square(p->x - x) + square(p->y - y); }
};

// accelerometer wrapper class
class Accel
{
public:
    Accel(PinName sda, PinName scl, int i2cAddr, PinName irqPin, 
        int range, int autoCenterMode)
        : mma_(sda, scl, i2cAddr)        
    {
        // remember the interrupt pin assignment
        irqPin_ = irqPin;
        
        // remember the range
        range_ = range;
        
        // set the auto-centering mode
        setAutoCenterMode(autoCenterMode);
        
        // no manual centering request has been received
        manualCenterRequest_ = false;

        // reset and initialize
        reset();
    }
    
    // Request manual centering.  This applies the trailing average
    // of recent measurements and applies it as the new center point
    // as soon as we have enough data.
    void manualCenterRequest() { manualCenterRequest_ = true; }
    
    // set the auto-centering mode
    void setAutoCenterMode(int mode)
    {
        // remember the mode
        autoCenterMode_ = mode;
        
        // Set the time between checks.  We check 5 times over the course
        // of the centering time, so the check interval is 1/5 of the total.
        if (mode == 0)
        {
            // mode 0 is the old default of 5 seconds, so check every 1s
            autoCenterCheckTime_ = 1000000;
        }
        else if (mode <= 60)
        {
            // mode 1-60 means reset after 'mode' seconds; the check
            // interval is 1/5 of this
            autoCenterCheckTime_ = mode*200000;
        }
        else
        {
            // Auto-centering is off, but still gather statistics to apply
            // when we get a manual centering request.  The check interval
            // in this case is 1/5 of the total time for the trailing average
            // we apply for the manual centering.  We want this to be long
            // enough to smooth out the data, but short enough that it only
            // includes recent data.
            autoCenterCheckTime_ = 500000;
        }
    }
    
    void reset()
    {
        // clear the center point
        cx_ = cy_ = 0;
        
        // start the auto-centering timer
        tCenter_.start();
        iAccPrv_ = nAccPrv_ = 0;
        
        // reset and initialize the MMA8451Q
        mma_.init();
        
        // set the range
        mma_.setRange(
            range_ == AccelRange4G ? 4 :
            range_ == AccelRange8G ? 8 :
            2);
                
        // set the average accumulators to zero
        xSum_ = ySum_ = 0;
        nSum_ = 0;
        
        // read the current registers to clear the data ready flag
        mma_.getAccXYZ(ax_, ay_, az_);
    }
    
    void poll()
    {
        // read samples until we clear the FIFO
        while (mma_.getFIFOCount() != 0)
        {
            int x, y, z;
            mma_.getAccXYZ(x, y, z);
            
            // add the new reading to the running total for averaging
            xSum_ += (x - cx_);
            ySum_ += (y - cy_);
            ++nSum_;
            
            // store the updates
            ax_ = x;
            ay_ = y;
            az_ = z;
        }
    }

    void get(int &x, int &y) 
    {
        // read the shared data and store locally for calculations
        int ax = ax_, ay = ay_;
        int xSum = xSum_, ySum = ySum_;
        int nSum = nSum_;
         
        // reset the average accumulators for the next run
        xSum_ = ySum_ = 0;
        nSum_ = 0;

        // add this sample to the current calibration interval's running total
        AccHist *p = accPrv_ + iAccPrv_;
        p->addAvg(ax, ay);

        // If we're in auto-centering mode, check for auto-centering
        // at intervals of 1/5 of the overall time.  If we're not in
        // auto-centering mode, check anyway at one-second intervals
        // so that we gather averages for manual centering requests.
        if (tCenter_.read_us() > autoCenterCheckTime_)
        {
            // add the latest raw sample to the history list
            AccHist *prv = p;
            iAccPrv_ = (iAccPrv_ + 1);
            if (iAccPrv_ >= maxAccPrv)
               iAccPrv_ = 0;
            p = accPrv_ + iAccPrv_;
            p->set(ax, ay, prv);

            // if we have a full complement, check for auto-centering
            if (nAccPrv_ >= maxAccPrv)
            {
                // Center if:
                //
                // - Auto-centering is on, and we've been stable over the
                //   whole sample period at our spot-check points
                //
                // - A manual centering request is pending
                //
                static const int accTol = 164*164;  // 1% of range, squared
                AccHist *p0 = accPrv_;
                if (manualCenterRequest_
                    || (autoCenterMode_ <= 60
                        && p0[0].dsq < accTol
                        && p0[1].dsq < accTol
                        && p0[2].dsq < accTol
                        && p0[3].dsq < accTol
                        && p0[4].dsq < accTol))
                {
                    // Figure the new calibration point as the average of
                    // the samples over the rest period
                    cx_ = (p0[0].xAvg() + p0[1].xAvg() + p0[2].xAvg() + p0[3].xAvg() + p0[4].xAvg())/5;
                    cy_ = (p0[0].yAvg() + p0[1].yAvg() + p0[2].yAvg() + p0[3].yAvg() + p0[4].yAvg())/5;
                    
                    // clear any pending manual centering request
                    manualCenterRequest_ = false;
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
        x = rawToReport(xSum/nSum);
        y = rawToReport(ySum/nSum);
         
#ifdef DEBUG_PRINTF
        if (x != 0 || y != 0)        
            printf("%f %f %d %d %f\r\n", vx, vy, x, y, dt);
#endif
    }    
         
private:
    // adjust a raw acceleration figure to a usb report value
    int rawToReport(int v)
    {
        // Scale to the joystick report range.  The accelerometer
        // readings use the native 14-bit signed integer representation,
        // so their scale is 2^13.
        //
        // The 1G range is special: it uses the 2G native hardware range,
        // but rescales the result to a 1G range for the joystick reports.
        // So for that mode, we divide by 4096 rather than 8192.  All of
        // the other modes map use the hardware scaling directly.
        int i = v*JOYMAX;
        i = (range_ == AccelRange1G ? i/4096 : i/8192);
        
        // if it's near the center, scale it roughly as 20*(i/20)^2,
        // to suppress noise near the rest position
        static const int filter[] = { 
            -18, -16, -14, -13, -11, -10, -8, -7, -6, -5, -4, -3, -2, -2, -1, -1, 0, 0, 0, 0,
            0,
            0, 0, 0, 0, 1, 1, 2, 2, 3, 4, 5, 6, 7, 8, 10, 11, 13, 14, 16, 18
        };
        return (i > 20 || i < -20 ? i : filter[i+20]);
    }

    // underlying accelerometer object
    MMA8451Q mma_;
    
    // last raw acceleration readings, on the device's signed 14-bit 
    // scale -8192..+8191
    int ax_, ay_, az_;
    
    // running sum of readings since last get()
    int xSum_, ySum_;
    
    // number of readings since last get()
    int nSum_;
        
    // Calibration reference point for accelerometer.  This is the
    // average reading on the accelerometer when in the neutral position
    // at rest.
    int cx_, cy_;
    
    // range (AccelRangeXxx value, from config.h)
    uint8_t range_;
    
    // auto-center mode: 
    //   0 = default of 5-second auto-centering
    //   1-60 = auto-center after this many seconds
    //   255 = auto-centering off (manual centering only)
    uint8_t autoCenterMode_;
    
    // flag: a manual centering request is pending
    bool manualCenterRequest_;

    // time in us between auto-centering incremental checks
    uint32_t autoCenterCheckTime_;
    
    // atuo-centering timer
    Timer tCenter_;

    // Auto-centering history.  This is a separate history list that
    // records results spaced out sparsely over time, so that we can
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
    uint8_t iAccPrv_, nAccPrv_;
    static const uint8_t maxAccPrv = 5;
    AccHist accPrv_[maxAccPrv];
    
    // interurupt pin name
    PinName irqPin_;
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

// Current PSU2 power state:
//   1 -> default: latch was on at last check, or we haven't checked yet
//   2 -> latch was off at last check, SET pulsed high
//   3 -> SET pulsed low, ready to check status
//   4 -> TV timer countdown in progress
//   5 -> TV relay on
//   6 -> sending IR signals designed as TV ON signals
uint8_t psu2_state = 1;

// TV relay state.  The TV relay can be controlled by the power-on
// timer and directly from the PC (via USB commands), so keep a
// separate state for each:
//   0x01 -> turned on by power-on timer
//   0x02 -> turned on by USB command
uint8_t tv_relay_state = 0x00;
const uint8_t TV_RELAY_POWERON = 0x01;
const uint8_t TV_RELAY_USB     = 0x02;

// pulse timer for manual TV relay pulses
Timer tvRelayManualTimer;

// TV ON IR command state.  When the main PSU2 power state reaches
// the IR phase, we use this sub-state counter to send the TV ON
// IR signals.  We initialize to state 0 when the main state counter
// reaches the IR step.  In state 0, we start transmitting the first
// (lowest numbered) IR command slot marked as containing a TV ON
// code, and advance to state 1.  In state 1, we check to see if
// the transmitter is still sending; if so, we do nothing, if so
// we start transmitting the second TV ON code and advance to state
// 2.  Continue until we run out of TV ON IR codes, at which point
// we advance to the next main psu2_state step.
uint8_t tvon_ir_state = 0;

// TV ON switch relay control output pin
DigitalOut *tv_relay;

// PSU2 power sensing circuit connections
DigitalIn *psu2_status_sense;
DigitalOut *psu2_status_set;

// Apply the current TV relay state
void tvRelayUpdate(uint8_t bit, bool state)
{
    // update the state
    if (state)
        tv_relay_state |= bit;
    else
        tv_relay_state &= ~bit;
    
    // set the relay GPIO to the new state
    if (tv_relay != 0)
        tv_relay->write(tv_relay_state != 0);
}

// Does the current power status allow a reboot?  We shouldn't reboot
// in certain power states, because some states are purely internal:
// we can't get enough information from the external power sensor to 
// return to the same state later.  Code that performs discretionary
// reboots should always check here first, and delay any reboot until
// we say it's okay.
static inline bool powerStatusAllowsReboot()
{
    // The only safe state for rebooting is state 1, idle/default.
    // In other states, we can't reboot, because the external sensor
    // and latch circuit doesn't give us enough information to return
    // to the same state later.
    return psu2_state == 1;
}

// PSU2 Status update routine.  The main loop calls this from time 
// to time to update the power sensing state and carry out TV ON 
// functions.
Timer powerStatusTimer;
uint32_t tv_delay_time_us;
void powerStatusUpdate(Config &cfg)
{
    // If the manual relay pulse timer is past the pulse time, end the
    // manual pulse.  The timer only runs when a pulse is active, so
    // it'll never read as past the time limit if a pulse isn't on.
    if (tvRelayManualTimer.read_us() > 250000)
    {
        // turn off the relay and disable the timer
        tvRelayUpdate(TV_RELAY_USB, false);
        tvRelayManualTimer.stop();
        tvRelayManualTimer.reset();
    }

    // Only update every 1/4 second or so.  Note that if the PSU2
    // circuit isn't configured, the initialization routine won't 
    // start the timer, so it'll always read zero and we'll always 
    // skip this whole routine.
    if (powerStatusTimer.read_us() < 250000)
        return;
        
    // reset the update timer for next time
    powerStatusTimer.reset();
    
    // TV ON timer.  We start this timer when we detect a change
    // in the PSU2 status from OFF to ON.  When the timer reaches
    // the configured TV ON delay time, and the PSU2 power is still
    // on, we'll trigger the TV ON relay and send the TV ON IR codes.
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
        powerTimerDiagState = 0;
        break;
        
    case 2:
        // PSU2 was off last time we checked, and we tried setting
        // the latch.  Drop the SET signal and go to CHECK state.
        psu2_status_set->write(0);
        psu2_state = 3;
        powerTimerDiagState = 0;
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
            
            // start the power timer diagnostic flashes
            powerTimerDiagState = 2;
        }
        else
        {
            // The latch didn't stick, so PSU2 was still off at
            // our last check.  Return to idle state.
            psu2_state = 1;
        }
        break;
        
    case 4:
        // TV timer countdown in progress.  The latch has to stay on during
        // the countdown; if the latch turns off, PSU2 power must have gone
        // off again before the countdown finished.
        if (!psu2_status_sense->read())
        {
            // power is off - start a new check cycle
            psu2_status_set->write(1);
            psu2_state = 2;
            break;
        }
        
        // Flash the power time diagnostic every two cycles
        powerTimerDiagState = (powerTimerDiagState + 1) & 0x03;
        
        // if we've reached the delay time, pulse the relay
        if (tv_timer.read_us() >= tv_delay_time_us)
        {
            // turn on the relay for one timer interval
            tvRelayUpdate(TV_RELAY_POWERON, true);
            psu2_state = 5;
            
            // show solid blue on the diagnostic LED while the relay is on
            powerTimerDiagState = 2;
        }
        break;
        
    case 5:
        // TV timer relay on.  We pulse this for one interval, so
        // it's now time to turn it off.
        tvRelayUpdate(TV_RELAY_POWERON, false);
        
        // Proceed to sending any TV ON IR commands
        psu2_state = 6;
        tvon_ir_state = 0;
    
        // diagnostic LEDs off for now
        powerTimerDiagState = 0;
        break;
        
    case 6:        
        // Sending TV ON IR signals.  Start with the assumption that
        // we have no IR work to do, in which case we're done with the
        // whole TV ON sequence.  So by default return to state 1.
        psu2_state = 1;
        powerTimerDiagState = 0;
        
        // If we have an IR emitter, check for TV ON IR commands
        if (ir_tx != 0)
        {
            // check to see if the last transmission is still in progress
            if (ir_tx->isSending())
            {
                // We're still sending the last transmission.  Stay in
                // state 6.
                psu2_state = 6;
                powerTimerDiagState = 4;
                break;
            }
                
            // The last transmission is done, so check for a new one.
            // Look for the Nth TV ON IR slot, where N is our state
            // number.
            for (int i = 0, n = 0 ; i < MAX_IR_CODES ; ++i)
            {
                // is this a TV ON command?
                if ((cfg.IRCommand[i].flags & IRFlagTVON) != 0)
                {
                    // It's a TV ON command - check if it's the one we're
                    // looking for.
                    if (n == tvon_ir_state)
                    {
                        // It's the one.  Start transmitting it by
                        // pushing its virtual button.
                        int vb = IRConfigSlotToVirtualButton[i];
                        ir_tx->pushButton(vb, true);
                        
                        // Pushing the button starts transmission, and once
                        // started, the transmission runs to completion even
                        // if the button is no longer pushed.  So we can 
                        // immediately un-push the button, since we only need
                        // to send the code once.
                        ir_tx->pushButton(vb, false);
                        
                        // Advance to the next TV ON IR state, where we'll
                        // await the end of this transmission and move on to
                        // the next one.
                        psu2_state = 6;
                        tvon_ir_state++;
                        break;
                    }
                    
                    // it's not ours - count it and keep looking
                    ++n;
                }
            }
        }
        break;
    }
    
    // update the diagnostic LEDs
    diagLED();
}

// Start the power status timer.  If the status sense circuit is enabled 
// in the configuration, we'll set up the pin connections and start the
// timer for our periodic status checks.  Does nothing if any of the pins 
// are configured as NC.
void startPowerStatusTimer(Config &cfg)
{
    // only start the timer if the pins are configured and the delay
    // time is nonzero
    powerStatusTimer.reset();
    if (cfg.TVON.statusPin != 0xFF 
        && cfg.TVON.latchPin != 0xFF)
    {
        // set up the power sensing circuit connections
        psu2_status_sense = new DigitalIn(wirePinName(cfg.TVON.statusPin));
        psu2_status_set = new DigitalOut(wirePinName(cfg.TVON.latchPin));
        
        // if there's a TV ON relay, set up its control pin
        if (cfg.TVON.relayPin != 0xFF)
            tv_relay = new DigitalOut(wirePinName(cfg.TVON.relayPin));
            
        // Set the TV ON delay time.  We store the time internally in
        // microseconds, but the configuration stores it in units of
        // 1/100 second = 10ms = 10000us.
        tv_delay_time_us = cfg.TVON.delayTime * 10000;;
    
        // Start the TV timer
        powerStatusTimer.start();
    }
}

// Operate the TV ON relay.  This allows manual control of the relay
// from the PC.  See protocol message 65 submessage 11.
//
// Mode:
//    0 = turn relay off
//    1 = turn relay on
//    2 = pulse relay 
void TVRelay(int mode)
{
    // if there's no TV relay control pin, ignore this
    if (tv_relay == 0)
        return;
    
    switch (mode)
    {
    case 0:
        // relay off
        tvRelayUpdate(TV_RELAY_USB, false);
        break;
        
    case 1:
        // relay on
        tvRelayUpdate(TV_RELAY_USB, true);
        break;
        
    case 2:
        // Turn the relay on and reset the manual TV pulse timer
        tvRelayUpdate(TV_RELAY_USB, true);
        tvRelayManualTimer.reset();
        tvRelayManualTimer.start();
        break;
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

// Save Config followup time, in seconds.  After a successful save,
// we leave the success flag on in the status for this interval.  At
// the end of the interval, we reboot the device if requested.
uint8_t saveConfigFollowupTime;

// is a reboot pending at the end of the config save followup interval?
uint8_t saveConfigRebootPending;

// status flag for successful config save - set to 0x40 on success
uint8_t saveConfigSucceededFlag;

// Timer for configuration change followup timer
ExtTimer saveConfigFollowupTimer;    


// For convenience, a macro for the Config part of the NVM structure
#define cfg (nvm.d.c)

// flash memory controller interface
FreescaleIAP iap;

// figure the flash address for the config data
const NVM *configFlashAddr()
{
    // figure the number of sectors we need, rounding up
    int nSectors = (sizeof(NVM) + SECTOR_SIZE - 1)/SECTOR_SIZE;
    
    // figure the total size required from the number of sectors
    int reservedSize = nSectors * SECTOR_SIZE;
    
    // locate it at the top of memory
    uint32_t addr = iap.flashSize() - reservedSize;
    
    // return it as a read-only NVM pointer
    return (const NVM *)addr;
}

// Load the config from flash.  Returns true if a valid non-default
// configuration was loaded, false if we not.  If we return false,
// we load the factory defaults, so the configuration object is valid 
// in either case.
bool loadConfigFromFlash()
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
    const NVM *flash = configFlashAddr();
    
    // if the flash is valid, load it; otherwise initialize to defaults
    bool nvm_valid = flash->valid();
    if (nvm_valid) 
    {
        // flash is valid - load it into the RAM copy of the structure
        memcpy(&nvm, flash, sizeof(NVM));
    }
    else 
    {
        // flash is invalid - load factory settings into RAM structure
        cfg.setFactoryDefaults();
    }
    
    // tell the caller what happened
    return nvm_valid;
}

// Save the config.  Returns true on success, false on failure.
// 'tFollowup' is the follow-up time in seconds.  If the write is
// successful, we'll turn on the success flag in the status reports
// and leave it on for this interval.  If 'reboot' is true, we'll
// also schedule a reboot at the end of the followup interval.
bool saveConfigToFlash(int tFollowup, bool reboot)
{
    // get the config block location in the flash memory
    uint32_t addr = uint32_t(configFlashAddr());

    // save the data
    bool ok = nvm.save(iap, addr);

    // if the save succeeded, do post-save work
    if (ok)
    {
        // success - report the successful save in the status flags
        saveConfigSucceededFlag = 0x40;
            
        // start the followup timer
        saveConfigFollowupTime = tFollowup;
        saveConfigFollowupTimer.reset();
        saveConfigFollowupTimer.start();
        
        // if a reboot is pending, flag it
        saveConfigRebootPending = reboot;
    }
    
    // return the success indication
    return ok;
}

// ---------------------------------------------------------------------------
//
// Host-loaded configuration.  The Flash NVM block above is designed to be
// stored from within the firmware; in contrast, the host-loaded config is
// stored by the host, by patching the firwmare binary (.bin) file before
// downloading it to the device.
//
// Ideally, we'd use the host-loaded memory for all configuration updates
// from the host - that is, any time the host wants to update config settings,
// such as via user input in the config tool.  In the past, I wanted to do
// it this way because it seemed to be unreliable to write flash memory via
// the device.  But that turned out to be due to a bug in the mbed Ticker 
// code (of all things!), which we've fixed - since then, flash writing on
// the device has been bulletproof.  Even so, doing host-to-device flash
// writing for config updates would be nice just for the sake of speed, as
// the alternative is that we send the variables one at a time by USB, which
// takes noticeable time when reprogramming the whole config set.  But 
// there's no way to accomplish a single-sector flash write via OpenSDA; you 
// can only rewrite the entire flash memory as a unit.
// 
// We can at least use this approach to do a fast configuration restore
// when downloading new firmware.  In that case, we're rewriting all of
// flash memory anyway, so we might as well include the config data.
//
// The memory here is stored using the same format as the USB "Set Config
// Variable" command.  These messages are 8 bytes long and start with a
// byte value 66, followed by the variable ID, followed by the variable
// value data in a format defined separately for each variable.  To load
// the data, we'll start at the first byte after the signature, and 
// interpret each 8-byte block as a type 66 message.  If the first byte
// of a block is not 66, we'll take it as the end of the data.
//
// We provide a block of storage here big enough for 1,024 variables.
// The header consists of a 30-byte signature followed by two bytes giving
// the available space in the area, in this case 8192 == 0x0200.  The
// length is little-endian.  Note that the linker will implicitly zero
// the rest of the block, so if the host doesn't populate it, we'll see
// that it's empty by virtue of not containing the required '66' byte
// prefix for the first 8-byte variable block.
static const uint8_t hostLoadedConfig[8192+32]
    __attribute__ ((aligned(SECTOR_SIZE))) =
    "///Pinscape.HostLoadedConfig//\0\040";   // 30 byte signature + 2 byte length

// Get a pointer to the first byte of the configuration data
const uint8_t *getHostLoadedConfigData()
{
    // the first configuration variable byte immediately follows the
    // 32-byte signature header
    return hostLoadedConfig + 32;
};

// forward reference to config var store function
void configVarSet(const uint8_t *);

// Load the host-loaded configuration data into the active (RAM)
// configuration object.
void loadHostLoadedConfig()
{
    // Start at the first configuration variable.  Each variable
    // block is in the format of a Set Config Variable command in
    // the USB protocol, so each block starts with a byte value of
    // 66 and is 8 bytes long.  Continue as long as we find valid
    // variable blocks, or reach end end of the block.
    const uint8_t *start = getHostLoadedConfigData();
    const uint8_t *end = hostLoadedConfig + sizeof(hostLoadedConfig);
    for (const uint8_t *p = getHostLoadedConfigData() ; start < end && *p == 66 ; p += 8)
    {
        // load this variable
        configVarSet(p);
    }
}

// ---------------------------------------------------------------------------
//
// Pixel dump mode - the host requested a dump of image sensor pixels
// (helpful for installing and setting up the sensor and light source)
//
bool reportPlungerStat = false;
uint8_t reportPlungerStatFlags; // plunger pixel report flag bits (see ccdSensor.h)
uint8_t reportPlungerStatTime;  // extra exposure time for plunger pixel report
uint8_t tReportPlungerStat;     // timestamp of most recent plunger status request


// ---------------------------------------------------------------------------
//
// Night mode setting updates
//

// Turn night mode on or off
static void setNightMode(bool on)
{
    // Set the new night mode flag in the noisy output class.  Note
    // that we use the status report bit flag value 0x02 when on, so
    // that we can just '|' this into the overall status bits.
    nightMode = on ? 0x02 : 0x00;
    
    // update the special output pin that shows the night mode state
    int port = int(cfg.nightMode.port) - 1;
    if (port >= 0 && port < numOutputs)
        lwPin[port]->set(nightMode ? 255 : 0);
        
    // Reset all outputs at their current value, so that the underlying
    // physical outputs get turned on or off as appropriate for the night
    // mode change.
    for (int i = 0 ; i < numOutputs ; ++i)
        lwPin[i]->set(outLevel[i]);
        
    // update 74HC595 outputs
    if (hc595 != 0)
        hc595->update();
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
    // create the new sensor object according to the type
    switch (cfg.plunger.sensorType)
    {
    case PlungerType_TSL1410R:
        // TSL1410R, shadow edge detector
        // pins are: SI, CLOCK, AO
        plungerSensor = new PlungerSensorTSL1410R(
            wirePinName(cfg.plunger.sensorPin[0]), 
            wirePinName(cfg.plunger.sensorPin[1]),
            wirePinName(cfg.plunger.sensorPin[2]));
        break;
        
    case PlungerType_TSL1412S:
        // TSL1412S, shadow edge detector
        // pins are: SI, CLOCK, AO
        plungerSensor = new PlungerSensorTSL1412R(
            wirePinName(cfg.plunger.sensorPin[0]),
            wirePinName(cfg.plunger.sensorPin[1]), 
            wirePinName(cfg.plunger.sensorPin[2]));
        break;
    
    case PlungerType_Pot:
        // Potentiometer (or any other sensor with a linear analog voltage
        // reading as the proxy for the position)
        // pins are: AO (analog in)
        plungerSensor = new PlungerSensorPot(
            wirePinName(cfg.plunger.sensorPin[0]));
        break;
        
    case PlungerType_OptQuad:
        // Optical quadrature sensor, AEDR8300-K or similar.  The -K is
        // designed for a 75 LPI scale, which translates to 300 pulses/inch.
        // Pins are: CHA, CHB (quadrature pulse inputs).
        plungerSensor = new PlungerSensorQuad(
            300,
            wirePinName(cfg.plunger.sensorPin[0]),
            wirePinName(cfg.plunger.sensorPin[1]));
        break;
    
    case PlungerType_TSL1401CL:
        // TSL1401CL, absolute position encoder with bar code scale
        // pins are: SI, CLOCK, AO
        plungerSensor = new PlungerSensorTSL1401CL(
            wirePinName(cfg.plunger.sensorPin[0]), 
            wirePinName(cfg.plunger.sensorPin[1]),
            wirePinName(cfg.plunger.sensorPin[2]));
        break;
        
    case PlungerType_VL6180X:
        // VL6180X time-of-flight IR distance sensor
        // pins are: SDL, SCL, GPIO0/CE
        plungerSensor = new PlungerSensorVL6180X(
            wirePinName(cfg.plunger.sensorPin[0]),
            wirePinName(cfg.plunger.sensorPin[1]),
            wirePinName(cfg.plunger.sensorPin[2]));
        break;
        
    case PlungerType_AEAT6012:
        // Broadcom AEAT-6012-A06 magnetic rotary encoder
        // pins are: CS (chip select, dig out), CLK (dig out), DO (data, dig in)
        plungerSensor = new PlungerSensorAEAT601X<12>(
            wirePinName(cfg.plunger.sensorPin[0]),
            wirePinName(cfg.plunger.sensorPin[1]),
            wirePinName(cfg.plunger.sensorPin[2]));
        break;
        
    case PlungerType_TCD1103:
        // Toshiba TCD1103GFG linear CCD, optical edge detection, with
        // inverted logic gates.
        //
        // Pins are: fM (master clock, PWM), OS (sample data, analog in), 
        // ICG (integration clear gate, dig out), SH (shift gate, dig out)
        plungerSensor = new PlungerSensorTCD1103<true>(
            wirePinName(cfg.plunger.sensorPin[0]),
            wirePinName(cfg.plunger.sensorPin[1]),
            wirePinName(cfg.plunger.sensorPin[2]),
            wirePinName(cfg.plunger.sensorPin[3]));
        break;
        
    case PlungerType_None:
    default:
        plungerSensor = new PlungerSensorNull();
        break;
    }

    // initialize the plunger from the saved configuration
    plungerSensor->restoreCalibration(cfg);
    
    // initialize the config variables affecting the plunger
    plungerSensor->onConfigChange(19, cfg);
    plungerSensor->onConfigChange(20, cfg);
}

// Global plunger calibration mode flag
bool plungerCalMode;

// Plunger reader
//
// This class encapsulates our plunger data processing.  At the simplest
// level, we read the position from the sensor, adjust it for the
// calibration settings, and report the calibrated position to the host.
//
// In addition, we constantly monitor the data for "firing" motions.
// A firing motion is when the user pulls back the plunger and releases
// it, allowing it to shoot forward under the force of the main spring.
// When we detect that this is happening, we briefly stop reporting the
// real physical position that we're reading from the sensor, and instead
// report a synthetic series of positions that depicts an idealized 
// firing motion.
//
// The point of the synthetic reports is to correct for distortions
// created by the joystick interface conventions used by VP and other
// PC pinball emulators.  The convention they use is simply to have the
// plunger device report the instantaneous position of the real plunger.
// The PC software polls this reported position periodically, and moves 
// the on-screen virtual plunger in sync with the real plunger.  This
// works fine for human-scale motion when the user is manually moving
// the plunger.  But it doesn't work for the high speed motion of a 
// release.  The plunger simply moves too fast.  VP polls in about 10ms
// intervals; the plunger takes about 50ms to travel from fully
// retracted to the park position when released.  The low sampling
// rate relative to the rate of change of the sampled data creates
// a classic digital aliasing effect.  
//
// The synthetic reporting scheme compensates for the interface
// distortions by essentially changing to a coarse enough timescale
// that VP can reliably interpret the readings.  Conceptually, there
// are three steps involved in doing this.  First, we analyze the
// actual sensor data to detect and characterize the release motion.
// Second, once we think we have a release in progress, we fit the 
// data to a mathematical model of the release.  The model we use is 
// dead simple: we consider the release to have one parameter, namely
// the retraction distance at the moment the user lets go.  This is an 
// excellent proxy in the real physical system for the final speed 
// when the plunger hits the ball, and it also happens to match how 
// VP models it internally.  Third, we construct synthetic reports
// that will make VP's internal state match our model.  This is also
// pretty simple: we just need to send VP the maximum retraction
// distance for long enough to be sure that it polls it at least
// once, and then send it the park position for long enough to 
// ensure that VP will complete the same firing motion.  The 
// immediate jump from the maximum point to the zero point will
// cause VP to move its simulation model plunger forward from the
// starting point at its natural spring acceleration rate, which 
// is exactly what the real plunger just did.
//
class PlungerReader
{
public:
    PlungerReader()
    {
        // not in a firing event yet
        firing = 0;
    }
    
    // Collect a reading from the plunger sensor.  The main loop calls
    // this frequently to read the current raw position data from the
    // sensor.  We analyze the raw data to produce the calibrated
    // position that we report to the PC via the joystick interface.
    void read()
    {
        // if the sensor is busy, skip the reading on this round
        if (!plungerSensor->ready())
            return;
        
        // Read a sample from the sensor
        PlungerReading r;
        if (plungerSensor->read(r))
        {
            // check for calibration mode
            if (plungerCalMode)
            {
                // Calibration mode.  Adjust the calibration bounds to fit
                // the value.  If this value is beyond the current min or max,
                // expand the envelope to include this new value.
                if (r.pos > cfg.plunger.cal.max)
                    cfg.plunger.cal.max = r.pos;
                if (r.pos < cfg.plunger.cal.min)
                    cfg.plunger.cal.min = r.pos;
                    
                // update our cached calibration data
                onUpdateCal();

                // If we're in calibration state 0, we're waiting for the
                // plunger to come to rest at the park position so that we
                // can take a sample of the park position.  Check to see if
                // we've been at rest for a minimum interval.
                if (calState == 0)
                {
                    if (abs(r.pos - calZeroStart.pos) < 65535/3/50)
                    {
                        // we're close enough - make sure we've been here long enough
                        if (uint32_t(r.t - calZeroStart.t) > 100000UL)
                        {
                            // we've been at rest long enough - count it
                            calZeroPosSum += r.pos;
                            calZeroPosN += 1;
                            
                            // update the zero position from the new average
                            cfg.plunger.cal.zero = uint16_t(calZeroPosSum / calZeroPosN);
                            onUpdateCal();
                            
                            // switch to calibration state 1 - at rest
                            calState = 1;
                        }
                    }
                    else
                    {
                        // we're not close to the last position - start again here
                        calZeroStart = r;
                    }
                }
                
                // Rescale to the joystick range, and adjust for the current
                // park position, but don't calibrate.  We don't know the maximum
                // point yet, so we can't calibrate the range.
                r.pos = int(
                    (long(r.pos - cfg.plunger.cal.zero) * JOYMAX)
                    / (65535 - cfg.plunger.cal.zero));
            }
            else
            {
                // Not in calibration mode.  Apply the existing calibration and 
                // rescale to the joystick range.
                r.pos = applyCal(r.pos);
                    
                // limit the result to the valid joystick range
                if (r.pos > JOYMAX)
                    r.pos = JOYMAX;
                else if (r.pos < -JOYMAX)
                    r.pos = -JOYMAX;
            }

            // Look for a firing event - the user releasing the plunger and
            // allowing it to shoot forward at full speed.  Wait at least 5ms
            // between samples for this, to help distinguish random motion 
            // from the rapid motion of a firing event.  
            //
            // There's a trade-off in the choice of minimum sampling interval.
            // The longer we wait, the more certain we can be of the trend.
            // But if we wait too long, the user will perceive a delay.  We
            // also want to sample frequently enough to see the release motion
            // at intermediate steps along the way, so the sampling has to be
            // considerably faster than the whole travel time, which is about
            // 25-50ms.
            if (uint32_t(r.t - prv.t) < 5000UL)
                return;
                
            // assume that we'll report this reading as-is
            z = r.pos;
                
            // Firing event detection.
            //
            // A "firing event" is when the player releases the plunger from
            // a retracted position, allowing it to shoot forward under the
            // spring tension.
            //
            // We monitor the plunger motion for these events, and when they
            // occur, we report an "idealized" version of the motion to the
            // PC.  The idealized version consists of a series of readings
            // frozen at the fully retracted position for the whole duration 
            // of the forward travel, followed by a series of readings at the
            // fully forward position for long enough for the plunger to come
            // mostly to rest.  The series of frozen readings aren't meant to
            // be perceptible to the player - we try to keep them short enough
            // that they're not apparent as delay.  Instead, they're for the
            // PC client software's benefit.  PC joystick clients use polling,
            // so they only see an unpredictable subset of the readings we
            // send.  The only way to be sure that the client sees a particular 
            // reading is to hold it for long enough that the client is sure to
            // poll within the hold interval.  In the case of the plunger 
            // firing motion, it's important that the client sees the *ends*
            // of the travel - the fully retracted starting position in
            // particular.  If the PC client only polls for a sample while the
            // plunger is somewhere in the middle of the travel, the PC will
            // think that the firing motion *started* in that middle position,
            // so it won't be able to model the right amount of momentum when
            // the plunger hits the ball.  We try to ensure that the PC sees
            // the right starting point by reporting the starting point for 
            // extra time during the forward motion.  By the same token, we
            // want the PC to know that the plunger has moved all the way
            // forward, rather than mistakenly thinking that it stopped
            // somewhere in the middle of the travel, so we freeze at the
            // forward position for a short time.
            //
            // To detect a firing event, we look for forward motion that's
            // fast enough to be a firing event.  To determine how fast is
            // fast enough, we use a simple model of the plunger motion where 
            // the acceleration is constant.  This is only an approximation, 
            // as the spring force actually varies with spring's compression, 
            // but it's close enough for our purposes here.
            //
            // Do calculations in fixed-point 2^48 scale with 64-bit ints.
            // acc2 = acceleration/2 for 50ms release time, units of unit
            // distances per microsecond squared, where the unit distance
            // is the overall travel from the starting retracted position
            // to the park position.
            const int32_t acc2 = 112590;  // 2^48 scale
            switch (firing)
            {
            case 0:
                // Not in firing mode.  If we're retracted a bit, and the
                // motion is forward at a fast enough rate to look like a
                // release, enter firing mode.
                if (r.pos > JOYMAX/6)
                {
                    const uint32_t dt = uint32_t(r.t - prv.t);
                    const uint32_t dt2 = dt*dt;  // dt^2
                    if (r.pos < prv.pos - int((prv.pos*acc2*uint64_t(dt2)) >> 48))
                    {
                        // Tentatively enter firing mode.  Use the prior reading
                        // as the starting point, and freeze reports for now.
                        firingMode(1);
                        f0 = prv;
                        z = f0.pos;

                        // if in calibration state 1 (at rest), switch to 
                        // state 2 (not at rest)
                        if (calState == 1)
                            calState = 2;
                    }
                }
                break;
                
            case 1:
                // Tentative firing mode: the plunger was moving forward
                // at last check.  To stay in firing mode, the plunger has
                // to keep moving forward fast enough to look like it's 
                // moving under spring force.  To figure out how fast is
                // fast enough, we use a simple model where the acceleration
                // is constant over the whole travel distance and the total
                // travel time is 50ms.  The acceleration actually varies
                // slightly since it comes from the spring force, which
                // is linear in the displacement; but the plunger spring is
                // fairly compressed even when the plunger is all the way
                // forward, so the difference in tension from one end of
                // the travel to the other is fairly small, so it's not too
                // far off to model it as constant.  And the real travel
                // time obviously isn't a constant, but all we need for 
                // that is an upper bound.  So: we'll figure the time since 
                // we entered firing mode, and figure the distance we should 
                // have traveled to complete the trip within the maximum
                // time allowed.  If we've moved far enough, we'll stay
                // in firing mode; if not, we'll exit firing mode.  And if
                // we cross the finish line while still in firing mode,
                // we'll switch to the next phase of the firing event.
                if (r.pos <= 0)
                {
                    // We crossed the park position.  Switch to the second
                    // phase of the firing event, where we hold the reported
                    // position at the "bounce" position (where the plunger
                    // is all the way forward, compressing the barrel spring).
                    // We'll stick here long enough to ensure that the PC
                    // client (Visual Pinball or whatever) sees the reading
                    // and processes the release motion via the simulated
                    // physics.
                    firingMode(2);
                    
                    // if in calibration mode, and we're in state 2 (moving), 
                    // collect firing statistics for calibration purposes
                    if (plungerCalMode && calState == 2)
                    {
                        // collect a new zero point for the average when we 
                        // come to rest
                        calState = 0;
                        
                        // collect average firing time statistics in millseconds,
                        // if it's in range (20 to 255 ms)
                        const int dt = uint32_t(r.t - f0.t)/1000UL;
                        if (dt >= 15 && dt <= 255)
                        {
                            calRlsTimeSum += dt;
                            calRlsTimeN += 1;
                            cfg.plunger.cal.tRelease = uint8_t(calRlsTimeSum / calRlsTimeN);
                        }
                    }

                    // Figure the "bounce" position as forward of the park
                    // position by 1/6 of the starting retraction distance.
                    // This simulates the momentum of the plunger compressing
                    // the barrel spring on the rebound.  The barrel spring
                    // can compress by about 1/6 of the maximum retraction 
                    // distance, so we'll simply treat its compression as
                    // proportional to the retraction.  (It might be more
                    // realistic to use a slightly higher value here, maybe
                    // 1/4 or 1/3 or the retraction distance, capping it at
                    // a maximum of 1/6, because the real plunger probably 
                    // compresses the barrel spring by 100% with less than 
                    // 100% retraction.  But that won't affect the physics
                    // meaningfully, just the animation, and the effect is
                    // small in any case.)
                    z = f0.pos = -f0.pos / 6;
                    
                    // reset the starting time for this phase
                    f0.t = r.t;
                }
                else
                {
                    // check for motion since the start of the firing event
                    const uint32_t dt = uint32_t(r.t - f0.t);
                    const uint32_t dt2 = dt*dt;  // dt^2
                    if (dt < 50000 
                        && r.pos < f0.pos - int((f0.pos*acc2*uint64_t(dt2)) >> 48))
                    {
                        // It's moving fast enough to still be in a release
                        // motion.  Continue reporting the start position, and
                        // stay in the first release phase.
                        z = f0.pos;
                    }
                    else
                    {
                        // It's not moving fast enough to be a release
                        // motion.  Return to the default state.
                        firingMode(0);
                        calState = 1;
                    }
                }
                break;
                
            case 2:
                // Firing mode, holding at forward compression position.
                // Hold here for 25ms.
                if (uint32_t(r.t - f0.t) < 25000)
                {
                    // stay here for now
                    z = f0.pos;
                }
                else
                {
                    // advance to the next phase, where we report the park
                    // position until the plunger comes to rest
                    firingMode(3);
                    z = 0;

                    // remember when we started
                    f0.t = r.t;
                }
                break;
                
            case 3:
                // Firing event, holding at park position.  Stay here for
                // a few moments so that the PC client can simulate the
                // full release motion, then return to real readings.
                if (uint32_t(r.t - f0.t) < 250000)
                {
                    // stay here a while longer
                    z = 0;
                }
                else
                {
                    // it's been long enough - return to normal mode
                    firingMode(0);
                }
                break;
            }
            
            // Check for auto-zeroing, if enabled
            if ((cfg.plunger.autoZero.flags & PlungerAutoZeroEnabled) != 0)
            {
                // If we moved since the last reading, reset and restart the 
                // auto-zero timer.  Otherwise, if the timer has reached the 
                // auto-zero timeout, it means we've been motionless for that 
                // long, so auto-zero now.                
                if (r.pos != prv.pos)
                {
                    // movement detected - reset the timer
                    autoZeroTimer.reset();
                    autoZeroTimer.start();
                }
                else if (autoZeroTimer.read_us() > cfg.plunger.autoZero.t * 1000000UL)
                {
                    // auto-zero now
                    plungerSensor->autoZero();
                    
                    // stop the timer so that we don't keep repeating this
                    // if the plunger stays still for a long time
                    autoZeroTimer.stop();
                    autoZeroTimer.reset();
                }
            }
            
            // this new reading becomes the previous reading for next time
            prv = r;
        }
    }
    
    // Get the current value to report through the joystick interface
    int16_t getPosition()
    {
        // return the last reading
        return z;
    }
        
    // Set calibration mode on or off
    void setCalMode(bool f) 
    {
        // check to see if we're entering calibration mode
        if (f && !plungerCalMode)
        {
            // reset the calibration in the configuration
            cfg.plunger.cal.begin();
            
            // start in state 0 (waiting to settle)
            calState = 0;
            calZeroPosSum = 0;
            calZeroPosN = 0;
            calRlsTimeSum = 0;
            calRlsTimeN = 0;
            
            // tell the plunger we're starting calibration
            plungerSensor->beginCalibration(cfg);
            
            // set the initial zero point to the current position
            PlungerReading r;
            if (plungerSensor->read(r))
            {
                // got a reading - use it as the initial zero point
                cfg.plunger.cal.zero = r.pos;
                onUpdateCal();
                
                // use it as the starting point for the settling watch
                calZeroStart = r;
            }
            else
            {
                // no reading available - use the default 1/6 position
                cfg.plunger.cal.zero = 0xffff/6;
                onUpdateCal();
                
                // we don't have a starting point for the setting watch
                calZeroStart.pos = -65535;
                calZeroStart.t = 0;
            }
        }
        else if (!f && plungerCalMode)
        {
            // Leaving calibration mode.  Make sure the max is past the
            // zero point - if it's not, we'd have a zero or negative
            // denominator for the scaling calculation, which would be
            // physically meaningless.
            if (cfg.plunger.cal.max <= cfg.plunger.cal.zero)
            {
                // bad settings - reset to defaults
                cfg.plunger.cal.max = 0xffff;
                cfg.plunger.cal.zero = 0xffff/6;
            }
            
            // finalize the configuration in the plunger object
            plungerSensor->endCalibration(cfg);

            // update our internal cached information for the new calibration
            onUpdateCal();
        }
            
        // remember the new mode
        plungerCalMode = f; 
    }
    
    // Cached inverse of the calibration range.  This is for calculating
    // the calibrated plunger position given a raw sensor reading.  The
    // cached inverse is calculated as
    //
    //    64K * JOYMAX / (cfg.plunger.cal.max - cfg.plunger.cal.zero)
    //
    // To convert a raw sensor reading to a calibrated position, calculate
    //
    //    ((reading - cfg.plunger.cal.zero)*invCalRange) >> 16
    //
    // That yields the calibration result without performing a division.
    int invCalRange;
    
    // apply the calibration range to a reading
    inline int applyCal(int reading)
    {
        return ((reading - cfg.plunger.cal.zero)*invCalRange) >> 16;
    }
    
    void onUpdateCal()
    {
        invCalRange = (JOYMAX << 16)/(cfg.plunger.cal.max - cfg.plunger.cal.zero);
    }

    // is a firing event in progress?
    bool isFiring() { return firing == 3; }
    
private:
    // current reported joystick reading
    int z;
    
    // previous reading
    PlungerReading prv;

    // Calibration state.  During calibration mode, we watch for release
    // events, to measure the time it takes to complete the release
    // motion; and we watch for the plunger to come to reset after a
    // release, to gather statistics on the rest position.
    //   0 = waiting to settle
    //   1 = at rest
    //   2 = retracting
    //   3 = possibly releasing
    uint8_t calState;
    
    // Calibration zero point statistics.
    // During calibration mode, we collect data on the rest position (the 
    // zero point) by watching for the plunger to come to rest after each 
    // release.  We average these rest positions to get the calibrated 
    // zero point.  We use the average because the real physical plunger 
    // itself doesn't come to rest at exactly the same spot every time, 
    // largely due to friction in the mechanism.  To calculate the average,
    // we keep a sum of the readings and a count of samples.
    PlungerReading calZeroStart;
    long calZeroPosSum;
    int calZeroPosN;
    
    // Calibration release time statistics.
    // During calibration, we collect an average for the release time.
    long calRlsTimeSum;
    int calRlsTimeN;

    // Auto-zeroing timer
    Timer autoZeroTimer;
    
    // set a firing mode
    inline void firingMode(int m) 
    {
        firing = m;
    }
    
    // Firing event state.
    //
    //   0 - Default state: not in firing event.  We report the true
    //       instantaneous plunger position to the joystick interface.
    //
    //   1 - Moving forward at release speed
    //
    //   2 - Firing - reporting the bounce position
    //
    //   3 - Firing - reporting the park position
    //
    int firing;
    
    // Starting position for current firing mode phase
    PlungerReading f0;
};

// plunger reader singleton
PlungerReader plungerReader;

// ---------------------------------------------------------------------------
//
// Handle the ZB Launch Ball feature.
//
// The ZB Launch Ball feature, if enabled, lets the mechanical plunger
// serve as a substitute for a physical Launch Ball button.  When a table
// is loaded in VP, and the table has the ZB Launch Ball LedWiz port
// turned on, we'll disable mechanical plunger reports through the
// joystick interface and instead use the plunger only to simulate the
// Launch Ball button.  When the mode is active, pulling back and 
// releasing the plunger causes a brief simulated press of the Launch
// button, and pushing the plunger forward of the rest position presses
// the Launch button as long as the plunger is pressed forward.
//
// This feature has two configuration components:
//
//   - An LedWiz port number.  This port is a "virtual" port that doesn't
//     have to be attached to any actual output.  DOF uses it to signal 
//     that the current table uses a Launch button instead of a plunger.
//     DOF simply turns the port on when such a table is loaded and turns
//     it off at all other times.  We use it to enable and disable the
//     plunger/launch button connection.
//
//   - A joystick button ID.  We simulate pressing this button when the
//     launch feature is activated via the LedWiz port and the plunger is
//     either pulled back and releasd, or pushed forward past the rest
//     position.
//
class ZBLaunchBall
{
public:
    ZBLaunchBall()
    {
        // start in the default state
        lbState = 0;
        btnState = false;
    }

    // Update state.  This checks the current plunger position and
    // the timers to see if the plunger is in a position that simulates
    // a Launch Ball button press via the ZB Launch Ball feature.
    // Updates the simulated button vector according to the current
    // launch ball state.  The main loop calls this before each 
    // joystick update to figure the new simulated button state.
    void update()
    {
        // If the ZB Launch Ball led wiz output is ON, check for a 
        // plunger firing event
        if (zbLaunchOn)
        {                
            // note the new position
            int znew = plungerReader.getPosition();
            
            // figure the push threshold from the configuration data
            const int pushThreshold = int(-JOYMAX/3.0 * cfg.plunger.zbLaunchBall.pushDistance/1000.0);

            // check the state
            switch (lbState)
            {
            case 0:
                // Default state.  If a launch event has been detected on
                // the plunger, activate a timed pulse and switch to state 1.
                // If the plunger is pushed forward of the threshold, push
                // the button.
                if (plungerReader.isFiring())
                {
                    // firing event - start a timed Launch button pulse
                    lbTimer.reset();
                    lbTimer.start();
                    setButton(true);
                    
                    // switch to state 1
                    lbState = 1;
                }
                else if (znew <= pushThreshold)
                {
                    // pushed forward without a firing event - hold the
                    // button as long as we're pushed forward
                    setButton(true);
                }
                else
                {
                    // not pushed forward - turn off the Launch button
                    setButton(false);
                }
                break;
                
            case 1:
                // State 1: Timed Launch button pulse in progress after a
                // firing event.  Wait for the timer to expire.
                if (lbTimer.read_us() > 200000UL)
                {
                    // timer expired - turn off the button
                    setButton(false);
                    
                    // switch to state 2
                    lbState = 2;
                }
                break;
                
            case 2:
                // State 2: Timed Launch button pulse done.  Wait for the
                // plunger launch event to end.
                if (!plungerReader.isFiring())
                {
                    // firing event done - return to default state
                    lbState = 0;
                }
                break;
            }
        }
        else
        {
            // ZB Launch Ball disabled - turn off the button if it was on
            setButton(false);
                
            // return to the default state
            lbState = 0;
        }
    }
    
    // Set the button state
    void setButton(bool on)
    {
        if (btnState != on)
        {
            // remember the new state
            btnState = on;
            
            // update the virtual button state
            buttonState[zblButtonIndex].virtPress(on);
        }
    }
    
private:
    // Simulated Launch Ball button state.  If a "ZB Launch Ball" port is
    // defined for our LedWiz port mapping, any time that port is turned ON,
    // we'll simulate pushing the Launch Ball button if the player pulls 
    // back and releases the plunger, or simply pushes on the plunger from
    // the rest position.  This allows the plunger to be used in lieu of a
    // physical Launch Ball button for tables that don't have plungers.
    //
    // States:
    //   0 = default
    //   1 = firing (firing event has activated a Launch button pulse)
    //   2 = firing done (Launch button pulse ended, waiting for plunger
    //       firing event to end)
    uint8_t lbState;
    
    // button state
    bool btnState;
    
    // Time since last lbState transition.  Some of the states are time-
    // sensitive.  In the "uncocked" state, we'll return to state 0 if
    // we remain in this state for more than a few milliseconds, since
    // it indicates that the plunger is being slowly returned to rest
    // rather than released.  In the "launching" state, we need to release 
    // the Launch Ball button after a moment, and we need to wait for 
    // the plunger to come to rest before returning to state 0.
    Timer lbTimer;
};

// ---------------------------------------------------------------------------
//
// Reboot - resets the microcontroller
//
void reboot(USBJoystick &js, bool disconnect = true, long pause_us = 2000000L)
{
    // disconnect from USB
    if (disconnect)
        js.disconnect();
    
    // wait a few seconds to make sure the host notices the disconnect
    wait_us(pause_us);
    
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
    switch (cfg.accel.orientation)
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
#define v_byte(var, ofs)    cfg.var = data[ofs]
#define v_byte_wo(var, ofs) cfg.var = data[ofs]
#define v_ui16(var, ofs)    cfg.var = wireUI16(data+(ofs))
#define v_ui32(var, ofs)    cfg.var = wireUI32(data+(ofs))
#define v_pin(var, ofs)     cfg.var = wirePinName(data[ofs])
#define v_byte_ro(val, ofs) // ignore read-only variables on SET
#define v_ui32_ro(val, ofs) // ignore read-only variables on SET
#define VAR_MODE_SET 1      // we're in SET mode
#define v_func configVarSet(const uint8_t *data)
#include "cfgVarMsgMap.h"

// redefine everything for the SET messages
#undef if_msg_valid
#undef v_byte
#undef v_ui16
#undef v_ui32
#undef v_pin
#undef v_byte_ro
#undef v_byte_wo
#undef v_ui32_ro
#undef VAR_MODE_SET
#undef v_func

// Handle GET messages - read variable values and return in USB message data
#define if_msg_valid(test)
#define v_byte(var, ofs)    data[ofs] = cfg.var
#define v_ui16(var, ofs)    ui16Wire(data+(ofs), cfg.var)
#define v_ui32(var, ofs)    ui32Wire(data+(ofs), cfg.var)
#define v_pin(var, ofs)     pinNameWire(data+(ofs), cfg.var)
#define v_byte_ro(val, ofs) data[ofs] = (val)
#define v_ui32_ro(val, ofs) ui32Wire(data+(ofs), val);
#define VAR_MODE_SET 0      // we're in GET mode
#define v_byte_wo(var, ofs) // ignore write-only variables in GET mode
#define v_func  configVarGet(uint8_t *data)
#include "cfgVarMsgMap.h"


// ---------------------------------------------------------------------------
//
// Timer for timestamping input requests
//
Timer requestTimestamper;

// ---------------------------------------------------------------------------
//
// Handle an input report from the USB host.  Input reports use our extended
// LedWiz protocol.
//
void handleInputMsg(LedWizMsg &lwm, USBJoystick &js, Accel &accel)
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
    //   0-48     -> PBA
    //   64       -> SBA 
    //   65       -> private control message; second byte specifies subtype
    //   129-132  -> PBA
    //   200-228  -> extended bank brightness set for outputs N to N+6, where
    //               N is (first byte - 200)*7
    //   other    -> reserved for future use
    //
    uint8_t *data = lwm.data;
    if (data[0] == 64)
    {
        // 64 = SBA (original LedWiz command to set on/off switches for ports 1-32)
        //printf("SBA %02x %02x %02x %02x, speed %02x\r\n",
        //       data[1], data[2], data[3], data[4], data[5]);
        sba_sbx(0, data);

        // SBA resets the PBA port group counter
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
    
                // we'll need a reboot if the LedWiz unit number is changing
                bool reboot = (newUnitNo != cfg.psUnitNo);
                
                // set the configuration parameters from the message
                cfg.psUnitNo = newUnitNo;
                cfg.plunger.enabled = data[3] & 0x01;
                
                // set the flag to do the save
                saveConfigToFlash(0, reboot);
            }
            break;
            
        case 2:
            // 2 = Calibrate plunger
            // (No parameters)
            
            // enter calibration mode
            calBtnState = 3;
            plungerReader.setCalMode(true);
            calBtnTimer.reset();
            break;
            
        case 3:
            // 3 = plunger sensor status report
            //     data[2] = flag bits
            //     data[3] = extra exposure time, 100us (.1ms) increments
            reportPlungerStat = true;
            reportPlungerStatFlags = data[2];
            reportPlungerStatTime = data[3];
            
            // set the extra integration time in the sensor
            plungerSensor->setExtraIntegrationTime(reportPlungerStatTime * 100);
            
            // make a note of the request timestamp
            tReportPlungerStat = requestTimestamper.read_us();
            
            // show purple until we finish sending the report
            diagLED(1, 0, 1);
            break;
            
        case 4:
            // 4 = hardware configuration query
            // (No parameters)
            js.reportConfig(
                numOutputs, 
                cfg.psUnitNo - 1,   // report 0-15 range for unit number (we store 1-16 internally)
                cfg.plunger.cal.zero, cfg.plunger.cal.max, cfg.plunger.cal.tRelease,
                nvm.valid(),        // a config is loaded if the config memory block is valid
                true,               // we support sbx/pbx extensions
                true,               // we support the new accelerometer settings
                true,               // we support the "flash write ok" status bit in joystick reports
                true,               // we support the configurable joystick report timing features
                true,               // chime logic is supported
                mallocBytesFree()); // remaining memory size
            break;
            
        case 5:
            // 5 = all outputs off, reset to LedWiz defaults
            allOutputsOff();
            break;
            
        case 6:
            // 6 = Save configuration to flash.  Optionally reboot after the 
            // delay time in seconds given in data[2].
            //
            // data[2] = delay time in seconds
            // data[3] = flags:
            //           0x01 -> do not reboot
            saveConfigToFlash(data[2], !(data[3] & 0x01));
            break;
            
        case 7:
            // 7 = Device ID report
            //     data[2] = ID index: 1=CPU ID, 2=OpenSDA TUID
            js.reportID(data[2]);
            break;
            
        case 8:
            // 8 = Engage/disengage night mode.
            //     data[2] = 1 to engage, 0 to disengage
            setNightMode(data[2]);
            break;
            
        case 9:
            // 9 = Config variable query.
            //     data[2] = config var ID
            //     data[3] = array index (for array vars: button assignments, output ports)
            {
                // set up the reply buffer with the variable ID data, and zero out
                // the rest of the buffer
                uint8_t reply[8];
                reply[1] = data[2];
                reply[2] = data[3];
                memset(reply+3, 0, sizeof(reply)-3);
                
                // query the value
                configVarGet(reply);
                
                // send the reply
                js.reportConfigVar(reply + 1);
            }
            break;
            
        case 10:
            // 10 = Build ID query.
            js.reportBuildInfo(getBuildID());
            break;
            
        case 11:
            // 11 = TV ON relay control.
            //      data[2] = operation:
            //         0 = turn relay off
            //         1 = turn relay on
            //         2 = pulse relay (as though the power-on timer fired)
            TVRelay(data[2]);
            break;
            
        case 12:
            // 12 = Learn IR code.  This enters IR learning mode.  While
            // in learning mode, we report raw IR signals and the first IR
            // command decoded through the special IR report format.  IR
            // learning mode automatically ends after a timeout expires if
            // no command can be decoded within the time limit.
            
            // enter IR learning mode
            IRLearningMode = 1;
            
            // cancel any regular IR input in progress
            IRCommandIn = 0;
            
            // reset and start the learning mode timeout timer
            IRTimer.reset();
            break;
            
        case 13:
            // 13 = Send button status report
            reportButtonStatus(js);
            break;
            
        case 14:
            // 14 = manually center the accelerometer
            accel.manualCenterRequest();
            break;
            
        case 15:
            // 15 = set up ad hoc IR command, part 1.  Mark the command
            // as not ready, and save the partial data from the message.
            IRAdHocCmd.ready = 0;
            IRAdHocCmd.protocol = data[2];
            IRAdHocCmd.dittos = (data[3] & IRFlagDittos) != 0;
            IRAdHocCmd.code = wireUI32(&data[4]);
            break;
            
        case 16:
            // 16 = send ad hoc IR command, part 2.  Fill in the rest
            // of the data from the message and mark the command as
            // ready.  The IR polling routine will send this as soon
            // as the IR transmitter is free.
            IRAdHocCmd.code |= (uint64_t(wireUI32(&data[2])) << 32);
            IRAdHocCmd.ready = 1;
            break;
            
        case 17:
            // 17 = send pre-programmed IR command.  This works just like
            // sending an ad hoc command above, but we get the command data
            // from an IR slot in the config rather than from the client.
            // First make sure we have a valid slot number.
            if (data[2] >= 1 && data[2] <= MAX_IR_CODES)
            {
                // get the IR command slot in the config
                IRCommandCfg &cmd = cfg.IRCommand[data[2] - 1];
                
                // copy the IR command data from the config
                IRAdHocCmd.protocol = cmd.protocol;
                IRAdHocCmd.dittos = (cmd.flags & IRFlagDittos) != 0;
                IRAdHocCmd.code = (uint64_t(cmd.code.hi) << 32) | cmd.code.lo;
                
                // mark the command as ready - this will trigger the polling
                // routine to send the command as soon as the transmitter
                // is free
                IRAdHocCmd.ready = 1;
            }
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
        
        // notify the plunger, so that it can update relevant variables
        // dynamically
        plungerSensor->onConfigChange(data[1], cfg);
    }
    else if (data[0] == 67)
    {
        // SBX - extended SBA message.  This is the same as SBA, except
        // that the 7th byte selects a group of 32 ports, to allow access
        // to ports beyond the first 32.
        sba_sbx(data[6], data);
    }
    else if (data[0] == 68)
    {
        // PBX - extended PBA message.  This is similar to PBA, but
        // allows access to more than the first 32 ports by encoding
        // a port group byte that selects a block of 8 ports.
        
        // get the port group - the first port is 8*group
        int portGroup = data[1];
        
        // unpack the brightness values
        uint32_t tmp1 = data[2] | (data[3]<<8) | (data[4]<<16);
        uint32_t tmp2 = data[5] | (data[6]<<8) | (data[7]<<16);
        uint8_t bri[8] = {
            tmp1 & 0x3F, (tmp1>>6) & 0x3F, (tmp1>>12) & 0x3F, (tmp1>>18) & 0x3F,
            tmp2 & 0x3F, (tmp2>>6) & 0x3F, (tmp2>>12) & 0x3F, (tmp2>>18) & 0x3F
        };
        
        // map the flash levels: 60->129, 61->130, 62->131, 63->132
        for (int i = 0 ; i < 8 ; ++i)
        {
            if (bri[i] >= 60)
                bri[i] += 129-60;
        }
        
        // Carry out the PBA
        pba_pbx(portGroup*8, bri);
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
        
        // figure the block of 7 ports covered in the message
        int i0 = (data[0] - 200)*7;
        int i1 = i0 + 7 < numOutputs ? i0 + 7 : numOutputs; 
        
        // update each port
        for (int i = i0 ; i < i1 ; ++i)
        {
            // set the brightness level for the output
            uint8_t b = data[i-i0+1];
            outLevel[i] = b;
            
            // set the port's LedWiz state to the nearest equivalent, so
            // that it maintains its current setting if we switch back to
            // LedWiz mode on a future update
            if (b != 0)
            {
                // Non-zero brightness - set the SBA switch on, and set the
                // PBA brightness to the DOF brightness rescaled to the 1..48
                // LedWiz range.  If the port is subsequently addressed by an
                // LedWiz command, this will carry the current DOF setting
                // forward unchanged.
                wizOn[i] = 1;
                wizVal[i] = dof_to_lw[b];
            }
            else
            {
                // Zero brightness.  Set the SBA switch off, and leave the
                // PBA brightness the same as it was.
                wizOn[i] = 0;
            }
            
            // set the output
            lwPin[i]->set(b);
        }
        
        // update 74HC595 outputs, if attached
        if (hc595 != 0)
            hc595->update();
    }
    else 
    {
        // Everything else is an LedWiz PBA message.  This is a full 
        // "profile" dump from the host for one bank of 8 outputs.  Each
        // byte sets one output in the current bank.  The current bank
        // is implied; the bank starts at 0 and is reset to 0 by any SBA
        // message, and is incremented to the next bank by each PBA.  Our
        // variable pbaIdx keeps track of the current bank.  There's no 
        // direct way for the host to select the bank; it just has to count
        // on us staying in sync.  In practice, clients always send the
        // full set of 4 PBA messages in a row to set all 32 outputs.
        //
        // Note that a PBA implicitly overrides our extended profile
        // messages (message prefix 200-219), because this sets the
        // wizVal[] entry for each output, and that takes precedence
        // over the extended protocol settings when we're in LedWiz
        // protocol mode.
        //
        //printf("LWZ-PBA[%d] %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
        //       pbaIdx, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

        // carry out the PBA
        pba_pbx(pbaIdx, data);
        
        // update the PBX index state for the next message
        pbaIdx = (pbaIdx + 8) % 32;
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
    // say hello to the debug console, in case it's connected
    printf("\r\nPinscape Controller starting\r\n");
    
    // Set the default PWM period to 0.5ms = 2 kHz.  This will be used 
    // for PWM channels on PWM units whose periods aren't changed 
    // explicitly, so it'll apply to LW outputs assigned to GPIO pins.
    // The KL25Z only allows the period to be set at the TPM unit
    // level, not per channel, so all channels on a given unit will
    // necessarily use the same frequency.  We (currently) have two
    // subsystems that need specific PWM frequencies: TLC5940NT (which
    // uses PWM to generate the grayscale clock signal) and IR remote
    // (which uses PWM to generate the IR carrier signal).  Since
    // those require specific PWM frequencies, it's important to assign
    // those to separate TPM units if both are in use simultaneously;
    // the Config Tool includes checks to ensure that will happen when
    // setting a config interactively.  In addition, for the greatest
    // flexibility, we take care NOT to assign explicit PWM frequencies
    // to pins that don't require special frequences.  That way, if a
    // pin that doesn't need anything special happens to be sharing a
    // TPM unit with a pin that does require a specific frequency, the
    // two will co-exist peacefully on the TPM.
    //
    // We set this default first, before we create any PWM GPIOs, so
    // that it will apply to all channels by default but won't override
    // any channels that need specific frequences.  Currently, the only
    // frequency-agnostic PWM user is the LW outputs, so we can choose
    // the default to be suitable for those.  This is chosen to minimize
    // flicker on attached LEDs.
    NewPwmUnit::defaultPeriod = 0.0005f;
        
    // clear the I2C connection
    clear_i2c();
    
    // Elevate GPIO pin interrupt priorities, so that they can preempt
    // other interrupts.  This is important for some external peripherals,
    // particularly the quadrature plunger sensors, which can generate
    // high-speed interrupts that need to be serviced quickly to keep
    // proper count of the quadrature position.
    FastInterruptIn::elevatePriority();

    // Load the saved configuration.  There are two sources of the
    // configuration data:
    //
    // - Look for an NVM (flash non-volatile memory) configuration.
    // If this is valid, we'll load it.  The NVM is config data that can 
    // be updated dynamically by the host via USB commands and then stored 
    // in the flash by the firmware itself.  If this exists, it supersedes 
    // any of the other settings stores.  The Windows config tool uses this
    // to store user settings updates.
    //
    // - If there's no NVM, we'll load the factory defaults, then we'll
    // load any settings stored in the host-loaded configuration.  The
    // host can patch a set of configuration variable settings into the
    // .bin file when loading new firmware, in the host-loaded config
    // area that we reserve for this purpose.  This allows the host to
    // restore a configuration at the same time it installs firmware,
    // without a separate download of the config data.
    //
    // The NVM supersedes the host-loaded config, since it can be updated
    // between firmware updated and is thus presumably more recent if it's
    // present.  (Note that the NVM and host-loaded config are both in    
    // flash, so in principle we could just have a single NVM store that
    // the host patches.  The only reason we don't is that the NVM store
    // is an image of our in-memory config structure, which is a native C
    // struct, and we don't want the host to have to know the details of 
    // its byte layout, for obvious reasons.  The host-loaded config, in
    // contrast, uses the wire protocol format, which has a well-defined
    // byte layout that's independent of the firmware version or the
    // details of how the C compiler arranges the struct memory.)
    if (!loadConfigFromFlash())
        loadHostLoadedConfig();
    
    // initialize the diagnostic LEDs
    initDiagLEDs(cfg);

    // we're not connected/awake yet
    bool connected = false;
    Timer connectChangeTimer;

    // create the plunger sensor interface
    createPlunger();
    
    // update the plunger reader's cached calibration data
    plungerReader.onUpdateCal();

    // set up the TLC5940 interface, if these chips are present
    init_tlc5940(cfg);

    // initialize the TLC5916 interface, if these chips are present
    init_tlc59116(cfg);
    
    // set up 74HC595 interface, if these chips are present
    init_hc595(cfg);
    
    // Initialize the LedWiz ports.  Note that the ordering here is important:
    // this has to come after we create the TLC5940 and 74HC595 object instances
    // (which we just did above), since we need to access those objects to set
    // up ports assigned to the respective chips.
    initLwOut(cfg);

    // start the TLC5940 refresh cycle clock
    if (tlc5940 != 0)
        tlc5940->start();
        
    // Assume that nothing uses keyboard keys.  We'll check for keyboard
    // usage when initializing the various subsystems that can send keys
    // (buttons, IR).  If we find anything that does, we'll create the
    // USB keyboard interface.
    bool kbKeys = false;

    // set up the IR remote control emitter & receiver, if present
    init_IR(cfg, kbKeys);

    // start the power status time, if applicable
    startPowerStatusTimer(cfg);

    // initialize the button input ports
    initButtons(cfg, kbKeys);
    
    // Create the joystick USB client.  Note that the USB vendor/product ID
    // information comes from the saved configuration.  Also note that we have
    // to wait until after initializing the input buttons (which we just did
    // above) to set up the interface, since the button setup will determine
    // whether or not we need to present a USB keyboard interface in addition
    // to the joystick interface.
    MyUSBJoystick js(cfg.usbVendorID, cfg.usbProductID, USB_VERSION_NO, false, 
        cfg.joystickEnabled, cfg.joystickAxisFormat, kbKeys);
        
    // start the request timestamp timer
    requestTimestamper.start();
        
    // Wait for the USB connection to start up.  Show a distinctive diagnostic
    // flash pattern while waiting.
    Timer connTimeoutTimer, connFlashTimer;
    connTimeoutTimer.start();
    connFlashTimer.start();
    while (!js.configured())
    {
        // show one short yellow flash at 2-second intervals
        if (connFlashTimer.read_us() > 2000000)
        {
            // short yellow flash
            diagLED(1, 1, 0);
            wait_us(50000);
            diagLED(0, 0, 0);
            
            // reset the flash timer
            connFlashTimer.reset();
        }

        // If we've been disconnected for more than the reboot timeout,
        // reboot.  Some PCs won't reconnect if we were left plugged in
        // during a power cycle on the PC, but fortunately a reboot on
        // the KL25Z will make the host notice us and trigger a reconnect.
        // Don't do this if we're in a non-recoverable PSU2 power state.
        if (cfg.disconnectRebootTimeout != 0 
            && connTimeoutTimer.read() > cfg.disconnectRebootTimeout
            && powerStatusAllowsReboot())
            reboot(js, false, 0);
            
        // update the PSU2 power sensing status
        powerStatusUpdate(cfg);
    }
    
    // we're now connected to the host
    connected = true;
    
    // Set up a timer for keeping track of how long it's been since we
    // sent the last joystick report.  We use this to determine when it's
    // time to send the next joystick report.  
    //
    // We have to use a timer for two reasons.  The first is that our main
    // loop runs too fast (about .25ms to 2.5ms per loop, depending on the
    // type of plunger sensor attached and other factors) for us to send
    // joystick reports on every iteration.  We *could*, but the PC couldn't
    // digest them at that pace.  So we need to slow down the reports to a
    // reasonable pace.  The second is that VP has some complicated timing
    // issues of its own, so we not only need to slow down the reports from
    // our "natural" pace, but also time them to sync up with VP's input
    // sampling rate as best we can.
    Timer jsReportTimer;
    jsReportTimer.start();
    
    // Accelerometer sample "stutter" counter.  Each time we send a joystick
    // report, we increment this counter, and check to see if it has reached 
    // the threshold set in the configuration.  If so, we take a new 
    // accelerometer sample and send it with the new joystick report.  It
    // not, we don't take a new sample, but simply repeat the last sample.
    //
    // This lets us send joystick reports more frequently than accelerometer
    // samples.  The point is to let us slow down accelerometer reports to
    // a pace that matches VP's input sampling frequency, while still sending
    // joystick button updates more frequently, so that other programs that
    // can read input faster will see button changes with less latency.
    int jsAccelStutterCounter = 0;
    
    // Last accelerometer report, in joystick units.  We normally report the 
    // acceleromter reading via the joystick X and Y axes, per the VP 
    // convention.  We can alternatively report in the RX and RY axes; this
    // can be set in the configuration.
    int x = 0, y = 0;
    
    // Time since we successfully sent a USB report.  This is a hacky 
    // workaround to deal with any remaining sporadic problems in the USB 
    // stack.  I've been trying to bulletproof the USB code over time to 
    // remove all such problems at their source, but it seems unlikely that
    // we'll ever get them all.  Thus this hack.  The idea here is that if
    // we go too long without successfully sending a USB report, we'll
    // assume that the connection is broken (and the KL25Z USB hardware
    // hasn't noticed this), and we'll try taking measures to recover.
    Timer jsOKTimer;
    jsOKTimer.start();
    
    // Initialize the calibration button and lamp, if enabled.  To be enabled,
    // the pin has to be assigned to something other than NC (0xFF), AND the
    // corresponding feature enable flag has to be set.
    DigitalIn *calBtn = 0;
    DigitalOut *calBtnLed = 0;
    
    // calibration button input - feature flag 0x01
    if ((cfg.plunger.cal.features & 0x01) && cfg.plunger.cal.btn != 0xFF)
        calBtn = new DigitalIn(wirePinName(cfg.plunger.cal.btn));
        
    // calibration button indicator lamp output - feature flag 0x02
    if ((cfg.plunger.cal.features & 0x02) && cfg.plunger.cal.led != 0xFF)
        calBtnLed = new DigitalOut(wirePinName(cfg.plunger.cal.led));

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
    Accel accel(MMA8451_SCL_PIN, MMA8451_SDA_PIN, MMA8451_I2C_ADDRESS, 
        MMA8451_INT_PIN, cfg.accel.range, cfg.accel.autoCenterTime);
       
    // initialize the plunger sensor
    plungerSensor->init();
    
    // set up the ZB Launch Ball monitor
    ZBLaunchBall zbLaunchBall;
    
    // enable the peripheral chips
    if (tlc5940 != 0)
        tlc5940->enable(true);
    if (hc595 != 0)
        hc595->enable(true);
    if (tlc59116 != 0)
        tlc59116->enable(true);
        
    // start the LedWiz flash cycle timer
    wizCycleTimer.start();
    
    // start the PWM update polling timer
    polledPwmTimer.start();
    
    // we're all set up - now just loop, processing sensor reports and 
    // host requests
    for (;;)
    {
        // start the main loop timer for diagnostic data collection
        IF_DIAG(mainLoopTimer.reset(); mainLoopTimer.start();)
        
        // Process incoming reports on the joystick interface.  The joystick
        // "out" (receive) endpoint is used for LedWiz commands and our 
        // extended protocol commands.  Limit processing time to 5ms to
        // ensure we don't starve the input side.
        LedWizMsg lwm;
        Timer lwt;
        lwt.start();
        IF_DIAG(int msgCount = 0;) 
        while (js.readLedWizMsg(lwm) && lwt.read_us() < 5000)
        {
            handleInputMsg(lwm, js, accel);
            IF_DIAG(++msgCount;)
        }
        
        // collect performance statistics on the message reader, if desired
        IF_DIAG(
            if (msgCount != 0)
            {
                mainLoopMsgTime += lwt.read_us();
                mainLoopMsgCount++;
            }
        )
        
        // process IR input
        process_IR(cfg, js);
    
        // update the PSU2 power sensing status
        powerStatusUpdate(cfg);

        // update flashing LedWiz outputs periodically
        wizPulse();
        
        // update PWM outputs
        pollPwmUpdates();
        
        // update Flipper Logic and Chime Logic outputs
        LwFlipperLogicOut::poll();
        LwChimeLogicOut::poll();
        
        // poll the accelerometer
        accel.poll();
            
        // Note the "effective" plunger enabled status.  This has two
        // components: the explicit "enabled" bit, and the plunger sensor
        // type setting.  For most purposes, a plunger type of NONE is
        // equivalent to disabled.  Set this to explicit 0x01 or 0x00
        // so that we can OR the bit into status reports.
        uint8_t effectivePlungerEnabled = (cfg.plunger.enabled
            && cfg.plunger.sensorType != PlungerType_None) ? 0x01 : 0x00;
            
        // collect diagnostic statistics, checkpoint 0
        IF_DIAG(mainLoopIterCheckpt[0] += mainLoopTimer.read_us();)

        // send TLC5940 data updates if applicable
        if (tlc5940 != 0)
            tlc5940->send();
            
        // send TLC59116 data updates
        if (tlc59116 != 0)
            tlc59116->send();
       
        // collect diagnostic statistics, checkpoint 1
        IF_DIAG(mainLoopIterCheckpt[1] += mainLoopTimer.read_us();)
        
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
                if (calBtnTimer.read_us() > 50000)
                    calBtnState = 2;
                break;
                
            case 2:
                // in the hold period - if the button has been held down
                // for the entire hold period, move to calibration mode
                if (calBtnTimer.read_us() > 2050000)
                {
                    // enter calibration mode
                    calBtnState = 3;
                    calBtnTimer.reset();
                    
                    // begin the plunger calibration limits
                    plungerReader.setCalMode(true);
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
            if (calBtnState == 3 && calBtnTimer.read_us() > 15000000)
            {
                // exit calibration mode
                calBtnState = 0;
                plungerReader.setCalMode(false);
                
                // save the updated configuration
                cfg.plunger.cal.calibrated = 1;
                saveConfigToFlash(0, false);
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
            newCalBtnLit = ((calBtnTimer.read_us()/250000) & 1);
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
        
        // collect diagnostic statistics, checkpoint 2
        IF_DIAG(mainLoopIterCheckpt[2] += mainLoopTimer.read_us();)

        // read the plunger sensor
        plungerReader.read();
        
        // collect diagnostic statistics, checkpoint 3
        IF_DIAG(mainLoopIterCheckpt[3] += mainLoopTimer.read_us();)

        // update the ZB Launch Ball status
        zbLaunchBall.update();
        
        // collect diagnostic statistics, checkpoint 4
        IF_DIAG(mainLoopIterCheckpt[4] += mainLoopTimer.read_us();)

        // process button updates
        processButtons(cfg);
        
        // collect diagnostic statistics, checkpoint 5
        IF_DIAG(mainLoopIterCheckpt[5] += mainLoopTimer.read_us();)

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
        
        // collect diagnostic statistics, checkpoint 6
        IF_DIAG(mainLoopIterCheckpt[6] += mainLoopTimer.read_us();)

        // flag:  did we successfully send a joystick report on this round?
        bool jsOK = false;
        
        // figure the current status flags for joystick reports
        uint16_t statusFlags = 
            effectivePlungerEnabled         // 0x01
            | nightMode                     // 0x02
            | ((psu2_state & 0x07) << 2)    // 0x04 0x08 0x10
            | saveConfigSucceededFlag;      // 0x40
        if (IRLearningMode != 0)
            statusFlags |= 0x20;

        // If it's been long enough since our last USB status report, send
        // the new report.  VP only polls for input in 10ms intervals, so
        // there's no benefit in sending reports more frequently than this.
        // More frequent reporting would only add USB I/O overhead.
        if (cfg.joystickEnabled && jsReportTimer.read_us() > cfg.jsReportInterval_us)
        {
            // Increment the "stutter" counter.  If it has reached the
            // stutter threshold, read a new accelerometer sample.  If 
            // not, repeat the last sample.
            if (++jsAccelStutterCounter >= cfg.accel.stutter)
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
                
                // rotate X and Y according to the device orientation in the cabinet
                accelRotate(x, y);

                // reset the stutter counter
                jsAccelStutterCounter = 0;
            }
            
            // Report the current plunger position unless the plunger is
            // disabled, or the ZB Launch Ball signal is on.  In either of
            // those cases, just report a constant 0 value.  ZB Launch Ball 
            // temporarily disables mechanical plunger reporting because it 
            // tells us that the table has a Launch Ball button instead of
            // a traditional plunger, so we don't want to confuse VP with
            // regular plunger inputs.
            int zActual = plungerReader.getPosition();
            int zReported = (!effectivePlungerEnabled || zbLaunchOn ? 0 : zActual);
            
            // send the joystick report
            jsOK = js.update(x, y, zReported, jsButtons, statusFlags);
            
            // we've just started a new report interval, so reset the timer
            jsReportTimer.reset();
        }

        // If we're in sensor status mode, report all pixel exposure values
        if (reportPlungerStat && plungerSensor->ready())
        {
            // send the report            
            plungerSensor->sendStatusReport(js, reportPlungerStatFlags);

            // we have satisfied this request
            reportPlungerStat = false;
        }
        
        // Reset the plunger status report extra timer after enough time has
        // elapsed to satisfy the request.  We don't just do this immediately
        // because of the complexities of the pixel frame buffer pipelines in
        // most of the image sensors.  The pipelines delay the effect of the
        // exposure time request by a couple of frames, so we can't be sure
        // exactly when they're applied - meaning we can't consider the
        // delay time to be consumed after a fixed number of frames.  Instead,
        // we'll consider it consumed after a long enough time to be sure
        // we've sent a few frames.  The extra time value is meant to be an
        // interactive tool for debugging, so it's not important to reset it
        // immediately - the user will probably want to see the effect over
        // many frames, so they're likely to keep sending requests with the
        // time value over and over.  They'll eventually shut down the frame
        // viewer and return to normal operation, at which point the requests
        // will stop.  So we just have to clear things out after we haven't
        // seen a request with extra time for a little while.
        if (reportPlungerStatTime != 0 
            && static_cast<uint32_t>(requestTimestamper.read_us() - tReportPlungerStat) > 1000000)
        {
            reportPlungerStatTime = 0;
            plungerSensor->setExtraIntegrationTime(0);
        }
        
        // If joystick reports are turned off, send a generic status report
        // periodically for the sake of the Windows config tool.
        if (!cfg.joystickEnabled && jsReportTimer.read_us() > 10000UL)
        {
            jsOK = js.updateStatus(statusFlags);
            jsReportTimer.reset();
        }

        // if we successfully sent a joystick report, reset the watchdog timer
        if (jsOK) 
        {
            jsOKTimer.reset();
            jsOKTimer.start();
        }

        // collect diagnostic statistics, checkpoint 7
        IF_DIAG(mainLoopIterCheckpt[7] += mainLoopTimer.read_us();)

#ifdef DEBUG_PRINTF
        if (x != 0 || y != 0)
            printf("%d,%d\r\n", x, y);
#endif

        // check for connection status changes
        bool newConnected = js.isConnected() && !js.isSleeping();
        if (newConnected != connected)
        {
            // give it a moment to stabilize
            connectChangeTimer.start();
            if (connectChangeTimer.read_us() > 1000000)
            {
                // note the new status
                connected = newConnected;
                
                // done with the change timer for this round - reset it for next time
                connectChangeTimer.stop();
                connectChangeTimer.reset();
                
                // if we're newly disconnected, clean up for PC suspend mode or power off
                if (!connected)
                {
                    // turn off all outputs
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
                    if (tlc59116 != 0)
                        tlc59116->enable(false);
                    if (hc595 != 0)
                        hc595->enable(false);
                }
            }
        }
        
        // if we have a reboot timer pending, check for completion
        if (saveConfigFollowupTimer.isRunning() 
            && saveConfigFollowupTimer.read_us() > saveConfigFollowupTime*1000000UL)
        {
            // if a reboot is pending, execute it now
            if (saveConfigRebootPending)
            {
                // Only reboot if the PSU2 power state allows it.  If it 
                // doesn't, suppress the reboot for now, but leave the boot
                // flags set so that we keep checking on future rounds.
                // That way we should eventually reboot when the power
                // status allows it.
                if (powerStatusAllowsReboot())
                    reboot(js);
            }
            else
            {
                // No reboot required.  Exit the timed post-save state.
                
                // stop and reset the post-save timer
                saveConfigFollowupTimer.stop();
                saveConfigFollowupTimer.reset();
                
                // clear the post-save success flag
                saveConfigSucceededFlag = 0;
            }
        }
                        
        // if we're disconnected, initiate a new connection
        if (!connected)
        {
            // show USB HAL debug events
            extern void HAL_DEBUG_PRINTEVENTS(const char *prefix);
            HAL_DEBUG_PRINTEVENTS(">DISC");
            
            // show immediate diagnostic feedback
            js.diagFlash();
            
            // clear any previous diagnostic LED display
            diagLED(0, 0, 0);
            
            // set up a timer to monitor the reboot timeout
            Timer reconnTimeoutTimer;
            reconnTimeoutTimer.start();
            
            // set up a timer for diagnostic displays
            Timer diagTimer;
            diagTimer.reset();
            diagTimer.start();
            
            // turn off the main loop timer while spinning
            IF_DIAG(mainLoopTimer.stop();)

            // loop until we get our connection back            
            while (!js.isConnected() || js.isSleeping())
            {
                // try to recover the connection
                js.recoverConnection();
                
                // update Flipper Logic and Chime Logic outputs
                LwFlipperLogicOut::poll();
                LwChimeLogicOut::poll();

                // send TLC5940 data if necessary
                if (tlc5940 != 0)
                    tlc5940->send();
                    
                // update TLC59116 outputs
                if (tlc59116 != 0)
                    tlc59116->send();
                
                // show a diagnostic flash every couple of seconds
                if (diagTimer.read_us() > 2000000)
                {
                    // flush the USB HAL debug events, if in debug mode
                    HAL_DEBUG_PRINTEVENTS(">NC");
                    
                    // show diagnostic feedback
                    js.diagFlash();
                    
                    // reset the flash timer
                    diagTimer.reset();
                }
                
                // If the disconnect reboot timeout has expired, reboot.
                // Some PC hosts won't reconnect to a device that's left
                // plugged in through various events on the PC side, such as 
                // rebooting Windows, cycling power on the PC, or just a lost
                // USB connection.  Rebooting the KL25Z seems to be the most
                // reliable way to get Windows to notice us again after one
                // of these events and make it reconnect.  Only reboot if
                // the PSU2 power status allows it - if not, skip it on this
                // round and keep waiting.
                if (cfg.disconnectRebootTimeout != 0 
                    && reconnTimeoutTimer.read() > cfg.disconnectRebootTimeout
                    && powerStatusAllowsReboot())
                    reboot(js, false, 0);

                // update the PSU2 power sensing status
                powerStatusUpdate(cfg);
            }
            
            // resume the main loop timer
            IF_DIAG(mainLoopTimer.start();)
            
            // if we made it out of that loop alive, we're connected again!
            connected = true;
            HAL_DEBUG_PRINTEVENTS(">C");

            // Enable peripheral chips and update them with current output data
            if (tlc5940 != 0)
                tlc5940->enable(true);
            if (tlc59116 != 0)
                tlc59116->enable(true);
            if (hc595 != 0)
            {
                hc595->enable(true);
                hc595->update(true);
            }
        }

        // provide a visual status indication on the on-board LED
        if (calBtnState < 2 && hbTimer.read_us() > 1000000) 
        {
            if (jsOKTimer.read_us() > 1000000)
            {
                // USB freeze - show red/yellow.
                //
                // It's been more than a second since we successfully sent a joystick
                // update message.  This must mean that something's wrong on the USB
                // connection, even though we haven't detected an outright disconnect.
                // Show a distinctive diagnostic LED pattern when this occurs.
                hb = !hb;
                diagLED(1, hb, 0);
                
                // If the reboot-on-disconnect option is in effect, treat this condition
                // as equivalent to a disconnect, since something is obviously wrong
                // with the USB connection.  
                if (cfg.disconnectRebootTimeout != 0)
                {
                    // The reboot timeout is in effect.  If we've been incommunicado for
                    // longer than the timeout, reboot.  If we haven't reached the time
                    // limit, keep running for now, and leave the OK timer running so 
                    // that we can continue to monitor this.  Only reboot if the PSU2
                    // power status allows it.
                    if (jsOKTimer.read() > cfg.disconnectRebootTimeout
                        && powerStatusAllowsReboot())
                        reboot(js, false, 0);
                }
                else
                {
                    // There's no reboot timer, so just keep running with the diagnostic
                    // pattern displayed.  Since we're not waiting for any other timed
                    // conditions in this state, stop the timer so that it doesn't 
                    // overflow if this condition persists for a long time.
                    jsOKTimer.stop();
                }
            }
            else if (psu2_state >= 4)
            {
                // We're in the TV timer countdown.  Skip the normal heartbeat
                // flashes and show the TV timer flashes instead.
                diagLED(0, 0, 0);
            }
            else if (effectivePlungerEnabled && !cfg.plunger.cal.calibrated)
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
        
        // collect statistics on the main loop time, if desired
        IF_DIAG(
            mainLoopIterTime += mainLoopTimer.read_us();
            mainLoopIterCount++;
        )
    }
}
