# Pinscape Controller for KL25Z

This is Version 2 of the Pinscape Controller, an I/O controller for virtual pinball machines.  (You can find the old version 1
software [here](https://developer.mbed.org/users/mjr/code/Pinscape_Controller/).  Pinscape is software for the KL25Z that turns
the board into a full-featured I/O controller for virtual pinball, with support for accelerometer-based nudging, a mechanical 
plunger, button inputs, and feedback device control.

In case you haven't heard of the idea before, a "virtual pinball machine" is basically a **video pinball simulator** that's built 
into a **real pinball machine body**.  A TV monitor goes in place of the pinball playfield, and a second TV goes in the backbox 
to show the backglass artwork.  Some cabs also include a third monitor to simulate the DMD (Dot Matrix Display) used for
scoring on 1990s machines, or even an original plasma DMD.   A computer (usually a Windows PC) is hidden inside the cabinet, 
running pinball emulation software that displays a life-sized playfield on the main TV.  The cabinet has all of the usual buttons, 
too, so it not only looks like the real thing, but plays like it too.  That's a picture of my own machine to the right. 
On the outside, it's built exactly like a real arcade pinball machine, with the same overall dimensions and all of the standard
pinball cabinet trim hardware.

It's possible to buy a pre-built virtual pinball machine, but it also makes a great DIY project.  If you have some 
basic wood-working skills and know your way around PCs, you can build one from scratch.  The computer part is just an
ordinary Windows PC, and all of the pinball emulation can be built out of free, open-source software.  In that spirit,
the Pinscape Controller is an open-source software/hardware project that offers a no-compromises, all-in-one control 
center for all of the unique input/output needs of a virtual pinball cabinet.  If you've been thinking about building 
one of these, but you're not sure how to connect a plunger, flipper buttons, lights, nudge sensor, and whatever else 
you can think of, this project might be just what you're looking for.

You can find much more information about DIY Pin Cab building in general in the
[Virtual Pinball Cabinet Forum](http://vpforums.org/index.php?showforum=29) on
[vpforums.org](http://vpforums.org).  Also visit my [Pinscape Resources](http://mjrnet.org/pinscape/) page
for more about this project and other virtual pinball projects I'm working on.


## Downloads

* [Pinscape Release Builds](http://www.mjrnet.org/pinscape/swversions.php): This page has download links for all of the
  Pinscape software.  To get started, install and run the Pinscape Config Tool on your Windows computer.  It will lead
  you through the steps for installing the Pinscape firmware on the KL25Z.

* [Config Tool Source Code](https://github.com/mjrgh/PinscapeConfigTool).  The complete C# source code for the config
  tool.  You don't need this to run the tool, but it's available if you want to customize anything or see how it works inside.


## Documentation

The [Pinscape Build Guide](http://mjrnet.org/pinscape/BuildGuideV2/preface.htm) covers Pinscape Controller setup as part
of its aspiration to be a complete guide to building a virtual pinball machine.  It covers topics from selecting parts,
building the wood cabinet, setting up a PC with virtual pinball software, setting up plunger and nudge sensors and
pushbutton inputs, setting up lighting and mechanical devices for feedback effects, and setting up specialized I/O devices
such as Pinscape to connect all of these things to the PC.

You can also refer to the old version 1 [Pinscape Hardware Build Guide](old-v1-archive-items/the_pinscape_controller_20170218.pdf), 
but that's out of date now, since it refers to the old version 1 software, which was rather different (especially when it comes
to configuration).


## System Requirements

The new Config Tool requires a fairly up-to-date Microsoft .NET installation.  If you use Windows Update to keep your system 
current, you should be fine.  **A modern version of Internet Explorer (IE) is required, even if you don't use it as your main browser**, 
because the Config Tool uses some system components that Microsoft packages into the IE install set.  I test with IE11, so that's known 
to work.  IE8 **doesn't** work.  IE9 and 10 are unknown at this point.

The Windows requirements are only for the config tool.  The firmware doesn't care about anything on the Windows side, so if you 
can make do without the config tool, you can use almost any Windows setup.

## Main Features

* **Plunger:** The Pinscape Controller started out as a "mechanical plunger" controller: a device for attaching a real pinball
 plunger to the video game software so that you could launch the ball the natural way.  This is still, of course, a central
 feature of the project.  The software supports several types of sensors: a high-resolution optical sensor (which works by
 essentially taking pictures of the plunger as it moves); a slide potentiometer (which determines the position via the
 changing electrical resistance in the pot); a quadrature sensor (which counts bars printed on a special guide rail that
 it moves along); and an IR distance sensor (which determines the position by sending pulses of light at the plunger
 and measuring the round-trip travel time).  The Build Guide explains how to set up each type of sensor.

* **Nudging:** The KL25Z (the little microcontroller that the software runs on) has a built-in accelerometer.  The
  Pinscape software uses it to sense when you nudge the cabinet, and feeds the acceleration data to the pinball software
  on the PC.  This turns physical nudges into virtual English on the ball.  The accelerometer is quite sensitive and accurate,
   so we can measure the difference between little bumps and hard shoves, and everything in between.  The result is natural
  and immersive.

* **Buttons:**  You can wire real pinball buttons to the KL25Z, and the software will translate the buttons into PC input.
  You have the option to map each button to a keyboard key or joystick button.  You can wire up your flipper buttons, Magna
  Save buttons, Start button, coin slots, operator buttons, and whatever else you need.

* **Feedback devices:**  You can also attach "feedback devices" to the KL25Z.  Feedback devices are things that create tactile,
  sound, and lighting effects in sync with the game action.  The most popular PC pinball emulators know how to address a wide
  variety of these devices, and know how to match them to on-screen action in each virtual table.  You just need an I/O controller
   that translates commands from the PC into electrical signals that turn the devices on and off.  The Pinscape Controller can do
   that for you.

## Expansion Boards

There are two main ways to run the Pinscape Controller: standalone, or using the "expansion boards".  

In the basic **standalone** setup, you just need the KL25Z, plus whatever buttons, sensors, and feedback devices you want to attach
to it.  This mode lets you take advantage of everything the software can do, but for some features, you'll have to build some ad hoc
external circuitry to interface external devices with the KL25Z.  The Build Guide has detailed plans for exactly what you need to build.

The other option is the Pinscape **Expansion Boards**.  The expansion boards are a companion project, which is also totally
free and open-source, that provides Printed Circuit Board (PCB) layouts that are designed specifically to work with the Pinscape
software.  The PCB designs are in the widely used EAGLE format, which many PCB manufacturers can turn directly into physical boards 
for you.  The expansion boards organize all of the external connections more neatly than on the standalone KL25Z, and they add all 
of the interface circuitry needed for all of the advanced software functions.  The big thing they bring to the table is lots of 
high-power outputs.  The boards provide a modular system that lets you add boards to add more outputs.  If you opt for the basic 
core setup, you'll have enough outputs for all of the toys in a really well-equipped cabinet.  If your ambitions go beyond merely
well-equipped and run to the ridiculously extravagant, just add an extra board or two.  The modular design also means that you can
add to the system over time.

[Expansion Board project page](http://mjrnet.org/pinscape/expansion-board.html)


## Updating from a v1 Pinscape setup

If you have a Pinscape V1 setup already installed, you should be able to switch to the new version pretty seamlessly.  
There are just a couple of things to be aware of.

First, the "configuration" procedure is **completely different** in the new version.  Way better and way easier, but 
it's not what you're used to from V1.  In V1, you had to edit the project source code and compile your own custom 
version of the program.  No more!  With V2, you simply install the **standard**, **pre-compiled** .bin file, and 
select options using the Pinscape Config Tool on Windows.

Second, if you're using the TSL1410R optical sensor for your plunger, there's a chance you'll need to boost your light 
source's brightness a little bit.  The "shutter speed" is faster in this version, which means that it doesn't spend as
much time collecting light per frame as before.  The software actually does "auto exposure" adaptation on every frame, 
so the increased shutter speed really shouldn't bother it, but it does require a certain minimum level of contrast, 
which requires a certain minimal level of lighting.  Check the plunger viewer in the setup tool if you have any problems;
if the image looks totally dark, try increasing the light level to see if that helps.

## New Features in Pinscape Controller v2

V2 has numerous new features.  Here are some of the highlights...

Dynamic configuration: as explained above, configuration is now handled through the Config Tool on Windows. 
It's no longer necessary to edit the source code or compile your own modified binary. 

Improved plunger sensing: the software now reads the TSL1410R optical sensor about 15x faster than it did before.
This allows reading the sensor at full resolution (400dpi), about 400 times per second.  The faster frame rate makes 
a big difference in how accurately we can read the plunger position during the fast motion of a release, which allows for
more precise position sensing and faster response.  The differences aren't dramatic, since the sensing was already pretty
good even with the slower V1 scan rate, but you might notice a little better precision in tricky skill shots.

Keyboard keys: button inputs can now be mapped to keyboard keys.  The joystick button option is still available as well, of course.
Keyboard keys have the advantage of being closer to universal for PC pinball software: some pinball software can be set up to take 
joystick input, but nearly all PC pinball emulators can take keyboard input, and nearly all of them use the same key mappings.

Local shift button: one physical button can be designed as the local shift button.  This works like a Shift button on a keyboard,
but with cabinet buttons.  It allows each physical button on the cabinet to have two PC keys assigned, one normal and one shifted. 
Hold down the local shift button, then press another key, and the other key's shifted key mapping is sent to the PC.  The shift button
can have a regular key mapping of its own as well, so it can do double duty.  The shift feature lets you access more functions without
cluttering your cabinet with extra buttons.  It's especially nice for less frequently used functions like adjusting the volume or 
activating night mode.

Night mode: the output controller has a new "night mode" option, which lets you turn off all of your noisy devices with a single button, 
switch, or PC command.  You can designate individual ports as noisy or not.  Night mode only disables the noisemakers, so you still get
the benefit of your flashers, button lights, and other quiet devices.  This lets you play late into the night without disturbing your
housemates or neighbors.

Gamma correction: you can designate individual output ports for gamma correction.  This adjusts the intensity level of an output to
make it match the way the human eye perceives brightness, so that fades and color mixes look more natural in lighting devices.  You
can apply this to individual ports, so that it only affects ports that actually have lights of some kind attached.

IR Remote Control: the controller software can transmit and/or receive IR remote control commands if you attach appropriate parts
(an IR LED to send, an IR sensor chip to receive).  This can be used to turn on your TV(s) when the system powers on, if they don't 
turn on automatically, and for any other functions you can think of requiring IR send/receive capabilities.  You can assign IR commands
to cabinet buttons, so that pressing a button on your cabinet sends a remote control command from the attached IR LED, and you can have 
the controller generate virtual key presses on your PC in response to received IR commands.  If you have the IR sensor attached, the system 
can use it to learn commands from your existing remotes.

Yet more USB fixes:  I've been gradually finding and fixing USB bugs in the mbed library for months now.  This version has all of the 
fixes of the last couple of releases, of course, plus some new ones.  It also has a new "last resort" feature, since there always seems
to be "just one more" USB bug.  The last resort is that you can tell the device to automatically reboot itself if it loses the USB
connection and can't restore it within a given time limit.


## More Downloads

* [Custom VP builds](https://www.dropbox.com/sh/0gtnrck3yr9w9oa/AAAZpK2Nhw2HtOshcJWkIe6ha?dl=0):  I created modified versions of Visual
   Pinball 9.9 and Physmod5 that you might want to use in combination with this controller.  The modified versions have special handling
   for plunger calibration specific to the Pinscape Controller, as well as some enhancements to the nudge physics.  If you're not using
  the plunger, you might still want it for the nudge improvements.  The modified version also works with any other input controller, so
  you can get the enhanced nudging effects even if you're using a different plunger/nudge kit.  The big change in the modified versions
  is a "filter" for accelerometer input that's designed to make the response to cabinet nudges more realistic.  It also makes the response
  more subdued than in the standard VP, so it's not to everyone's taste.  The downloads include both the updated executables and the source
   code changes, in case you want to merge the changes into your own custom version(s).  **Note! These features are now standard in the official VP releases,**
   so you don't need my custom builds if you're using 9.9.1 or later and/or VP 10.  I don't think there's any reason to use my versions
  instead of the latest official ones, and in fact I'd encourage you to use the official releases since they're more up to date, but I'm
   leaving my builds available just in case.  In the official versions, look for the checkbox "Enable Nudge Filter" in the Keys preferences
   dialog.  My custom versions don't include that checkbox; they just enable the filter unconditionally.

* [Output circuit shopping list](https://www.mouser.com/ProjectManager/ProjectDetail.aspx?AccessID=6bc801c8d6):  This is a saved shopping
  cart at mouser.com with the parts needed to build one copy of the high-power output circuit for the LedWiz emulator feature, for use with
  the standalone KL25Z (that is, without the expansion boards).  The quantities in the cart are for one output channel, so if you want N outputs,
  simply multiply the quantities by the N, with one exception: you only need **one** ULN2803 transistor array chip for each **eight** output
  circuits.  If you're using the expansion boards, you won't need any of this, since the boards provide their own high-power outputs.

* [Cary Owens' optical sensor housing](http://mjrnet.org/pinscape/plungerBracket.html): A 3D-printable design for a housing/mounting bracket for
   the optical plunger sensor, designed by Cary Owens.  This makes it easy to mount the sensor.

* [Lemming77's potentiometer mounting bracket](misc/Lemming77/plunger-skp.zip):
  Sketchup designs for 3D-printable parts for mounting a slide potentiometer as the plunger sensor.  These were designed for a
  particular slide potentiometer that used to be available from an Aliexpress.com seller but is no longer listed.  You can probably
  use this design as a starting point for other similar devices; just check the dimensions before committing the design to plastic.

## Copyright and License

The Pinscape firmware is copyright 2014, 2021 by Michael J Roberts.  It's released under an MIT open-source license.  See [License](License.txt).


## Warning to VirtuaPin Kit Owners

This software isn't designed as a replacement for the VirtuaPin plunger kit's firmware.  If you bought the VirtuaPin kit,
I recommend that you **don't**  install this software.  The KL25Z can only run one firmware program at a time, so if you install
the Pinscape firmware on your KL25Z, it will **replace** and **erase** your existing VirtuaPin proprietary firmware.  If you do this, 
the only way to restore your VirtuaPin firmware is to physically ship the KL25Z back to VirtuaPin and ask them to re-flash it.  They don't 
allow you to do this at home, and they don't even allow you to back up your firmware, since they want to protect their proprietary software 
from copying.  For all of these reasons, if you want to run the Pinscape software, I strongly recommend that you buy a "blank" retail KL25Z 
to use with Pinscape.  They only cost about $15 and are available at several online retailers, including Amazon, Mouser, and eBay.  
The blank retail boards don't come with any proprietary firmware pre-installed, so installing Pinscape won't delete anything that 
you paid extra for.

With those warnings in mind, if you're absolutely sure that you don't mind permanently erasing your VirtuaPin firmware, it is at least 
possible to use Pinscape as a replacement for the VirtuaPin firmware.  Pinscape uses the same button wiring conventions as the VirtuaPin 
setup, so you can keep your buttons (although you'll have to update the GPIO pin mappings in the Config Tool to match your physical wiring). 
As of the June, 2021 firmware, the Vishay VCNL4010 plunger sensor that comes with the VirtuaPin v3 plunger kit is supported, so you can also 
keep your plunger, if you have that chip.  (You should check to be sure that's the sensor chip you have before committing to this route, 
if keeping the plunger sensor is important to you.  The older VirtuaPin plunger kits came with different IR sensors that the Pinscape 
software doesn't handle.)

