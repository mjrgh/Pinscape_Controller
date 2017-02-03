// Pinscape Controller TLC5940 interface
//
// Based on Spencer Davis's mbed TLC5940 library.  Adapted for the
// KL25Z and simplified (removes dot correction and status input 
// support).

 
#ifndef TLC5940_H
#define TLC5940_H

#include "FastPWM.h"

// --------------------------------------------------------------------------
// Data Transmission Mode.
//
// NOTE!  This section contains a possible workaround to try if you're 
// having data signal stability problems with your TLC5940 chips.  If
// things are working properly, you can ignore this part.
//
// The software has two options for sending data updates to the chips:
//
// Mode 0:  Send data *during* the grayscale cycle.  This is the default,
// and it's the standard method the chips are designed for.  In this mode, 
// we start sending an update just after then blanking interval that starts 
// a new grayscale cycle.  The timing is arranged so that the update is 
// completed well before the end of the grayscale cycle.  At the next 
// blanking interval, we latch the new data, so the new brightness levels 
// will be shown starting on the next cycle.
//
// Mode 1:  Send data *between* grayscale cycles.  In this mode, we send
// each complete update during a blanking period, then latch the update
// and start the next grayscale cycle.  This isn't the way the chips were
// intended to be used, but it works.  The disadvantage is that it requires
// the blanking interval to be extended long enough for the full data 
// update (192 bits * the number of chips in the chain).  Since the
// outputs are turned off throughout the blanking period, this reduces
// the overall brightness/intensity of the outputs by reducing the duty
// cycle.  The TLC5940 chips can't achieve 100% duty cycle to begin with,
// since they require a brief minimum time in the blanking interval
// between grayscale cycles; however, the minimum is so short that the
// duty cycle is close to 100%.  With the full data transmission stuffed
// into the blanking interval, we reduce the duty cycle further below
// 100%.  With four chips in the chain, a 28 MHz data clock, and a
// 500 kHz grayscale clock, the reduction is about 0.3%.
//
// Mode 0 is the method documented in the manufacturer's data sheet.
// It works well empirically with the Pinscape expansion boards.
//
// So what's the point of Mode 1?  In early testing, with a breadboard 
// setup, I saw some problems with data signal stability, which manifested 
// as sporadic flickering in the outputs.  Switching to Mode 1 improved
// the signal stability considerably.  I'm therefore leaving this code
// available as an option in case anyone runs into similar signal problems
// and wants to try the alternative mode as a workaround.
//
#define DATA_UPDATE_INSIDE_BLANKING  0

#include "mbed.h"


// --------------------------------------------------------------------------
// Some notes on the data transmission design
//
// I spent a while working on using DMA to send the data, thinking that
// this would reduce the CPU load.  But I couldn't get this working
// reliably; there was some kind of timing interaction or race condition
// that caused crashes when initiating the DMA transfer from within the
// blanking interrupt.  I spent quite a while trying to debug it and
// couldn't figure out what was going on.  There are some complications
// involved in using DMA with SPI that are documented in the KL25Z
// reference manual, and I was following those carefully, but I suspect
// that the problem was somehow related to that, because it seemed to
// be sporadic and timing-related, and I couldn't find any software race
// conditions or concurrency issues that could explain it.
//
// I finally decided that I wasn't going to crack that and started looking
// for alternatives, so out of curiosity, I measured the time needed for a 
// synchronous (CPU-driven) SPI send, to see how it would fit into various
// places in the code.  This turned out to be faster than I expected: with
// SPI at 28MHz, the measured time for a synchronous send is about 72us for
// 4 chips worth of GS data (192 bits), which I expect to be the typical
// Expansion Board setup.  For an 8-chip setup, which will probably be 
// about the maximum workable setup, the time would be 144us.  We only have
// to send the data once per grayscale cycle, and each cycle is 11.7ms with 
// the grayscale clock at 350kHz (4096 steps per cycle divided by 350,000 
// steps per second = 11.7ms per cycle), so this is only 1% overhead.  The 
// main loop spends most of its time polling anyway, so we have plenty of 
// cycles to reallocate from idle polling to the sending the data.
//
// The easiest place to do the send is in the blanking interval ISR, but
// I wanted to keep this out of the ISR.  It's only ~100us, but even so,
// it's critical to minimize time in ISRs so that we don't miss other 
// interrupts.  So instead, I set it up so that the ISR coordinates with
// the main loop via a flag:
//
//  - In the blanking interrupt, set a flag ("cts" = clear to send),
//    and arm a timeout that fires 2/3 through the next blanking cycle
//
//  - In the main loop, poll "cts" each time through the loop.  When 
//    cts is true, send the data synchronously and clear the flag.
//    Do nothing when cts is false.
//
// The main loop runs on about a 1.5ms cycle, and 2/3 of the grayscale
// cycle is 8ms, so the main loop will poll cts on average 5 times per
// 8ms window.  That makes it all but certain that we'll do a send in
// a timely fashion on every grayscale cycle.
//
// The point of the 2/3 window is to guarantee that the data send is
// finished before the grayscale cycle ends.  The TLC5940 chips require
// this; data transmission has to be entirely between blanking intervals.
// The main loop and interrupt handler are operating asynchronously
// relative to one another, so the exact phase alignment will vary
// randomly.  If we start a transmission within the 2/3 window, we're
// guaranteed to have at least 3.5ms (1/3 of the cycle) left before
// the next blanking interval.  The transmission only takes ~100us,
// so we're leaving tons of margin for error in the timing - we have
// 34x longer than we need.
//
// The main loop can easily absorb the extra ~100us of overhead without
// even noticing.  The loop spends most of its time polling devices, so
// it's really mostly idle time to start with.  So we're effectively
// reallocating some idle time to useful work.  The chunk of time is
// only about 6% of one loop iteration, so we're not even significantly
// extending the occasional iterations that actually do this work.
// (If we had a 2ms chunk of monolithic work to do, that could start
// to add undesirable latency to other polling tasks.  100us won't.)
//
// We could conceivably reduce this overhead slightly by adding DMA, 
// but I'm not sure it would actually do much good.  Setting up the DMA
// transfer would probably take at least 20us in CPU time just to set
// up all of the registers.  And SPI is so fast that the DMA transfer
// would saturate the CPU memory bus for the 30us or so of the transfer.
// (I have my suspicions that this bus saturation effect might be part
// of the problem I was having getting DMA working in the first place.)
// So we'd go from 100us of overhead per cycle to at maybe 50us per 
// cycle.  We'd also have to introduce some concurrency controls to the 
// output "set" operation that we don't need with the current scheme 
// (because it's synchronous).  So overall I think the current
// synchronous approach is almost as good in terms of performance as 
// an asynchronous DMA setup would be, and it's a heck of a lot simpler
// and seems very reliable.
//
// --------------------------------------------------------------------------


/**
  * SPI speed used by the mbed to communicate with the TLC5940
  * The TLC5940 supports up to 30Mhz.  It's best to keep this as
  * high as possible, since a higher SPI speed yields a faster 
  * grayscale data update.  However, I've seen some slight
  * instability in the signal in my breadboard setup using the
  * full 30MHz, so I've reduced this slightly, which seems to
  * yield a solid signal.  The limit will vary according to how
  * clean the signal path is to the chips; you can probably crank
  * this up to full speed if you have a well-designed PCB, good
  * decoupling capacitors near the 5940 VCC/GND pins, and short
  * wires between the KL25Z and the PCB.  A short, clean path to
  * KL25Z ground seems especially important.
  *
  * The SPI clock must be fast enough that the data transmission
  * time for a full update is comfortably less than the blanking 
  * cycle time.  The grayscale refresh requires 192 bits per TLC5940 
  * in the daisy chain, and each bit takes one SPI clock to send.  
  * Our reference setup in the Pinscape controller allows for up to 
  * 4 TLC5940s, so a full refresh cycle on a fully populated system 
  * would be 768 SPI clocks.  The blanking cycle is 4096 GSCLK cycles.  
  *
  *   t(blank) = 4096 * 1/GSCLK_SPEED
  *   t(refresh) = 768 * 1/SPI_SPEED
  *   Therefore:  SPI_SPEED must be > 768/4096 * GSCLK_SPEED
  *
  * Since the SPI speed can be so high, and since we want to keep
  * the GSCLK speed relatively low, the constraint above simply
  * isn't a factor.  E.g., at SPI=30MHz and GSCLK=500kHz, 
  * t(blank) is 8192us and t(refresh) is 25us.
  */
#define SPI_SPEED 28000000

/**
  * The rate at which the GSCLK pin is pulsed.   This also controls 
  * how often the reset function is called.   The reset function call
  * interval is (1/GSCLK_SPEED) * 4096.  The maximum reliable rate is
  * around 32Mhz.  It's best to keep this rate as low as possible:
  * the higher the rate, the higher the refresh() call frequency,
  * so the higher the CPU load.  Higher frequencies also make it more
  * challenging to wire the chips for clean signal transmission, so
  * minimizing the clock speed will help with signal stability.
  *
  * The lower bound depends on the application.  For driving lights,
  * the limiting factor is flicker: the lower the rate, the more
  * noticeable the flicker.  Incandescents tend to look flicker-free
  * at about 50 Hz (205 kHz grayscale clock).  LEDs need slightly 
  * faster rates.
  */
#define GSCLK_SPEED    350000

class TLC5940
{
public:
    /**
      *  Set up the TLC5940
      *
      *  @param SCLK - The SCK pin of the SPI bus
      *  @param MOSI - The MOSI pin of the SPI bus
      *  @param GSCLK - The GSCLK pin of the TLC5940(s)
      *  @param BLANK - The BLANK pin of the TLC5940(s)
      *  @param XLAT - The XLAT pin of the TLC5940(s)
      *  @param nchips - The number of TLC5940s (if you are daisy chaining)
      */
    TLC5940(PinName SCLK, PinName MOSI, PinName GSCLK, PinName BLANK, PinName XLAT, int nchips)
        : spi(MOSI, NC, SCLK),
          gsclk(GSCLK),
          blank(BLANK, 1),
          xlat(XLAT),
          nchips(nchips)
    {
        // start up initially disabled
        enabled = false;
        
        // set XLAT to initially off
        xlat = 0;
        
        // Assert BLANK while starting up, to keep the outputs turned off until
        // everything is stable.  This helps prevent spurious flashes during startup.
        // (That's not particularly important for lights, but it matters more for
        // tactile devices.  It's a bit alarming to fire a replay knocker on every
        // power-on, for example.)
        blank = 1;
        
        // Configure SPI format and speed.  The KL25Z only supports 8-bit mode.
        // We nominally need to write the data in 12-bit chunks for the TLC5940
        // grayscale levels, but SPI is ultimately just a bit-level serial format,
        // so we can reformat the 12-bit blocks into 8-bit bytes to fit the 
        // KL25Z's limits.  This should work equally well on other microcontrollers 
        // that are more flexible.  The TLC5940 requires polarity/phase format 0.
        spi.format(8, 0);
        spi.frequency(SPI_SPEED);
        
        // Send out a full data set to the chips, to clear out any random
        // startup data from the registers.  Include some extra bits - there
        // are some cases (such as after sending dot correct commands) where
        // an extra bit per chip is required, and the initial state is 
        // unpredictable, so send extra bits to make sure we cover all bases.  
        // This does no harm; extra bits just fall off the end of the daisy 
        // chain, and since we want all registers initially set to 0, we can 
        // send arbitrarily many extra 0's.
        for (int i = 0 ; i < nchips*25 ; ++i)
            spi.write(0x00);
            
        // do an initial XLAT to latch all of these "0" values into the
        // grayscale registers
        xlat = 1;
        xlat = 0;

        // Allocate our SPI buffer.  The transfer on each cycle is 192 bits per
        // chip = 24 bytes per chip.
        spilen = nchips*24;
        spibuf = new uint8_t[spilen];
        memset(spibuf, 0x00, spilen);
        
        // Configure the GSCLK output's frequency
        gsclk.period(1.0/GSCLK_SPEED);
        
        // we're not yet ready to send new data to the chips
        cts = false;
        
        // we don't need an XLAT signal until we send data
        needXlat = false;
    }
     
    // Global enable/disble.  When disabled, we assert the blanking signal
    // continuously to keep all outputs turned off.  This can be used during
    // startup and sleep mode to prevent spurious output signals from
    // uninitialized grayscale registers.  The chips have random values in
    // their internal registers when power is first applied, so we have to 
    // explicitly send the initial zero levels after power cycling the chips.
    // The chips might not have power even when the KL25Z is running, because
    // they might be powered from a separate power supply from the KL25Z
    // (the Pinscape Expansion Boards work this way).  Global blanking helps
    // us start up more cleanly by suppressing all outputs until we can be
    // reasonably sure that the various chip registers are initialized.
    void enable(bool f)
    {
        // note the new setting
        enabled = f;
        
        // If disabled, apply blanking immediately.  If enabled, do nothing
        // extra; we'll drop the blanking signal at the end of the next 
        // blanking interval as normal.
        if (!f)
        {
            // disable interrupts, since the blanking interrupt writes gsclk too
            __disable_irq();
        
            // turn off the GS clock and assert BLANK to turn off all outputs
            gsclk.write(0);
            blank = 1;

            // done messing with shared data
            __enable_irq();
        }
        
    }
    
    // Start the clock running
    void start()
    {        
        // Set up the first call to the reset function, which asserts BLANK to
        // end the PWM cycle and handles new grayscale data output and latching.
        // The original version of this library uses a timer to call reset
        // periodically, but that approach is somewhat problematic because the
        // reset function itself takes a small amount of time to run, so the
        // *actual* cycle is slightly longer than what we get from counting
        // GS clocks.  Running reset on a timer therefore causes the calls to
        // slip out of phase with the actual full cycles, which causes 
        // premature blanking that shows up as visible flicker.  To get the
        // reset cycle to line up more precisely with a full PWM cycle, it
        // works better to set up a new timer at the end of each cycle.  That
        // organically accounts for the time spent in the interrupt handler.
        // This doesn't result in perfectly uniform timing, since interrupt
        // latency varies slightly on each interrupt, but it does guarantee
        // that the blanking will never be premature - all variation will go
        // into the tail end of the cycle after the 4096 GS clocks.  That
        // might cause some brightness variation, but it won't cause flicker,
        // and in practice any brightness variation from this seems to be too 
        // small to be visible.
        armReset();
    }
    
    // stop the timer
    void stop()
    {
        disarmReset();
    }
    
     /*
      *  Set an output.  'idx' is the output index: 0 is OUT0 on the first
      *  chip, 1 is OUT1 on the first chip, 16 is OUT0 on the second chip
      *  in the daisy chain, etc.  'data' is the brightness value for the
      *  output, 0=off, 4095=full brightness.
      */
    void set(int idx, unsigned short data) 
    {
        // validate the index
        if (idx >= 0 && idx < nchips*16)
        {
#if DATA_UPDATE_INSIDE_BLANKING
            // If we send data within the blanking interval, turn off interrupts while 
            // modifying the buffer, since the send happens in the interrupt handler.
            __disable_irq();
#endif

            // Figure the SPI buffer location of the output we're changing.  The SPI
            // buffer has the packed bit format that we send across the wire, with 12 
            // bits per output, arranged from last output to first output (N = number 
            // of outputs = nchips*16):
            //
            //       byte 0  =  high 8 bits of output N-1
            //            1  =  low 4 bits of output N-1 | high 4 bits of output N-2
            //            2  =  low 8 bits of N-2
            //            3  =  high 8 bits of N-3
            //            4  =  low 4 bits of N-3 | high 4 bits of N-2
            //            5  =  low 8bits of N-4
            //           ...
            //  24*nchips-3  =  high 8 bits of output 1
            //  24*nchips-2  =  low 4 bits of output 1 | high 4 bits of output 0
            //  24*nchips-1  =  low 8 bits of output 0
            //
            // So this update will affect two bytes.  If the output number if even, we're
            // in the high 4 + low 8 pair; if odd, we're in the high 8 + low 4 pair.
            int di = nchips*24 - 3 - (3*(idx/2));
            if (idx & 1)
            {
                // ODD = high 8 | low 4
                spibuf[di]    = uint8_t((data >> 4) & 0xff);
                spibuf[di+1] &= 0x0F;
                spibuf[di+1] |= uint8_t((data << 4) & 0xf0);
            }
            else
            {
                // EVEN = high 4 | low 8
                spibuf[di+1] &= 0xF0;
                spibuf[di+1] |= uint8_t((data >> 8) & 0x0f);
                spibuf[di+2]  = uint8_t(data & 0xff);
            }

#if DATA_UPDATE_INSIDE_BLANKING
            // re-enable interrupts
            __enable_irq();
#endif
        }
    }
    
    // Update the outputs.  In our current implementation, this doesn't do
    // anything, since we send the current state to the chips on every grayscale
    // cycle, whether or not there are updates.  We provide the interface for
    // consistency with other peripheral device interfaces in the main loop,
    // and in case we make any future implementation changes that require some
    // action to carry out an explicit update.
    void update(bool force = false)
    {
    }
    
    // Send updates if ready.  Our top-level program's main loop calls this on
    // every iteration.  This lets us send grayscale updates to the chips in
    // regular application context (rather than in interrupt context), to keep
    // the time in the ISR as short as possible.  We return immediately if
    // we're not within the update window or we've already sent updates for
    // the current cycle.
    void send()
    {
        // if we're in the transmission window, send the data
        if (cts)
        {
            // Write the data to the SPI port.  Note that we go directly
            // to the hardware registers rather than using the mbed SPI
            // class, because this makes the operation about 50% faster.
            // The mbed class checks for input on every byte in case the
            // SPI connection is bidirectional, but for this application
            // it's strictly one-way, so we can skip checking for input 
            // and just blast bits to the output register as fast as 
            // it'll take them.  Before writing the output register 
            // ("D"), we have to check the status register ("S") and see
            // that the Transmit Empty Flag (SPTEF) is set.  The 
            // procedure is: spin until SPTEF s set in "S", write the 
            // next byte to "D", loop until out of bytes.
            uint8_t *p = spibuf;
            for (int i = spilen ; i > 0 ; --i) {
                while (!(SPI0->S & SPI_S_SPTEF_MASK)) ;
                SPI0->D = *p++;
            }
        
            // we've sent new data, so we need an XLAT signal to latch it
            needXlat = true;
            
            // done - we don't need to send again until the next GS cycle
            cts = false;
        }
    }

private:
    // SPI port.  This is master mode, output only, so we only assign the MOSI 
    // and SCK pins.
    SPI spi;

    // SPI transfer buffer.  This contains the live grayscale data, formatted
    // for direct transmission to the TLC5940 chips via SPI.
    uint8_t *volatile spibuf;
    
    // Length of the SPI buffer in bytes.  The native data format of the chips
    // is 12 bits per output = 1.5 bytes.  There are 16 outputs per chip, which
    // comes to 192 bits == 24 bytes per chip.
    uint16_t spilen;
    
    // Dirty: true means that the non-live buffer has new pending data.  False means
    // that the non-live buffer is empty.
    volatile bool dirty;
    
    // Enabled: this enables or disables all outputs.  When this is true, we assert the
    // BLANK signal continuously.
    bool enabled;
    
    // use a PWM out for the grayscale clock - this provides a stable
    // square wave signal without consuming CPU
    FastPWM gsclk;

    // Digital out pins used for the TLC5940
    DigitalOut blank;
    DigitalOut xlat;
    
    // number of daisy-chained TLC5940s we're controlling
    int nchips;

    // Timeout to end each PWM cycle.  This is a one-shot timer that we reset
    // on each cycle.
    Timeout resetTimer;
    
    // Timeout to end the data window for the PWM cycle.
    Timeout windowTimer;
    
    // "Clear To Send" flag: 
    volatile bool cts;
    
    // Do we need an XLAT signal on the next blanking interval?
    volatile bool needXlat;
        
    // Reset the grayscale cycle and send the next data update
    void reset()
    {
        // start the blanking cycle
        startBlank();
        
        // we're now clear to send the new GS data
        cts = true;
        
#if DATA_UPDATE_INSIDE_BLANKING
        // We're configured to send the new GS data inline during each 
        // blanking cycle.  Send it now.
        send();
#else
        // We're configured to send GS data during the GS cycle.  This means
        // we can defer the GS data transmission to any point within the next
        // GS cycle, which will last about 12ms (assuming a 350kHz GS clock).
        // That's a ton of time given that our GS transmission only takes about
        // 100us.  With such a leisurely time window to work with, we can move
        // the transmission out of the ISR context and into regular application
        // context, which is good because it greatly reduces the time we spend 
        // in this ISR, which is good in turn because more ISR time means more 
        // latency for other interrupts and more chances to miss interrupts
        // entirely.  
        //
        // The mechanism for deferring the transmission to application context
        // is simple.  The main program loop periodically polls the "cts" flag
        // and transmits the data if it finds "cts" set.  To conform to the
        // hardware spec for the TLC5940 chips, the data transmission has to
        // finish before the next blanking interval.  This means our time 
        // window to do the transmission is the 12ms of the grayscale cycle 
        // minus the ~100us to do the transmission.  So basically 12ms.  
        // Timing is never exact on the KL25Z, though, so we should build in
        // a little margin for error.  To be conservative, we'll say that the 
        // update must begin within the first 2/3 of the grayscale cycle time.
        // That's an 8ms window, and leaves a 4ms margin of error.  It's
        // almost inconceivable that any of the timing factors would be 
        // outside of those bounds.
        //
        // To coordinate this 2/3-of-a-cycle window with the main loop, set
        // up a timeout to clear the "cts" flag 2/3 into the cycle time.  If
        // for some reason the main loop doesn't do the transmission before
        // this timer fires, it'll see the "cts" flag turned off and won't
        // attempt the transmission on this round.  (That should essentially
        // never happen, but it wouldn't be a problem even if it happened with
        // some regularity, because we'd just transmit the data on the next
        // cycle.)
        windowTimer.attach_us(this, &TLC5940::closeSendWindow, 
            uint32_t((1.0f/GSCLK_SPEED)*4096.0f*2.0f/3.0f*1000000.0f));
#endif

        // end the blanking interval
        endBlank();

        // re-arm the reset handler for the next blanking interval
        armReset();
    }
    
    // End the data-send window.  This is a timeout routine that fires halfway
    // through each grayscale cycle.  The TLC5940 chips allow new data to be
    // sent at any time during the grayscale pulse cycle, but the transmission
    // has to fit into this window.  We do these transmissions from the main loop,
    // so that they happen in application context rather than interrupt context,
    // but this means that we have to synchronize the main loop activity to the
    // grayscale timer cycle.  To make sure the transmission is done before the
    // next grayscale cycle ends, we only allow the transmission to start for
    // the first 2/3 of the cycle.  This gives us plenty of time to send the
    // data and plenty of padding to make sure we don't go too late.  Consider
    // the relative time periods: we run the grayscale clock at 350kHz, and each
    // grayscale cycle has 4096 steps, so each cycle takes 11.7ms.  For the
    // typical Expansion Board setup with 4 TLC5940 chips, we have 768 bits 
    // to send via SPI at 28 MHz, which nominally takes 27us.  The actual
    // measured time to send 768 bits via send() is 72us, so there's CPU overhead 
    // of about 2.6x.  The biggest workable Expnasion Board setup would probably 
    // be around 8 TLC chips, so we'd have twice the bits and twice the 
    // transmission time of our 4-chip scenario, so the send time would be
    // about 150us.  2/3 of the grayscale cycle gives us an 8ms window to 
    // perform a 150us operation.  The main loop runs about every 1.5ms, so 
    // we're all but certain to poll CTS more than once during each 8ms window.  
    // Even if we start at the very end of the window, we still have about 3.5ms 
    // to finish a <150us operation, so we're all but certain to finish in time.
    void closeSendWindow() 
    { 
        cts = false; 
    }
    
    // arm the reset handler - this fires at the end of each GS cycle    
    void armReset()
    {
        resetTimer.attach_us(this, &TLC5940::reset, 
            uint32_t((1.0/GSCLK_SPEED)*4096.0*1000000.0f));
    }
    
    void disarmReset()
    {
        resetTimer.detach();
    }

    void startBlank()
    {
        // turn off the grayscale clock, and assert BLANK to end the grayscale cycle
        gsclk.write(0);
        blank = (enabled ? 1 : 0);  // for the slight delay (20ns) required after GSCLK goes low
        blank = 1;        
    }
            
    void endBlank()
    {
        // if we've sent new grayscale data since the last blanking
        // interval, latch it by asserting XLAT
        if (needXlat)
        {
            // latch the new data while we're still blanked
            xlat = 1;
            xlat = 0;
            needXlat = false;
        }

        // End the blanking interval and restart the grayscale clock.  Note
        // that we keep the blanking on if the chips are globally disabled.
        if (enabled)
        {
            blank = 0;
            gsclk.write(.5);
        }
    }
};
 
#endif
