// UPDATES
//
// This is a record of new features and changes in recent versions.
//

// January 2016
//
// Dynamic configuration:  all configuration options are now handled dynamically,
// through the Windows config tool.  In earlier versions, most configuration options
// were set through compile-time constants, which made it necessary for everyone
// who wanted to customize anything to create a private branched version of the
// source repository, edit the source code, and compile their own binary.  This
// was cumbersome, and required way too much technical knowledge to be worth the
// trouble to a lot of people.  The goal of the new approach is that everyone can
// use the same standard binary build, and set options from the Windows tool.
//
// TSL1410R and 1412R high-speed scanning: the software now takes advantage
// of the KL25Z's fastest hardware features to scan the optical sensors at much
// higher speed than in the past.  The software can now read these sensors at
// full resolution in about 2.5ms, which means a frame rate of about 400 frames
// per second.  That's fast enough that we can capture images of the plunger
// moving at full release speed without any significant motion blur, and fast
// enough to capture the position throughout a release motion without any
// aliasing from the bounce-back at the end of the travel.  In past versions,
// the frame rate wasn't high enough to avoid either blur or aliasing, so it
// was necessary to use heuristics to guess when a release was happening.  The
// heuristics worked pretty well, but at the cost of some slight lag while we
// waited to see what was happening.  The new higher rate allows for essentially
// zero lag, as well as more precise position sensing.
//
// Keyboard mappings for buttons: button inputs can now be mapped to keyboard
// keys.  Joystick buttons are of course also still supported.  Some software on
// the PC side is easier to configure for keyboard input than for joystick
// input, so many users might prefer to map some or all buttons to keys.  If
// you map any buttons to keyboard input, the controller device will have
// two entries in the Windows Device Manager list, one as a joystick and
// the other as a keyboard.  This is automatic; the keyboard interface will
// appear automatically if you have any keyboard keys mapped, otherwise only
// the joystick interface will appear.
//
// "Pulse" buttons: you can now designate individual button inputs as pulse
// mode buttons.  When a button is configured in pulse mode, the software
// translates each ON/OFF or OFF/ON transition in the physical switch to a
// short virtual key press.  This is especially designed to make it easier
// to wire a coin door switch, but could be used for other purposes as well.
// For the coin door, the VPinMAME software uses the End key to *toggle* the
// open/closed state of the door in the simulation, but it's much easier
// to wire a physical on/off switch to the door instead.  Pulse mode bridges
// this gap by translating the on/off switch state to key presses.  When
// you open the door, the switch will go from OFF to ON, so the controller
// will send one short key press, causing VPinMAME to toggle the simulated
// door to OPEN.  When you close the door, the switch will go from ON to
// OFF, which will make the controller send another short key press, which
// in turn will make VPinMAME toggle the simulated door state to CLOSED.
// There are other ways to solve this problem (VP cab builders have come
// up with various physical devices and electronic timer circuits to deal
// with it), but the software approach implemented here is a lot simpler
// to set up and is very reliable.
//
// Night mode: you can now put the device in "night mode" by configuring a 
// physical button input to activate the mode, or by sending a command from
// the PC config tool software.  When night mode is activated, outputs that
// you designate as "noisemaker" devices are disabled.  You can designate
// any outputs as noisy or not.  This feature is designed to let you use your
// virtual pinball machine during quiet hours (e.g., late at night) without
// disturbing housemates or neighbors with noise from flippers, knockers,
// shaker motors, and so on.  You can designate outputs individually as
// noisy, so you can still enjoy the rest of your feedback features during
// night play (e.g., flashers and other lighting effects).
//
// Gamma correction: each output can now optionally have gamma correction 
// applied.  This can be set in the configuration individually for each 
// output attached to an LED or lamp.  Gamma correction translates the
// computer's idea of linear brightness to the human eye's logarithmic
// brightness curve, which makes make the perceived brightness level of a 
// lamp more linear.  This can greatly improve the appearance of fading 
// effects and the fidelity of color mixing in RGB devices.  Without gamma
// correction, fades tend to saturate on the bright end of the scale, and
// mixed colors tend to look washed out.
//
// USB fixes: the low-level USB device code had some serious bugs that only
// very occasionally manifested in past versions, but became much more
// frequently triggered due to other changes in this release (particularly
// the USB keyboard input feature).  These should now be fixed, so the USB
// connection should now be very reliable.
//
