// Fast Interrupt In for KL25Z
//
// This is a replacement for the mbed library InterruptIn class, which
// sets up GPIO ports for edge-sensitive interrupt handling.  This class
// provides the same API but has a shorter code path for responding to
// each interrupt.  In my tests, the mbed InterruptIn class has a maximum
// interrupt rate of about 112kHz; this class can increase that to about
// 181kHz.
//
// If speed is critical (and it is, because why else would you be using 
// this class?), you should elevate the GPIO interrupt priority in the
// hardware interrupt controller so that GPIO pin signals can preempt other
// interrupt handlers.  The mbed USB and timer handlers in particular spend
// relative long periods in interrupt context, so if these are at the same
// or higher priority than the GPIO interrupts, they'll become the limiting
// factor.  The mbed library leaves all interrupts set to maximum priority
// by default, so to elevate the GPIO interrupt priority, you have to lower 
// the priority of everything else.  Call FastInterruptIn::elevatePriority()
// to do this.
//
//
// Performance measurements:  I set up a test program using one KL25Z to
// send 50% duty cycle square wave signals to a second KL25Z (using a PWM
// output on the sender), and measured the maximum interrupt frequency
// where the receiver could correctly count every edge, repeating the test
// with FastInterruptIn and the mbed InterruptIn.  I tested with handlers
// for both edges and handlers for single edges (just rise() or just fall()).
// The Hz rates reflect the maximum *interrupt* frequency, which is twice
// the PWM frequency when testing with handlers for both rise + fall in
// effect.  In all cases, the user callbacks were minimal code paths that
// just incremented counters, and all tests ran with PTA/PTD at elevated
// IRQ priority.  The time per interrupt values shown are the inverse of
// the maximum frequency; these reflect the time between interrupts at
// the corresponding frequency.  Since each frequency is the maximum at
// which that class can handle every interrupt without losing any, the
// time between interrupts tells us how long the CPU takes to fully process
// one interrupt and return to the base state where it's able to handle the
// next one.  This time is the sum of the initial CPU interrupt latency
// (the time it takes from an edge signal occuring on a pin to the CPU
// executing the first instruction of the IRQ vector), the time spent in
// the InterruptIn or FastInterruptIn code, the time spent in the user
// callback, and the time for the CPU to return from the interrupt to
// normal context.  For the test program, the user callback is about 4
// instructions, so perhaps 6 clocks or 360ns.  Other people have measured
// the M0+ initial interrupt latency at about 450ns, and the return time
// is probably similar.  So we have about 1.2us in fixed overhead and user
// callback time, hence the rest is the time spent in the library code.
//
//   mbed InterruptIn:
//     max rate 112kHz
//     -> 8.9us per interrupt 
//        less 1.2us fixed overhead = 7.7us in library code
//
//   FastInterruptIn:
//     max rate 181kHz
//     -> 5.5us per interrupt
//        less 1.2us fixed overhead = 3.3us in library code
// 
//
// Limitations:
//
// 1. KL25Z ONLY.  This is a bare-metal KL25Z class.
//
// 2. Globally incompatible with InterruptIn.  Both classes take over the
// IRQ vectors for the GPIO interrupts globally, so they can't be mixed
// in the same system.  If you use this class anywhere in a program, it
// has to be used exclusively throughout the whole program - don't use
// the mbed InterruptIn anywhere in a program that uses this class.
//
// 3. API differences.  The API is very similar to InterruptIn's API,
// but we don't support the method-based rise/fall callback attachers.  We
// instead use static function pointers (void functions with 'void *'
// context arguments).  It's easy to write static methods for these that 
// dispatch to regular member functions, so the functionality is the same; 
// it's just a little different syntax.  The simpler (in the sense of
// more primitive) callback interface saves a little memory and is
// slightly faster than the method attachers, since it doesn't require
// any variation checks at interrupt time.
//
// Theory of operation
//
// How the mbed code works
// On every interrupt event, the mbed library's GPIO interrupt handler
// searches for a port with an active interrupt.  Each PORTx_IRQn vector
// handles 32 ports, so each handler has to search this space of 32 ports
// for an active interrupt.  The mbed code approaches this problem by
// searching for a '1' bit in the ISFR (interrupt status flags register),
// which is effectively a 32-bit vector of bits indicating which ports have
// active interrupts.  This search could be done quickly if the hardware
// had a "count leading zeroes" instruction, which actually does exist in
// the ARM instruction set, but alas not in the M0+ subset.  So the mbed
// code has to search for the bit by other means.  It accomplishes this by
// way of a binary search.  By my estimate, this takes about 110 clocks or
// 7us.  The routine has some other slight overhead dispatching to the
// user callback once one is selected via the bit search, but the bulk of
// the time is spent in the bit search.  The mbed code could be made more
// efficient by using a better 'count leading zeroes' algorithm; there are 
// readily available implementations that run in about 15 clocks on M0+.
//
// How this code works
// FastInterruptIn takes a different approach that bypasses the bit vector
// search.  We instead search the installed handlers.  We work on the 
// assumption that the total number of interrupt handlers in the system is 
// small compared with the number of ports.  So instead of searching the 
// entire ISFR bit vector, we only check the ports with installed handlers.
//
// The mbed code takes essentially constant time to run.  It doesn't have
// any dependencies (that I can see) on the number of active InterruptIn
// pins.  In contrast, FastInterruptIn's run time is linear in the number
// of active pins: adding more pins will increase the run time.  This is
// a tradeoff, obviously.  It's very much the right tradeoff for the Pinscape 
// system, because we have very few interrupt pins overall.  I suspect it's
// the right tradeoff for most systems, too, since most embedded systems
// have a small fixed set of peripherals they're talking to.
//
// We have a few other small optimizations to maximize our sustainable
// interrupt frequency.  The most important is probably that we read the
// port pin state immediately on entry to the IRQ vector handler.  Since
// we get the same interrupt on a rising or falling edge, we have to read
// the pin state to determine which type of transition triggered the
// interrupt.  This is inherently problematic because the pin state could 
// have changed between the time the interrupt occurred and the time we 
// got around to reading the state - the likelihood of this increases as
// the interrupt source frequency increases.  The soonest we can possibly
// read the state is at entry to the IRQ vector handler, so we do that.
// Even that isn't perfectly instantaneous, due to the unavoidable 450ns
// or so latency in the hardware before the vector code starts executing;
// it would be better if the hardware read the state at the moment the
// interrupt was triggered, but there's nothing we can do about that.
// In contrast, the mbed code waits until after deciding which interrupt
// is active to read the port, so its reading is about 7us delayed vs our
// 500ns delay.  That further reduces the mbed code's ability to keep up
// with fast interrupt sources when both rise and fall handlers are needed.


#ifndef _FASTINTERRUPTIN_H_
#define _FASTINTERRUPTIN_H_

#include "mbed.h"
#include "gpio_api.h"

struct fiiCallback
{
    fiiCallback() { func = 0; }
    void (*func)(void *);
    void *context;
    
    inline void call() { func(context); }
};

class FastInterruptIn
{
public:
    // Globally elevate the PTA and PTD interrupt priorities.  Since the
    // mbed default is to start with all IRQs at maximum priority, we
    // LOWER the priority of all IRQs to the minimum, then raise the PTA
    // and PTD interrupts to maximum priority.  
    //
    // The reason we set all priorities to minimum (except for PTA and PTD) 
    // rather than some medium priority is that this is the most flexible
    // default.  It really should have been the mbed default, in my opinion,
    // since (1) it doesn't matter what the setting is if they're all the
    // same, so an mbed default of 3 would have been equivalent to an mbed
    // default of 0 (the current one) for all programs that don't make any
    // changes anyway, and (2) the most likely use case for programs that
    // do need to differentiate IRQ priorities is that they need one or two
    // items to respond MORE quickly.  It seems extremely unlikely that
    // anyone would need only one or two to be especially slow, which is
    // effectively the case the mbed default is optimized for.
    //
    // This should be called (if desired at all) once at startup.  The 
    // effect is global and permanent (unless later changes are made by
    // someone else), so there's no need to call this again when setting
    // up new handlers or changing existing handlers.  Callers are free to 
    // further adjust priorities as needed (e.g., elevate the priority of
    // some other IRQ), but that should be done after calling this, since we
    // change ALL IRQ priorities with prejudice.
    static void elevatePriority()
    {
        // Set all IRQ priorities to minimum.  M0+ has priority levels
        // 0 (highest) to 3 (lowest).  (Note that the hardware uses the
        // high-order two bits of the low byte, so the hardware priority
        // levels are 0x00 [highest], 0x40, 0x80, 0xC0 [lowest]).  The
        // mbed NVIC macros, in contrast, abstract this to use the LOW
        // two bits, for levels 0, 1, 2, 3.)
        for (int irq = 0 ; irq < 32 ; ++irq)
            NVIC_SetPriority(IRQn(irq), 0x3);
            
        // set the PTA and PTD IRQs to highest priority
        NVIC_SetPriority(PORTA_IRQn, 0x00);
        NVIC_SetPriority(PORTD_IRQn, 0x00);
    }
    
    // set up a FastInterruptIn handler on a given pin
    FastInterruptIn(PinName pin)
    {
        // start with the null callback
        callcb = &FastInterruptIn::callNone;
        
        // initialize the pin as a GPIO Digital In port
        gpio_t gpio;
        gpio_init_in(&gpio, pin);

        // get the port registers
        PDIR = gpio.reg_in;
        pinMask = gpio.mask;
        portno = uint8_t(pin >> PORT_SHIFT);
        pinno = uint8_t((pin & 0x7F) >> 2);
        
        // set up for the selected port
        IRQn_Type irqn;
        void (*vector)();
        switch (portno)
        {
        case PortA:
            irqn = PORTA_IRQn;
            vector = &PortA_ISR;
            PDIR = &FPTA->PDIR;
            break;
        
        case PortD:
            irqn = PORTD_IRQn;
            vector = &PortD_ISR;
            PDIR = &FPTD->PDIR;
            break;
        
        default:
            error("FastInterruptIn: invalid pin specified; "
                "only PTAxx and PTDxx pins are interrupt-capable");
            return;
        }
        
        // set the vector
        NVIC_SetVector(irqn, uint32_t(vector));
        NVIC_EnableIRQ(irqn);
    }
    
    // read the current pin status - returns 1 or 0
    int read() const { return (fastread() >> pinno) & 0x01; }
    
    // Fast read - returns the pin's port bit, which is '0' or '1' shifted
    // left by the port number (e.g., PTA7 or PTD7 return (1<<7) or (0<<7)).
    // This is slightly faster than read() because it doesn't normalize the
    // result to a literal '0' or '1' value.  When the value is only needed
    // for an 'if' test or the like, zero/nonzero is generally good enough,
    // so you can save a tiny bit of time by skiping the shift.
    uint32_t fastread() const { return *PDIR & pinMask; }
    
    // set a rising edge handler
    void rise(void (*func)(void *), void *context = 0)
    {
        setHandler(&cbRise, PCR_IRQC_RISING, func, context);
    }
    
    // set a falling edge handler
    void fall(void (*func)(void *), void *context = 0)
    {
        setHandler(&cbFall, PCR_IRQC_FALLING, func, context);
    }
    
    // Set the pull mode.  Note that the KL25Z only supports PullUp
    // and PullNone modes.  We'll ignore other modes.
    void mode(PinMode pull)
    {
        volatile uint32_t *PCR = &(portno == PortA ? PORTA : PORTD)->PCR[pinno];
        switch (pull)
        {
        case PullNone:
            *PCR &= ~PORT_PCR_PE_MASK;
            break;
            
        case PullUp:
            *PCR |= PORT_PCR_PE_MASK;
            break;
        }
    }
    
protected:
    // set a handler - the mode is PCR_IRQC_RISING or PCR_IRQC_FALLING
    void setHandler(
        fiiCallback *cb, uint32_t mode, void (*func)(void *), void *context)
    {
        // get the PCR (port control register) for the pin
        volatile uint32_t *PCR = &(portno == PortA ? PORTA : PORTD)->PCR[pinno];

        // disable interrupts while messing with shared statics
        __disable_irq();

        // set the callback
        cb->func = func;
        cb->context = context;
        
        // enable or disable the mode in the PCR
        if (func != 0)
        {
            // Handler function is non-null, so we're setting a handler.
            // Enable the mode in the PCR.  Note that we merely need to
            // OR the new mode bits into the existing mode bits, since
            // disabled is 0 and BOTH is equal to RISING|FALLING.
            *PCR |= mode;
            
            // if we're not already in the active list, add us
            listAdd();
        }
        else
        {
            // Handler function is null, so we're clearing the handler.
            // Disable the mode bits in the PCR.  If the old mode was
            // the same as the mode we're disabling, switch to NONE.
            // If the old mode was BOTH, switch to the mode we're NOT
            // disabling.  Otherwise make no change.
            int cur = *PCR & PORT_PCR_IRQC_MASK;
            if (cur == PCR_IRQC_BOTH)
            {
                *PCR &= ~PORT_PCR_IRQC_MASK;
                *PCR |= (mode == PCR_IRQC_FALLING ? PCR_IRQC_RISING : PCR_IRQC_FALLING);
            }
            else if (cur == mode)
            {
                *PCR &= ~PORT_PCR_IRQC_MASK;
            }
            
            // if we're disabled, remove us from the list
            if ((*PCR & PORT_PCR_IRQC_MASK) == PCR_IRQC_DISABLED)
                listRemove();
        }
        
        // set the appropriate callback mode
        if (cbRise.func != 0 && cbFall.func != 0)
        {
            // They want to be called on both Rise and Fall events. 
            // The hardware triggers the same interrupt on both, so we
            // need to distinguish which is which by checking the current
            // pin status when the interrupt occurs.
            callcb = &FastInterruptIn::callBoth;
        }
        else if (cbRise.func != 0)
        {
            // they only want Rise events
            callcb = &FastInterruptIn::callRise;
        }
        else if (cbFall.func != 0)
        {
            // they only want Fall events
            callcb = &FastInterruptIn::callFall;
        }
        else
        {
            // no events are registered
            callcb = &FastInterruptIn::callNone;
        }
        
        // done messing with statics
        __enable_irq();
    }
    
    // add me to the active list for my port
    void listAdd()
    {
        // figure the list head
        FastInterruptIn **headp = (portno == PortA) ? &headPortA : &headPortD;
        
        // search the list to see if I'm already there
        FastInterruptIn **nxtp = headp;
        for ( ; *nxtp != 0 && *nxtp != this ; nxtp = &(*nxtp)->nxt) ;
        
        // if we reached the last entry without finding me, add me
        if (*nxtp == 0)
        {
            *nxtp = this;
            this->nxt = 0;
        }
    }
    
    // remove me from the active list for my port
    void listRemove()
    {
        // figure the list head
        FastInterruptIn **headp = (portno == PortA) ? &headPortA : &headPortD;
        
        // find me in the list
        FastInterruptIn **nxtp = headp;
        for ( ; *nxtp != 0 && *nxtp != this ; nxtp = &(*nxtp)->nxt) ;
        
        // if we found me, unlink me
        if (*nxtp == this)
        {
            *nxtp = this->nxt;
            this->nxt = 0;
        }
    }
    
    // next link in active list for our port
    FastInterruptIn *nxt;
    
    // pin mask - this is 1<<pinno, used for selecting or setting the port's
    // bit in the port-wide bit vector registers (IFSR, PDIR, etc)
    uint32_t pinMask;
    
    // Internal interrupt dispatcher.  This is set to one of 
    // &callNone, &callRise, &callFall, or &callBoth, according 
    // to which type of handler(s) we have registered.
    void (*callcb)(FastInterruptIn *, uint32_t pinstate);
    
    // PDIR (data read) register
    volatile uint32_t *PDIR;
    
    // port and pin number
    uint8_t portno;
    uint8_t pinno;

    // user interrupt handler callbacks
    fiiCallback cbRise;
    fiiCallback cbFall;
    
protected:
    static void callNone(FastInterruptIn *f, uint32_t pinstate) { }
    static void callRise(FastInterruptIn *f, uint32_t pinstate) { f->cbRise.call(); }
    static void callFall(FastInterruptIn *f, uint32_t pinstate) { f->cbFall.call(); }
    static void callBoth(FastInterruptIn *f, uint32_t pinstate)
    {
        if (pinstate)
            f->cbRise.call();
        else
            f->cbFall.call();
    }
    
    // Head of active interrupt handler lists.  When a handler is
    // active, we link it into this static list.  At interrupt time,
    // we search the list for an active interrupt.
    static FastInterruptIn *headPortA;
    static FastInterruptIn *headPortD;

    // PCR_IRQC modes
    static const uint32_t PCR_IRQC_DISABLED = PORT_PCR_IRQC(0);
    static const uint32_t PCR_IRQC_RISING = PORT_PCR_IRQC(9);
    static const uint32_t PCR_IRQC_FALLING = PORT_PCR_IRQC(10);
    static const uint32_t PCR_IRQC_BOTH = PORT_PCR_IRQC(11);
    
    // IRQ handlers.  We set up a separate handler for each port to call
    // the common handler with the port-specific parameters.  
    // 
    // We read the current pin input status immediately on entering the 
    // handler, so that we have the pin reading as soon as possible after 
    // the interrupt.  In cases where we're handling both rising and falling
    // edges, the only way to tell which type of edge triggered the interrupt
    // is to look at the pin status, since the same interrupt is generated
    // in either case.  For a high-frequency signal source, the pin state
    // might change again very soon after the edge that triggered the
    // interrupt, so we can get the wrong state if we wait too long to read
    // the pin.  The soonest we can read the pin is at entry to our handler,
    // which isn't even perfectly instantaneous, since the hardware has some
    // latency (reportedly about 400ns) responding to an interrupt.  
    static void PortA_ISR() { ISR(&PORTA->ISFR, headPortA, FPTA->PDIR); }
    static void PortD_ISR() { ISR(&PORTD->ISFR, headPortD, FPTD->PDIR); }
    inline static void ISR(volatile uint32_t *pifsr, FastInterruptIn *f, uint32_t pdir)
    {
        // search the list for an active entry
        uint32_t ifsr = *pifsr;
        for ( ; f != 0 ; f = f->nxt)
        {
            // check if this entry's pin is in interrupt state
            if ((ifsr & f->pinMask) != 0)
            {
                // clear the interrupt flag by writing '1' to the bit
                *pifsr = f->pinMask;
                
                // call the appropriate user callback
                f->callcb(f, pdir & f->pinMask);
                
                // Stop searching.  If another pin has an active interrupt,
                // or this pin already has another pending interrupt, the
                // hardware will immediately call us again as soon as we
                // return, and we'll find the new interrupt on that new call.
                // This should be more efficient on average than checking all 
                // pins even after finding an active one, since in most cases 
                // there will only be one interrupt to handle at a time.
                return;
            }                
        }
    }
    
};

#endif
