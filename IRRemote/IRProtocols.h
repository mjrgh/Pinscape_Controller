// IR Remote Protocols
//
// This file implements a handlers for specific IR protocols.  A protocol
// is the set of rules that a particular IR remote uses to convert binary
// data to and from a series of timed infrared pulses.  A protocol generally
// encompasses a bit-level encoding scheme (how each data bit is represented 
// as one or more pulses of IR light) and a message or packet format (how a 
// series of bits is assembled to form a higher level datatype, which in the 
// case of an IR remote usually amounts to a representation of a key press 
// on a remote control).
//
// Lots of companies make CE products with remotes, and many of them use
// their own custom protocols.  There's some commonality; a few proprietary
// protocols, such as those from NEC and Philips, are quasi industry
// standards that multiple companies adopted, more or less intact.  On
// the other hand, other companies not only have their own systems but
// use multiple proprietary systems across different products.  So it'll
// never be possible for us to recognize every remote code out there.
// So we'll cover the widely used ones, and then add the rarer ones as
// needed.
//
// For each protocol we recognize, we define a subclass of IRReceiver
// that implements the code to encode and decode signals for the protocol.
// To send a command, we call the sender function for the protocol we want
// to use for the transmission.  When we receive a signal, we run it through
// each protocol class's decode routine, to determine which protocol the
// signal was encoded with and what the signal means.  The decoders can
// usually tell if a signal uses their protocol, since there's enough
// structure in most of the protocols that you can distinguish signals that
// use the protocol from those that don't.  This allows the decoders to 
// serve the dual purposes of decoding signals and also classifying them
// by protocol.
//
// To add support for a new protocol, we (a) define a class for it here,
// and (b) add an entry for the class to IRProtocolList.h.  The list entry
// automatically adds the new protocol to the tables we use to look up the
// desired protocol class when sending, and to check each supported protocol
// when receiving.
//
// The protocol decoders operate in parallel: as a transmission is received,
// we run the signal through all of the decoders at the same time.  This
// allows each decoder to keep track of the incoming pulse stream and 
// recognize messages that conform to its protocol.  The parallel operation
// means that each protocol object needs its own complete, independent
// receiver state.
//
// In contrast, there can only be one transmission in progress at a time,
// since a transmission obviously requires exclusive access to the IR LED.
// (You can't eve interleave two transmissions in theory, since the coding 
// is all about pulse timing and order.)  That means that we *don't* need
// separate independent transmitter state per object.  That lets us save
// a little memory by using a single, shared transmitter state object,
// managed by the global transmitter class and passed to the current
// transmitter on each send.  The exception would be any state that has
// to be maintained across, which would have to be tracked per protocol.
// The only common example is "toggle bits".
//

#ifndef _IRPROTOCOLS_H_
#define _IRPROTOCOLS_H_

#include <ctype.h>
#include "NewPwm.h"
#include "IRRemote.h"
#include "IRReceiver.h"
#include "IRProtocolID.h"
#include "IRCommand.h"

using namespace IRRemote;

struct DebugItem { char c; int t; DebugItem(char c, int t) : c(c),t(t) {} 
    DebugItem(char c) : c(c), t(0) { } DebugItem() : c(0), t(0) { }
};
extern CircBuf<DebugItem,256> debug;


// IR transmitter state object.
//
// We can only transmit one signal at a time, so we only need to keep
// track of the state of one transmission at a time.  This lets us 
// save a little memory by using a single combined state object that's
// shared by all transmitters.  The transmitter currently operating 
// uses the shared object for as long as the transmission takes.
struct IRTXState
{
    // Time since the transmission started
    Timer txTime;
    
    // The command code we're transmitting
    uint64_t cmdCode;
    
    // Bit stream to transmit.  Many of the protocols use a universal
    // command representation (in IRCommand.code) that rearranges the
    // bits from the transmission order.  This is a scratch pad where the
    // protocol handler can translate a command code back into transmission
    // order once at the start of the transmission, then just shift bits out 
    // of here to transmit them in sequence.
    uint64_t bitstream;
    
    // The IR LED control pin
    NewPwmOut *pin;
    
    // Protocol ID
    uint8_t protocolId;
    
    // Transmission step.  The meaning is up to the individual protocols,
    // but generally this is used to keep track of the current structural
    // part of the pulse stream we're generating.  E.g., a protocol might
    // use 0 for the header mark, 1 for the header space, etc.
    uint8_t step;
    
    // number of bits to transmit
    uint8_t nbits;
    
    // Current bit position within the data
    uint8_t bit;
    
    // Substep within the current bit.  Many of the protocols represent each 
    // bit as multiple IR symbols, such as mark+space.  This keeps track of 
    // the step within the current bit.
    uint8_t bitstep;
    
    // Repeat phase.  Some protocols have rules about minimum repeat counts,
    // or use different coding for auto-repeats.  This lets the sender keep
    // track of the current step.  For example, the Sony protocol requires
    // each message to be sent a minimum of 3 times.  The NEC protocol uses
    // a "ditto" code for each repeat of a code after the initial command.
    // The OrtekMCE protocol requires a minimum of 2 sends per code, and has 
    // a position counter within the code that indicates which copy we're on.
    uint8_t rep;
    
    // Is the virtual button that initiated this transmission still pressed?  
    // The global transmitter sets this before each call to txStep() to let
    // the protocol know if it should auto-repeat at the end of the code.
    uint8_t pressed : 1;

    // Use "ditto" codes when sending repeats?  Some protocols use special
    // coding for auto-repeats, so that receivers can tell whether a key
    // is being held down or pressed repeatedly.  But in some cases, the
    // same protocol may be used with dittos on some devices but without
    // dittos on other devices.  It's therefore not always enough to know
    // that the protocol supports dittos; we have to know separately whether
    // the device we're sending to wants them.  This flag lets the caller
    // tell us which format to use.  This is ignored if the protocol either
    // never uses dittos or always does: in that case we'll do whatever the
    // protocol specifies.  To implement a "learning remote", you should
    // make sure that the user holds down each key long enough for several
    // repeats when learning codes, so that the learning remote can determine
    // when dittos are used by observing how the repeats are sent from the
    // reference remote.  Then you can set this bit if you saw any ditto
    // codes during training for a given key.
    uint8_t dittos : 1;
    
    // TX toggle bit.  We provide this for protocols that need it.  Note
    // that this is a global toggle bit, so if we switch from transmitting
    // one protocol to another and then return to the first, we'll lose 
    // continuity with the toggle sequence in the original protocol.  But
    // that shouldn't be a problem: the protocols with toggles only use it
    // to distinguish two rapid presses of the same key in succession from 
    // auto-repeat while the key is held down.  If we had an intervening
    // transmission to another protocol, the original receiver will see a
    // long gap between the earlier and later messages; the toggle bit isn't
    // necessary in this case to tell that these were two key presses.
    uint8_t toggle : 1;    
};


// Base class for all protocols
//
// Note that most of the data parameters are set through virtuals rather
// than member variables.  This helps minimize the RAM footprint.
//
class IRProtocol
{
public:
    IRProtocol() { }
    virtual ~IRProtocol() { }
    
    // look up a protocol by ID
    static IRProtocol *senderForId(int id);
    
    // name and ID
    virtual const char *name() const = 0;
    virtual int id() const = 0;
    
    // Are we a transmitter for the given protocol?  Some protocol
    // handlers send and receive variations on a protocol that have
    // distinct protocol IDs, such as the various Sony codes at
    // different bit sizes.  By default, we assume we handle only
    // our nominal protocol as returned by id().
    virtual bool isSenderFor(int protocolId) const { return protocolId == id(); }

    // parse a pulse on receive
    virtual void rxPulse(IRRecvProIfc *receiver, uint32_t t, bool mark) = 0;
    
    // PWM carrier frequency used for the IR signal, expressed as a PWM
    // cycle period in seconds.  We use this to set the appropriate pWM
    // frequency for transmissions.  The most common carrier is 38kHz.
    //
    // We can't use adjust the carrier frequency for receiving signals, since 
    // the TSOP sensor we use does the demodulation at a fixed frequency.
    // You can choose a frequency by choosing your sensor, since TSOP sensors
    // are available in a range of carrier frequencies, but once you choose a
    // sensor we can't set the frequency in software.  Fortunately, the 
    // TSOP384xx seems tolerant in practice of a fairly wide range around
    // its nominal 38kHz carrier.  I've successfull tested it with remotes
    // documented to use a few other frequencies from 36 to 40 kHz.)
    virtual float pwmPeriod(IRTXState *state) const { return 26.31578947e-6f; } // 38kHz
    
    // PWM duty cycle when transmitting
    virtual float pwmDutyCycle() const { return .3f; }
    
    // Begin transmitting the given command.  Before calling, the caller
    // turns off the IR LED and sets its PWM period to the one given by
    // our pwmPeriod() method.  The caller also clears 'state', and then
    // sets the 'cmd', 'pin', and 'pressed' fields.  The rest of the struct
    // is for the protocol handler's private use during the transmission.
    // handler is free to store its interim state here to pass information
    // from txStart() to txStep() and from one txStep() to the next.
    // 
    // Subclass implementations should start by setting up 'state' with any
    // data they need during the transmission.  E.g., convert the code word
    // value into a series of bits to transmit, and store this in the
    // 'bitstream' field.  Once that's set up, determine how long the initial
    // gap should be (the IR off time between transmissions in the protocol),
    // and return this time in microseconds.  The caller will return to
    // other work while the gap time is elapsing, then it will call txStep()
    // to advance to the next step of the transmission.
    //
    // DON'T do a timer pause or spin wait here.  This routine is called in
    // interrupt context, so it must return as quickly as possible.  All
    // waits should be done simply by returning the desired wait time.
    //
    // By convention, implementations should start each transmission with a
    // sufficient gap time (IR off) to allow senders to recognize the new
    // transmission.  It's best to put the gap time at the *start* of each
    // new transmission rather than at the end because two consecutive
    // transmissions might use different protocols with different timing
    // requirements.  If the first protocol has a short gap time and the
    // second has a long gap time, putting the gap at the end of the first
    // transmission would only use the shorter time, which might not be
    // sufficient for the second protocol's receiver.
    virtual int txStart(IRTXState *state) = 0;
        
    // Continue a transmission with the next pulse step.  This carries
    // out the next step of the transmission, then returns a time value
    // with the number of microseconds until the next event.  For example,
    // if the current step in the transmission is a 600us mark, turn on the
    // IR transmitter, update 'state' to indicate that we're in the mark,
    // and return 600 to tell the caller to go do other work while the
    // mark is being sent.  Don't wait or spin here, since this is called
    // in interrupt context and should thus return as quickly as possible.
    //
    // Before calling this, the caller will update the 'pressed' field in
    // 'state' to let us know if the virtual button is still being pressed.
    // The protocol handler can auto-repeat if the button is still pressed
    // at the end of the current transmission, if that's appropriate for
    // the protocol.  We let the handlers choose how to handle auto-repeat
    // rather than trying to manage it globally, since there's a lot of
    // variation in how it needs to be handled.  Some protocols, for example,
    // use "ditto" codes (distinct codes that mean "same as last time"
    // rather than actually re-transmitting a full command), while others
    // have internal position markers to indicate a series of repeats
    // for one key press.
    virtual int txStep(IRTXState *state) = 0;
    
    // protocol singletons
    static class IRProtocols *protocols;
    
    // allocate the protocol singletons, if we haven't already
    static void allocProtocols();
        
protected:
    // report code with a specific protocol and sub-protocol
    void reportCode(IRRecvProIfc *receiver, int pro, uint64_t code, bool3 toggle, bool3 ditto);
};

// -----------------------------------------------------------------------
//
// Protocol containing a decoded command value
//
template<class CodeType> class IRPWithCode: public IRProtocol
{
public:
    IRPWithCode() 
    {
        rxState = 0;
        bit = 0;
        code = 0;
    }
    
    // Minimum gap on receive (space before header)
    virtual uint32_t minRxGap() const = 0;
    
    // Gap time to send on transmit between before the first code, and
    // between repeats.  By default, we use the the generic txGap() in
    // both cases.
    virtual uint32_t txGap(IRTXState *state) const = 0;
    virtual uint32_t txPreGap(IRTXState *state) const { return txGap(state); }
    virtual uint32_t txPostGap(IRTXState *state) const { return txGap(state); }
    
    // Minimum number of repetitions when transmitting.  Some codes
    // (e.g., Sony) require a minimum repeat count, no matter how
    // quickly the button was released.  1 means that only a single
    // transmission is required, which is true of most codes.
    virtual int txMinReps(IRTXState *state) const { return 1; }
    
    // Header mark and space length.  Most IR protocols have an initial
    // mark and space of fixed length as a lead-in or header.  Being of
    // fixed length, they carry no data themselves, but they serve three
    // useful functions: the initial mark can be used to set an AGC level
    // if the receiver needs it (the TSOP sensors don't, but some older
    // devices did); they can help distinguish one protocol from another
    // by observing the distinctive timing of the header; and they serve
    // as synchronization markers, by using timing that can't possibly
    // occur in the middle of a code word to tell us unambiguously that
    // we're at the start of a code word.
    //
    // Not all protocols have the lead-in mark and/or space.  Some
    // protocols simply open with the initial data bit, and others use
    // an AGC mark but follow it immediately with a data bit, with no
    // intervening fixed space.
    //
    // * If the protocol doesn't use a header mark at all but opens with
    // a mark that's part of a data bit, set tHeaderMark() to 0.
    //
    // * If the protocol has a fixed AGC mark, but follows it with a 
    // varying-length space that's part of the first data bit, set 
    // tHeaderMark() to the fixed mark length and set tHeaderSpace() to 0.
    //
    virtual uint32_t tHeaderMark() const = 0;
    virtual uint32_t tHeaderSpace() const = 0;
    
    // Can the header space be adjacent to a data space?  In most protocols,
    // the header space is of constant length because it's always followed
    // by a mark.  This is accomplished in some protocols with explicit
    // structural marks; in some, it happens naturally because all bits start
    // with marks (e.g., NEC, Sony); and in others, the protocol requires a
    // a "start bit" with a fixed 0 or 1 value whose representation starts
    // with a mark (e.g., RC6).  But in a few protocols (e.g., OrtekMCE),
    // there's no such care taken, so the header space can flow into a space 
    // at the start of the first data bit.  Set this to true for such
    // protocols, and we'll divvy up the space after the header mark into
    // a fixed part and a data portion for the first bit.
    virtual bool headerSpaceToData() const { return false; }
    
    // Ditto header.  For codes with dittos that use a distinct header
    // format, this gives the header timing that identifies a ditto.
    // Return zero for a code that doesn't use dittos at all or encodes
    // them in some other way than a distinctive header (e.g., in the
    // payload data).
    virtual uint32_t tDittoMark() const { return 0; }
    virtual uint32_t tDittoSpace() const { return 0; }
    
    // Stop mark length.  Many codes have a fixed-length mark following
    // the data bits.  Return 0 if there's no final mark.
    virtual uint32_t tStopMark() const { return 0; }
    
    // Number of bits in the code.  For protocols with multiple bit
    // lengths, use the longest here.
    virtual int nbits() const = 0;
    
    // true -> bits arrive LSB-first, false -> MSB first
    virtual bool lsbFirst() const { return true; }

    // Pulse processing state machine.
    // This state machine handles the basic protocol structure used by
    // most IR remotes:
    //
    //   Header mark of fixed duration
    //   Header space (possibly followed directly by the first bit's space)
    //   Data bits
    //   Stop mark
    //   Gap between codes
    //   Ditto header mark   }  a pattern that's distinguishable from
    //   Ditto header space  }   the standard header mark
    //   Ditto data bits
    //   Gap betwee codes
    //
    // The state machine can handle protocols that use all of these sections,
    // or only a subset of them.  For example, a few protocols (Denon, RC5)
    // have no header at all and start directly wtih the data bits.  Most
    // protocols have no "ditto" coding and just repeat the main code to
    // signal auto-repeat.  
    //
    // The monolithic state machine switch looks kind of ugly, but it seems
    // to be the cleanest way to handle this.  My initial design was more OO,
    // using virtual subroutines to handle each step.  But that turned out to
    // be fragile, because there was too much interaction between the handlers
    // and the overall state machine sequencing.  The monolithic switch actually
    // seems much cleaner in practice.  The variations are all handled through
    // simple data parameters.  The resulting design isn't as flexible in
    // principle as something more virtualized at each step, but nearly all
    // of the IR protocols I've seen so far are so close to the same basic
    // structure that this routine works for practically everything.  Any
    // protocols that don't fit the same structure will be best served by
    // replacing the whole state machine for the individual protocols.
    virtual void rxPulse(IRRecvProIfc *receiver, uint32_t t, bool mark)
    {
        uint32_t tRef;
        switch (rxState)
        {
        case 0:
        s0:
            // Initial gap or inter-code gap.  When we see a space of
            // sufficient length, switch to Header Mark mode.
            rxState = (!mark && t > minRxGap() ? 1 : 0);
            break;
            
        case 1:
        s1:
            // Header mark.  If the protocol has no header mark, go
            // straight to the data section.  Otherwise, if we have
            // a mark that matches the header mark we're expecting, 
            // go to Header Space mode.  Otherwise, we're probably in
            // the middle of a code that we missed the beginning of, or
            // we're just receiving a code using another protocol.  Go
            // back to Gap mode - that will ignore everything until we
            // get radio silence for long enough to be sure we're 
            // starting a brand new code word.
            if ((tRef = tHeaderMark()) == 0)
                goto s3;
            rxState = (mark && inRange(t, tRef) ? 2 : 0);
            break;
            
        case 2:
        s2:
            // Header space.  If the protocol doesn't have a header
            // space, go straight to the data.
            if ((tRef = tHeaderSpace()) == 0)
                goto s3;            
                
            // If this protocol has an undelimited header space, make
            // sure this space is long enough to qualify, but allow it
            // to be longer.  If it qualifies, deduct the header space
            // span from it and apply the balance as the first data bit.
            if (headerSpaceToData() && aboveRange(t, tRef, tRef))
            {
                t -= tRef;
                goto s3;
            }
            
            // If we have a space matching the header space, enter the
            // data section. 
            if (!mark && inRange(t, tRef))
            {
                rxState = 3;
                break;
            }
            
            // Not a match - go back to gap mode
            goto s0;
            
        case 3:
        s3:
            // enter data section
            rxReset();
            if (mark) 
                goto s4;
            else 
                goto s5;
            
        case 4:
        s4:
            // data mark
            if (mark && rxMark(receiver, t))
            {
                rxState = bit < nbits() ? 5 : 7;
                break;
            }
            goto s0;
            
        case 5:
        s5:
            // data space
            if (!mark && rxSpace(receiver, t))
            {
                rxState = bit < nbits() ? 4 : 6;
                break;
            }
            else if (!mark && t > minRxGap())
                goto s7;
            goto s0;
            
        case 6:
            // stop mark - if we don't have a mark, go to the gap instead
            if (!mark)
                goto s7;
                
            // check to see if the protocol even has a stop mark
            if ((tRef = tStopMark()) == 0)
            {
                // The protocol has no stop mark, and we've processed
                // the last data bit.  Close the data section and go
                // straight to the next header.
                rxClose(receiver, false);
                goto s8;
            }
            
            // there is a mark - make sure it's in range
            if (inRange(t, tRef))
            {
                rxState = 7;
                break;
            }
            goto s0;
            
        case 7: 
        s7:
            // end of data - require minimum gap
            if (!mark && t > minRxGap())
            {
                // close the data section
                rxClose(receiver, false);
                rxState = 8;
                break;
            }
            goto s0;
                
        case 8:
        s8:
            // Ditto header.  If the protocol has a ditto header at all,
            // and this mark matches, proceed to the ditto space.  Otherwise
            // try interepreting this as a new regular header instead.
            if (mark && (tRef = tDittoMark()) != 0 && inRange(t, tRef))
            {
                rxState = 9;
                break;
            }
            goto s1;
            
        case 9:
            // Ditto space.  If this doesn't match the ditto space, and
            // the ditto header and regular header are the same, try
            // re-interpreting the space as a new regular header space.
            if (!mark && (tRef = tDittoSpace()) != 0 && inRange(t, tRef))
            {
                rxState = 10;
                break;
            }
            else if (!mark && tDittoMark() == tHeaderMark())
                goto s2;
            goto s0;
                
        case 10:
            // Enter ditto data
            rxDittoReset();
            goto s11;
            
        case 11:
        s11:
            // Ditto data - mark
            if (mark && rxMark(receiver, t))
            {
                rxState = bit < nbits() ? 12 : 13;
                break;
            }
            goto s0;
            
        case 12:
            // data space
            if (!mark && rxSpace(receiver, t))
            {
                rxState = bit < nbits() ? 11 : 13;
                break;
            }
            else if (!mark && t > minRxGap())
                goto s13;
            goto s0;
            
        case 13:
        s13:
            // end ditto data
            if (!mark && t > minRxGap())
            {
                // close the ditto data section
                rxClose(receiver, true);
                rxState = 8;
                break;
            }
            goto s0;
        }

        // if this is a space longer than the timeout, go into idle mode
        if (!mark && t >= IRReceiver::MAX_PULSE)
            rxIdle(receiver);
    }

    // Start transmission.  By convention, we start each transmission with
    // a gap of sufficient length to allow receivers to recognize a new
    // transmission.  The only protocol-specific work we usually have to
    // do here is to prepare a bit string to send.
    virtual int txStart(IRTXState *state)
    {
        // convert the code into a bitstream to send
        codeToBitstream(state);
        
        // transmit the initial gap to make sure we've been silent long enough
        return txPreGap(state);
    }
    
    // Continue transmission.  Most protocols have a similar structure,
    // with a header mark, header gap, data section, and trailing gap.
    // We implement the framework for this common structure with a
    // simple state machine:
    //
    //   state 0 = done with gap, transmitting header mark
    //   state 1 = done with header, transmitting header space
    //   state 2 = transmitting data bits, via txDataStep() in subclasses
    //   state 3 = done with data, sending post-code gap
    //
    // Subclasses for protocols that don't follow the usual structure can 
    // override this entire routine as needed and redefine these internal 
    // states.  Protocols that match the common structure will only have
    // to define txDataStep().
    //
    // Returns the time to the next step, or a negative value if we're
    // done with the transmission.
    virtual int txStep(IRTXState *state)
    {
        // The individual step handlers can return 0 to indicate
        // that we should go immediately to the next step without
        // a delay, so iterate as long as they return 0.
        for (;;)
        {
            // Use the first-code or "ditto" handling, as appropriate
            int t = state->rep > 0 && state->dittos ?
                txDittoStep(state) : 
                txMainStep(state);
            
            // If it's a positive time, it's a delay; if it's a negative
            // time, it's the end of the transmission.  In either case,
            // return the time to the main transmitter routine.  If it's
            // zero, though, it means to proceed without delay, so we'll
            // simply continue iterating.
            if (t != 0)
                return t;
        }
    }
    
    // Main transmission handler.  This handles the txStep() work for
    // the first code in a repeat group.
    virtual int txMainStep(IRTXState *state)
    {        
        // One state might transition directly to the next state
        // without any time delay, so keep going until we come to
        // a wait state.
        int t;
        for (;;)
        {
            switch (state->step)
            {
            case 0:
                // Finished the pre-code gap.  This is the actual start
                // of the transmission, so mark the time.
                state->txTime.reset();
                
                // if there's a header mark, transmit it
                state->step++;
                if ((t = this->tHeaderMark()) > 0)
                {
                    state->pin->write(pwmDutyCycle());
                    return t;
                }
                break;
                
            case 1:
                // finished header mark, start header space
                state->step++;
                if ((t = this->tHeaderSpace()) > 0)
                {
                    state->pin->write(0);
                    return this->tHeaderSpace();
                }
                break;
                
            case 2:
                // data section - this is up to the subclass
                if ((t = txDataStep(state)) != 0)
                    return t;
                break;
                
            case 3:
                // done with data; send the stop mark, if applicable
                state->step++;
                if ((t = tStopMark()) > 0)
                {
                    state->pin->write(pwmDutyCycle());
                    return t;
                }
                break;
                
            case 4:
                // post-code gap
                state->step++;
                state->pin->write(0);
                if ((t = this->txPostGap(state)) > 0)
                    return t;
                break;
                
            default:
                // Done with the iteration.  Finalize the transmission;
                // this will figure out if we're going to repeat.
                return this->txEnd(state);
            }
        }
    }
    
    // Ditto step handler.  This handles txStep() work for a repeated
    // code.  Most protocols just re-transmit the same code each time,
    // so by default we use the main step handling.  Subclasses for
    // protocols that transmit different codes on repeat (such as NEC)
    // can override this to send the ditto code instead.
    virtual int txDittoStep(IRTXState *state)
    {
        return txMainStep(state);
    }
    
    // Handle a txStep() iteration for a data bit.  Subclasses must
    // override this to handle the particulars of sending the data bits.
    // At the end of the data transmission, the subclass should increment
    // state->step to tell us that we've reached the post-code gap.
    virtual int txDataStep(IRTXState *state)
    {
        state->step++;
        return 0;
    }
    
    // Send the stop bit, if applicable.  If there's no stop bit or
    // equivalent, simply increment state->step and return 0;
    int txStopBit(IRTXState *state)
    {
        state->step++;
        return 0;
    }
    
    // Handle the end of a transmission.  This figures out if we're
    // going to auto-repeat, and if so, resets for the next iteration.
    // If we're going to repeat, we return the time to the next tx step,
    // as usual.  If the transmission is done, we return -1 to indicate
    // that no more steps are required.
    int txEnd(IRTXState *state)
    {
        // count the repeat
        state->rep++;

        // If the button is still down, or we haven't reached our minimum
        // repetition count, repeat the code.
        if (state->pressed || state->rep < txMinReps(state))
        {
            // return to the first transmission step
            state->step = 0;
            state->bit = 0;
            state->bitstep = 0;
            
            // re-generate the bitstream, in case we need to encode positional
            // information such as a toggle bit or position counter
            codeToBitstream(state);
            
            // restart the transmission timer
            state->txTime.reset();
            
            // we can go immediately to the next step, so return a zero delay
            return 0;
        }
        
        // we're well and truly done - tell the caller not to call us
        // again by returning a negative time interval
        return -1;
    }

protected:
    // Reset the receiver.  This is called when the receiver enters
    // the data section of the frame, after parsing the header (or
    // after a gap, if the protocol doesn't use a header).
    virtual void rxReset()
    {
        bit = 0;
        code = 0;
    }
    
    // Reset on entering a new ditto frame.  This is called after
    // parsing a "ditto" header.  This is only needed for protocols
    // that use distinctive ditto framing.
    virtual void rxDittoReset() { }
    
    // Receiver is going idle.  This is called any time we get a space
    // (IR OFF) that exceeds the general receiver timeout, regardless
    // of protocol state.  
    virtual void rxIdle(IRRecvProIfc *receiver) { }
    
    // Parse a data mark or space.  If the symbol is valid, shifts the
    // bit into 'code' (the code word under construction) and returns
    // true.  If the symbol is invalid, returns false.  Updates the
    // bit counter in 'bit' if this symbol finishes a bit.
    virtual bool rxMark(IRRecvProIfc *receiver, uint32_t t) { return false; }
    virtual bool rxSpace(IRRecvProIfc *receiver, uint32_t t) { return false; }
    
    // Report the decoded value in our internal register, if it's valid.
    // By default, we'll report the code value as stored in 'code', with
    // no toggle bit, if the number of bits we've decoded matches the
    // expected number of bits as given by nbits().  Subclasses can 
    // override as needed to do other validation checks; e.g., protocols
    // with varying bit lengths will need to check for all of the valid
    // bit lengths, and protocols that contain error-checking information
    // can validate that.
    //
    // Unless the protocol subclass overrides the basic pulse handler
    // (rxPulse()), this is called when we end the data section of the
    // code.
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        if (bit == nbits())
            reportCode(receiver, id(), code, bool3::null, ditto);
    }
    
    // Encode a universal code value into a bit string in preparation
    // for transmission.  This should take the code value in state->cmdCode,
    // convert it to the string of bits to serially, and store the result
    // in state->bitstream.
    virtual void codeToBitstream(IRTXState *state)
    {
        // by default, simply store the code as-is
        state->bitstream = state->cmdCode;
        state->nbits = nbits();
    }
    
    // Get the next bit for transmission from the bitstream object
    int getBit(IRTXState *state)
    {
        // get the number of bits and the current bit position
        int nbits = state->nbits;
        int bit = state->bit;
        
        // figure the bit position according to the bit order
        int bitpos = (lsbFirst() ? bit : nbits - bit - 1);
        
        // pull out the bit
        return int((state->bitstream >> bitpos) & 1);
    }
    
    // Set the next bit to '1', optionally incrementing the bit counter
    void setBit(bool inc = true)
    {
        // ignore overflow
        int nbits = this->nbits();
        if (bit >= nbits)
            return;

        // Figure the starting bit position in 'code' for the bit,
        // according to whether we're adding them LSB-first or MSB-first.
        int bitpos = (lsbFirst() ? bit : nbits - bit - 1);
        
        // mask in the bit
        code |= (CodeType(1) << bitpos);
        
        // advance the bit position
        if (inc)
            ++bit;
    }
    
    // Set the next N bits to '1'
    void setBits(int n)
    {
        // ignore overflow
        int nbits = this->nbits();
        if (bit + n - 1 >= nbits)
            return;
        
        // Figure the starting bit position in 'code' for the bits we're 
        // setting.  If bits arrive LSB-first, the 'bit' counter gives us
        // the bit position directly.  If bits arrive MSB-first, we need
        // to start from the high end instead, so we're starting at
        // ((nbits()-1) - bit).  However, we want to set multiple bits
        // left-to-right in any case, so move our starting position to
        // the "end" of the window by moving right n-1 addition bits.
        int bitpos = (lsbFirst() ?  bit : nbits - bit - n);
        
        // turn that into a bit mask for the first bit
        uint64_t mask = (CodeType(1) << bitpos);
        
        // advance our 'bit' counter past the added bits
        bit += n;
        
        // set each added bit to 1
        for ( ; n != 0 ; --n, mask <<= 1)
            code |= mask;
    }

    // decoding state   
    uint8_t rxState; 

    // next bit position
    uint8_t bit;
    
    // command code under construction
    CodeType code;
};

// -----------------------------------------------------------------------
//
// Basic asynchronous encoding
// 
// This is essentially the IR equivalent of a wired UART.  The transmission 
// for a code word is divided into a fixed number of bit time periods of 
// fixed length, with each period containing one bit, signified by IR ON for 
// 1 or IR OFF for 0.
// 
// Simple async coding doesn't have any inherent structure other than the
// bit length.  That makes it hard to distinguish from other IR remotes that
// might share the environment, and even from random noise, which is why
// most CE manufacturers use more structured protocols.  In practice, remotes
// using this coding are likely have to impose some sort of structure on the
// bits, such as long bit strings, error checking bits, or distinctive prefixes
// or suffixes.  The only example of a pure async code I've seen in the wild 
// is Lutron, and they do in fact use fairly long codes (36 bits) with set 
// prefixes.  Their prefixes aren't only distinctive as bit sequences, but
// also in the raw space/mark timing, which makes them effectively serve the
// function that header marks do in most of the structured protocols.
//
template<class CodeType> class IRPAsync: public IRPWithCode<CodeType>
{
public:
    IRPAsync() { }
    
    // duration of each bit in microseconds
    virtual int tBit() const = 0;
    
    // maximum legal mark in a data section, if applicable
    virtual uint32_t maxMark() const { return IRReceiver::MAX_PULSE; }
    
    // Async codes lack the structure of the more typical codes, so we
    // use a completely custom pulse handler
    virtual void rxPulse(IRRecvProIfc *receiver, uint32_t t, bool mark)
    {
        uint32_t tRef, tRef2;
        switch (this->rxState)
        {
        case 0:
        s0:
            // Gap - if this is a long enough space, switch to header mode.
            this->rxState = (!mark && t > this->minRxGap() ? 1 : 0);
            break;
            
        case 1:
            // Header mode.  Async protocols don't necessarily have headers,
            // but some (e.g., Lutron) do start all codes with a distinctively
            // long series of bits.  If the subclass defines a header space,
            // apply it to sense the start of a code.
            if ((tRef = this->tHeaderMark()) == 0)
                goto s2;
                
            // we have a defined header mark - make sure this is long enough
            tRef2 = tBit();
            if (mark && inRangeOrAbove(t, tRef, tRef2))
            {
                // deduct the header time
                t = t > tRef ? t - tRef : 0;
                
                // if that leaves us with a single bit time or better,
                // treat it as the first data bit
                if (inRangeOrAbove(t, tRef2, tRef2))
                    goto s2;
                    
                // that consumes the whole mark, so just switch to data
                // mode starting with the next space
                this->rxState = 2;
                break;
            }
            
            // doesn't look like the start of a code; back to gap mode
            goto s0;
                
        case 2:
        s2:
            // Enter data mode
            this->rxReset();
            goto s3;
            
        case 3:
        s3:
            // Data mode.  Process the mark or space as a number of bits.
            {
                // figure how many bits this symbol represents
                int tb = tBit();
                int n = (t + tb/2)/tb;
                
                // figure how many bits remain in the code
                int rem = this->nbits() - this->bit;
                
                // check to see if this symbol overflows the bits remaining
                if (n > rem)
                {
                    // marks simply can't exceed the bits remaining
                    if (mark)
                        goto s0;
                        
                    // Spaces can exceed the remaining bits, since we can 
                    // have a string of 0 bits followed by a gap between 
                    // codes.  Use up the remaining bits as 0's, and apply 
                    // the balance as a gap.
                    this->bit += rem;
                    t -= rem*tb;
                    goto s4;
                }
                
                // check if it exceeds the code's maximum mark length
                if (mark && t > maxMark())
                    goto s0;
                
                // Make sure that it actually looks like an integral
                // number of bits.  If it's not, take it as a bad code.
                if (!inRange(t, n*tb, tb))
                    goto s0;
                    
                // Add the bits
                if (mark)
                    this->setBits(n);
                else
                    this->bit += n;
                    
                // we've consumed the whole interval as bits
                t = 0;
                
                // if that's enough bits, we have a decode
                if (this->bit == this->nbits())
                    goto s4;
                    
                // stay in data mode
                this->rxState = 3;
            }
            break;
            
        case 4:
        s4:
            // done with the code - close it out and start over
            this->rxClose(receiver, false);
            goto s0;
        }
    }
    
    // send data
    virtual int txDataStep(IRTXState *state) 
    { 
        // get the next bit
        int b = this->getBit(state);
        state->bit++;
        
        // count how many consecutive matching bits follow
        int n = 1;
        int nbits = state->nbits;
        for ( ; state->bit < nbits && this->getBit(state) == b ; 
            ++n, ++state->bit) ;
        
        // if we're out of bits, advance to the next step
        if (state->bit >= nbits)
            ++state->step;
        
        // 0 bits are IR OFF and 1 bits are IR ON
        state->pin->write(b ? this->pwmDutyCycle() : 0);
        
        // stay on for the number of bits times the time per bit
        return n * this->tBit();
    }
};


// -----------------------------------------------------------------------
//
// Space-length encoding
//
// This type of encoding uses the lengths of the spaces to encode the bit
// values.  Marks are all the same length, and spaces come in two lengths,
// short and long, usually T and 2T for a vendor-specific time T (typically
// on the order of 500us).  The short space encodes 0 and the long space
// encodes 1, or vice versa.
//
// The widely used NEC protocol is a space-length encoding, and in practice
// it seems that most of the ad hoc proprietary protocols are also space-
// length encodings, mostly with similar parameters to NEC.
//
template<class CodeType> class IRPSpaceLength: public IRPWithCode<CodeType>
{
public:
    IRPSpaceLength() { }

    // mark length, in microseconds
    virtual int tMark() const = 0;
    
    // 0 and 1 bit space lengths, in microseconds
    virtual int tZero() const = 0;
    virtual int tOne() const = 0;
    
    // Space-length codings almost always need a mark after the last
    // bit, since otherwise the last bit's space (the significant part)
    // would just flow into the gap that follows.
    virtual uint32_t tStopMark() const { return tMark(); }

    // process a mark
    virtual bool rxMark(IRRecvProIfc *receiver, uint32_t t)
    {
        // marks simply delimit spaces in this protocol and thus
        // carry no bit information
        if (inRange(t, tMark()))
            return true;
        else
            return false;
    }
     
    // process a space   
    virtual bool rxSpace(IRRecvProIfc *receiver, uint32_t t)
    {
        // a short space represents a '0' bit, a long space is a '1'
        if (inRange(t, tZero()))
            return this->bit++, true;
        else if (inRange(t, tOne()))
            return this->setBit(), true;
        else
            return false;
    }

    // continue a transmission
    virtual int txDataStep(IRTXState *state)
    {
        // Data section.
        if (state->bitstep == 0)
        {
            // mark - these are fixed length
            state->pin->write(this->pwmDutyCycle());
            state->bitstep = 1;
            return tMark();
        }
        else
        {
            // space - these are variable length according to the data
            state->pin->write(0);
            int t = this->getBit(state) ? tOne() : tZero();
            state->bitstep = 0;

            // advance to the next bit; stop if we're done
            if (++state->bit >= state->nbits)
                ++state->step;
                
            // return the space time
            return t;
        }
    }
    
};


// -----------------------------------------------------------------------
//
// Mark-length encoding
//
// This is the inverse of space-length coding.  In this scheme, the bit
// values are encoded in the mark length.  Spaces are fixed length, and
// marks come in short (0) and long (1) lengths, usually of time T and 2T
// for a protocol-specific time T.
//
// Sony uses this type of encoding.
template<class CodeType> class IRPMarkLength: public IRPWithCode<CodeType>
{
public:
    IRPMarkLength() { }

    // space length, in microseconds
    virtual int tSpace() const = 0;
    
    // 0 and 1 bit mark lengths, in microseconds
    virtual int tZero() const = 0;
    virtual int tOne() const = 0;

    // process a mark
    virtual bool rxMark(IRRecvProIfc *receiver, uint32_t t)
    {
        // a short mark represents a '0' bit, a long space is a '1'
        if (inRange(t, tZero()))
            this->bit++;
        else if (inRange(t, tOne()))
            this->setBit();
        else
            return false;
        return true;
    }
     
    // process a space   
    virtual bool rxSpace(IRRecvProIfc *receiver, uint32_t t)
    {
        // spaces simply delimit marks in this protocol and carry
        // no bit information of their own
        return inRange(t, tSpace());
    }

    // continue a transmission
    virtual int txDataStep(IRTXState *state)
    {
        // check if we're on a mark (step 0) or space (step 1)
        if (state->bitstep == 0)
        {
            // space - these are variable length according to the data
            state->pin->write(this->pwmDutyCycle());
            int t = this->getBit(state) ? tOne() : tZero();
            state->bitstep = 1;
                
            // return the mark time
            return t;
        }
        else
        {
            // Space - fixed length
            state->pin->write(0);
            state->bitstep = 0;
            
            // advance to the next bit; stop if we're done
            if (++state->bit >= state->nbits)
                state->step = 3;
            return tSpace();
        }
    }
    
};


// -----------------------------------------------------------------------
//
// Manchester coding
//
// This type of coding uses a fixed time per bit, and encodes the bit
// value in a mark/space or space/mark transition within each bit's
// time window.  
//
// The decoding process is a little tricky to grap when you're looking
// at just the raw data, because the raw data renders things in terms of 
// monolithic marks and spaces of different lengths, whereas the coding 
// divides the time axis into even chunks and looks at what's going on
// in each chunk.  In terms of the raw data, we can think of it this way.
// Because every bit time window has a transition (mark/space or space/mark)
// in the middle of it, there has to be at least one transition per window.
// There can also be a transition between each window, or not, as needed
// to get the transmitter into the right initial state for the next bit.  
// This means that each mark and each space is either T or 2T long, where
// T is the half the bit window time.  So we can simply count these units.
// If we see a mark or space of approximate length T, we count one unit;
// if the length is around 2T, we count two units.  On each ODD count, we
// look at the state just before the count.  If we were in a space just
// before the count, the bit is a 1; if it was a mark, the bit is a 0.
//
// Manchester coding is used in the Philips RC5 and RC6 protocols, which
// are in turn used by most other European CE companies.
template<class CodeType> class IRPManchester: public IRPWithCode<CodeType>
{
public:
    IRPManchester() { }
    
    // Half-bit time.  This is half of the time interval of one bit,
    // so it's equal to the time on each side of the mark/space or
    // space/mark transition in the middle of each bit.
    virtual int tHalfBit(int bit) const = 0;
    
    // Bit value (0 or 1) of a space-to-mark transition.  A mark-to-space
    // transition always has the opposite sense.
    virtual int spaceToMarkBit() const { return 1; }
    inline int markToSpaceBit() const { return !spaceToMarkBit(); }
    
    // reset the decoder state
    virtual void rxReset()
    {
        IRPWithCode<CodeType>::rxReset();
        halfBitPos = 0;
    }

    // process a mark
    virtual bool rxMark(IRRecvProIfc *receiver, uint32_t t)
    {
        // transitioning from mark to space, so this is a 
        return processTransition(t, spaceToMarkBit());
    }
    
    // process a space
    virtual bool rxSpace(IRRecvProIfc *receiver, uint32_t t)
    {
        return (processTransition(t, markToSpaceBit()));
    }
    
    // Process a space/mark or mark/space transition.  Returns true on
    // success, false on failure.
    bool processTransition(uint32_t t, int bitval)
    {
        // If this time is close to zero, ignore it.
        int thb = tHalfBit(this->bit);
        if (t < ((thb*toleranceShl8) >> 8))
            return true;
        
        // If the current time is the middle of a bit, the transition
        // specifies a bit value, so set the bit.  Transitions between
        // bits are just clocking.
        if (halfBitPos == 1)
        {
            if (bitval)
            {
                // set the bit, but keep the bit counter where it is, since
                // we manage it ourselves
                this->setBit();
                this->bit--;
            }
        }
        
        // Advance by the time interval.  Check that we have at least one
        // half-bit interval to work with.
        if (t < ((thb * (256 - toleranceShl8)) >> 8))
            return false;
        
        // If we're at a half-bit position, start by advancing to the next
        // bit boundary.
        if (halfBitPos)
        {
            // deduct the half-bit time
            t = (t > thb ? t - thb : 0);

            // advance our position counters to the next bit boundary
            halfBitPos = 0;
            this->bit++;
            
            // Some subprotocols (e.g., RC6) have variable bit timing,
            // so the timing for this bit might be different than for
            // the previous bit.  Re-fetch the time.
            thb = tHalfBit(this->bit);
        }
        
        // If we have another half-interval left to process, advance
        // to the middle of the current bit.
        if (t < ((thb * toleranceShl8) >> 8))
        {
            // we already used up the symbol time, so we're done
            return true;
        }
        else if (inRange(t, thb))
        {
            // we have another half-bit time to use, so advance to
            // the middle of the bit
            halfBitPos = true;
            return true;
        }
        else
        {
            // The time remaining is wrong, so terminate decoding.
            // Note that this could simply be because we've reached
            // the gap at the end of the code word, in which case we'll
            // already have all of the bits stored and will generate
            // the finished code value.
            return false;
        }
    }
    
    virtual int txDataStep(IRTXState *state) 
    { 
        // Get the current bit
        int b = this->getBit(state);
        
        // Determine if this bit uses a space-to-mark or mark-to-space
        // transition.  It uses a space-to-mark transition if it matches
        // the space-to-mark bit.
        int stm = (b == spaceToMarkBit());
        
        // Check to see if we're at the start or middle of the bit
        if (state->bitstep == 0)
        {
            // Start of the current bit.  Set the level for the first
            // half of the bit.  If we're doing a space-to-mark bit,
            // the first half is a space, otherwise it's a mark.
            state->pin->write(stm ? 0 : this->pwmDutyCycle());
            
            // leave this on for a half-bit time to get to the 
            // middle of the bit
            state->bitstep = 1;
            return tHalfBit(state->bit);
        }
        else
        {
            // Middle of the current bit.  Set the level for the second
            // half of the bit.  If we're in a space-to-mark bit, the
            // second half is the mark, otherwise it's the space.
            state->pin->write(stm ? this->pwmDutyCycle() : 0);
            
            // figure the time to the start of the next bit
            int t = tHalfBit(state->bit);
            
            // advance to the start of the next bit
            state->bit++;
            state->bitstep = 0;
            
            // If the next bit is the inverse of the current bit, it will
            // lead in with the same level we're going out with.  That 
            // means we can go straight to the middle of the next bit
            // without another interrupt.
            if (state->bit < state->nbits && this->getBit(state) != b)
            {
                // proceed to the middle of the next bit
                state->bitstep = 1;
                
                // add the half-bit time
                t += tHalfBit(state->bit);
            }
            
            // if this was the last bit, advance to the next state
            if (state->bit >= state->nbits)
                state->step++;
            
            // return the time to the next transition
            return t;
        }
    }

    // Current half-bit position.  If the last transition was on the
    // border between two bits, this is 0.  If it was in the middle
    // of a bit, this is 1.
    uint8_t halfBitPos : 1;
};

// -----------------------------------------------------------------------
// 
// NEC protocol family.  This is one of the more important proprietary 
// protocols, since many CE companies use the standard NEC code or a 
// variation of it.  This class handles the following variations on the
// basic NEC code:
//
//   NEC-32:  32-bit payload, 9000us header mark, 4500us header space
//   NEC-32X: 32-bit payload, 4500us header mark, 4500us header space
//   NEC-48:  48-bit payload, 9000us header mark, 4500us header space
//   Pioneer: NEC-32 with address A0..AF + possible "shift" prefixes
//
// Each of the three NEC-nn protocol types comes in two sub-types: one
// that uses "ditto" codes for repeated keys, and one that simply sends
// the same code again on repeats.  The ditto code, when used, varies with 
// the main protocol type:
//
//   NEC-32:  9000us mark + 2250us space + 564us mark
//   NEC-32x: 4500us mark + 4500us space + one data bit + 564us mark
//   NEC-48:  9000us mark + 2250us space + 564us mark
//   Pioneer: no dittos
//
// The NEC-32 and NEC-48 dittos can be detected from the header space
// length.  The NEC-32x dittos can be detected by the one-bit code length.
//
// All variations of the NEC code are space-length encodings with 564us
// marks between bits, 564us '0' spaces, and 3*564us '1' spaces.  All
// variations use a long header mark and a 564us stop mark.  With those
// fixed features, there are three main variations:
//
// The bits in the NEC 32-bit codes are structured into four 8-bit fields, 
// with the first bit transmitted in the most significant position:
//
//   A1 A0 C1 C0
//
// A1 is the high 8 bits of the address, and A0 is the low 8 bits of the
// address.  The address specifies a particular type of device, such as
// "NEC VCR" or "Vizio TV".  These are assigned by NEC.  C1 and C0 form the
// command code, which has a meaning specific to the device type specified 
// by the address field.
//
// In the original NEC protocol, the nominal address is in A1, and A0 is
// the 1's complement of A1 (A0 = ~A1), for error checking.  This was removed
// in a later revision to expand the address space.  Most modern equipment
// uses the newer system, so A0 is typically an independent value in remotes
// you'll find in use today.
//
// In the official version of the protocol, C1 is the nominal command code,
// and C0 = ~C1, for error checking.  However, some other manufacturers who
// use the basic NEC protocol, notably Yamaha and Onkyo, violate this by using
// C0 as an independent byte to expand the command space.  We therefore don't
// test for the complemented byte, so that we don't reject codes from devices
// that treat it as independent. 
// 
// Pioneer uses the NEC protocol with two changes.  First, the carrier is
// 40kHz instead of 38kHz.  The TSOP384xx seems to receive the 40kHz signal
// reliably, so the frequency doesn't matter on receive, but it might matter
// on transmit if the target equipment isn't as tolerant.  Second, Pioneer
// sometimes transmits a second code for the same key.  In these cases, the
// first code is a sort of "shift" code (shared among many keys), and the
// second code has the actual key-specific meaning.  To learn or recognize
// these extended codes, we have to treat the pair of code words as a single 
// command.  We sense Pioneer codes based on the address field, and use
// special handling when we find a Pioneer address code.
//
class IRPNEC: public IRPSpaceLength<uint64_t>
{
public:
    // code parameters
    virtual uint32_t minRxGap() const { return 3400; }
    virtual uint32_t txGap(IRTXState *state) const 
        { return 108000 - state->txTime.read_us(); }
        
    // space length encoding parameters
    virtual int tMark() const { return 560; }
    virtual int tZero() const { return 560; }
    virtual int tOne() const { return 1680; }
    
    // PWM period is 40kHz for Pioneer, 38kHz for everyone else
    virtual float pwmPeriod(IRTXState *state) const 
    { 
        return state->protocolId == IRPRO_PIONEER ? 25.0e-6f : 26.31578947e-6f; 
    }
    
    // for Pioneer, we have to send two codes if we have a shift field
    virtual int txMinReps(IRTXState *state) const
    {
        if (state->protocolId == IRPRO_PIONEER 
            && (state->cmdCode & 0xFFFF0000) != 0)
            return 2;
        else
            return 1;
    }
    
    // get the protocol to report for a given data packet bit count
    virtual int necPro(int bits) const = 0;
    
    // close out a received bitstream
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        // Check the bit count.  If nbits() says we can accept 48 bits,
        // accept 48 bits, otherwise only accept 32.
        if (bit == 32 || (nbits() >= 48 && bit == 48))
        {
            uint64_t codeOut;
            if (ditto)
            {
                // report 0 for dittos
                codeOut = 0;
            }
            else
            {
                // Put the bytes in the right order.  The bits are LSB-first, but
                // we want the bytes to be ordered within the uint32 as (high to
                // low) A0 A1 C0 C1.
                uint8_t c1 = uint8_t((code >> 24) & 0xff);
                uint8_t c0 = uint8_t((code >> 16) & 0xff);
                uint8_t a1 = uint8_t((code >> 8) & 0xff);
                uint8_t a0 = uint8_t(code & 0xff);
                codeOut = (uint64_t(a0) << 24) | (uint64_t(a1) << 16)
                    | (uint64_t(c0) << 8) | uint64_t(c1);
                    
                // If it's a 48-bit code, add the additional 16 bits for E0 E1
                // (the extended command code) at the low end.
                if (bit == 48)
                {
                    // get the E1 and E0 bytes from the top end of the code                
                    uint8_t e1 = uint8_t((code >> 40) & 0xff);
                    uint8_t e0 = uint8_t((code >> 32) & 0xff);
                    
                    // insert them at the low end in E0 E1 order
                    codeOut <<= 16;
                    codeOut |= (uint64_t(e0) << 8) | uint64_t(e1);
                }
            }
    
            // report it
            reportCode(receiver, necPro(bit), codeOut, bool3::null, ditto);
        }
    }

    // convert a code to a bitstream for sending    
    virtual void codeToBitstream(IRTXState *state)
    {
        if (state->protocolId == IRPRO_PIONEER)
        {
            // Check if we have an extended code
            uint32_t c;
            if ((state->cmdCode & 0xFFFF0000) != 0)
            {
                // Extended code.  These are transmitted as two codes in
                // a row, one for the shift code in the high 16 bits, and
                // one for the subcode in the low 16 bits.  Transmit the
                // shift code on even reps and the subcode on odd reps.
                if (state->rep == 0 || state->rep == 2)
                {
                    // even rep - use the shift code
                    c = (state->cmdCode >> 16) & 0xFFFF;
                    
                    // wrap back to rep 0 on rep 2
                    state->rep = 0;
                }
                else
                {
                    // odd rep - use the subcode
                    c = state->cmdCode & 0xFFFF;
                }
            }
            else
            {
                // it's a single-part code
                c = state->cmdCode;
            }
            
            // encode it in the 32-bit original NEC format with the address
            // and command byte complemented
            uint8_t a0 = uint8_t((c >> 8) & 0xff);
            uint8_t a1 = uint8_t(~a0);
            uint8_t c0 = uint8_t(c & 0xff);
            uint8_t c1 = uint8_t(~c0);
            state->bitstream = (uint64_t(c1) << 24) | (uint64_t(c0) << 16)
                | (uint64_t(a1) << 8) | uint64_t(a0);
            state->nbits = 32;
            
            // Pioneer *can't* use NEC dittos even if the caller thinks we
            // should, because that breaks the shift-code model Pioneer uses
            state->dittos = false;
        }
        else if (state->protocolId == IRPRO_NEC48)
        {
            // NEC 48-bit code.  We store the bytes in the universal 
            // representation in order A0 A1 C0 C1 E0 E1.  Reverse this
            // order for transmission.
            uint64_t code = state->cmdCode;
            uint8_t a0 = uint8_t((code >> 40) & 0xff);
            uint8_t a1 = uint8_t((code >> 32) & 0xff);
            uint8_t c0 = uint8_t((code >> 24) & 0xff);
            uint8_t c1 = uint8_t((code >> 16)& 0xff);
            uint8_t e0 = uint8_t((code >> 8) & 0xff);
            uint8_t e1 = uint8_t((code) & 0xff);
            state->bitstream = 
                (uint64_t(e1) << 40) | (uint64_t(e0) << 32)
                | (uint64_t(c1) << 24) | (uint64_t(c0) << 16)
                | (uint64_t(a1) << 8) | uint64_t(a0);
            state->nbits = 48; 
        }
        else
        {
            // NEC 32-bit code.  The universal representation stores
            // the bytes in order A0 A1 C0 C1.  For transmission, we
            // need to reverse this to C1 C0 A1 A0.
            uint32_t code = uint32_t(state->cmdCode);
            uint8_t a0 = uint8_t((code >> 24) & 0xff);
            uint8_t a1 = uint8_t((code >> 16) & 0xff);
            uint8_t c0 = uint8_t((code >> 8) & 0xff);
            uint8_t c1 = uint8_t(code & 0xff);
            state->bitstream = 
                (uint64_t(c1) << 24) | (uint64_t(c0) << 16)
                | (uint64_t(a1) << 8) | uint64_t(a0);
            state->nbits = 32; 
        }
    }

    // NEC uses a special "ditto" code for repeats.  The ditto consists
    // of the normal header mark, half a header space, a regular data
    // mark.  After this, a standard inter-message gap follows, and then
    // we repeat the ditto as long as the button is held down.
    virtual int txDittoStep(IRTXState *state)
    {
        // send the ditto
        uint32_t t;
        switch (state->step)
        {
        case 0:
            // Ditto header mark
            state->step++;
            state->pin->write(pwmDutyCycle());
            
            // use the special ditto mark timing if it's different; 0 means
            // that we use the same timing as the standard data frame header
            return (t = tDittoMark()) != 0 ? t : tHeaderMark();
            
        case 1:
            // Ditto header space
            state->step++;
            state->pin->write(0);
            
            // use the special ditto timing if it's different
            return (t = tDittoSpace()) != 0 ? t : tHeaderSpace();
            
        case 2:
            // Data section.  NEC-32X sends one data bit.  The others
            // send no data bits, so go straight to the stop bit.
            if (state->protocolId == IRPRO_NEC32X && state->bit == 0)
                return txDataStep(state);
                
            // for others, fall through to the stop mark
            state->step++;
            // FALL THROUGH...
            
        case 3:
            // stop mark
            state->step++;
            state->pin->write(pwmDutyCycle());
            return tMark();
            
        case 4:
            // send a gap
            state->step++;
            state->pin->write(0);
            return 108000 - state->txTime.read_us();
            
        default:
            // done
            return txEnd(state);
        }
    }

};

// NEC-32, NEC-48, and Pioneer
class IRPNEC_32_48: public IRPNEC
{
public:
    IRPNEC_32_48()
    {
        pioneerPrvCode = 0;
    }

    // name and ID
    virtual const char *name() const { return "NEC"; }
    virtual int id() const { return IRPRO_NEC32; }
    virtual int necPro(int bits) const 
    { 
        return bits == 48 ? IRPRO_NEC48 : IRPRO_NEC32; 
    }
    
    // we encode several protocols
    virtual bool isSenderFor(int pro) const
    {
        return pro == IRPRO_NEC32 || pro == IRPRO_NEC48 || pro == IRPRO_PIONEER;
    }

    // NEC-32 and NEC-48 use the same framing
    virtual uint32_t tHeaderMark() const { return 9000; }
    virtual uint32_t tHeaderSpace() const { return 4500; }
    virtual uint32_t tDittoMark() const { return 9000; }
    virtual uint32_t tDittoSpace() const { return 2250; }
    virtual int nbits() const { return 48; }

    // receiver format descriptor
    // decode, check, and report a code value
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        // If we're in Pioneer mode, use special handling
        if (isPioneerCode())
        {
            rxClosePioneer(receiver);
            return;
        }
        
        // The new code isn't a pioneer code, so if we had a Pioneer
        // code stashed, it doesn't have a second half.  Report is as
        // a standalone code.
        if (pioneerPrvCode != 0)
        {
            reportPioneerFormat(receiver, pioneerPrvCode);
            pioneerPrvCode = 0;
        }

        // use the generic NEC handling
        IRPNEC::rxClose(receiver, ditto);
    }
    
    virtual void rxIdle(IRRecvProIfc *receiver)
    {
        // if we have a stashed prior pioneer code, close it out, since
        // no more codes are forthcoming
        if (pioneerPrvCode != 0)
        {
            reportPioneerFormat(receiver, pioneerPrvCode);
            pioneerPrvCode = 0;
        }
    }

    // close out a Pioneer code
    virtual void rxClosePioneer(IRRecvProIfc *receiver)
    {
        // Check to see if we have a valid previous code and/or
        // a valid new code.
        if (pioneerPrvCode != 0)
        {
            // We have a stashed Pioneer code plus the new one.  If
            // they're different, we must have an extended code with
            // a "shift" prefix.
            if (pioneerPrvCode != code)
            {
                // distinct code - it's an extended code with a shift
                reportPioneerFormat(receiver, pioneerPrvCode, code);
            }
            else
            {
                // same code - it's just a repeat, so report it twice
                reportPioneerFormat(receiver, code);
                reportPioneerFormat(receiver, code);
            }
            
            // we've now consumed the previous code
            pioneerPrvCode = 0;
        }
        else
        {
            // There's no stashed code.  Don't report the new one yet,
            // since it might be a "shift" prefix.  Stash it until we
            // find out if another code follows.
            pioneerPrvCode = code;
            bit = 0;
            code = 0;
        }
    }
    
    // determine if we have Pioneer address
    bool isPioneerCode()
    {
        // pull out the command and address fields fields
        uint8_t c1 = uint8_t((code >> 24) & 0xff);
        uint8_t c0 = uint8_t((code >> 16) & 0xff);
        uint8_t a1 = uint8_t((code >> 8) & 0xff);
        uint8_t a0 = uint8_t(code & 0xff);
            
        // Pioneer uses device codes A0..AF, with A1 complemented, and
        // uses only the 32-bit code format.  Pioneer also always uses
        // a complemented C0-C1 pair.
        return bit == 32 
            && (a0 >= 0xA0 && a0 <= 0xAF) 
            && a0 == uint8_t(~a1)
            && c0 == uint8_t(~c1);        
    }

    // Report a code in Pioneer format.  This takes the first address
    // byte and combines it with the first command byte to form a 16-bit
    // code.  Pioneer writes codes in this format because the second
    // address and command bytes are always the complements of the first,
    // so they contain no information, so it makes the codes more readable
    // for human consumption to drop the redundant bits.  
    void reportPioneerFormat(IRRecvProIfc *receiver, uint32_t code)
    {
        uint8_t a0 = uint8_t(code & 0xff);
        uint8_t c0 = uint8_t((code >> 16) & 0xff);
        reportCode(receiver, IRPRO_PIONEER, (uint64_t(a0) << 8) | c0, 
            bool3::null, bool3::null);
    }
    
    // Report an extended two-part code in Pioneer format.  code1 is the
    // first code received (the "shift" prefix code), and code2 is the
    // second (the key-specific subcode).  We'll convert each code to
    // the standard Pioneer 16-bit format (<address>:<key>), then pack
    // the two into a 32-bit int with the shift code in the high half.
    void reportPioneerFormat(IRRecvProIfc *receiver, uint32_t code1, uint32_t code2)
    {
        uint8_t a1 = code1 & 0xff;
        uint8_t c1 = (code1 >> 16) & 0xff;
        uint8_t a2 = code2 & 0xff;
        uint8_t c2 = (code2 >> 16) & 0xff;
        reportCode(
            receiver, IRPRO_PIONEER,
            (uint64_t(a1) << 24) | (uint64_t(c1) << 16) 
            | (uint64_t(a2) << 8) | c2,
             bool3::null, bool3::null);
    }
   
    // The previous code value.  This is used in decoding Pioneer
    // codes, since some keys in the Pioneer scheme send a "shift"
    // prefix code plus a subcode.
    uint32_t pioneerPrvCode;
};

// NEC-32x.  This is a minor variation on the standard NEC-32 protocol,
// with a slightly different header signature and a different ditto
// pattern.
class IRPNEC_32x: public IRPNEC
{
public:
    virtual int id() const { return IRPRO_NEC32X; }
    virtual const char *name() const { return "NEC32x"; }
    virtual int necPro(int bits) const { return IRPRO_NEC32X; }
    
    virtual int nbits() const { return 32; }
    virtual uint32_t tHeaderMark() const { return 4500; }
    virtual uint32_t tHeaderSpace() const { return 4500; }

    // Close out the code.  NEC-32x has an unusual variation of the
    // NEC ditto: it uses the same header as a regular code, but only
    // has a 1-bit payload.
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        if (bit == 1)
            reportCode(receiver, IRPRO_NEC32X, code, bool3::null, true);
        else
            IRPNEC::rxClose(receiver, ditto);
    }
    
};

// -----------------------------------------------------------------------
// 
// Kaseikyo protocol handler.  Like NEC, this is a quasi industry standard
// used by many companies.  Unlike NEC, it seems to be used consistently
// by just about everyone.  There are only two main variations: a 48-bit
// coding and a 56-bit coding.
//
// For all versions, the first 16 bits in serial order provide an OEM ID.
// We use this to report manufacturer-specific protocol IDs, even though
// the low-level coding is the same for all of them.  Differentiating
// by manufacturer is mostly for cosmetic reasons, so that human users
// looking at learned codes or looking for codes to program will see
// names matching their equipment.  In some cases it's also useful for
// interpreting the internal data fields within the bit string; some
// OEMs use checksums or other fields that clients might want to 
// interpret.
//
class IRPKaseikyo: public IRPSpaceLength<uint64_t>
{
public:
    IRPKaseikyo() { }

    // name and ID
    virtual const char *name() const { return "Kaseikyo"; }
    virtual int id() const { return IRPRO_KASEIKYO48; }
    
    // we handle all of the OEM-specific protocols
    virtual bool isSenderFor(int id) const
    {
        switch (id)
        {
        case IRPRO_KASEIKYO48:
        case IRPRO_KASEIKYO56:
        case IRPRO_DENONK:
        case IRPRO_FUJITSU48:
        case IRPRO_FUJITSU56:
        case IRPRO_JVC48:
        case IRPRO_JVC56:
        case IRPRO_MITSUBISHIK:
        case IRPRO_PANASONIC48:
        case IRPRO_PANASONIC56:
        case IRPRO_SHARPK:
        case IRPRO_TEACK:
            return true;
            
        default:
            return false;
        }
    }
    
    // code boundary parameters
    virtual uint32_t tHeaderMark() const { return 3500; }
    virtual uint32_t tHeaderSpace() const { return 1750; }
    virtual uint32_t minRxGap() const { return 2500; }
    virtual uint32_t txGap(IRTXState *state) const { return 173000; }
    
    // space length coding
    virtual int nbits() const { return 56; }
    virtual int tMark() const { return 420; }
    virtual int tZero() const { return 420; }
    virtual int tOne() const { return 1300; }
    
    // protocol/OEM mappings
    struct OEMMap
    {
        uint16_t oem;
        uint8_t pro;
        uint8_t bits;
    };
    static const OEMMap oemMap[];
    static const int nOemMap;

    // close code reception
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        // it must be a 48-bit or 56-bit code
        if (bit != 48 && bit != 56)
            return;
            
        // pull out the OEM code in the low 16 bits
        int oem = int(code & 0xFFFF);
        
        // Find the protocol based on the OEM
        uint8_t pro = bit == 48 ? IRPRO_KASEIKYO48 : IRPRO_KASEIKYO56;
        for (int i = 0 ; i < nOemMap ; ++i)
        {
            if (oemMap[i].oem == oem && oemMap[i].bits == bit)
            {
                pro = oemMap[i].pro;
                break;
            }
        }
                
        // report the code
        reportCode(receiver, pro, code, bool3::null, bool3::null);
    }

    virtual void codeToBitstream(IRTXState *state)
    {
        // presume no OEM and a 48-bit version of the protocol
        uint16_t oem = 0;
        state->nbits = 48;

        // find the protocol variation in the table
        for (int i = 0 ; i < nOemMap ; ++i)
        {
            if (state->protocolId == oemMap[i].pro)
            {
                state->nbits = oemMap[i].bits;
                oem = oemMap[i].oem;
                break;
            }
        }
        
        // if we found a non-zero OEM code, and it doesn't match the
        // low-order 16 data bits, replace the OEM code in the data
        uint64_t code = state->cmdCode;
        if (oem != 0 && int(code & 0xFFFF) != oem)
            code = (code & ~0xFFFFLL) | oem;
        
        // store the code (with possibly updated OEM coding)
        state->bitstream = code;
    }
};

// -----------------------------------------------------------------------
// 
// Philips RC5 protocol handler.  This (along with RC6) is a quasi industry 
// standard among European CE companies, so this protocol gives us 
// compatibility with many devices from companies besides Philips.
//
// RC5 is a 14-bit Manchester-coded protocol.  '1' bits are encoded as
// low->high transitions.
//
// The 14 bits of the command are internally structured as follows:
//
//   S F T AAAAA CCCCCC
//
// S = "start bit".  Always 1.  We omit this from the reported code
//     since it's always the same.
//
// F = "field bit", which selects a default (1) or extended (0) set of 
//     commands.  Note the reverse sensing, with '1' being the default.
//     This is because this position was a second stop bit in the original
//     code, always set to '1'.  When Philips repurposed the bit as the
//     field code in a later version of the protocol, they used '1' as
//     the default for compatibility with older devices.  We pass the bit
//     through as-is to the universal representation, so be aware that 
//     you might have to flip it to match some published code tables.
//
// T = "toggle bit".  This changes on each successive key press, to allow
//     the receiver to distinguish pressing the same key twice from auto-
//     repeat due to holding down the key.
//
// A = "address", most significant bit first; specifies which type of
//     device the command is for (e.g., "TV", "VCR", etc).  The meanings
//     of the possible numeric values are arbitrarily assigned by Philips; 
//     you can Philips; you can find tables online (e.g., at Wikipedia)
//     with the published assignments.
//
// C = "command", most significant bit first; the command code.  The
//     meaning of the command code varies according to the type of device 
//     in the address field.  Published tables with the standard codes can
//     be found online.
//
// Note that this protocol doesn't have a "header" per se; it just starts
// in directly with the first bit.  As soon as we see a long enough gap,
// we're ready for the start bit.
//
class IRPRC5: public IRPManchester<uint16_t>
{
public:
    IRPRC5() { }
    
    // name and ID
    virtual const char *name() const { return "Philips RC5"; }
    virtual int id() const { return IRPRO_RC5; }
    
    // code parameters
    virtual float pwmPeriod(IRTXState *state) const { return 27.7777778e-6; } // 36kHz
    virtual uint32_t minRxGap() const { return 3600; }
    virtual uint32_t txGap(IRTXState *state) const { return 114000; }
    virtual bool lsbFirst() const { return false; }
    virtual int nbits() const { return 14; }
    
    // RC5 has no header; the start bit immediately follows the gap
    virtual uint32_t tHeaderMark() const { return 0; }
    virtual uint32_t tHeaderSpace() const { return 0; }
    
    // Manchester coding parameters
    virtual int tHalfBit(int bit) const { return 1778/2; }
    
    // After the gap, the start of the next mark is in the middle
    // of the start bit.  A '1' start bit always follows the gap,
    // and a '1' bit is represented by a space-to-mark transition,
    // so the end of the gap is the middle of the start bit.
    virtual void rxReset()
    {
        IRPManchester<uint16_t>::rxReset();
        halfBitPos = 1;
    }

    virtual void codeToBitstream(IRTXState *state)
    {
        // add the start bit and toggle bit to the command code
        state->nbits = nbits();
        state->bitstream = (state->cmdCode & DataMask) | StartMask;
        if (state->toggle)
            state->bitstream |= ToggleMask;
    }
    
    // report the code 
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        // make sure we have the full code
        if (bit == 14)
        {
            // Pull out the toggle bit to report separately, and zero it
            // out in the code word, so that a given key always reports
            // the same code value.  Also zero out the start bit, since
            // it's really just structural.
            reportCode(
                receiver, id(),
                code & ~(StartMask | ToggleMask),
                (code & ToggleMask) != 0, bool3::null);
        }
    }

    // masks for the internal fields
    static const int CmdMask = 0x3F;
    static const int AddrMask = 0x1F << 6;
    static const int ToggleMask = 1 << 11;
    static const int FieldMask = 1 << 12;
    static const int StartMask = 1 << 13;
    static const int DataMask = FieldMask | AddrMask | CmdMask;
};

// -----------------------------------------------------------------------
// 
// RC6 protocol handler.  This (along with RC5) is a quasi industry 
// standard among European CE companies, so this protocol gives us 
// compatibility with many devices from companies besides Philips.
//
// RC6 is a 21-bit Manchester-coded protocol.  '1' bits are coded as
// High->Low transitions.  The bits are nominally structured into
// fields as follows:
//
//   S FFF T AAAAAAAA CCCCCCCC
//
// The fields are:
//
//   S = start bit; always 1.  We omit this from the reported value since
//       it's always the same.
//
//   F = "field".  These bits are used to select different command sets,
//       so they're effectively three additional bits (added as the three
//       most significant bits) for the command code.
//
//   A = "address", specifying the type of device the command is for.
//       This has the same meanings as the address field in RC5.
//
//   C = "command".  The command code, specific to the device type
//       in the address field.
//
// As with all protocols, we don't reproduce the internal field structure
// in the decoded command value; we simply pack all of the bits into a
// 18-bit integer, in the order shown above, field bits at the high end.
// (We omit the start bit, since it's a fixed element that's more properly
// part of the protocol than part of the code.)
//
// Note that this protocol contains an odd exception to normal Manchester 
// coding for the "toggle" bit.  This bit has a period 2X that of the other
// bits.
//
class IRPRC6: public IRPManchester<uint32_t>
{
public:
    IRPRC6() { }

    // name and ID
    virtual const char *name() const { return "Philips RC6"; }
    virtual int id() const { return IRPRO_RC6; }

    // code parameters
    virtual float pwmPeriod(IRTXState *state) const { return 27.7777778e-6; } // 36kHz
    virtual uint32_t tHeaderMark() const { return 2695; }
    virtual uint32_t tHeaderSpace() const { return 895; }
    virtual uint32_t minRxGap() const { return 2650; }
    virtual uint32_t txGap(IRTXState *state) const { return 107000; }
    virtual bool lsbFirst() const { return false; }
    virtual int nbits() const { return 21; }
    
    // Manchester coding parameters
    virtual int spaceToMarkBit() const { return 0; }

    // RC6 has the weird exception to the bit timing in the Toggle bit,
    // which is twice as long as the other bits.  The toggle bit is the
    // 5th bit (bit==4).
    virtual int tHalfBit(int bit) const { return bit == 4 ? 895 : 895/2; }

    // create the bit stream for the command code
    virtual void codeToBitstream(IRTXState *state)
    {
        // add the start bit and toggle bit to the command code
        state->nbits = nbits();
        state->bitstream = (state->cmdCode & DataMask) | StartMask;
        if (state->toggle)
            state->bitstream |= ToggleMask;
    }
    
    // report the code 
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        // make sure we have the full code
        if (bit == nbits())
        {
            // Pull out the toggle bit to report separately, and zero it
            // out in the code word, so that a given key always reports
            // the same code value.  Also clear the start bit, since it's
            // just structural.
            reportCode(
                receiver, id(),
                code & ~(ToggleMask | StartMask), 
                (code & ToggleMask) != 0, bool3::null);
        }
    }

    // field masks
    static const int CmdMask =   0xFF;
    static const int AddrMask =  0xFF << 8;
    static const int ToggleMask = 1 << 16;
    static const int FieldMask = 0x07 << 17;
    static const int StartMask = 1 << 20;
    static const int DataMask =  FieldMask | AddrMask | CmdMask;
};

// -----------------------------------------------------------------------
//
// Ortek MCE remote.  This device uses Manchester coding, with either 16
// or 17 bits, depending on which keys are pressed.  The low-order 5 bits
// of either code are the device code.  The interpretation of the rest of
// the bits depends on the device code.  Bits are sent least-significant
// first (little-endian) for interpretation as integer values.
//
// Device code = 0x14 = Mouse.  This is a 16-bit code, with the fields
// as follows (low bit first):
//
//  DDDDD L R MMMMM CCCC
//
//  D = device code (common field for all codes)
//  L = left-click (1=pressed, 0=not pressed)
//  R = right-click (1=pressed, 0=not pressed)
//  M = mouse movement direction.  This type of device is a mouse stick
//      rather than a mouse, so it gives only the direction of travel rather
//      than X/Y motion in pixels.  There are 15 increments of motion numbered
//      0 to 15, starting with 0 at North (straight up), going clockwise:
//        0 = N
//        1 = NNE
//        2 = NE
//        3 = ENE
//        4 = E
//        5 = ESE
//        6 = SE
//        7 = SSE
//        8 = S
//        9 = SSW
//        10 = SW
//        11 = WSW
//        12 = W
//        13 = WNW
//        14 = NW
//        15 = NNW
//     The MMMMM field contains 0x10 | the value above when a mouse motion
//     direction is pressed, so the codes will be 0x10 (N), 0x11 (NNE), ...,
//     0x1F (NNW).  These are shifted left by two in the reported function
//     code, so you'll actually see 0x40 ... 0x7C.
//  C = checksum
//
// There's no equivalent of the "position" code (see device 0x15 below) for
// the mouse commands, so there's no coding for auto-repeat per se.  The
// remote does let the receiver when the last key is released, though, by
// sending one code word with the L, R, and M bits all set to zero; this
// apparently signifies "key up".  One of these is always sent after the 
// last key is released, but it's not sent between auto-repeated codes,
// so if you hold a key down you'll see a sequence of repeating codes
// for that key (or key combination), followed by one "key up" code when
// you release the last key.
//
// Receivers who interpret these codes will probably want to separate the
// L, R, and M bits and treat them separately, rather than treating any given
// combination as a discrete command.  The reason is that the L and R bits
// can combine with the mouse movement field when a left-click or right-click
// button is held down while the movement keys are pressed.  This can be used
// to perform click-and-drag operations on a GUI.
//
// Device code = 0x15 = MCE buttons.  This is a 17-bit code, with the
// fields as follows (low bit first):
//
//  DDDDD PP FFFFFF CCCC
//
//  D = device code (common field for all codes)
//  P = "position code", for sensing repeats: 00=first, 01=middle, 10=last
//  F = function (key code)
//  C = checksum
//
// The checksum is the 3 + the total number of '1' bits in the other fields
// combined.
//
// We report these codes in our own 16-bit format, with D in the high byte,
// and the function code in the low byte.  For the mouse commands (device 0x14),
// the low byte is (high bit to low bit) 0MMMMMRL.  For the MCE buttons, the
// low byte is the function code (F).  We drop the position code for uniformity
// with other protocols and instead report a synthetic toggle bit, which we
// flip whenever we see a new transmission as signified by P=0.  We don't do
// anything with the toggle bit on mouse commands.
//
class IRPOrtekMCE: public IRPManchester<uint32_t>
{
public:
    IRPOrtekMCE() 
    {
        toggle = 0;
    }

    // name and ID
    virtual const char *name() const { return "OrtekMCE"; }
    virtual int id() const { return IRPRO_ORTEKMCE; }
    
    // code parameters
    virtual float pwmPeriod(IRTXState *state) const { return 25.906736e-6; } // 38.6kHz
    virtual uint32_t tHeaderMark() const { return 4*480; }
    virtual uint32_t tHeaderSpace() const { return 1*480; }
    virtual bool headerSpaceToData() const { return true; }
    virtual uint32_t minRxGap() const { return 32000; }
    virtual uint32_t txGap(IRTXState *state) const { return 40000; }
    
    // We always require a final rep with position code 2, so
    // ask for a minimum of 3 reps (0, 1, 2).
    virtual int txMinReps(IRTXState *) const { return 3; }
    
    // Manchester coding parameters
    virtual int nbits() const { return 17; }
    virtual int spaceToMarkBit() const { return 1; }
    virtual int tHalfBit(int bit) const { return 480; }
    
    // encode the bitstream for a given code
    virtual void codeToBitstream(IRTXState *state)
    {
        // Set the repeat count.  If we're on any repeat > 0 and 
        // the key is still pressed, reset the repeat counter to 1.
        // All repeats are "1" as long as the key is down.  As soon
        // as the key is up, advance directly to "2", even if there
        // was never a "1".  Our txMinRep() count is 2, so this will
        // ensure that we always transmit that last position 2 code,
        // as required by the protocol.
        if (state->rep > 0)
            state->rep = state->pressed ? 1 : 2;

        // Function field and checksum bit position - we'll fill
        // these in momentarily according to the device type.
        uint32_t f;
        int c_shift;

        // check the product code to determine the encoding
        uint32_t cmdcode = uint32_t(state->cmdCode);
        int dev = (int(cmdcode) >> 8) & 0xff;
        if (dev == 0x14)
        {
            // Mouse function:  DDDDD L R MMMMM CCCC
            // There's no position field in the mouse messages, but
            // we always have to send a final message with all bits
            // zeroed.  Do this when our rep count is 2, indicating
            // that this is the final close-out rep.
            if (state->rep == 2)
                f = 0;
            else
                f = cmdcode & 0xff;
                
            // the checksum starts at the 12th bit
            c_shift = 12;
        }
        else if (dev == 0x15)
        {
            // MCE button:  DDDDD PP FFFFFF CCCC
            
            // Set the position field to the rep counter
            int p = state->rep;
            
            // fill in the function fields: PP FFFFFF
            f = (p) | ((cmdcode & 0x3F) << 2);
            
            // the checksum starts at the 13th bit
            c_shift = 13;
        }
        else
        {
            // unknown device code - just transmit the low byte as given
            f = cmdcode & 0xff;
            c_shift = 13;
        }
        
        // construct the bit vector with the device code in the low 5
        // bits and code in the next 7 or 8 bits
        uint32_t bitvec = dev | (f << 5);
            
        // figure the checksum: it's the number of '1' bits in
        // the rest of the fields, plus 3
        uint32_t checksum = 3;
        for (uint32_t v = bitvec ; v != 0 ; checksum += (v & 1), v >>= 1) ;
        
        // construct the bit stream
        state->bitstream = bitvec | (checksum << c_shift);
        state->nbits = c_shift + 4;
    }
        
    // report the code value    
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        // common field masks
        const uint32_t D_mask = 0x001F;   // lowest 5 bits = device code
        
        // pull out the common fields
        uint32_t d = code & D_mask;
        
        // assume no post-toggle
        bool postToggle = false;
        
        // check for the different code types, and pull out the fields
        uint32_t f, c, checkedBits;
        if (bit == 16 && d == 0x14)
        {
            // field masks for the mouse codes
            const int      F_shift = 5;
            const uint32_t F_mask = 0x7f << F_shift;  // next 7 bits
            const int      C_shift = 12;
            const uint32_t C_mask = 0x3F << C_shift;  // top 4 bits

            // separate the fields
            f = (code & F_mask) >> F_shift;
            c = (code & C_mask) >> C_shift;
            checkedBits = code & ~C_mask;
            
            // if the F bits are all zero, take it as a "key up" sequence,
            // so flip the toggle bit next time
            if (f == 0)
                postToggle = true;
        }
        else if (bit == 17 && d == 0x15)
        {
            // 17 bits with device code 0x15 = MCE keyboard commands
            
            // field masks for the keyboard codes
            const int      P_shift = 5;
            const uint32_t P_mask = 0x0003 << P_shift;  // next 2 bits
            const int      F_shift = 7;
            const uint32_t F_mask = 0x3F << F_shift;    // next 6 bits
            const int      C_shift = 13;
            const uint32_t C_mask = 0x0F << C_shift;    // top 4 bits

            // separate the fields
            uint32_t p = (code & P_mask) >> P_shift;
            f = (code & F_mask) >> F_shift;
            c = (code & C_mask) >> C_shift;
            checkedBits = code & ~C_mask;

            /// validate the position code - 0,1,2 are valid, 3 is invalid
            if (p == (0x03 << P_shift))
                return;
                
            // flip the toggle bit if this is the first frame in a group
            // as signified by P=0
            if (p == 0)
                toggle ^= 1;                
        }
        else
        {
            // invalid bit length or device code - reject the code
            return;
        }
            
        // count the '1' bits in the other fields to get the checksum value
        int ones = 0;
        for ( ; checkedBits != 0 ; checkedBits >>= 1)
            ones += (checkedBits & 1);
            
        // check the checksum
        if (c != ones + 3)
            return;
                
        // rearrange the code into our canonical format and report it
        // along with the synthetic toggle
        reportCode(receiver, id(), (d << 8) | f, toggle, bool3::null);
        
        // flip the toggle for next time, if desired
        if (postToggle)
            toggle ^= 1;
    }
    
    // synthetic toggle bit
    uint8_t toggle : 1;
};

// -----------------------------------------------------------------------
// 
// Sony protocol handler.  Sony uses mark-length encoding, with
// 8, 12, 15, and 20 bit code words.  We use a common receiver
// base class that determines how many bits are in the code from
// the input itself, plus separate base classes for each bit size.
// The common receiver class reports the appropriate sub-protocol
// ID for the bit size, so that the appropriate sender class is
// used if we want to transmit the captured code.
//
class IRPSony: public IRPMarkLength<uint32_t>
{
public:
    IRPSony() { }

    // Name and ID.  We handle all of the Sony bit size variations,
    // so we use IRPRO_NONE as our nominal ID, but set the actual code
    // on a successful receive based on the actual bit size.  We
    // transmit any of the Sony codes.
    virtual const char *name() const { return "Sony"; }
    virtual int id() const { return IRPRO_NONE; }
    virtual bool isSenderFor(int protocolId) const
    {
        return protocolId == IRPRO_SONY8
            || protocolId == IRPRO_SONY12
            || protocolId == IRPRO_SONY15
            || protocolId == IRPRO_SONY20;
    }
    
    // code boundary parameters
    virtual uint32_t tHeaderMark() const { return 2400; }
    virtual uint32_t tHeaderSpace() const { return 600; }
    virtual uint32_t minRxGap() const { return 5000; }
    virtual uint32_t txGap(IRTXState *state) const { return 45000; }
    
    // mark-length coding parameters
    virtual int nbits() const { return 20; }  // maximum - can also be 8, 12, or 15
    virtual int tSpace() const { return 600; }
    virtual int tZero() const { return 600; }
    virtual int tOne() const { return 1200; }
    
    // Sony requires at least 3 sends per key press
    virtual int txMinReps(IRTXState *state) const { return 3; }

    // set up the bitstream for a code value    
    virtual void codeToBitstream(IRTXState *state)
    {
        // store the code, and set the bit counter according to
        // the Sony protocol subtype
        state->bitstream = state->cmdCode;
        switch (state->protocolId)
        {
        case IRPRO_SONY8:
            state->nbits = 8;
            break;
            
        case IRPRO_SONY12:
            state->nbits = 12;
            break;
            
        case IRPRO_SONY15:
            state->nbits = 15;
            break;
            
        case IRPRO_SONY20:
        default:
            state->nbits = 20;
            break;
        }
    }
    
    // report a code value
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        // we have a valid code if we have 8, 12, 15, or 20 bits
        switch (bit)
        {
        case 8:
            reportCode(receiver, IRPRO_SONY8, code, bool3::null, bool3::null);
            break;
            
        case 12:
            reportCode(receiver, IRPRO_SONY12, code, bool3::null, bool3::null);
            break;
            
        case 15:
            reportCode(receiver, IRPRO_SONY15, code, bool3::null, bool3::null);
            break;
            
        case 20:
            reportCode(receiver, IRPRO_SONY20, code, bool3::null, bool3::null);
            break;
        }
    }
};

// -----------------------------------------------------------------------
// 
// Denon protocol handler.  Denon uses a 15-bit space-length coding, but 
// has two unusual features.  First, it has no header; it just starts
// right off with the data bits.  Second, every code is transmitted a
// second time, starting 65ms after the start of the first iteration,
// with the "F" and "S" fields inverted:
//
//   DDDDD FFFFFFFF SS
//
//   D = device code
//   F = function code (key)
//   S = sequence; 0 for the first half of the pair, 1 for the second half
//
// The first half-code is transmitted with the actual function code and 
// SS=00; the second half uses ~F (1's complement of the function code)
// and SS=11.
//
// Many learning remotes get this wrong, only learning one or the other
// half.  That's understandable, since learning remotes often only collect
// the raw bit codes and thus wouldn't be able to detect the multi-code
// structure.  It's a little less forgiveable that some pre-programmed
// universal remotes, such as the Logitech Harmony series, also miss the
// half-code nuance.  To be robust against errant remotes, we'll accept
// and report lone half-codes, and we'll invert the F bits if the S bits 
// indicate that a second half was learned, to ensure that the client sees
// the correct version of the codes.  We'll also internally track the
// pairs so that, when we *do* see a properly formed inverted pair, we'll
// only report one code out of the pair to the client.
//
class IRPDenon: public IRPSpaceLength<uint16_t>
{
public:
    IRPDenon() 
    {
        prvCode = 0;
    }

    // name and ID
    virtual const char *name() const { return "Denon"; }
    virtual int id() const { return IRPRO_DENON; }
    
    // Code parameters.  The Denon protocol has no header; it just
    // jumps right in with the first bit.
    virtual uint32_t tHeaderMark() const { return 0; }
    virtual uint32_t tHeaderSpace() const { return 0; }
    virtual uint32_t minRxGap() const { return 30000; }
    
    // use a short gap between paired complemented codes, and a longer
    // gap between repeats
    virtual uint32_t txGap(IRTXState *state) const { return 165000; }
    virtual uint32_t txPostGap(IRTXState *state) const 
    {
        if (state->rep == 0)
        {
            // Even rep - this is the gap between the two halves of a
            // complemented pair.  The next code starts 65ms from the
            // *start* of the last code, so the gap is 65ms minus the
            // transmission time for the last code.
            return 65000 - state->txTime.read_us();
        }
        else
        {
            // odd rep - use the normal long gap
            return 165000; 
        }
    }
    
    // on idle, clear the previous code
    virtual void rxIdle(IRRecvProIfc *receiver)
    {
        prvCode = 0;
    }
    
    // space length coding
    virtual int nbits() const { return 15; }
    virtual int tMark() const { return 264; }
    virtual int tZero() const { return 792; }
    virtual int tOne() const { return 1848; }
    
    // handle a space
    virtual bool rxSpace(IRRecvProIfc *receiver, uint32_t t)
    {
        // If this space is longer than the standard space between
        // adjacent codes, clear out any previous first-half code 
        // we've stored.  Complementary pairs have to be transmitted
        // with the standard inter-code gap between them.  Anything
        // longer must be separate key presses.
        if (t > 65000)
            prvCode = 0;

        // do the normal work
        return IRPSpaceLength<uint16_t>::rxSpace(receiver, t);
    }
    
    // always send twice, once for the base version, once for the
    // inverted follow-up version
    virtual int txMinReps(IRTXState *state) const { return 2; }
    
    // encode the bitstream
    virtual void codeToBitstream(IRTXState *state)
    {
        // If we're on an even repetition, just send the code
        // exactly as given.  If we're on an odd repetition,
        // send the inverted F and S fields.
        state->nbits = 15;
        if (state->rep == 0 || state->rep == 2)
        {
            // even rep - send the base version
            state->bitstream = state->cmdCode & (D_mask | F_mask);
            
            // If we're on rep 2, switch back to rep 0.  This will
            // combine with our minimum rep count of 2 to ensure that
            // we always transmit an even number of copies.  Note that
            // this loop terminates: when the key is up after we finish
            // rep 1, the rep counter will advance to 2 and we'll stop.
            // We'll only reset the rep counter here if we actually
            // enter rep 2, which means the key is still down at that
            // point, which means that we'll have to go at least another
            // two iterations.
            state->rep = 0;
        }
        else
        {
            // odd rep - invert the F field, and use all 1's in the S field
            uint32_t d = state->cmdCode & D_mask;
            uint32_t f = (~state->cmdCode) & F_mask;
            state->bitstream = d | f | S_mask;
        }
    }
            
    // report the code
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        // If this code matches the inverted prior code, we have
        // a matching pair.  Otherwise, hang onto this code until
        // next time.
        if (bit == 15)
        {
            // get the current and previous code's subfields
            uint16_t curD = code & D_mask;
            uint16_t curF = code & F_mask;
            uint16_t curS = code & S_mask;
            uint16_t prvD = prvCode & D_mask;
            uint16_t prvF = prvCode & F_mask;
            uint16_t prvS = prvCode & S_mask;
            
            // check to see if the current FS fields match the inverted
            // prior FS fields, to make up a complementary pair as required
            // by the protocol
            if (curD == prvD && curF == (~prvF & F_mask) && curS == (~prvS & S_mask))
            {
                // The current code is the second half of the first code.
                // Don't report the current code, since it's just an 
                // error-checking copy of the previous code, which we already
                // reported.  We've now seen both halves, so clear the 
                // previous code.
                prvCode = 0;
            }
            else
            {
                // This isn't the second half of an complementary pair, so
                // report it as a separate code.  If the 'S' bits are 1's
                // in this code, it's the second half of the pair, so invert
                // the F and S fields to get the original code.  It might
                // have been sent by a learning remote or universal remote
                // that only captured the second half field.
                if (curS == S_mask)
                {
                    // The S bits are 1's, so it's a second-half code.  We
                    // don't have a matching first-half code, so either the
                    // first half was lost, or it was never transmitted.  In
                    // either case, reconstruct the original first-half code
                    // by inverting the F bits and clearing the S bits.
                    reportCode(receiver, id(), curD | (~curF & F_mask),
                        bool3::null, bool3::null);
                    
                    // Forget any previous code.  This is a second-half
                    // code, so if we do get a first-half code after this,
                    // it's a brand new key press or repetition.
                    prvCode = 0;
                }
                else if (curS == 0)
                {
                    // The S bits are 0, so it's a first-half code.  Report
                    // the code, and save it for next time, so that we can
                    // check the next code to see if it's the second half.
                    reportCode(receiver, id(), code, bool3::null, bool3::null);
                    prvCode = code;
                }
                else
                {
                    // The S bits are invalid, so this isn't a valid code.
                    // Clear out any previous code and reject this one.
                    prvCode = 0;
                }
            }
        }
        else
        {
            // we seem to have an invalid code; clear out any
            // previous code so we can start from scratch
            prvCode = 0;
        }
    }
        
    // stored first code - we hang onto this until we see the
    // inverted second copy, so that we can verify a matching pair
    uint16_t prvCode;
    
    // masks for the subfields
    static const int F_shift = 5;
    static const int S_shift = 13;
    static const uint16_t D_mask = 0x1F;
    static const uint16_t F_mask = 0x00FF << F_shift;
    static const uint16_t S_mask = 0x0003 << S_shift;
};

// -----------------------------------------------------------------------
// 
// Samsung 20-bit protocol handler.  This is a simple space-length 
// encoding.
//
class IRPSamsung20: public IRPSpaceLength<uint32_t>
{
public:
    IRPSamsung20() { }
    
    // name and ID
    virtual const char *name() const { return "Samsung20"; }
    virtual int id() const { return IRPRO_SAMSUNG20; }
    
    // code parameters
    virtual float pwmPeriod(IRTXState *state) const { return 26.0416667e-6; } // 38.4 kHz
    virtual uint32_t tHeaderMark() const { return 8*564; }
    virtual uint32_t tHeaderSpace() const { return 8*564; }
    virtual uint32_t minRxGap() const { return 2*564; }
    virtual uint32_t txGap(IRTXState *state) const { return 118000; }
    
    // space length coding
    virtual int nbits() const { return 20; }
    virtual int tMark() const { return 1*564; }
    virtual int tZero() const { return 1*564; }
    virtual int tOne() const { return 3*564; }
};

// Samsung 36-bit protocol.  This is similar to the NEC protocol,
// with different header timing.
class IRPSamsung36: public IRPSpaceLength<uint32_t>
{
public:
    IRPSamsung36() { }

    // name and ID
    virtual const char *name() const { return "Samsung36"; }
    virtual int id() const { return IRPRO_SAMSUNG36; }
    
    // code parameters
    virtual uint32_t tHeaderMark() const { return 9*500; }
    virtual uint32_t tHeaderSpace() const { return 9*500; }
    virtual uint32_t minRxGap() const { return 40000; }
    virtual uint32_t txGap(IRTXState *state) const { return 118000; }
    
    // space length coding
    virtual int nbits() const { return 36; }
    virtual int tMark() const { return 1*500; }
    virtual int tZero() const { return 1*500; }
    virtual int tOne() const { return 3*500; }    
};

// -----------------------------------------------------------------------
//
// Lutron lights, fans, and home automation.  Lutron uses a simple async
// bit coding: a 2280us space represents '0', a 2280us mark represents '1'.
// Each code consists of 36 bits.  The start of a code word is indicated
// by a space of at least 4*2280us followed by a mark of at least 8*2280us.
// The mark amounts to 8 consecutive '1' bits, which Lutron includes as
// digits in the published codes, so we'll include them in our representation 
// as well (as opposed to treating them as a separate header).

// These raw 36-bit strings use an internal error correction coding.  This
// isn't mentioned in Lutron's technical documentation (they just list the
// raw async bit strings), but there's a description on the Web (see, e.g.,
// hifi-remote.com; the original source is unclear).  According to that,
// you start with a pair of 8-bit values (device code + function).  Append
// two parity bits for even parity in each byte.  This gives us 18 bits.
// Divide the 18 bits into 6 groups of 3 bits.  For each 3-bit group, take
// the binary value (0..7), recode the bits as a reflected Gray code for
// the same number (e.g., '111' binary = 7 = '100' Gray coded), then add
// an even parity bit.  We now have 24 bits.  Concatenate these together
// and sandwich them between the 8 x '1' start bits and the 4 x '0' stop
// bits, and we have the 36-bit async bit stream.
//
// The error correction code lets us apply at least one validation check:
// we can verify that each 4-bit group in the raw data has odd parity.  If
// it doesn't, we'll reject the code.  Ideally, we could also reverse the
// whole coding scheme above and check the two even parity bits for the
// recovered bytes.  Unfortunately, I haven't figured out how to do this;
// taking the Web description of the coding at face value doesn't yield
// correct parity bits for all of the codes published by Lutron.  Either
// the description is wrong, or I'm just misinterpreting it (which is
// likely given that it's pretty sketchy).  
// 
class IRPLutron: public IRPAsync<uint32_t>
{
public:
    IRPLutron() { }

    // name and ID
    virtual const char *name() const { return "Lutron"; }
    virtual int id() const { return IRPRO_LUTRON; }
    
    // carrier is 40kHz
    virtual float pwmPeriod(IRTXState *state) const { return 25.0e-6f; } // 40kHz

    // All codes end with 4 '0' bits, which is as much of a gap as these
    // remotes provide.
    virtual uint32_t minRxGap() const { return 2280*4 - 700; }
    virtual uint32_t txGap(IRTXState *state) const { return 2280*16; }
    
    // Lutron doesn't have a formal header, but there's a long mark at 
    // the beginning of every code.
    virtual uint32_t tHeaderMark() const { return 2280*8; }
    virtual uint32_t tHeaderSpace() const { return 0; }
    
    // Bit code parameters.  The Lutron codes are 36-bits, each bit
    // is 2280us long, and bits are stored MSB first.
    virtual bool lsbFirst() const { return false; }
    virtual int nbits() const { return 24; }
    virtual int tBit() const { return 2280; }
    
    // Validate a received code value, using the decoding process
    // described above.  The code value given here is the low-order
    // 32 bits of the 36-bit code, with the initial FF stripped.
    bool rxValidate(uint32_t c)
    {
        // the low 4 bits must be zeroes
        if ((c & 0x0000000F) != 0)
            return false;
            
        // drop the 4 '0' bits at the end
        c >>= 4;
        
        // parity table for 4-bit values 0x00..0x0F - 0=even, 1=odd
        static const int parity[] = {
            0, 1, 1, 0,     // 0, 1, 2, 3
            1, 0, 0, 1,     // 4, 5, 6, 7
            1, 0, 0, 1,     // 8, 9, A, B
            0, 1, 1, 0      // C, D, E, F
        };
        
        // add up the number of groups with odd parity
        int odds = 0;
        for (int i = 0 ; i < 6 ; ++i, c >>= 4)
            odds += parity[c & 0x0F];
            
        // we need all 6 groups to have odd parity
        return odds == 6;
    }

    // Return the code.  Lutron's code tables have all codes starting
    // with FF (8 consecutive '1' bits), but we treat this as a header,
    // since it can never occur within a code, so it doesn't show up
    // in the bit string.  We also count the tailing four '0' bits,
    // which Lutron also encodes in each string, as a gap.  So we only
    // actually capture 24 data bits.  To make our codes match the
    // Lutron tables, add the structural bits back in for reporting.
    virtual void rxClose(IRRecvProIfc *receiver, bool ditto)
    {
        if (bit == 24)
        {
            uint64_t report = 0xFF0000000LL | (code << 4);
            if (rxValidate(report))
                reportCode(receiver, id(), report, bool3::null, bool3::null);
        }
    }
    
    // For transmission, convert back to the 24 data bits, stripping out
    // the structural leading 8 '1' bits and trailing 4 '0' bits.
    virtual void codeToBitstream(IRTXState *state)
    {
        state->bitstream = (state->cmdCode >> 4) & 0x00FFFFFF;
        state->nbits = 24;
    }
};

// -------------------------------------------------------------------------
//
// Protocol singletons.  We combine these into a container structure
// so that we can allocate the whole set of protocol handlers in one
// 'new'.
//
struct IRProtocols
{
    #define IR_PROTOCOL_RXTX(cls) cls s_##cls;
    #include "IRProtocolList.h"
};

#endif
