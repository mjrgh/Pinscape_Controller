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
//    ss     status bits:  
//              0x01 -> plunger enabled
//              0x02 -> night mode engaged
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
// 2A. Plunger sensor status report
// Software on the PC can request a detailed status report from the plunger
// sensor.  The status information is meant as an aid to installing and
// adjusting the sensor device for proper performance.  For imaging sensor
// types, the status report includes a complete current image snapshot
// (an array of all of the pixels the sensor is currently imaging).  For
// all sensor types, it includes the current plunger position registered
// on the sensor, and some timing information.
//
// To request the sensor status, the host sends custom protocol message 65 3
// (see below).  The device replies with a message in this format:
//
//    bytes 0:1 = 0x87FF
//    byte  2   = 0 -> first (currently only) status report packet
//                (additional packets could be added in the future if
//                more fields need to be added)
//    bytes 3:4 = number of pixels to be sent in following messages, as
//                an unsigned 16-bit little-endian integer.  This is 0 if 
//                the sensor isn't an imaging type.
//    bytes 5:6 = current plunger position registered on the sensor.
//                For imaging sensors, this is the pixel position, so it's
//                scaled from 0 to number of pixels - 1.  For non-imaging
//                sensors, this uses the generic joystick scale 0..4095.
//                The special value 0xFFFF means that the position couldn't
//                be determined,
//    byte  7   = bit flags: 
//                   0x01 = normal orientation detected
//                   0x02 = reversed orientation detected
//                   0x04 = calibration mode is active (no pixel packets
//                          are sent for this reading)
//    bytes 8:9:10 = average time for each sensor read, in 10us units.
//                This is the average time it takes to complete the I/O
//                operation to read the sensor, to obtain the raw sensor
//                data for instantaneous plunger position reading.  For 
//                an imaging sensor, this is the time it takes for the 
//                sensor to capture the image and transfer it to the
//                microcontroller.  For an analog sensor (e.g., an LVDT
//                or potentiometer), it's the time to complete an ADC
//                sample.
//    bytes 11:12:13 = time it took to process the current frame, in 10us 
//                units.  This is the software processing time that was
//                needed to analyze the raw data read from the sensor.
//                This is typically only non-zero for imaging sensors,
//                where it reflects the time required to scan the pixel
//                array to find the indicated plunger position.  The time
//                is usually zero or negligible for analog sensor types, 
//                since the only "analysis" is a multiplication to rescale 
//                the ADC sample.
//
// If the sensor is an imaging sensor type, this will be followed by a
// series of pixel messages.  The imaging sensor types have too many pixels
// to send in a single USB transaction, so the device breaks up the array
// into as many packets as needed and sends them in sequence.  For non-
// imaging sensors, the "number of pixels" field in the lead packet is
// zero, so obviously no pixel packets will follow.  If the "calibration
// active" bit in the flags byte is set, no pixel packets are sent even
// if the sensor is an imaging type, since the transmission time for the
// pixels would intefere with the calibration process.  If pixels are sent,
// they're sent in order starting at the first pixel.  The format of each 
// pixel packet is:
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
// Note that we currently only support one-dimensional imaging sensors
// (i.e., pixel arrays that are 1 pixel wide).  The report format doesn't
// have any provision for a two-dimensional layout.  The KL25Z probably
// isn't powerful enough to do real-time image analysis on a 2D image
// anyway, so it's unlikely that we'd be able to make 2D sensors work at
// all, but if we ever add such a thing we'll have to upgrade the report 
// format here accordingly.
// 
//
// 2B. Configuration report.
// This is requested by sending custom protocol message 65 4 (see below).
// In reponse, the device sends one report to the host using this format:
//
//    bytes 0:1 = 0x8800.  This has the bit pattern 10001 in the high
//                5 bits, which distinguishes it from regular joystick
//                reports and from other special report types.
//    bytes 2:3 = total number of outputs, little endian
//    bytes 6:7 = plunger calibration zero point, little endian
//    bytes 8:9 = plunger calibration maximum point, little endian
//    byte  10  = plunger calibration release time, in milliseconds
//    byte  11  = bit flags: 
//                 0x01 -> configuration loaded; 0 in this bit means that
//                         the firmware has been loaded but no configuration
//                         has been sent from the host
//    The remaining bytes are reserved for future use.
//
// 2C. Device ID report.
// This is requested by sending custom protocol message 65 7 (see below).
// In response, the device sends one report to the host using this format:
//
//    bytes 0:1 = 0x9000.  This has bit pattern 10010 in the high 5 bits
//                to distinguish this from other report types.
//    byte 2    = ID type.  This is the same ID type sent in the request.
//    bytes 3-12 = requested ID.  The ID is 80 bits in big-endian byte
//                order.  For IDs longer than 80 bits, we truncate to the
//                low-order 80 bits (that is, the last 80 bits).
//
//                ID type 1 = CPU ID.  This is the globally unique CPU ID
//                  stored in the KL25Z CPU.
//
//                ID type 2 = OpenSDA ID.  This is the globally unique ID
//                  for the connected OpenSDA controller, if known.  This
//                  allow the host to figure out which USB MSD (virtual
//                  disk drive), if any, represents the OpenSDA module for
//                  this Pinscape USB interface.  This is primarily useful
//                  to determine which MSD to write in order to update the
//                  firmware on a given Pinscape unit.
//
// 2D. Configuration variable report.
// This is requested by sending custom protocol message 65 9 (see below).
// In response, the device sends one report to the host using this format:
//
//   bytes 0:1 = 0x9800.  This has bit pattern 10011 in the high 5 bits
//               to distinguish this from other report types.
//   byte  2   = Variable ID.  This is the same variable ID sent in the
//               query message, to relate the reply to the request.
//   bytes 3-8 = Current value of the variable, in the format for the
//               individual variable type.  The variable formats are
//               described in the CONFIGURATION VARIABLES section below.
//
// 2E. Software build information report.
// This is requested by sending custom protocol message 65 10 (see below).
// In response, the device sends one report using this format:
//
//   bytes 0:1 = 0xA0.  This has bit pattern 10100 in the high 5 bits
//               to distinguish it from other report types.
//   bytes 2:5 = Build date.  This is returned as a 32-bit integer,
//               little-endian as usual, encoding a decimal value
//               in the format YYYYMMDD giving the date of the build.
//               E.g., Feb 16 2016 is encoded as 20160216 (decimal).
//   bytes 6:9 = Build time.  This is a 32-bit integer, little-endian,
//               encoding a decimal value in the format HHMMSS giving
//               build time on a 24-hour clock.
//
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
// first output setting.  The way the LedWiz creators conceived this, an
// SBA message is distinguishable from a PBA because there's no such thing
// as a brightness level 64, hence 64 is never valid as a byte in an PBA
// message, hence a message starting with 64 must be something other than
// an PBA message.
//
// Our extended protocol uses the same principle, taking advantage of the
// many other byte values that are also invalid in PBA messages.  To be a 
// valid PBA message, the first byte must be in the range 0-49 or 129-132.  
// As already mentioned, byte value 64 indicates an SBA message, so we
// can't use that one for private extensions.  This still leaves many
// other byte values for us, though, namely 50-63, 65-128, and 133-255.


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
//             the image is broken up into as many messages as necessary.)  The device
//             then resumes sending normal joystick messages.  If the plunger sensor 
//             isn't an imaging type, or no sensor is installed, no pixel messages are 
//             sent.  Parameters:
//
//               third byte = bit flags:
//                  0x01 = low res mode.  The device rescales the sensor pixel array
//                         sent in the dump messages to a low-resolution subset.  The
//                         size of the subset is determined by the device.  This has
//                         no effect on the sensor operation; it merely reduces the
//                         USB transmission time to allow for a faster frame rate for
//                         viewing in the config tool.
//
//               fourth byte = extra exposure time in 100us (.1ms) increments.  For
//                  imaging sensors, we'll add this delay to the minimum exposure 
//                  time.  This allows the caller to explicitly adjust the exposure
//                  level for calibration purposes.
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
//               third byte = delay time in seconds.  The device will wait this long
//               before disconnecting, to allow the PC to perform any cleanup tasks
//               while the device is still attached (e.g., modifying Windows device
//               driver settings)
//
//        7 -> Query device ID.  The device replies with a special device ID report
//             (see above; see also USBJoystick.cpp), then resumes sending normal
//             joystick reports.
//
//             The third byte of the message is the ID index to retrieve:
//
//                 1 = CPU ID: returns the KL25Z globally unique CPU ID.
//
//                 2 = OpenSDA ID: returns the OpenSDA TUID.  This must be patched
//                     into the firmware by the PC host when the .bin file is
//                     installed onto the device.  This will return all 'X' bytes
//                     if the value wasn't patched at install time.
//
//        8 -> Engage/disengage night mode.  The third byte of the message is 1 to
//             engage night mode, 0 to disengage night mode.  The current mode isn't 
//             stored persistently; night mode is always off after a reset.
//
//        9 -> Query configuration variable.  The second byte is the config variable
//             number (see the CONFIGURATION VARIABLES section below).  For the array
//             variables (button assignments, output ports), the third byte is the
//             array index.  The device replies with a configuration variable report
//             (see above) with the current setting for the requested variable.
//
//       10 -> Query software build information.  No parameters.  This replies with
//             the software build information report (see above).
//
// 66  -> Set configuration variable.  The second byte of the message is the config
//        variable number, and the remaining bytes give the new value for the variable.
//        The value format is specific to each variable; see the CONFIGURATION VARIABLES
//        section below for a list of the variables and their formats.  This command
//        only sets the value in RAM; it doesn't write the value to flash and doesn't 
//        put the change into effect.  To save the new settings, the host must send a 
//        type 65 subtype 6 message (see above).  That saves the settings to flash and
//        reboots the device, which makes the new settings active.
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
//        This message is the way to address ports 33 and higher.  Original LedWiz
//        protocol messages can't access ports above 32, since the protocol is
//        hard-wired for exactly 32 ports.
//
//        Note that the extended output messages differ from regular LedWiz commands
//        in two ways.  First, the brightness is the ONLY attribute when an output is
//        set using this mode.  There's no separate ON/OFF state per output as there 
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
//
//        (The precedence scheme is designed to accommodate a mix of legacy and DOF
//        software transparently.  The behavior described is really just to ensure
//        transparent interoperability; it's not something that host software writers
//        should have to worry about.  We expect that anyone writing new software will
//        just use the extended protocol and ignore the old LedWiz commands, since
//        the extended protocol is easier to use and more powerful.)


// ------- CONFIGURATION VARIABLES -------
//
// Message type 66 (see above) sets one configuration variable.  The second byte
// of the message is the variable ID, and the rest of the bytes give the new
// value, in a variable-specific format.  16-bit values are little endian.
// Any bytes at the end of the message not otherwise specified are reserved
// for future use and should always be set to 0 in the message data.
//
// 0  -> QUERY ONLY: Describe the configuration variables.  The device
//       sends a config variable query report with the following fields:
//         
//         byte 3  -> number of scalar (non-array) variables (these are
//                    numbered sequentially from 1 to N)
//         byte 4  -> number of array variables (these are numbered
//                    sequentially from 256-N to 255)
//          
//       The description query is meant to allow the host to capture all
//       configuration settings on the device without having to know what
//       the variables mean or how many there are.  This is useful for
//       backing up the settings in a file on the PC, for example, or for
//       capturing them to restore after a firmware update.  This allows
//       more flexible interoperability between unsynchronized versions 
//       of the firmware and the host software.
//
// 1  -> USB device ID.  This sets the USB vendor and product ID codes
//       to use when connecting to the PC.  For LedWiz emulation, use
//       vendor 0xFAFA and product 0x00EF + unit# (where unit# is the
//       nominal LedWiz unit number, from 1 to 16).  If you have any
//       REAL LedWiz units in your system, we recommend starting the
//       Pinscape LedWiz numbering at 8 to avoid conflicts with the 
//       real LedWiz units.  If you don't have any real LedWiz units,
//       you can number your Pinscape units starting from 1.
//
//       If LedWiz emulation isn't desired or causes host conflicts, 
//       use our private ID: Vendor 0x1209, product 0xEAEA.  (These IDs
//       are registered with http://pid.codes, a registry for open-source 
//       USB devices, so they're guaranteed to be free of conflicts with
//       other properly registered devices).  The device will NOT appear
//       as an LedWiz if you use the private ID codes, but DOF (R3 or 
//       later) will still recognize it as a Pinscape controller.
//
//         bytes 3:4 -> USB Vendor ID
//         bytes 5:6 -> USB Product ID
//
// 2  -> Pinscape Controller unit number for DOF.  The Pinscape unit
//       number is independent of the LedWiz unit number, and indepedent
//       of the USB vendor/product IDs.  DOF (R3 and later) uses this to 
//       identify the unit for the extended Pinscape functionality.
//       For easiest DOF configuration, we recommend numbering your
//       units sequentially starting at 1 (regardless of whether or not
//       you have any real LedWiz units).
//
//         byte 3 -> unit number, from 1 to 16
//
// 3  -> Enable/disable joystick reports.  
//
//         byte 2 -> 1 to enable, 0 to disable
//
//       When joystick reports are disabled, the device registers as a generic HID 
//       device, and only sends the private report types used by the Windows config 
//       tool.  It won't appear to Windows as a USB game controller or joystick.
//
//       Note that this doesn't affect whether the device also registers a keyboard
//       interface.  A keyboard interface will appear if and only if any buttons
//       (including virtual buttons, such as the ZB Launch Ball feature) are assigned 
//       to generate keyboard key input.
//
// 4  -> Accelerometer orientation.
//
//        byte 3 -> orientation:
//           0 = ports at front (USB ports pointing towards front of cabinet)
//           1 = ports at left
//           2 = ports at right
//           3 = ports at rear
//
// 5  -> Plunger sensor type.
//
//        byte 3 -> plunger type:
//           0 = none (disabled)
//           1 = TSL1410R linear image sensor, 1280x1 pixels, serial mode
//          *2 = TSL1410R, parallel mode
//           3 = TSL1412R linear image sensor, 1536x1 pixels, serial mode
//          *4 = TSL1412R, parallel mode
//           5 = Potentiometer with linear taper, or any other device that
//               represents the position reading with a single analog voltage
//          *6 = AEDR8300 optical quadrature sensor, 75lpi
//          *7 = AS5304 magnetic quadrature sensor, 160 steps per 2mm
//
//       * The sensor types marked with asterisks (*) are reserved for types
//       that aren't currently implemented but could be added in the future.  
//       Selecting these types will effectively disable the plunger.  
//
// 6  -> Plunger pin assignments.
//
//         byte 3 -> pin assignment 1
//         byte 4 -> pin assignment 2
//         byte 5 -> pin assignment 3
//         byte 6 -> pin assignment 4
//
//       All of the pins use the standard GPIO port format (see "GPIO pin number
//       mappings" below).  The actual use of the four pins depends on the plunger
//       type, as shown below.  "NC" means that the pin isn't used at all for the
//       corresponding plunger type.
//
//         Plunger Type              Pin 1            Pin 2             Pin 3           Pin 4
//
//         TSL1410R/1412R, serial    SI (DigitalOut)  CLK (DigitalOut)  AO (AnalogIn)   NC
//         TSL1410R/1412R, parallel  SI (DigitalOut)  CLK (DigitalOut)  AO1 (AnalogIn)  AO2 (AnalogIn)
//         Potentiometer             AO (AnalogIn)    NC                NC              NC
//         AEDR8300                  A (InterruptIn)  B (InterruptIn)   NC              NC
//         AS5304                    A (InterruptIn)  B (InterruptIn)   NC              NC
//
// 7  -> Plunger calibration button pin assignments.
//
//         byte 3 -> features enabled/disabled: bit mask consisting of:
//                   0x01  button input is enabled
//                   0x02  lamp output is enabled
//         byte 4 -> DigitalIn pin for the button switch
//         byte 5 -> DigitalOut pin for the indicator lamp
//
//       Note that setting a pin to NC (Not Connected) will disable it even if the
//       corresponding feature enable bit (in byte 3) is set.
//
// 8  -> ZB Launch Ball setup.  This configures the ZB Launch Ball feature.
//
//         byte 3    -> LedWiz port number (1-255) mapped to "ZB Launch Ball" in DOF
//         byte 4    -> key type
//         byte 5    -> key code
//         bytes 6:7 -> "push" distance, in 1/1000 inch increments (16 bit little endian)
//
//       Set the port number to 0 to disable the feature.  The key type and key code
//       fields use the same conventions as for a button mapping (see below).  The
//       recommended push distance is 63, which represents .063" ~ 1/16".
//
// 9  -> TV ON relay setup.  This requires external circuitry implemented on the
//       Expansion Board (or an equivalent circuit as described in the Build Guide).
//
//         byte 3    -> "power status" input pin (DigitalIn)
//         byte 4    -> "latch" output (DigitalOut)
//         byte 5    -> relay trigger output (DigitalOut)
//         bytes 6:7 -> delay time in 10ms increments (16 bit little endian);
//                      e.g., 550 (0x26 0x02) represents 5.5 seconds
//
//       Set the delay time to 0 to disable the feature.  The pin assignments will
//       be ignored if the feature is disabled.
//
// 10 -> TLC5940NT setup.  This chip is an external PWM controller, with 32 outputs
//       per chip and a serial data interface that allows the chips to be daisy-
//       chained.  We can use these chips to add an arbitrary number of PWM output 
//       ports for the LedWiz emulation.
//
//          byte 3 = number of chips attached (connected in daisy chain)
//          byte 4 = SIN pin - Serial data (must connect to SPIO MOSI -> PTC6 or PTD2)
//          byte 5 = SCLK pin - Serial clock (must connect to SPIO SCLK -> PTC5 or PTD1)
//          byte 6 = XLAT pin - XLAT (latch) signal (any GPIO pin)
//          byte 7 = BLANK pin - BLANK signal (any GPIO pin)
//          byte 8 = GSCLK pin - Grayscale clock signal (must be a PWM-out capable pin)
//
//       Set the number of chips to 0 to disable the feature.  The pin assignments are 
//       ignored if the feature is disabled.
//
// 11 -> 74HC595 setup.  This chip is an external shift register, with 8 outputs per
//       chip and a serial data interface that allows daisy-chaining.  We use this
//       chips to add extra digital outputs for the LedWiz emulation.  In particular,
//       the Chime Board (part of the Expansion Board suite) uses these to add timer-
//       protected outputs for coil devices (knockers, chimes, bells, etc).
//
//          byte 3 = number of chips attached (connected in daisy chain)
//          byte 4 = SIN pin - Serial data (any GPIO pin)
//          byte 5 = SCLK pin - Serial clock (any GPIO pin)
//          byte 6 = LATCH pin - LATCH signal (any GPIO pin)
//          byte 7 = ENA pin - ENABLE signal (any GPIO pin)
//
//       Set the number of chips to 0 to disable the feature.  The pin assignments are
//       ignored if the feature is disabled.
//
// 12 -> Disconnect reboot timeout.  The reboot timeout allows the controller software
//       to automatically reboot the KL25Z after it detects that the USB connection is
//       broken.  On some hosts, the device isn't able to reconnect after the initial
//       connection is lost.  The reboot timeout is a workaround for these cases.  When
//       the software detects that the connection is no longer active, it will reboot
//       the KL25Z automatically if a new connection isn't established within the
//       timeout period.  Set the timeout to 0 to disable the feature (i.e., the device
//       will never automatically reboot itself on a broken connection).
//
//          byte 3 -> reboot timeout in seconds; 0 = disabled
//
// 13 -> Plunger calibration.  In most cases, the calibration is set internally by the
//       device by running the calibration procedure.  However, it's sometimes useful
//       for the host to be able to get and set the calibration, such as to back up
//       the device settings on the PC, or to save and restore the current settings
//       when installing a software update.
//
//         bytes 3:4 = rest position (unsigned 16-bit little-endian)
//         bytes 5:6 = maximum retraction point (unsigned 16-bit little-endian)
//         byte  7   = measured plunger release travel time in milliseconds
//
// 14 -> Expansion board configuration.  This doesn't affect the controller behavior
//       directly; the individual options related to the expansion boards (such as 
//       the TLC5940 and 74HC595 setup) still need to be set separately.  This is
//       stored so that the PC config UI can store and recover the information to
//       present in the UI.  For the "classic" KL25Z-only configuration, simply set 
//       all of the fields to zero.
//
//         byte 3 = board set type.  At the moment, the Pinscape expansion boards
//                  are the only ones supported in the software.  This allows for
//                  adding new designs or independent designs in the future.
//                    0 = Standalone KL25Z (no expansion boards)
//                    1 = Pinscape expansion boards
//
//         byte 4 = board set interface revision.  This *isn't* the version number
//                  of the board itself, but rather of its software interface.  In
//                  other words, this doesn't change every time the EAGLE layout
//                  for the board changes.  It only changes when a revision is made
//                  that affects the software, such as a GPIO pin assignment.
//
//                  For Pinscape expansion boards (board set type = 1):
//                    0 = first release (Feb 2016)
//
//         bytes 5:8 = additional hardware-specific data.  These slots are used
//                  to store extra data specific to the expansion boards selected.
//
//                  For Pinscape expansion boards (board set type = 1):
//                    byte 5 = number of main interface boards
//                    byte 6 = number of MOSFET power boards
//                    byte 7 = number of chime boards
//
// 15 -> Night mode setup.  
//
//       byte 3 = button number - 1..MAX_BUTTONS, or 0 for none.  This selects
//                a physically wired button that can be used to control night mode.
//                The button can also be used as normal for PC input if desired.
//                Note that night mode can still be activated via a USB command
//                even if no button is assigned.
//
//       byte 4 = flags:
//
//                0x01 -> The wired input is an on/off switch: night mode will be
//                        active when the input is switched on.  If this bit isn't
//                        set, the input is a momentary button: pushing the button
//                        toggles night mode.
//
//                0x02 -> Night Mode is assigned to the SHIFTED button (see Shift
//                        Button setup at variable 16).  This can only be used
//                        in momentary mode; it's ignored if flag bit 0x01 is set.
//                        When the shift flag is set, the button only toggles
//                        night mode when you press it while also holding down
//                        the Shift button.                        
//
//       byte 5 = indicator output number - 1..MAX_OUT_PORTS, or 0 for none.  This
//                selects an output port that will be turned on when night mode is
//                activated.  Night mode activation overrides any setting made by
//                the host.
//
// 16 -> Shift Button setup.  One button can be designated as a "Local Shift
//       Button" that can be pressed to select a secondary meaning for other
//       buttons.  This isn't to be confused with the PC Shift keys; those can
//       be programmed using the USB key codes for Left Shift and Right Shift.
//       Rather, this applies a LOCAL shift feature in the cabinet button that
//       lets you select a secondary meaning.  For example, you could assign
//       the Start button to the "1" key (VP "Start Game") normally, but have
//       its meaning change to the "5" key ("Insert Coin") when the shift
//       button is pressed.  This provides access to more control functions
//       without adding more physical buttons.
//
//       The shift button itself can also have a regular key assignment.  If
//       it does, the key is only sent to the PC when you RELEASE the shift 
//       button, and then only if no other key with a shifted key code assigned
//       was pressed while the shift button was being held down.  If another 
//       key was pressed, and it has a shifted meaning assigned, we assume that
//       the shift button was only pressed in the first place for its shifting
//       function rather than for its normal keystroke.  This dual usage lets
//       you make the shifting function even more unobtrusive by assigning it
//       to an ordinary button that has its own purpose when not used as a
//       shift button.  For example, you could assign the shift function to the
//       rarely used Extra Ball button.  In those cases where you actually want 
//       to use the Extra Ball feature, it's there, but you also get more
//       mileage out of the button by using it to select secondary mappings for
//       other buttons.
//
//       byte 3 = button number - 1..MAX_BUTTONS, or 0 for none.
//
//
// ARRAY VARIABLES:  Each variable below is an array.  For each get/set message,
// byte 3 gives the array index.  These are grouped at the top end of the variable 
// ID range to distinguish this special feature.  On QUERY, set the index byte to 0 
// to query the number of slots; the reply will be a report for the array index
// variable with index 0, with the first (and only) byte after that indicating
// the maximum array index.
//
// 253 -> Extended input button setup.  This adds on to the information set by 
//        variable 254 below, accessing additional fields.  The "shifted" key
//        type and code fields assign a secondary meaning to the button that's
//        used when the local Shift button is being held down.  See variable 16 
//        above for more details on the Shift button.
//
//          byte 3 = Button number 91..MAX_BUTTONS
//          byte 4 = shifted key type (same codes as "key type" in var 254)
//          byte 5 = shifted key code (same meaning as "key code" in var 254)
//
// 254 -> Input button setup.  This sets up one button; it can be repeated for each
//        button to be configured.  There are MAX_EXT_BUTTONS button slots (see
//        config.h for the constant definition), numbered 1..MAX_EXT_BUTTONS.  Each
//        slot can be configured as a joystick button, a regular keyboard key, or a
//        media control key (mute, volume up, volume down).
//
//        The bytes of the message are:
//          byte 3 = Button number (1..MAX_BUTTONS)
//          byte 4 = GPIO pin for the button input; mapped as a DigitalIn port
//          byte 5 = key type reported to PC when button is pushed:
//                    0 = none (no PC input reported when button pushed)
//                    1 = joystick button -> byte 6 is the button number, 1-32
//                    2 = regular keyboard key -> byte 6 is the USB key code (see below)
//                    3 = media key -> byte 6 is the USB media control code (see below)
//          byte 6 = key code, which depends on the key type in byte 5
//          byte 7 = flags - a combination of these bit values:
//                    0x01 = pulse mode.  This reports a physical on/off switch's state
//                           to the host as a brief key press whenever the switch changes
//                           state.  This is useful for the VPinMAME Coin Door button,
//                           which requires the End key to be pressed each time the
//                           door changes state.
//
// 255 -> LedWiz output port setup.  This sets up one output port; it can be repeated
//        for each port to be configured.  There are 128 possible slots for output ports, 
//        numbered 1 to 128.  The number of ports atcually active is determined by
//        the first DISABLED port (type 0).  For example, if ports 1-32 are set as GPIO
//        outputs and port 33 is disabled, we'll report to the host that we have 32 ports,
//        regardless of the settings for post 34 and higher.
//
//        The bytes of the message are:
//          byte 3 = LedWiz port number (1 to MAX_OUT_PORTS)
//          byte 4 = physical output type:
//                    0 = Disabled.  This output isn't used, and isn't visible to the
//                        LedWiz/DOF software on the host.  The FIRST disabled port
//                        determines the number of ports visible to the host - ALL ports
//                        after the first disabled port are also implicitly disabled.
//                    1 = GPIO PWM output: connected to GPIO pin specified in byte 5,
//                        operating in PWM mode.  Note that only a subset of KL25Z GPIO
//                        ports are PWM-capable.
//                    2 = GPIO Digital output: connected to GPIO pin specified in byte 5,
//                        operating in digital mode.  Digital ports can only be set ON
//                        or OFF, with no brightness/intensity control.  All pins can be
//                        used in this mode.
//                    3 = TLC5940 port: connected to TLC5940 output port number specified 
//                        in byte 5.  Ports are numbered sequentially starting from port 0
//                        for the first output (OUT0) on the first chip in the daisy chain.
//                    4 = 74HC595 port: connected to 74HC595 output port specified in byte 5.
//                        As with the TLC5940 outputs, ports are numbered sequentially from 0
//                        for the first output on the first chip in the daisy chain.
//                    5 = Virtual output: this output port exists for the purposes of the
//                        LedWiz/DOF software on the host, but isn't physically connected
//                        to any output device.  This can be used to create a virtual output
//                        for the DOF ZB Launch Ball signal, for example, or simply as a
//                        placeholder in the LedWiz port numbering.  The physical output ID 
//                        (byte 5) is ignored for this port type.
//          byte 5 = physical output port, interpreted according to the value in byte 4
//          byte 6 = flags: a combination of these bit values:
//                    0x01 = active-high output (0V on output turns attached device ON)
//                    0x02 = noisemaker device: disable this output when "night mode" is engaged
//                    0x04 = apply gamma correction to this output
//
//        Note that the on-board LED segments can be used as LedWiz output ports.  This
//        is useful for testing a new installation with DOF or other PC software without
//        having to connect any external devices.  Assigning the on-board LED segments to
//        output ports overrides their normal status/diagnostic display use, so the normal
//        status flash pattern won't appear when they're used this way.
//


// --- GPIO PIN NUMBER MAPPINGS ---
//
// In USB messages that specify GPIO pin assignments, pins are identified by
// 8-bit integers.  The special value 0xFF means NC (not connected).  All actual
// pins are mapped with the port number in the top 3 bits and the pin number in
// the bottom 5 bits.  Port A=0, B=1, ..., E=4.  For example, PTC7 is port C (2)
// pin 7, so it's represented as (2 << 5) | 7.


// --- USB KEYBOARD SCAN CODES ---
//
// For regular keyboard keys, we use the standard USB HID scan codes
// for the US keyboard layout.  The scan codes are defined by the USB
// HID specifications; you can find a full list in the official USB
// specs.  Some common codes are listed below as a quick reference.
//
//    Key name         -> USB scan code (hex)
//    A-Z              -> 04-1D
//    top row 1!->0)   -> 1E-27
//    Return           -> 28
//    Escape           -> 29
//    Backspace        -> 2A
//    Tab              -> 2B
//    Spacebar         -> 2C
//    -_               -> 2D
//    =+               -> 2E
//    [{               -> 2F
//    ]}               -> 30
//    \|               -> 31
//    ;:               -> 33
//    '"               -> 34
//    `~               -> 35
//    ,<               -> 36
//    .>               -> 37
//    /?               -> 38
//    Caps Lock        -> 39
//    F1-F12           -> 3A-45
//    F13-F24          -> 68-73
//    Print Screen     -> 46
//    Scroll Lock      -> 47
//    Pause            -> 48
//    Insert           -> 49
//    Home             -> 4A
//    Page Up          -> 4B
//    Del              -> 4C
//    End              -> 4D
//    Page Down        -> 4E
//    Right Arrow      -> 4F
//    Left Arrow       -> 50
//    Down Arrow       -> 51
//    Up Arrow         -> 52
//    Num Lock/Clear   -> 53
//    Keypad / * - +   -> 54 55 56 57
//    Keypad Enter     -> 58
//    Keypad 1-9       -> 59-61
//    Keypad 0         -> 62
//    Keypad .         -> 63
//    Mute             -> 7F
//    Volume Up        -> 80
//    Volume Down      -> 81
//    Left Control     -> E0
//    Left Shift       -> E1
//    Left Alt         -> E2
//    Left GUI         -> E3
//    Right Control    -> E4
//    Right Shift      -> E5
//    Right Alt        -> E6
//    Right GUI        -> E7
//
// Due to limitations in Windows, there's a limit of 6 regular keys
// pressed at the same time.  The shift keys in the E0-E7 range don't
// count against this limit, though, since they're encoded as modifier
// keys; all of these can be pressed at the same time in addition to 6
// regular keys.

// --- USB MEDIA CONTROL SCAN CODES ---
//
// Buttons mapped to type 3 are Media Control buttons.  These select
// a small set of common media control functions.  We recognize the
// following type codes only:
//
//   Mute              -> E2
//   Volume up         -> E9
//   Volume Down       -> EA
//   Next Track        -> B5
//   Previous Track    -> B6
//   Stop              -> B7
//   Play/Pause        -> CD
