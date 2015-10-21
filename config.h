// Pinscape Controller Configuration
//
// To customize your private configuration, simply open this file in the 
// mbed on-line IDE, make your changes, save the file, and click the Compile
// button at the top of the window.  That will generate a customized .bin
// file that you can download onto your KL25Z board.

#ifndef CONFIG_H
#define CONFIG_H

// ---------------------------------------------------------------------------
//
// Expansion Board.  If you're using the expansion board, un-comment the
// line below.  This will select all of the correct defaults for the board.
//
// The expansion board settings are mostly automatic, so you shouldn't have
// to change much else.  However, you should still look at and adjust the
// following as needed:
//    - TV power on delay time
//    - Plunger sensor settings, if you're using a plunger
//
//#define EXPANSION_BOARD


// --------------------------------------------------------------------------
//
// Enable/disable joystick functions.
//
// This controls whether or not we send joystick reports to the PC with the 
// plunger and accelerometer readings.  By default, this is enabled.   If
// you want to use two or more physical KL25Z Pinscape controllers in your
// system (e.g., if you want to increase the number of output ports
// available by using two or more KL25Z's), you should disable the joystick
// features on the second (and third+) controller.  It's not useful to have
// more than one board reporting the accelerometer readings to the host -
// doing so will just add USB overhead.  This setting lets you turn off the
// reports for the secondary controllers, turning the secondary boards into
// output-only devices.
//
// Note that you can't use button inputs on a controller that has the
// joystick features disabled, because the buttons are handled via the
// joystick reports.  Wire all of your buttons to the primary KL25Z that
// has the joystick features enabled.
//
// To disable the joystick features, just comment out the next line (add
// two slashes at the beginning of the line).
//
#define ENABLE_JOYSTICK


// ---------------------------------------------------------------------------
//
// USB device vendor ID and product ID.  These values identify the device 
// to the host software on the PC.  By default, we use the same settings as
// a real LedWiz so that host software will recognize us as an LedWiz.
//
// The standard settings *should* work without conflicts, even if you have 
// a real LedWiz.  My reference system is 64-bit Windows 7 with a real LedWiz 
// on unit #1 and a Pinscape controller on unit #8 (the default), and the 
// two coexist happily in my system.  The LedWiz is designed specifically 
// to allow multiple units in one system, using the unit number value 
// (see below) to distinguish multiple units, so there should be no conflict
// between Pinscape and any real LedWiz devices you have.
//
// However, even though conflicts *shouldn't* happen, I've had one report
// from a user who experienced a Windows USB driver conflict that they could
// only resolve by changing the vendor ID.  The real underlying cause is 
// still a mystery, but whatever was going on, changing the vendor ID fixed 
// it.  If you run into a similar problem, you can try the same fix as a
// last resort.  Before doing that, though, you should try changing the 
// Pinscape unit number first - it's possible that your real LedWiz is using 
// unit #8, which is our default setting.
//
// If you must change the vendor ID for any reason, you'll sacrifice LedWiz
// compatibility, which means that old programs like Future Pinball that use
// the LedWiz interface directly won't be able to access the LedWiz output
// controller features.  However, all is not lost.  All of the other functions
// (plunger, nudge, and key input) use the joystick interface, which will 
// work regardless of the ID values.  In addition, DOF R3 recognizes the
// "emergency fallback" ID below, so if you use that, *all* functions
// including the output controller will work in any DOF R3-enabled software,
// including Visual Pinball and PinballX.  So the only loss will be that
// old LedWiz-only software won't be able to control the outputs.
//
// The "emergency fallback" ID below is officially registerd with 
// http://pid.codes, a registry for open-source USB projects, which should 
// all but guarantee that this alternative ID shouldn't conflict with 
// any other devices in your system.


// STANDARD ID SETTINGS.  These provide full, transparent LedWiz compatibility.
const uint16_t USB_VENDOR_ID = 0xFAFA;      // LedWiz vendor ID = FAFA
const uint16_t USB_PRODUCT_ID = 0x00F0;     // LedWiz start of product ID range = 00F0


// EMERGENCY FALLBACK ID SETTINGS.  These settings are not LedWiz-compatible,
// so older LedWiz-only software won't be able to access the output controller
// features.  However, DOF R3 recognizes these IDs, so DOF-aware software (Visual 
// Pinball, PinballX) will have full access to all features.
//
//const uint16_t USB_VENDOR_ID = 0x1209;   // DOF R3-compatible vendor ID = 1209
//const uint16_t USB_PRODUCT_ID = 0xEAEA;  // DOF R3-compatible product ID = EAEA


// ---------------------------------------------------------------------------
//
// LedWiz unit number.
//
// Each LedWiz device has a unit number, from 1 to 16.  This lets you install
// more than one LedWiz in your system: as long as each one has a different
// unit number, the software on the PC can tell them apart and route commands 
// to the right device.
//
// A real LedWiz has its unit number set at the factory.  If you don't tell
// them otherwise when placing your order, they will set it to unit #1.  Most
// real LedWiz units therefore are set to unit #1.  There's no provision on
// a real LedWiz for users to change the unit number after it leaves the 
// factory.
//
// For our *emulated* LedWiz, we default to unit #8 if we're the primary
// Pinscape controller in the system, or unit #9 if we're set up as the
// secondary controller with the joystick functions turned off.
//
// The reason we start at unit #8 is that we want to avoid conflicting with
// any real LedWiz devices in your system.  Most real LedWiz devices are
// set up as unit #1, and in the rare cases where people have two of them,
// the second one is usually unit #2.  
//
// Note 1:  the unit number here is the *user visible* unit number that
// you use on the PC side.  It's the number you specify in your DOF
// configuration and so forth.  Internally, the USB reports subtract
// one from this number - e.g., nominal unit #1 shows up as 0 in the USB
// reports.  If you're trying to puzzle out why all of the USB reports
// are all off by one from the unit number you select here, that's why.
//
// Note 2:  the DOF Configtool (google it) knows about the Pinscape 
// controller.  There it's referred to as simply "KL25Z" rather than 
// Pinscape Controller, but that's what they're talking about.  The DOF 
// tool knows that it uses #8 as its default unit number, so it names the 
// .ini file for this controller xxx8.ini.  If you change the unit number 
// here, remember to rename the DOF-generated .ini file to match, by 
// changing the "8" at the end of the filename to the new number you set 
// here.
const uint8_t DEFAULT_LEDWIZ_UNIT_NUMBER = 
#ifdef ENABLE_JOYSTICK
   0x08;   // joystick enabled - assume we're the primary KL25Z, so use unit #8
#else
   0x09;   // joystick disabled - assume we're a secondary, output-only KL25Z, so use #9
#endif


// --------------------------------------------------------------------------
//
// Accelerometer orientation.  The accelerometer feature lets Visual Pinball 
// (and other pinball software) sense nudges to the cabinet, and simulate 
// the effect on the ball's trajectory during play.  We report the direction
// of the accelerometer readings as well as the strength, so it's important
// for VP and the KL25Z to agree on the physical orientation of the
// accelerometer relative to the cabinet.  The accelerometer on the KL25Z
// is always mounted the same way on the board, but we still have to know
// which way you mount the board in your cabinet.  We assume as default
// orientation where the KL25Z is mounted flat on the bottom of your
// cabinet with the USB ports pointing forward, toward the coin door.  If
// it's more convenient for you to mount the board in a different direction,
// you simply need to select the matching direction here.  Comment out the
// ORIENTATION_PORTS_AT_FRONT line and un-comment the line that matches
// your board's orientation.

#define ORIENTATION_PORTS_AT_FRONT      // USB ports pointing toward front of cabinet
// #define ORIENTATION_PORTS_AT_LEFT    // USB ports pointing toward left side of cab
// #define ORIENTATION_PORTS_AT_RIGHT   // USB ports pointing toward right side of cab
// #define ORIENTATION_PORTS_AT_REAR    // USB ports pointing toward back of cabinet



// --------------------------------------------------------------------------
//
// Plunger CCD sensor.
//
// If you're NOT using the CCD sensor, comment out the next line (by adding
// two slashes at the start of the line).

#define ENABLE_CCD_SENSOR

// Physical pixel count for your sensor.  This software has been tested with
// TAOS TSL1410R (1280 pixels) and TSL1412R (1536 pixels) sensors.  It might
// work with other similar sensors as well, but you'll probably have to make
// some changes to the software interface to the sensor if you're using any
// sensor outside of the TAOS TSL14xxR series.
//
// If you're not using a CCD sensor, you can ignore this.
const int CCD_NPIXELS = 1280;

// Number of pixels from the CCD to sample on each high-res scan.  We don't
// sample every pixel from the sensor on each scan, because (a) we don't
// have to, and (b) we don't want to.  We don't have to sample all of the
// pixels because these sensors have much finer resolution than we need to
// get good results.  On a typical pinball cabinet setup with a 1920x1080
// HD TV display, the on-screen plunger travel distance is about 165 pixels,
// so that's all the pixels we need to sample for pixel-accurate animation.
// Even so, we still *could* sample at higher resolution, but we don't *want*
// to sample more pixels than we have to,  because reading each pixel takes 
// time.  The limiting factor for read speed is the sampling time for the ADC 
// (analog to digital  converter); it needs about 20us per sample to get an 
// accurate voltage reading.  We want to animate the on-screen plunger in 
// real time, with minimal lag, so it's important that we complete each scan 
// as quickly as possible.  The fewer pixels we sample, the faster we 
// complete each scan.
//
// Happily, the time needed to read the approximately 165 pixels required
// for pixel-accurate positioning on the display is short enough that we can
// complete a scan within the cycle time for USB reports.  Visual Pinball
// only polls for input at about 10ms intervals, so there's no benefit
// to going much faster than this.  The sensor timing is such that we can
// read about 165 pixels in well under 10ms.  So that's really the sweet
// spot for our scans.
//
// Note that we distribute the sampled pixels evenly across the full range
// of the sensor's pixels.  That is, we read every nth pixel, and skip the
// ones in between.  That means that the sample count here has to be an even
// divisor of the physical pixel count.  Empirically, reading every 8th
// pixel gives us good results on both the TSL1410R and TSL1412R, so you
// shouldn't need to change this if you're using one of those sensors.  If
// you're using a different sensor, you should be sure to adjust this so that 
// it works out to an integer result with no remainder.
//
const int CCD_NPIXELS_SAMPLED = CCD_NPIXELS / 8;

// The KL25Z pins that the CCD sensor is physically attached to:
//
//  CCD_SI_PIN = the SI (sensor data input) pin
//  CCD_CLOCK_PIN = the sensor clock pin
//  CCD_SO_PIN = the SO (sensor data output) pin
//
// The SI an Clock pins are DigitalOut pins, so these can be set to just
// about any gpio pins that aren't used for something else.  The SO pin must
// be an AnalogIn capable pin - only a few of the KL25Z gpio pins qualify, 
// so check the pinout diagram to find suitable candidates if you need to 
// change this.  Note that some of the gpio pins shown in the mbed pinout
// diagrams are committed to other uses by the mbed software or by the KL25Z
// wiring itself, so if you do change these, be sure that the new pins you
// select are really available.

const PinName CCD_SI_PIN = PTE20;
const PinName CCD_CLOCK_PIN = PTE21;
const PinName CCD_SO_PIN = PTB0;

// --------------------------------------------------------------------------
//
// Plunger potentiometer sensor.
//
// If you're using a potentiometer as the plunger sensor, un-comment the
// next line (by removing the two slashes at the start of the line), and 
// also comment out the ENABLE_CCD_SENSOR line above.

//#define ENABLE_POT_SENSOR

// The KL25Z pin that your potentiometer is attached to.  The potentiometer
// requires wiring three connectins:
//
// - Wire the fixed resistance end of the potentiometer nearest the KNOB 
//   end of the plunger to the 3.3V output from the KL25Z
//
// - Wire the other fixed resistance end to KL25Z Ground
//
// -  Wire the potentiometer wiper (the variable output terminal) to the 
//    KL25Z pin identified below.  
//
// Note that you can change the pin selection below, but if you do, the new
// pin must be AnalogIn capable.  Only a few of the KL25Z pins qualify.  Refer
// to the KL25Z pinout diagram to find another AnalogIn pin if you need to
// change this for any reason.  Note that the default is to use the same analog 
// input that the CCD sensor would use if it were enabled, which is why you 
// have to be sure to disable the CCD support in the software if you're using 
// a potentiometer as the sensor.

const PinName POT_PIN = PTB0;

// --------------------------------------------------------------------------
//
// Plunger calibration button and indicator light.
//
// These specify the pin names of the plunger calibration button connections.
// If you're not using these, you can set these to NC.  (You can even use the
// button but not the LED; set the LED to NC if you're only using the button.)
//
// If you're using the button, wire one terminal of a momentary switch or
// pushbutton to the input pin you select, and wire the other terminal to the 
// KL25Z ground.  Push and hold the button for a few seconds to enter plunger 
// calibration mode.
// 
// If you're using the LED, you'll need to build a little transistor power
// booster circuit to power the LED, as described in the build guide.  The
// LED gives you visual confirmation that the you've triggered calibration
// mode and lets you know when the mode times out.  Note that the LED on
// board the KL25Z also changes color to indicate the same information, so
// if the KL25Z is positioned so that you can see it while you're doing the
// calibration, you don't really need a separate button LED.  But the
// separate LED is spiffy, especially if it's embedded in the pushbutton.
//
// Note that you can skip the pushbutton altogether and trigger calibration
// from the Windows control software.  But again, the button is spiffier.

// calibration button input 
const PinName CAL_BUTTON_PIN = PTE29;

// calibration button indicator LED
const PinName CAL_BUTTON_LED = PTE23;


// ---------------------------------------------------------------------------
//
// TV Power-On Timer.  This section lets you set up a delayed relay timer
// for turning on your TV monitor(s) shortly after you turn on power to the
// system.  This requires some external circuitry, which is built in to the
// expansion board, or which you can build yourself - refer to the Build
// Guide for the circuit plan.  
//
// If you're using this feature, un-comment the next line, and make any
// changes to the port assignments below.  The default port assignments are
// suitable for the expansion board.  Note that the TV timer is enabled
// automatically if you're using the expansion board, since it's built in.
//#define ENABLE_TV_TIMER

#if defined(ENABLE_TV_TIMER) || defined(EXPANSION_BOARD)
# define PSU2_STATUS_SENSE  PTD2    // Digital In pin to read latch status
# define PSU2_STATUS_SET    PTE0    // Digital Out pin to set latch
# define TV_RELAY_PIN       PTD3    // Digital Out pin to control TV switch relay

// Amount of time (in seconds) to wait after system power-up before 
// pulsing the TV ON switch relay.  Adjust as needed for your TV(s).
// Most monitors won't respond to any buttons for the first few seconds
// after they're plugged in, so we need to wait long enough to make sure
// the TVs are ready to receive input before pressing the button.
#define TV_DELAY_TIME    7.0

#endif


// --------------------------------------------------------------------------
//
// Pseudo "Launch Ball" button.
//
// Zeb of zebsboards.com came up with a clever scheme for his plunger kit
// that lets the plunger simulate a Launch Ball button for tables where
// the original used a Launch button instead of a plunger (e.g., Medieval 
// Madness, T2, or Star Trek: The Next Generation).  The scheme uses an
// LedWiz output to tell us when such a table is loaded.  On the DOF
// Configtool site, this is called "ZB Launch Ball".  When this LedWiz
// output is ON, it tells us that the table will ignore the analog plunger
// because it doesn't have a plunger object, so the analog plunger should
// send a Launch Ball button press signal when the user releases the plunger.
// 
// If you wish to use this feature, you need to do two things:
//
// First, adjust the two lines below to set the LedWiz output and joystick
// button you wish to use for this feature.  The defaults below should be
// fine for most people, but if you're using the Pinscape controller for
// your physical button wiring, you should set the launch button to match
// where you physically wired your actual Launch Ball button.  Likewise,
// change the LedWiz port if you're using the one below for some actual
// hardware output.  This is a virtual port that won't control any hardware;
// it's just for signaling the plunger that we're in "button mode".  Note
// that the numbering for the both the LedWiz port and joystick button 
// start at 1 to match the DOF Configtool and VP dialog numbering.
//
// Second, in the DOF Configtool, make sure you have a Pinscape controller
// in your cabinet configuration, then go to your Port Assignments and set
// the port defined below to "ZB Launch Ball".
//
// Third, open the Visual Pinball editor, open the Preferences | Keys
// dialog, and find the Plunger item.  Open the drop-down list under that
// item and select the button number defined below.
//
// To disable this feature, just set ZBLaunchBallPort to 0 here.

const int ZBLaunchBallPort = 32;
const int LaunchBallButton = 24;

// Distance necessary to push the plunger to activate the simulated 
// launch ball button, in inches.  A standard pinball plunger can be 
// pushed forward about 1/2".  However, the barrel spring is very
// stiff, and anything more than about 1/8" requires quite a bit
// of force.  Ideally the force required should be about the same as 
// for any ordinary pushbutton.
//
// On my cabinet, empirically, a distance around 2mm (.08") seems
// to work pretty well.  It's far enough that it doesn't trigger
// spuriously, but short enough that it responds to a reasonably
// light push.
//
// You might need to adjust this up or down to get the right feel.
// Alternatively, if you don't like the "push" gesture at all and
// would prefer to only make the plunger respond to a pull-and-release
// motion, simply set this to, say, 2.0 - it's impossible to push a 
// plunger forward that far, so that will effectively turn off the 
// push mode.
const float LaunchBallPushDistance = .08;


// --------------------------------------------------------------------------
//
// TLC5940 PWM controller chip setup - Enhanced LedWiz emulation
//
// By default, the Pinscape Controller software can provide limited LedWiz
// emulation through the KL25Z's on-board GPIO ports.  This lets you hook
// up external devices, such as LED flashers or solenoids, to the KL25Z
// outputs (using external circuitry to boost power - KL25Z GPIO ports
// are limited to a meager 4mA per port).  This capability is limited by
// the number of available GPIO ports on the KL25Z, and even smaller limit
// of 10 PWM-capable GPIO ports.
//
// As an alternative, the controller software lets you use external PWM
// controller chips to control essentially unlimited channels with full
// PWM control on all channels.  This requires building external circuitry
// using TLC5940 chips.  Each TLC5940 chip provides 16 full PWM channels,
// and you can daisy-chain multiple TLC5940 chips together to set up 32, 
// 48, 64, or more channels.
//
// If you do add TLC5940 circuits to your controller hardware, use this
// section to configure the connection to the KL25Z.
//
// Note that when using the TLC5940, you can still also use some GPIO
// pins for outputs as normal.  See ledWizPinMap[] for 

// Number of TLC5940 chips you're using.  For a full LedWiz-compatible
// setup, you need two of these chips, for 32 outputs.  The software
// will handle up to 8.  The expansion board uses 4 of these chips; if
// you're not using the expansion board, we assume you're not using
// any of them.
#ifdef EXPANSION_BOARD
# define TLC5940_NCHIPS  4
#else
# define TLC5940_NCHIPS  0     // change this if you're using TLC5940's without the expansion board
#endif

// If you're using TLC5940s, change any of these as needed to match the
// GPIO pins that you connected to the TLC5940 control pins.  Note that
// SIN and SCLK *must* be connected to the KL25Z SPI0 MOSI and SCLK
// outputs, respectively, which effectively limits them to the default
// selections, and that the GSCLK pin must be PWM-capable.  These defaults
// all match the expansion board wiring.
#define TLC5940_SIN    PTC6    // Must connect to SPI0 MOSI -> PTC6 or PTD2
#define TLC5940_SCLK   PTC5    // Must connect to SPI0 SCLK -> PTC5 or PTD1; however, PTD1 isn't
                               //   recommended because it's hard-wired to the on-board blue LED
#define TLC5940_XLAT   PTC10   // Any GPIO pin can be used
#define TLC5940_BLANK  PTC7    // Any GPIO pin can be used
#define TLC5940_GSCLK  PTA1    // Must be a PWM-capable pin

// TLC5940 output power enable pin.  This is a GPIO pin that controls
// a high-side transistor switch that controls power to the optos and
// LEDs connected to the TLC5940 outputs.  This is a precaution against
// powering the chip's output pins before Vcc is powered.  Vcc comes
// from the KL25Z, so when our program is running, we know for certain
// that Vcc is up.  This means that we can simply enable this pin any
// time after entering our main().  Un-comment this line if using this
// circuit.
// #define TLC5940_PWRENA PTC11   // Any GPIO pin can be used
#ifdef EXPANSION_BOARD
# define TLC5940_PWRENA PTC11
#endif

#endif // CONFIG_H - end of include-once section (code below this point can be multiply included)


#ifdef DECL_EXTERNS  // this section defines global variables, only if this macro is set

// --------------------------------------------------------------------------
//

// Joystick button input pin assignments.  
//
// You can wire up to 32 GPIO ports to buttons (equipped with 
// momentary switches).  Connect each switch between the desired 
// GPIO port and ground (J9 pin 12 or 14).  When the button is pressed, 
// we'll tell the host PC that the corresponding joystick button is 
// pressed.  We debounce the keystrokes in software, so you can simply 
// wire directly to pushbuttons with no additional external hardware.
//
// Note that we assign 24 buttons by default, even though the USB
// joystick interface can handle up to 32 buttons.  VP itself only
// allows mapping of up to 24 buttons in the preferences dialog 
// (although it can recognize 32 buttons internally).  If you want 
// more buttons, you can reassign pins that are assigned by default
// as LedWiz outputs.  To reassign a pin, find the pin you wish to
// reassign in the LedWizPortMap array below, and change the pin name 
// there to NC (for Not Connected).  You can then change one of the
// "NC" entries below to the reallocated pin name.  The limit is 32
// buttons total.
//
// (If you're using TLC5940 chips to control outputs, ALL of the
// LedWiz mapped ports can be reassigned as keys, except, of course,
// those taken over for the 5940 interface.)
//
// Note: PTD1 (pin J2-12) should NOT be assigned as a button input,
// as this pin is physically connected on the KL25Z to the on-board
// indicator LED's blue segment.  This precludes any other use of
// the pin.
PinName buttonMap[] = {
    PTC2,      // J10 pin 10, joystick button 1
    PTB3,      // J10 pin 8,  joystick button 2
    PTB2,      // J10 pin 6,  joystick button 3
    PTB1,      // J10 pin 4,  joystick button 4
    
    PTE30,     // J10 pin 11, joystick button 5
    PTE22,     // J10 pin 5,  joystick button 6
    
    PTE5,      // J9 pin 15,  joystick button 7
    PTE4,      // J9 pin 13,  joystick button 8
    PTE3,      // J9 pin 11,  joystick button 9
    PTE2,      // J9 pin 9,   joystick button 10
    PTB11,     // J9 pin 7,   joystick button 11
    PTB10,     // J9 pin 5,   joystick button 12
    PTB9,      // J9 pin 3,   joystick button 13
    PTB8,      // J9 pin 1,   joystick button 14
    
    PTC12,     // J2 pin 1,   joystick button 15
    PTC13,     // J2 pin 3,   joystick button 16
    PTC16,     // J2 pin 5,   joystick button 17
    PTC17,     // J2 pin 7,   joystick button 18
    PTA16,     // J2 pin 9,   joystick button 19
    PTA17,     // J2 pin 11,  joystick button 20
    PTE31,     // J2 pin 13,  joystick button 21
    PTD6,      // J2 pin 17,  joystick button 22
    PTD7,      // J2 pin 19,  joystick button 23
    
    PTE1,      // J2 pin 20,  joystick button 24

    NC,        // not used,   joystick button 25
    NC,        // not used,   joystick button 26
    NC,        // not used,   joystick button 27
    NC,        // not used,   joystick button 28
    NC,        // not used,   joystick button 29
    NC,        // not used,   joystick button 30
    NC,        // not used,   joystick button 31
    NC         // not used,   joystick button 32
};

// --------------------------------------------------------------------------
//
// LED-Wiz emulation output pin assignments
//
// This sets the mapping from logical LedWiz port numbers, as used
// in the software on the PC side, to physical hardware pins on the
// KL25Z and/or the TLC5940 controllers.
//
// The LedWiz protocol lets the PC software set a "brightness" level
// for each output.  This is used to control the intensity of LEDs
// and other lights, and can also control motor speeds.  To implement 
// the intensity level in hardware, we use PWM, or pulse width
// modulation, which switches the output on and off very rapidly
// to give the effect of a reduced voltage.  Unfortunately, the KL25Z
// hardware is limited to 10 channels of PWM control for its GPIO
// outputs, so it's impossible to implement the LedWiz's full set
// of 32 adjustable outputs using only GPIO ports.  However, you can
// create 10 adjustable ports and fill out the rest with "digital"
// GPIO pins, which are simple on/off switches.  The intensity level
// of a digital port can't be adjusted - it's either fully on or
// fully off - but this is fine for devices that don't have
// different intensity settings anyway, such as replay knockers
// and flipper solenoids.
//
// In the mapping list below, you can decide how to dole out the
// PWM-capable and digital-only GPIO pins.  To make it easier to
// remember which is which, the default mapping below groups all
// of the PWM-capable ports together in the first 10 logical LedWiz
// port numbers.  Unfortunately, these ports aren't *physically*
// together on the KL25Z pin headers, so this layout may be simple
// in terms of the LedWiz numbering, but it's a little jumbled
// in the physical layout.t
//
// "NC" in the pin name slot means "not connected".  This means
// that there's no physical output for this LedWiz port number.
// The device will still accept commands that control the port,
// but these will just be silently ignored, since there's no pin
// to turn on or off for these ports.  The reason we leave some 
// ports unconnected is that we don't have enough physical GPIO 
// pins to fill out the full LedWiz complement of 32 ports.  Many 
// pins are already taken for other purposes, such as button 
// inputs or the plunger CCD interface.
//
// The mapping between physical output pins on the KL25Z and the
// assigned LED-Wiz port numbers is essentially arbitrary.  You can
// customize this by changing the entries in the array below if you
// wish to rearrange the pins for any reason.  Be aware that some
// of the physical outputs are already used for other purposes
// (e.g., some of the GPIO pins on header J10 are used for the
// CCD sensor - but you can of course reassign those as well by
// changing the corresponding declarations elsewhere in this file).
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
// If you wish to reallocate a pin in the array below to some other
// use, such as a button input port, simply change the pin name in
// the entry to NC (for Not Connected).  This will disable the given
// logical LedWiz port number and free up the physical pin.
//
// If you wish to reallocate a pin currently assigned to the button
// input array, simply change the entry for the pin in the buttonMap[]
// array above to NC (for "not connected"), and plug the pin name into
// a slot of your choice in the array below.
//
// Note: Don't assign PTD1 (pin J2-12) as an LedWiz output.  That pin
// is hard-wired on the KL25Z to the on-board indicator LED's blue segment,  
// which pretty much precludes any other use of the pin.
//
// ACTIVE-LOW PORTS:  By default, when a logical port is turned on in
// the software, we set the physical GPIO voltage to "high" (3.3V), and
// set it "low" (0V) when the logical port is off.  This is the right
// scheme for the booster circuit described in the build guide.  Some
// third-party booster circuits want the opposite voltage scheme, where
// logical "on" is represented by 0V on the port and logical "off" is
// represented by 3.3V.  If you're using an "active low" booster like
// that, set the PORT_ACTIVE_LOW flag in the array below for each 
// affected port.
//
// TLC5940 PORTS:  To assign an LedWiz output port number to a particular
// output on a TLC5940, set tlcPortNum to the non-zero port number,
// starting at 1 for the first output on the first chip, 16 for the
// last output on the first chip, 17 for the first output on the second
// chip, and so on.  TLC ports are inherently PWM-capable only, so it's 
// not necessary to set the PORT_IS_PWM flag for those.
//

// ledWizPortMap 'flags' bits - combine these with '|'
const int PORT_IS_PWM     = 0x0001;  // this port is PWM-capable
const int PORT_ACTIVE_LOW = 0x0002;  // use LOW voltage (0V) when port is ON

struct {
    PinName pin;        // the GPIO pin assigned to this output; NC if not connected or a TLC5940 port
    int flags;          // flags - a combination of PORT_xxx flag bits (see above)
    int tlcPortNum;     // for TLC5940 ports, the TLC output number (1 to number of chips*16); otherwise 0
} ledWizPortMap[] = {
    
#if TLC5940_NCHIPS == 0

    // *** BASIC MODE - GPIO OUTPUTS ONLY ***
    // This is the basic mapping, using entirely GPIO pins, for when you're 
    // not using external TLC5940 chips.  We provide 22 physical outputs, 10 
    // of which are PWM capable.
    //
    // Important!  Note that the "isPWM" setting isn't just something we get to
    // choose.  It's a feature of the KL25Z hardware.  Some pins are PWM capable 
    // and some aren't, and there's nothing we can do about that in the software.
    // Refer to the KL25Z manual or schematics for the possible connections.  Note 
    // that there are other PWM-capable pins besides the 10 shown below, BUT they 
    // all share TPM channels with the pins below.  For example, TPM 2.0 can be 
    // connected to PTA1, PTB2, PTB18, PTE22 - but only one at a time.  So if you 
    // want to use PTB2 as a PWM out, it means you CAN'T use PTA1 as a PWM out.
    // We commented each PWM pin with its hardware channel number to help you keep
    // track of available channels if you do need to rearrange any of these pins.

    { PTA1,  PORT_IS_PWM },      // pin J1-2,  LW port 1  (PWM capable - TPM 2.0 = channel 9)
    { PTA2,  PORT_IS_PWM },      // pin J1-4,  LW port 2  (PWM capable - TPM 2.1 = channel 10)
    { PTD4,  PORT_IS_PWM },      // pin J1-6,  LW port 3  (PWM capable - TPM 0.4 = channel 5)
    { PTA12, PORT_IS_PWM },      // pin J1-8,  LW port 4  (PWM capable - TPM 1.0 = channel 7)
    { PTA4,  PORT_IS_PWM },      // pin J1-10, LW port 5  (PWM capable - TPM 0.1 = channel 2)
    { PTA5,  PORT_IS_PWM },      // pin J1-12, LW port 6  (PWM capable - TPM 0.2 = channel 3)
    { PTA13, PORT_IS_PWM },      // pin J2-2,  LW port 7  (PWM capable - TPM 1.1 = channel 13)
    { PTD5,  PORT_IS_PWM },      // pin J2-4,  LW port 8  (PWM capable - TPM 0.5 = channel 6)
    { PTD0,  PORT_IS_PWM },      // pin J2-6,  LW port 9  (PWM capable - TPM 0.0 = channel 1)
    { PTD3,  PORT_IS_PWM },      // pin J2-10, LW port 10 (PWM capable - TPM 0.3 = channel 4)
    { PTD2,  0 },                // pin J2-8,  LW port 11
    { PTC8,  0 },                // pin J1-14, LW port 12
    { PTC9,  0 },                // pin J1-16, LW port 13
    { PTC7,  0 },                // pin J1-1,  LW port 14
    { PTC0,  0 },                // pin J1-3,  LW port 15
    { PTC3,  0 },                // pin J1-5,  LW port 16
    { PTC4,  0 },                // pin J1-7,  LW port 17
    { PTC5,  0 },                // pin J1-9,  LW port 18
    { PTC6,  0 },                // pin J1-11, LW port 19
    { PTC10, 0 },                // pin J1-13, LW port 20
    { PTC11, 0 },                // pin J1-15, LW port 21
    { PTE0,  0 },                // pin J2-18, LW port 22
    { NC,    0 },                // Not connected,  LW port 23
    { NC,    0 },                // Not connected,  LW port 24
    { NC,    0 },                // Not connected,  LW port 25
    { NC,    0 },                // Not connected,  LW port 26
    { NC,    0 },                // Not connected,  LW port 27
    { NC,    0 },                // Not connected,  LW port 28
    { NC,    0 },                // Not connected,  LW port 29
    { NC,    0 },                // Not connected,  LW port 30
    { NC,    0 },                // Not connected,  LW port 31
    { NC,    0 }                 // Not connected,  LW port 32
    
#elif defined(EXPANSION_BOARD)

    // *** EXPANSION BOARD MODE ***
    // 
    // This mapping is for the expansion board, which uses four TLC5940
    // chips to provide 64  outputs.  The expansion board also uses
    // one GPIO pin to provide a digital (non-PWM) output dedicated to
    // the knocker circuit.  That's on a digital pin because it's used
    // to trigger an external timer circuit that limits the amount of
    // time that the knocker coil can be continuously energized, to protect
    // it against software faults on the PC that leave the port stuck on.
    // (The knocker coil is unique among standard virtual cabinet output
    // devices in this respect - it's the only device in common use that
    // can be damaged if left on for too long.  Other devices won't be
    // damaged, so they don't require such elaborate precautions.)
    //
    // The specific device assignments in the last column are just 
    // recommendations - you can assign any port to any device with 
    // compatible power needs.  The "General Purpose" ports are good to
    // at least 5A, so you can use these for virtually anything.  The
    // "Button light" ports are good to about 1.5A, so these are most
    // suitable for smaller loads like lamps, flashers, LEDs, etc.  The
    // flipper and magnasave ports will only provide 20mA, so these are
    // only usable for small LEDs.

    // The first 32 ports are LedWiz-compatible, so they're universally
    // accessible, even to older non-DOF software.  Attach the most common
    // devices to these ports.
    { NC,     0,    1 },         // TLC port 1,  LW output 1  - Flasher 1 R
    { NC,     0,    2 },         // TLC port 2,  LW output 2  - Flasher 1 G
    { NC,     0,    3 },         // TLC port 3,  LW output 3  - Flasher 1 B
    { NC,     0,    4 },         // TLC port 4,  LW output 4  - Flasher 2 R
    { NC,     0,    5 },         // TLC port 5,  LW output 5  - Flasher 2 G
    { NC,     0,    6 },         // TLC port 6,  LW output 6  - Flasher 2 B
    { NC,     0,    7 },         // TLC port 7,  LW output 7  - Flasher 3 R
    { NC,     0,    8 },         // TLC port 8,  LW output 8  - Flasher 3 G
    { NC,     0,    9 },         // TLC port 9,  LW output 9  - Flasher 3 B
    { NC,     0,    10 },        // TLC port 10, LW output 10 - Flasher 4 R
    { NC,     0,    11 },        // TLC port 11, LW output 11 - Flasher 4 G
    { NC,     0,    12 },        // TLC port 12, LW output 12 - Flasher 4 B
    { NC,     0,    13 },        // TLC port 13, LW output 13 - Flasher 5 R
    { NC,     0,    14 },        // TLC port 14, LW output 14 - Flasher 5 G
    { NC,     0,    15 },        // TLC port 15, LW output 15 - Flasher 5 B
    { NC,     0,    16 },        // TLC port 16, LW output 16 - Strobe/Button light
    { NC,     0,    17 },        // TLC port 17, LW output 17 - Button light 1
    { NC,     0,    18 },        // TLC port 18, LW output 18 - Button light 2
    { NC,     0,    19 },        // TLC port 19, LW output 19 - Button light 3
    { NC,     0,    20 },        // TLC port 20, LW output 20 - Button light 4
    { PTC8,   0,    0 },         // PTC8,        LW output 21 - Replay Knocker
    { NC,     0,    21 },        // TLC port 21, LW output 22 - Contactor 1/General purpose
    { NC,     0,    22 },        // TLC port 22, LW output 23 - Contactor 2/General purpose
    { NC,     0,    23 },        // TLC port 23, LW output 24 - Contactor 3/General purpose
    { NC,     0,    24 },        // TLC port 24, LW output 25 - Contactor 4/General purpose
    { NC,     0,    25 },        // TLC port 25, LW output 26 - Contactor 5/General purpose
    { NC,     0,    26 },        // TLC port 26, LW output 27 - Contactor 6/General purpose
    { NC,     0,    27 },        // TLC port 27, LW output 28 - Contactor 7/General purpose
    { NC,     0,    28 },        // TLC port 28, LW output 29 - Contactor 8/General purpose
    { NC,     0,    29 },        // TLC port 29, LW output 30 - Contactor 9/General purpose
    { NC,     0,    30 },        // TLC port 30, LW output 31 - Contactor 10/General purpose
    { NC,     0,    31 },        // TLC port 31, LW output 32 - Shaker Motor/General purpose
    
    // Ports 33+ are accessible only to DOF-based software.  Older LedWiz-only
    // software on the can't access these.  Attach less common devices to these ports.
    { NC,     0,    32 },        // TLC port 32, LW output 33 - Gear Motor/General purpose
    { NC,     0,    33 },        // TLC port 33, LW output 34 - Fan/General purpose
    { NC,     0,    34 },        // TLC port 34, LW output 35 - Beacon/General purpose
    { NC,     0,    35 },        // TLC port 35, LW output 36 - Undercab RGB R/General purpose
    { NC,     0,    36 },        // TLC port 36, LW output 37 - Undercab RGB G/General purpose
    { NC,     0,    37 },        // TLC port 37, LW output 38 - Undercab RGB B/General purpose
    { NC,     0,    38 },        // TLC port 38, LW output 39 - Bell/General purpose
    { NC,     0,    39 },        // TLC port 39, LW output 40 - Chime 1/General purpose
    { NC,     0,    40 },        // TLC port 40, LW output 41 - Chime 2/General purpose
    { NC,     0,    41 },        // TLC port 41, LW output 42 - Chime 3/General purpose
    { NC,     0,    42 },        // TLC port 42, LW output 43 - General purpose
    { NC,     0,    43 },        // TLC port 43, LW output 44 - General purpose
    { NC,     0,    44 },        // TLC port 44, LW output 45 - Button light 5
    { NC,     0,    45 },        // TLC port 45, LW output 46 - Button light 6
    { NC,     0,    46 },        // TLC port 46, LW output 47 - Button light 7
    { NC,     0,    47 },        // TLC port 47, LW output 48 - Button light 8
    { NC,     0,    49 },        // TLC port 49, LW output 49 - Flipper button RGB left R
    { NC,     0,    50 },        // TLC port 50, LW output 50 - Flipper button RGB left G
    { NC,     0,    51 },        // TLC port 51, LW output 51 - Flipper button RGB left B
    { NC,     0,    52 },        // TLC port 52, LW output 52 - Flipper button RGB right R
    { NC,     0,    53 },        // TLC port 53, LW output 53 - Flipper button RGB right G
    { NC,     0,    54 },        // TLC port 54, LW output 54 - Flipper button RGB right B
    { NC,     0,    55 },        // TLC port 55, LW output 55 - MagnaSave button RGB left R
    { NC,     0,    56 },        // TLC port 56, LW output 56 - MagnaSave button RGB left G
    { NC,     0,    57 },        // TLC port 57, LW output 57 - MagnaSave button RGB left B
    { NC,     0,    58 },        // TLC port 58, LW output 58 - MagnaSave button RGB right R
    { NC,     0,    59 },        // TLC port 59, LW output 59 - MagnaSave button RGB right G
    { NC,     0,    60 }         // TLC port 60, LW output 60 - MagnaSave button RGB right B
    
#else

    // *** TLC5940 + GPIO OUTPUTS, Without the expansion board ***
    //
    // This is the mapping for the ehnanced mode, with one or more TLC5940 
    // chips connected.  Each TLC5940 chip provides 16 PWM channels.  We
    // can supplement the TLC5940 outputs with GPIO pins to get even more 
    // physical outputs.  
    //
    // Because we've already declared the number of TLC5940 chips earlier
    // in this file, we don't actually have to map out all of the TLC5940
    // ports here.  The software will automatically assign all of the 
    // TLC5940 ports that aren't explicitly mentioned here to the next
    // available LedWiz port numbers after the end of this array, assigning
    // them sequentially in TLC5940 port order.
    //
    // In contrast to the basic mode arrangement, we're putting all of the
    // NON PWM ports first in this mapping.  The logic is that all of the 
    // TLC5940 ports are PWM-capable, and they'll all at the end of the list
    // here, so by putting the PWM GPIO pins last here, we'll keep all of the
    // PWM ports grouped in the final mapping.
    //
    // Note that the TLC5940 control wiring takes away several GPIO pins
    // that we used as output ports in the basic mode.  Further, because the
    // TLC5940 makes ports so plentiful, we're intentionally omitting several 
    // more of the pins from the basic set, to make them available for other
    // uses.  To keep things more neatly grouped, we're only assigning J1 pins
    // in this set.  This leaves the following ports from the basic mode output
    // set available for other users: PTA13, PTD0, PTD2, PTD3, PTD5, PTE0.
    
    { PTC8,  0 },                // pin J1-14, LW port 1
    { PTC9,  0 },                // pin J1-16, LW port 2
    { PTC0,  0 },                // pin J1-3,  LW port 3
    { PTC3,  0 },                // pin J1-5,  LW port 4
    { PTC4,  0 },                // pin J1-7,  LW port 5
    { PTA2,  PORT_IS_PWM },      // pin J1-4,  LW port 6   (PWM capable - TPM 2.1 = channel 10)
    { PTD4,  PORT_IS_PWM },      // pin J1-6,  LW port 7   (PWM capable - TPM 0.4 = channel 5)
    { PTA12, PORT_IS_PWM },      // pin J1-8,  LW port 8   (PWM capable - TPM 1.0 = channel 7)
    { PTA4,  PORT_IS_PWM },      // pin J1-10, LW port 9   (PWM capable - TPM 0.1 = channel 2)
    { PTA5,  PORT_IS_PWM }       // pin J1-12, LW port 10  (PWM capable - TPM 0.2 = channel 3)

    // TLC5940 ports start here!
    // First chip port 0 ->   LW port 12
    // First chip port 1 ->   LW port 13
    // ... etc, filling out all chip ports sequentially ...

#endif // TLC5940_NCHIPS
};


#endif // DECL_EXTERNS
