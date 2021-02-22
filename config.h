// Pinscape Controller Configuration
//
// !!! ATTENTION !!! 
// If you've come here on advice in a forum to change a GPIO setting or 
// to #define a macro to enable the expansion boards, >>>STOP NOW<<<.  The 
// advice you found is out of date and no longer applies.  You don't need 
// to edit this file or recompile the firmware, and you shouldn't.  Instead,
// use the standard firmware, and set options using the Pinscape Config Tool
// on your Windows PC.  All options that were formerly configurable by 
// editing this file can be selected with the Config Tool.  That's much 
// cleaner and easier than editing the source code, and it eliminates the
// problem of re-synchronizing a private copy of the source code with future 
// updates.  With the config tool, you only need the standard firmware build, 
// so future updates are a simple matter of downloading the latest version.
//
//
// IN THE PAST (but NOT NOW - see above), configuration was handled mostly 
// with #defines and #ifdefs.  To customize the setup, you had to create a 
// private forked copy of the source code, edit the constants defined in 
// config.h, and compile a custom binary.  That's no longer necessary because
// the config tool lets you set all configurable options dynamically.  Of 
// course, you're still free to create a custom version if you want to add 
// entirely new features or make changes that go beyond the configurable
// options.
//
#ifndef CONFIG_H
#define CONFIG_H

#include "USBJoystick.h"


// TEST SETTINGS - FOR DEBUGGING PURPOSES ONLY.  The macros below select
// special option combinations for debugging purposes.
//
// IMPORTANT!  If you're trying to create a custom configuration because
// you have a pin conflict or because you're using the expansion boards,
// DON'T modify this file, DON'T use these macros, and DON'T recompile 
// the firmware.  Use the Config Tool on your Windows PC instead.
#define STANDARD_CONFIG       1     // standard settings, based on v1 base settings
#define TEST_CONFIG_EXPAN     0     // configuration for the expansion boards
#define TEST_KEEP_PRINTF      0     // for debugging purposes, keep printf() enabled
                                    // by leaving the SDA UART GPIO pins unallocated

// Plunger type codes
// NOTE!  These values are part of the external USB interface.  New
// values can be added, but the meaning of an existing assigned number 
// should remain fixed to keep the PC-side config tool compatible across 
// versions.
const int PlungerType_None      = 0;     // no plunger
const int PlungerType_TSL1410R  = 1;     // TSL1410R linear image sensor (1280x1 pixels, 400dpi), serial mode, edge detection
const int PlungerType_TSL1412S  = 3;     // TSL1412S linear image sensor (1536x1 pixels, 400dpi), serial mode, edge detection
const int PlungerType_Pot       = 5;     // potentionmeter
const int PlungerType_OptQuad   = 6;     // AEDR8300 optical quadrature sensor
const int PlungerType_MagQuad   = 7;     // AS5304 magnetic quadrature sensor
const int PlungerType_TSL1401CL = 8;     // TSL1401CL linear image sensor (128x1 pixels, 400dpi), bar code reader
const int PlungerType_VL6180X   = 9;     // VL6180X time-of-flight distance sensor
const int PlungerType_AEAT6012  = 10;    // AEAT-6012-A06 magnetic rotary encoder; absolute angle sensing, 12-bit precision
const int PlungerType_TCD1103   = 11;    // Toshiba TCD1103GFG linear image sensor (1500x1 pixels, ~4600dpi), edge detection
const int PlungerType_VCNL4010  = 12;    // VCNL4010 IR proximity sensor

// Plunger auto-zero flags
const int PlungerAutoZeroEnabled = 0x01; // auto-zeroing enabled

// Accelerometer orientation codes
// These values are part of the external USB interface
const int OrientationFront     = 0;      // USB ports pointed toward front of cabinet
const int OrientationLeft      = 1;      // ports pointed toward left side of cabinet
const int OrientationRight     = 2;      // ports pointed toward right side of cabinet
const int OrientationRear      = 3;      // ports pointed toward back of cabinet

// Accelerometer dynamic range codes
const int AccelRange1G         = 0;      // +/-1G
const int AccelRange2G         = 1;      // +/-2G
const int AccelRange4G         = 2;      // +/-4G
const int AccelRange8G         = 3;      // +/-8G

// input button types
const int BtnTypeNone          = 0;      // unused
const int BtnTypeJoystick      = 1;      // joystick button
const int BtnTypeKey           = 2;      // keyboard key
const int BtnTypeMedia         = 3;      // media control key

// input button flags
const uint8_t BtnFlagPulse     = 0x01;   // pulse mode - reports each change in the physical switch state
                                         // as a brief press of the logical button/keyboard key
                                         
// button setup structure
struct ButtonCfg
{
    // physical GPIO pin - a Wire-to-PinName mapping index
    uint8_t pin;
    
    // Key type and value reported to the PC
    uint8_t typ;        // key type reported to PC - a BtnTypeXxx value
    uint8_t val;        // key value reported - meaning depends on 'typ' value:
                        //   none     -> no PC input reports (val is unused)
                        //   joystick -> val is joystick button number (1..32)
                        //   keyboard -> val is USB scan code
    uint8_t IRCommand;  // IR command to send when the button is pressed, as
                        // an IR command slot number: 1..MAX_IR_CODES, or 0
                        // if no IR command is to be sent
                        
    // Shifted key type and value.  These used when the button is pressed 
    // while the Local Shift Button is being held down.  We send the key
    // code given here instead of the regular typ/val code in this case.
    // If typ2 is BtnTypeNone, we use the regular typ/val code whether or
    // not the shift button is being held.
    uint8_t typ2;       // shifted key type
    uint8_t val2;       // shifted key value
    uint8_t IRCommand2; // IR command to send when shifted button is pressed
    
    // key flags - a bitwise combination of BtnFlagXxx values
    uint8_t flags;

    void set(uint8_t pin, uint8_t typ, uint8_t val, uint8_t flags = 0)
    {
        this->pin = pin;
        this->typ = typ;
        this->val = val;
        this->IRCommand = 0;
        this->flags = flags;
        this->typ2 = 0;
        this->val2 = 0;
        this->IRCommand2 = 0;
    }
        
} __attribute__((packed));
    

// maximum number of input button mappings in configuration
const int MAX_BUTTONS = 48;

// extra slots for virtual buttons (ZB Launch Ball)
const int VIRTUAL_BUTTONS = 1;             // total number of buttons
const int ZBL_BUTTON_CFG = MAX_BUTTONS;    // index of ZB Launch Ball slot

// LedWiz output port type codes
// These values are part of the external USB interface
const int PortTypeDisabled     = 0;        // port is disabled - not visible to LedWiz/DOF host
const int PortTypeGPIOPWM      = 1;        // GPIO port, PWM enabled
const int PortTypeGPIODig      = 2;        // GPIO port, digital out
const int PortTypeTLC5940      = 3;        // TLC5940 port
const int PortType74HC595      = 4;        // 74HC595 port
const int PortTypeVirtual      = 5;        // Virtual port - visible to host software, but not connected 
                                           //  to a physical output
const int PortTypeTLC59116     = 6;        // TLC59116 port

// LedWiz output port flag bits
const uint8_t PortFlagActiveLow    = 0x01; // physical output is active-low
const uint8_t PortFlagNoisemaker   = 0x02; // noisemaker device - disable when night mode is engaged
const uint8_t PortFlagGamma        = 0x04; // apply gamma correction to this output
const uint8_t PortFlagFlipperLogic = 0x08; // enable Flipper Logic on the port (timed power limitation)
const uint8_t PortFlagChimeLogic   = 0x10; // enable Chime Logic on this port (min/max time limits)

// maximum number of output ports
const int MAX_OUT_PORTS = 128;

// port configuration data
struct LedWizPortCfg
{
    // port type:  a PortTypeXxx value
    uint8_t typ;        
    
    // physical output pin:  
    //
    //  - for a GPIO port, this is an index in the 
    //    USB-to-PinName mapping list
    //
    //  - for a TLC5940 or 74HC595 port, it's the output
    //    number in the overall daisy chain, starting 
    //    from 0 for OUT0 on the first chip in the chain
    //
    //  - for a TLC59116, the high 4 bits are the chip
    //    address (the low 4 bits of the address only),
    //    and the low 4 bits are the output number on
    //    the chip
    //
    //  - for inactive and virtual ports, this is unused
    //
    uint8_t pin;
    
    // flags:  a combination of PortFlagXxx values
    uint8_t flags;
    
    // flipper logic properties:
    //
    //  - high 4 bits (0xF0) give full-power time 
    //
    //  - low 4 bits (0x0F) give reduced power level (used
    //    after full-power time expires), in 6.66% units
    //
    uint8_t flipperLogic;
    
    void set(uint8_t typ, uint8_t pin, uint8_t flags = 0, uint8_t flipperLogic = 0)
    {
        this->typ = typ;
        this->pin = pin;
        this->flags = flags;
        this->flipperLogic = flipperLogic;
    }
        
} __attribute__ ((packed));

// IR command configuration flags
const uint8_t IRFlagTVON = 0x01;     // send command at TV ON time
const uint8_t IRFlagDittos = 0x02;   // use "ditto" codes on send

// IR command configuration data
struct IRCommandCfg
{
    uint8_t flags;      // flags: a combination of IRFlagXxx values
    uint8_t keytype;    // key type to send when IR command is received
    uint8_t keycode;    // key code to send when IR command is received
    uint8_t protocol;   // IR protocol ID (see IRRemote/IRProtocolID.h)
    struct
    {
        uint32_t lo;    // low 32 bits of code
        uint32_t hi;    // high 32 bits of code
    } code;             // 64-bit command code (protocol-specific; see IRProtocols.h)
} __attribute__ ((packed));

// Maximum number of IR commands
const int MAX_IR_CODES = 16;


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
                
        // Set the default Pinscape unit number to #1.  This is a separate identifier
        // from the LedWiz ID, so you don't have to worry about making this different
        // from your LedWiz units.  Each Pinscape unit should have a unique value for
        // this ID, though.
        //
        // Note that Pinscape unit #1 corresponds to DOF Pinscape #51, PS 2 -> DOF 52,
        // and so on - just add 50 to get the DOF ID.
        psUnitNo = 1;
        
        // set a disconnect reboot timeout of 10 seconds by default
        disconnectRebootTimeout = 10;
        
        // enable joystick reports
        joystickEnabled = true;
        
        // use the XYZ axis format
        joystickAxisFormat = USBJoystick::AXIS_FORMAT_XYZ;
        
        // send reports every 8.33ms by default (120 Hz, 2X the typical video
        // refresh rate)
        jsReportInterval_us = 8333;
        
        // assume standard orientation, with USB ports toward front of cabinet
        accel.orientation = OrientationFront;
        
        // default dynamic range +/-1G
        accel.range = AccelRange1G;
        
        // default auto-centering time
        accel.autoCenterTime = 0;
        
        // take a new accelerometer reading on every other joystick report
        accel.stutter = 2;

        // assume a basic setup with no expansion boards
        expan.typ = 0;
        expan.vsn = 0;
        memset(expan.ext, 0, sizeof(expan.ext));

        // assume no plunger is attached
        plunger.enabled = 0x00;
        plunger.sensorType = PlungerType_None;
        
        // no jitter filter
        plunger.jitterWindow = 0;
        
        // normal orientation
        plunger.reverseOrientation = false;
        
#if TEST_CONFIG_EXPAN || STANDARD_CONFIG
        plunger.enabled = 0x01;
        plunger.sensorType = PlungerType_TSL1410R;
        plunger.sensorPin[0] = PINNAME_TO_WIRE(PTE20); // SI
        plunger.sensorPin[1] = PINNAME_TO_WIRE(PTE21); // SCLK
        plunger.sensorPin[2] = PINNAME_TO_WIRE(PTB0);  // AO1 = PTB0 = ADC0_SE8
        plunger.sensorPin[3] = PINNAME_TO_WIRE(PTE22); // AO2 (parallel mode) = PTE22 = ADC0_SE3
#endif
        
        // default plunger calibration button settings
        plunger.cal.features = 0x03;                   // 0x01 = enable button, 0x02 = enable indicator lamp
        plunger.cal.btn = PINNAME_TO_WIRE(PTE29);      // button input (DigitalIn port)
        plunger.cal.led = PINNAME_TO_WIRE(PTE23);      // button output (DigitalOut port)
        
        // set the default plunger calibration
        plunger.cal.setDefaults();
        
        // disable the ZB Launch Ball by default
        plunger.zbLaunchBall.port = 0;                  // 0 = disabled
        plunger.zbLaunchBall.keytype = BtnTypeKey;      // keyboard key
        plunger.zbLaunchBall.keycode = 0x28;            // USB keyboard scan code for Enter key
        plunger.zbLaunchBall.pushDistance = 63;         // 63/1000 in == .063" == about 1/16"
        
        // assume no TV ON switch
        TVON.statusPin = PINNAME_TO_WIRE(NC);
        TVON.latchPin = PINNAME_TO_WIRE(NC);
        TVON.relayPin = PINNAME_TO_WIRE(NC);
        TVON.delayTime = 700;   // 7 seconds
        
#if TEST_CONFIG_EXPAN
        // expansion board TV ON wiring
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
        
#if TEST_CONFIG_EXPAN
        // for expansion board testing purposes, assume the common setup
        // with one main board and one power board
        tlc5940.nchips = 4;
#endif

        // Default TLC5940 pin assignments.  Note that it's harmless to set
        // these to valid pins even if no TLC5940 chips are actually present,
        // since the main program won't allocate the connections if 'nchips'
        // is zero.  This means that the pins are free to be used for other
        // purposes (such as output ports) if not using TLC5940 chips.
        tlc5940.sin = PINNAME_TO_WIRE(PTC6);
        tlc5940.sclk = PINNAME_TO_WIRE(PTC5);
        tlc5940.xlat = PINNAME_TO_WIRE(PTC10);
        tlc5940.blank = PINNAME_TO_WIRE(PTC7);        
#if TEST_KEEP_PRINTF
        tlc5940.gsclk = PINNAME_TO_WIRE(PTA13);     // PTA1 is reserved for SDA printf()
#else
        tlc5940.gsclk = PINNAME_TO_WIRE(PTA1);
#endif
        
        // assume no 74HC595 chips
        hc595.nchips = 0;

#if TEST_CONFIG_EXPAN
        // for expansion board testing purposes, assume one chime board
        hc595.nchips = 1;
#endif
    
        // Default 74HC595 pin assignments.  As with the TLC5940 pins, it's
        // harmless to assign pins here even if no 74HC595 chips are used,
        // since the main program won't actually allocate the pins if 'nchips'
        // is zero.
        hc595.sin = PINNAME_TO_WIRE(PTA5);
        hc595.sclk = PINNAME_TO_WIRE(PTA4);
        hc595.latch = PINNAME_TO_WIRE(PTA12);
        hc595.ena = PINNAME_TO_WIRE(PTD4);
        
        // disable all TLC59116 chips by default
        tlc59116.chipMask = 0;
        
        // Default TLC59116 pin assignments
        tlc59116.sda = PINNAME_TO_WIRE(PTC6);
        tlc59116.scl = PINNAME_TO_WIRE(PTC5);
        tlc59116.reset = PINNAME_TO_WIRE(PTC10);
        
        // Default IR hardware pin assignments.  On the expansion boards,
        // the sensor is connected to PTA13, and the emitter LED is on PTC9.
#if TEST_CONFIG_EXPAN
        IR.sensor = PINNAME_TO_WIRE(PTA13);
        IR.emitter = PINNAME_TO_WIRE(PTC9);
#else
        IR.sensor = PINNAME_TO_WIRE(NC);
        IR.emitter = PINNAME_TO_WIRE(NC);
#endif     

        // clear out all IR slots
        memset(IRCommand, 0, sizeof(IRCommand));
        for (int i = 0 ; i < MAX_IR_CODES ; ++i)
        {
            IRCommand[i].protocol = 0;
            IRCommand[i].keytype = BtnTypeNone;
        }
   
        // initially configure with no LedWiz output ports
        outPort[0].typ = PortTypeDisabled;
        
        // initially configure with no shift key
        shiftButton.idx = 0;
        shiftButton.mode = 0;
            
        // initially configure with no input buttons
        for (int i = 0 ; i < MAX_BUTTONS ; ++i)
            button[i].set(PINNAME_TO_WIRE(NC), BtnTypeNone, 0);

#if STANDARD_CONFIG | TEST_CONFIG_EXPAN
        // For the standard configuration, assign 24 input ports to
        // joystick buttons 1-24.  Assign the same GPIO pins used
        // in the original v1 default configuration.  For expansion
        // board testing purposes, also assign the input ports, with
        // the noted differences.
        for (int i = 0 ; i < 24 ; ++i) {
            static const int bp[] = {
                PINNAME_TO_WIRE(PTC2),  // 1
                PINNAME_TO_WIRE(PTB3),  // 2
                PINNAME_TO_WIRE(PTB2),  // 3
                PINNAME_TO_WIRE(PTB1),  // 4
                PINNAME_TO_WIRE(PTE30), // 5 
#if TEST_CONFIG_EXPAN
                PINNAME_TO_WIRE(PTC11), // 6 - expansion boards use PTC11 for this, since PTE22
                                        //     is reserved for a plunger connection
#elif STANDARD_CONFIG
                PINNAME_TO_WIRE(PTE22), // 6 - original standalone setup uses PTE22
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
                // For expansion board testing only, assign the inputs
                // to keyboard keys A, B, etc.  This isn't useful; it's
                // just for testing purposes.  Note that the USB key code
                // for "A" is 4, "B" is 5, and so on sequentially through 
                // the alphabet.
                BtnTypeKey, i+4);
#elif STANDARD_CONFIG
                // For the standard configuration, assign the input to
                // joystick buttons 1-24, as in the original v1 default
                // configuration.
                BtnTypeJoystick, i+1);
#endif

        }
#endif
        
#if TEST_CONFIG_EXPAN
        // For testing purposes, configure the basic complement of 
        // expansion board ports.  AS MENTIONED ABOVE, THIS IS PURELY FOR
        // TESTING.  DON'T USE THIS METHOD TO CONFIGURE YOUR EXPANSION 
        // BOARDS FOR ACTUAL DEPLOYMENT.  It's much easier and cleaner
        // to use the unmodified standard build, and customize your
        // installation with the Pinscape Config Tool on Windows.
        //
        // For this testing setup, we'll configure one main board, one
        // power board, and one chime board.  The *physical* ports on
        // the board are shown below.  The logical (LedWiz/DOF) numbering
        // ISN'T sequential through the physical ports, because we want
        // to arrange the DOF ports so that the most important and most
        // common toys are assigned to ports 1-32.  Those ports are
        // special because they're accessible to ALL software on the PC,
        // including older LedWiz-only software such as Future Pinball.
        // Ports above 32 are accessible only to modern DOF software,
        // like Visual Pinball and PinballX.
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

#if STANDARD_CONFIG
        //
        // For the standard build, set up the original complement
        // of 22 ports from the v1 default onfiguration.  
        //
        // IMPORTANT!  As mentioned above, don't edit this file to
        // customize this for your machine.  Instead, use the unmodified
        // standard build, and customize your installation using the
        // Pinscape Config Tool on Windows.
        //
#if TEST_KEEP_PRINTF
        outPort[ 0].set(PortTypeVirtual, PINNAME_TO_WIRE(NC));       // port 1  = NC to keep debug printf (PTA1 is SDA UART)
        outPort[ 1].set(PortTypeVirtual, PINNAME_TO_WIRE(NC));       // port 2  = NC to keep debug printf (PTA2 is SDA UART)
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
    uint8_t joystickEnabled;
    
    // Joystick axis report format, as a USBJoystick::AXIS_FORMAT_xxx value.
    uint8_t joystickAxisFormat;
    
    // Joystick report timing.  This is the minimum time between joystick
    // reports, in microseconds. 
    uint32_t jsReportInterval_us;
    
    // Timeout for rebooting the KL25Z when the connection is lost.  On some
    // hosts, the mbed USB stack has problems reconnecting after an initial
    // connection is dropped.  As a workaround, we can automatically reboot
    // the KL25Z when it detects that it's no longer connected, after the
    // interval set here expires.  The timeout is in seconds; setting this
    // to 0 disables the automatic reboot.
    uint8_t disconnectRebootTimeout;
    
    // --- ACCELEROMETER ---
    struct
    {
        // accelerometer orientation (OrientationXxx value)
        uint8_t orientation;
    
        // dynamic range (AccelRangeXxx value)
        uint8_t range;
    
        // Auto-centering mode:
        //   0 = auto-centering on, 5-second timer
        //   1-60 = auto-centering on with the given timer in seconds
        //   255 = auto-centering off
        uint8_t autoCenterTime;
        
        // Accelerometer report "stuttering".  This is the number of times 
        // that each accelerometer reading is repeated in the joystick 
        // reports.  If this is set to 1 (or 0), a new accelerometer reading 
        // is taken on every joystick report.  If set to 2, a new reading 
        // is taken on every other report, and the previous reading is 
        // repeated on the alternating reports.  If set to 3, we take a 
        // new  reading on each third report, and so on.  The purpose is 
        // to slow down accelerometer readings for the benefit of Visual
        // Pinball, which will miss readings if taken faster than the 
        // video refresh rate, while sending joystick reports at a
        // faster rate for lower button input latency.
        uint8_t stutter;
    
    } accel;
    
    
    // --- EXPANSION BOARDS ---
    struct
    {
        uint8_t typ;        // expansion board set type:
                            //    1 -> Pinscape Expansion Boards
                            //    2 -> Pinscape All-In-One (AIO)
        uint8_t vsn;        // board set interface version
        uint8_t ext[3];     // board set type-specific extended data
        
    } expan;
    
    
    // --- PLUNGER CONFIGURATION ---
    struct
    {
        // Plunger enabled/disabled.  Note that we use the status flag
        // bit 0x01 if enabled, 0x00 if disabled.  This conveniently
        // can be tested as though it's a bool, but should always be
        // stored as 0x01 or 0x00 so that it can be OR'ed into the
        // status report flag bits.
        uint8_t enabled;

        // plunger sensor type
        uint8_t sensorType;
    
        // Plunger sensor pins.  To accommodate a wide range of sensor types,
        // we keep a generic list of 4 pin assignments.  The use of each pin
        // varies by sensor.  The lists below are in order of the entries in
        // the sensorPin[] array, which is also the order of the pin numbers
        // passed in the USB configuration commands.  "NC" means that the pin
        // isn't used by the sensor, so the slot is ignored.  Each pin's GPIO
        // usage is also listed, because usages like AnalogIn and PWM mean
        // that you have to use a GPIO pin that can multiplexed to the
        // specified peripheral function.  If the usage is listed as simply
        // "GPIO", it means that no special peripheral function is needed for
        // that connection, so any GPIO pin can be used.
        //
        // TSL1410R/1412S/1401CL:     SI (GPIO),       CLK (GPIO),       AO (AnalogIn),   NC
        // Potentiometer:             AO (AnalogIn),   NC,               NC,              NC
        // AEDR8300:                  A (InterruptIn), B (InterruptIn),  NC,              NC
        // AS5304:                    A (InterruptIn), B (InterruptIn),  NC,              NC
        // VL6180X:                   SDA (GPIO),      SCL (GPIO),       GPIO0/CE (GPIO), NC
        // AEAT-6012-A06:             CS (GPIO),       CLK (GPIO),       DO (GPIO),       NC
        // TCD1103GFG:                fM (PWM),        OS (AnalogIn),    ICG (GPIO),      SH (GPIO)
        // VCNL4010:                  SDA (GPIO),      SCL (GPIO),       NC,              NC
        //
        // Note!  These are stored in uint8_t WIRE format, not PinName format.
        // In other words, the values here are the byte values passed in the
        // USB protocol to represent pin numbers.  You can translate these
        // byte values to PinName values using wirePinName(uint8_t).
        //
        uint8_t sensorPin[4];
        
        // Automatic zeroing.  If enabled, we'll reset the plunger position to
        // the park position after a period of inactivity.  This only applies
        // to certain sensor types; sensors that don't use it simply ignore it.  
        struct
        {
            uint8_t flags;  // flags bits - combination of PlungerAutoZeroXxx flags
            uint8_t t;      // inactivity time in seconds
        } autoZero;
        
        // Jitter filter.  This is the size of the hysteresis window, in joystick
        // units (-4095..+4095).  One joystick unit is approximately 1/10000" of
        // physical travel.  Zero disables the jitter filter.
        uint16_t jitterWindow;
        
        // Plunger sensor reverse orientation flags.  This is a bit mask:
        //
        //  0x01 = Reverse orientation enabled.  We invert the plunger sensor
        //         readings, as though the sensor were physically flipped
        //         around.  This can be used to correct for installing the
        //         sensor backwards without having to change the hardware.
        //
        //  0x80 = READ-ONLY feature flag.  This always reads as set if the
        //         feature is enabled.  Note that the USB data exchanger always
        //         sets the bit on read, so it's not necessary to actually
        //         store it.
        //
        uint8_t reverseOrientation;
        
        // bar code sensor parameters
        struct
        {
            uint16_t startPix;  // starting pixel offset
        } barCode;
        
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
            
            // Feature enable mask:
            //
            //  0x01 = calibration button enabled
            //  0x02 = indicator light enabled
            uint8_t features;
        
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
            
            // Raw calibration data.  Some sensors need to keep track of raw
            // sensor data for calibration, in addition to the processed
            // range information that the generic code maintains.  We 
            // provide three uint16 slots for the specific sensor subclass's
            // use, with the meanings defined by the subclass.
            uint16_t raw0;
            uint16_t raw1;
            uint16_t raw2;
            
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
                raw0 = raw1 = raw2 = 0;   // clear the raw sensor data items
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
        uint8_t btn;        // night mode button number (1..MAX_BUTTONS, 0 = no button)
        uint8_t flags;      // flags:
                            //    0x01 = on/off switch (if not set, it's a momentary button)
        uint8_t port;       // indicator output port number (1..MAX_OUT_PORTS, 0 = no indicator)
    } nightMode;
    

    // --- TLC5940NT PWM Controller Chip Setup ---
    struct
    {
        // number of TLC5940NT chips connected in daisy chain
        uint8_t nchips;
        
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
        uint8_t nchips;
        
        // pin connections
        uint8_t sin;        // Serial data - use any GPIO pin
        uint8_t sclk;       // Serial clock - use any GPIO pin
        uint8_t latch;      // Latch - use any GPIO pin
        uint8_t ena;        // Enable signal - use any GPIO pin
    
    } hc595;
    
    // --- TLC59116 PWM Controller Chip Setup --
    struct
    {
        // Chip mask.  Each bit represents an enabled chip at the
        // corresponding 4-bit address (i.e., bit 1<<addr represents
        // the chip at 'addr').
        uint16_t chipMask;
        
        // pin connections
        uint8_t sda;        // I2C SDA
        uint8_t scl;        // I2C SCL
        uint8_t reset;      // !RESET (hardware reset line, active low)
        
    } tlc59116;
    
    
    // --- IR Remote Control Hardware Setup ---
    struct
    {
        // sensor (receiver) GPIO input pin; must be interrupt-capable
        uint8_t sensor;
        
        // IR emitter LED GPIO output pin; must be PWM-capable
        uint8_t emitter;
    } IR;
    
    
    // --- Button Input Setup ---
    ButtonCfg button[MAX_BUTTONS + VIRTUAL_BUTTONS] __attribute__((packed));
    
    // Shift button.  This can be used to give each physical button a
    // second meaning.
    struct
    {
        // Shift button index, 1..MAX_BUTTONS.  If this is zero, there's
        // no shift button.
        uint8_t idx;
        
        // Shift button mode.  If the shift button has a key mapping or
        // IR command assigned, this determines what happens when the 
        // shift button is pressed in combination with another key.
        //
        // 0 = Shift OR Key mode.  In this mode, when you initially press
        // the shift button, nothing happens.  Instead, we wait to see if
        // any other buttons are pressed.  If so, we use the shifted meaning
        // of the other button, and we DON'T send the shift button's key or
        // IR command at all.  
        //
        // 1 = Shift AND Key mode.  In this mode, the shift button acts like
        // any other button: its assigned key is sent to the PC as soon as
        // you press it.  If you also press another button while the shift
        // button is down, the shifted meaning of the other button is used.
        //
        // Mode 0, the "OR" mode, is the default.  This allows a button with
        // a key assignment to do double duty as the shift button without
        // creating any confusing situations where the shift button's own
        // key is also sent to the PC during shift usage.  
        uint8_t mode;
    
    } shiftButton;

    // --- LedWiz Output Port Setup ---
    LedWizPortCfg outPort[MAX_OUT_PORTS] __attribute__ ((packed));  // LedWiz & extended output ports 

    // --- IR Command Slots ---
    IRCommandCfg IRCommand[MAX_IR_CODES] __attribute__ ((packed));
};

#endif
