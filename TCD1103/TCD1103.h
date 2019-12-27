// Toshiba TCD1103 linear CCD image sensor, 1x1500 pixels.
//
// This sensor is conceptually similar to the TAOS TSL1410R (the original 
// Pinscape sensor!).  Like the TSL1410R, it has a linear array of optical
// sensor pixels that convert incident photons into electrical charge, an
// internal shift register connected to the pixel file that acts as an 
// electronic shutter, and a serial interface that clocks the pixels out
// to the host in analog voltage level format.
//
// The big physical difference between this sensor and the old TAOS sensors
// is the size.  The TAOS sensors were (by some miracle) approximately the
// same size as the plunger travel range, so we were able to take "contact"
// images without any optics, by placing the plunger close to the sensor,
// back-lighting it, and essentially taking a picture of its shadow.  The
// Toshiba sensor, in contrast, has a pixel window that's only 8mm long, so
// the contact image approach won't work.  Instead, we have to use a lens
// to focus a reduced image (about 1:10 scale) on the sensor.  That makes
// the physical setup more complex, but it has the great advantage that we
// get a focused image.  The shadow was always fuzzy in  the old contact 
// image approach, which reduced the effective resolution when determining 
// the plunger position.  With a focused image, we can get single-pixel 
// resolution.  With this Toshiba sensor's 1500 pixels, that's about 500 
// dpi, which beats every other sensor we've come up with.
//
// The electronic interface to this sensor is similar to the TAOS, but it
// has enough differences that we can't share the same code base.
//
// As with the 1410R, we have to use DMA for the ADC transfers in order
// to keep up with the high data rate without overloading the KL25Z CPU.
// With the 1410R, we're able to use the ADC itself as the clock source,
// by running the ADC in continous mode and using its "sample ready" signal
// to trigger the DMA transfer.  We used this to generate the external clock
// signal for the sensor by "linking" the ADC's DMA channel to another pair
// of DMA channels that generated the clock up/down signal each time an ADC
// sample completed.  This strategy won't work with the Toshiba sensor,
// though, because the Toshiba sensor's timing sequence requires *two* clock
// pulses per pixel.  I can't come up with a way to accomplish that with the
// linked-DMA approach.  Instead, we'll have to generate a true clock signal
// for the sensor, and drive the DMA conversions off of that clock.
//
// The obvious (and, as far as I can tell, only) way to generate the clock
// signal with the KL25Z at the high frequency required is to use a TPM -
// the KL25Z module that drives PWM outputs.  TPM channels are designed
// precisely for this kind of work, so this is the right approach in terms of
// suitability, but it has the downside that TPM units are an extremely scarce
// resource on the KL25Z.  We only have three of them to work with.  Luckily,
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
// other uses, so we only have one unit available for our use here.
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
// This is all possible because the ADC timing is deterministic, and runs on
// the same clock as the TPM.  The KL25Z Subfamily Reference Manual explains
// how to calculate the ADC conversion time for a given combination of mode
// bits.  So we just have to pick an ADC mode, calculate its conversion time,
// and then select a TPM period that's slightly more than 1/2 of the ADC
// conversion time.
//
//
// Pixel output signal
//
// The pixel output signal from this sensor is an analog voltage level.  It's
// inverted from the brightness: higher brightness is represented by lower
// voltage.  The dynamic range is only about 1V - dark pixels read at about 
// 2V, and saturated pixels read at about 1V.  
//
//
// Inverted logic signals
//
// The Toshiba data sheet recommends buffering the logic signal inputs from 
// an MCU through a 74HC04 inverter, because the sensor's logic gates have
// relatively high input capacitance that an MCU might not be able to drive 
// fast enough directly to keep up with the sensor's timing requirements.  
// SH in particular might be a problem because of its 150pF capacitance,
// which implies about a 2us rise/fall time if driven directly by KL25Z
// GPIOs, which is too slow.
//
// The software willo work with or without the logic inversion, in case anyone
// wants to try implementing it with direct GPIO drive (not recommended) or 
// with a non-inverting buffer in place of the 74HC04.  Simply instantiate the
// class with the 'invertedLogicGates' template parameter set to false to use 
// non-inverted logic.
//
//
// How to connect to the KL25Z
//
// Follow the "typical drive circuit" presented in the Toshiba data sheet.
// They leave some of the parts unspecified, so here are the specific values
// we used for our reference implementation:
//
//   - 3.3V power supply
//   - 74HC04N hex inverter for the logic gate inputs (fM, SH, ICG)
//   - 0.1uF ceramic + 10uF electrolytic decoupling capacitors (GND to Vcc))
//   - BC212A PNP transistor for the output drive (OS), with:
//     - 150 ohm resistor on the base
//     - 150 ohm resistor between collector and GND
//     - 2.2K ohm resistor between emitter and Vcc
//

#include "config.h"
#include "NewPwm.h"
#include "AltAnalogIn.h"
#include "SimpleDMA.h"
#include "DMAChannels.h"


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
        // Idle conditions: SH low, ICG high.
        sh = logicLow;
        icg = logicHigh;

        // Set a zero minimum integration time by default.  Note that tIntMin
        // has no effect when it's less than the absolute minimum, which is
        // the pixel transfer time for one frame (around 3ms).  tIntMin only
        // kicks in when it goes above that absolute minimum, at which point
        // we'll wait for any additional time needed to reach tIntMin before
        // starting the next integration cycle.
        tIntMin = 0;

        // Calibrate the ADC for best accuracy
        os.calibrate();
        
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
        
        // Start the master clock running with a 50% duty cycle
        fm.write(0.5f);

        // Allocate our double pixel buffers.  
        pix1 = new uint8_t[nPixAlo * 2];
        pix2 = pix1 + nPixAlo;
        
        // put the first DMA transfer into the first buffer (pix1)
        pixDMA = 0;
        clientOwnsStablePix = false;

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

        // start the first transfer
        startTransfer();
    }
        
    // logic gate levels, based on whether or not the logic gate connections
    // in the hardware are buffered through inverters
    static const int logicLow = invertedLogicGates ? 1 : 0;
    static const bool logicHigh = invertedLogicGates ? 0 : 1;
    
    // ready to read
    bool ready() { return clientOwnsStablePix; }
        
    // Get the stable pixel array.  This is the image array from the
    // previous capture.  It remains valid until the next startCapture()
    // call, at which point this buffer will be reused for the new capture.
    void getPix(uint8_t * &pix, uint32_t &t)
    {
        // Return the pixel array that ISN'T assigned to the DMA.
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
        
        // The raw pixel array we transfer in from the sensor on the serial 
        // connection consists of 32 dummy elements, followed by 1500 actual
        // image pixels, followed by 14 dummy elements.  Skip the leading 32 
        // dummy pixels when passing the buffer back to the client.
        pix += 32;
    }
    
    // release the client's pixel buffer
    void releasePix() { clientOwnsStablePix = false; }
    
    // figure the average scan time from the running totals
    uint32_t getAvgScanTime() { return static_cast<uint32_t>(totalXferTime / nRuns);}

    // Set the requested minimum integration time.  If this is less than the
    // sensor's physical minimum time, the physical minimum applies.
    virtual void setMinIntTime(uint32_t us)
    {
        tIntMin = us;
    }
    
protected:
    // Start an image capture from the sensor.  Waits the previous
    // capture to finish if it's still running, then starts a new one
    // and returns immediately.  The new capture proceeds asynchronously 
    // via DMA hardware transfer, so the client can continue with other 
    // processing during the capture.
    void startTransfer()
    {
        // if we own the stable buffer, swap buffers
        if (!clientOwnsStablePix)
        {
            // swap buffers
            pixDMA ^= 1;
            
            // release the prior DMA buffer to the client
            clientOwnsStablePix = true;
        }
        
        // figure our destination buffer
        uint8_t *dst = pixDMA ? pix2 : pix1;
        
        // Set up the active pixel array as the destination buffer for 
        // the ADC DMA channel. 
        os_dma.destination(dst, true);
        
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
    }
    
    // End of transfer notification.  This runs as an interrupt handler when
    // the DMA transfer completes.
    void transferDone()
    {
        // stop the ADC triggering
        os.stop();

        // add this sample to the timing statistics (for diagnostics and
        // performance measurement)
        uint32_t now = t.read_us();
        uint32_t dt = dtPixXfer = static_cast<uint32_t>(now - tXfer);
        totalXferTime += dt;
        nRuns += 1;
        
        // collect debug statistics
        if (dt < minXferTime) minXferTime = dt;
        if (dt > maxXferTime) maxXferTime = dt;

        // figure how long we've been integrating so far on this cycle 
        uint32_t dtInt = now - tInt;
        
        // Figure the time to the start of the next transfer.  Wait for the
        // remainder of the current integration period if we haven't yet
        // reached the requested minimum, otherwise just start almost
        // immediately.  (Not *actually* immediately: we don't want to start 
        // the new transfer within this interrupt handler, because the DMA
        // IRQ doesn't reliably clear if we start a new transfer immediately.)
        uint32_t dtNext = dtInt < tIntMin ? tIntMin - dtInt : 1;
        
        // Schedule the next transfer
        integrationTimeout.attach_us(this, &TCD1103::startTransfer, dtNext);
    }

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
        icg = logicLow;  // one more to be safer

        sh = logicHigh;  // take SH high
        
        wait_us(1);      // >1000ns delay
        sh = logicHigh;  // a little more padding to be sure we're over the minimum
        sh = logicHigh;  // more padding
        
        sh = logicLow;   // take SH low
        
        uint32_t t_sh = t.read_us();  // this is the start time of the NEXT integration
        
        wait_us(3);      // >1000ns delay, 5000ns typical; 3us should get us most
                         // of the way there, considering that we have some more
                         // work to do before we end the ICG pulse
        
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
        
        // Eerything is queued up!  We just have to fire the starting gun
        // on the sensor at the right moment.  And that right moment is the 
        // start of the next TPM cycle.  Wait for it...
        fm.waitEndCycle();
        
        // And go!
        icg = logicHigh;
        
        // note the start time of the transfer
        tXfer = t.read_us();
        
        // return the timestamp of the end of the SH pulse - this is the start
        // of the new integration period that we just initiated
        return t_sh;
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
    
    // Figure the number of pixels to allocate per pixel buffer.  Round
    // up to the next 4-byte boundary, so that the buffers are both DWORD-
    // aligned.  (This allows using DWORD pointers into the buffer to 
    // operate on buffer pixels four at a time, such as in the negative 
    // image inversion code in the generic PlungerSensorImage base class.)
    static const int nPixAlo = (nPixSensor + 3) & ~3;
    
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
    
    // Stable buffer ownership.  At any given time, the DMA subsystem owns
    // the buffer specified by pixDMA.  The other buffer - the "stable" buffer,
    // which contains the most recent completed frame, can be owned by EITHER
    // the client or by the DMA subsystem.  Each time a DMA transfer completes,
    // the DMA subsystem looks at the stable buffer owner flag to determine 
    // what to do:
    //
    // - If the DMA subsystem owns the stable buffer, it swaps buffers.  This
    //   makes the newly completed DMA buffer the new stable buffer, and makes
    //   the old stable buffer the new DMA buffer.  At this time, the DMA 
    //   subsystem also changes the stable buffer ownership to CLIENT.
    //
    // - If the CLIENT owns the stable buffer, the DMA subsystem can't swap
    //   buffers, because the client is still using the stable buffer.  It
    //   simply leaves things as they are.
    //
    // In either case, the DMA system starts a new transfer at this point.
    //
    // The client, meanwhile, is free to access the stable buffer when it has
    // ownership.  If the client *doesn't* have ownership, it must wait for
    // the ownership to be transferred, which can only be done by the DMA
    // subsystem on completing a transfer.
    //
    // When the client is done with the stable buffer, it transfers ownership
    // back to the DMA subsystem.
    //
    // Transfers of ownership from DMA to CLIENT are done only by DMA.
    // Transfers from CLIENT to DMA are done only by CLIENT.  So whoever has
    // ownership now is responsible for transferring ownership.
    //
    volatile bool clientOwnsStablePix;
    
    // Minimum requested integration time, in microseconds
    uint32_t tIntMin;
    
    // Timeout for generating an interrupt at the end of the integration period
    Timeout integrationTimeout;
        
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
