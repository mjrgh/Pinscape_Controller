// Pinscape Controller TLC5940 interface
//
// Based on Spencer Davis's mbed TLC5940 library.  Adapted for the
// KL25Z, and simplified to just the functions needed for this
// application.  In particular, this version doesn't include support 
// for dot correction programming or status input.  This version also
// uses a different approach for sending the grayscale data updates,
// sending updates during the blanking interval rather than overlapping
// them with the PWM cycle.  This results in very slightly longer 
// blanking intervals when updates are pending, effectively reducing 
// the PWM "on" duty cycle (and thus the output brightness) by about 
// 0.3%.  This shouldn't be perceptible to users, so it's a small
// trade-off for the advantage gained, which is much better signal 
// stability when using multiple TLC5940s daisy-chained together.
// I saw a lot of instability when using the overlapped approach,
// which seems to be eliminated entirely when sending updates during
// the blanking interval.

 
#ifndef TLC5940_H
#define TLC5940_H

// Data Transmission Mode.
//
// NOTE!  This section contains a possible workaround to try if you're 
// having data signal stability problems with your TLC5940 chips.  If
// your chips are working properly, you can ignore this part!
//
// The software has two options for sending data updates to the chips:
//
// Mode 0:  Send data *during* the grayscale cycle.  This is the way the
// chips are designed to be used.  While the grayscale clock is running,
// we send data for the *next* cycle, then latch the updated data to the
// output registers during the blanking interval at the end of the cycle.
//
// Mode 1:  Send data *between* grayscale cycles.  In this mode, we send
// each complete update during a blanking period, then latch the update
// and start the next grayscale cycle.  This isn't the way the chips were
// intended to be used, but it works.  The disadvantage is that it requires
// the blanking interval to be extended to be long enough for the full
// data update (192 bits * the number of chips in the chain).  Since the
// outputs are turned off for the entire blanking period, this reduces
// the overall brightness/intensity of the outputs by reducing the duty
// cycle.  The TLC5940 chips can't achieve 100% duty cycle to begin with,
// since they require a certain minimum time in the blanking interval
// between grayscale cycles; however, the minimum is so short that the
// duty cycle is close to 100%.  With the full data transmission stuffed
// into the blanking interval, we reduce the duty cycle further below
// 100%.  With four chips in the chain, a 28 MHz data clock, and a
// 500 kHz grayscale clock, the reduction is about 0.3%.
//
// By default, we use Mode 0, because that's the timing model specified
// by the manufacturer, and empirically it works well with the Pinscape 
// Expansion boards.  
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
#include "FastPWM.h"
#include "SimpleDMA.h"

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
  * so the higher the CPU load.
  *
  * The lower bound depends on the application.  For driving LEDs, 
  * the limiting factor is that lower rates will increase visible flicker.
  * A GSCLK speed of 200 kHz is about as low as you can go with LEDs 
  * without excessive flicker.  That equals about 48 full grayscale
  * cycles per second.  That might seem perfectly good in that it's 
  * about the same as the standard 50Hz A/C cycle rate in many countries, 
  * but the 50Hz rate was chosen to minimize visible flicker in 
  * incandescent lamps, not LEDs.  LEDs need a higher rate because they 
  * don't have thermal inertia as incandescents do.  The default we use 
  * here is 500 kHz = 122 full grayscale cycles per second.  That seems
  * to produce excellent visual results.  Higher rates would probably
  * produce diminishing returns given that they also increase CPU load.
  */
#define GSCLK_SPEED    500000

/**
  *  This class controls a TLC5940 PWM driver IC.
  *
  *  Using the TLC5940 class to control an LED:
  *  @code
  *  #include "mbed.h"
  *  #include "TLC5940.h"
  *  
  *  // Create the TLC5940 instance
  *  TLC5940 tlc(p7, p5, p21, p9, p10, p11, p12, 1);
  *  
  *  int main()
  *  {   
  *      // Enable the first LED
  *      tlc.set(0, 0xfff);
  *      
  *      while(1)
  *      {
  *      }
  *  }
  *  @endcode
  */
class TLC5940
{
public:
    /**
      *  Set up the TLC5940
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
          blank(BLANK),
          xlat(XLAT),
          nchips(nchips)
    {
        // set XLAT to initially off
        xlat = 0;
        
        // Assert BLANK while starting up, to keep the outputs turned off until
        // everything is stable.  This helps prevent spurious flashes during startup.
        // (That's not particularly important for lights, but it matters more for
        // tactile devices.  It's a bit alarming to fire a replay knocker on every
        // power-on, for example.)
        blank = 1;
        
        // allocate the grayscale buffer, and set all outputs to fully off
        gs = new unsigned short[nchips*16];
        memset(gs, 0, nchips*16*sizeof(gs[0]));
        
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
        // somewhat unpredictable, so send extra just to make sure we cover
        // all bases.  This does no harm; extra bits just fall off the end of
        // the daisy chain, and since we want all registers set to 0, we can
        // send arbitrarily many extra 0's.
        for (int i = 0 ; i < nchips*25 ; ++i)
            spi.write(0);
            
        // do an initial XLAT to latch all of these "0" values into the
        // grayscale registers
        xlat = 1;
        xlat = 0;

        // Allocate a DMA buffer.  The transfer on each cycle is 192 bits per
        // chip = 24 bytes per chip.
        dmabuf = new char[nchips*24];
        memset(dmabuf, 0, nchips*24);
        
        // Set up the Simple DMA interface object.  We use the DMA controller to
        // send grayscale data updates to the TLC5940 chips.  This lets the CPU
        // keep running other tasks while we send gs updates, and importantly
        // allows our blanking interrupt handler return almost immediately.
        // The DMA transfer is from our internal DMA buffer to SPI0, which is
        // the SPI controller physically connected to the TLC5940s.
        sdma.source(dmabuf, true, 8);
        sdma.destination(&(SPI0->D), false, 8);
        sdma.trigger(Trigger_SPI0_TX);
        sdma.attach(this, &TLC5940::dmaDone);
        
        // Enable DMA on SPI0.  SimpleDMA doesn't do this for us; we have to
        // do it explicitly.  This is just a matter of setting bit 5 (TXDMAE)
        // in the SPI controller's Control Register 2 (C2).
        SPI0->C2 |= 0x20; // set bit 5 = 0x20 = TXDMAE in SPI0 control register 2

        // Configure the GSCLK output's frequency
        gsclk.period(1.0/GSCLK_SPEED);
        
        // mark that we need an initial update
        newGSData = true;
        needXlat = false;
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
        // reset cycle to line up exactly with a full PWM cycle, it works
        // better to set up a new timer on each cycle, *after* we've finished
        // with the somewhat unpredictable overhead of the interrupt handler.
        // This ensures that we'll get much closer to exact alignment of the
        // cycle phase, and in any case the worst that happens is that some
        // cycles are very slightly too long or short (due to imperfections
        // in the timer clock vs the PWM clock that determines the GSCLCK
        // output to the TLC5940), which is far less noticeable than a 
        // constantly rotating phase misalignment.
        resetTimer.attach(this, &TLC5940::reset, (1.0/GSCLK_SPEED)*4096.0);
    }
    
    ~TLC5940()
    {
        delete [] gs;
        delete [] dmabuf;
    }

    /**
      *  Set the next chunk of grayscale data to be sent
      *  @param data - Array of 16 bit shorts containing 16 12 bit grayscale data chunks per TLC5940
      *  @note These must be in intervals of at least (1/GSCLK_SPEED) * 4096 to be sent
      */
    void set(int idx, unsigned short data) 
    {
        // store the data, and flag the pending update for the interrupt handler to carry out
        gs[idx] = data; 
        newGSData = true;
    }

private:
    // current level for each output
    unsigned short *gs;
    
    // Simple DMA interface object
    SimpleDMA sdma;

    // DMA transfer buffer.  Each time we have data to transmit to the TLC5940 chips,
    // we format the data into this buffer exactly as it will go across the wire, then
    // hand the buffer to the DMA controller to move through the SPI port.
    char *dmabuf;
    
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
    
    // Has new GS/DC data been loaded?
    volatile bool newGSData;
    
    // Do we need an XLAT signal on the next blanking interval?
    volatile bool needXlat;

    // Function to reset the display and send the next chunks of data
    void reset()
    {
        // start the blanking cycle
        startBlank();
        
#if DATA_UPDATE_INSIDE_BLANKING
        // We're configured to send the new GS data entirely within
        // the blanking interval.  Start the DMA transfer now, and
        // return without ending the blanking interval.  The DMA
        // completion interrupt handler will do that when the data
        // update has completed.  
        //
        // Note that we do the data update/ unconditionally in the 
        // send-during-blanking case, whether or not we have new GS 
        // data.  This is because the update causes a 0.3% reduction 
        // in brightness because of the elongated BLANK interval.
        // That would be visible as a flicker on each update if we
        // did updates on some cycles and not others.  By doing an
        // update on every cycle, we make the brightness reduction
        // uniform across time, which makes it less perceptible.
        update();
        sdma.start(nchips*24);

        
#else // DATA_UPDATE_INSIDE_BLANKING
        
        // end the blanking interval
        endBlank();
        
        // if we have pending grayscale data, update the DMA data
        if (newGSData)
            update();
                    
        // send out the DMA contents
        sdma.start(nchips*24);


#endif // DATA_UPDATE_INSIDE_BLANKING
    }

    void startBlank()
    {
        // turn off the grayscale clock, and assert BLANK to end the grayscale cycle
        gsclk.write(0);
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

        // end the blanking interval and restart the grayscale clock
        blank = 0;
        gsclk.write(.5);
        
        // set up the next blanking interrupt
        resetTimer.attach(this, &TLC5940::reset, (1.0/GSCLK_SPEED)*4096.0);
    }
    
    void update()
    {
        // Send new grayscale data to the TLC5940 chips.
        //
        // To do this, we set up our DMA buffer with the bytes formatted exactly
        // as they will go across the wire, then kick off the transfer request with 
        // the DMA controller.  We can then return from the interrupt and continue
        // with other tasks while the DMA hardware handles the transfer for us.
        // When the transfer is completed, the DMA controller will fire an
        // interrupt, which will call our interrupt handler, which will finish
        // the blanking cycle.
        //
        // The serial format orders the outputs from last to first (output #15 on 
        // the last chip in the daisy-chain to output #0 on the first chip).  For 
        // each output, we send 12 bits containing the grayscale level (0 = fully 
        // off, 0xFFF = fully on).  Bit order is most significant bit first.  
        // 
        // The KL25Z SPI can only send in 8-bit increments, so we need to divvy up 
        // the 12-bit outputs into 8-bit bytes.  Each pair of 12-bit outputs adds up 
        // to 24 bits, which divides evenly into 3 bytes, so send each pairs of 
        // outputs as three bytes:
        //
        //   [    element i+1 bits   ]  [ element i bits        ]
        //   11 10 9 8 7 6 5 4 3 2 1 0  11 10 9 8 7 6 5 4 3 2 1 0
        //   [  first byte   ] [   second byte  ] [  third byte ]
        for (int i = (16 * nchips) - 2, dst = 0 ; i >= 0 ; i -= 2)
        {
            // first byte - element i+1 bits 4-11
            dmabuf[dst++] = (((gs[i+1] & 0xFF0) >> 4) & 0xff);
            
            // second byte - element i+1 bits 0-3, then element i bits 8-11
            dmabuf[dst++] = ((((gs[i+1] & 0x00F) << 4) | ((gs[i] & 0xF00) >> 8)) & 0xFF);
            
            // third byte - element i bits 0-7
            dmabuf[dst++] = (gs[i] & 0x0FF);
        }
        
        // we've now cleared the new GS data
        newGSData = false;
    }

    // Interrupt handler for DMA completion.  The DMA controller calls this
    // when it finishes with the transfer request we set up above.  When the
    // transfer is done, we simply end the blanking cycle and start a new
    // grayscale cycle.    
    void dmaDone()
    {
        // mark that we need to assert XLAT to latch the new
        // grayscale data during the next blanking interval
        needXlat = true;
        
#if DATA_UPDATE_INSIDE_BLANKING
        // we're doing the gs update within the blanking cycle, so end
        // the blanking cycle now that the transfer has completed
        endBlank();
#endif
    }

};
 
#endif
