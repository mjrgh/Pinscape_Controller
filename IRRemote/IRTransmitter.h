// IR Remote Transmitter
//
// This class lets you control an IR emitter LED connected to a GPIO port 
// to transmit remote control codes using numerous standard and proprietary 
// protocols.  You can use this to send remote codes to any device with
// a typical IR remote, such as A/V equipment, home automation devices, etc.
// You can also use this with the companion IR Receiver class running on
// a separate KL25Z to send IR commands to the other device.
//
// We do all of our transmissions with specific protocols rather than raw
// IR signals.  Every remote control has its own way of representing a
// string of data bits as a series of timed IR flashes.  The exact mapping
// between data bits and IR flashes is the protocol.  There are some quasi
// industry standard protocols, where several companies use the same format
// for their codes, but there are many proprietary protocols as well.  We
// have handlers for the most widely used protocols:  NEC, Sony, Philips RC5
// and RC6, Pioneer, Panasonic, and several others.  If your device isn't
// covered yet, it could probably be added, since we've tried to design
// the system to make it easy to add new protocols.
//
// When you transmit a code, you specify it in terms of the protocol to use 
// and the "code" value to send.  A "code" is just the data value for a
// particular key on a particular remote control, usually expressed as a 
// hex number.  There are published tables of codes for many remotes, but
// unfortunately they're not very consistent in how they represent the hex
// code values, so you'll often see the same key represented with different
// hex codes in different published tables.  We of course have our own way
// of mapping the hex codes; we've tried to use the format that the original
// manufacturer uses in their tales, if they publish them at all, but these
// may or may not be consistent with what you find in any tables you consult.
// So your best bet for finding the right codes to use here is usually to 
// "learn" the codes using our companion class IRReceiver.  That class has a 
// protocol decoder for each protocol transmitter we can use here, so if you
// set that up and point a remote at it, it will tell you the exact code we
// use for the key.
//
// The transmitter class provides a "virtual remote control" interface.
// This gives you an imaginary remote control keypad, with a set of
// virtual buttons programmed for individual remote control commands.
// You specify the protocol and command code for each virtual button.
// You can use different protocols for different buttons.
//
//
// How to use the software
//
// First, create an instance of IRTransmitter, telling it which pin the
// IR emitter is connected to (see below for wiring instructions) and how
// many virtual remote control keys you want.  The pin must be PWM capable.
//
//    IRTransmitter *tx = new IRTransmitter(PTC9, 32);
//
// Next, program the virtual remote keys.  For each key, set the IR protocol
// to use (an IRPRO_xxx code from IRProtocolID.h), the "ditto" mode (more on
// this below), and the hex code for the command.
//
//    // program virtual button #0 with Sony 20-bit code 0x123, no dittos
//    tx->programButton(0, IRPRO_SONY20, false, 0x123);
//
// Now you're set up to transmit.  In your main loop, decide when it's time
// to transmit a button, such as by monitoring a physical pushbutton via a
// GPIO DigitalIn pin.  When you want to transmit a code, just tell the
// transmitter that your virtual button is pressed, by calling pushButton()
// with the virtual button ID (corresponding to a virtual button ID you
// previously programmed wtih programButton()) and a status of 'true',
// meaning that the button is pressed.
//
//    tx->pushButton(0, true);  // push virtual button #0
//
// This starts the transmission and returns immediately.  The transmission
// proceeds in the background (via timer interrupts), so your main loop can
// go about its other business without waiting for the transmission to
// finish.  Most remote codes take 50ms to 100ms to transmit, and you don't
// usually want to stall an MCU app for that long.
//
// If a prior transmission is still in progress when you call pushButton(), 
// the new transmission doesn't interrupt the previous one.  Every code is
// sent as a complete unit to ensure data integrity, so the old one has to
// finish before the new one starts.  Some protocols have minimum repeat
// counts, and the transmitter takes this into account as well.  For example,
// the Sony protocols require each command to be sent at least three times,
// even if the button is only tapped for a brief instant.  So if you send
// a Sony code, a new command won't start transmitting until the last command
// has been sent completely, not just once, but at least three times.
//
// Once the transmitter starts sending the code for a new button, it keeps
// sending the same code on auto-repeat until you either un-press the
// virtual button or press a new virtual button.  Handling auto-repeat
// in the transmitter like this has an important benefit, besides just making
// the API simpler: it allows the transmitter to use the proper coding for
// the repeats according to the rules of the protocol.  Some protocols use
// a different format for the first code of a key press and auto-repeats
// of the same key.  Some protocols also have other repetition features,
// such as "toggle bits" or sequence counters.  The protocol handlers use
// the appropriate handling for their protocols, so you only have to think
// in terms of when the virtual buttons are pressed and un-pressed, without
// worrying about whether a toggle bit or a "ditto" code or a sequence
// counter is needed.
//
// When the button is no longer pressed, call pushButton() again with a
// status of 'false':
//
//    tx->pushButton(0, false);
//
// Multiple button presses use simple PC keyboard-like semantics.  At any
// given time, there can be only one pressed button.  When you call 
// pushButton(N, true), N becomes the pressed button, which means that the
// previous pressed button (if any) is forgotten.  As mentioned above, this
// doesn't cancel the previous transmission if it's still in progress.  The
// transmitter continues with the last code until it's finished.  When it
// finishes with a code, the transmitter looks to see if the same button is
// still pressed.  If so, it starts a new transmission for the same button,
// using the appropriate repeat code.  If a new button is pressed, the
// transmitter starts transmitting the new button's code.  If no button is
// pressed, the transmitter stops sending and becomes idle until you press
// another button.
//
// Note that button presses aren't queued.  Suppose you press button #0
// (while no other code is being sent): this starts transmitting the code
// for button #0 and returns.  Now suppose that a very short time later, 
// while that first send is still in progress, you briefly press and release
// button #1.  Button #1 will never be sent in this case.  When you press
// button #1, the transmitter is still sending the first code, so all it
// does at this point is mark button #1 as the currently pressed button,
// replacing button #0.  But as explained above, this doesn't cancel the
// button #0 code transmission in progress.  That continues until the
// complete code has been sent.  At that point, the transmitter looks to
// see which button is pressed, and discovers that NO button is pressed:
// you already told it button #1 was released.  So the transmitter simply
// stops sending and becomes idle.
//
//
// How to determine command codes and the "ditto" mode
//
// Our command codes are expressed as 64-bit integers.  The code numbers
// are in essence the data bits transmitted in the IR signal, but the mapping
// between the IR data bits and the 64-bit code value is different for each
// protocol.  We've tried to make our codes match the numbers shown in the
// tables published by the respective manufacturers for any given remote,
// but you might also find third-party tables that have completely different
// mappings.  The easiest thing to do, really, is to ignore all of that and
// just treat the codes as arbitrary, opaque identifiers, and identify the
// codes for the remote you want to use by "learning" them.  That is, set up
// a receiver with our companion class IRReceiver, point your remote at it,
// and see what IRReceiver reports as the decoded value for each button. 
// Simply use the same code value for each button when sending.
//
// The "ditto" flag is ignored for most protocols, but it's important for a
// few, such as the various NEC protocols.  This tells the sender whether to
// use the protocol's special repeat code for auto-repeats (true), or to send
// send the same key code repeatedly (false).  The concept of dittos only
// applies to a few protocols; most protocols just do the obvious thing and
// send the same code repeatedly when you hold down a key.  But the NEC
// protocols and a few others have special coding for repeated keys.  It's 
// important to use the special coding for devices that expect it, because 
// it lets them distinguish auto-repeat from multiple key presses, which
// can affect how they respond to certain commands.  The tricky part is that 
// manufacturers aren't always consistent about using dittos even when it's
// a standard part of the protocol they're using, so you have to determine
// whether or not to use it on a per-device basis.  The easiest way to do
// this is just like learning codes: set up a receiever with IRReceiver and
// see what it reports.  But this time, you're interested in what happens
// when you hold down a key.  You'll always get one ordinary report first,
// but check what happens for the repeats.  If IRReceiver reports the same 
// code repeatedly, set dittos = false when sending those codes.  If the
// repeats have the "ditto bit" set, though, set dittos = true when sending.
//
//
// How to wire an IR emitter
//
// Any IR LED should work as the emitter.  I used a Vishay TSAL6400 for my
// reference/testing implementation.  The TSAL6400 is quite bright, so it
// should send signals well across fairly large distances.
//
// WARNING!  DON'T connect the LED directly to the GPIO pin.  KL25Z GPIO
// pins have very low current limits - a typical IR emitter LED draws
// enough current to damage or destroy the KL25Z.  You'll need to build a
// simple transistor circuit to interface with the LED.  You'll need a
// common small signal NPN transistor (such as a 2222 or 2N4401), a 2.2K
// resistor, the IR LED, of course, and a current-limiting resistor for
// the LED.  Choose the current-limiting resistor by plugging your LED's
// specs into an LED resistor calculator, using a 5V supply voltage.  Now
// connect the GPIO pin to the current-limiting resistor, connect the
// resistor to the LED anode (+), connect the LED cathode (-) to the NPN
// collector, connect the NPN emitter to ground, connect the NPN base to
// the 2.2K resistor, and connect the 2.2K resistor to the GPIO pin.
// It's simple enough for a schematic rendered in ASCII art:
//
//       +5V   (from the KL25Z +5V pin, or directly from
//        |     the KL25Z's power supply)
//        <
//        >  R1 - use an LED resistor calculator to choose
//        <       the resistor size based on your selected 
//        |       LED's forward current & voltage and 5V source
//       ---  +
//       \ /  LED - Infrared emitter (e.g., Vishay TSAL6400)
//       ---  -
//        |
//        |
//         \|     2.2K
//          |-----/\/\/\---> to this GPIO pin
//         /|
//        v
//        |
//      -----
//       ---   Ground (KL25Z GND pin, or ground on the
//        -            KL25Z's power supply)
//
// If you want to be able to see the transmitter in action, you can connect
// another LED (a blue one, say) and its own current-limiting resistor in
// parallel with the R1 + IR LED circuit.  Let's call the blue LED's
// resistor R2.  Connect R2 to +5V, connect the other end of R2 to the
// blue LED (+), and connect the blue LED (-) to the NPN collector.  This
// will make the blue LED flash in sync with the IR LED.  IR remote control
// codes are slow enough that you'll be able to see the blue LED come on
// and flicker during each transmission, although the "bits" are too fast
// to see individually with the naked eye.  The detector shouldn't be 
// bothered by the extra light since these sensors have optical filters 
// that block most of the incoming light outside of the IR band the sensor 
// is looking for.

#ifndef _IRTRANSMITTER_H_
#define _IRTRANSMITTER_H_

#include <mbed.h>

#include "NewPwm.h"
#include "IRRemote.h"
#include "IRCommand.h"
#include "IRProtocols.h"


// IR Remote Transmitter
class IRTransmitter
{
public:
    // Construct.  
    //
    // 'pin' is the GPIO pin controlling the IR LED.  The pin must be 
    // PWM-capable.  (Note also that each PWM channel on the KL25Z is 
    // shared among multiple pins, so be sure you're using a pin connected 
    // to a channel that isn't already used elsewhere in your application.)
    // Don't connect the LED directly to this pin; see the circuit diagram
    // at the top of the file for details of how to connect it through a
    // transistor to safely boost the current to LED levels.
    //
    // 'nButtons' is the number of virtual button slots to allocate.  Each
    // slot represents a virtual remote control button that can be programmed
    // with a remote code to transmit.  Allocate as many slots as you need
    // for unique commands or buttons.  Note that the caller is responsible
    // for deciding when a button is pressed; if you want to tie these to
    // physical buttons, you'll need to create your own DigitalIn objects
    // for the pins, monitor them, and call pushButton() to press and
    // release virtual buttons when the physical button states change.
    IRTransmitter(PinName pin, int nButtons) : ledPin(pin)
    {
        // make sure the protocol singletons are allocated
        IRProtocol::allocProtocols();
        
        // no command is active
        curBtnId = -1;
        
        // allocate the command list
        buttons = new ButtonCmd[nButtons];
        
        // the transmitter "thread" isn't yet running
        txRunning = false;
        txBtnId = -1;
        txProtocol = 0;
    }
    
    ~IRTransmitter()
    {
        delete[] buttons;
    }
    
    // Program the command code for a virtual button
    void programButton(int buttonId, int protocolId, bool dittos, uint64_t cmdCode)
    {
        ButtonCmd &btn = buttons[buttonId];
        btn.pro = protocolId;
        btn.dittos = dittos;
        btn.cmd = cmdCode;
    }
    
    // Push a virtual button.
    // 
    // When this is called, we'll start transmitting the command code
    // associated with the button immediately if no other transmission
    // is already in progress.  On the other hand, if a transmission of
    // a prior command code is already in progress, the previous command
    // isn't interrupted; we always send whole commands, and never
    // interrupt a command in progress.  Instead, the new button is
    // set as pending.  As soon as the prior transmission finishes,
    // the pending button becomes the current button and we start
    // transmitting its code - but only if the button is still pressed
    // when the previous code finishes.  This means that if you both 
    // press and release a button during the time that another 
    // transmission is in progress, the new button will never be 
    // transmitted.  We operate this way to keep things simple and
    // consistent when it comes to more than just one pending button.
    // This way we don't have to consider queues of pending buttons
    // or create mechanisms for canceling pending commands.
    //
    // If the button is still down when its first transmission ends,
    // and no other button has been pressed in the meantime, the button
    // will auto-repeat.  This continues as long as the button is still
    // pressed and no other button has been pressed.
    // 
    // Only one code can be transmitted at a time, obviously.  The
    // semantics for multiple simultaneous button presses are like those
    // of a PC keyboard.  Suppose you press button A, then a while later,
    // while A is still down, you press B.  Then a while later still,
    // you press C, continuing to hold both A and B down.  We transmit
    // A repeatedly until you press B, at which point we finish sending
    // the current repeat of A (we never interrupt a code in the middle:
    // once started, a code is always finished whole) and start sending
    // B.  B continues to repeat until you press C, at which point we
    // finish the last repetition of B and start sending C.  Once A or
    // B have been superseded, it makes no difference whether you continue
    // to hold them down or release them.  They'll never start repeating
    // again, even if you then release C while A and B are still down.
    void pushButton(int id, bool on)
    {
        if (on)
        {
            // make this the current command
            curBtnId = id;

            // start the transmitter
            txStart();
        }
        else
        {
            // if this is the current command, cancel it
            if (id == curBtnId)
                curBtnId = -1;
        }
    }
    
    // Is a transmission in progress?
    bool isSending() const { return txRunning; }
    

protected:
    // Start the transmitter "thread", if it's not already running.  The
    // thread is actually just a series of timer interrupts; each interrupt
    // sets the next interrupt at an appropriate interval, so the effect is
    // like a thread.
    void txStart()
    {
        if (!txRunning)
        {
            // The thread isn't running.  Note that this means that there's
            // no possibility that txRunning will change out from under us
            // asynchronously, since there's no pending interrupt handler
            // to change it.  Mark the thread as running.
            txRunning = true;
            
            // Directly invoke the thread handler for the first call.  It
            // will normally run in interrupt context, but since there's
            // no pending interrupt yet that would re-enter it, we can
            // launch it first in application context.  If there's work
            // pending, it'll kick off the transmission and schedule the
            // next timer interrupt to continue the thread.
            txThread();
        }
    }
    
    // Transmitter "thread" main.  This handles the timer interrupt for each
    // event in a transmission.
    void txThread()
    {
        // if we're working on a command, process the next step
        if (txProtocol != 0)
        {
            // Determine if the virtual button for the current transmission
            // is still pressed.  It's still pressed if we have a valid 
            // transmitting button ID, and the current pressed button is the 
            // same as the transmitting button.
            txState.pressed = (txBtnId != -1 && txBtnId == curBtnId);

            // Perform the next step via the protocol handler.  The handler
            // returns a positive time value for the next timeout if it still
            // has more work to do.
            int t = txProtocol->txStep(&txState);
            
            // check if the transmission is done
            if (t > 0)
            {
                // The handler returned a positive time value, so it has
                // more work to do.  That means we're done here - just set
                // the next timeout and exit the interrupt handler.
                txTimeout.attach_us(this, &IRTransmitter::txThread, t);
                return;
            }
            else
            {
                // The transmission is done.  Clear the send data.
                txBtnId = -1;
                txProtocol = 0;
            }
        }
        
        // If we made it here, the transmitter is now idle.  Check to
        // see if we have a new virtual button press.
        if (curBtnId != -1)
        {
            // load the command
            txBtnId = curBtnId;
            txCmd = buttons[curBtnId];
            txProtocol = IRProtocol::senderForId(txCmd.pro);
            
            // If we found a protocol handler, start the transmission
            if (txProtocol != 0)
            {
                // fill in the transmission state object with the new command
                // details
                txState.cmdCode = txCmd.cmd;
                txState.protocolId = txCmd.pro;
                txState.dittos = txCmd.dittos;
                txState.pin = &ledPin;
                txState.pressed = true;
                
                // reset the transmission step counters
                txState.step = 0;
                txState.bit = 0;
                txState.bitstep = 0;
                txState.rep = 0;
                
                // this is a new transmission, so toggle the toggle bit
                txState.toggle ^= 1;
                
                // Turn off the IR and set the PWM frequency of the IR LED to
                // the carrier frequency for the chosen protocol
                ledPin.write(0);
                ledPin.getUnit()->period(txProtocol->pwmPeriod(&txState));

                // start the transmission timer
                txState.txTime.reset();
                txState.txTime.start();
                
                // initiate the transmission
                int t = txProtocol->txStart(&txState);
                
                // set the timer for the next step of the transmission, then
                // we're done
                txTimeout.attach_us(this, &IRTransmitter::txThread, t);
                return;
            }
        }
        
        // If we made it here, there's no transmission in progress,
        // so the thread is no longer running.
        txRunning = false;
    }

    // LED output pin controlling the IR LED.  The pin must be PWM-capable.
    // WARNING!  Don't connect the IR LED directly to the pin.  See wiring
    // diagram at the top of the file.
    NewPwmOut ledPin;
    
    // Virtual button slots.  Each slot represents a virtual remote control
    // button, containing a preprogrammed IR command code to send when the 
    // button is pressed.  Program a button by calling programButton().
    // Press a button by calling pushButton().
    struct ButtonCmd
    {
        uint64_t cmd;           // command code
        uint8_t pro;            // protocol ID (IRPRO_xxx)
        uint8_t dittos : 1;     // use "ditto" codes for auto-repeat
    } __attribute__ ((packed));
    ButtonCmd *buttons;
    
    // Current active virtual button ID.   This is managed in application
    // context and read in interrupt context.  This represents the currently 
    // pushed button.
    int curBtnId;
    
    // Is the transmitter "thread" running?  This is true when a timer is
    // pending, false if not.  The timer interrupt handler clears this
    // before exiting on its last run of a transmission.
    //
    // Synchronization: if txRunning is false, no timer interrupt is either
    // running or pending, so there's no possibility that anyone else will
    // change it, so it's safe for the application to test and set it.  If
    // txRunning is true, only interrupt context can change it, so application
    // context can only read it.
    volatile bool txRunning;
    
    // Transmitter thread timeout
    Timeout txTimeout;
    
    // Command ID being transmitted in the background "thread".  The thread
    // loads this from curBtnID whenever it's out of other work to do.
    int txBtnId;
    
    // Protocol for the current transmission
    IRProtocol *txProtocol;
    
    // Command value we're currently transmitting
    ButtonCmd txCmd;
    
    // Protocol state.  This is for use by the individual protocol
    // classes to keep track of their state while the transmission
    // proceeds.
    IRTXState txState;
};

#endif
