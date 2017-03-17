// IR Receiver
//
#include "IRReceiver.h"
#include "IRTransmitter.h"
#include "IRProtocols.h"

// utility macro
#define countof(arr) (sizeof(arr)/sizeof((arr)[0]))

// Constructor
IRReceiver::IRReceiver(PinName rxpin, size_t rawBufCount) : 
    pin(rxpin),
    rawbuf(rawBufCount)
{
    // the TSOP384xx has an internal pull-up resistor, so we don't
    // need one of our own
    pin.mode(PullNone);
    
    // make sure the protocol singletons are allocated
    IRProtocol::allocProtocols();
    
    // there's no transmitter connected yet
    transmitter = 0;
}

// Destructor
IRReceiver::~IRReceiver() {
}

// Enable reception
void IRReceiver::enable()
{
    // start the pulse timers
    startPulse(pin.read() ? 0 : 1);
    
    // set interrupt handlers for edges on the input pin
    pin.fall(this, &IRReceiver::fall);
    pin.rise(this, &IRReceiver::rise);
}

// Disable reception
void IRReceiver::disable()
{
     // Shut down all of our asynchronous handlers: remove the pin level
     // interrupts, stop the pulse timer, and cancel the maximum pulse 
     // length timeout.
     pin.fall(0);
     pin.rise(0);
     pulseTimer.stop();
     timeout.detach();
}

// Start a new pulse of the given type.
void IRReceiver::startPulse(bool newPulseState)
{
    // set the new state
    pulseState = newPulseState;
    
    // reset the pulse timer
    pulseTimer.reset();
    pulseTimer.start();
    pulseAtMax = false;
    
    // cancel any prior pulse timeout
    timeout.detach();
    
    // Set a new pulse timeout for the maximum pulse length
    timeout.attach_us(this, &IRReceiver::pulseTimeout, MAX_PULSE);
}

// End the current pulse
void IRReceiver::endPulse(bool lastPulseState)
{
    // Add the pulse to the buffer.  If the pulse already timed out,
    // we already wrote it, so there's no need to write it again.
    if (!pulseAtMax)
    {
        // get the time of the ending space
        uint32_t t = pulseTimer.read_us();
        
        // Scale by 2X to give us more range in a 16-bit int.  Since we're
        // also discarding the low bit (for the mark/space indicator below),
        // round to the nearest 4us by adding 2us before dividing.
        t += 2;
        t >>= 1;
        
        // limit the stored value to the uint16 maximum value
        if (t > 65535)
            t = 65535;
            
        // set the low bit if it's a mark, clear it if it's a space
        t &= ~0x0001;
        t |= lastPulseState;

        // add it to the buffer
        rawbuf.write(uint16_t(t));
    }
}

// Falling-edge interrupt.  The sensors we work with use active-low 
// outputs, so a high->low edge means that we're switching from a "space"
//(IR off) to a "mark" (IR on).
void IRReceiver::fall(void) 
{
    // If the transmitter is sending, ignore new ON pulses, so that we
    // don't try to read our own transmissions.
    if (transmitter != 0 && transmitter->isSending())
        return;

    // if we were in a space, end the space and start a mark
    if (!pulseState)
    {
        endPulse(false);
        startPulse(true);
    }
}

// Rising-edge interrupt.  A low->high edge means we're switching from
// a "mark" (IR on) to a "space" (IR off).
void IRReceiver::rise(void) 
{
    // if we were in a mark, end the mark and start a space
    if (pulseState)
    {
        endPulse(true);
        startPulse(false);
    }
}

// Pulse timeout.  
void IRReceiver::pulseTimeout(void)
{
    // End the current pulse, even though it hasn't physically ended,
    // so that the protocol processor can read it.  Pulses longer than
    // the maximum are all the same to the protocols, so we can process
    // these as soon as we reach the timeout.  However, don't start a
    // new pulse yet; we'll wait to do that until we get an actual
    // physical pulser.
    endPulse(pulseState);

    // note that we've reached the pulse timeout
    pulseAtMax = true;
}

// Process the buffer contents
void IRReceiver::process()
{
    // keep going until we run out of samples
    uint16_t t;
    while (rawbuf.read(t))
    {
        // Process it through the protocol handlers.  Note that the low
        // bit is the mark/space indicator, not a time bit, so pull it
        // out as the 'mark' argument and mask it out of the time.  And
        // note that the value in the buffer is in 2us units, so multiply
        // by 2 to get microseconds.
        processProtocols((t & ~0x0001) << 1, t & 0x0001);
    }
}

// Process one buffer pulse
bool IRReceiver::processOne(uint16_t &sample)
{
    // try reading a sample
    if (rawbuf.read(sample))
    {
        // Process it through the protocols - convert to microseconds
        // by masking out the low bit and mulitplying by the 2us units
        // we use in the sample buffer, and pull out the low bit as
        // the mark/space type.
        processProtocols((sample & ~0x0001) << 1, sample & 0x0001);
        
        // got a sample
        return true;
    }
    
    // no sample
    return false;
}

// Process one buffer pulse
bool IRReceiver::processOne(uint32_t &t, bool &mark)
{
    // try reading a sample
    uint16_t sample;
    if (rawbuf.read(sample))
    {
        // it's a mark if the low bit is set
        mark = sample & 0x0001;
        
        // remove the low bit, as it's not actually part of the time value,
        // and multiply by 2 to get from the 2us units in the buffer to
        // microseconds
        t = (sample & ~0x0001) << 1;
        
        // process it through the protocol handlers
        processProtocols(t, mark);
        
        // got a sample
        return true;
    }
    
    // no sample
    return false;
}

// Process a pulse through the protocol handlers
void IRReceiver::processProtocols(uint32_t t, bool mark)
{
    // generate a call to each sender in the RX list
    #define IR_PROTOCOL_RX(cls) IRProtocol::protocols->s_##cls.rxPulse(this, t, mark);
    #include "IRProtocolList.h"
}
