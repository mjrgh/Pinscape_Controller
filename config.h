// Pinscape Controller Configuration
//
// New for 2016:  dynamic configuration!  To configure the controller, connect
// the KL25Z to your PC, install the .bin file, and run the Windows config tool.  
// There's no need (as there was in the past) to edit this file or to compile a 
// custom version of the binary (.bin) to customize setup options.
//
// In earlier versions, configuration was largely handled with compile-time
// constants.  To customize the setup, you had to create a private forked copy
// of the source code, edit the constants defined in config.h, and compile a
// custom binary.  That's no longer necessary!
//
// The new approach is to do everything (or as much as possible, anyway)
// via the Windows config tool.  You shouldn't have to recompile a custom
// version just to make a configurable change.  Of course, you're still free
// to create a custom version if you need to add or change features in ways
// that weren't anticipated in the original design. 
//


#ifndef CONFIG_H
#define CONFIG_H


// Plunger type codes
// NOTE!  These values are part of the external USB interface.  New
// values can be added, but the meaning of an existing assigned number 
// should remain fixed to keep the PC-side config tool compatible across 
// versions.
const int PlungerType_None      = 0;     // no plunger
const int PlungerType_TSL1410RS = 1;     // TSL1410R linear image sensor (1280x1 pixels, 400dpi), serial mode
const int PlungerType_TSL1410RP = 2;     // TSL1410R, parallel mode (reads the two sensor sections concurrently)
const int PlungerType_TSL1412RS = 3;     // TSL1412R linear image sensor (1536x1 pixels, 400dpi), serial mode
const int PlungerType_TSL1412RP = 4;     // TSL1412R, parallel mode
const int PlungerType_Pot       = 5;     // potentionmeter
const int PlungerType_OptQuad   = 6;     // AEDR8300 optical quadrature sensor
const int PlungerType_MagQuad   = 7;     // AS5304 magnetic quadrature sensor

// Accelerometer orientation codes
// These values are part of the external USB interface
const int OrientationFront     = 0;      // USB ports pointed toward front of cabinet
const int OrientationLeft      = 1;      // ports pointed toward left side of cabinet
const int OrientationRight     = 2;      // ports pointed toward right side of cabinet
const int OrientationRear      = 3;      // ports pointed toward back of cabinet

// input button types
const int BtnTypeJoystick      = 1;      // joystick button
const int BtnTypeKey           = 2;      // regular keyboard key
const int BtnTypeModKey        = 3;      // keyboard modifier key (shift, ctrl, etc)
const int BtnTypeMedia         = 4;      // media control key (volume up/down, etc)

// maximum number of input button mappings
const int MAX_BUTTONS = 32;

// LedWiz output port type codes
// These values are part of the external USB interface
const int PortTypeDisabled     = 0;      // port is disabled - not visible to LedWiz/DOF host
const int PortTypeGPIOPWM      = 1;      // GPIO port, PWM enabled
const int PortTypeGPIODig      = 2;      // GPIO port, digital out
const int PortTypeTLC5940      = 3;      // TLC5940 port
const int PortType74HC595      = 4;      // 74HC595 port
const int PortTypeVirtual      = 5;      // Virtual port - visible to host software, but not connected to a physical output

// LedWiz output port flag bits
const uint8_t PortFlagActiveLow = 0x01;  // physical output is active-low

// maximum number of output ports
const int MAX_OUT_PORTS = 203;

struct Config
{
    // set all values to factory defaults
    void setFactoryDefaults()
    {
        // By default, pretend to be LedWiz unit #8.  This can be from 1 to 16.  Real
        // LedWiz units have their unit number set at the factory, and the vast majority
        // are set up as unit #1, since that's the default for anyone who doesn't ask
        // for a different setting.  It seems rare for anyone to use more than one unit
        // in a pin cab, but for the few who do, the others will probably be numbered
        // sequentially as #2, #3, etc.  It seems safe to assume that no one out there
        // has a unit #8, so we'll use that as our default starting number.  This can
        // be changed from the config tool, but for the sake of convenience we want the
        // default to be a value that most people won't have to change.
        usbVendorID = 0xFAFA;      // LedWiz vendor code
        usbProductID = 0x00F7;     // LedWiz product code for unit #8
        psUnitNo = 8;
        
        // enable joystick reports
        joystickEnabled = true;
        
        // assume standard orientation, with USB ports toward front of cabinet
        orientation = OrientationFront;

        // assume no plunger is attached
        plunger.enabled = false;
        plunger.sensorType = PlungerType_None;
        
        // assume that there's no calibration button
        plunger.cal.btn = NC;
        plunger.cal.led = NC;
        
        // clear the plunger calibration
        plunger.cal.reset(4096);
        
        // disable the ZB Launch Ball by default
        plunger.zbLaunchBall.port = 0;
        plunger.zbLaunchBall.btn = 0;
        
        // assume no TV ON switch
        TVON.statusPin = NC;
        TVON.latchPin = NC;
        TVON.relayPin = NC;
        TVON.delayTime = 0;
        
        // assume no TLC5940 chips
        tlc5940.nchips = 0;
        
        // assume no 74HC595 chips
        hc595.nchips = 0;
        
        // initially configure with no LedWiz output ports
        outPort[0].typ = PortTypeDisabled;
        
        // initially configure with no input buttons
        for (int i = 0 ; i < MAX_BUTTONS ; ++i)
            button[i].pin = 0;   // 0 == index of NC in USB-to-PinName mapping
            
        button[0].pin = 6; // PTA13
        button[0].typ = BtnTypeKey;
        button[0].val = 4;  // A
        button[1].pin = 38; // PTD5
        button[1].typ = BtnTypeJoystick;
        button[1].val = 5;  // B
        button[2].pin = 37; // PTD4
        button[2].typ = BtnTypeModKey;
        button[2].val = 0x02;  // left shift
        button[3].pin = 5;  // PTA12
        button[3].typ = BtnTypeMedia;
        button[3].val = 0x01;  // volume up
        button[4].pin = 3;  // PTA4
        button[4].typ = BtnTypeMedia;
        button[4].val = 0x02;  // volume down
    }        
    
    // --- USB DEVICE CONFIGURATION ---
    
    // USB device identification - vendor ID and product ID.  For LedLWiz
    // emulation, use vendor ID 0xFAFA and product ID 0x00EF + unit#, where
    // unit# is the nominal LedWiz unit number from 1 to 16.  Alternatively,
    // if LedWiz emulation isn't desired or causes any driver conflicts on
    // the host, we have a private Pinscape assignment as vendor ID 0x1209 
    // and product ID 0xEAEA (registered with http://pid.codes, a registry
    // for open-source USB projects).
    uint16_t usbVendorID;
    uint16_t usbProductID;
    
    // Pinscape Controller unit number.  This is the nominal unit number,
    // from 1 to 16.  We report this in the status query; DOF uses it to
    // distinguish multiple Pinscape units.  Note that this doesn't affect 
    // the LedWiz unit numbering, which is implied by the USB Product ID.
    uint8_t psUnitNo;
            
    // Are joystick reports enabled?  Joystick reports can be turned off, to
    // use the device as purely an output controller.
    char joystickEnabled;
    
    
    // --- ACCELEROMETER ---
    
    // accelerometer orientation (ORIENTATION_xxx value)
    char orientation;
    
    
    // --- PLUNGER CONFIGURATION ---
    struct
    {
        // plunger enabled/disabled
        char enabled;

        // plunger sensor type
        char sensorType;
    
        // Plunger sensor pins.  To accommodate a wide range of sensor types,
        // we keep a generic list of 4 pin assignments.  The use of each pin
        // varies by sensor.  The lists below are in order of the generic
        // pins; NC means that the pin isn't used by the sensor.  Each pin's
        // GPIO usage is also listed.  Certain usages limit which physical
        // pins can be assigned (e.g., AnalogIn or PwmOut).
        //
        // TSL1410R/1412R, serial:    SI (DigitalOut), CLK (DigitalOut), AO (AnalogIn),  NC
        // TSL1410R/1412R, parallel:  SI (DigitalOut), CLK (DigitalOut), AO1 (AnalogIn), AO2 (AnalogIn)
        // Potentiometer:             AO (AnalogIn),   NC,               NC,             NC
        // AEDR8300:                  A (InterruptIn), B (InterruptIn),  NC,             NC
        // AS5304:                    A (InterruptIn), B (InterruptIn),  NC,             NC
        PinName sensorPin[4];
        
        // Pseudo LAUNCH BALL button.  
        //
        // This configures the "ZB Launch Ball" feature in DOF, based on Zeb's (of 
        // zebsboards.com) scheme for using a mechanical plunger as a Launch button.
        // Set the port to 0 to disable the feature.
        //
        // The port number is an LedWiz port number that we monitor for activation.
        // This port isn't connected to a physical device; rather, the host turns it
        // on to indicate that the pseudo Launch button mode is in effect.  
        //
        // The button number gives the button that we "press" when a launch occurs.
        // This can be connected to the physical Launch button, or can simply be
        // an otherwise unused button.
        //
        // The "push distance" is the distance, in inches, for registering a push
        // on the plunger as a button push.  If the player pushes the plunger forward
        // of the rest position by this amount, we'll treat it as pushing the button,
        // even if the player didn't pull back the plunger first.  This lets the
        // player treat the plunger knob as a button for games where it's meaningful
        // to hold down the Launch button for specific intervals (e.g., "Championship 
        // Pub").
        struct
        {
            int port;
            int btn;
            float pushDistance;
        
        } zbLaunchBall;
           
        // --- PLUNGER CALIBRATION ---
        struct
        {
            // has the plunger been calibrated?
            int calibrated;
        
            // calibration button switch pin
            PinName btn;
        
            // calibration button indicator light pin
            PinName led;
            
            // Plunger calibration min, zero, and max.  The zero point is the 
            // rest position (aka park position), where it's in equilibrium between 
            // the main spring and the barrel spring.  It can travel a small distance
            // forward of the rest position, because the barrel spring can be
            // compressed by the user pushing on the plunger or by the momentum
            // of a release motion.  The minimum is the maximum forward point where
            // the barrel spring can't be compressed any further.
            int min;
            int zero;
            int max;
    
            // reset the plunger calibration
            void reset(int npix)
            {
                calibrated = 0;          // not calibrated
                min = 0;                 // assume we can go all the way forward...
                max = npix;              // ...and all the way back
                zero = npix/6;           // the rest position is usually around 1/2" back = 1/6 of total travel
            }

        } cal;

    } plunger;

    
    // --- TV ON SWITCH ---
    //
    // To use the TV ON switch feature, the special power sensing circuitry
    // implemented on the Expansion Board must be attached (or an equivalent
    // circuit, as described in the Build Guide).  The circuitry lets us
    // detect power state changes on the secondary power supply.
    struct 
    {
        // PSU2 power status sense (DigitalIn pin).  This pin goes LOW when the
        // secondary power supply is turned off, and remains LOW until the LATCH
        // pin is raised high AND the secondary PSU is turned on.  Once HIGH,
        // it remains HIGH as long as the secondary PSU is on.
        PinName statusPin;
    
        // PSU2 power status latch (DigitalOut pin)
        PinName latchPin;
        
        // TV ON relay pin (DigitalOut pin).  This pin controls the TV switch 
        // relay.  Raising the pin HIGH turns the relay ON (energizes the coil).
        PinName relayPin;
        
        // TV ON delay time, in seconds.  This is the interval between sensing
        // that the secondary power supply has turned on and pulsing the TV ON
        // switch relay.  
        float delayTime;
    
    } TVON;
    

    // --- TLC5940NT PWM Controller Chip Setup ---
    struct
    {
        // number of TLC5940NT chips connected in daisy chain
        int nchips;
        
        // pin connections
        PinName sin;        // Serial data - must connect to SPIO MOSI -> PTC6 or PTD2
        PinName sclk;       // Serial clock - must connect to SPIO SCLK -> PTC5 or PTD1
                            // (but don't use PTD1, since it's hard-wired to the on-board blue LED)
        PinName xlat;       // XLAT (latch) signal - connect to any GPIO pin
        PinName blank;      // BLANK signal - connect to any GPIO pin
        PinName gsclk;      // Grayscale clock - must connect to a PWM-out capable pin

    } tlc5940; 
    

    // --- 74HC595 Shift Register Setup ---
    struct
    {
        // number of 74HC595 chips attached in daisy chain
        int nchips;
        
        // pin connections
        PinName sin;        // Serial data - use any GPIO pin
        PinName sclk;       // Serial clock - use any GPIO pin
        PinName latch;      // Latch - use any GPIO pin
        PinName ena;        // Enable signal - use any GPIO pin
    
    } hc595;


    // --- Button Input Setup ---
    struct
    {
        uint8_t pin;        // physical input GPIO pin - a USB-to-PinName mapping index
        uint8_t typ;        // key type reported to PC - a BtnTypeXxx value
        uint8_t val;        // key value reported - meaning depends on 'typ' value
        
    } button[MAX_BUTTONS];
    

    // --- LedWiz Output Port Setup ---
    struct
    {
        uint8_t typ;        // port type:  a PortTypeXxx value
        uint8_t pin;        // physical output pin:  for a GPIO port, this is an index in the 
                            // USB-to-PinName mapping list; for a TLC5940 or 74HC595 port, it's 
                            // the output number, starting from 0 for OUT0 on the first chip in 
                            // the daisy chain.  For inactive and virtual ports, it's unused.
        uint8_t flags;      // flags:  a combination of PortFlagXxx values
    } outPort[MAX_OUT_PORTS];
};

#endif
