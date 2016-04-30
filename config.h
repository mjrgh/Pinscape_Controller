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

// $$$ TESTING CONFIGURATIONS
#define TEST_CONFIG_EXPAN     1
#define TEST_CONFIG_CAB       0
#define TEST_KEEP_PRINTF      1


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
const int BtnTypeNone          = 0;      // unused
const int BtnTypeJoystick      = 1;      // joystick button
const int BtnTypeKey           = 2;      // keyboard key

// input button flags
const uint8_t BtnFlagPulse     = 0x01;   // pulse mode - reports each change in the physical switch state
                                         // as a brief press of the logical button/keyboard key
                                         
// button setup structure
struct ButtonCfg
{
    uint8_t pin;        // physical input GPIO pin - a USB-to-PinName mapping index
    uint8_t typ;        // key type reported to PC - a BtnTypeXxx value
    uint8_t val;        // key value reported - meaning depends on 'typ' value:
                        //   none     -> no PC input reports (val is unused)
                        //   joystick -> val is joystick button number (1..32)
                        //   keyboard -> val is USB scan code
    uint8_t flags;      // key flags - a bitwise combination of BtnFlagXxx values

    void set(uint8_t pin, uint8_t typ, uint8_t val, uint8_t flags = 0)
    {
        this->pin = pin;
        this->typ = typ;
        this->val = val;
        this->flags = flags;
    }
        
} __attribute__((packed));
    

// maximum number of input button mappings
const int MAX_EXT_BUTTONS = 32;             // buttons visible through USB interface
const int VIRTUAL_BUTTONS = 1;              // number of internal virtual buttons
const int ZBL_BUTTON = MAX_EXT_BUTTONS;     // index of virtual ZB Launch Ball button
const int MAX_BUTTONS = MAX_EXT_BUTTONS + VIRTUAL_BUTTONS;  // total button slots

// LedWiz output port type codes
// These values are part of the external USB interface
const int PortTypeDisabled     = 0;      // port is disabled - not visible to LedWiz/DOF host
const int PortTypeGPIOPWM      = 1;      // GPIO port, PWM enabled
const int PortTypeGPIODig      = 2;      // GPIO port, digital out
const int PortTypeTLC5940      = 3;      // TLC5940 port
const int PortType74HC595      = 4;      // 74HC595 port
const int PortTypeVirtual      = 5;      // Virtual port - visible to host software, but not connected 
                                         //  to a physical output

// LedWiz output port flag bits
const uint8_t PortFlagActiveLow  = 0x01; // physical output is active-low
const uint8_t PortFlagNoisemaker = 0x02; // noisemaker device - disable when night mode is engaged
const uint8_t PortFlagGamma      = 0x04; // apply gamma correction to this output

// maximum number of output ports
const int MAX_OUT_PORTS = 128;

// port configuration data
struct LedWizPortCfg
{
    uint8_t typ;        // port type:  a PortTypeXxx value
    uint8_t pin;        // physical output pin:  for a GPIO port, this is an index in the 
                        // USB-to-PinName mapping list; for a TLC5940 or 74HC595 port, it's 
                        // the output number, starting from 0 for OUT0 on the first chip in 
                        // the daisy chain.  For inactive and virtual ports, it's unused.
    uint8_t flags;      // flags:  a combination of PortFlagXxx values
    
    void set(uint8_t typ, uint8_t pin, uint8_t flags = 0)
    {
        this->typ = typ;
        this->pin = pin;
        this->flags = flags;
    }
        
} __attribute__((packed));


// Convert a physical pin name to a wire pin name
#define PINNAME_TO_WIRE(p) \
    uint8_t((p) == NC ? 0xFF : \
      (((p) & 0xF000 ) >> (PORT_SHIFT - 5)) | (((p) & 0xFF) >> 2))

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
        // has a unit #8, so we'll use that as our default.  This can be changed from 
        // the config tool, but for the sake of convenience, it's better to pick a
        // default that most people won't have to change.
        usbVendorID = 0xFAFA;      // LedWiz vendor code 
        usbProductID = 0x00F7;     // LedWiz product code for unit #8
        psUnitNo = 8;
        
        // set a disconnect reboot timeout of 10 seconds by default
        disconnectRebootTimeout = 60; // $$$
        
        // enable joystick reports
        joystickEnabled = true;
        
        // assume standard orientation, with USB ports toward front of cabinet
        orientation = OrientationFront;

        // assume a basic setup with no expansion boards
        expan.typ = 0;
        expan.vsn = 0;
        memset(expan.ext, 0, sizeof(expan.ext));

        // assume no plunger is attached
        plunger.enabled = false;
        plunger.sensorType = PlungerType_None;
        
#if TEST_CONFIG_EXPAN || TEST_CONFIG_CAB // $$$
        plunger.enabled = true;
        plunger.sensorType = PlungerType_TSL1410RS;
        plunger.sensorPin[0] = PINNAME_TO_WIRE(PTE20); // SI
        plunger.sensorPin[1] = PINNAME_TO_WIRE(PTE21); // SCLK
        plunger.sensorPin[2] = PINNAME_TO_WIRE(PTB0);  // AO1 = PTB0 = ADC0_SE8
        plunger.sensorPin[3] = PINNAME_TO_WIRE(PTE22); // AO2 (parallel mode) = PTE22 = ADC0_SE3
#endif
        
        // default plunger calibration button settings
        plunger.cal.btn = PINNAME_TO_WIRE(PTE29);
        plunger.cal.led = PINNAME_TO_WIRE(PTE23);
        
        // set the default plunger calibration
        plunger.cal.setDefaults();
        
        // disable the ZB Launch Ball by default
        plunger.zbLaunchBall.port = 0;                  // 0 = disabled
        plunger.zbLaunchBall.keytype = 2;               // keyboard key
        plunger.zbLaunchBall.keycode = 0x28;            // Enter key (USB scan code)
        plunger.zbLaunchBall.pushDistance = 63;         // about 1/16"
        
        // assume no TV ON switch
        TVON.statusPin = PINNAME_TO_WIRE(NC);
        TVON.latchPin = PINNAME_TO_WIRE(NC);
        TVON.relayPin = PINNAME_TO_WIRE(NC);
        TVON.delayTime = 700;   // 7 seconds
#if TEST_CONFIG_EXPAN //$$$
        TVON.statusPin = PINNAME_TO_WIRE(PTD2);
        TVON.latchPin = PINNAME_TO_WIRE(PTE0);
        TVON.relayPin = PINNAME_TO_WIRE(PTD3);
        TVON.delayTime = 700;   // 7 seconds
#endif

        // assume no night mode switch or indicator lamp
        nightMode.btn = 0;
        nightMode.flags = 0;
        nightMode.port = 0;
        
        // assume no TLC5940 chips
        tlc5940.nchips = 0;
#if TEST_CONFIG_EXPAN // $$$
        tlc5940.nchips = 4;
#endif

        // default TLC5940 pin assignments
        tlc5940.sin = PINNAME_TO_WIRE(PTC6);
        tlc5940.sclk = PINNAME_TO_WIRE(PTC5);
        tlc5940.xlat = PINNAME_TO_WIRE(PTC10);
        tlc5940.blank = PINNAME_TO_WIRE(PTC7);
        tlc5940.gsclk = PINNAME_TO_WIRE(PTA1);
        
        // assume no 74HC595 chips
        hc595.nchips = 0;
#if TEST_CONFIG_EXPAN // $$$
        hc595.nchips = 1;
#endif
    
        // default 74HC595 pin assignments
        hc595.sin = PINNAME_TO_WIRE(PTA5);
        hc595.sclk = PINNAME_TO_WIRE(PTA4);
        hc595.latch = PINNAME_TO_WIRE(PTA12);
        hc595.ena = PINNAME_TO_WIRE(PTD4);
        
        // initially configure with no LedWiz output ports
        outPort[0].typ = PortTypeDisabled;
            
        // initially configure with no input buttons
        for (int i = 0 ; i < MAX_BUTTONS ; ++i)
            button[i].set(PINNAME_TO_WIRE(NC), BtnTypeNone, 0);

#if TEST_CONFIG_EXPAN | TEST_CONFIG_CAB
        for (int i = 0 ; i < 24 ; ++i) {
            static int bp[] = {
                PINNAME_TO_WIRE(PTC2),  // 1
                PINNAME_TO_WIRE(PTB3),  // 2
                PINNAME_TO_WIRE(PTB2),  // 3
                PINNAME_TO_WIRE(PTB1),  // 4
                PINNAME_TO_WIRE(PTE30), // 5 
#if TEST_CONFIG_EXPAN
                PINNAME_TO_WIRE(PTC11), // 6 
#elif TEST_CONFIG_CAB
                PINNAME_TO_WIRE(PTE22), // 6 
#endif
                PINNAME_TO_WIRE(PTE5),  // 7
                PINNAME_TO_WIRE(PTE4),  // 8
                PINNAME_TO_WIRE(PTE3),  // 9
                PINNAME_TO_WIRE(PTE2),  // 10
                PINNAME_TO_WIRE(PTB11), // 11 
                PINNAME_TO_WIRE(PTB10), // 12 
                PINNAME_TO_WIRE(PTB9),  // 13
                PINNAME_TO_WIRE(PTB8),  // 14
                PINNAME_TO_WIRE(PTC12), // 15 
                PINNAME_TO_WIRE(PTC13), // 16 
                PINNAME_TO_WIRE(PTC16), // 17 
                PINNAME_TO_WIRE(PTC17), // 18 
                PINNAME_TO_WIRE(PTA16), // 19 
                PINNAME_TO_WIRE(PTA17), // 20 
                PINNAME_TO_WIRE(PTE31), // 21 
                PINNAME_TO_WIRE(PTD6),  // 22
                PINNAME_TO_WIRE(PTD7),  // 23
                PINNAME_TO_WIRE(PTE1)   // 24
            };               
            button[i].set(bp[i], 
#if TEST_CONFIG_EXPAN
                BtnTypeKey, i+4);       // keyboard key A, B, C... 
#elif TEST_CONFIG_CAB
                BtnTypeJoystick, i+1);  // joystick button 0, 1, ...
#endif

        }
#endif
        
#if 0
        button[23].typ = BtnTypeJoystick;
        button[23].val = 5;  // B
        button[23].flags = 0x01;  // pulse button
        
        button[22].typ = BtnTypeModKey;
        button[22].val = 0xE1;  // left shift
        
        button[21].typ = BtnTypeKey;
        button[21].val = 0x81;  // vol down
        
        button[20].typ = BtnTypeKey;
        button[20].val = 0x80;  // vol up
        
#endif
        

#if TEST_CONFIG_EXPAN // $$$
        // CONFIGURE EXPANSION BOARD PORTS
        //
        // We have the following hardware attached:
        //
        //   Main board
        //     TLC ports 0-15  -> flashers
        //     TLC ports 16    -> strobe
        //     TLC ports 17-31 -> flippers
        //     Dig GPIO PTC8   -> knocker (timer-protected outputs)
        //
        //   Power board:
        //     TLC ports 32-63 -> general purpose outputs
        //
        //   Chime board:
        //     HC595 ports 0-7 -> timer-protected outputs
        //
        {
            int n = 0;
            
            // 1-15 = flashers (TLC ports 0-15)
            // 16   = strobe   (TLC port 15)
            for (int i = 0 ; i < 16 ; ++i)
                outPort[n++].set(PortTypeTLC5940, i, PortFlagGamma);
            
            // 17 = knocker (PTC8)
            outPort[n++].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC8));
            
            // 18-49 = power board outputs 1-32 (TLC ports 32-63)
            for (int i = 0 ; i < 32 ; ++i)
                outPort[n++].set(PortTypeTLC5940, i+32);
            
            // 50-65 = flipper RGB (TLC ports 16-31)
            for (int i = 0 ; i < 16 ; ++i)
                outPort[n++].set(PortTypeTLC5940, i+16, PortFlagGamma);
            
            // 66-73 = chime board ports 1-8 (74HC595 ports 0-7)
            for (int i = 0 ; i < 8 ; ++i)
                outPort[n++].set(PortType74HC595, i);
            
            // set Disabled to signify end of configured outputs
            outPort[n].typ = PortTypeDisabled;
        }
#endif

#if TEST_CONFIG_CAB
#if TEST_KEEP_PRINTF
        outPort[ 0].set(PortTypeVirtual, PINNAME_TO_WIRE(NC));       // port 1  = NC to keep debug printf (PTA1 is UART)
        outPort[ 1].set(PortTypeVirtual, PINNAME_TO_WIRE(NC));       // port 2  = NC to keep debug printf (PTA2 is UART)
#else
        outPort[ 0].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(PTA1));     // port 1  = PTA1
        outPort[ 1].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(PTA2));     // port 2  = PTA2
#endif
        outPort[ 2].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(PTD4));     // port 3  = PTD4
        outPort[ 3].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(PTA12));    // port 4  = PTA12
        outPort[ 4].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(PTA4));     // port 5  = PTA4
        outPort[ 5].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(PTA5));     // port 6  = PTA5
        outPort[ 6].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(PTA13));    // port 7  = PTA13
        outPort[ 7].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(PTD5));     // port 8  = PTD5
        outPort[ 8].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(PTD0));     // port 9  = PTD0
        outPort[ 9].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(PTD3));     // port 10 = PTD3
        outPort[10].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTD2));     // port 11 = PTD2
        outPort[11].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC8));     // port 12 = PTC8
        outPort[12].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC9));     // port 13 = PTC9
        outPort[13].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC7));     // port 14 = PTC7
        outPort[14].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC0));     // port 15 = PTC0
        outPort[15].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC3));     // port 16 = PTC3
        outPort[16].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC4));     // port 17 = PTC4
        outPort[17].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC5));     // port 18 = PTC5
        outPort[18].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC6));     // port 19 = PTC6
        outPort[19].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC10));    // port 20 = PTC10
        outPort[20].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTC11));    // port 21 = PTC11
        outPort[21].set(PortTypeGPIODig, PINNAME_TO_WIRE(PTE0));     // port 22 = PTE0
#endif

#if 0
        // configure the on-board RGB LED as outputs 1,2,3
        outPort[0].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(LED1), PortFlagActiveLow);     // PTB18 = LED1 = Red LED
        outPort[1].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(LED2), PortFlagActiveLow);     // PTB19 = LED2 = Green LED
        outPort[2].set(PortTypeGPIOPWM, PINNAME_TO_WIRE(LED3), PortFlagActiveLow);     // PTD1  = LED3 = Blue LED
        outPort[3].typ = PortTypeDisabled;
#endif
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
    // distinguish among Pinscape units.  Note that this doesn't affect 
    // the LedWiz unit numbering, which is implied by the USB Product ID.
    uint8_t psUnitNo;
            
    // Are joystick reports enabled?  Joystick reports can be turned off, to
    // use the device as purely an output controller.
    char joystickEnabled;
    
    // Timeout for rebooting the KL25Z when the connection is lost.  On some
    // hosts, the mbed USB stack has problems reconnecting after an initial
    // connection is dropped.  As a workaround, we can automatically reboot
    // the KL25Z when it detects that it's no longer connected, after the
    // interval set here expires.  The timeout is in seconds; setting this
    // to 0 disables the automatic reboot.
    uint8_t disconnectRebootTimeout;
    
    // --- ACCELEROMETER ---
    
    // accelerometer orientation (ORIENTATION_xxx value)
    char orientation;
    
    
    // --- EXPANSION BOARDS ---
    struct
    {
        uint8_t typ;        // expansion board set type:
                            //    1 -> Pinscape expansion boards
        uint8_t vsn;        // board set interface version
        uint8_t ext[3];     // board set type-specific extended data
        
    } expan;
    
    
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
        //
        // Note!  These are stored in uint8_t WIRE format, not PinName format.
        uint8_t sensorPin[4];
        
        // ZB LAUNCH BALL button setup.
        //
        // This configures the "ZB Launch Ball" feature in DOF, based on Zeb's (of 
        // zebsboards.com) scheme for using a mechanical plunger as a Launch button.
        // Set the port to 0 to disable the feature.
        //
        // The port number is an LedWiz port number that we monitor for activation.
        // This port isn't meant to be connected to a physical device, although it
        // can be if desired.  It's primarily to let the host tell the controller
        // when the ZB Launch feature is active.  The port numbering starts at 1;
        // set this to zero to disable the feature.
        //
        // The key type and code has the same meaning as for a button mapping.  This
        // sets the key input sent to the PC when the plunger triggers a launch when
        // the mode is active.  For example, set keytype=2 and keycode=0x28 to send
        // the Enter key (which is the key almost all PC pinball software uses for
        // plunger and Launch button input).
        //
        // The "push distance" is the distance, in 1/1000 inch units, for registering a 
        // push on the plunger as a button push.  If the player pushes the plunger 
        // forward of the rest position by this amount, we'll treat it as pushing the 
        // button, even if the player didn't pull back the plunger first.  This lets 
        // the player treat the plunger knob as a button for games where it's meaningful
        // to hold down the Launch button for specific intervals (e.g., "Championship 
        // Pub").
        struct
        {
            uint8_t port;
            uint8_t keytype;
            uint8_t keycode;
            uint16_t pushDistance;
        
        } zbLaunchBall;
           
        // --- PLUNGER CALIBRATION ---
        struct
        {
            // has the plunger been calibrated?
            bool calibrated;
        
            // calibration button switch pin
            uint8_t btn;
        
            // calibration button indicator light pin
            uint8_t led;
            
            // Plunger calibration min, zero, and max.  These are in terms of the
            // unsigned 16-bit scale (0x0000..0xffff) that we use for the raw sensor
            // readings.
            //
            // The zero point is the rest position (aka park position), where the
            // plunger is in equilibrium between the main spring and the barrel 
            // spring.  In the standard setup, the plunger can travel a small 
            // distance forward of the rest position, because the barrel spring 
            // can be compressed a bit.  The minimum is the maximum forward point 
            // where the barrel spring can't be compressed any further.
            uint16_t min;
            uint16_t zero;
            uint16_t max;
            
            // Measured release time, in milliseconds.
            uint8_t tRelease;
    
            // Reset the plunger calibration
            void setDefaults()
            {
                calibrated = false;       // not calibrated
                min = 0;                  // assume we can go all the way forward...
                max = 0xffff;             // ...and all the way back
                zero = max/6;             // the rest position is usually around 1/2" back = 1/6 of total travel
                tRelease = 65;            // standard 65ms release time
            }
            
            // Begin calibration.  This sets each limit to the worst
            // case point - for example, we set the retracted position
            // to all the way forward.  Each actual reading that comes
            // in is then checked against the current limit, and if it's
            // outside of the limit, we reset the limit to the new reading.
            void begin()
            {
                min = 0;                  // we don't calibrate the maximum forward position, so keep this at zero
                zero = 0xffff;            // set the zero position all the way back
                max = 0;                  // set the retracted position all the way forward
                tRelease = 65;            // revert to a default release time
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
        uint8_t statusPin;
    
        // PSU2 power status latch (DigitalOut pin)
        uint8_t latchPin;
        
        // TV ON relay pin (DigitalOut pin).  This pin controls the TV switch 
        // relay.  Raising the pin HIGH turns the relay ON (energizes the coil).
        uint8_t relayPin;
        
        // TV ON delay time, in 1/100 second units.  This is the interval between 
        // sensing that the secondary power supply has turned on and pulsing the 
        // TV ON switch relay.  
        int delayTime;
    
    } TVON;
    
    // --- Night Mode ---
    struct
    {
        uint8_t btn;        // night mode button number (1..MAX_BUTTONS, 0=disabled)
        uint8_t flags;      // flags:
                            //    0x01 = on/off switch (if not set, it's a momentary button)
        uint8_t port;       // indicator output port number (1..MAX_OUT_PORTS, 0=disabled)
    } nightMode;
    

    // --- TLC5940NT PWM Controller Chip Setup ---
    struct
    {
        // number of TLC5940NT chips connected in daisy chain
        int nchips;
        
        // pin connections (wire pin IDs)
        uint8_t sin;        // Serial data - must connect to SPIO MOSI -> PTC6 or PTD2
        uint8_t sclk;       // Serial clock - must connect to SPIO SCLK -> PTC5 or PTD1
                            // (but don't use PTD1, since it's hard-wired to the on-board blue LED)
        uint8_t xlat;       // XLAT (latch) signal - connect to any GPIO pin
        uint8_t blank;      // BLANK signal - connect to any GPIO pin
        uint8_t gsclk;      // Grayscale clock - must connect to a PWM-out capable pin

    } tlc5940; 
    

    // --- 74HC595 Shift Register Setup ---
    struct
    {
        // number of 74HC595 chips attached in daisy chain
        int nchips;
        
        // pin connections
        uint8_t sin;        // Serial data - use any GPIO pin
        uint8_t sclk;       // Serial clock - use any GPIO pin
        uint8_t latch;      // Latch - use any GPIO pin
        uint8_t ena;        // Enable signal - use any GPIO pin
    
    } hc595;


    // --- Button Input Setup ---
    ButtonCfg button[MAX_BUTTONS] __attribute__((packed));

    // --- LedWiz Output Port Setup ---
    LedWizPortCfg outPort[MAX_OUT_PORTS] __attribute__((packed));  // LedWiz & extended output ports 

};

#endif
