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
// "Pinscape" is the name of my custom-built virtual pinball cabinet, so I call this
// software the Pinscape Controller.  I wrote it to handle several tasks that I needed
// for my cabinet.  It runs on a Freescale KL25Z microcontroller, which is a small and 
// inexpensive device that attaches to the cabinet PC via a USB cable, and can attach
// via custom wiring to sensors, buttons, and other devices in the cabinet.
//
// I designed the software and hardware in this project especially for my own
// cabinet, but it uses standard interfaces in Windows and Visual Pinball, so it should
// work in any VP-based cabinet, as long as you're using the usual VP software suite.  
// I've tried to document the hardware in enough detail for anyone else to duplicate 
// the entire project, and the full software is open source.
//
// The Freescale board appears to the host PC as a standard USB joystick.  This works 
// with the built-in Windows joystick device drivers, so there's no need to install any
// new drivers or other software on the PC.  Windows should recognize the Freescale
// as a joystick when you plug it into the USB port, and Windows shouldn't ask you to 
// install any drivers.  If you bring up the Windows control panel for USB Game 
// Controllers, this device will appear as "Pinscape Controller".  *Don't* do any 
// calibration with the Windows control panel or third-part calibration tools.  The 
// software calibrates the accelerometer portion automatically, and has its own special
// calibration procedure for the plunger sensor, if you're using that (see below).
//
// This software provides a whole bunch of separate features.  You can use any of these 
// features individually or all together.  If you're not using a particular feature, you
// can simply omit the extra wiring and/or hardware for that feature.  You can use
// the nudging feature by itself without any extra hardware attached, since the
// accelerometer is built in to the KL25Z board.
//
//  - Nudge sensing via the KL25Z's on-board accelerometer.  Nudging the cabinet
//    causes small accelerations that the accelerometer can detect; these are sent to
//    Visual Pinball via the joystick interface so that VP can simulate the effect
//    of the real physical nudges on its simulated ball.  VP has native handling for
//    this type of input, so all you have to do is set some preferences in VP to tell 
//    it that an accelerometer is attached.
//
//  - Plunger position sensing via an attached TAOS TSL 1410R CCD linear array sensor.  
//    To use this feature, you need to buy the TAOS device (it's not built in to the
//    KL25Z, obviously), wire it to the KL25Z (5 wire connections between the two
//    devices are required), and mount the TAOS sensor in your cabinet so that it's
//    positioned properly to capture images of the physical plunger shooter rod.
//
//    The physical mounting and wiring details are desribed in the project 
//    documentation.  
//
//    If the CCD is attached, the software constantly captures images from the CCD
//    and analyzes them to determine how far back the plunger is pulled.  It reports
//    this to Visual Pinball via the joystick interface.  This allows VP to make the
//    simulated on-screen plunger track the motion of the physical plunger in real
//    time.  As with the nudge data, VP has native handling for the plunger input, 
//    so you just need to set the VP preferences to tell it that an analog plunger 
//    device is attached.  One caveat, though: although VP itself has built-in 
//    support for an analog plunger, not all existing tables take advantage of it.  
//    Many existing tables have their own custom plunger scripting that doesn't
//    cooperate with the VP plunger input.  All tables *can* be made to work with
//    the plunger, and in most cases it only requires some simple script editing,
//    but in some cases it requires some more extensive surgery.
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
//    for buttons and switches.  The software reports these as joystick buttons when
//    it sends reports to the PC.  These can be used to wire physical pinball-style
//    buttons in the cabinet (e.g., flipper buttons, the Start button) and miscellaneous 
//    switches (such as a tilt bob) to the PC.  Visual Pinball can use joystick buttons
//    for input - you just have to assign a VP function to each button using VP's
//    keyboard options dialog.  To wire a button physically, connect one terminal of
//    the button switch to the KL25Z ground, and connect the other terminal to the
//    the GPIO port you wish to assign to the button.  See the buttonMap[] array
//    below for the available GPIO ports and their assigned joystick button numbers.
//    If you're not using a GPIO port, you can just leave it unconnected - the digital
//    inputs have built-in pull-up resistors, so an unconnected port is the same as
//    an open switch (an "off" state for the button).
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
//    should be plenty for nearly any virtual pinball project.
//
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
//        that the device can't tell whether a CCD is physically attached;
//        if you don't have a CCD attached, you can set the appropriate option 
//        in config.h or use the  Windows config tool to disable the CCD 
//        software features.
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
//
// Plunger calibration mode: the host can activate plunger calibration mode
// by sending this packet.  This has the same effect as pressing and holding
// the plunger calibration button for two seconds, to allow activating this
// mode without attaching a physical button.
//
//    length = 8 bytes
//    byte 0 = 65 (0x41)
//    byte 1 = 2 (0x02)
//
// Exposure reports: the host can request a report of the full set of pixel
// values for the next frame by sending this special packet:
//
//    length = 8 bytes
//    byte 0 = 65 (0x41)
//    byte 1 = 3 (0x03)
//
// We'll respond with a series of special reports giving the exposure status.
// Each report has the following structure:
//
//    bytes 0:1 = 11-bit index, with high 5 bits set to 10000.  For 
//                example, 0x04 0x80 indicates index 4.  This is the 
//                starting pixel number in the report.  The first report 
//                will be 0x00 0x80 to indicate pixel #0.  
//    bytes 2:3 = 16-bit unsigned int brightness level of pixel at index
//    bytes 4:5 = brightness of pixel at index+1
//    etc for the rest of the packet
//
// This still has the form of a joystick packet at the USB level, but
// can be differentiated by the host via the status bits.  It would have
// been cleaner to use a different Report ID at the USB level, but this
// would have necessitated a different container structure in the report
// descriptor, which would have broken LedWiz compatibility.  Given that
// constraint, we have to re-use the joystick report type, making for
// this somewhat kludgey approach.
 
#include "mbed.h"
#include "math.h"
#include "USBJoystick.h"
#include "MMA8451Q.h"
#include "tsl1410r.h"
#include "FreescaleIAP.h"
#include "crc32.h"
#include "TLC5940.h"

// our local configuration file
#define DECL_EXTERNS
#include "config.h"


// ---------------------------------------------------------------------------
// utilities

// number of elements in an array
#define countof(x) (sizeof(x)/sizeof((x)[0]))

// floating point square of a number
inline float square(float x) { return x*x; }

// floating point rounding
inline float round(float x) { return x > 0 ? floor(x + 0.5) : ceil(x - 0.5); }


// ---------------------------------------------------------------------------
// USB device vendor ID, product ID, and version.  
//
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


// Joystick axis report range - we report from -JOYMAX to +JOYMAX
#define JOYMAX 4096

// --------------------------------------------------------------------------
//
// Set up mappings for the joystick X and Y reports based on the mounting
// orientation of the KL25Z in the cabinet.  Visual Pinball and other 
// pinball software effectively use video coordinates to define the axes:
// positive X is to the right of the table, negative Y to the left, positive
// Y toward the front of the table, negative Y toward the back.  The KL25Z
// accelerometer is mounted on the board with positive Y toward the USB
// ports and positive X toward the right side of the board with the USB
// ports pointing up.  It's a simple matter to remap the KL25Z coordinate
// system to match VP's coordinate system for mounting orientations at
// 90-degree increments...
//
#if defined(ORIENTATION_PORTS_AT_FRONT)
# define JOY_X(x, y)   (y)
# define JOY_Y(x, y)   (x)
#elif defined(ORIENTATION_PORTS_AT_LEFT)
# define JOY_X(x, y)   (-(x))
# define JOY_Y(x, y)   (y)
#elif defined(ORIENTATION_PORTS_AT_RIGHT)
# define JOY_X(x, y)   (x)
# define JOY_Y(x, y)   (-(y))
#elif defined(ORIENTATION_PORTS_AT_REAR)
# define JOY_X(x, y)   (-(y))
# define JOY_Y(x, y)   (-(x))
#else
# error Please define one of the ORIENTATION_PORTS_AT_xxx macros to establish the accelerometer orientation in your cabinet
#endif



// --------------------------------------------------------------------------
//
// Define a symbol to tell us whether any sort of plunger sensor code
// is enabled in this build.  Note that this doesn't tell us that a
// plunger device is actually attached or *currently* enabled; it just
// tells us whether or not the code for plunger sensing is enabled in 
// the software build.  This lets us leave out some unnecessary code
// on installations where no physical plunger is attached.
//
const int PLUNGER_CODE_ENABLED =
#if defined(ENABLE_CCD_SENSOR) || defined(ENABLE_POT_SENSOR)
    1;
#else
    0;
#endif

// ---------------------------------------------------------------------------
//
// On-board RGB LED elements - we use these for diagnostic displays.
//
// Note that LED3 (the blue segment) is hard-wired on the KL25Z to PTD1,
// so PTD1 shouldn't be used for any other purpose (e.g., as a keyboard
// input or a device output).  (This is kind of unfortunate in that it's 
// one of only two ports exposed on the jumper pins that can be muxed to 
// SPI0 SCLK.  This effectively limits us to PTC5 if we want to use the 
// SPI capability.)
//
DigitalOut ledR(LED1), ledG(LED2), ledB(LED3);


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

// Figure the number of outputs.  If we're in the default LedWiz mode,
// we have a fixed set of 32 outputs.  If we're in TLC5940 enhanced mode,
// we have 16 outputs per chip.  To simplify the LedWiz compatibility code,
// always use a minimum of 32 outputs even if we have fewer than two of the
// TLC5940 chips.
#if !defined(ENABLE_TLC5940) || (TLC_NCHIPS) < 2
# define NUM_OUTPUTS   32
#else
# define NUM_OUTPUTS   ((TLC5940_NCHIPS)*16)
#endif

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


#ifdef ENABLE_TLC5940

// The TLC5940 interface object.
TLC5940 tlc5940(TLC5940_SCLK, TLC5940_SIN, TLC5940_GSCLK, TLC5940_BLANK,
    TLC5940_XLAT, TLC5940_NCHIPS);

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
           tlc5940.set(idx, (int)(val * 4095));
    }
    int idx;
    float prv;
};

#else // ENABLE_TLC5940

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

#endif // ENABLE_TLC5940

// LwOut class for unmapped ports.  The LedWiz protocol is hardwired
// for 32 ports, but we might not want to assign all 32 software ports
// to physical output pins - the KL25Z has a limited number of GPIO
// ports, so we might not have enough available GPIOs to fill out the
// full LedWiz complement after assigning GPIOs for other functions.
// This class is used to populate the LedWiz mapping array for ports
// that aren't connected to physical outputs; it simply ignores value 
// changes.
class LwUnusedOut: public LwOut
{
public:
    LwUnusedOut() { }
    virtual void set(float val) { }
};

// Array of output physical pin assignments.  This array is indexed
// by LedWiz logical port number - lwPin[n] is the maping for LedWiz
// port n (0-based).  If we're using GPIO ports to implement outputs,
// we initialize the array at start-up to map each logical port to the 
// physical GPIO pin for the port specified in the ledWizPortMap[] 
// array in config.h.  If we're using TLC5940 chips for the outputs,
// we map each logical port to the corresponding TLC5940 output.
static LwOut *lwPin[NUM_OUTPUTS];

// initialize the output pin array
void initLwOut()
{
    for (int i = 0 ; i < countof(lwPin) ; ++i)
    {
#ifdef ENABLE_TLC5940
        // Set up a TLC5940 output.  If the output is within range of
        // the connected number of chips (16 outputs per chip), assign it
        // to the current index, otherwise leave it unattached.
        if (i < (TLC5940_NCHIPS)*16)
            lwPin[i] = new Lw5940Out(i);
        else
            lwPin[i] = new LwUnusedOut();

#else // ENABLE_TLC5940
        // Set up the GPIO pin.  If the pin is not connected ("NC" in the
        // pin map), set up a dummy "unused" output for it.  If it's a
        // real pin, set up a PWM-capable or Digital-Only output handler
        // object, according to the pin type in the map.
        PinName p = (i < countof(ledWizPortMap) ? ledWizPortMap[i].pin : NC);
        if (p == NC)
            lwPin[i] = new LwUnusedOut();
        else if (ledWizPortMap[i].isPWM)
            lwPin[i] = new LwPwmOut(p);
        else
            lwPin[i] = new LwDigOut(p);
            
#endif // ENABLE_TLC5940

    }
}

// Current absolute brightness level for an output.  This is a float
// value from 0.0 for fully off to 1.0 for fully on.  This is the final
// derived value for the port.  For outputs set by LedWiz messages, 
// this is derived from te LedWiz state, and is updated on each pulse 
// timer interrupt for lights in flashing states.  For outputs set by 
// extended protocol messages, this is simply the brightness last set.
static float outLevel[NUM_OUTPUTS];

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
        return val/48.0;
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
        return 1.0;
    }
    else if (val == 129)
    {
        //   129 = ramp up / ramp down
        return wizFlashCounter < 128 
            ? wizFlashCounter/128.0 
            : (256 - wizFlashCounter)/128.0;
    }
    else if (val == 130)
    {
        //   130 = flash on / off
        return wizFlashCounter < 128 ? 1.0 : 0.0;
    }
    else if (val == 131)
    {
        //   131 = on / ramp down
        return wizFlashCounter < 128 ? 1.0 : (255 - wizFlashCounter)/128.0;
    }
    else if (val == 132)
    {
        //   132 = ramp up / on
        return wizFlashCounter < 128 ? wizFlashCounter/128.0 : 1.0;
    }
    else
    {
        // Other values are undefined in the LedWiz documentation.  Hosts
        // *should* never send undefined values, since whatever behavior an
        // LedWiz unit exhibits in response is accidental and could change
        // in a future version.  We'll treat all undefined values as equivalent 
        // to 48 (fully on).
        return 1.0;
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
#define WIZ_PULSE_TIME_BASE  (1.0/127.0)
static void wizPulse()
{
    // increase the counter by the speed increment, and wrap at 256
    wizFlashCounter += wizSpeed;
    wizFlashCounter &= 0xff;
    
    // if we have any flashing lights, update them
    int ena = false;
    for (int i = 0 ; i < 32 ; ++i)
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
    for (int i = 0 ; i < 32 ; ++i)
    {
        pulse |= (wizVal[i] >= 129 && wizVal[i] <= 132);
        lwPin[i]->set(wizState(i));
    }
    
    // if any outputs are set to flashing mode, and the pulse timer
    // isn't running, turn it on
    if (pulse)
        wizPulseTimer.attach(wizPulse, WIZ_PULSE_TIME_BASE);
}

// ---------------------------------------------------------------------------
//
// Button input
//

// button input map array
DigitalIn *buttonDigIn[32];

// button state
struct ButtonState
{
    // current on/off state
    int pressed;
    
    // Sticky time remaining for current state.  When a
    // state transition occurs, we set this to a debounce
    // period.  Future state transitions will be ignored
    // until the debounce time elapses.
    int t;
} buttonState[32];

// timer for button reports
static Timer buttonTimer;

// initialize the button inputs
void initButtons()
{
    // create the digital inputs
    for (int i = 0 ; i < countof(buttonDigIn) ; ++i)
    {
        if (i < countof(buttonMap) && buttonMap[i] != NC)
            buttonDigIn[i] = new DigitalIn(buttonMap[i]);
        else
            buttonDigIn[i] = 0;
    }
    
    // start the button timer
    buttonTimer.start();
}


// read the button input state
uint32_t readButtons()
{
    // start with all buttons off
    uint32_t buttons = 0;
    
    // figure the time elapsed since the last scan
    int dt = buttonTimer.read_ms();
    
    // reset the timef for the next scan
    buttonTimer.reset();
    
    // scan the button list
    uint32_t bit = 1;
    DigitalIn **di = buttonDigIn;
    ButtonState *bs = buttonState;
    for (int i = 0 ; i < countof(buttonDigIn) ; ++i, ++di, ++bs, bit <<= 1)
    {
        // read this button
        if (*di != 0)
        {
            // deduct the elapsed time since the last update
            // from the button's remaining sticky time
            bs->t -= dt;
            if (bs->t < 0)
                bs->t = 0;
            
            // If the sticky time has elapsed, note the new physical
            // state of the button.  If we still have sticky time
            // remaining, ignore the physical state; the last state
            // change persists until the sticky time elapses so that
            // we smooth out any "bounce" (electrical transients that
            // occur when the switch contact is opened or closed).
            if (bs->t == 0)
            {
                // get the new physical state
                int pressed = !(*di)->read();
                
                // update the button's logical state if this is a change
                if (pressed != bs->pressed)
                {
                    // store the new state
                    bs->pressed = pressed;
                    
                    // start a new sticky period for debouncing this
                    // state change
                    bs->t = 25;
                }
            }
            
            // if it's pressed, OR its bit into the state
            if (bs->pressed)
                buttons |= bit;
        }
    }
    
    // return the new button list
    return buttons;
}

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
// Include the appropriate plunger sensor definition.  This will define a
// class called PlungerSensor, with a standard interface that we use in
// the main loop below.  This is *kind of* like a virtual class interface,
// but it actually defines the methods statically, which is a little more
// efficient at run-time.  There's no need for a true virtual interface
// because we don't need to be able to change sensor types on the fly.
//

#if defined(ENABLE_CCD_SENSOR)
#include "ccdSensor.h"
#elif defined(ENABLE_POT_SENSOR)
#include "potSensor.h"
#else
#include "nullSensor.h"
#endif


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
    
    // reset calibration data for calibration mode
    void resetPlunger()
    {
        // set extremes for the calibration data
        d.plungerMax = 0;
        d.plungerZero = npix;
        d.plungerMin = npix;
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
        
        // Plunger calibration min, zero, and max.  The zero point is the 
        // rest position (aka park position), where it's in equilibrium between 
        // the main spring and the barrel spring.  It can travel a small distance
        // forward of the rest position, because the barrel spring can be
        // compressed by the user pushing on the plunger or by the momentum
        // of a release motion.  The minimum is the maximum forward point where
        // the barrel spring can't be compressed any further.
        int plungerMin;
        int plungerZero;
        int plungerMax;
        
        // is the plunger sensor enabled?
        int plungerEnabled;
        
        // LedWiz unit number
        uint8_t ledWizUnitNo;
    } d;
};


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
    
    // initialize the button input ports
    initButtons();
    
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
        cfg.d.plungerMin = 0;        // assume we can go all the way forward...
        cfg.d.plungerMax = npix;     // ...and all the way back
        cfg.d.plungerZero = npix/6;  // the rest position is usually around 1/2" back
        cfg.d.ledWizUnitNo = DEFAULT_LEDWIZ_UNIT_NUMBER - 1;  // unit numbering starts from 0 internally
        cfg.d.plungerEnabled = PLUNGER_CODE_ENABLED;
    }
    
    // Create the joystick USB client.  Note that we use the LedWiz unit
    // number from the saved configuration.
    MyUSBJoystick js(
        USB_VENDOR_ID, 
        USB_PRODUCT_ID | cfg.d.ledWizUnitNo,
        USB_VERSION_NO);
        
    // last report timer - we use this to throttle reports, since VP
    // doesn't want to hear from us more than about every 10ms
    Timer reportTimer;
    reportTimer.start();

    // initialize the calibration buttons, if present
    DigitalIn *calBtn = (CAL_BUTTON_PIN == NC ? 0 : new DigitalIn(CAL_BUTTON_PIN));
    DigitalOut *calBtnLed = (CAL_BUTTON_LED == NC ? 0 : new DigitalOut(CAL_BUTTON_LED));

    // plunger calibration button debounce timer
    Timer calBtnTimer;
    calBtnTimer.start();
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
    
#ifdef ENABLE_JOYSTICK
    // last accelerometer report, in joystick units (we report the nudge
    // acceleration via the joystick x & y axes, per the VP convention)
    int x = 0, y = 0;
    
    // flag: send a pixel dump after the next read
    bool reportPix = false;
#endif

#ifdef ENABLE_TLC5940
    // start the TLC5940 clock
    tlc5940.start();
#endif

    // create our plunger sensor object
    PlungerSensor plungerSensor;

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
    plungerSensor.init();
    
    // Device status.  We report this on each update so that the host config
    // tool can detect our current settings.  This is a bit mask consisting
    // of these bits:
    //    0x01  -> plunger sensor enabled
    uint16_t statusFlags = (cfg.d.plungerEnabled ? 0x01 : 0x00);
    
    // we're all set up - now just loop, processing sensor reports and 
    // host requests
    for (;;)
    {
        // Look for an incoming report.  Process a few input reports in
        // a row, but stop after a few so that a barrage of inputs won't
        // starve our output event processing.  Also, pause briefly between
        // reads; allowing reads to occur back-to-back seems to occasionally 
        // stall the USB pipeline (for reasons unknown; I'd fix the underlying 
        // problem if I knew what it was).
        HID_REPORT report;
        for (int rr = 0 ; rr < 4 && js.readNB(&report) ; ++rr, wait_ms(1))
        {
            // all Led-Wiz reports are 8 bytes exactly
            if (report.length == 8)
            {
                uint8_t *data = report.data;
                if (data[0] == 64) 
                {
                    // LWZ-SBA - first four bytes are bit-packed on/off flags
                    // for the outputs; 5th byte is the pulse speed (1-7)
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
                    
                    // set the flash speed - enforce the value range 1-7
                    wizSpeed = data[5];
                    if (wizSpeed < 1)
                        wizSpeed = 1;
                    else if (wizSpeed > 7)
                        wizSpeed = 7;
        
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
                        // 1 = Set Configuration:
                        //     data[2] = LedWiz unit number (0x00 to 0x0f)
                        //     data[3] = feature enable bit mask:
                        //               0x01 = enable plunger sensor
                        
                        // we'll need a reset if the LedWiz unit number is changing
                        uint8_t newUnitNo = data[2] & 0x0f;
                        needReset |= (newUnitNo != cfg.d.ledWizUnitNo);
                        
                        // set the configuration parameters from the message
                        cfg.d.ledWizUnitNo = newUnitNo;
                        cfg.d.plungerEnabled = data[3] & 0x01;
                        
                        // update the status flags
                        statusFlags = (statusFlags & ~0x01) | (data[3] & 0x01);
                        
                        // if the ccd is no longer enabled, use 0 for z reports
                        if (!cfg.d.plungerEnabled)
                            z = 0;
                        
                        // save the configuration
                        cfg.save(iap, flash_addr);
                    }
#ifdef ENABLE_JOYSTICK
                    else if (data[1] == 2)
                    {
                        // 2 = Calibrate plunger
                        // (No parameters)
                        
                        // enter calibration mode
                        calBtnState = 3;
                        calBtnTimer.reset();
                        cfg.resetPlunger();
                    }
                    else if (data[1] == 3)
                    {
                        // 3 = pixel dump
                        // (No parameters)
                        reportPix = true;
                        
                        // show purple until we finish sending the report
                        ledR = 0;
                        ledB = 0;
                        ledG = 1;
                    }
#endif // ENABLE_JOYSTICK
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
                    {
                        updateWizOuts();
                        pbaIdx = 0;
                    }
                    else
                        pbaIdx += 8;
                }
            }
        }
       
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
                    cfg.resetPlunger();
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
                ledR = 1;
                ledG = 1;
                ledB = 0;
            }
            else {
                if (calBtnLed != 0)
                    calBtnLed->write(0);
                ledR = 1;
                ledG = 1;
                ledB = 1;
            }
        }
        
        // If the plunger is enabled, and we're not already in a firing event,
        // and the last plunger reading had the plunger pulled back at least
        // a bit, watch for plunger release events until it's time for our next
        // USB report.
        if (!firing && cfg.d.plungerEnabled && z >= JOYMAX/6)
        {
            // monitor the plunger until it's time for our next report
            while (reportTimer.read_ms() < 15)
            {
                // do a fast low-res scan; if it's at or past the zero point,
                // start a firing event
                if (plungerSensor.lowResScan() <= cfg.d.plungerZero)
                    firing = 1;
            }
        }

        // read the plunger sensor, if it's enabled
        if (cfg.d.plungerEnabled)
        {
            // start with the previous reading, in case we don't have a
            // clear result on this frame
            int znew = z;
            if (plungerSensor.highResScan(pos))
            {
                // We got a new reading.  If we're in calibration mode, use it
                // to figure the new calibration, otherwise adjust the new reading
                // for the established calibration.
                if (calBtnState == 3)
                {
                    // Calibration mode.  If this reading is outside of the current
                    // calibration bounds, expand the bounds.
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
                    // Not in calibration mode, so normalize the new reading to the 
                    // established calibration range.  
                    //
                    // Note that negative values are allowed.  Zero represents the
                    // "park" position, where the plunger sits when at rest.  A mechanical 
                    // plunger has a small amount of travel in the "push" direction,
                    // since the barrel spring can be compressed slightly.  Negative
                    // values represent travel in the push direction.
                    if (pos > cfg.d.plungerMax)
                        pos = cfg.d.plungerMax;
                    znew = int(round(float(pos - cfg.d.plungerZero)
                        / (cfg.d.plungerMax - cfg.d.plungerZero + 1) * JOYMAX));
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
                int pos0 = plungerSensor.lowResScan();
                int pos1 = pos0;
                Timer tw;
                tw.start();
                while (tw.read_ms() < 6)
                {
                    // read the new position
                    int pos2 = plungerSensor.lowResScan();
                    
                    // If it's stable over consecutive readings, stop looping.
                    // (Count it as stable if the position is within about 1/8".
                    // pos1 and pos2 are reported in pixels, so they range from
                    // 0 to npix.  The overall travel of a standard plunger is
                    // about 3.2", so we have (npix/3.2) pixels per inch, hence
                    // 1/8" is (npix/3.2)*(1/8) pixels.)
                    if (abs(pos2 - pos1) < int(npix/(3.2*8)))
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
                    if (pos1 < cfg.d.plungerZero
                        && abs(pos2 - pos0) > int(npix/(3.2*8)))
                    {
                        firing = 1;
                        break;
                    }
                                            
                    // the new reading is now the prior reading
                    pos1 = pos2;
                }
            }
            
            // Check for a simulated Launch Ball button press, if enabled
            if (ZBLaunchBallPort != 0)
            {
                const int cockThreshold = JOYMAX/3;
                const int pushThreshold = int(-JOYMAX/3 * LaunchBallPushDistance);
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
                const uint32_t lbButtonBit = (1 << (LaunchBallButton - 1));
                if (newState != lbState)
                {
                    // If we're entering Launch state OR we're entering the
                    // Press-and-Hold state, AND the ZB Launch Ball LedWiz signal 
                    // is turned on, simulate a Launch Ball button press.
                    if (((newState == 3 && lbState != 4) || newState == 5)
                        && wizOn[ZBLaunchBallPort-1])
                    {
                        lbBtnTimer.reset();
                        lbBtnTimer.start();
                        simButtons |= lbButtonBit;
                    }
                    
                    // if we're switching to state 0, release the button
                    if (newState == 0)
                        simButtons &= ~(1 << (LaunchBallButton - 1));
                    
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
                    if (!wizOn[ZBLaunchBallPort-1])
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

        // update the buttons
        uint32_t buttons = readButtons();

#ifdef ENABLE_JOYSTICK
        // If it's been long enough since our last USB status report,
        // send the new report.  We throttle the report rate because
        // it can overwhelm the PC side if we report too frequently.
        // VP only wants to sync with the real world in 10ms intervals,
        // so reporting more frequently only creates i/o overhead
        // without doing anything to improve the simulation.
        if (reportTimer.read_ms() > 15)
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
            int zrep = (ZBLaunchBallPort != 0 && wizOn[ZBLaunchBallPort-1] ? 0 : z);
            
            // Send the status report.  Note that we have to map the X and Y
            // axes from the accelerometer to match the Windows joystick axes.
            // The mapping is determined according to the mounting direction
            // set in config.h via the ORIENTATION_xxx macros.
            js.update(JOY_X(x,y), JOY_Y(x,y), zrep, buttons | simButtons, statusFlags);
            
            // we've just started a new report interval, so reset the timer
            reportTimer.reset();
        }

        // If we're in pixel dump mode, report all pixel exposure values
        if (reportPix)
        {
            // send the report            
            plungerSensor.sendExposureReport(js);

            // we have satisfied this request
            reportPix = false;
        }
        
#else // ENABLE_JOYSTICK
        // We're a secondary controller, with no joystick reporting.  Send
        // a generic status report to the host periodically for the sake of
        // the Windows config tool.
        if (reportTimer.read_ms() > 200)
        {
            js.updateStatus(0);
        }

#endif // ENABLE_JOYSTICK
        
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
            else if (cfg.d.plungerEnabled && !cfg.d.plungerCal)
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
