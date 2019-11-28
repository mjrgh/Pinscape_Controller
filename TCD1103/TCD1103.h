// Toshiba TCD1103 linear CCD image sensor, 1x1500 pixels
//
// This sensor is conceptually similar to the TAOS TSL1410R (the original 
// Pinscape sensor!).  Like the TSL1410R, it has a linear array of optical
// sensor pixels that convert incident photons into electrical charge, an
// internal shift register connected to the pixel file that acts as an 
// electronic shutter, and a serial interface that clocks the pixels out
// to the host in analog voltage level format.
//
// Mechanically, this sensor has an entirely different size scale vs the
// TSL1410R.  The 1410R's sensor window is about the same size as a standard 
// plunger's travel range (about 80mm), so the mechanical setup we use with
// sensor is to situate the sensor adjacent to the plunger, with the pixel
// window aligned with the plunger's axis of motion, so that the plunger
// casts a shadow on the sensor at 1:1 scale.  The TCD1103, in contrast, is
// a tiny little thing, with about an 8mm window.  That means that we have
// to reduce the plunger shadow image by about 10X to fit the sensor, so an
// optical lens is required.  This makes it more complicated to set up, but
// it also adds the advantage of allowing us to focus the image, for a more
// precise reading.  The shadow in the lens-less 1410R setup is usually about
// four of five pixels wide, so we lose a lot of the sensor's native
// precision to the poor optics - we only get about 1/50" resolution as a
// result.  With a focusing lens, we could potentially get single-pixel
// resolution, which would be about 1/500" resolution.  The reality will
// be somewhat lower, depending on how hard we want to work at the optics,
// but it should be possible to do much better than the unfocused 1410R.
//
// The electronic interface to this sensor has some fairly tight timing
// requirements, per the data sheet.  The sensor requires the host to 
// provide a master clock that runs at 0.4 MHz to 4 MHz.  The data sheet's
// timing diagrams imply that the master clock runs continuously, although
// it's probably like the 1410R, where the clock is only needed when you 
// want to run the shift register and can be stopped at other times.
//
// As with the 1410R, we'll have to use DMA for the ADC transfers in order
// to keep up with the high data rate without overloading the KL25Z CPU.
// With the 1410R, we're able to use the ADC itself as the clock source,
// by running the ADC in continous mode and using its "sample ready" signal
// to trigger the DMA transfer.  We used this to generate the external clock
// signal for the sensor by "linking" the ADC's DMA channel to another pair
// of DMA channels that generated the clock up/down signal each time an ADC
// sample completed.  This strategy won't work with the Toshiba sensor,
// though, because the Toshiba sensor's timing sequence requires *two* clock
// pulses per pixel.  I can't come up with a way to accomplish that with the
// linked-DMA approach.  (I've tried!)
//
// So instead, we'll have to generate a true clock signal for the sensor. 
// The obvious way to do this (and the only way, as far as I can come up with)
// is to use a TPM channel - that is, a PWM output.  TPM channels are designed
// precisely for this kind of work, so this is the right approach in terms of
// suitability, but it has the downside that TPM units are an extremely scarce
// resource on the KL25Z.  We only have three of them to work with.  Luckily
// the rest of the Pinscape software only requires two of them: one for the
// IR transmitter (which uses a TPM channel to generate the 41-48 kHz carrier
// wave used by nearly all consumer IR remotes), and one for the TLC5940
// driver (which uses it to generate the grayscale clock signal).  Note that
// we also use PWM channels for feedback device output ports, but those don't
// have any dependency on the TPM period - they'll work with whatever period
// the underlying TPM is set to use.  So the feedback output ports can all
// happily use free channels on TPM units claimed by any of the dedicated
// users (IR, TLC5940, and us).
//
// But what do we do about the 2:1 ratio between master clock pulses and ADC
// samples?  The "right" way would be to allocate a second TPM unit to
// generate a second clock signal at half the frequency of the master clock, 
// and use that as the ADC trigger.  But as we just said, we only have three 
// TPM units in the whole system, and two of them are already claimed for 
// other uses, so we only have one unit to use here.  
//
// Fortunately, we can make do with one TPM unit, by taking advantage of a 
// feature/quirk of the KL25Z ADC.  The quirk lets us take ADC samples at
// exactly half of the master clock rate, in perfect sync.  The trick is to
// pick a combination of master clock rate and ADC sample mode such that the
// ADC conversion time is *almost but not quite* twice as long as the master
// clock rate.  With that combination of timings, we can trigger the ADC
// from the TPM, and we'll get an ADC sample on exactly every other tick of
// the master clock.  The reason this works is that the KL25Z ADC ignores
// hardware triggers (the TPM trigger is a hardware trigger) that occur when
// a conversion is already in progress.  So if the ADC sampling time is more
// than one master clock period, the ADC will always be busy one clock tick
// after a sample starts, so it'll ignore that first clock tick.  But as 
// long as the sampling time is less than *two* master clock periods, the
// ADC will always be ready again on the second tick.  So we'll get one ADC
// sample for every two master clock ticks, exactly as we need.
//

#include "config.h"
#include "NewPwm.h"
#include "AltAnalogIn.h"
#include "SimpleDMA.h"
#include "DMAChannels.h"


// Logic Gate Inverters:  if invertedLogicGates is true, it means that the
// hardware is buffering all of the logic signals (fM, ICG, SH) through an
// inverter.  The data sheet recommends using a 74HC04 between the host MCU
// and the chip logic gates because of the high capacitive load on some of
// the gates (particularly SH, 150pF).  

template<bool invertedLogicGates> class TCD1103
{
public:
    TCD1103(PinName fmPin, PinName osPin, PinName icgPin, PinName shPin) :
       fm(fmPin, invertedLogicGates),
       os(osPin, false, 6, 1),    // single sample, 6-cycle long sampling mode, no averaging
       icg(icgPin), 
       sh(shPin),
       os_dma(DMAch_TDC_ADC)
    {
        // Calibrate the ADC for best accuracy
        os.calibrate();

        // Idle conditions: SH low, ICG high.
        sh = logicLow;
        icg = logicHigh;
        
        // ADC sample conversion time.  This must be calculated based on the
        // combination of parameters selected for the os() initializer above.
        // See the KL25 Sub-Family Reference Manual, section 28.4.45, for the
        // formula.
        const float ADC_TIME = 2.2083333e-6f; // 6-cycle long sampling, no averaging

        // Set the TPM cycle time to satisfy our timing constraints:
        // 
        //   Tm + epsilon1 < A < 2*Tm - epsilon2
        //
        // where A is the ADC conversion time and Tm is the master clock
        // period, and the epsilons are a margin of safety for any 
        // non-deterministic component to the timing of A and Tm.  The
        // epsilons could be zero if the timing of the ADC is perfectly
        // deterministic; this must be determined empirically.
        //
        // The most conservative solution would be to make epsilon as large
        // as possible, which means bisecting the time window by making
        // A = 1.5*T, or, equivalently, T = A/1.5 (the latter form being more 
        // useful because T is the free variable here, as we can only control
        // A to the extent that we can choose the ADC parameters).
        //
        // But we'd also like to make T as short as possible while maintaining
        // reliable operation.  Shorter T yields a higher frame rate, and we
        // want the frame rate to be as high as possible so that we can track
        // fast plunger motion accurately.  Empirically, we can get reliable
        // results by using half of the ADC time plus a small buffer time.
        //
        fm.getUnit()->period(masterClockPeriod = ADC_TIME/2 + 0.1e-6f);
        printf("TCD1103 master clock period = %g\r\n", masterClockPeriod);
        
        // Start the master clock running with a 50% duty cycle
        fm.write(0.5f);

        // allocate our double pixel buffers
        pix1 = new uint8_t[nPixSensor*2];
        pix2 = pix1 + nPixSensor;
        
        // put the first DMA transfer into the first buffer (pix1)
        pixDMA = 0;
        running = false;

        // start the sample timer with an arbitrary epoch of "now"
        t.start();

        // Set up the ADC transfer DMA channel.  This channel transfers
        // the current analog sampling result from the ADC output register
        // to our pixel array.
        os.initDMA(&os_dma);

        // Register an interrupt callback so that we're notified when
        // the last ADC transfer completes.
        os_dma.attach(this, &TCD1103::transferDone);
        
        // Set up the ADC to trigger on the master clock's TPM channel
        os.setTriggerTPM(fm.getUnitNum());
        
        // clear the timing statistics        
        totalXferTime = 0.0; 
        maxXferTime = 0;
        minXferTime = 0xffffffff;
        nRuns = 0;

        // clear random power-up data by clocking through all pixels twice
        clear();
        clear();
    }
        
    // logic gate levels, based on whether or not the logic gate connections
    // in the hardware are buffered through inverters
    static const int logicLow = invertedLogicGates ? 1 : 0;
    static const bool logicHigh = invertedLogicGates ? 0 : 1;
    
    // ready to read
    bool ready() { return !running; }
    
    // is the DMA busy?
    bool dmaBusy() { return running; }

    // wait for the current DMA cycle to finish
    void wait() { while (running) ; }
    
    // Get the stable pixel array.  This is the image array from the
    // previous capture.  It remains valid until the next startCapture()
    // call, at which point this buffer will be reused for the new capture.
    void getPix(uint8_t * &pix, uint32_t &t)
    {
        // return the pixel array that ISN'T assigned to the DMA
        if (pixDMA)
        {
            // DMA owns pix2, so the stable array is pix1
            pix = pix1;
            t = t1;
        }
        else
        {
            // DMA owns pix1, so the stable array is pix2
            pix = pix2;
            t = t2;
        }

        // debugging - print out the pixel transfer time stats periodically
        static int n;
        ++n;
        if (n > 1000)
        {
            printf("TCD1103 scan last=%d, min=%d, max=%d (us)\r\n", dtPixXfer, minXferTime, maxXferTime);
            n = 0;
        }
    }
    
    // wait for pixels to become ready
    void waitPix(uint8_t * &pix, uint32_t &t) 
    {
        // wait for the current transfer to finish
        wait();
        
        // Return the pixel array that IS assigned to DMA, since this
        // is the latest buffer filled.  This buffer is stable, even
        // though it's assigned to DMA, because the last transfer is
        // already finished and thus DMA is no longer accessing the
        // buffer.
        if (pixDMA)
        {
            // DMA owns pix2
            pix = pix2;
            t = t2;
        }
        else
        {
            // DMA owns pix1
            pix = pix1;
            t = t1;
        }
   }
        
    // Start an image capture from the sensor.  Waits the previous
    // capture to finish if it's still running, then starts a new one
    // and returns immediately.  The new capture proceeds autonomously 
    // via the DMA hardware, so the caller can continue with other 
    // processing during the capture.
    void startCapture(uint32_t minIntTime_us = 0)
    {
        IF_DIAG(uint32_t tDiag0 = mainLoopTimer.read_us();)
        
        // wait for the last current capture to finish
        while (running) { }

        // we're starting a new capture immediately        
        running = true;

        // collect timing diagnostics
        IF_DIAG(mainLoopIterCheckpt[8] += uint32_t(mainLoopTimer.read_us() - tDiag0);)
        
        // If the elapsed time since the start of the last integration
        // hasn't reached the specified minimum yet, wait.  This allows
        // the caller to control the integration time to optimize the
        // exposure level.
        uint32_t dt = uint32_t(t.read_us() - tInt);
        if (dt < minIntTime_us)
        {
            // we haven't reached the required minimum yet - wait for the 
            // remaining interval
            wait_us(minIntTime_us - dt);
        }
        
        // swap to the other DMA buffer for reading the new pixel samples
        pixDMA ^= 1;
        
        // Set up the active pixel array as the destination buffer for 
        // the ADC DMA channel. 
        os_dma.destination(pixDMA ? pix2 : pix1, true);
        
        // Start the read cycle by sending the ICG/SH pulse sequence
        uint32_t tNewInt = gen_SH_ICG_pulse(true);

        // Set the timestamp for the current active buffer.  The ICG/SH
        // gymnastics we just did transferred the CCD pixels into the sensor's
        // internal shift register and reset the pixels, starting a new
        // integration cycle.  So the pixels we just shifted started
        // integrating the *last* time we did that, which we recorded as
        // tInt at the time.  The image we're about to transfer therefore 
        // represents the light collected between tInt and the SH pulse we
        // just did.  The image covers a time range rather than a single 
        // point in time, but we still have to give it a single timestamp. 
        // Use the midpoint of the integration period.
        uint32_t tmid = (tNewInt + tInt) >> 1;
        if (pixDMA)
            t2 = tmid;
        else
            t1 = tmid;

        // Record the start time of the currently active integration period
        tInt = tNewInt;
        
        IF_DIAG(mainLoopIterCheckpt[9] += uint32_t(mainLoopTimer.read_us() - tDiag0);)
    }
    
    // clear the sensor pixels    
    void clear() 
    {
        // make sure any DMA run is completed
        wait();
        
        // send an SH/ICG pulse sequence to start an integration cycle
        // (without initiating a DMA transfer, as we just want to discard
        // the incoming samples for a "clear")
        tInt = gen_SH_ICG_pulse(false);
        
        // wait for one full readout cycle, plus a little extra for padding
        ::wait(nPixSensor*masterClockPeriod*2 + 4.0e-6f);
    }
    
    // figure the average scan time from the running totals
    uint32_t getAvgScanTime() { return static_cast<uint32_t>(totalXferTime / nRuns);}

protected:
    // Generate an SH/ICG pulse.  This transfers the pixel data from the live
    // sensor photoreceptors into the sensor's internal shift register, clears
    // the live pixels, and starts a new integration cycle.
    //
    // If start_dma_xfer is true, we'll start the DMA transfer for the ADC
    // pixel data.  We handle this here because the sensor starts clocking
    // out pixels precisely at the end of the ICG pulse, so we have to be
    // be very careful about the timing.
    //
    // Returns the timestamp (relative to our image timer 't') of the end
    // of the SH pulse, which is the moment the new integration cycle starts.
    //
    // Note that we send these pulses synchronously - that is, this routine
    // blocks until the pulses have been sent.  The overall sequence takes 
    // about 2.5us to 3us, so it's not a significant interruption of the 
    // main loop.
    //
    uint32_t gen_SH_ICG_pulse(bool start_dma_xfer)
    {
        // If desired, prepare to start the DMA transfer for the ADC data.
        // (Set up a dummy location to write in lieu of the DMA register if
        // DMA initiation isn't required, so that we don't have to take the
        // time for a conditional when we're ready to start the DMA transfer.
        // The timing there will be extremely tight, and we can't afford the
        // extra instructions to test a condition.)
        uint8_t dma_chcfg_dummy = 0;
        volatile uint8_t *dma_chcfg = start_dma_xfer ? os_dma.prepare(nPixSensor, true) : &dma_chcfg_dummy;
        
        // The basic idea is to take ICG low, and while holding ICG low,
        // pulse SH.  The coincidence of the two pulses transfers the charge
        // from the live pixels into the shift register, which effectively
        // discharges the live pixels and thereby starts a new integration
        // cycle.
        //
        // The timing of the pulse sequence is rather tightly constrained 
        // per the data sheet, so we have to take some care in executing it:
        //
        //   ICG ->  LOW
        //   100-1000 ns delay   (*)
        //   SH -> HIGH
        //   >1000ns delay
        //   SH -> LOW
        //   >1000ns delay
        //   ICG -> high         (**)
        //
        // There are two steps here that are tricky:
        //
        // (*) is a narrow window that we can't achieve with an mbed 
        // microsecond timer.  Instead, we'll do a couple of extra writes 
        // to the ICG register, which take about 60ns each.
        //
        // (**) has the rather severe constraint that the transition must 
        // occur AND complete while the master clock is high.  Other people 
        // working with similar Toshiba chips in MCU projects have suggested
        // that this constraint can safely be ignored, so maybe the data
        // sheet's insistence about it is obsolete advice from past Toshiba
        // sensors that the doc writers carried forward by copy-and-paste.
        // Toshiba has been making these sorts of chips for a very long time,
        // and the data sheets for many of them are obvious copy-and-paste
        // jobs.  But let's take the data sheet at its word and assume that 
        // this is important for proper operation.  Our best hope of 
        // satisfying this constraint is to synchronize the start of the
        // ICG->high transition with the start of a TPM cycle on the master
        // clock.  That guarantees that the ICG transition starts when the
        // clock signal is high (as each TPM cycle starts out high), and
        // gives us the longest possible runway for the transition to
        // complete while the clock is still high, as we get the full
        // length of the high part of the cycle to work with.  To quantify,
        // it gives us about 600ns.  The register write takes about 60ns, 
        // and waitEndCycle() adds several instructions of overhead, perhaps
        // 200ns, so we get around 300ns for the transition to finish.  That
        // should be a gracious plenty assuming that the hardware is set up 
        // with an inverter to buffer the clock signals.  The inverter should
        // be able to pull up the 35pF on ICG in a "typical" 30ns (rise time
        // plus propagation delay, per the 74HC04 data sheet) and max 150ns.
        // This seems to be one place where the inverter might really be
        // necessary to meet the timing requirements, as the KL25Z GPIO
        // might need more like 2us to pull that load up.
        //
        // There's an additional constraint on the timing at the end of the
        // ICG pulse.  The sensor starts clocking out pixels on the rising
        // edge of the ICG pulse.  So we need the ICG pulse end to align
        // with the start of an ADC cycle.  If we get that wrong, all of our
        // ADC samples will be off by half a clock, so every sample will be
        // the average of two adjacent pixels instead of one pixel.  That
        // would lose half of the image resolution, which would obviously
        // be bad.  So make certain we're at the tail end of an ADC cycle
        // by waiting for the ADC "ready" bit to be set.
        //
        // The end of the SH pulse triggers the start of a new integration 
        // cycle, so note the time of that pulse for image timestamping 
        // purposes.  That will be the start time of the NEXT image we 
        // transfer after we shift out the current sensor pixels, which 
        // represent the pixels from the last time we pulsed SH.
        //
        icg = logicLow;
        icg = logicLow;  // for timing, adds about 60ns
        icg = logicLow;  // ditto, another 60ns, total is now 120ns > min 100ns
        sh = logicHigh;
        wait_us(1);      // >1000ns delay
        sh = logicLow;
        uint32_t t_sh = t.read_us();  // this is the start time of the NEXT image
        wait_us(1);      // >1000ns delay
        
        // Now the tricky part!  We have to end the ICG pulse (take ICG high)
        // at the start of a master clock cycle, AND at the start of an ADC 
        // sampling cycle.  The sensor will start clocking out pixels the
        // instance ICG goes high, so we have to align our ADC cycle so that
        // we start a sample at almost exactly the same time we take ICG
        // high.
        //
        // Now, every ADC sampling cycle always starts at a rising edge of 
        // the master clock, since the master clock is the ADC trigger.  BUT,
        // the converse is NOT true: every rising edge of the master clock
        // is NOT an ADC sample start.  Recall that we've contrived the timing
        // so that every OTHER master clock rising edge starts an ADC sample.
        // 
        // So how do we detect which part of the clock cycle we're in?  We
        // could conceivably use the COCO bit in the ADC status register to
        // detect the little window between the end of one sample and the
        // start of the next.  Unfortunately, this doesn't work: the COCO
        // bit is never actually set for the duration of even a single CPU
        // instruction in our setup, no matter how loose we make the timing
        // between the ADC and the fM cycle.  I think the reason is the DMA
        // setup: the COCO bit triggers the DMA, and the DMA controller
        // reads the ADC result register (the DMA source in our setup),
        // which has the side effect of clearing COCO.  I've experimented
        // with this using different timing parameters, and the result is
        // always the same: the CPU *never* sees the COCO bit set.  The DMA
        // trigger timing is evidently deterministic such that the DMA unit
        // invariably gets its shot at reading ADC0->R before the CPU does.
        //
        // The COCO approach would be a little iffy anyway, since we want the
        // ADC idle time to be as short as possible, which wouldn't give us
        // much time to do all we have to do in the COCO period, even if
        // there were one.  What we can do instead is seize control of the
        // ADC cycle timing: rather than trying to detect when the cycle
        // ends, we can specify when it begins.  We can do this by canceling
        // the TPM->ADC trigger and aborting any conversion in progress, then
        // reprogramming the TPM->ADC trigger at our leisure.  What we *can*
        // detect reliably is the start of a TPM cycle.  So here's our
        // strategy:
        //
        //   - Turn off the TPM->ADC trigger and abort the current conversion
        //   - Wait until a new TPM cycle starts
        //   - Reset the TPM->ADC trigger.  The first new conversion will
        //     start on the next TPM cycle, so we have the remainder of
        //     the current TPM cycle to make this happen (about 1us, enough
        //     for 16 CPU instructions - plenty for this step)
        //   - Wait for the new TPM cycle
        //   - End the ICG pulse
        //

        // The timing is so tight here that we want to be sure we're not
        // interrupted by other tasks - disable interrupts.
        __disable_irq();
        
        // disable the TPM->ADC trigger and abort the current conversion
        os.stop();

        // Enable the DMA controller for the new transfer from the ADC.
        // The sensor will start clocking out new samples at the ICG rising
        // edge, so the next ADC sample to complete will represent the first
        // pixel in the new frame.  So we need the DMA ready to go at the
        // very next sample.  Recall that the DMA is triggered by ADC
        // completion, and ADC is stopped right now, so enabling the DMA 
        // won't have any immediate effect - it just spools it up so that
        // it's ready to move samples as soon as we resume the ADC.
        *dma_chcfg |= DMAMUX_CHCFG_ENBL_MASK;
        
        // wait for the start of a new master clock cycle
        fm.waitEndCycle();
        
        // Okay, a master clock cycle just started, so we have about 1us 
        // (about 16 CPU instructions) before the next one begins.  Resume 
        // ADC sampling.  The first new sample will start with the next
        // TPM cycle 1us from now.  (This step takes about 3 instructions.)
        os.resume();
        
        // Okay, everything is queued up!  We just have to fire the starting
        // pistol on the sensor at the right moment.  And that right moment 
        // is the start of the next TPM cycle.  Wait for it...
        fm.waitEndCycle();
        
        // And go!
        icg = logicHigh;
        
        // note the start time of the transfer
        tXfer = t.read_us();
        
        // done with the critical timing section
        __enable_irq();
        
        // return the timestamp of the end of the SH pulse - this is the start
        // of the new integration period that we just initiated
        return t_sh;
    }

    // end of transfer notification
    void transferDone()
    {
        // add this sample to the timing statistics (for diagnostics and
        // performance measurement)
        uint32_t dt = dtPixXfer = static_cast<uint32_t>(t.read_us() - tXfer);
        totalXferTime += dt;
        nRuns += 1;
        
        // collect debug statistics
        if (dt < minXferTime) minXferTime = dt;
        if (dt > maxXferTime) maxXferTime = dt;
        
        // the sampler is no long running
        running = false;
    }

    // master clock
    NewPwmOut fm;
    
    // analog input for reading the pixel voltage level
    AltAnalogIn_8bit os;
    
    // Integration Clear Gate output
    DigitalOut icg;
    
    // Shift Gate output
    DigitalOut sh;
    
    // DMA channel for the analog input
    SimpleDMA os_dma;
    
    // Master clock period, in seconds, calculated based on the ADC timing
    float masterClockPeriod;
    
    // Number of pixels.  The TCD1103 has 1500 image pixels, plus 32 dummy
    // pixels at the front end (before the first image pixel) and another 14
    // dummy pixels at the back end.  The sensor always transfers the full
    // file on each read cycle, including the dummies, so we have to make
    // room for the dummy pixels during each read.
    static const int nPixSensor = 1546;
    
    // pixel buffers - we keep two buffers so that we can transfer the
    // current sensor data into one buffer via DMA while we concurrently
    // process the last buffer
    uint8_t *pix1;            // pixel array 1
    uint8_t *pix2;            // pixel array 2
    
    // Timestamps of pix1 and pix2 arrays, in microseconds, in terms of the 
    // sample timer (this->t).
    uint32_t t1;
    uint32_t t2;
    
    // DMA target buffer.  This is the buffer for the next DMA transfer.
    // 0 means pix1, 1 means pix2.  The other buffer contains the stable 
    // data from the last transfer.
    uint8_t pixDMA;
    
    // flag: sample is running
    volatile bool running;

    // timing statistics
    Timer t;                  // sample timer
    uint32_t tInt;            // start time (us) of current integration period
    uint32_t tXfer;           // start time (us) of current pixel transfer
    uint32_t dtPixXfer;       // pixel transfer time of last frame
    uint64_t totalXferTime;   // total time consumed by all reads so far
    uint32_t nRuns;           // number of runs so far
    
    // debugging - min/max transfer time statistics
    uint32_t minXferTime;
    uint32_t maxXferTime;
};
