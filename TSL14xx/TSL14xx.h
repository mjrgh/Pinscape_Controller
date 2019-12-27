/*
 *  AMS/TAOS TSL14xx series photodiode array interface class.
 *
 *  This provides a high-level interface for the AMS/TAOS TSLxx series
 *  of photodiode arrays.  This class works with most of the sensors
 *  in this series, which differ only in pixel array sizes.  This code
 *  has been tested with the following sensors from the series:
 *
 *  TSL1410R  - 1280 pixels, 400dpi
 *  TSL1412S  - 1536 pixels, 400dpi
 *  TSL1401CL - 128 pixels, 400dpi
 *
 *  All of these sensors have the same electrical interface, consisting
 *  of a clock input (CLK), start pulse input (SI), and analog pixel
 *  output (AO).  The sensors are equipped with hold capacitors and
 *  shift registers that allow simultaneous sampling of all pixels, and
 *  serial access to the pixel values.
 *
 *  (Note on the plunger sensor class hierarchy: this class is for the
 *  sensor only, not for the plunger application.  This class is meant
 *  to be reusable in other contexts that just need to read raw pixel
 *  data from the sensor.  Plunger/tslxxSensor.h implements the 
 *  specializations of the plunger interface class for these sensors.)
 *
 *
 *  *** Double buffering ***
 *
 *  Our API is based on a double-buffered asynchronous read.  The caller
 *  can access a completed buffer, containing the pixels from the last image 
 *  frame, while the sensor is transferring data asynchronously (using the 
 *  microcontroller's DMA capability) into the other buffer.  Each time a
 *  new read is started, we swap buffers, making the last completed buffer 
 *  available to the client and handing the other buffer to the DMA
 *  controller to fill asynchronously.
 *
 *  In a way, there are actually THREE frames in our pipeline at any given
 *  time:
 *
 *   - a live image integrating light on the photo receptors on the sensor
 *   - the prior image, held in the sensor's shift register and being 
 *     transferred via DMA into one of our buffers (the "DMA" buffer)
 *   - the second prior image, in our other buffer (the "stable" buffer),
 *     available for the client to process
 *
 *  The integration process on the sensor starts when we begin the transfer
 *  of an image via DMA.  That frame's integration period ends when the next 
 *  transfer starts.  So the minimum integration time is also the DMA pixel
 *  transfer time.  Longer integration times can be achieved by waiting
 *  for an additional interval after a DMA transfer finishes, before starting
 *  the next transfer.  We make provision for this added time to allow for
 *  longer exposure times to optimize image quality.
 *
 *
 *  *** Optimizing pixel transfer speed ***
 *
 *  For Pinscape purposes, we want the fastest possible frame rate, as we're
 *  trying to accurately capture the motion of a fast-moving object (the 
 *  plunger).  The TSL14xx sensors can achieve a frame rate up to about
 *  1000 frames per second, if everything is clocked at the limits in the
 *  data sheet.  The KL25Z, however, can't achieve that fast a rate.  The
 *  limiting factor is the KL25Z's ADC.  We have to take an ADC sample for
 *  every pixel, and the minimum sampling time for the ADC on the KL25Z is
 *  about 2us.  With the 1280-pixel TSL1410R, that gives us a minimum
 *  pixel transfer time of about 2.6ms.  And it's actually very difficult
 *  to achieve that speed - my original, naive implementation took more
 *  like 30ms (!!!) to transfer each frame.
 *
 *  As a rule, I don't like tricky code, because it's hard to understand
 *  and hard to debug.  But in this case it's justified.  For good plunger
 *  tracking, it's critical to achieve a minimum frame rate of around 200 
 *  fps (5ms per frame).  I'm pretty sure there's no way to even get close 
 *  to this rate without the complex setup described below.
 *
 *  Here's our approach for fast data transfer:
 * 
 *  First, we put the analog input port (the ADC == Analog-to-Digital 
 *  Converter) in "continuous" mode, at the highest clock speed we can 
 *  program with the available clocks and the fastest read cycle 
 *  available in the ADC hardware.  (The analog input port is the 
 *  GPIO pin attached to the sensor's AO == Analog Output pin, where 
 *  it outputs each pixel's value, one at a time, as an analog voltage 
 *  level.)  In continuous mode, every time the ADC finishes taking a 
 *  sample, it stores the result value in its output register and then 
 *  immediately starts taking a new sample.  This means that no MCU 
 *  (or even DMA) action is required to start each new sample.  This 
 *  is where most of the speedup comes from, since it takes significant
 *  time (multiple microseconds) to move data through the peripheral 
 *  registers, and it takes more time (also multiple microseconds) for
 *  the ADC to spin up for each new sample when in single-sample mode.
 *  We cut out about 7us this way and get the time per sample down to 
 *  about 2us.  This is close to the documented maximum speed for the
 *  ADC hardware.
 *
 *  Second, we use the DMA controller to read the ADC result register
 *  and store each sample in a memory array for processing.  The ADC
 *  hardware is designed to work with the DMA controller by signaling
 *  the DMA controller when a new sample is ready; this allows DMA to
 *  move each sample immediately when it's available without any CPU
 *  involvement.
 *
 *  Third - and this is where it really gets tricky - we use two
 *  additional "linked" DMA channels to generate the clock signal
 *  to the CCD sensor.  The clock signal is how we tell the CCD when
 *  to place the next pixel voltage on its AO pin, so the clock has
 *  to be generated in lock step with the ADC sampling cycle.  The
 *  ADC timing isn't perfectly uniform or predictable, so we can't 
 *  just generate the pixel clock with a *real* clock.  We have to
 *  time the signal exactly with the ADC, which means that we have 
 *  to generate it from the ADC "sample is ready" signal.  Fortunately,
 *  there is just such a signal, and in fact we're already using it,
 *  as described above, to tell the DMA when to move each result from
 *  the ADC output register to our memory array.  So how do we use this
 *  to generate the CCD clock?  The answer lies in the DMA controller's
 *  channel linking feature.  This allows one DMA channel to trigger a
 *  second DMA channel each time the first channel completes one
 *  transfer.  And we can use DMA to control our clock GPIO pin by
 *  using the pin's GPIO IPORT register as the DMA destination address.
 *  Specifically, we can take the clock high by writing our pin's bit 
 *  pattern to the PSOR ("set output") register, and we can take the 
 *  clock low by writing to the PCOR ("clear output") register.  We 
 *  use one DMA channel for each of these operations.
 *
 *  Putting it all together, the cascade of linked DMA channels
 *  works like this:
 *
 *   - We kick off the first ADC sample.
 *
 *   - When the ADC sample completes, the ADC DMA trigger fires,
 *     which triggers channel 1, the "Clock Up" channel.  This 
 *     performs one transfer of the clock GPIO bit to the clock 
 *     PSOR register, taking the clock high, which causes the CCD 
 *     to move the next pixel onto AO.
 *
 *   - After the Clock Up channel does its transfer, it triggers
 *     its link to channel 2, the ADC transfer channel.  This
 *     channel moves the ADC output register value to our memory
 *     array.
 *
 *   - After the ADC channel does its transfer, it triggers channel
 *     3, the "Clock Down" channel.  This performs one transfer of
 *     the clock GPIO bit to the clock PCOR register, taking the
 *     clock low.
 *
 *  Note that the order of the channels - Clock Up, ADC, Clock Down -
 *  is important.  It ensures that we don't toggle the clock line
 *  too quickly.  The CCD has a minimum pulse duration of 50ns for
 *  the clock signal.  The DMA controller is so fast that it might
 *  toggle the clock faster than this limit if we did the Up and 
 *  Down transfers back-to-back.
 *
 *  Note also that it's important for Clock Up to be the very first
 *  operation after the DMA trigger.  The ADC is in continuous mode, 
 *  meaning that it starts taking a new sample immediately upon 
 *  finishing the previous one.  So when the ADC DMA signal fires, 
 *  the new sample is already starting.  We therefore have to get 
 *  the next pixel onto the sampling pin immediately, or as close
 *  to immediately as possible.  The sensor's "analog output 
 *  settling time" is 120ns - this is the time for a new pixel 
 *  voltage to stabilize on AO after a clock rising edge.  So 
 *  assuming that the ADC raises the DMA signal immediately on
 *  sample completion, and the DMA controller responds within a 
 *  couple of MCU clock cycles, we should have the new pixel voltage 
 *  stable on the sampling pin by about 200ns after the new ADC 
 *  sample cycle starts.  The sampling cycle with our current 
 *  parameters is about 2us, so the voltage level is stable for 
 *  90% of the cycle.  
 *
 *  Also, note that it's okay that the ADC sample transfer doesn't
 *  happen until after the Clock Up DMA transfer.  The ADC output 
 *  register holds the last result until the next sample completes, 
 *  so we have about 2us to grab it.  The first Clock Up DMA 
 *  transfer only takes a couple of clocks - order of 100ns - so 
 *  we get to it with time to spare.
 *
 *  (Note that it would nicer to handle the clock with a single DMA
 *  channel, since DMA channels are a limited resource.  We could
 *  conceivably consolidate the clock generator one DMA channel by
 *  switching the DMA destination to the PTOR "toggle" register, and
 *  writing *two* times per trigger - once to toggle the clock up, 
 *  and a second time to toggle it down.  But I haven't found a way 
 *  to make this work.  The obstacle is that the DMA controller can 
 *  only do one transfer per trigger in the fully autonomous mode 
 *  we're using, and to make this toggle scheme work, we'd have to do 
 *  two writes per trigger.  Maybe even three or four:  I think we'd
 *  have to throw in one or two no-op writes (of all zeroes) between 
 *  the two toggles, to pad the timing to ensure that the clock pulse 
 *  width is over the sensor's 50ns minimum.  But it's the same issue 
 *  whether it's two writes or four.  The DMA controller does have a 
 *  "continuous" mode that does an entire transfer on a single trigger,
 *  but it can't reset itself after such a transfer; CPU intervention 
 *  is required to do that, which means we'd have to service an 
 *  interrupt on every ADC cycle to set up the next clock write.  
 *  Given the 2us cycle time, an interrupt would create a ton of CPU 
 *  load, and I don't think the CPU is fast enough to reliably complete
 *  the work we'd have to do on each 2us cycle.  Fortunately, at
 *  the moment we can afford to dedicate three channels to this
 *  module.  We only have one other module using the DMA at all
 *  (the TLC5940 PWM controller interface), and it only needs one
 *  channel.  So the KL25Z's complement of four DMA channels is just
 *  enough for all of our needs for the moment.)
 *
 *  Note that some of the sensors in this series (TSL1410R, TSL1412S)
 *  have a "parallel" readout mode that lets them physically deliver 
 *  two pixels at once the MCU, via separate physical connections.  This 
 *  could provide a 2X speedup on an MCU equipped with two independent 
 *  ADC samplers.  Unfortunately, the KL25Z is not so equipped; even 
 *  though it might appear at first glance to support multiple ADC 
 *  "channels", all of the channels internally multiplex into a single 
 *  converter unit, so the hardware can ultimately perform only one 
 *  conversion at a time.  Paradoxically, using the sensor's parallel 
 *  mode is actually *slower* with a KL25Z than using its serial mode,
 *  because we can only maintain the higher throughput of the KL25Z ADC's
 *  continuous sampling mode by reading all samples thorugh a single
 *  channel.  Switching channels on alternating samples involves a
 *  bunch of setup overhead within the ADC hardware that adds lots of
 *  clocks compared to single-channel continuous mode.
 */
 
#include "mbed.h"
#include "config.h"
#include "AltAnalogIn.h"
#include "SimpleDMA.h"
#include "DMAChannels.h"
 
#ifndef TSL14XX_H
#define TSL14XX_H


// To allow DMA access to the clock pin, we need to point the DMA
// controller to the IOPORT registers that control the pin.  PORT_BASE()
// gives us the address of the register group for the 32 GPIO pins with
// the same letter name as our target pin (e.g., PTA0 through PTA31), 
// and PINMASK gives us the bit pattern to write to those registers to
// access our single GPIO pin.  Each register group has three special
// registers that update the pin in particular ways:  PSOR ("set output 
// register") turns pins on, PCOR ("clear output register") turns pins 
// off, and PTOR ("toggle output register") toggle pins to the opposite
// of their current values.  These registers have special semantics:
// writing a bit as 0 has no effect on the corresponding pin, while
// writing a bit as 1 performs the register's action on the pin.  This
// allows a single GPIO pin to be set, cleared, or toggled with a
// 32-bit write to one of these registers, without affecting any of the
// other pins addressed by the register.  (It also allows changing any
// group of pins with a single write, although we don't use that
// feature here.)
//
// - To turn a pin ON:  PORT_BASE(pin)->PSOR = PINMASK(pin)
// - To turn a pin OFF: PORT_BASE(pin)->PCOR = PINMASK(pin)
// - To toggle a pin:   PORT_BASE(pin)->PTOR = PINMASK(pin)
//
#define GPIO_PORT(pin)        (((unsigned int)(pin)) >> PORT_SHIFT)
#define GPIO_PORT_BASE(pin)   ((GPIO_Type *)(PTA_BASE + GPIO_PORT(pin) * 0x40))
#define GPIO_PINMASK(pin)     gpio_set(pin)
 
IF_DIAG(
    extern uint64_t mainLoopIterCheckpt[];
    extern Timer mainLoopTimer;)
        
class TSL14xx
{
public:
    // Set up the interface.  
    //
    //  nPixSensor = native number of pixels on sensor
    //  siPin = SI pin (GPIO, digital out)
    //  clockPin = CLK pin (GPIO, digital out)
    //  aoPin = AO pin (GPIO, analog in - must be ADC-capable)
    TSL14xx(int nPixSensor, PinName siPin, PinName clockPin, PinName aoPin)
        : adc_dma(DMAch_TSL_ADC), 
          clkUp_dma(DMAch_TSL_CLKUP), 
          clkDn_dma(DMAch_TSL_CLKDN),
          si(siPin), 
          clock(clockPin), 
          ao(aoPin, true, 0),  // continuous sampling, fast sampling mode
          nPixSensor(nPixSensor)
    {
        // Calibrate the ADC for best accuracy
        ao.calibrate();
        
        // start the sample timer with an arbitrary zero point of 'now'
        t.start();
        
        // start with no minimum integration time
        tIntMin = 0;
        
        // allocate our double pixel buffers
        pix1 = new uint8_t[nPixSensor*2];
        pix2 = pix1 + nPixSensor;
        
        // put the first DMA transfer into the first buffer (pix1)
        pixDMA = 0;
        
        // DMA owns both buffers until the first transfer completes
        clientOwnsStablePix = true;

        // remember the clock pin port base and pin mask for fast access
        clockPort = GPIO_PORT_BASE(clockPin);
        clockMask = GPIO_PINMASK(clockPin);
        
        // clear out power-on random data by clocking through all pixels twice
        clear();
        clear();
        
        // Set up the Clock Up DMA channel.  This channel takes the
        // clock high by writing the clock bit to the PSOR (set output) 
        // register for the clock pin.
        clkUp_dma.source(&clockMask, false, 32);
        clkUp_dma.destination(&clockPort->PSOR, false, 32);

        // Set up the Clock Down DMA channel.  This channel takes the
        // clock low by writing the clock bit to the PCOR (clear output)
        // register for the clock pin.
        clkDn_dma.source(&clockMask, false, 32);
        clkDn_dma.destination(&clockPort->PCOR, false, 32);
        
        // Set up the ADC transfer DMA channel.  This channel transfers
        // the current analog sampling result from the ADC output register
        // to our pixel array.
        ao.initDMA(&adc_dma);

        // Set up our chain of linked DMA channel:
        //
        //   ADC sample completion triggers Clock Up
        //   ...which triggers the ADC transfer
        //   ...which triggers Clock Down
        //
        // We operate the ADC in "continuous mode", meaning that it starts
        // a new sample immediately after the last one completes.  This is
        // what keeps the cycle going after the Clock Down, since the Clock
        // Down transfer itself doesn't trigger another DMA operation.
        clkUp_dma.trigger(Trigger_ADC0);
        clkUp_dma.link(adc_dma);
        adc_dma.link(clkDn_dma, false);
        
        // Set the trigger on the downstream links to NONE - these are
        // triggered by their upstream links, so they don't need separate
        // peripheral or software triggers.
        adc_dma.trigger(Trigger_NONE);
        clkDn_dma.trigger(Trigger_NONE);
        
        // Register an interrupt callback so that we're notified when
        // the last transfer completes.
        clkDn_dma.attach(this, &TSL14xx::transferDone);

        // clear the timing statistics        
        totalTime = 0.0; 
        nRuns = 0;
        
        // start the first transfer
        startTransfer();
    }
    
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
    }
    
    // Wait for the current DMA transfer to finish, and retrieve its
    // pixel array buffer.  This provides access to the latest image
    // without starting a new transfer.  These pixels are valid throughout
    // the next transfer (started via startCapture()) and remain valid 
    // until the next transfer after that.
    void waitPix(uint8_t * &pix, uint32_t &t)
    {
        // wait for stable buffer ownership to transfer to the client
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
    
    // Set the requested minimum integration time.  If this is less than the
    // sensor's physical minimum time, the physical minimum applies.
    virtual void setMinIntTime(uint32_t us)
    {
        tIntMin = us;
    }
    
    // Wait for the stable buffer ownership to transfer to the client
    void wait() { while (!clientOwnsStablePix) ; }
    
    // Is a buffer available?
    bool ready() const { return clientOwnsStablePix; }
    
    // Release the client DMA buffer.  The client must call this when it's
    // done with the current image frame to release the frame back to the
    // DMA subsystem, so that it can hand us the next frame.
    void releasePix() { clientOwnsStablePix = false; }
        
    // get the timing statistics - sum of scan time for all scans so far 
    // in microseconds, and total number of scans so far
    void getTimingStats(uint64_t &totalTime, uint32_t &nRuns) const
    {
        totalTime = this->totalTime;
        nRuns = this->nRuns;
    }
    
    // get the average scan time in microseconds
    uint32_t getAvgScanTime() const
    {
        return uint32_t(totalTime / nRuns);
    }
    
private:
    // Start a new transfer.  We call this at the end of each integration
    // cycle, in interrupt mode.  This can be called directly by the interrupt
    // handler invoked when the DMA transfer completes, or by a timeout.  In
    // either case, we're in interrupt mode.
    void startTransfer()
    {
        // If we own the stable buffer, swap buffers: hand ownership of the
        // old DMA buffer to the client, and take control of the old client
        // buffer (which the client must be done with if we own it) as our
        // new DMA buffer.
        //
        // If the client owns the stable buffer, we can't swap buffers,
        // because the client is still working on the stable one.  So we
        // must start the new transfer using the existing DMA buffer.
        if (!clientOwnsStablePix)
        {
            // swap buffers
            pixDMA ^= 1;
            
            // release the prior DMA buffer to the client
            clientOwnsStablePix = true;
        }
        
        // Set up the active pixel array as the destination buffer for 
        // the ADC DMA channel. 
        adc_dma.destination(pixDMA ? pix2 : pix1, true);

        // start the DMA transfers
        clkDn_dma.start(nPixSensor*4, true);
        adc_dma.start(nPixSensor, true);
        clkUp_dma.start(nPixSensor*4, true);
            
        // note the start time of this transfer
        t0 = t.read_us();
        
        // start the next integration cycle by pulsing SI and one clock
        si = 1;
        clock = 1;
        si = 0;
        clock = 0;
        
        // Set the timestamp for the current active buffer.  The SI pulse
        // we just did performed the HOLD operation, which takes a snapshot
        // of the photo receptors and stores it in the sensor's shift
        // register.  We noted the start of the current integration cycle 
        // in tInt when we started it during the previous scan.  The image 
        // we're about to transfer therefore represents the light collected
        // between tInt and right now (actually, the SI pulse above, but 
        // close enough).  The image covers a time range rather than a
        // single point in time, but we still have to give it a single
        // timestamp.  Use the midpoint of the integration period.
        uint32_t tmid = (t0 + tInt) >> 1;
        if (pixDMA)
            t2 = tmid;
        else
            t1 = tmid;

        // Start the ADC sampler.  The ADC will read samples continuously
        // until we tell it to stop.  Each sample completion will trigger 
        // our linked DMA channel, which will store the next sample in our
        // pixel array and pulse the CCD serial data clock to load the next
        // pixel onto the analog sampler pin.  This will all happen without
        // any CPU involvement, so we can continue with other work.
        ao.start();
        
        // The new integration cycle starts with the 19th clock pulse
        // after the SI pulse.  We offload all of the transfer work (including
        // the clock pulse generation) to the DMA controller, which doesn't
        // notify when that 19th pulse occurs, so we have to approximate.
        // Based on empirical measurements, each pixel transfer in our DMA
        // setup takes about 2us, so clocking 19 pixels takes about 38us.
        // In addition, the ADC takes about 4us extra for the first read.
        tInt = t.read_us() + 19*2 + 4;
    }
    
    // End of transfer notification.  This is called as an interrupt
    // handler when the DMA transfer completes.
    void transferDone()
    {
        // stop the ADC sampler
        ao.stop();
            
        // clock out one extra pixel to leave the analog out pin on
        // the sensor in the high-Z state
        clock = 1;
        clock = 0;
        
        // add this sample to the timing statistics (for diagnostics and
        // performance measurement)
        uint32_t now = t.read_us();
        totalTime += uint32_t(now - t0);
        nRuns += 1;
        
        // note the ending time of the transfer
        tDone = now;
        
        // Figure the time remaining to reach the minimum requested 
        // integration time for the next cycle.  The sensor is currently
        // working on an integration cycle that started at tInt, and that
        // cycle will end when we start the next cycle.  We therefore want
        // to wait to start the next cycle until we've reached the desired
        // total integration time.
        uint32_t dt = now - tInt;
        
        // Figure the time to the start of the next transfer.  Wait for the
        // remainder of the current integration period if we haven't yet
        // reached the requested minimum, otherwise just start almost
        // immediately.  (Not *actually* immediately: we don't want to start 
        // the new transfer within this interrupt handler, because the DMA
        // IRQ doesn't reliably clear if we start a new transfer immediately.)
        uint32_t dtNext = dt < tIntMin ? tIntMin - dt : 1;

        // Schedule the next transfer
        integrationTimeout.attach_us(this, &TSL14xx::startTransfer, dtNext);
    }

    // Clear the sensor shift register.  Clocks in all of the pixels from
    // the sensor without bothering to read them on the ADC.  Pulses SI 
    // at the beginning of the operation, which starts a new integration 
    // cycle.
    void clear()
    {
        // get the clock toggle register
        volatile uint32_t *ptor = &clockPort->PTOR;
        
        // make sure any DMA run is completed
        wait();
        
        // clock in an SI pulse
        si = 1;
        *ptor = clockMask;
        clockPort->PSOR = clockMask;
        si = 0;
        *ptor = clockMask;
        
        // This starts a new integration period.  Or more precisely, the
        // 19th clock pulse will start the new integration period.  We're
        // going to blast the clock signal as fast as we can, at about
        // 100ns intervals (50ns up and 50ns down), so the 19th clock
        // will be about 2us from now.
        tInt = t.read_us() + 2;
        
        // clock out all pixels, plus an extra one to clock past the last
        // pixel and reset the last pixel's internal sampling switch in
        // the sensor
        for (int i = 0 ; i < nPixSensor + 1 ; ) 
        {
            // toggle the clock to take it high
            *ptor = clockMask;
            
            // increment our loop variable here to pad the timing, to
            // keep our pulse width long enough for the sensor
            ++i;
            
            // toggle the clock to take it low
            *ptor = clockMask;
        }
    }
        
    // DMA controller interfaces
    SimpleDMA adc_dma;        // DMA channel for reading the analog input
    SimpleDMA clkUp_dma;      // "Clock Up" channel
    SimpleDMA clkDn_dma;      // "Clock Down" channel

    // Sensor interface pins
    DigitalOut si;            // GPIO pin for sensor SI (serial data) 
    DigitalOut clock;         // GPIO pin for sensor SCLK (serial clock)
    GPIO_Type *clockPort;     // IOPORT base address for clock pin - cached for DMA writes
    uint32_t clockMask;       // IOPORT register bit mask for clock pin
    AltAnalogIn_8bit ao;           // GPIO pin for sensor AO (analog output)
    
    // number of pixels in the physical sensor array
    int nPixSensor;           // number of pixels in physical sensor array

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
    
    // End-of-integration timeout handler.  This lets us fire an interrupt
    // when the current integration cycle is done, so that we can start the
    // next cycle.
    Timeout integrationTimeout;
    
    // Requested minimum integration time, in micoseconds.  The client can use 
    // this to control the exposure level, by increasing it for a longer
    // exposure and thus more light-gathering in low-light conditions.  Note 
    // that the physical limit on the minimum integration time is roughly equal 
    // to the pixel file transfer time, because the integration cycle is
    // initiated and ended by transfer starts.  It's thus impossible to make
    // the integration time less than the time for one full pixel file 
    // transfer.
    uint32_t tIntMin;
    
    // timing statistics
    Timer t;                  // sample timer
    uint32_t t0;              // start time (us) of current sample
    uint32_t tInt;            // start time (us) of current integration period
    uint32_t tDone;           // end time of latest finished transfer
    uint64_t totalTime;       // total time consumed by all reads so far
    uint32_t nRuns;           // number of runs so far
};
 
#endif /* TSL14XX_H */
