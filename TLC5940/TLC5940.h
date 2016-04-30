// Pinscape Controller TLC5940 interface
//
// Based on Spencer Davis's mbed TLC5940 library.  Adapted for the
// KL25Z and modified to use SPI with DMA to transmit data.  The DMA
// scheme results in greatly reduced CPU load.  This version is also
// simplified to remove dot correction and status input support, which
// the Pinscape Controller app doesn't use.

 
#ifndef TLC5940_H
#define TLC5940_H

#include "FastPWM.h"

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
#include "SimpleDMA.h"
#include "DMAChannels.h"


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
    uint64_t spi_total_time;//$$$
    uint32_t spi_runs;//$$$

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
        : sdma(DMAch_TLC5940),
          spi(MOSI, NC, SCLK),
          gsclk(GSCLK),
          blank(BLANK),
          xlat(XLAT),
          nchips(nchips)
    {
        spi_total_time = 0; spi_runs = 0; // $$$
        
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
        
        // Configure SPI format and speed.  Note that KL25Z ONLY supports 8-bit
        // mode.  The TLC5940 nominally requires 12-bit data blocks for the
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

        // Allocate our DMA buffers.  The transfer on each cycle is 192 bits per
        // chip = 24 bytes per chip.  Allocate two buffers, so that we have a
        // stable buffer that we can send to the chips, and a separate working
        // copy that we can asynchronously update.
        dmalen = nchips*24;
        livebuf = new uint8_t[dmalen*2];
        memset(livebuf, 0x00, dmalen*2);
        
        // start with buffer 0 live, with no new data pending
        workbuf = livebuf + dmalen;
        dirty = false;

        // Set up the Simple DMA interface object.  We use the DMA controller to
        // send grayscale data updates to the TLC5940 chips.  This lets the CPU
        // keep running other tasks while we send gs updates, and importantly
        // allows our blanking interrupt handler return almost immediately.
        // The DMA transfer is from our internal DMA buffer to SPI0, which is
        // the SPI controller physically connected to the TLC5940s.
        SPI0->C2 &= ~SPI_C2_TXDMAE_MASK;
        sdma.attach(this, &TLC5940::dmaDone);
        sdma.destination(&SPI0->D, false, 8);
        sdma.trigger(Trigger_SPI0_TX);
        
        // Configure the GSCLK output's frequency
        gsclk.period(1.0/GSCLK_SPEED);
        
        // mark that we need an initial update
        forceUpdate = true;
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
        
        // if disabled, apply blanking immediately
        if (!f)
        {
            gsclk.write(0);
            blank = 1;
        }
        
        // do a full update with the new setting
        forceUpdate = true;
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
    
     /*
      *  Set an output
      */
    void set(int idx, unsigned short data) 
    {
        // validate the index
        if (idx >= 0 && idx < nchips*16)
        {
            // If the buffer isn't dirty, it means that the previous working buffer
            // was swapped into the live buffer on the last blanking interval.  This
            // means that the working buffer hasn't been updated to the live data yet,
            // so we need to copy it now.
            //
            // If 'dirty' is false, it can't change to true asynchronously - it can
            // only transition from false to true in application (non-ISR) context.
            // If it's true, though, the interrupt handler can change it to false
            // asynchronously, and can also swap the 'live' and 'work' buffer pointers.
            // This means we must do the whole update atomically if 'dirty' is true.
            __disable_irq();
            if (!dirty) 
            {
                // Buffer is clean, so the interrupt handler won't touch 'dirty'
                // or the live/work buffer pointers.  This means we can do the
                // rest of our work with interrupts on.
                __enable_irq();
                
                // get the current live data into our work buffer
                memcpy(workbuf, livebuf, dmalen);
            }

            // Figure the DMA buffer location of the output we're changing.  The DMA 
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
                workbuf[di]    = uint8_t((data >> 4) & 0xff);
                workbuf[di+1] &= 0x0F;
                workbuf[di+1] |= uint8_t((data << 4) & 0xf0);
            }
            else
            {
                // EVEN = high 4 | low 8
                workbuf[di+1] &= 0xF0;
                workbuf[di+1] |= uint8_t((data >> 8) & 0x0f);
                workbuf[di+2]  = uint8_t(data & 0xff);
            }
            
            // if we weren't dirty before, we are now
            if (!dirty)
            {
                // we need an update
                dirty = true;
            }
            else
            {            
                // The buffer was already dirty, so we had to write the buffer with
                // interrupts off.  We're done, so we can re-enable interrupts now.
                __enable_irq();
            }
        }
    }
    
    // Update the outputs.  We automatically update the outputs on the grayscale timer
    // when we have pending changes, so it's not necessary to call this explicitly after 
    // making a change via set().  This can be called to force an update when the chips
    // might be out of sync with our internal state, such as after power-on.
    void update(bool force = false)
    {
        if (force)
            forceUpdate = true;
    }

private:
    // current level for each output
    unsigned short *gs;
    
    // Simple DMA interface object
    SimpleDMA sdma;

    // DMA transfer buffers - double buffer.  Each time we have data to transmit to the 
    // TLC5940 chips, we format the data into the working half of this buffer exactly as 
    // it will go across the wire, then hand the buffer to the DMA controller to move 
    // through the SPI port.  This memory block is actually two buffers, one live and 
    // one pending.  When we're ready to send updates to the chips, we swap the working
    // buffer into the live buffer so that we can send the latest updates.  We keep a
    // separate working copy so that our live copy is stable, so that we don't alter
    // any data in the midst of an asynchronous DMA transmission to the chips.
    uint8_t *volatile livebuf;
    uint8_t *volatile workbuf;
    
    // length of each DMA buffer, in bytes - 12 bits = 1.5 bytes per output, 16 outputs
    // per chip -> 24 bytes per chip
    uint16_t dmalen;
    
    // Dirty: true means that the non-live buffer has new pending data.  False means
    // that the non-live buffer is empty.
    volatile bool dirty;
    
    // Force an update: true means that we'll send our GS data to the chips even if
    // the buffer isn't dirty.
    volatile bool forceUpdate;
    
    // Enabled: this enables or disables all outputs.  When this is true, we assert the
    // BLANK signal continuously.
    bool enabled;
    
    // SPI port - only MOSI and SCK are used
    SPI spi;

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
    
    // Do we need an XLAT signal on the next blanking interval?
    volatile bool needXlat;
    
    // Reset the grayscale cycle and send the next data update
    void reset()
    {
        // start the blanking cycle
        startBlank();
        
#if !DATA_UPDATE_INSIDE_BLANKING
        // We're configured to send new GS data during the GS cycle,
        // not during the blanking interval, so end the blanking
        // interval now, before we start sending the new data.  Ending
        // the blanking interval starts the new GS cycle.
        //
        // (For the other configuration, we send GS data during the
        // blanking interval, so in that case we DON'T end the blanking
        // interval yet - we defer that until the end-of-DMA interrupt
        // handler, which fires after the GS data send has completed.)
        endBlank();
#endif

        // if we have pending grayscale data, update the DMA data
        bool sendGS = true; // $$$
        if (dirty)
        {
            // The working buffer has changes since our last update.  Swap
            // the live and working buffers so that we send the latest updates.
            uint8_t *tmp = livebuf;
            livebuf = workbuf;
            workbuf = tmp;
            
            // the working buffer is no longer dirty
            dirty = false;
            sendGS = true;
        }
        else if (forceUpdate)
        {
            // send the GS data and consume the forced update flag
            sendGS = true;
            forceUpdate = false;
        }

        // Set the new DMA source to the live buffer.  Note that we start
        // the DMA transfer with the *second* byte - the first byte must
        // be sent by the CPU rather than the DMA module, as outlined in
        // the KL25Z hardware reference manual.

        // Start the new DMA transfer.
        // 
        // The hardware reference manual says that the CPU has to send
        // the first byte of a DMA transfer explicitly.  This is required
        // to avoid a hardware deadlock condition that happens due to
        // a timing interaction between the SPI and DMA controllers.
        // The correct sequence per the manual is:
        //
        //  - reset the SPI controller 
        //  - set up the DMA registers, starting at the 2nd byte to send
        //  - read the SPI status register (SPI0->S), wait for SPTEF to be set
        //  - write the first byte to the SPI data register (SPI0->D)
        //  - enable TXDMAE in the SPI control register (SPI0->C2)
        //
        if (sendGS)
        {
#if 1 // $$$
            Timer t; t.start(); //$$$
            uint8_t *p = livebuf;
            for (int i = dmalen ; i != 0 ; --i) {
                while (!(SPI0->S & SPI_S_SPTEF_MASK)) ;
                SPI0->D = *p++;
            }
            needXlat = true;
            
            spi_total_time += t.read_us();
            spi_runs += 1;
#else
            // disable DMA on SPI0
            SPI0->C2 &= ~SPI_C2_TXDMAE_MASK;
            
            // reset SPI0
            SPI0->C1 &= ~SPI_C1_SPE_MASK;

            // set up a transfer from the second byte of the buffer
            sdma.source(livebuf + 1, true, 8);
            sdma.start(dmalen - 1, false);

            // enable SPI0
            SPI0->C1 |= SPI_C1_SPE_MASK;

            // wait for the TX buffer to clear, then write the first byte manually
            while (!(SPI0->S & SPI_S_SPTEF_MASK)) ;
            SPI0->D = livebuf[0];
            
            // enable DMA to carry out the rest of the transfer
            SPI0->C2 |= SPI_C2_TXDMAE_MASK;
            
            // we'll need a translate on the next blanking cycle
            needXlat = true;
#endif
        }
        
#if !DATA_UPDATE_INSIDE_BLANKING
        // arm the reset handler
        armReset();
#endif
    }

    // arm the reset handler - this fires at the end of each GS cycle    
    void armReset()
    {
        resetTimer.attach(this, &TLC5940::reset, (1.0/GSCLK_SPEED)*4096.0);
    }

    void startBlank()
    {
        //static int i=0; i=(i+1)%200; extern void diagLED(int,int,int); diagLED(i<100,i>=100,0);//$$$

        // turn off the grayscale clock, and assert BLANK to end the grayscale cycle
        gsclk.write(0);
        blank = (enabled ? 1 : 0);  // for the slight delay (20ns) required after GSCLK goes low
        blank = 1;        
    }
            
    void endBlank()
    {
       //static int i=0; i=(i+1)%200; extern void diagLED(int,int,int); diagLED(-1,i<100,-1);//$$$

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
    
    // Interrupt handler for DMA completion.  The DMA controller calls this
    // when it finishes with the transfer request we set up above.  When the
    // transfer is done, we simply end the blanking cycle and start a new
    // grayscale cycle.    
    void dmaDone()
    {
        //static int i=0; i=(i+1)%200; extern void diagLED(int,int,int); diagLED(i<100,-1,-1);//$$$
        
        // disable DMA triggering in the SPI controller until we set
        // up the next transfer
        SPI0->C2 &= ~SPI_C2_TXDMAE_MASK;
        SPI0->C1 &= ~SPI_C1_SPE_MASK;

        // mark that we need to assert XLAT to latch the new
        // grayscale data during the next blanking interval
        needXlat = true;
        
#if DATA_UPDATE_INSIDE_BLANKING
        // we're doing the gs update within the blanking cycle, so end
        // the blanking cycle now that the transfer has completed
        endBlank();

        // set up the next blanking interrupt
        armReset();
#endif
    }

};
 
#endif
