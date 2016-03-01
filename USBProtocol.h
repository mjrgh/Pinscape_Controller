// USB Message Protocol
//
// This file is purely for documentation, to describe our USB protocol.
// We use the standard HID setup with one endpoint in each direction.
// See USBJoystick.cpp/.h for our USB descriptor arrangement.
//

// ------ OUTGOING MESSAGES (DEVICE TO HOST) ------
//
// General note: 16-bit and 32-bit fields in our reports are little-endian
// unless otherwise specified.
//
// 1. Joystick reports
// In most cases, our outgoing messages are HID joystick reports, using the
// format defined in USBJoystick.cpp.  This allows us to be installed on
// Windows as a standard USB joystick, which all versions of Windows support
// using in-the-box drivers.  This allows a completely transparent, driverless,
// plug-and-play installation experience on Windows.  Our joystick report
// looks like this (see USBJoystick.cpp for the formal HID report descriptor):
//
//    ss     status bits:  0x01 -> plunger enabled
//    00     2nd byte of status (reserved)
//    00     3rd byte of status (reserved)
//    00     always zero for joystick reports
//    bb     joystick buttons, low byte (buttons 1-8, 1 bit per button)
//    bb     joystick buttons, 2nd byte (buttons 9-16)
//    bb     joystick buttons, 3rd byte (buttons 17-24)
//    bb     joystick buttons, high byte (buttons 25-32)
//    xx     low byte of X position = nudge/accelerometer X axis
//    xx     high byte of X position
//    yy     low byte of Y position = nudge/accelerometer Y axis
//    yy     high byte of Y position
//    zz     low byte of Z position = plunger position
//    zz     high byte of Z position
//
// The X, Y, and Z values are 16-bit signed integers.  The accelerometer
// values are on an abstract scale, where 0 represents no acceleration,
// negative maximum represents -1g on that axis, and positive maximum
// represents +1g on that axis.  For the plunger position, 0 is the park
// position (the rest position of the plunger) and positive values represent
// retracted (pulled back) positions.  A negative value means that the plunger
// is pushed forward of the park position.
//
// 2. Special reports
// We subvert the joystick report format in certain cases to report other 
// types of information, when specifically requested by the host.  This allows
// our custom configuration UI on the Windows side to query additional 
// information that we don't normally send via the joystick reports.  We
// define a custom vendor-specific "status" field in the reports that we
// use to identify these special reports, as described below.
//
// Normal joystick reports always have 0 in the high bit of the 2nd byte
// of the report.  Special non-joystick reports always have 1 in the high bit 
// of the first byte.  (This byte is defined in the HID Report Descriptor
// as an opaque vendor-defined value, so the joystick interface on the
// Windows side simply ignores it.)
//
// 2A. Plunger sensor pixel dump
// Software on the PC can request a full read of the pixels from the plunger 
// image sensor (if an imaging sensor type is being used) by sending custom 
// protocol message 65 3 (see below).  Normally, the pixels from the image
// sensor are read and processed on the controller device without being sent
// to the PC; the PC only receives the plunger position reading obtained from
// analyzing the image data.  For debugging and setup purposes, software on
// the host can use this special report to obtain the full image pixel array.
// The image sensors we use have too many pixels to fit into one report, so 
// we have to send a series of reports to transmit the full image.  We send
// as many reports as necessary to transmit the full image.  Each report
// looks like this:
//
//    bytes 0:1 = 11-bit index, with high 5 bits set to 10000.  For 
//                example, 0x8004 (encoded little endian as 0x04 0x80) 
//                indicates index 4.  This is the starting pixel number 
//                in the report.  The first report will be 0x00 0x80 to 
//                indicate pixel #0.  
//    bytes 2   = 8-bit unsigned int brightness level of pixel at index
//    bytes 3   = brightness of pixel at index+1
//    etc for the rest of the packet
//
// The pixel dump also sends a special final report, after all of the
// pixel messages, with the "index" field set to 0x7FF (11 bits of 1's).  
// This report packs special fields instead of pixels.  There are two
// subtypes, sent in sequence:
//
//  Subtype 0:
//    bytes 0:1 = 0x87FF (pixel report flags + index 0x7FF)
//    byte 2    = 0x00 -> special report subtype 0
//    bytes 3:4 = pixel position of detected shadow edge in this image,
//                or 0xFFFF if no edge was found in this image.  For
//                raw pixel reports, no edge will be detected because
//                we don't look for one.
//    byte 5    = flags: 
//                   0x01 = normal orientation detected
//                   0x02 = reversed orientation detected
//    bytes 6:7:8 = average time for a sensor scan, in 10us units
//    byte 9:10:11 = time for processing this image, in 10us units
//
//  Subtype 1:
//    bytes 0:1 = 0x87FF
//    byte 2    = 0x01 -> special report subtype 1
//    bytes 3:4 = calibration zero point, in pixels (16-bit little-endian)
//    bytes 5:6 = calibration maximum point, in pixels
//    bytes 7:8 = calibration minimum point, in pixels
//    byte 9    = calibrated release time, in milliseconds
// 
//
// 2B. Configuration query.
// This is requested by sending custom protocol message 65 4 (see below).
// In reponse, the device sends one report to the host using this format:
//
//    bytes 0:1 = 0x8800.  This has the bit pattern 10001 in the high
//                5 bits, which distinguishes it from regular joystick
//                reports and from other special report types.
//    bytes 2:3 = total number of outputs, little endian
//    bytes 6:7 = plunger calibration zero point, little endian
//    bytes 8:9 = plunger calibration maximum point, little endian
//    byte  10   = bit flags: 
//                 0x01 -> configuration loaded; 0 in this bit means that
//                         the firmware has been loaded but no configuration
//                         has been sent from the host
//    The remaining bytes are reserved for future use.
//
// 2C. Device ID query.
// This is requested by sending custom protocol message 65 7 (see below).
// In response, the device sends one report to the host using this format:
//
//    bytes 0:1 = 0x9000.  This has bit pattern 10010 in the high 5
//                bits, which distinguishes this special report from other 
//                report types.
//    bytes 2-11 = Unique CPU ID.  This is the ID stored in the CPU at the
//                factory, guaranteed to be unique across Kinetis devices.
//                This can be used by the host to distinguish devices when
//                two or more controllers are attached.
//
// WHY WE USE THIS HACKY APPROACH TO DIFFERENT REPORT TYPES
//
// The HID report system was specifically designed to provide a clean,
// structured way for devices to describe the data they send to the host.
// Our approach isn't clean or structured; it ignores the promises we
// make about the contents of our report via the HID Report Descriptor
// and stuffs our own different data format into the same structure.
//
// We use this hacky approach only because we can't use the official 
// mechanism, due to the constraint that we want to emulate the LedWiz.
// The right way to send different report types is to declare different
// report types via extra HID Report Descriptors, then send each report
// using one of the types we declared.  If it weren't for the LedWiz
// constraint, we'd simply define the pixel dump and config query reports
// as their own separate HID Report types, each consisting of opaque
// blocks of bytes.  But we can't do this.  The snag is that some versions
// of the LedWiz Windows host software parse the USB HID descriptors as part
// of identifying a device as a valid LedWiz unit, and will only recognize
// the device if it matches certain particulars about the descriptor
// structure of a real LedWiz.  One of the features that's important to
// some versions of the software is the descriptor link structure, which
// is affected by the layout of HID Report Descriptor entries.  In order
// to match the expected layout, we can only define a single kind of output
// report.  Since we have to use Joystick reports for the sake of VP and
// other pinball software, and we're only allowed the one report type, we
// have to make that one report type the Joystick type.  That's why we
// overload the joystick reports with other meanings.  It's a hack, but
// at least it's a fairly reliable and isolated hack, iun that our special 
// reports are only generated when clients specifically ask for them.
// Plus, even if a client who doesn't ask for a special report somehow 
// gets one, the worst that happens is that they get a momentary spurious
// reading from the accelerometer and plunger.



// ------- INCOMING MESSAGES (HOST TO DEVICE) -------
//
// For LedWiz compatibility, our incoming message format conforms to the
// basic USB format used by real LedWiz units.  This is simply 8 data
// bytes, all private vendor-specific values (meaning that the Windows HID
// driver treats them as opaque and doesn't attempt to parse them).
//
// Within this basic 8-byte format, we recognize the full protocol used
// by real LedWiz units, plus an extended protocol that we define privately.
// The LedWiz protocol leaves a large part of the potential protocol space 
// undefined, so we take advantage of this undefined region for our 
// extensions.  This ensures that we can properly recognize all messages 
// intended for a real LedWiz unit, as well as messages from custom host 
// software that knows it's talking to a Pinscape unit.

// --- REAL LED WIZ MESSAGES ---
//
// The real LedWiz protocol has two message types, identified by the first
// byte of the 8-byte USB packet:
//
// 64              -> SBA (64 xx xx xx xx ss uu uu)
//                    xx = on/off bit mask for 8 outputs
//                    ss = global flash speed setting (1-7)
//                    uu = unused
//
// If the first byte has value 64 (0x40), it's an SBA message.  This type of 
// message sets all 32 outputs individually ON or OFF according to the next 
// 32 bits (4 bytes) of the message, and sets the flash speed to the value in 
// the sixth byte.  (The flash speed sets the global cycle rate for flashing
// outputs - outputs with their values set to the range 128-132 - to a   
// relative speed, scaled linearly in frequency.  1 is the slowest at about 
// 2 Hz, 7 is the fastest at about 14 Hz.)
//
// 0-49 or 128-132 -> PBA (bb bb bb bb bb bb bb bb)
//                    bb = brightness level/flash pattern for one output
//
// If the first byte is any valid brightness setting, it's a PBA message.
// Valid brightness settings are:
//
//     0-48 = fixed brightness level, linearly from 0% to 100% intensity
//     49   = fixed brightness level at 100% intensity (same as 48)
//     129  = flashing pattern, fade up / fade down (sawtooth wave)
//     130  = flashing pattern, on / off (square wave)
//     131  = flashing pattern, on for 50% duty cycle / fade down
//     132  = flashing pattern, fade up / on for 50% duty cycle
//     
// A PBA message sets 8 outputs out of 32.  Which 8 are to be set is 
// implicit in the message sequence: the first PBA sets outputs 1-8, the 
// second sets 9-16, and so on, rolling around after each fourth PBA.  
// An SBA also resets the implicit "bank" for the next PBA to outputs 1-8.
//
// Note that there's no special first byte to indicate the PBA message
// type, as there is in an SBA.  The first byte of a PBA is simply the
// first output setting.  The way the LedWiz creators conceived this, the 
// SBA distinguishable from a PBA because 64 isn't a valid output setting, 
// hence a message that starts with a byte value of 64 isn't a valid PBA 
// message.
//
// Our extended protocol uses the same principle, taking advantage of the
// other byte value ranges that are invalid in PBA messages.  To be a valid
// PBA message, the first byte must be in the range 0-49 or 129-132.  As
// already mentioned, byte value 64 indicates an SBA message.  This leaves
// these ranges available for other uses: 50-63, 65-128, and 133-255.


// --- PRIVATE EXTENDED MESSAGES ---
//
// All of our extended protocol messages are identified by the first byte:
//
// 65  -> Miscellaneous control message.  The second byte specifies the specific
//        operation:
//
//        0 -> No Op - does nothing.  (This can be used to send a test message on the
//             USB endpoint.)
//
//        1 -> Set device unit number and plunger status, and save the changes immediately
//             to flash.  The device will automatically reboot after the changes are saved.
//             The additional bytes of the message give the parameters:
//
//               third byte = new unit number (0-15, corresponding to nominal unit numbers 1-16)
//               fourth byte = plunger on/off (0=disabled, 1=enabled)
//
//        2 -> Begin plunger calibration mode.  The device stays in this mode for about
//             15 seconds, and sets the zero point and maximum retraction points to the
//             observed endpoints of sensor readings while the mode is running.  After
//             the time limit elapses, the device automatically stores the results in
//             non-volatile flash memory and exits the mode.
//
//        3 -> Send pixel dump.  The device sends one complete image snapshot from the
//             plunger sensor, as as series of pixel dump messages.  (The message format
//             isn't big enough to allow the whole image to be sent in one message, so
//             the image is broken up into as many messages as necessary.)  After sending
//             the pixels, the device sends the special suffix messages with additional
//             data about the sensor.  See the "pixel dump message" section above.  The 
//             device then resumes sending normal joystick messages.  If the plunger 
//             sensor isn't an imaging type, no pixel messages are sent, but the extra 
//             suffix reports are still sent.  If no plunger sensor is installed, no
//             reports are sent.  Parameters:
//
//               third byte = bit flags:
//                  0x01 = low res mode.  The device rescales the sensor pixel array
//                         sent in the dump messages to a low-resolution subset.  The
//                         size of the subset is determined by the device.  This has
//                         no effect on the sensor operation; it merely reduces the
//                         USB transmission time to allow for a faster frame rate for
//                         viewing in the config tool.
//
//        4 -> Query configuration.  The device sends a special configuration report,
//             (see above; see also USBJoystick.cpp), then resumes sending normal 
//             joystick reports.
//
//        5 -> Turn all outputs off and restore LedWiz defaults.  Sets output ports
//             1-32 to OFF and LedWiz brightness/mode setting 48, sets outputs 33 and
//             higher to brightness level 0, and sets the LedWiz global flash speed to 2.
//
//        6 -> Save configuration to flash.  This saves all variable updates sent via
//             type 66 messages since the last reboot, then automatically reboots the
//             device to put the changes into effect.
//
//        7 -> Query device ID.  The device replies with a special device ID report
//             (see above; see also USBJoystick.cpp), then resumes sending normal
//             joystick reports.
//
//        8 -> Engage/disengage night mode.  The third byte of the message is 1 to
//             engage night mode, 0 to disengage night mode.  (This mode isn't stored
//             persistently; night mode is disengaged after a reset or power cycle.)
//
// 66  -> Set configuration variable.  The second byte of the message is the config
//        variable number, and the remaining bytes give the new value for the variable.
//        The value format is specific to each variable; see the list below for details.
//        This message only sets the value in RAM - it doesn't write the value to flash
//        and doesn't put the change into effect immediately.  To put updates into effect,
//        the host must send a type 65 subtype 6 message (see above), which saves updates
//        to flash and reboots the device.
//
// 200-228 -> Set extended output brightness.  This sets outputs N to N+6 to the
//        respective brightness values in the 2nd through 8th bytes of the message
//        (output N is set to the 2nd byte value, N+1 is set to the 3rd byte value, 
//        etc).  Each brightness level is a linear brightness level from 0-255,
//        where 0 is 0% brightness and 255 is 100% brightness.  N is calculated as
//        (first byte - 200)*7 + 1:
//
//               200 = outputs 1-7
//               201 = outputs 8-14
//               202 = outputs 15-21
//               ...
//               228 = outputs 197-203
//
//        This message is the only way to address ports 33 and higher, since standard
//        LedWiz messages are inherently limited to ports 1-32.
//
//        Note that these extended output messages differ from regular LedWiz settings
//        in two ways.  First, the brightness is the ONLY attribute when an output is
//        set using this mode - there's no separate ON/OFF setting per output as there 
//        is with the SBA/PBA messages.  To turn an output OFF with this message, set
//        the intensity to 0.  Setting a non-zero intensity turns it on immediately
//        without regard to the SBA status for the port.  Second, the brightness is
//        on a full 8-bit scale (0-255) rather than the LedWiz's approximately 5-bit
//        scale, because there are no parts of the range reserved for flashing modes.
//
//        Outputs 1-32 can be controlled by EITHER the regular LedWiz SBA/PBA messages
//        or by the extended messages.  The latest setting for a given port takes
//        precedence.  If an SBA/PBA message was the last thing sent to a port, the
//        normal LedWiz combination of ON/OFF and brightness/flash mode status is used
//        to determine the port's physical output setting.  If an extended brightness
//        message was the last thing sent to a port, the LedWiz ON/OFF status and
//        flash modes are ignored, and the fixed brightness is set.  Outputs 33 and
//        higher inherently can't be addressed or affected by SBA/PBA messages.


// ------- CONFIGURATION VARIABLES -------
//
// Message type 66 (see above) sets one configuration variable.  The second byte
// of the message is the variable ID, and the rest of the bytes give the new
// value, in a variable-specific format.  16-bit values are little endian.
//
// 1  -> USB device ID.  Bytes 3-4 give the 16-bit USB Vendor ID; bytes
//       5-6 give the 16-bit USB Product ID.  For LedWiz emulation, use
//       vendor 0xFAFA and product 0x00EF + unit# (where unit# is the
//       nominal LedWiz unit number, from 1 to 16).  If LedWiz emulation
//       isn't desired or causes host conflicts, you can use our private
//       ID assigned by http://pid.codes (a registry for open-source USB
//       devices) of vendor 0x1209 and product 0xEAEA.  (You can also use
//       any other values that don't cause a conflict on your PC, but we
//       recommend using one of these pre-assigned values if possible.)
//
// 2  -> Pinscape Controller unit number for DOF.  Byte 3 is the new
//       unit number, from 1 to 16.
//
// 3  -> Enable/disable joystick reports.  Byte 2 is 1 to enable, 0 to
//       disable.  When disabled, the device registers as a generic HID 
/        device, and only sends the private report types used by the
//       Windows config tool.
//
// 4  -> Accelerometer orientation.  Byte 3 is the new setting:
//        
//        0 = ports at front (USB ports pointing towards front of cabinet)
//        1 = ports at left
//        2 = ports at right
//        3 = ports at rear
//
// 5  -> Plunger sensor type.  Byte 3 is the type ID:
//
//         0 = none (disabled)
//         1 = TSL1410R linear image sensor, 1280x1 pixels, serial mode
//        *2 = TSL1410R, parallel mode
//         3 = TSL1412R linear image sensor, 1536x1 pixels, serial mode
//        *4 = TSL1412R, parallel mode
//         5 = Potentiometer with linear taper, or any other device that
//             represents the position reading with a single analog voltage
//        *6 = AEDR8300 optical quadrature sensor, 75lpi
//        *7 = AS5304 magnetic quadrature sensor, 160 steps per 2mm
//
//       * The sensor types marked with asterisks (*) are planned but not 
//       currently implemented.  Selecting these types will effectively
//       disable the plunger.
//
// 6  -> Plunger pin assignments.  Bytes 3-6 give the pin assignments for
//       pins 1, 2, 3, and 4.  These use the Pin Number Mappings listed
//       below.  The meaning of each pin depends on the plunger type:
//
//         TSL1410R/1412R, serial:    SI (DigitalOut), CLK (DigitalOut), AO (AnalogIn),  NC
//         TSL1410R/1412R, parallel:  SI (DigitalOut), CLK (DigitalOut), AO1 (AnalogIn), AO2 (AnalogIn)
//         Potentiometer:             AO (AnalogIn),   NC,               NC,             NC
//         AEDR8300:                  A (InterruptIn), B (InterruptIn),  NC,             NC
//         AS5304:                    A (InterruptIn), B (InterruptIn),  NC,             NC
//
// 7  -> Plunger calibration button pin assignments.  Byte 3 is the DigitalIn
//       pin for the button switch; byte 4 is the DigitalOut pin for the indicator
//       lamp.  Either can be set to NC to disable the function.  (Use the Pin
//       Number Mappins listed below for both bytes.)
//
// 8  -> ZB Launch Ball setup.  This configures the ZB Launch Ball feature.  Byte
//       3 is the LedWiz port number (1-255) mapped to the "ZB Launch Ball" output
//       in DOF.  Set the port to 0 to disable the feature.  Byte 4 is the button
//       number (1-32) that we'll "press" when the feature is activated.  Bytes 5-6
//       give the "push distance" for activating the button by pushing forward on
//       the plunger knob, in 1/1000 inch increments (e.g., 80 represents 0.08", 
//       which is the recommended setting).
//
// 9  -> TV ON relay setup.  This requires external circuitry implemented on the
//       Expansion Board (or an equivalent circuit as described in the Build Guide).
//       Byte 3 is the GPIO DigitalIn pin for the "power status" input, using the 
//       Pin Number Mappings below.  Byte 4 is the DigitalOut pin for the "latch"
//       output.  Byte 5 is the DigitalOut pin for the relay trigger.  Bytes 6-7
//       give the delay time in 10ms increments as an unsigned 16-bit value (e.g.,
//       550 represents 5.5 seconds).  
//
// 10 -> TLC5940NT setup.  This chip is an external PWM controller, with 32 outputs
//       per chip and a serial data interface that allows the chips to be daisy-
//       chained.  We can use these chips to add an arbitrary number of PWM output 
//       ports for the LedWiz emulation.  Set the number of chips to 0 to disable
//       the feature.  The bytes of the message are:
//          byte 3 = number of chips attached (connected in daisy chain)
//          byte 4 = SIN pin - Serial data (must connect to SPIO MOSI -> PTC6 or PTD2)
//          byte 5 = SCLK pin - Serial clock (must connect to SPIO SCLK -> PTC5 or PTD1)
//          byte 6 = XLAT pin - XLAT (latch) signal (any GPIO pin)
//          byte 7 = BLANK pin - BLANK signal (any GPIO pin)
//          byte 8 = GSCLK pin - Grayscale clock signal (must be a PWM-out capable pin)
//
// 11 -> 74HC595 setup.  This chip is an external shift register, with 8 outputs per
//       chip and a serial data interface that allows daisy-chaining.  We use this
//       chips to add extra digital outputs for the LedWiz emulation.  In particular,
//       the Chime Board (part of the Expansion Board suite) uses these to add timer-
//       protected outputs for coil devices (knockers, chimes, bells, etc).  Set the
//       number of chips to 0 to disable the feature.  The message bytes are:
//          byte 3 = number of chips attached (connected in daisy chain)
//          byte 4 = SIN pin - Serial data (any GPIO pin)
//          byte 5 = SCLK pin - Serial clock (any GPIO pin)
//          byte 6 = LATCH pin - LATCH signal (any GPIO pin)
//          byte 7 = ENA pin - ENABLE signal (any GPIO pin)
//
// 12 -> Input button setup.  This sets up one button; it can be repeated for each
//       button to be configured.  There are 32 button slots, numbered 1-32.  Each
//       key can be configured as a joystick button, a regular keyboard key, a
//       keyboard modifier key (such as Shift, Ctrl, or Alt), or a media control
//       key (such as volume up/down).
//
//       The bytes of the message are:
//          byte 3 = Button number (1-32)
//          byte 4 = GPIO pin to read for button input
//          byte 5 = key type reported to PC when button is pushed:
//                    1 = joystick button -> byte 6 is the button number, 1-32
//                    2 = regular keyboard key -> byte 6 is the USB key code (see below)
//                    3 = keyboard modifier key -> byte 6 is the USB modifier code (see below)
//                    4 = media control key -> byte 6 is the USB key code (see below)
//                    5 = special button -> byte 6 is the special button code (see below)
//          byte 6 = key code, which depends on the key type in byte 5
//          byte 7 = flags - a combination of these bit values:
//                    0x01 = pulse mode.  This reports a physical on/off switch's state
//                           to the host as a brief key press whenever the switch changes
//                           state.  This is useful for the VPinMAME Coin Door button,
//                           which requires the End key to be pressed each time the
//                           door changes state.
//          
// 13 -> LedWiz output port setup.  This sets up one output port; it can be repeated
//       for each port to be configured.  There are 203 possible slots for output ports, 
//       numbered 1 to 203.  The number of ports visible to the host is determined by
//       the first DISABLED port (type 0).  For example, if ports 1-32 are set as GPIO
//       outputs and port 33 is disabled, the host will see 32 ports, regardless of
//       the settings for post 34 and higher.
//
//       The bytes of the message are:
//         byte 3 = LedWiz port number (1 to maximum number or ports)
//         byte 4 = physical output type:
//                   0 = Disabled.  This output isn't used, and isn't visible to the
//                       LedWiz/DOF software on the host.  The FIRST disabled port
//                       determines the number of ports visible to the host - ALL ports
//                       after the first disabled port are also implicitly disabled.
//                   1 = GPIO PWM output: connected to GPIO pin specified in byte 5,
//                       operating in PWM mode.  Note that only a subset of KL25Z GPIO
//                       ports are PWM-capable.
//                   2 = GPIO Digital output: connected to GPIO pin specified in byte 5,
//                       operating in digital mode.  Digital ports can only be set ON
//                       or OFF, with no brightness/intensity control.  All pins can be
//                       used in this mode.
//                   3 = TLC5940 port: connected to TLC5940 output port number specified 
//                       in byte 5.  Ports are numbered sequentially starting from port 0
//                       for the first output (OUT0) on the first chip in the daisy chain.
//                   4 = 74HC595 port: connected to 74HC595 output port specified in byte 5.
//                       As with the TLC5940 outputs, ports are numbered sequentially from 0
//                       for the first output on the first chip in the daisy chain.
//                   5 = Virtual output: this output port exists for the purposes of the
//                       LedWiz/DOF software on the host, but isn't physically connected
//                       to any output device.  This can be used to create a virtual output
//                       for the DOF ZB Launch Ball signal, for example, or simply as a
//                       placeholder in the LedWiz port numbering.  The physical output ID 
//                       (byte 5) is ignored for this port type.
//         byte 5 = physical output ID, interpreted according to the value in byte 4
//         byte 6 = flags: a combination of these bit values:
//                   0x01 = active-high output (0V on output turns attached device ON)
//                   0x02 = noisemaker device: disable this output when "night mode" is engaged
//                   0x04 = apply gamma correction to this output
//
//       Note that the on-board LED segments can be used as LedWiz output ports.  This
//       is useful for testing a new installation with DOF or other PC software without
//       having to connect any external devices.  Assigning the on-board LED segments to
//       output ports overrides their normal status/diagnostic display use, so the normal
//       status flash pattern won't appear when they're used this way.
//
//       Special port numbers:  if the LedWiz port number is one of these special values,
//       the physical output is used for a special purpose.  These ports aren't visible
//       to the PC as LedWiz ports; they're for internal use by the controller.  The
//       special port numbers are:
//
//         254 = Night Mode indicator lamp.  This port is turned on when night mode
//               is engaged, and turned off when night mode is disengaged.  This can
//               be used, for example, to control an indicator LED inside a lighted
//               momentary pushbutton switch used to activate night mode.  The light 
//               provides visual feedback that the mode is turned on.
//
// 14 -> Disconnect reboot timeout.  The reboot timeout allows the controller software
//       to automatically reboot the KL25Z after it detects that the USB connection is
//       broken.  On some hosts, the device isn't able to reconnect after the initial
//       connection is lost.  The reboot timeout is a workaround for these cases.  When
//       the software detects that the connection is no longer active, it will reboot
//       the KL25Z automatically if a new connection isn't established within the
//       timeout period.  Bytes 3 give the new reboot timeout in seconds.  Setting this
//       to 0 disables the reboot timeout.
//



// --- PIN NUMBER MAPPINGS ---
//
// In USB messages that specify GPIO pin assignments, pins are identified with
// our own private numbering scheme.  Our numbering scheme only includes the 
// ports connected to external header pins on the KL25Z board, so this is only
// a sparse subset of the full GPIO port set.  These are numbered in order of
// pin name.  The special value 0 = NC = Not Connected can be used where
// appropriate to indicate a disabled or unused pin.
//
//     0 = NC (not connected)
//     1 = PTA1
//     2 = PTA2
//     3 = PTA4
//     4 = PTA5
//     5 = PTA12
//     6 = PTA13
//     7 = PTA16
//     8 = PTA17
//     9 = PTB0
//    10 = PTB1
//    11 = PTB2
//    12 = PTB3
//    13 = PTB8
//    14 = PTB9
//    15 = PTB10
//    16 = PTB11
//    17 = PTB18    (on-board LED Red segment - not exposed as a header pin)
//    18 = PTB19    (on-board LED Green segment - not exposed as a header pin)
//    19 = PTC0
//    20 = PTC1
//    21 = PTC2
//    22 = PTC3
//    23 = PTC4
//    24 = PTC5
//    25 = PTC6
//    26 = PTC7
//    27 = PTC8
//    28 = PTC9
//    29 = PTC10
//    30 = PTC11
//    31 = PTC12
//    32 = PTC13
//    33 = PTC16
//    34 = PTC17
//    35 = PTD0
//    36 = PTD1     (on-board LED Blue segment)
//    37 = PTD2
//    38 = PTD3
//    39 = PTD4
//    40 = PTD5
//    41 = PTD6
//    42 = PTD7
//    43 = PTE0
//    44 = PTE1
//    45 = PTE2
//    46 = PTE3
//    47 = PTE4
//    48 = PTE5
//    49 = PTE20
//    50 = PTE21
//    51 = PTE22
//    52 = PTE23
//    53 = PTE29
//    54 = PTE30
//    55 = PTE31

// --- USB KEYBOARD SCAN CODES ---
//
// Use the standard USB HID keyboard codes for regular keys.  See the
// HID Usage Tables in the official USB specifications for a full list.
// Here are the most common codes for quick references:
//
//    A-Z              -> 4-29
//    top row numbers  -> 30-39
//    Return           -> 40
//    Escape           -> 41
//    Backspace        -> 42
//    Tab              -> 43
//    Spacebar         -> 44
//    -_               -> 45
//    =+               -> 46
//    [{               -> 47
//    ]}               -> 48
//    \|               -> 49
//    ;:               -> 51
//    '"               -> 52
//    `~               -> 53
//    ,<               -> 54
//    .>               -> 55
//    /?               -> 56
//    Caps Lock        -> 57
//    F1-F12           -> 58-69
//    F13-F24          -> 104-115
//    Print Screen     -> 70
//    Scroll Lock      -> 71
//    Pause            -> 72
//    Insert           -> 73
//    Home             -> 74
//    Page Up          -> 75
//    Del              -> 76
//    End              -> 77
//    Page Down        -> 78
//    Right Arrow      -> 79
//    Left Arrow       -> 80
//    Down Arrow       -> 81
//    Up Arrow         -> 82
//    Num Lock/Clear   -> 83
//    Keypad / * - +   -> 84 85 86 87
//    Keypad Enter     -> 88
//    Keypad 1-9       -> 89-97
//    Keypad 0         -> 98
//    Keypad .         -> 99
//  


// --- USB KEYBOARD MODIFIER KEY CODES ---
//
// Use these codes for modifier keys in the button mappings
//
//    0x01 = Left Control
//    0x02 = Left Shift
//    0x04 = Left Alt
//    0x08 = Left GUI ("Windows" key)
//    0x10 = Right Control
//    0x20 = Right Shift
//    0x40 = Right Alt
//    0x80 = Right GUI ("Windows" key)


// --- USB KEYBOARD MEDIA KEY CODES ---
//
// Use these for media control keys in the button mappings
//
//    0x01 = Volume Up
//    0x02 = Volume Down
//    0x04 = Mute on/off


// --- SPECIAL BUTTON KEY CODES ---
//
// Use these for special keys in the button mappings
//
//    0x01 = Night mode switch, momentary switch mode.  Pushing this button 
//           engages night mode, disabling all LedWiz outputs marked with the 
//           "noisemaker" flag.  Other outputs are unaffected.  Pushing
//           the button again disengages night mode.  Use this option if the
//           physical button attached to the input is a momentary switch type.
//
//    0x02 = Night mode switch, toggle switch mode.  When this switch is on,
//           night mode is engaged; when the switch is off, night mode is 
//           disengaged.  Use this option if the physical switch attached to
//           to the input is a toggle switch (not a momentary switch).

