// USB Message Protocol
//
// This file is purely for documentation, to describe our USB protocol
// for incoming messages (host to device).  We use the standard HID setup 
// with one endpoint in each direction.  See USBJoystick.cpp and .h for
// the USB descriptors.
//
// Our incoming message protocol is an extended version of the protocol 
// used by the LedWiz.  Our protocol is designed to be 100% backwards
// compatible with clients using the original LedWiz wire protocol, as long 
// as they only send well-formed messages in the original protocol.  The
// "well-formed" part is an important condition, because our extensions to
// the original protocol all consist of messages that aren't defined in the
// original protocol and are meaningless to a real LedWiz.
//
// The protocol compatibility ensures that all original LedWiz clients can
// also transparently access a Pinscape unit.  Clients will simply think the
// Pinscape unit is an LedWiz, thus they'll be able to operate 32 of our
// ports.  We designate the first 32 ports (ports 1-32) as the ones accessible
// through the LedWiz protocol.
//
// In addition the wire-level protocol compatibility, we can provide legacy
// LedWiz clients with access to more than 32 ports by emulating multiple
// virtual LedWiz units.  We can't do this across the wire protocol, since
// the KL25Z USB interface constrains us to a single VID/PID (which is how
// LedWiz clients distinguish units).  However, virtuall all legacy LedWiz
// clients access the device through a shared library, LEDWIZ.DLL, rather
// than directly through USB.  LEDWIZ.DLL is distributed by the LedWiz's
// manufacturer and has a published client interface.  We can thus provide
// a replacement DLL that contains the logic needed to recognize a Pinscape
// unit and represent it to clients as multiple LedWiz devices.  This allows
// old clients to access our full complement of ports without any changes
// to the clients.  We define some extended message types (SBX and PBX)
// specifically to support this DLL feature.
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
//              0x04,0x08,0x10 -> power sense status: meaningful only when
//                      the TV-on timer is used.  Figure (ss>>2) & 0x07 to
//                      isolate the status bits.  The resulting value is:
//                         1 -> latch was on at last check
//                         2 -> latch was off at last check, SET pin high
//                         3 -> latch off, SET pin low, ready to check status
//                         4 -> TV timer countdown in progress
//                         5 -> TV relay is on
//                         6 -> sending IR signals designated as TV ON signals
//              0x20 -> IR learning mode in progress
//              0x40 -> configuration saved successfully (see below)
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
// Status bit 0x40 is set after a successful configuration update via special
// command 65 6 (save config to flash).  The device always reboots after this
// command, so if the host wants to receive a status update verifying the 
// save, it has to request a non-zero reboot delay in the message to allow
// us time to send at least one of these status reports after the save.
// This bit is only sent after a successful save, which means that the flash
// write succeeded and the written sectors verified as correct.
// NOTE: older firmware versions didn't support this status bit, so clients
// can't interpret the lack of a response as a failure for older versions.
// To determine if the flag is supported, check the config report feature
// flags.
//
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
//    byte  2   = 0 -> first status report packet
//    bytes 3:4 = number of pixels to be sent in following messages, as
//                an unsigned 16-bit little-endian integer.  This is 0 if 
//                the sensor isn't an imaging type.
//    bytes 5:6 = current plunger position registered on the sensor.  This
//                is on the *native* scale for the sensor, which might be
//                different from joystick units.  By default, the native
//                scale is the number of pixels for an imaging sensor, or
//                4096 for other sensor types.  The actual native scale can
//                be reported separately via a second status report message
//                (see below).
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
// An optional second message provides additional information:
//
//    bytes 0:1 = 0x87FF
//    byte  2   = 1 -> second status report packet
//    bytes 3:4 = Native sensor scale.  This is the actual native scale
//                used for the position report in the first status report
//                packet above.
//    bytes 5:6 = Jitter window lower bound, in native sensor scale units.
//    bytes 7:8 = Jitter window upper bound, in native sensor scale units.
//                The jitter window bounds reflect the current jitter filter
//                status as of this reading.
//    bytes 9:10 = Raw sensor reading before jitter filter was applied.
//    bytes 11:12 = Auto-exposure time in microseconds
//
// An optional third message provides additional information specifically
// for bar-code sensors:
//
//    bytes 0:1 = 0x87FF
//    byte  2   = 2 -> bar code status report
//    byte  3   = number of bits in bar code
//    byte  4   = bar code type:
//                  1 = Gray code/Manchester bit coding
//    bytes 5:6 = pixel offset of first bit
//    byte  7   = width in pixels of each bit
//    bytes 8:9 = raw bar code bits
//    bytes 10:11 = mask of successfully read bar code bits; a '1' bit means
//                that the bit was read successfully, '0' means the bit was
//                unreadable
//
// An optional third message provides additional information specifically
// for digital quadrature sensors:
//
//    bytes 0:1 = 0x87FF
//    byte  2   = 3 -> digital quadrature sensor status report
//    byte  3   = "A" channel reading (0 or 1)
//    byte  4   = "B" channel reading (0 or 1)
//   
//
// If the sensor is an imaging sensor type, this will be followed by a
// series of pixel messages.  The imaging sensor types have too many pixels
// to send in a single USB transaction, so the device breaks up the array
// into as many packets as needed and sends them in sequence.  For non-
// imaging sensors, the "number of pixels" field in the lead packet is
// zero, so obviously no pixel packets will follow.  If the "calibration
// active" bit in the flags byte is set, no pixel packets are sent even
// if the sensor is an imaging type, since the transmission time for the
// pixels would interfere with the calibration process.  If pixels are sent,
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
//    bytes 2:3 = total number of configured outputs, little endian.  This
//                is the number of outputs with assigned functions in the
//                active configuration.
//    byte  4   = Pinscape unit number (0-15)
//    byte  5   = reserved (currently always zero)
//    bytes 6:7 = plunger calibration zero point, little endian
//    bytes 8:9 = plunger calibration maximum point, little endian
//    byte  10  = plunger calibration release time, in milliseconds
//    byte  11  = bit flags: 
//                 0x01 -> configuration loaded; 0 in this bit means that
//                         the firmware has been loaded but no configuration
//                         has been sent from the host
//                 0x02 -> SBX/PBX extension features: 1 in this bit means
//                         that these features are present in this version.
//                 0x04 -> new accelerometer features supported (adjustable
//                         dynamic range, auto-centering on/off, adjustable
//                         auto-centering time)
//                 0x08 -> flash write status flag supported (see flag 0x40
//                         in normal joystick status report)
//                 0x10 -> joystick report timing features supports
//                         (configurable joystick report interval, acceler-
//                         ometer stutter counter)
//                 0x20 -> chime logic is supported
//    bytes 12:13 = available RAM, in bytes, little endian.  This is the amount
//                of unused heap (malloc'able) memory.  The firmware generally
//                allocates all of the dynamic memory it needs during startup,
//                so the free memory figure doesn't tend to fluctuate during 
//                normal operation.  The dynamic memory used is a function of 
//                the set of features enabled.
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
//   bytes 0:1 = 0xA000.  This has bit pattern 10100 in the high 5 bits
//               (and 10100000 in the high 8 bits) to distinguish it from 
//               other report types.
//   bytes 2:5 = Build date.  This is returned as a 32-bit integer,
//               little-endian as usual, encoding a decimal value
//               in the format YYYYMMDD giving the date of the build.
//               E.g., Feb 16 2016 is encoded as 20160216 (decimal).
//   bytes 6:9 = Build time.  This is a 32-bit integer, little-endian,
//               encoding a decimal value in the format HHMMSS giving
//               build time on a 24-hour clock.
//
// 2F. Button status report.
// This is requested by sending custom protocol message 65 13 (see below).
// In response, the device sends one report using this format:
//
//   bytes 0:1 = 0xA1.  This has bit pattern 10100 in the high 5 bits (and
//               10100001 in the high 8 bits) to distinguish it from other 
//               report types.
//   byte 2    = number of button reports
//   byte 3    = Physical status of buttons 1-8, 1 bit each.  The low-order
//               bit (0x01) is button 1.  Each bit is 0 if the button is off,
//               1 if on.  This reflects the physical status of the button
//               input pins, after debouncing but before any logical state
//               processing.  Pulse mode and shifting have no effect on the
//               physical state; this simply indicates whether the button is
//               electrically on (shorted to GND) or off (open circuit).
//   byte 4    = buttons 9-16
//   byte 5    = buttons 17-24
//   byte 6    = buttons 25-32
//   byte 7    = buttons 33-40
//   byte 8    = buttons 41-48
//
// 2G. IR sensor data report.
// This is requested by sending custom protocol message 65 12 (see below).
// That command puts controller in IR learning mode for a short time, during
// which it monitors the IR sensor and send these special reports to relay the
// readings.  The reports contain the raw data, plus the decoded command code
// and protocol information if the controller is able to recognize and decode
// the command.
//
//   bytes 0:1 = 0xA2.  This has bit pattern 10100 in the high 5 bits (and
//               10100010 in the high 8 bits to distinguish it from other 
//               report types.
//   byte 2    = number of raw reports that follow
//   bytes 3:4 = first raw report, as a little-endian 16-bit int.  The
//               value represents the time of an IR "space" or "mark" in
//               2us units.  The low bit is 0 for a space and 1 for a mark.
//               To recover the time in microseconds, mask our the low bit
//               and multiply the result by 2.  Received codes always
//               alternate between spaces and marks.  A space is an interval
//               where the IR is off, and a mark is an interval with IR on.
//               If the value is 0xFFFE (after masking out the low bit), it
//               represents a timeout, that is, a time greater than or equal
//               to the maximum that can be represented in this format,
//               which is 131068us.  None of the IR codes we can parse have
//               any internal signal component this long, so a timeout value 
//               is generally seen only during a gap between codes where 
//               nothing is being transmitted.
//   bytes 4:5 = second raw report
//   (etc for remaining reports)
//
//   If byte 2 is 0x00, it indicates that learning mode has expired without
//   a code being received, so it's the last report sent for the learning
//   session.
//
//   If byte 2 is 0xFF, it indicates that a code has been successfully 
//   learned.  The rest of the report contains the learned code instead
//   of the raw data:
//
//   byte 3 = protocol ID, which is an integer giving an internal code
//            identifying the IR protocol that was recognized for the 
//            received data.  See IRProtocolID.h for a list of the IDs.
//   byte 4 = bit flags:
//            0x02 -> the protocol uses "dittos"
//   bytes 5:6:7:8:9:10:11:12 = a little-endian 64-bit int containing
//            the code received.  The code is essentially the data payload 
//            of the IR packet, after removing bits that are purely
//            structural, such as toggle bits and error correction bits.
//            The mapping between the IR bit stream and our 64-bit is 
//            essentially arbitrary and varies by protocol, but it always
//            has round-trip fidelity: using the 64-bit code value +
//            protocol ID + flags to send an IR command will result in
//            the same IR bit sequence being sent, modulo structural bits 
//            that need to be updates in the reconstruction (such as toggle
//            bits or sequencing codes).
//
//
// WHY WE USE A HACKY APPROACH TO DIFFERENT REPORT TYPES
//
// The HID report system was specifically designed to provide a clean,
// structured way for devices to describe the data they send to the host.
// Our approach isn't clean or structured; it ignores the promises we
// make about the contents of our report via the HID Report Descriptor
// and stuffs our own different data format into the same structure.
//
// We use this hacky approach only because we can't use the standard USB
// HID mechanism for varying report types, which is to provide multiple
// report descriptors and tag each report with a type byte that indicates 
// which descriptor applies.  We can't use that standard approach because
// we want to be 100% LedWiz compatible.  The snag is that some Windows
// LedWiz clients parse the USB HID descriptors as part of identifying a
// USB HID device as a valid LedWiz unit, and will only recognize the device
// if certain properties of the HID descriptors match those of a real LedWiz.
// One of the features that's important to some clients is the descriptor 
// link structure, which is affected by the layout of HID Report Descriptor 
// entries.  In order to match the expected layout, we can only define a 
// single kind of output report.  Since we have to use Joystick reports for 
// the sake of VP and other pinball software, and we're only allowed the 
// one report type, we have to make that one report type the Joystick type.  
// That's why we overload the joystick reports with other meanings.  It's a
// hack, but at least it's a fairly reliable and isolated hack, in that our 
// special reports are only generated when clients specifically ask for 
// them.  Plus, even if a client who doesn't ask for a special report 
// somehow gets one, the worst that happens is that they get a momentary 
// spurious reading from the accelerometer and plunger.



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
// The real LedWiz protocol has two message types, "SBA" and "PBA".  The
// message type can be determined from the first byte of the 8-byte message
// packet: if the first byte 64 (0x40), it's an SBA message.  If the first
// byte is 0-49 or 129-132, it's a PBA message.  All other byte values are
// invalid in the original protocol and have undefined behavior if sent to
// a real LedWiz.  We take advantage of this to extend the protocol with
// our new features by assigning new meanings to byte patterns that have no 
// meaning in the original protocol.
//
// "SBA" message:   64 xx xx xx xx ss 00 00
//     xx = on/off bit mask for 8 outputs
//     ss = global flash speed setting (valid values 1-7)
//     00 = unused/reserved; client should set to zero (not enforced, but
//          strongly recommended in case of future additions)
//
// If the first byte has value 64 (0x40), it's an SBA message.  This type of 
// message sets all 32 outputs individually ON or OFF according to the next 
// 32 bits (4 bytes) of the message, and sets the flash speed to the value in 
// the sixth byte.  The flash speed sets the global cycle rate for flashing
// outputs - outputs with their values set to the range 128-132.  The speed
// parameter is in ad hoc units that aren't documented in the LedWiz API, but
// observations of real LedWiz units show that the "speed" is actually the
// period, each unit representing 0.25s: so speed 1 is a 0.25s period, or 4Hz,
// speed 2 is a 0.5s period or 2Hz, etc., up to speed 7 as a 1.75s period or
// 0.57Hz.  The period is the full waveform cycle time.
//
//
// "PBA" message:  bb bb bb bb bb bb bb bb
//     bb = brightness level, 0-49 or 128-132
//
// Note that there's no prefix byte indicating this message type.  This
// message is indicated simply by the first byte being in one of the valid
// ranges.
//
// Each byte gives the new brightness level or flash pattern for one part.
// The valid values are:
//
//     0-48 = fixed brightness level, linearly from 0% to 100% intensity
//     49   = fixed brightness level at 100% intensity (same as 48)
//     129  = flashing pattern, fade up / fade down (sawtooth wave)
//     130  = flashing pattern, on / off (square wave)
//     131  = flashing pattern, on for 50% duty cycle / fade down
//     132  = flashing pattern, fade up / on for 50% duty cycle
//
// This message sets new brightness/flash settings for 8 ports.  There's
// no port number specified in the message; instead, the port is given by
// the protocol state.  Specifically, the device has an internal register
// containing the base port for PBA messages.  On reset AND after any SBA
// message is received, the base port is set to 0.  After any PBA message
// is received and processed, the base port is incremented by 8, resetting
// to 0 when it reaches 32.  The bytes of the message set the brightness
// levels for the base port, base port + 1, ..., base port + 7 respectively.
//
//

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
//        1 -> Set the device's LedWiz unit number and plunger status, and save the 
//             changes to flash.  The device automatically reboots after the changes 
//             are saved if the unit number is changed, since this changes the USB
//             product ID code.  The additional bytes of the message give the 
//             parameters:
//
//               third byte  = new LedWiz unit number (0-15, corresponding to nominal 
//                             LedWiz unit numbers 1-16)
//               fourth byte = plunger on/off (0=disabled, 1=enabled)
//
//             Note that this command is from the original version and isn't typically
//             used any more, since the same information has been subsumed into more
//             generalized option settings via the config variable system.
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
//        5 -> Turn all outputs off and restore LedWiz defaults.  Sets all output 
//             ports to OFF and LedWiz brightness/mode setting 48, and sets the LedWiz
//             global flash speed to 2.
//
//        6 -> Save configuration to flash.  This saves all variable updates sent via
//             type 66 messages since the last reboot, then optionally reboots the
//             device to put the changes into effect.  If the flash write succeeds,
//             we set the "flash write OK" bit in our status reports, which we 
//             continue sending between the successful write and the delayed reboot.
//             We don't set the bit or reboot if the write fails.  If the "do not
//             reboot" flag is set, we still set the flag on success for the delay 
//             time, then clear the flag.
//
//               third byte = delay time in seconds.  The device will wait this long
//               before disconnecting, to allow the PC to test for the success bit
//               in the status report, and to perform any cleanup tasks while the 
//               device is still attached (e.g., modifying Windows device driver 
//               settings)
//
//               fourth byte = flags:
//                 0x01 -> do not reboot
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
//       11 -> TV ON relay manual control.  This allows testing and operating the
//             relay from the PC.  This doesn't change the power-up configuration;
//             it merely allows the relay to be controlled directly.  The third
//             byte specifies the relay operation to perform:
//              
//                 0 = turn relay off
//                 1 = turn relay on
//                 2 = pulse the relay as though the power-on delay timer fired
//
//       12 -> Learn IR code.  The device enters "IR learning mode".  While in 
//             learning mode, the device reports the raw signals read through 
//             the IR sensor to the PC through the special IR learning report 
//             (see "2G" above).  If a signal can be decoded through a known 
//             protocol, the device sends a final "2G" report with the decoded 
//             command, then terminates learning mode.  If no signal can be 
//             decoded within a timeout period, the mode automatically ends,
//             and the device sends a final IR learning report with zero raw 
//             signals to indicate termination.  After initiating IR learning 
//             mode, the user should point the remote control with the key to 
//             be learned at the IR sensor on the KL25Z, and press and hold the 
//             key on the remote for a few seconds.  Holding the key for a few
//             moments is important because it lets the decoder sense the type
//             of auto-repeat coding the remote uses.  The learned code can be
//             written to an IR config variable slot to program the controller
//             to send the learned command on events like TV ON or a button
//             press.
//             
//       13 -> Get button status report.  The device sends one button status 
//             report in response (see section "2F" above).
//
//       14 -> Manually center the accelerometer.  This sets the accelerometer
//             zero point to the running average of readings over the past few
//             seconds.
//
//       15 -> Set up ad hoc IR command, part 1.  This sets up the first part 
//             of an IR command to transmit.  The device stores the data in an
//             internal register for later use in message 65 16.  Send the
//             remainder of the command data with 65 16.
//
//               byte 3 = IR protocol ID
//               byte 4 = flags (IRFlagXxx bit flags)
//               byte 5-8 = low-order 32 bits of command code, little-endian
//
//       16 -> Finish and send an ad hoc IR command.  Use message 65 15 first
//             to set up the start of the command data, then send this message 
//             to fill in the rest of the data and transmit the command.  Upon
//             receiving this message, the device performs the transmission.
//
//               byte 3-6 = high-order 32 bits of command code, little-endian
//
//       17 -> Send a pre-programmed IR command.  This immediately transmits an
//             IR code stored in a command slot.
//
//               byte 3 = command number (1..MAX_IR_CODES)
//               
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
// 67  -> "SBX".  This is an extended form of the original LedWiz SBA message.  This
//        version is specifically designed to support a replacement LEDWIZ.DLL on the
//        host that exposes one Pinscape device as multiple virtual LedWiz devices,
//        in order to give legacy clients access to more than 32 ports.  Each virtual
//        LedWiz represents a block of 32 ports.  The format of this message is the
//        same as for the original SBA, with the addition of one byte:
//
//            67 xx xx xx xx ss pp 00
//               xx = on/off switches for 8 ports, one bit per port
//               ss = global flash speed setting for this bank of ports, 1-7
//               pp = port group: 0 for ports 1-32, 1 for ports 33-64, etc
//               00 = unused/reserved; client should set to zero
//
//        As with SBA, this sets the on/off switch states for a block of 32 ports.
//        SBA always addresses ports 1-32; SBX can address any set of 32 ports.
//
//        We keep a separate speed setting for each group of 32 ports.  The purpose
//        of the SBX extension is to allow a custom LEDWIZ.DLL to expose multiple
//        virtual LedWiz units to legacy clients, so clients will expect each unit
//        to have its separate flash speed setting.  Each block of 32 ports maps to
//        a virtual unit on the client side, so each block needs its own speed state.
//
// 68  -> "PBX".  This is an extended form of the original LedWiz PBA message; it's
//        the PBA equivalent of our SBX extension above.
//
//            68 pp ee ee ee ee ee ee
//               pp = port group: 0 for ports 1-8, 1 for 9-16, etc
//               qq = sequence number: 0 for the first 8 ports in the group, etc
//               ee = brightness/flash values, 6 bits per port, packed into the bytes
//
//        The port group 'pp' selects a group of 8 ports.  Note that, unlike PBA,
//        the port group being updated is explicitly coded in the message, which makes
//        the message stateless.  This eliminates any possibility of the client and
//        host getting out of sync as to which ports they're talking about.  This
//        message doesn't affect the PBA port address state.
//
//        The brightness values are *almost* the same as in PBA, but not quite.  We
//        remap the flashing state values as follows:
//
//            0-48 = brightness level, 0% to 100%, on a linear scale
//            49   = brightness level 100% (redundant with 48)
//            60   = PBA 129 equivalent, sawtooth
//            61   = PBA 130 equivalent, square wave (on/off)
//            62   = PBA 131 equivalent, on/fade down
//            63   = PBA 132 equivalent, fade up/on
//
//        We reassign the brightness levels like this because it allows us to pack
//        every possible value into 6 bits.  This allows us to fit 8 port settings
//        into six bytes.  The 6-bit fields are packed into the 8 bytes consecutively
//        starting with the low-order bit of the first byte.  An efficient way to
//        pack the 'ee' fields given the brightness values is to shift each group of 
//        four bytes  into a uint, then shift the uint into three 'ee' bytes:
//
//           unsigned int tmp1 = bri[0] | (bri[1]<<6) | (bri[2]<<12) | (bri[3]<<18);
//           unsigned int tmp2 = bri[4] | (bri[5]<<6) | (bri[6]<<12) | (bri[7]<<18);
//           unsigned char port_group = FIRST_PORT_TO_ADDRESS / 8;
//           unsigned char msg[8] = {
//               68, pp, 
//               tmp1 & 0xFF, (tmp1 >> 8) & 0xFF, (tmp1 >> 16) & 0xFF,
//               tmp2 & 0xFF, (tmp2 >> 8) & 0xFF, (tmp2 >> 16) & 0xFF
//           };
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
// Variable IDs:
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
// 3  -> Joystick report settings.
//
//         byte 3 -> Enable joystick interface: 1 to enable, 0 to disable
//         byte 4 -> Joystick axis format, as a USBJoystick::AXIS_FORMAT_XXX value
//         bytes 5:8 -> Reporting interval in microseconds
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
// 4  -> Accelerometer settings
//
//        byte 3 -> orientation:
//           0 = ports at front (USB ports pointing towards front of cabinet)
//           1 = ports at left
//           2 = ports at right
//           3 = ports at rear
//        byte 4 -> dynamic range
//           0 = +/- 1G (2G hardware mode, but rescales joystick reports to 1G 
//                   range; compatible with older versions)
//           1 = +/- 2G (2G hardware mode)
//           2 = +/- 4G (4G hardware mode)
//           3 = +/- 8G (8G hardware mode)
//        byte 5 -> Auto-centering mode
//           0      = auto-centering on, 5 second timer (default, compatible 
//                    with older versions)
//           1-60   = auto-centering on with the given time in seconds
//           61-245 = reserved
//           255    = auto-centering off; manual centering only
//        byte 6 -> joystick report stutter count: 1 (or 0) means that we
//           take a fresh accelerometer on every joystick report; 2 means
//           that we take a new reading on every other report, and repeat
//           the prior readings on alternate reports; etc
//
// 5  -> Plunger sensor type.
//
//        byte 3 -> plunger type:
//           0 = none (disabled)
//           1 = TSL1410R linear image sensor, 1280x1 pixels, serial mode, edge detection
//           3 = TSL1412R linear image sensor, 1536x1 pixels, serial mode, edge detection
//           5 = Potentiometer with linear taper, or any other device that
//               represents the position reading with a single analog voltage
//           6 = AEDR8300 optical quadrature sensor, 75lpi
//          *7 = AS5304 magnetic quadrature sensor, 160 steps per 2mm
//           8 = TSL1401CL linear image sensor, 128x1 pixel, bar code detection
//           9 = VL6180X time-of-flight distance sensor
//        **10 = AEAT-6012-A06 magnetic rotary encoder
//        **11 = TCD1103GFG Toshiba linear CCD, 1500x1 pixels, edge detection
//        **12 = VCNL4010 Vishay IR proximity sensor
//
//       * The sensor types marked with asterisks (*) are reserved for types
//       that aren't currently implemented but could be added in the future.  
//       Selecting these types will effectively disable the plunger.
//
//       ** Experimental
//
//       Sensor types 2 and 4 were formerly reserved for TSL14xx sensors in
//       parallel wiring mode, but support for these is no longer planned, as
//       the KL25Z's single ADC sampler negates any speed advantage from using
//       the sensors' parallel mode.  Those slots could be reassigned for 
//       other sensors, since they were never enabled in any release version.
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
//       corresponding plunger type.  "GPIO" means that any GPIO pin will work.
//       AnalogIn, InterruptIn, and PWM mean that only pins with the respective 
//       capabilities can be chosen.
//
//         Plunger Type              Pin 1            Pin 2             Pin 3           Pin 4
//
//         TSL1410R/1412R/1401CL     SI (GPIO)        CLK (GPIO)        AO (AnalogIn)   NC
//         Potentiometer             AO (AnalogIn)    NC                NC              NC
//         AEDR8300                  A (InterruptIn)  B (InterruptIn)   NC              NC
//         AS5304                    A (InterruptIn)  B (InterruptIn)   NC              NC
//         VL6180X                   SDA (GPIO)       SCL (GPIO)        GPIO0/CE (GPIO) NC
//         AEAT-6012-A06             CS (GPIO)        CLK (GPIO)        DO (GPIO)       NC
//         TCD1103GFG                fM (PWM)         OS (AnalogIn)     ICG (GPIO)      SH (GPIO)
//         VCNL4010                  SDA (GPIO)       SCL (GPIO)        NC              NC
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
//       If an IR remote control transmitter is installed (see variable 17), we'll
//       also transmit any IR codes designated as TV ON codes when the startup timer
//       finishes.  This allows TVs to be turned on via IR remotes codes rather than
//       hard-wiring them through the relay.  The relay can be omitted in this case.
//
// 10 -> TLC5940NT setup.  This chip is an external PWM controller, with 16 outputs
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
//                    1 = Pinscape Expansion Boards
//                    2 = Pinscape All-In-One (AIO) (Oak Micros)
//
//         byte 4 = board set interface revision.  This *isn't* the version number
//                  of the board itself, but rather of its software interface.  In
//                  other words, this doesn't change every time the EAGLE layout
//                  for the board changes.  It only changes when a revision is made
//                  that affects the software, such as a GPIO pin assignment.
//
//                  For Pinscape Expansion Boards (board set type = 1):
//                    0 = first release (Feb 2016)
//
//                  For AIO (board set type = 2):
//                    0 = first release (2019)
//
//         bytes 5:8 = additional hardware-specific data.  These slots are used
//                  to store extra data specific to the expansion boards selected.
//
//                  For Pinscape Expansion Boards (board set type = 1) and
//                  AIO (type = 2):
//                    byte 5 = number of main interface or AIO boards (always 1)
//                    byte 6 = number of MOSFET power boards
//                    byte 7 = number of chime boards
//                  
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
//       buttons.  This isn't the same as the PC keyboard Shift keys; those can
//       be programmed using the USB key codes for Left Shift and Right Shift.
//       Rather, this applies a LOCAL shift feature in the cabinet button that
//       lets you select a secondary meaning.  For example, you could assign
//       the Start button to the "1" key (VP "Start Game") normally, but have
//       its meaning change to the "5" key ("Insert Coin") when the shift
//       button is pressed.  This provides access to more control functions
//       without adding more physical buttons.
//
//       byte 3 = button number - 1..MAX_BUTTONS, or 0 for none
//       byte 4 = mode (default is 0):
//
//          0 -> Shift OR Key mode.  In this mode, the Shift button doesn't
//               send its assigned key or IR command when initially pressed.
//               Instead, we wait to see if another button is pressed while
//               the Shift button is held down.  If so, this Shift button 
//               press ONLY counts as the Shift function, and its own assigned
//               key is NOT sent to the PC.  On the other hand, if you press
//               the Shift button and then release it without having pressed
//               any other key in the meantime, this press counts as a regular
//               key press, so we send the assigned key to the PC.
//
//          1 -> Shift AND Key mode.  In this mode, the Shift button sends its
//               assigned key when pressed, just like a normal button.  If you
//               press another button while the Shift key is pressed, the
//               shifted meaning of the other key is used.
//
// 17 -> IR Remote Control physical device setup.  We support IR remotes for
//       both sending and receiving.  On the receive side, we can read from a 
//       sensor such as a TSOP384xx.  The sensor requires one GPIO pin with 
//       interrupt support, so any PTAxx or PTDxx pin will work.  On the send 
//       side, we can transmit through any IR LED.  This requires one PWM 
//       output pin.  To enable send and/or receive, specify a valid pin; to
//       disable, set the pin NC (not connected).  Send and receive can be
//       enabled and disabled independently; it's not necessary to enable
//       the transmit function to use the receive function, or vice versa.
//
//       byte 3 = receiver input GPIO pin ID.  Must be interrupt-capable.
//       byte 4 = transmitter pin.  Must be PWM-capable.
//
// 18 -> Plunger auto-zeroing.  This only applies to sensor types with
//       relative positioning, such as quadrature sensors.  Other sensor
//       types simply ignore this.
//
//       byte 3 = bit flags:
//                0x01 -> auto-zeroing enabled
//       byte 4 = auto-zeroing time in seconds
//
// 19 -> Plunger filters.  There are two filters that can be applied:
//
//       - Jitter filter.  This sets a hysteresis window size, to reduce jitter
//       jitter in the plunger reading.  Most sensors aren't perfectly accurate;
//       consecutive readings at the same physical plunger position vary 
//       slightly, wandering in a range near the true reading.  Over time, the 
//       readings will usually average the true value, but that's not much of a 
//       consolation to us because we want to display the position in real time.
//       To reduce the visible jitter, we can apply a hysteresis filter that 
//       hides random variations within the specified window.  The window is in 
//       the sensor's native units, so the effect of a given window size 
//       depends on the sensor type.  A value of zero disables the filter.
//
//       - Reversed orientation.  If set, this inverts the sensor readings, as
//       though the sensor were physically flipped to the opposite direction.
//       This allows for correcting a reversed physical sensor installation in
//       software without having to mess with the hardware.
//
//       byte 3:4 = jitter window size in native sensor units, little-endian
//       byte 5   = orientation filter bit mask:
//                  0x01  -> set if reversed orientation, clear if normal
//                  0x80  -> Read-only: this bit is set if the feature is supported
//
// 20 -> Plunger bar code setup.  Sets parameters applicable only to bar code
//       sensor types.
//
//       bytes 3:4 = Starting pixel offset of bar code (margin width)
//
// 21 -> TLC59116 setup.  This chip is an external PWM controller with 16
//       outputs per chip and an I2C bus interface.  Up to 14 of the chips
//       can be connected to a single bus.  This chip is a successor to the 
//       TLC5940 with a more modern design and some nice improvements, such 
//       as glitch-free startup and a standard (I2C) physical interface.
//
//       Each chip has a 7-bit I2C address.  The top three bits of the
//       address are fixed in the chip itself and can't be configured, but
//       the low four bits are configurable via the address line pins on
//       the chip, A3 A2 A1 A0.  Our convention here is to ignore the fixed
//       three bits and refer to the chip address as just the A3 A2 A1 A0
//       bits.  This gives each chip an address from 0 to 15.
//
//       I2C allows us to discover the attached chips automatically, so in
//       principle we don't need to know which chips will be present.  
//       However, it's useful for the config tool to know which chips are
//       expected so that it can offer them in the output port setup UI.
//       We therefore provide a bit mask specifying the enabled chips.  Each
//       bit specifies whether the chip at the corresponding address is
//       present: 0x0001 is the chip at address 0, 0x0002 is the chip at
//       address 1, etc.  This is mostly for the config tool's use; we only
//       use it to determine if TLC59116 support should be enabled at all,
//       by checking if it's non-zero.
//
//       To disable support, set the populated chip mask to 0.  The pin
//       assignments are all ignored in this case.
//
//          bytes 3:4 = populated chips, as a bit mask (OR in 1<<address
//                   each populated address)
//          byte 5 = SDA (any GPIO pin)
//          byte 6 = SCL (any GPIO pin)
//          byte 7 = RESET (any GPIO pin)
//
// 22 -> Plunger raw calibration data.  Some sensor types need to store
//       additional raw calibration.  We provide three uint16 slots for
//       use by the sensor, with the meaning defined by the sensor subclass.
//
//          bytes 3:4 = raw data 0
//          bytes 5:6 = raw data 1
//          bytes 7:8 = raw data 2
//
//
// SPECIAL DIAGNOSTICS VARIABLES:  These work like the array variables below,
// the only difference being that we don't report these in the number of array
// variables reported in the "variable 0" query.
//
// 220 -> Performance/diagnostics variables.  Items marked "read only" can't
//        be written; any SET VARIABLE messages on these are ignored.  Items
//        marked "diagnostic only" refer to counters or statistics that are
//        collected only when the diagnostics are enabled via the diags.h
//        macro ENABLE_DIAGNOSTICS.  These will simply return zero otherwise.
//
//          byte 3 = diagnostic index (see below)
//
//        Diagnostic index values:
//
//          1 -> Main loop cycle time [read only, diagnostic only]
//               Retrieves the average time of one iteration of the main
//               loop, in microseconds, as a uint32.  This excludes the
//               time spent processing incoming messages, as well as any
//               time spent waiting for a dropped USB connection to be
//               restored.  This includes all subroutine time and polled
//               task time, such as processing button and plunger input,
//               sending USB joystick reports, etc.
//
//          2 -> Main loop message read time [read only, diagnostic only]
//               Retrieves the average time spent processing incoming USB
//               messages per iteration of the main loop, in microseconds, 
//               as a uint32.  This only counts the processing time when 
//               messages are actually present, so the average isn't reduced
//               by iterations of the main loop where no messages are found.
//               That is, if we run a million iterations of the main loop,
//               and only five of them have messages at all, the average time
//               includes only those five cycles with messages to process.
//
//          3 -> PWM update polling time [read only, diagnostic only]
//               Retrieves the average time, as a uint32 in microseconds,
//               spent in the PWM update polling routine.
//
//          4 -> LedWiz update polling time [read only, diagnostic only]
//               Retrieves the average time, as a uint32 in microseconds,
//               units, spent in the LedWiz flash cycle update routine.
//
//
// ARRAY VARIABLES:  Each variable below is an array.  For each get/set message,
// byte 3 gives the array index.  These are grouped at the top end of the variable 
// ID range to distinguish this special feature.  On QUERY, set the index byte to 0 
// to query the number of slots; the reply will be a report for the array index
// variable with index 0, with the first (and only) byte after that indicating
// the maximum array index.
//
// 250 -> IR remote control commands - code part 2.  This stores the high-order
//        32 bits of the remote control for each slot.  These are combined with
//        the low-order 32 bits from variable 251 below to form a 64-bit code.
//        
//          byte 3 = Command slot number (1..MAX_IR_CODES)
//          byte 4 = bits 32..39 of remote control command code
//          byte 5 = bits 40..47
//          byte 6 = bits 48..55
//          byte 7 = bits 56..63
//
// 251 -> IR remote control commands - code part 1.  This stores the protocol
//        identifier and low-order 32 bits of the remote control code for each
//        remote control command slot.  The code represents a key press on a
//        remote, and is usually determined by reading it from the device's
//        actual remote via the IR sensor input feature.  These codes combine
//        with variable 250 above to form a 64-bit code for each slot.
//        See IRRemote/IRProtocolID.h for the protocol ID codes.
//
//          byte 3 = Command slot number (1..MAX_IR_CODES)
//          byte 4 = protocol ID
//          byte 5 = bits 0..7 of remote control command code
//          byte 6 = bits 8..15
//          byte 7 = bits 16..23
//          byte 8 = bits 24..31
//
// 252 -> IR remote control commands - control information.  This stores
//        descriptive information for each remote control command slot.
//        The IR code for each slot is stored in the corresponding array
//        entry in variables 251 & 250 above; the information is split over
//        several variables like this because of the 8-byte command message 
//        size in our USB protocol (which we use for LedWiz compatibility).
//
//          byte 3 = Command slot number (1..MAX_IR_CODES)
//          byte 4 = bit flags:
//                     0x01 -> send this code as a TV ON signal at system start
//                     0x02 -> use "ditto" codes when sending the command
//          byte 5 = key type; same as the key type in an input button variable
//          byte 6 = key code; same as the key code in an input button variable
//
//        Each IR command slot can serve three purposes:
//
//        - First, it can be used as part of the TV ON sequence when the 
//          system powers up, to turn on cabinet TVs that don't power up by 
//          themselves.  To use this feature, set the TV ON bit in the flags.  
//
//        - Second, when the IR sensor receives a command in a given slot, we 
//          can translate it into a keyboard key or joystick button press sent
//          to the PC.  This lets you use any IR remote to send commands to the
//          PC, allowing access to additional control inputs without any extra
//          buttons on the cabinet.  To use this feature, assign the key to
//          send in the key type and key code bytes.
//
//        - Third, we can send a given IR command when a physical cabinet
//          button is pressed.  This lets you use cabinet buttons to send IR 
//          commands to other devices in your system.  For example, you could 
//          assign cabinet buttons to control the volume on a cab TV.  To use
//          this feature, assign an IR slot as a button function in the button
//          setup.
//
// 253 -> Extended input button setup.  This adds on to the information set by 
//        variable 254 below, accessing additional fields.  The "shifted" key
//        type and code fields assign a secondary meaning to the button that's
//        used when the local Shift button is being held down.  See variable 16 
//        above for more details on the Shift button.
//
//          byte 3 = Button number (1..MAX_BUTTONS)
//          byte 4 = shifted key type (same codes as "key type" in var 254)
//          byte 5 = shifted key code (same codes as "key code" in var 254)
//          byte 6 = shifted IR command (see "IR command" in var 254)
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
//          byte 8 = IR command to transmit when unshifted button is pressed.  This
//                   contains an IR slot number (1..MAX_IR_CODES), or 0 if no code
//                   is associated with the button.
//
// 255 -> LedWiz output port setup.  This sets up one output port; it can be repeated
//        for each port to be configured.  There are 128 possible slots for output ports, 
//        numbered 1 to 128.  The number of ports atcually active is determined by
//        the first DISABLED port (type 0).  For example, if ports 1-32 are set as GPIO
//        outputs and port 33 is disabled, we'll report to the host that we have 32 ports,
//        regardless of the settings for post 34 and higher.
//
//        The bytes of the message are:
//
//          byte 3 = LedWiz port number (1 to MAX_OUT_PORTS)
//
//          byte 4 = physical output type:
//
//                    0 = Disabled.  This output isn't used, and isn't visible to the
//                        LedWiz/DOF software on the host.  The FIRST disabled port
//                        determines the number of ports visible to the host - ALL ports
//                        after the first disabled port are also implicitly disabled.
//
//                    1 = GPIO PWM output: connected to GPIO pin specified in byte 5,
//                        operating in PWM mode.  Note that only a subset of KL25Z GPIO
//                        ports are PWM-capable.
//
//                    2 = GPIO Digital output: connected to GPIO pin specified in byte 5,
//                        operating in digital mode.  Digital ports can only be set ON
//                        or OFF, with no brightness/intensity control.  All pins can be
//                        used in this mode.
//
//                    3 = TLC5940 port: connected to TLC5940 output port number specified 
//                        in byte 5.  Ports are numbered sequentially starting from port 0
//                        for the first output (OUT0) on the first chip in the daisy chain.
//
//                    4 = 74HC595 port: connected to 74HC595 output port specified in byte 5.
//                        As with the TLC5940 outputs, ports are numbered sequentially from 0
//                        for the first output on the first chip in the daisy chain.
//
//                    5 = Virtual output: this output port exists for the purposes of the
//                        LedWiz/DOF software on the host, but isn't physically connected
//                        to any output device.  This can be used to create a virtual output
//                        for the DOF ZB Launch Ball signal, for example, or simply as a
//                        placeholder in the LedWiz port numbering.  The physical output ID 
//                        (byte 5) is ignored for this port type.
//
//                    6 = TLC59116 output: connected to the TLC59116 output port specified
//                        in byte 5.  The high four bits of this value give the chip's
//                        I2C address, specifically the A3 A2 A1 A0 bits configured in
//                        the hardware.  (A chip's I2C address is actually 7 bits, but
//                        the three high-order bits are fixed, so we don't bother including
//                        those in the byte 5 value).  The low four bits of this value
//                        give the output port number on the chip.  For example, 0x37
//                        specifies chip 3 (the one with A3 A2 A1 A0 wired as 0 0 1 1),
//                        output #7 on that chip.  Note that outputs are numbered from 0
//                        to 15 (0xF) on each chip.
//
//          byte 5 = physical output port, interpreted according to the value in byte 4
//
//          byte 6 = flags: a combination of these bit values:
//                    0x01 = active-high output (0V on output turns attached device ON)
//                    0x02 = noisemaker device: disable this output when "night mode" is engaged
//                    0x04 = apply gamma correction to this output (PWM outputs only)
//                    0x08 = "Flipper Logic" enabled for this output
//                    0x10 = "Chime Logic" enabled for this port
//
//          byte 7 = Flipper Logic OR Chime Logic parameters.  If flags bit 0x08 is set,
//                   this is the Flipper Logic settings.  If flags bit 0x10 is is set, 
//                   it's the Chime Logic settings.  The two are mutually exclusive.
//
//                   For flipper logic: (full power time << 4) | (hold power level)
//                   For chime logic:   (max on time << 4)     | (min on time)
//
//                   Flipper logic uses PWM to reduce the power level on the port after an
//                   initial timed interval at full power.  This is designed for pinball
//                   coils, which are designed to be energized only in short bursts.  In
//                   a pinball machine, most coils are used this way naturally: bumpers,
//                   slingshots, kickers, knockers, chimes, etc. are only fired in brief
//                   bursts.  Some coils are left on for long periods, though, particularly
//                   the flippers.  The Flipper Logic feature is designed to handle this
//                   in a way similar to how real pinball machines solve the same problem.
//                   When Flipper Logic is enabled, the software gives the output full
//                   power when initially turned on, but reduces the power to a lower
//                   level (via PWM) after a short time elapses.  The point is to reduce
//                   the power to a level low enough that the coil can safely dissipate
//                   the generated heat indefinitely, but still high enough to keep the 
//                   solenoid mechanically actuated.  This is possible because solenoids 
//                   generally need much less power to "hold" than to actuate initially.
//
//                   The high-order 4 bits of this byte give the initial full power time,
//                   using the following mapping for 0..15:  1ms, 2ms, 5ms, 10ms, 20ms, 40ms
//                   80ms, 100ms, 150ms, 200ms, 300ms, 400ms, 500ms, 600ms, 700ms, 800ms.
//                   
//                   Note that earlier versions prior to 3/2019 used a scale of (X+1)*50ms. 
//                   We changed to this pseudo-logarithmic scale for finer gradations at the
//                   low end of the time scale, for coils that need fast on/off cycling.
//
//                   The low-order 4 bits of the byte give the percentage power, in 6.66%
//                   increments: 0 = 0% (off), 1 = 6.66%, ..., 15 = 100%.
//
//                   A hold power of 0 provides a software equivalent of the timer-protected
//                   output logic of the Pinscape expansion boards used in the main board's
//                   replay knocker output and all of the chime board outputs.  This is
//                   suitable for devices that shouldn't ever fire for long periods to
//                   start with.  
//
//                   Non-zero hold powers are suitable for devices that do need to stay on 
//                   for long periods, such as flippers.  The "right" level will vary by
//                   device; you should experiment to find the lowest setting where the
//                   device stays mechanically actuated.  Once you find the level, you
//                   should confirm that the device won't overheat at that level by turning
//                   it on at the selected level and carefully monitoring it for heating.
//                   If the coil stays cool for a minute or two, it should be safe to assume
//                   that it's in thermal equilibrium, meaning it should be able to sustain
//                   the power level indefinitely.
//
//                   Note that this feature can be used with any port, but it's only fully
//                   functional with a PWM port.  A digital output port can only be set to
//                   0% or 100%, so the only meaningful reduced hold power is 0%.  This
//                   makes the feature a simple time limiter - basically a software version
//                   of the Chime Board from the expansion board set.
//
//                   Chime Logic encodes a minimum and maximum ON time for the port.  It
//                   doesn't use PWM; it simply forces the port on or off in specified
//                   durations.  The low 4 bits encode the minimum ON time, as an index
//                   into the table below.  The high 4 bits encode the maximum ON time,
//                   with the special case that 0 means "infinite".
//
//                   Chime logic time table: 0ms, 1ms, 2ms, 5ms, 10ms, 20ms, 40ms, 80ms,
//                   100ms, 200ms, 300ms, 400ms, 500ms, 600ms, 700ms, 800ms.
//
//
//        Note that the KL25Z's on-board LEDs can be used as LedWiz output ports, simply
//        by assigning the LED GPIO pins as output ports.  This is useful for testing a new 
//        installation without having to connect any external devices.  Assigning the 
//        on-board LEDs as output ports automatically overrides their normal status and
//        diagnostic display use, so be aware that the normal status flash pattern won't
//        appear when they're used this way.
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
