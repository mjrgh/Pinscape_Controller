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
* BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILIT Y, FITNESS FOR A PARTICULAR PURPOSE AND
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
//  - Enhanced LedWiz emulation with TLC5940 PWM controller chips.  You can attach
//    external PWM controller chips for controlling device outputs, instead of using
//    the on-board GPIO ports as described above.  The software can control a set of 
//    daisy-chained TLC5940 chips.  Each chip provides 16 PWM outputs, so you just
//    need two of them to get the full complement of 32 output ports of a real LedWiz.
//    You can hook up even more, though.  Four chips gives you 64 ports, which should
//    be plenty for nearly any virtual pinball project.  To accommodate the larger
//    supply of ports possible with the PWM chips, the controller software provides
//    a custom, extended version of the LedWiz protocol that can handle up to 128
//    ports.  PC software designed only for the real LedWiz obviously won't know
//    about the extended protocol and won't be able to take advantage of its extra
//    capabilities, but the latest version of DOF (DirectOutput Framework) *does* 
//    know the new language and can take full advantage.  Older software will still
//    work, though - the new extensions are all backward compatible, so old software
//    that only knows about the original LedWiz protocol will still work, with the
//    obvious limitation that it can only access the first 32 ports.
//
//    The Pinscape Expansion Board project (which appeared in early 2016) provides
//    a reference hardware design, with EAGLE circuit board layouts, that takes full
//    advantage of the TLC5940 capability.  It lets you create a customized set of
//    outputs with full PWM control and power handling for high-current devices
//    built in to the boards.  
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
#include "TinyDigitalIn.h"


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
float mainLoopIterTime, mainLoopIterCount;
float mainLoopMsgTime, mainLoopMsgCount;

// --------------------------------------------------------------------------
//
// Custom memory allocator.  We use our own version of malloc() for more
// efficient memory usage, and to provide diagnostics if we run out of heap.
//
// We can implement a more efficient malloc than the library can because we
// can make an assumption that the library can't: allocations are permanent.
// The normal malloc has to assume that allocations can be freed, so it has
// to track blocks individually.  For the purposes of this program, though,
// we don't have to do this because virtually all of our allocations are 
// de facto permanent.  We only allocate dyanmic memory during setup, and 
// once we set things up, we never delete anything.  This means that we can 
// allocate memory in bare blocks without any bookkeeping overhead.
//
// In addition, we can make a much larger overall pool of memory available
// in a custom allocator.  The mbed library malloc() seems to have a pool
// of about 3K to work with, even though there's really about 9K of RAM
// left over after counting the static writable data and reserving space
// for a reasonable stack.  I haven't looked at the mbed malloc to see why 
// they're so stingy, but it appears from empirical testing that we can 
// create a static array up to about 9K before things get crashy.

// Dynamic memory pool.  We'll reserve space for all dynamic 
// allocations by creating a simple C array of bytes.  The size
// of this array is the maximum number of bytes we can allocate
// with malloc or operator 'new'.
//
// The maximum safe size for this array is, in essence, the
// amount of physical KL25Z RAM left over after accounting for
// static data throughout the rest of the program, the run-time
// stack, and any other space reserved for compiler or MCU
// overhead.  Unfortunately, it's not straightforward to
// determine this analytically.  The big complication is that
// the minimum stack size isn't easily predictable, as the stack
// grows according to what the program does.  In addition, the
// mbed platform tools don't give us detailed data on the
// compiler/linker memory map.  All we get is a generic total
// RAM requirement, which doesn't necessarily account for all
// overhead (e.g., gaps inserted to get proper alignment for
// particular memory blocks).  
//
// A very rough estimate: the total RAM size reported by the 
// linker is about 3.5K (currently - that can obviously change 
// as the project evolves) out of 16K total.  Assuming about a 
// 3K stack, that leaves in the ballpark of 10K.  Empirically,
// that seems pretty close.  In testing, we start to see some
// instability at 10K, while 9K seems safe.  To be conservative,
// we'll reduce this to 8K.
//
// Our measured total usage in the base configuration (22 GPIO
// output ports, TSL1410R plunger sensor) is about 4000 bytes.
// A pretty fully decked-out configuration (121 output ports,
// with 8 TLC5940 chips and 3 74HC595 chips, plus the TSL1412R
// sensor with the higher pixel count, and all expansion board
// features enabled) comes to about 6700 bytes.  That leaves
// us with about 1.5K free out of our 8K, so we still have a 
// little more headroom for future expansion.
//
// For comparison, the standard mbed malloc() runs out of
// memory at about 6K.  That's what led to this custom malloc:
// we can just fit the base configuration into that 4K, but
// it's not enough space for more complex setups.  There's
// still a little room for squeezing out unnecessary space
// from the mbed library code, but at this point I'd prefer
// to treat that as a last resort, since it would mean having
// to fork private copies of the libraries.
static const size_t XMALLOC_POOL_SIZE = 8*1024;
static char xmalloc_pool[XMALLOC_POOL_SIZE];
static char *xmalloc_nxt = xmalloc_pool;
static size_t xmalloc_rem = XMALLOC_POOL_SIZE;
    
void *xmalloc(size_t siz)
{
    // align to a 4-byte increment
    siz = (siz + 3) & ~3;
    
    // If insufficient memory is available, halt and show a fast red/purple 
    // diagnostic flash.  We don't want to return, since we assume throughout
    // the program that all memory allocations must succeed.  Note that this
    // is generally considered bad programming practice in applications on
    // "real" computers, but for the purposes of this microcontroller app,
    // there's no point in checking for failed allocations individually
    // because there's no way to recover from them.  It's better in this 
    // context to handle failed allocations as fatal errors centrally.  We
    // can't recover from these automatically, so we have to resort to user
    // intervention, which we signal with the diagnostic LED flashes.
    if (siz > xmalloc_rem)
    {
        // halt with the diagnostic display (by looping forever)
        for (;;)
        {
            diagLED(1, 0, 0);
            wait_us(200000);
            diagLED(1, 0, 1);
            wait_us(200000);
        }
    }

    // get the next free location from the pool to return   
    char *ret = xmalloc_nxt;
    
    // advance the pool pointer and decrement the remaining size counter
    xmalloc_nxt += siz;
    xmalloc_rem -= siz;
    
    // return the allocated block
    return ret;
};

// our malloc() replacement

// Overload operator new to call our custom malloc.  This ensures that
// all 'new' allocations throughout the program (including library code)
// go through our private allocator.
void *operator new(size_t siz) { return xmalloc(siz); }
void *operator new[](size_t siz) { return xmalloc(siz); }

// Since we don't do bookkeeping to track released memory, 'delete' does
// nothing.  In actual testing, this routine appears to never be called.
// If it *is* ever called, it will simply leave the block in place, which
// will make it unavailable for re-use but will otherwise be harmless.
void operator delete(void *ptr) { }


// ---------------------------------------------------------------------------
//
// Forward declarations
//
void setNightMode(bool on);
void toggleNightMode();

// ---------------------------------------------------------------------------
// utilities

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
// Reboot timer.  When we have a deferred reboot operation pending, we
// set the target time and start the timer.
Timer2 rebootTimer;
long rebootTime_us;

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
// nothing else is going on.  State 0-1 = off, 2-3 = on
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
        b = (powerTimerDiagState >= 2);
        
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
        (diagLEDState >> 1) & 0x02);
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

// global night mode flag
static bool nightMode = false;

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



// 74HC595 interface object.  Set this up with the port assignments in
// config.h.
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


// Conversion table for 8-bit DOF level to pulse width in microseconds,
// with gamma correction.  We could use the layered gamma output on top 
// of the regular LwPwmOut class for this, but we get better precision
// with a dedicated table, because we apply gamma correction to the
// 32-bit microsecond values rather than the 8-bit DOF levels.
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

// MyPwmOut - a slight customization of the base mbed PwmOut class.  The 
// mbed version of PwmOut.write() resets the PWM cycle counter on every 
// update.  That's problematic, because the counter reset interrupts the
// cycle in progress, causing a momentary drop in brightness that's visible
// to the eye if the output is connected to an LED or other light source.
// This is especially noticeable when making gradual changes consisting of
// many updates in a short time, such as a slow fade, because the light 
// visibly flickers on every step of the transition.  This customized 
// version removes the cycle reset, which makes for glitch-free updates 
// and nice smooth fades.
//
// Initially, I thought the counter reset in the mbed code was simply a
// bug.  According to the KL25Z hardware reference, you update the duty
// cycle by writing to the "compare values" (CvN) register.  There's no
// hint that you should reset the cycle counter, and indeed, the hardware
// goes out of its way to allow updates mid-cycle (as we'll see shortly).
// They went to lengths specifically so that you *don't* have to reset
// that counter.  And there's no comment in the mbed code explaining the
// cycle reset, so it looked to me like something that must have been
// added by someone who didn't read the manual carefully enough and didn't
// test the result thoroughly enough to find the glitch it causes.
//
// After some experimentation, though, I've come to think the code was
// added intentionally, as a workaround for a rather nasty KL25Z hardware
// bug.   Whoever wrote the code didn't add any comments explaning why it's
// there, so we can't know for sure, but it does happen to work around the 
// bug, so it's a good bet the original programmer found the same hardware
// problem and came up with the counter reset as an imperfect solution.
//
// We'll get to the KL25Z hardware bug shortly, but first we need to look at
// how the hardware is *supposed* to work.  The KL25Z is *supposed* to make
// it super easy for software to do glitch-free updates of the duty cycle of 
// a PWM channel.  With PWM hardware in general, you have to be careful to
// update the duty cycle counter between grayscale cycles, beacuse otherwise
// you might interrupt the cycle in progress and cause a brightness glitch.  
// The KL25Z TPM simplifies this with a "staging" register for the duty
// cycle counter.  At the end of each cycle, the TPM moves the value from
// the staging register into its internal register that actually controls 
// the duty cycle.  The idea is that the software can write a new value to
// the staging register at any time, and the hardware will take care of
// synchronizing the actual internal update with the grayscale cycle.  In
// principle, this frees the software of any special timing considerations
// for PWM updates.  
//
// Now for the bug.  The staging register works as advertised, except for
// one little detail: it seems to be implemented as a one-element queue
// that won't accept a new write until the existing value has been read.
// The read only happens at the start of the new cycle.  So the effect is
// that we can only write one update per cycle.  Any writes after the first
// are simply dropped, lost forever.  That causes even worse problems than
// the original glitch.  For example, if we're doing a fade-out, the last
// couple of updates in the fade might get lost, leaving the output slightly
// on at the end, when it's supposed to be completely off.
//
// The mbed workaround of resetting the cycle counter fixes the lost-update
// problem, but it causes the constant glitching during fades.  So we need
// a third way that works around the hardware problem without causing 
// update glitches.
//
// Here's my solution: we basically implement our own staging register,
// using the same principle as the hardware staging register, but hopefully
// with an implementation that actually works!  First, when we update a PWM 
// output, we won't actually write the value to the hardware register.
// Instead, we'll just stash it internally, effectively in our own staging
// register (but actually just a member variable of this object).  Then
// we'll periodically transfer these staged updates to the actual hardware 
// registers, being careful to do this no more than once per PWM cycle.
// One way to do this would be to use an interrupt handler that fires at
// the end of the PWM cycle, but that would be fairly complex because we
// have many (up to 10) PWM channels.  Instead, we'll just use polling:
// we'll call a routine periodically in our main loop, and we'll transfer
// updates for all of the channels that have been updated since the last
// pass.  We can get away with this simple polling approach because the
// hardware design *partially* works: it does manage to free us from the
// need to synchronize updates with the exact end of a PWM cycle.  As long
// as we do no more than one write per cycle, we're golden.  That's easy
// to accomplish, too: all we need to do is make sure that our polling
// interval is slightly longer than the PWM period.  That ensures that
// we can never have two updates during one PWM cycle.  It does mean that
// we might have zero updates on some cycles, causing a one-cycle delay
// before an update is actually put into effect, but that shouldn't ever
// be noticeable since the cycles are so short.  Specifically, we'll use
// the mbed default 20ms PWM period, and we'll do our update polling 
// every 25ms.
class LessGlitchyPwmOut: public PwmOut
{
public:
    LessGlitchyPwmOut(PinName pin) : PwmOut(pin) { }
    
    void write(float value)
    {
        // Update the counter without resetting the counter.
        //
        // NB: this causes problems if there are multiple writes in one
        // PWM cycle: the first write will be applied and later writes 
        // during the same cycle will be lost.  Callers must take care
        // to limit writes to one per cycle.
        *_pwm.CnV = uint32_t((*_pwm.MOD + 1) * value);
    }
};


// Collection of PwmOut objects to update on each polling cycle.  The
// KL25Z has 10 physical PWM channels, so we need at most 10 polled outputs.
static int numPolledPwm;
static class LwPwmOut *polledPwm[10];

// LwOut class for a PWM-capable GPIO port.
class LwPwmOut: public LwOut
{
public:
    LwPwmOut(PinName pin, uint8_t initVal) : p(pin)
    {
         // set the cycle time to 20ms
         p.period_ms(20);
         
         // add myself to the list of polled outputs for periodic updates
         if (numPolledPwm < countof(polledPwm))
            polledPwm[numPolledPwm++] = this;
         
         // set the initial value, and an explicitly different previous value
         prv = ~initVal;
         set(initVal);
    }

    virtual void set(uint8_t val) 
    {
        // on set, just save the value for a later 'commit' 
        this->val = val;
    }

    // handle periodic update polling
    void poll()
    {
        // if the value has changed, commit it
        if (val != prv)
        {
            prv = val;
            commit(val);
        }
    }

protected:
    virtual void commit(uint8_t v)
    {
        // write the current value to the PWM controller if it's changed
        p.write(dof_to_pwm[v]);
    }
    
    LessGlitchyPwmOut p;
    uint8_t val, prv;
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
    virtual void commit(uint8_t v)
    {
        // write the current value to the PWM controller if it's changed
        p.write(dof_to_gamma_pwm[v]);
    }
};

// poll the PWM outputs
Timer polledPwmTimer;
float polledPwmTotalTime, polledPwmRunCount;
void pollPwmUpdates()
{
    // if it's been at least 25ms since the last update, do another update
    if (polledPwmTimer.read_us() >= 25000)
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
          polledPwmTotalTime += t.read();
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

// on/off state for each LedWiz output
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

// LedWiz cycle counters.  These must be updated before calling wizState().
static uint8_t wizFlashCounter[MAX_LW_BANKS];


// Current absolute brightness levels for all outputs.  These are
// DOF brightness level value, from 0 for fully off to 255 for fully
// on.  These are always used for extended ports (33 and above), and
// are used for LedWiz ports (1-32) when we're in extended protocol
// mode (i.e., ledWizMode == false).
static uint8_t *outLevel;

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

// LedWiz/Extended protocol mode.
//
// We implement output port control using both the legacy LedWiz
// protocol and a private extended protocol (which is 100% backwards
// compatible with the LedWiz protocol: we recognize all valid legacy
// protocol commands and handle them the same way a real LedWiz does).
//
// The legacy LedWiz protocol has only two message types, which
// set output port states for a fixed set of 32 outputs.  One message
// sets the "switch" state (on/off) of the ports, and the other sets
// the "profile" state (brightness or flash pattern).  The two states
// are stored independently, so turning a port off via the switch state
// doesn't forget or change its brightness: turning it back on will
// restore the same brightness or flash pattern as before.  The "profile"
// state can be a brightness level from 1 to 49, or one of four flash
// patterns, identified by a value from 129 to 132.  The flash pattern
// and brightness levels are mutually exclusive, since the single
// "profile" setting per port selects which is used.
//
// The extended protocol discards the flash pattern options and instead
// uses the full byte range 0..255 for brightness levels.  Modern clients
// based on DOF don't use the flash patterns, since DOF simply sends
// the individual brightness updates when it wants to create fades or 
// flashes.  What we gain by dropping the flash options is finer 
// gradations of brightness - 256 levels rather than the LedWiz's 48.
// This makes for noticeably smoother fades and a wider gamut for RGB
// color mixing.  The extended protocol also drops the LedWiz notion of 
// separate "switch" and "profile" settings, and instead combines the 
// two into the single brightness setting, with brightness 0 meaning off.
// This also is the way DOF thinks about the problem, so it's a better 
// match to modern clients.  
//
// To reconcile the different approaches in the two protocols to setting 
// output port states, we use a global mode: LedWiz mode or Pinscape mode.
// Whenever an output port message is received, we switch this flag to the
// mode of the message.  The assumption is that only one client at a time
// will be manipulating output ports, and that any given client uses one
// protocol exclusively.  There's no reason a client should mix the
// protocols; if a client is aware of the Pinscape protocol at all, it
// should use it exclusively.
static uint8_t ledWizMode = true;

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

// Translate an LedWiz output (ports 1-32) to a DOF brightness level.
// Note: update all wizFlashCounter[] entries before calling this to
// ensure that we're at the right place in each flash cycle.
//
// Important: the caller must update the wizFlashCounter[] array before
// calling this.  We leave it to the caller to update the array rather
// than doing it here, because each set of 32 outputs shares the same
// counter entry.
static uint8_t wizState(int idx)
{
    // If we're in extended protocol mode, ignore the LedWiz setting
    // for the port and use the new protocol setting instead.
    if (!ledWizMode)
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
        // compatible with the actual LedWiz, but it makes for such a
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
    else if (val >= 129 && val <= 132)
    {
        // flash mode - get the current counter for the bank, and look
        // up the current position in the cycle for the mode
        const int c = wizFlashCounter[idx/32];
        return wizFlashLookup[((val-129)*256) + c];
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

// LedWiz flash cycle timer.  This runs continuously.  On each update,
// we use this to figure out where we are on the cycle for each bank.
Timer wizCycleTimer;

// Update the LedWiz flash cycle counters
static void updateWizCycleCounts()
{
    // Update the LedWiz flash cycle positions.  Each cycle is 2/N
    // seconds long, where N is the speed setting for the bank.  N
    // ranges from 1 to 7.
    //
    // Note that we treat the microsecond clock as a 32-bit unsigned
    // int.  This rolls over (i.e., exceeds 0xffffffff) every 71 minutes.
    // We only care about the phase of the current LedWiz cycle, so we
    // don't actually care about the absolute time - we only care about
    // the time relative to some arbitrary starting point.  Whenever the
    // clock rolls over, it effectively sets a new starting point; since
    // we only need an arbitrary starting point, that's largely okay.
    // The one drawback is that these epoch resets can obviously occur
    // in the middle of a cycle.  When this occurs, the update just before
    // the rollover and the update just after the rollover will use
    // different epochs, so their phases might be misaligned.  That could
    // cause a sudden jump in brightness between the two updates and a 
    // shorter-than-usual or longer-than-usual time for that cycle.  To
    // avoid that, we'd have to use a higher-precision clock (say, a 64-bit
    // microsecond counter) and do all of the calculations at the higher
    // precision.  Given that the rollover only happens once every 71
    // minutes, and that the only problem it causes is a momentary glitch
    // in the flash pattern, I think it's an equitable trade for the slightly
    // faster processing in the 32-bit domain.  This routine is called 
    // frequently from the main loop, so it's critial to minimize execution
    // time.
    uint32_t tcur = wizCycleTimer.read_us();
    for (int i = 0 ; i < MAX_LW_BANKS ; ++i)
    {
        // Figure the point in the cycle.  The LedWiz "speed" setting is
        // waveform period in 0.25s units.  (There's no official LedWiz
        // documentation of what the speed means in real units, so this is
        // based on observations.)
        //
        // We do this calculation frequently from the main loop, since we
        // have to do it every time we update the output flash cycles,
        // which in turn has to be done frequently to make the cycles
        // appear smooth to users.  So we're going to get a bit tricky
        // with integer arithmetic to streamline it.  The goal is to find
        // the current phase position in the output waveform; in abstract
        // terms, we're trying to find the angle, 0 to 2*pi, in the current
        // cycle.  Floating point arithmetic is expensive on the KL25Z
        // since it's all done in software, so we'll do everything in
        // integers.  To do that, rather than trying to find the phase
        // angle as a continuous quantity, we'll quantize it, into 256
        // quanta per cycle.  Each quantum is 1/256 of the cycle length,
        // so for a 1-second cycle (LedWiz speed 4), each quantum is
        // 1/256 of second or about 3.9ms.  To find the phase, then, we
        // simply take the current time (as an elapsed time from an
        // arbitrary zero point aka epoch), quantize it into 3.9ms chunks,
        // and calculate the remainder mod 256.  Remainder mod 256 is a
        // fast operation since it's equivalent to bit masking with 0xFF.
        // (That's why we chose a power of two for the number of quanta
        // per cycle.)  Our timer gives us microseconds since it started,
        // so to convert to quanta, we divide by microseconds per quantum;
        // in the case of speed 1 with its 3.906ms quanta, we divide by 
        // 3906.  But we can take this one step further, getting really
        // tricky now.  Dividing by N is the same as muliplying by X/N
        // for some X, and then dividing the result by X.  Why, you ask,
        // would we want to do two operations where we could do one?
        // Because if we're clever, the two operations will be much 
        // faster the the one.  The M0+ has no DIVIDE instruction, so
        // integer division has to be done in software, at a cost of about
        // 100 clocks per operation.  The KL25Z M0+ has a one-cycle
        // hardware multiplier, though.  But doesn't that leave that
        // second division still to do?  Yes, but if we choose a power
        // of 2 for X, we can do that division with a bit shift, another
        // single-cycle operation.  So we can do the division in two
        // cycles by breaking it up into a multiply + shift.
        //
        // Each entry in this array represents X/N for the corresponding
        // LedWiz speed, where N is the number of time quanta per cycle
        // and X is 2^24.  The time quanta are chosen such that 256
        // quanta add up to approximately (LedWiz speed setting * 0.25s).
        // 
        // Note that the calculation has an implicit bit mask (result & 0xFF)
        // to get the final result mod 256.  But we don't have to actually
        // do that work because we're using 32-bit ints and a 2^24 fixed
        // point base (X in the narrative above).  The final shift right by
        // 24 bits to divide out the base will leave us with only 8 bits in
        // the result, since we started with 32.
#if 1
        static const uint32_t inv_us_per_quantum[] = { // indexed by LedWiz speed
            0, 17172, 8590, 5726, 4295, 3436, 2863, 2454
        };
        wizFlashCounter[i] = ((tcur * inv_us_per_quantum[wizSpeed[i]]) >> 24);
#else
        // Old, slightly less tricky way: this is almost the same as
        // above, but does the division the straightforward way.  The
        // array gives us the length of the quantum per microsecond for
        // each speed setting, so we just divide the microsecond counter
        // by the quantum size to get the current time in quantum units,
        // then figure the remainder mod 256 of the result to get the 
        // current cycle phase position.
        static const uint32_t us_per_quantum[] = {  // indexed by LedWiz "speed"
            0, 977, 1953, 2930, 3906, 4883, 5859, 6836
        };
        wizFlashCounter[i] = (tcur/us_per_quantum[wizSpeed[i]]) & 0xFF;
#endif
    }
}

// LedWiz flash timer pulse.  The main loop calls this periodically
// to update outputs set to LedWiz flash modes.
Timer wizPulseTimer;
float wizPulseTotalTime, wizPulseRunCount;
const uint32_t WIZ_INTERVAL_US = 8000;
static void wizPulse()
{
    // if it's been long enough, update the LedWiz outputs
    if (wizPulseTimer.read_us() >= WIZ_INTERVAL_US)
    {
        // reset the timer for the next round
        wizPulseTimer.reset();

        // if we're in LedWiz mode, update flashing outputs
        if (ledWizMode)
        {
            // start a timer for statistics collection
            IF_DIAG(
              Timer t;
              t.start();
            )
            
            // update the cycle counters
            updateWizCycleCounts();

            // update all outputs set to flashing values
            for (int i = numOutputs ; i > 0 ; )
            {
                if (wizOn[--i])
                {
                    // If the "brightness" is in the range 129..132, it's a 
                    // flash mode.  Note that we only have to check the high
                    // bit here, because the protocol message handler validates
                    // the wizVal[] entries when storing them: the only valid
                    // values with the high bit set are 129..132.  Skipping
                    // validation here saves us a tiny bit of work, which we
                    // care about because we have to loop over all outputs
                    // here, and we invoke this frequently from the main loop.
                    const uint8_t val = wizVal[i];
                    if ((val & 0x80) != 0)
                    {
                        // get the current cycle time, then look up the 
                        // value for the mode at the cycle time
                        const int c = wizFlashCounter[i >> 5];
                        lwPin[i]->set(wizFlashLookup[((val-129) << 8) + c]);
                    }
                }
            }
            
            // flush changes to 74HC595 chips, if attached
            if (hc595 != 0)
                hc595->update();

            // collect timing statistics
            IF_DIAG(
              wizPulseTotalTime += t.read();
              wizPulseRunCount += 1;
            )
        }
    }    
}

// Update the physical outputs connected to the LedWiz ports.  This is 
// called after any update from an LedWiz protocol message.
static void updateWizOuts()
{
    // update the cycle counters
    updateWizCycleCounts();
    
    // update each output
    for (int i = 0 ; i < numOutputs ; ++i)
        lwPin[i]->set(wizState(i));
    
    // flush changes to 74HC595 chips, if attached
    if (hc595 != 0)
        hc595->update();
}

// Update all physical outputs.  This is called after a change to a global
// setting that affects all outputs, such as engaging or canceling Night Mode.
static void updateAllOuts()
{
    // update LedWiz states
    updateWizOuts();
}

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
        
    // revert to LedWiz mode for output controls
    ledWizMode = true;
    
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
    // switch to LedWiz protocol mode
    ledWizMode = true;
    
    // update all on/off states
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
        wizOn[port] = ((data[imsg] & bit) != 0);
    }
    
    // set the flash speed for the port group
    if (portGroup < countof(wizSpeed))
        wizSpeed[portGroup] = (data[5] < 1 ? 1 : data[5] > 7 ? 7 : data[5]);

    // update the physical outputs with the new LedWiz states
    updateWizOuts();
}

// Carry out a PBA or PBX message.
void pba_pbx(int basePort, const uint8_t *data)
{
    // switch LedWiz protocol mode
    ledWizMode = true;

    // update each wizVal entry from the brightness data
    for (int i = 0, iwiz = basePort ; i < 8 && iwiz < numOutputs ; ++i, ++iwiz)
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
        wizVal[iwiz] = v;
    }

    // update the physical outputs
    updateWizOuts();
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

    // previous logical on/off state, when keys were last processed for USB 
    // reports and local effects
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
    // option brdiges this gap by generating a toggle key event each time
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
    uint8_t state : 2;          // current shift state:
                                //   0 = not shifted
                                //   1 = shift button down, no key pressed yet
                                //   2 = shift button down, key pressed
    uint8_t pulse : 1;          // sending pulsed keystroke on release
    uint32_t pulseTime;         // time of start of pulsed keystroke
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

// button scan interrupt ticker
Ticker buttonTicker;

// Button scan interrupt handler.  We call this periodically via
// a timer interrupt to scan the physical button states.  
void scanButtons()
{
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
    // presume we'll find no keyboard keys
    kbKeys = false;
    
    // presume no shift key
    shiftButton.index = -1;
    
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
            if (cfg.shiftButton == i+1)
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
    buttonTicker.attach_us(scanButtons, 1000);

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


// Process the button state.  This sets up the joystick, keyboard, and
// media control descriptors with the current state of keys mapped to
// those HID interfaces, and executes the local effects for any keys 
// mapped to special device functions (e.g., Night Mode).
void processButtons(Config &cfg)
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
    uint32_t dt = buttonTimer.read_us();
    buttonTimer.reset();
    
    // check the shift button state
    if (shiftButton.index != -1)
    {
        ButtonState *sbs = &buttonState[shiftButton.index];
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
                shiftButton.state = 0;
                shiftButton.pulse = 1;
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
        }
    }

    // scan the button list
    ButtonState *bs = buttonState;
    for (int i = 0 ; i < nButtons ; ++i, ++bs)
    {
        // Check the button type:
        //   - shift button
        //   - pulsed button
        //   - regular button
        if (shiftButton.index == i)
        {
            // This is the shift button.  Its logical state for key
            // reporting purposes is controlled by the shift buttton
            // pulse timer.  If we're in a pulse, its logical state
            // is pressed.
            if (shiftButton.pulse)
            {
                // deduct the current interval from the pulse time, ending
                // the pulse if the time has expired
                if (shiftButton.pulseTime > dt)
                    shiftButton.pulseTime -= dt;
                else
                    shiftButton.pulse = 0;
            }
            
            // the button is logically pressed if we're in a pulse
            bs->logState = shiftButton.pulse;
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

        // carry out any edge effects from buttons changing states
        if (bs->logState != bs->prevLogState)
        {
            // check for special key transitions
            if (cfg.nightMode.btn == i + 1)
            {
                // Check the switch type in the config flags.  If flag 0x01 is set,
                // it's a persistent on/off switch, so the night mode state simply
                // follows the current state of the switch.  Otherwise, it's a 
                // momentary button, so each button push (i.e., each transition from
                // logical state OFF to ON) toggles the current night mode state.
                if (cfg.nightMode.flags & 0x01)
                {
                    // on/off switch - when the button changes state, change
                    // night mode to match the new state
                    setNightMode(bs->logState);
                }
                else
                {
                    // Momentary switch - toggle the night mode state when the
                    // physical button is pushed (i.e., when its logical state
                    // transitions from OFF to ON).  
                    //
                    // In momentary mode, night mode flag 0x02 makes it the
                    // shifted version of the button.  In this case, only
                    // proceed if the shift button is pressed.
                    bool pressed = bs->logState;
                    if ((cfg.nightMode.flags & 0x02) != 0)
                    {
                        // if the shift button is pressed but hasn't been used
                        // as a shift yet, mark it as used, so that it doesn't
                        // also generate its own key code on release
                        if (shiftButton.state == 1)
                            shiftButton.state = 2;
                            
                        // if the shift button isn't even pressed
                        if (shiftButton.state == 0)
                            pressed = false;
                    }
                    
                    // if it's pressed (even after considering the shift mode),
                    // toggle night mode
                    if (pressed)
                        toggleNightMode();
                }
            }
            
            // remember the new state for comparison on the next run
            bs->prevLogState = bs->logState;
        }

        // if it's pressed, physically or virtually, add it to the appropriate 
        // key state list
        if (bs->logState || bs->virtState)
        {
            // Get the key type and code.  Start by assuming that we're
            // going to use the normal unshifted meaning.
            ButtonCfg *bc = &cfg.button[bs->cfgIndex];
            uint8_t typ = bc->typ;
            uint8_t val = bc->val;
            
            // If the shift button is down, check for a shifted meaning.
            if (shiftButton.state)
            {
                // assume there's no shifted meaning
                bool useShift = false;
                
                // If the button has a shifted meaning, use that.  The
                // meaning might be a keyboard key or joystick button,
                // but it could also be as the Night Mode toggle.
                //
                // The condition to check if it's the Night Mode toggle
                // is a little complicated.  First, the easy part: our
                // button index has to match the Night Mode button index.
                // Now the hard part: the Night Mode button flags have
                // to be set to 0x01 OFF and 0x02 ON: toggle mode (not
                // switch mode, 0x01), and shift mode, 0x02.  So AND the
                // flags with 0x03 to get these two bits, and check that
                // the result is 0x02, meaning that only shift mode is on.
                if (bc->typ2 != BtnTypeNone)
                {
                    // there's a shifted key assignment - use it
                    typ = bc->typ2;
                    val = bc->val2;
                    useShift = true;
                }
                else if (cfg.nightMode.btn == i+1 
                         && (cfg.nightMode.flags & 0x03) == 0x02)
                {
                    // shift+button = night mode toggle
                    typ = BtnTypeNone;
                    val = 0;
                    useShift = true;
                }
                
                // If there's a shifted meaning, advance the shift
                // button state from 1 to 2 if applicable.  This signals
                // that we've "consumed" the shift button press as the
                // shift button, so it shouldn't generate its own key
                // code event when released.
                if (useShift && shiftButton.state == 1)
                    shiftButton.state = 2;
            }
            
            // We've decided on the meaning of the button, so process
            // the keyboard or joystick event.
            switch (typ)
            {
            case BtnTypeJoystick:
                // joystick button
                newjs |= (1 << (val - 1));
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
                        for (int j = 0 ; j < nkeys ; ++j)
                        {
                            if (keys[j] == val)
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
        bool waitForConnect, bool enableJoystick, bool useKB) 
        : USBJoystick(vendor_id, product_id, product_release, waitForConnect, enableJoystick, useKB)
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
// We install an interrupt handler on the accelerometer "data ready" 
// interrupt to ensure that we fetch each sample immediately when it
// becomes available.  The accelerometer data rate is fairly high
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
         int dtus = tGet_.read_us();
         tGet_.reset();
         
         // done manipulating the shared data
         __enable_irq();
         
         // adjust the readings for the integration time
         float dt = dtus/1000000.0f;
         vx /= dt;
         vy /= dt;
         
         // add this sample to the current calibration interval's running total
         AccHist *p = accPrv_ + iAccPrv_;
         p->addAvg(ax, ay);

         // check for auto-centering every so often
         if (tCenter_.read_us() > 1000000)
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
uint8_t psu2_state = 1;

// TV relay state.  The TV relay can be controlled by the power-on
// timer and directly from the PC (via USB commands), so keep a
// separate state for each:
//
//   0x01 -> turned on by power-on timer
//   0x02 -> turned on by USB command
uint8_t tv_relay_state = 0x00;
const uint8_t TV_RELAY_POWERON = 0x01;
const uint8_t TV_RELAY_USB     = 0x02;

// TV ON switch relay control
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
            
            // start the power timer diagnostic flashes
            powerTimerDiagState = 2;
            diagLED();
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
            tvRelayUpdate(TV_RELAY_POWERON, true);
            psu2_state = 5;
        }
        
        // flash the power time diagnostic every two interrupts
        powerTimerDiagState = (powerTimerDiagState + 1) & 0x03;
        diagLED();
        break;
        
    case 5:
        // TV timer relay on.  We pulse this for one interval, so
        // it's now time to turn it off and return to the default state.
        tvRelayUpdate(TV_RELAY_POWERON, false);
        psu2_state = 1;
        
        // done with the diagnostic flashes
        powerTimerDiagState = 0;
        diagLED();
        break;
    }
}

// Start the TV ON checker.  If the status sense circuit is enabled in
// the configuration, we'll set up the pin connections and start the
// interrupt handler that periodically checks the status.  Does nothing
// if any of the pins are configured as NC.
void startTVTimer(Config &cfg)
{
    // only start the timer if the pins are configured and the delay
    // time is nonzero
    if (cfg.TVON.delayTime != 0
        && cfg.TVON.statusPin != 0xFF 
        && cfg.TVON.latchPin != 0xFF 
        && cfg.TVON.relayPin != 0xFF)
    {
        psu2_status_sense = new DigitalIn(wirePinName(cfg.TVON.statusPin));
        psu2_status_set = new DigitalOut(wirePinName(cfg.TVON.latchPin));
        tv_relay = new DigitalOut(wirePinName(cfg.TVON.relayPin));
        tv_delay_time = cfg.TVON.delayTime/100.0f;
    
        // Set up our time routine to run every 1/4 second.  
        tv_ticker.attach(&TVTimerInt, 0.25);
    }
}

// TV relay manual control timer.  This lets us pulse the TV relay
// under manual control, separately from the TV ON timer.
Ticker tv_manualTicker;
void TVManualInt()
{
    tv_manualTicker.detach();
    tvRelayUpdate(TV_RELAY_USB, false);
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
        // Pulse the relay.  Turn it on, then set our timer for 250ms.
        tvRelayUpdate(TV_RELAY_USB, true);
        tv_manualTicker.attach(&TVManualInt, 0.25);
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
// Pixel dump mode - the host requested a dump of image sensor pixels
// (helpful for installing and setting up the sensor and light source)
//
bool reportPlungerStat = false;
uint8_t reportPlungerStatFlags; // plunger pixel report flag bits (see ccdSensor.h)
uint8_t reportPlungerStatTime;  // extra exposure time for plunger pixel report



// ---------------------------------------------------------------------------
//
// Night mode setting updates
//

// Turn night mode on or off
static void setNightMode(bool on)
{
    // set the new night mode flag in the noisy output class
    nightMode = on;
    
    // update the special output pin that shows the night mode state
    int port = int(cfg.nightMode.port) - 1;
    if (port >= 0 && port < numOutputs)
        lwPin[port]->set(nightMode ? 255 : 0);

    // update all outputs for the mode change
    updateAllOuts();
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
    case PlungerType_TSL1410RS:
        // TSL1410R, serial mode (all pixels read in one file)
        // pins are: SI, CLOCK, AO
        plungerSensor = new PlungerSensorTSL1410R(
            wirePinName(cfg.plunger.sensorPin[0]), 
            wirePinName(cfg.plunger.sensorPin[1]),
            wirePinName(cfg.plunger.sensorPin[2]),
            NC);
        break;
        
    case PlungerType_TSL1410RP:
        // TSL1410R, parallel mode (each half-sensor's pixels read separately)
        // pins are: SI, CLOCK, AO1, AO2
        plungerSensor = new PlungerSensorTSL1410R(
            wirePinName(cfg.plunger.sensorPin[0]), 
            wirePinName(cfg.plunger.sensorPin[1]),
            wirePinName(cfg.plunger.sensorPin[2]), 
            wirePinName(cfg.plunger.sensorPin[3]));
        break;
        
    case PlungerType_TSL1412SS:
        // TSL1412S, serial mode
        // pins are: SI, CLOCK, AO1, AO2
        plungerSensor = new PlungerSensorTSL1412R(
            wirePinName(cfg.plunger.sensorPin[0]),
            wirePinName(cfg.plunger.sensorPin[1]), 
            wirePinName(cfg.plunger.sensorPin[2]), 
            NC);
        break;
    
    case PlungerType_TSL1412SP:
        // TSL1412S, parallel mode
        // pins are: SI, CLOCK, AO1, AO2
        plungerSensor = new PlungerSensorTSL1412R(
            wirePinName(cfg.plunger.sensorPin[0]), 
            wirePinName(cfg.plunger.sensorPin[1]), 
            wirePinName(cfg.plunger.sensorPin[2]), 
            wirePinName(cfg.plunger.sensorPin[3]));
        break;
    
    case PlungerType_Pot:
        // pins are: AO
        plungerSensor = new PlungerSensorPot(
            wirePinName(cfg.plunger.sensorPin[0]));
        break;
    
    case PlungerType_None:
    default:
        plungerSensor = new PlungerSensorNull();
        break;
    }
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

        // no history yet
        histIdx = 0;
        
        // initialize the filter
        initFilter();
    }

    // Collect a reading from the plunger sensor.  The main loop calls
    // this frequently to read the current raw position data from the
    // sensor.  We analyze the raw data to produce the calibrated
    // position that we report to the PC via the joystick interface.
    void read()
    {
        // Read a sample from the sensor
        PlungerReading r;
        if (plungerSensor->read(r))
        {
            // filter the raw sensor reading
            applyPreFilter(r);
            
            // Pull the previous reading from the history
            const PlungerReading &prv = nthHist(0);
            
            // If the new reading is within 1ms of the previous reading,
            // ignore it.  We require a minimum time between samples to
            // ensure that we have a usable amount of precision in the
            // denominator (the time interval) for calculating the plunger
            // velocity.  The CCD sensor hardware takes about 2.5ms to
            // read, so it will never be affected by this, but other sensor
            // types don't all have the same hardware cycle time, so we need
            // to throttle them artificially.  E.g., the potentiometer only
            // needs one ADC sample per reading, which only takes about 15us.
            // We don't need to check which sensor type we have here; we
            // just ignore readings until the minimum interval has passed,
            // so if the sensor is already slower than this, we'll end up
            // using all of its readings.
            if (uint32_t(r.t - prv.t) < 1000UL)
                return;

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
                r.pos = int(
                    (long(r.pos - cfg.plunger.cal.zero) * JOYMAX)
                    / (cfg.plunger.cal.max - cfg.plunger.cal.zero));
                    
                // limit the result to the valid joystick range
                if (r.pos > JOYMAX)
                    r.pos = JOYMAX;
                else if (r.pos < -JOYMAX)
                    r.pos = -JOYMAX;
            }

            // Calculate the velocity from the second-to-last reading
            // to here, in joystick distance units per microsecond.
            // Note that we use the second-to-last reading rather than
            // the very last reading to give ourselves a little longer
            // time base.  The time base is so short between consecutive
            // readings that the error bars in the position would be too
            // large.
            //
            // For reference, the physical plunger velocity ranges up
            // to about 100,000 joystick distance units/sec.  This is 
            // based on empirical measurements.  The typical time for 
            // a real plunger to travel the full distance when released 
            // from full retraction is about 85ms, so the average velocity 
            // covering this distance is about 56,000 units/sec.  The 
            // peak is probably about twice that.  In real-world units, 
            // this translates to an average speed of about .75 m/s and 
            // a peak of about 1.5 m/s.
            //
            // Note that we actually calculate the value here in units
            // per *microsecond* - the discussion above is in terms of
            // units/sec because that's more on a human scale.  Our
            // choice of internal units here really isn't important,
            // since we only use the velocity for comparison purposes,
            // to detect acceleration trends.  We therefore save ourselves
            // a little CPU time by using the natural units of our inputs.
            const PlungerReading &prv2 = nthHist(1);
            float v = float(r.pos - prv2.pos)/float(r.t - prv2.t);
            
            // presume we'll report the latest instantaneous reading
            z = r.pos;
            vz = v;
            
            // Check firing events
            switch (firing)
            {
            case 0:
                // Default state - not in a firing event.  
                
                // If we have forward motion from a position that's retracted 
                // beyond a threshold, enter phase 1.  If we're not pulled back
                // far enough, don't bother with this, as a release wouldn't
                // be strong enough to require the synthetic firing treatment.
                if (v < 0 && r.pos > JOYMAX/6)
                {
                    // enter firing phase 1
                    firingMode(1);
                    
                    // if in calibration state 1 (at rest), switch to state 2 (not 
                    // at rest)
                    if (calState == 1)
                        calState = 2;
                    
                    // we don't have a freeze position yet, but note the start time
                    f1.pos = 0;
                    f1.t = r.t;
                    
                    // Figure the barrel spring "bounce" position in case we complete 
                    // the firing event.  This is the amount that the forward momentum
                    // of the plunger will compress the barrel spring at the peak of
                    // the forward travel during the release.  Assume that this is
                    // linearly proportional to the starting retraction distance.  
                    // The barrel spring is about 1/6 the length of the main spring, 
                    // so figure it compresses by 1/6 the distance.  (This is overly
                    // simplistic and not very accurate, but it seems to give good 
                    // visual results, and that's all it's for.)
                    f2.pos = -r.pos/6;
                }
                break;
                
            case 1:
                // Phase 1 - acceleration.  If we cross the zero point, trigger
                // the firing event.  Otherwise, continue monitoring as long as we
                // see acceleration in the forward direction.
                if (r.pos <= 0)
                {
                    // switch to the synthetic firing mode
                    firingMode(2);
                    z = f2.pos;
                    
                    // note the start time for the firing phase
                    f2.t = r.t;
                    
                    // if in calibration mode, and we're in state 2 (moving), 
                    // collect firing statistics for calibration purposes
                    if (plungerCalMode && calState == 2)
                    {
                        // collect a new zero point for the average when we 
                        // come to rest
                        calState = 0;
                        
                        // collect average firing time statistics in millseconds, if 
                        // it's in range (20 to 255 ms)
                        int dt = uint32_t(r.t - f1.t)/1000UL;
                        if (dt >= 20 && dt <= 255)
                        {
                            calRlsTimeSum += dt;
                            calRlsTimeN += 1;
                            cfg.plunger.cal.tRelease = uint8_t(calRlsTimeSum / calRlsTimeN);
                        }
                    }
                }
                else if (v < vprv2)
                {
                    // We're still accelerating, and we haven't crossed the zero
                    // point yet - stay in phase 1.  (Note that forward motion is
                    // negative velocity, so accelerating means that the new 
                    // velocity is more negative than the previous one, which
                    // is to say numerically less than - that's why the test
                    // for acceleration is the seemingly backwards 'v < vprv'.)

                    // If we've been accelerating for at least 20ms, we're probably
                    // really doing a release.  Jump back to the recent local
                    // maximum where the release *really* started.  This is always
                    // a bit before we started seeing sustained accleration, because
                    // the plunger motion for the first few milliseconds is too slow
                    // for our sensor precision to reliably detect acceleration.
                    if (f1.pos != 0)
                    {
                        // we have a reset point - freeze there
                        z = f1.pos;
                    }
                    else if (uint32_t(r.t - f1.t) >= 20000UL)
                    {
                        // it's been long enough - set a reset point.
                        f1.pos = z = histLocalMax(r.t, 50000UL);
                    }
                }
                else
                {
                    // We're not accelerating.  Cancel the firing event.
                    firingMode(0);
                    calState = 1;
                }
                break;
                
            case 2:
                // Phase 2 - start of synthetic firing event.  Report the fake
                // bounce for 25ms.  VP polls the joystick about every 10ms, so 
                // this should be enough time to guarantee that VP sees this
                // report at least once.
                if (uint32_t(r.t - f2.t) < 25000UL)
                {
                    // report the bounce position
                    z = f2.pos;
                }
                else
                {
                    // it's been long enough - switch to phase 3, where we
                    // report the park position until the real plunger comes
                    // to rest
                    firingMode(3);
                    z = 0;
                    
                    // set the start of the "stability window" to the rest position
                    f3s.t = r.t;
                    f3s.pos = 0;
                    
                    // set the start of the "retraction window" to the actual position
                    f3r = r;
                }
                break;
                
            case 3:
                // Phase 3 - in synthetic firing event.  Report the park position
                // until the plunger position stabilizes.  Left to its own devices, 
                // the plunger will usualy bounce off the barrel spring several 
                // times before coming to rest, so we'll see oscillating motion
                // for a second or two.  In the simplest case, we can aimply wait
                // for the plunger to stop moving for a short time.  However, the
                // player might intervene by pulling the plunger back again, so
                // watch for that motion as well.  If we're just bouncing freely,
                // we'll see the direction change frequently.  If the player is
                // moving the plunger manually, the direction will be constant
                // for longer.
                if (v >= 0)
                {
                    // We're moving back (or standing still).  If this has been
                    // going on for a while, the user must have taken control.
                    if (uint32_t(r.t - f3r.t) > 65000UL)
                    {
                        // user has taken control - cancel firing mode
                        firingMode(0);
                        break;
                    }
                }
                else
                {
                    // forward motion - reset retraction window
                    f3r.t = r.t;
                }

                // Check if we're close to the last starting point.  The joystick
                // positive axis range (0..4096) covers the retraction distance of 
                // about 2.5", so 1" is about 1638 joystick units, hence 1/16" is
                // about 100 units.
                if (abs(r.pos - f3s.pos) < 100)
                {
                    // It's at roughly the same position as the starting point.
                    // Consider it stable if this has been true for 300ms.
                    if (uint32_t(r.t - f3s.t) > 30000UL)
                    {
                        // we're done with the firing event
                        firingMode(0);
                    }
                    else
                    {
                        // it's close to the last position but hasn't been
                        // here long enough; stay in firing mode and continue
                        // to report the park position
                        z = 0;
                    }
                }
                else
                {
                    // It's not close enough to the last starting point, so use
                    // this as a new starting point, and stay in firing mode.
                    f3s = r;
                    z = 0;
                }
                break;
            }
            
            // save the velocity reading for next time
            vprv2 = vprv;
            vprv = v;
            
            // add the new reading to the history
            hist[histIdx++] = r;
            histIdx %= countof(hist);
            
            // apply the post-processing filter
            zf = applyPostFilter();
        }
    }
    
    // Get the current value to report through the joystick interface
    int16_t getPosition()
    {
        // return the last filtered reading
        return zf;
    }
        
    // Get the current velocity (joystick distance units per microsecond)
    float getVelocity() const { return vz; }
    
    // get the timestamp of the current joystick report (microseconds)
    uint32_t getTimestamp() const { return nthHist(0).t; }

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
            
            // set the initial zero point to the current position
            PlungerReading r;
            if (plungerSensor->read(r))
            {
                // got a reading - use it as the initial zero point
                applyPreFilter(r);
                cfg.plunger.cal.zero = r.pos;
                
                // use it as the starting point for the settling watch
                calZeroStart = r;
            }
            else
            {
                // no reading available - use the default 1/6 position
                cfg.plunger.cal.zero = 0xffff/6;
                
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
        }
            
        // remember the new mode
        plungerCalMode = f; 
    }
    
    // is a firing event in progress?
    bool isFiring() { return firing == 3; }

private:

    // Plunger data filtering mode:  optionally apply filtering to the raw 
    // plunger sensor readings to try to reduce noise in the signal.  This
    // is designed for the TSL1410/12 optical sensors, where essentially all
    // of the noise in the signal comes from lack of sharpness in the shadow
    // edge.  When the shadow is blurry, the edge detector has to pick a pixel,
    // even though the edge is actually a gradient spanning several pixels.
    // The edge detection algorithm decides on the exact pixel, but whatever
    // the algorithm, the choice is going to be somewhat arbitrary given that
    // there's really no one pixel that's "the edge" when the edge actually
    // covers multiple pixels.  This can make the choice of pixel sensitive to
    // small changes in exposure and pixel respose from frame to frame, which
    // means that the reported edge position can move by a pixel or two from
    // one frame to the next even when the physical plunger is perfectly still.
    // That's the noise we're talking about.
    //
    // We previously applied a mild hysteresis filter to the signal to try to
    // eliminate this noise.  The filter tracked the average over the last
    // several samples, and rejected readings that wandered within a few
    // pixels of the average.  If a certain number of readings moved away from
    // the average in the same direction, even by small amounts, the filter
    // accepted the changes, on the assumption that they represented actual
    // slow movement of the plunger.  This filter was applied after the firing
    // detection.
    //
    // I also tried a simpler filter that rejected changes that were too fast
    // to be physically possible, as well as changes that were very close to
    // the last reported position (i.e., simple hysteresis).  The "too fast"
    // filter was there to reject spurious readings where the edge detector
    // mistook a bad pixel value as an edge.  
    //
    // The new "mode 2" edge detector (see ccdSensor.h) seems to do a better
    // job of rejecting pixel-level noise by itself than the older "mode 0"
    // algorithm did, so I removed the filtering entirely.  Any filtering has
    // some downsides, so it's better to reduce noise in the underlying signal
    // as much as possible first.  It seems possible to get a very stable signal
    // now with a combination of the mode 2 edge detector and optimizing the
    // physical sensor arrangement, especially optimizing the light source to
    // cast as sharp as shadow as possible and adjusting the brightness to
    // maximize bright/dark contrast in the image.
    //
    //   0 = No filtering (current default)
    //   1 = Filter the data after firing detection using moving average
    //       hysteresis filter (old version, used in most 2016 releases)
    //   2 = Filter the data before firing detection using simple hysteresis
    //       plus spurious "too fast" motion rejection
    //
#define PLUNGER_FILTERING_MODE  0

#if PLUNGER_FILTERING_MODE == 0
    // Disable all filtering
    inline void applyPreFilter(PlungerReading &r) { }
    inline int applyPostFilter() { return z; }
#elif PLUNGER_FILTERING_MODE == 1
    // Apply pre-processing filter.  This filter is applied to the raw
    // value coming off the sensor, before calibration or fire-event
    // processing.
    void applyPreFilter(PlungerReading &r)
    {
    }
    
    // Figure the next post-processing filtered value.  This applies a
    // hysteresis filter to the last raw z value and returns the 
    // filtered result.
    int applyPostFilter()
    { 
        if (firing <= 1)
        {
            // Filter limit - 5 samples.  Once we've been moving
            // in the same direction for this many samples, we'll
            // clear the history and start over.
            const int filterMask = 0x1f;
            
            // figure the last average
            int lastAvg = int(filterSum / filterN);
            
            // figure the direction of this sample relative to the average,
            // and shift it in to our bit mask of recent direction data
            if (z != lastAvg)
            {
                // shift the new direction bit into the vector
                filterDir <<= 1;
                if (z > lastAvg) filterDir |= 1;
            }
            
            // keep only the last N readings, up to the filter limit
            filterDir &= filterMask;
            
            // if we've been moving consistently in one direction (all 1's
            // or all 0's in the direction history vector), reset the average
            if (filterDir == 0x00 || filterDir == filterMask) 
            {
                // motion away from the average - reset the average
                filterDir = 0x5555;
                filterN = 1;
                filterSum = (lastAvg + z)/2;
                return int16_t(filterSum);
            }
            else
            {
                // we're directionless - return the new average, with the 
                // new sample included
                filterSum += z;
                ++filterN;
                return int16_t(filterSum / filterN);
            }
        }
        else
        {
            // firing mode - skip the filter
            filterN = 1;
            filterSum = z;
            filterDir = 0x5555;
            return z;
        }
    }
#elif PLUNGER_FILTERING_MODE == 2
    // Apply pre-processing filter.  This filter is applied to the raw
    // value coming off the sensor, before calibration or fire-event
    // processing.
    void applyPreFilter(PlungerReading &r)
    {
        // get the previous raw reading
        PlungerReading prv = pre.raw;
        
        // the new reading is the previous raw reading next time, no 
        // matter how we end up filtering it
        pre.raw = r;
        
        // If it's too big an excursion from the previous raw reading,
        // ignore it and repeat the previous reported reading.  This
        // filters out anomalous spikes where we suddenly jump to a
        // level that's too far away to be possible.  Real plungers
        // take about 60ms to travel the full distance when released,
        // so assuming constant acceleration, the maximum realistic
        // speed is about 2.200 distance units (on our 0..0xffff scale)
        // per microsecond.
        //
        // On the other hand, if the new reading is too *close* to the
        // previous reading, use the previous reported reading.  This
        // filters out jitter around a stationary position.
        const float maxDist = 2.184f*uint32_t(r.t - prv.t);
        const int minDist = 256;
        const int delta = abs(r.pos - prv.pos);
        if (maxDist > minDist && delta > maxDist)
        {
            // too big an excursion - discard this reading by reporting
            // the last reported reading instead
            r.pos = pre.reported;
        }
        else if (delta < minDist)
        {
            // too close to the prior reading - apply hysteresis
            r.pos = pre.reported;
        }
        else
        {
            // the reading is in range - keep it, and remember it as
            // the last reported reading
            pre.reported = r.pos;
        }
    }
    
    // pre-filter data
    struct PreFilterData {
        PreFilterData() 
            : reported(0) 
        {
            raw.t = 0;
            raw.pos = 0;
        }
        PlungerReading raw;         // previous raw sensor reading
        int reported;               // previous reported reading
    } pre;
    
    
    // Apply the post-processing filter.  This filter is applied after
    // the fire-event processing.  In the past, this used hysteresis to
    // try to smooth out jittering readings for a stationary plunger.
    // We've switched to a different approach that massages the readings
    // coming off the sensor before 
    int applyPostFilter()
    {
        return z;
    }
#endif
    
    void initFilter()
    {
        filterSum = 0;
        filterN = 1;
        filterDir = 0x5555;
    }
    int64_t filterSum;
    int64_t filterN;
    uint16_t filterDir;
    
    
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

    // set a firing mode
    inline void firingMode(int m) 
    {
        firing = m;
    }
    
    // Find the most recent local maximum in the history data, up to
    // the given time limit.
    int histLocalMax(uint32_t tcur, uint32_t dt)
    {
        // start with the prior entry
        int idx = (histIdx == 0 ? countof(hist) : histIdx) - 1;
        int hi = hist[idx].pos;
        
        // scan backwards for a local maximum
        for (int n = countof(hist) - 1 ; n > 0 ; idx = (idx == 0 ? countof(hist) : idx) - 1)
        {
            // if this isn't within the time window, stop
            if (uint32_t(tcur - hist[idx].t) > dt)
                break;
                
            // if this isn't above the current hith, stop
            if (hist[idx].pos < hi)
                break;
                
            // this is the new high
            hi = hist[idx].pos;
        }
        
        // return the local maximum
        return hi;
    }

    // velocity at previous reading, and the one before that
    float vprv, vprv2;
    
    // Circular buffer of recent readings.  We keep a short history
    // of readings to analyze during firing events.  We can only identify
    // a firing event once it's somewhat under way, so we need a little
    // retrospective information to accurately determine after the fact
    // exactly when it started.  We throttle our readings to no more
    // than one every 1ms, so we have at least N*1ms of history in this
    // array.
    PlungerReading hist[32];
    int histIdx;
    
    // get the nth history item (0=last, 1=2nd to last, etc)
    inline const PlungerReading &nthHist(int n) const
    {
        // histIdx-1 is the last written; go from there
        n = histIdx - 1 - n;
        
        // adjust for wrapping
        if (n < 0)
            n += countof(hist);
            
        // return the item
        return hist[n];
    }

    // Firing event state.
    //
    //   0 - Default state.  We report the real instantaneous plunger 
    //       position to the joystick interface.
    //
    //   1 - Moving forward
    //
    //   2 - Accelerating
    //
    //   3 - Firing.  We report the rest position for a minimum interval,
    //       or until the real plunger comes to rest somewhere.
    //
    int firing;
    
    // Position/timestamp at start of firing phase 1.  When we see a
    // sustained forward acceleration, we freeze joystick reports at
    // the recent local maximum, on the assumption that this was the
    // start of the release.  If this is zero, it means that we're
    // monitoring accelerating motion but haven't seen it for long
    // enough yet to be confident that a release is in progress.
    PlungerReading f1;
    
    // Position/timestamp at start of firing phase 2.  The position is
    // the fake "bounce" position we report during this phase, and the
    // timestamp tells us when the phase began so that we can end it
    // after enough time elapses.
    PlungerReading f2;
    
    // Position/timestamp of start of stability window during phase 3.
    // We use this to determine when the plunger comes to rest.  We set
    // this at the beginning of phase 3, and then reset it when the 
    // plunger moves too far from the last position.
    PlungerReading f3s;
    
    // Position/timestamp of start of retraction window during phase 3.
    // We use this to determine if the user is drawing the plunger back.
    // If we see retraction motion for more than about 65ms, we assume
    // that the user has taken over, because we should see forward
    // motion within this timeframe if the plunger is just bouncing
    // freely.
    PlungerReading f3r;
    
    // next raw (unfiltered) Z value to report to the joystick interface 
    // (in joystick distance units)
    int z;
    
    // velocity of this reading (joystick distance units per microsecond)
    float vz;

    // next filtered Z value to report to the joystick interface
    int zf;    
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
#define v_ui16(var, ofs)    cfg.var = wireUI16(data+(ofs))
#define v_pin(var, ofs)     cfg.var = wirePinName(data[ofs])
#define v_byte_ro(val, ofs) // ignore read-only variables on SET
#define v_ui32_ro(val, ofs) // ignore read-only variables on SET
#define VAR_MODE_SET 1      // we're in SET mode
#define v_func configVarSet
#include "cfgVarMsgMap.h"

// redefine everything for the SET messages
#undef if_msg_valid
#undef v_byte
#undef v_ui16
#undef v_pin
#undef v_byte_ro
#undef v_ui32_ro
#undef VAR_MODE_SET
#undef v_func

// Handle GET messages - read variable values and return in USB message daa
#define if_msg_valid(test)
#define v_byte(var, ofs)    data[ofs] = cfg.var
#define v_ui16(var, ofs)    ui16Wire(data+(ofs), cfg.var)
#define v_pin(var, ofs)     pinNameWire(data+(ofs), cfg.var)
#define v_byte_ro(val, ofs) data[ofs] = (val)
#define v_ui32_ro(val, ofs) ui32Wire(data+(ofs), val);
#define VAR_MODE_SET 0      // we're in GET mode
#define v_func  configVarGet
#include "cfgVarMsgMap.h"


// ---------------------------------------------------------------------------
//
// Handle an input report from the USB host.  Input reports use our extended
// LedWiz protocol.
//
void handleInputMsg(LedWizMsg &lwm, USBJoystick &js)
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
    
                // we'll need a reset if the LedWiz unit number is changing
                bool needReset = (newUnitNo != cfg.psUnitNo);
                
                // set the configuration parameters from the message
                cfg.psUnitNo = newUnitNo;
                cfg.plunger.enabled = data[3] & 0x01;
                
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
                nvm.valid(), xmalloc_rem);
            break;
            
        case 5:
            // 5 = all outputs off, reset to LedWiz defaults
            allOutputsOff();
            break;
            
        case 6:
            // 6 = Save configuration to flash.
            saveConfigToFlash();
            
            // before disconnecting, pause for the delay time specified in
            // the parameter byte (in seconds)
            rebootTime_us = data[2] * 1000000L;
            rebootTimer.start();
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
            // Unused
            break;
            
        case 13:
            // 13 = Send button status report
            reportButtonStatus(js);
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
        
        // flag that we're in extended protocol mode
        ledWizMode = true;
        
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
            wizOn[i] = (b != 0);
            wizVal[i] = (b*48)/255;
            
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

    // debugging: print memory config info
    //    -> no longer very useful, since we use our own custom malloc/new allocator (see xmalloc() above)
    // {int *a = new int; printf("Stack=%lx, heap=%lx, free=%ld\r\n", (long)&a, (long)a, (long)&a - (long)a);} 
    
    // clear the I2C bus (for the accelerometer)
    clear_i2c();

    // load the saved configuration (or set factory defaults if no flash
    // configuration has ever been saved)
    loadConfigFromFlash();
    
    // initialize the diagnostic LEDs
    initDiagLEDs(cfg);

    // we're not connected/awake yet
    bool connected = false;
    Timer connectChangeTimer;

    // create the plunger sensor interface
    createPlunger();

    // set up the TLC5940 interface, if these chips are present
    init_tlc5940(cfg);

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
        
    // start the TV timer, if applicable
    startTVTimer(cfg);

    // initialize the button input ports
    bool kbKeys = false;
    initButtons(cfg, kbKeys);
    
    // Create the joystick USB client.  Note that the USB vendor/product ID
    // information comes from the saved configuration.  Also note that we have
    // to wait until after initializing the input buttons (which we just did
    // above) to set up the interface, since the button setup will determine
    // whether or not we need to present a USB keyboard interface in addition
    // to the joystick interface.
    MyUSBJoystick js(cfg.usbVendorID, cfg.usbProductID, USB_VERSION_NO, false, 
        cfg.joystickEnabled, kbKeys);
        
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

        if (cfg.disconnectRebootTimeout != 0 
            && connTimeoutTimer.read() > cfg.disconnectRebootTimeout)
            reboot(js, false, 0);
    }
    
    // we're now connected to the host
    connected = true;
    
    // Last report timer for the joytick interface.  We use this timer to
    // throttle the report rate to a pace that's suitable for VP.  Without
    // any artificial delays, we could generate data to send on the joystick
    // interface on every loop iteration.  The loop iteration time depends
    // on which devices are attached, since most of the work in our main 
    // loop is simply polling our devices.  For typical setups, the loop
    // time ranges from about 0.25ms to 2.5ms; the biggest factor is the
    // plunger sensor.  But VP polls for input about every 10ms, so there's 
    // no benefit in sending data faster than that, and there's some harm,
    // in that it creates USB overhead (both on the wire and on the host 
    // CPU).  We therefore use this timer to pace our reports to roughly
    // the VP input polling rate.  Note that there's no way to actually
    // synchronize with VP's polling, but there's also no need to, as the
    // input model is designed to reflect the overall current state at any
    // given time rather than events or deltas.  If VP polls twice between
    // two updates, it simply sees no state change; if we send two updates
    // between VP polls, VP simply sees the latest state when it does get
    // around to polling.
    Timer jsReportTimer;
    jsReportTimer.start();
    
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
    Accel accel(MMA8451_SCL_PIN, MMA8451_SDA_PIN, MMA8451_I2C_ADDRESS, MMA8451_INT_PIN);
   
    // last accelerometer report, in joystick units (we report the nudge
    // acceleration via the joystick x & y axes, per the VP convention)
    int x = 0, y = 0;
    
    // initialize the plunger sensor
    plungerSensor->init();
    
    // set up the ZB Launch Ball monitor
    ZBLaunchBall zbLaunchBall;
    
    // enable the peripheral chips
    if (tlc5940 != 0)
        tlc5940->enable(true);
    if (hc595 != 0)
        hc595->enable(true);
        
    // start the LedWiz flash cycle timers
    wizPulseTimer.start();
    wizCycleTimer.start();
    
    // start the PWM update polling timer
    polledPwmTimer.start();
    
    // we're all set up - now just loop, processing sensor reports and 
    // host requests
    for (;;)
    {
        // start the main loop timer for diagnostic data collection
        IF_DIAG(
            Timer mainLoopTimer;
            mainLoopTimer.start();
        )
            
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
            handleInputMsg(lwm, js);
            IF_DIAG(++msgCount;)
        }
        
        // collect performance statistics on the message reader, if desired
        IF_DIAG(
            if (msgCount != 0)
            {
                mainLoopMsgTime += lwt.read();
                mainLoopMsgCount++;
            }
        )
        
        // update flashing LedWiz outputs periodically
        wizPulse();
        
        // update PWM outputs
        pollPwmUpdates();
            
        // send TLC5940 data updates if applicable
        if (tlc5940 != 0)
            tlc5940->send();
       
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
        
        // read the plunger sensor
        plungerReader.read();
        
        // update the ZB Launch Ball status
        zbLaunchBall.update();
        
        // process button updates
        processButtons(cfg);
        
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
        
        // figure the current status flags for joystick reports
        uint16_t statusFlags =
            (cfg.plunger.enabled ? 0x01 : 0x00)
            | (nightMode ? 0x02 : 0x00)
            | ((psu2_state & 0x07) << 2);

        // If it's been long enough since our last USB status report, send
        // the new report.  VP only polls for input in 10ms intervals, so
        // there's no benefit in sending reports more frequently than this.
        // More frequent reporting would only add USB I/O overhead.
        if (cfg.joystickEnabled && jsReportTimer.read_us() > 10000UL)
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
            
            // Report the current plunger position unless the plunger is
            // disabled, or the ZB Launch Ball signal is on.  In either of
            // those cases, just report a constant 0 value.  ZB Launch Ball 
            // temporarily disables mechanical plunger reporting because it 
            // tells us that the table has a Launch Ball button instead of
            // a traditional plunger, so we don't want to confuse VP with
            // regular plunger inputs.
            int z = plungerReader.getPosition();
            int zrep = (!cfg.plunger.enabled || zbLaunchOn ? 0 : z);
            
            // rotate X and Y according to the device orientation in the cabinet
            accelRotate(x, y);

            // send the joystick report
            jsOK = js.update(x, y, zrep, jsButtons, statusFlags);
            
            // we've just started a new report interval, so reset the timer
            jsReportTimer.reset();
        }

        // If we're in sensor status mode, report all pixel exposure values
        if (reportPlungerStat)
        {
            // send the report            
            plungerSensor->sendStatusReport(js, reportPlungerStatFlags, reportPlungerStatTime);

            // we have satisfied this request
            reportPlungerStat = false;
        }
        
        // If joystick reports are turned off, send a generic status report
        // periodically for the sake of the Windows config tool.
        if (!cfg.joystickEnabled && jsReportTimer.read_us() > 5000)
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
                    if (hc595 != 0)
                        hc595->enable(false);
                }
            }
        }
        
        // if we have a reboot timer pending, check for completion
        if (rebootTimer.isRunning() && rebootTimer.read_us() > rebootTime_us)
            reboot(js);
        
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
                
                // send TLC5940 data if necessary
                if (tlc5940 != 0)
                    tlc5940->send();
                
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
                
                // if the disconnect reboot timeout has expired, reboot
                if (cfg.disconnectRebootTimeout != 0 
                    && reconnTimeoutTimer.read() > cfg.disconnectRebootTimeout)
                    reboot(js, false, 0);
            }
            
            // resume the main loop timer
            IF_DIAG(mainLoopTimer.start();)
            
            // if we made it out of that loop alive, we're connected again!
            connected = true;
            HAL_DEBUG_PRINTEVENTS(">C");

            // Enable peripheral chips and update them with current output data
            if (tlc5940 != 0)
            {
                tlc5940->enable(true);
                tlc5940->update(true);
            }
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
                    // that we can continue to monitor this.
                    if (jsOKTimer.read() > cfg.disconnectRebootTimeout)
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
        
        // collect statistics on the main loop time, if desired
        IF_DIAG(
            mainLoopIterTime += mainLoopTimer.read();
            mainLoopIterCount++;
        )
    }
}
