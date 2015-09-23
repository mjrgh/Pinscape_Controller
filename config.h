// Pinscape Controller Configuration
//
// To customize your private configuration, simply open this file in the 
// mbed on-line IDE, make your changes, save the file, and click the Compile
// button at the top of the window.  That will generate a customized .bin
// file that you can download onto your KL25Z board.

#ifndef CONFIG_H
#define CONFIG_H

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
// LedWiz default unit number.
//
// Each LedWiz device has a unit number, from 1 to 16.  This lets you install
// more than one LedWiz in your system: as long as each one has a different
// unit number, the software on the PC can tell them apart and route commands 
// to the right device.
//
// A *real* LedWiz has its unit number set at the factory; they set it to
// unit 1 unless you specifically request a different number when you place
// your order.
//
// For our *emulated* LedWiz, we default to unit #8.  However, if we're set 
// up as a secondary Pinscape controller with the joystick functions turned 
// off, we'll use unit #9 instead.  
//
// The reason we start at unit #8 is that we want to avoid conflicting with
// any real LedWiz devices you have in your system.  If you have a real
// LedWiz, it's probably unit #1, since that's the standard factor setting.
// If you have two real LedWiz's, they're probably units #1 and #2.  If you 
// have three... well, I don't think anyone actually has three, but if you 
// did it would probably be unit #3.  And so on.  That's why we start at #8 - 
// it seems really unlikely that this will conflict with anybody's existing 
// setup.  On the off chance it does, simply change the setting here to a 
// different unit number that's not already used in your system.
//
// Note 1:  the unit number here is the *user visible* unit number that
// you use on the PC side.  It's the number you specify in your DOF
// configuration and so forth.  Internally, the USB reports subtract
// one from this number - e.g., nominal unit #1 shows up as 0 in the USB
// reports.  If you're trying to puzzle out why all of the USB reports
// are all off by one from the unit number you select here, that's why.
//
// Note 2:  the DOF Configtool (google it) knows about the Pinscape 
// controller (it's known there as just a "KL25Z" rather than Pinscape).
// And the DOF tool knows that it uses #8 as its default unit number, so
// it names the .ini file for this controller xxx8.ini.  If you change the 
// unit number here, remember to rename the DOF-generated .ini file to 
// match, by changing the "8" at the end of the filename to the new number
// you set here.
const uint8_t DEFAULT_LEDWIZ_UNIT_NUMBER = 
#ifdef ENABLE_JOYSTICK
   0x08;   // joystick enabled - assume we're the primary KL25Z, so use unit #8
#else
   0x09;   // joystick disabled - assume we're a secondary, output-only KL25Z, so use #9
#endif

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
// Note that if you're using TLC5940 outputs, ALL of the outputs must go
// through the TLC5940s - you can't mix TLC5940s and the default GPIO
// device outputs.  This lets us take GPIO ports that we'd normally use
// for device outputs and reassign them to control the TLC5940 hardware.

// Uncomment this line if using TLC5940 chips
//#define ENABLE_TLC5940

// Number of TLC5940 chips you're using.  For a full LedWiz-compatible
// setup, you need two of these chips, for 32 outputs.
#define TLC5940_NCHIPS   2

// If you're using TLC5940s, change any of these as needed to match the
// GPIO pins that you connected to the TLC5940 control pins.  Note that
// SIN and SCLK *must* be connected to the KL25Z SPI0 MOSI and SCLK
// outputs, respectively, which effectively limits them to the default
// selections, and that the GSCLK pin must be PWM-capable.
#define TLC5940_SIN    PTC6    // Must connect to SPI0 MOSI -> PTC6 or PTD2
#define TLC5940_SCLK   PTC5    // Must connect to SPI0 SCLK -> PTC5 or PTD1; however, PTD1 isn't
                               //   recommended because it's hard-wired to the on-board blue LED
#define TLC5940_XLAT   PTC10   // Any GPIO pin can be used
#define TLC5940_BLANK  PTC0    // Any GPIO pin can be used
#define TLC5940_GSCLK  PTD4    // Must be a PWM-capable pin

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
// complete a scan within the cycle time for USB reports.  USB gives us a
// whole separate timing factor; we can't go much *faster* with USB than
// sending a new report about every 10ms.  The sensor timing is such that
// we can read about 165 pixels in well under 10ms.  So that's really the
// sweet spot for our scans.
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

#endif // CONFIG_H


#ifdef DECL_EXTERNS
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
// LED-Wiz emulation output pin assignments.  
//
//   NOTE!  This section isn't used if you have TLC5940 outputs - ALL
//   device outputs will be through the 5940s if you're using them.
//   See the TLC5940 setup section above to configure your interface
//   pins if you're using those chips.
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
// Ports with pins assigned as "NC" are not connected.  That is,
// there's no physical pin for that LedWiz port number.  You can
// send LedWiz commands to turn NC ports on and off, but doing so
// will have no effect.  The reason we leave some ports unassigned
// is that we don't have enough physical GPIO pins to fill out the
// full LedWiz complement of 32 ports.  Many pins are already taken
// for other purposes, such as button inputs or the plunger CCD
// interface.
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
// Note: PTD1 (pin J2-12) should NOT be assigned as an LedWiz output,
// as this pin is physically connected on the KL25Z to the on-board
// indicator LED's blue segment.  This precludes any other use of
// the pin.
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
    { PTD2, false },     // pin J2-8,  LW port 11
    { PTC8, false },     // pin J1-14, LW port 12
    { PTC9, false },     // pin J1-16, LW port 13
    { PTC7, false },     // pin J1-1,  LW port 14
    { PTC0, false },     // pin J1-3,  LW port 15
    { PTC3, false },     // pin J1-5,  LW port 16
    { PTC4, false },     // pin J1-7,  LW port 17
    { PTC5, false },     // pin J1-9,  LW port 18
    { PTC6, false },     // pin J1-11, LW port 19
    { PTC10, false },    // pin J1-13, LW port 20
    { PTC11, false },    // pin J1-15, LW port 21
    { PTE0, false },     // pin J2-18, LW port 22
    { NC, false },       // Not connected,  LW port 23
    { NC, false },       // Not connected,  LW port 24
    { NC, false },       // Not connected,  LW port 25
    { NC, false },       // Not connected,  LW port 26
    { NC, false },       // Not connected,  LW port 27
    { NC, false },       // Not connected,  LW port 28
    { NC, false },       // Not connected,  LW port 29
    { NC, false },       // Not connected,  LW port 30
    { NC, false },       // Not connected,  LW port 31
    { NC, false }        // Not connected,  LW port 32
};


#endif // DECL_EXTERNS
