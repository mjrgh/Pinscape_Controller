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

#include "mbed.h"
#include "FastPWM.h"

/**
  * SPI speed used by the mbed to communicate with the TLC5940
  * The TLC5940 supports up to 30Mhz.  It's best to keep this as
  * high as the microcontroller will allow, since a higher SPI 
  * speed yields a faster grayscale data update.  However, if
  * you have problems with unreliable signal transmission to the
  * TLC5940s, reducing this speed might help.
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
#define USE_SPI 1
#define SPI_SPEED 3000000

/**
  * The rate at which the GSCLK pin is pulsed.   This also controls 
  * how often the reset function is called.   The reset function call
  * rate is (1/GSCLK_SPEED) * 4096.  The maximum reliable rate is
  * around 32Mhz.  It's best to keep this rate as low as possible:
  * the higher the rate, the higher the refresh() call frequency,
  * so the higher the CPU load.
  *
  * The lower bound is probably dependent on the application.  For 
  * driving LEDs, the limiting factor is that lower rates will increase
  * visible flicker.  200 kHz seems to be a good lower bound for LEDs.  
  * That provides about 48 cycles per second - that's about the same as
  * the 50 Hz A/C cycle rate in many countries, which was itself chosen
  * so that incandescent lights don't flicker.  (This rate is a function 
  * of human eye physiology, which has its own refresh cycle of sorts
  * that runs at about 50 Hz.  If you're designing an LED system for
  * viewing by cats or drosophila, you might want to look into your
  * target species' eye physiology, since the persistence of vision
  * rate varies quite a bit from species to species.)  Flicker tends to 
  * be more noticeable in LEDs than in incandescents, since LEDs don't
  * have the thermal inertia of incandescents, so we use a slightly
  * higher default here.  500 kHz = 122 full grayscale cycles per
  * second = 122 reset calls per second (call every 8ms).
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
#if USE_SPI
        : spi(MOSI, NC, SCLK),
#else
        : sin(MOSI), sclk(SCLK),
#endif
          gsclk(GSCLK),
          blank(BLANK),
          xlat(XLAT),
          nchips(nchips),
          newGSData(true)
    {
        // allocate the grayscale buffer
        gs = new unsigned short[nchips*16];
        memset(gs, 0, nchips*16*sizeof(gs[0]));
        
#if USE_SPI
        // Configure SPI format and speed.  Note that KL25Z ONLY supports 8-bit
        // mode.  The TLC5940 nominally requires 12-bit data blocks for the
        // grayscale levels, but SPI is ultimately just a bit-level serial format,
        // so we can reformat the 12-bit blocks into 8-bit bytes to fit the 
        // KL25Z's limits.  This should work equally well on other microcontrollers 
        // that are more flexible.  The TLC5940 appears to require polarity/phase
        // format 0.
        spi.format(8, 0);
        spi.frequency(SPI_SPEED);
#else
        sclk = 1;
#endif

        // Set output pin states
        xlat = 0;
        blank = 1;
        
        // Configure PWM output for GSCLK frequency at 50% duty cycle
        gsclk.period(1.0/GSCLK_SPEED);
        gsclk.write(.5);
        blank = 0;
    }
    
    // start the clock running
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
        reset_timer.attach(this, &TLC5940::reset, (1.0/GSCLK_SPEED)*4096.0);
    }
    
    ~TLC5940()
    {
        delete [] gs;
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
//        newGSData = true;
    }

private:
    // current level for each output
    unsigned short *gs;
    
#if USE_SPI
    // SPI port - only MOSI and SCK are used
    SPI spi;
#else
    DigitalOut sin;
    DigitalOut sclk;
#endif

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
    Timeout reset_timer;
    
    // Has new GS/DC data been loaded?
    volatile bool newGSData;

    // Function to reset the display and send the next chunks of data
    void reset()
    {
        // turn off the grayscale clock, and assert BLANK to end the grayscale cycle
        gsclk.write(0);
        blank = 1;        

        // If we have new GS data, send it now
        if (true) // (newGSData)
        {
            // Send the new grayscale data.
            //
            // Note that ideally, we'd do this during the new PWM cycle
            // rather than during the blanking interval.  The TLC5940 is
            // specifically designed to allow this.  However, in my testing,
            // I found that sending new data during the PWM cycle was
            // unreliable - it seemed to cause a fair amount of glitching,
            // which as far as I can tell is signal noise coming from
            // crosstalk between the grayscale clock signal and the 
            // SPI signal.  This seems to be a common problem with
            // daisy-chained TLC5940s.  It can in principle be solved with
            // careful high-speed circuit design (good ground planes, 
            // short leads, decoupling capacitors), and indeed I was able
            // to improve stability to some extent with circuit tweaks,
            // but I wasn't able to eliminate it entirely.  Moving the
            // data refresh into the blanking interval, on the other 
            // hand, seems to entirely eliminate any instability.
            //
            // Note that there's no CPU performance penalty to this 
            // approach.  The KL25Z SPI implementation isn't capable of
            // asynchronous DMA, so the CPU has to wait for the 
            // transmission no matter when it happens.  The only downside
            // I see to this approach is that it decreases the duty cycle
            // of the PWM during updates - but very slightly.  With the
            // SPI clock at 30 MHz and the PWM clock at 500 kHz, the full
            // PWM cycle is 8192us, and the data refresh time is 25us.
            // So by doing the data refersh in the blanking interval, 
            // we're effectively extending the PWM cycle to 8217us, 
            // which is 0.3% longer.  Since the outputs are all off 
            // during the blanking cycle, this is equivalent to 
            // decreasing all of the output brightnesses by 0.3%.  That
            // should be imperceptible to users.
            update();

            // the chips are now in sync with our data, so we have no more
            // pending update
            newGSData = false;
            
            // latch the new data while we're still blanked
            xlat = 1;
            xlat = 0;
        }

        // end the blanking interval and restart the grayscale clock
        blank = 0;
        gsclk.write(.5);
        
        // set up the next blanking interrupt
        reset_timer.attach(this, &TLC5940::reset, (1.0/GSCLK_SPEED)*4096.0);
    }
    
    void update()
    {
#if USE_SPI
        // Send GS data.  The serial format orders the outputs from last to first
        // (output #15 on the last chip in the daisy-chain to output #0 on the
        // first chip).  For each output, we send 12 bits containing the grayscale
        // level (0 = fully off, 0xFFF = fully on).  Bit order is most significant 
        // bit first.  
        // 
        // The KL25Z SPI can only send in 8-bit increments, so we need to divvy up 
        // the 12-bit outputs into 8-bit bytes.  Each pair of 12-bit outputs adds up 
        // to 24 bits, which divides evenly into 3 bytes, so send each pairs of 
        // outputs as three bytes:
        //
        //   [    element i+1 bits   ]  [ element i bits        ]
        //   11 10 9 8 7 6 5 4 3 2 1 0  11 10 9 8 7 6 5 4 3 2 1 0
        //   [  first byte   ] [   second byte  ] [  third byte ]
        for (int i = 61 /* (16 * nchips) - 2 */ ; i >= 0 ; i -= 2)
        {
            // first byte - element i+1 bits 4-11
            spi.write(((gs[i+1] & 0xFF0) >> 4) & 0xff);
            
            // second byte - element i+1 bits 0-3, then element i bits 8-11
            spi.write((((gs[i+1] & 0x00F) << 4) | ((gs[i] & 0xF00) >> 8)) & 0xFF);
            
            // third byte - element i bits 0-7
            spi.write(gs[i] & 0x0FF);
        }
#else
        // Send GS data, from last output to first output, 12 bits per output,
        // most significant bit first.
        for (int i = 16*3 - 1 ; i >= 0 ; --i)
        {
            unsigned data = gs[i];
            for (unsigned int mask = 1 << 11, bit = 0 ; bit < 12 ; ++bit, mask >>= 1)
            {
                sclk = 0;                    
                sin = (data & mask) ? 1 : 0;
                sclk = 1;
            }
        }
#endif
    }
};

 
#endif
