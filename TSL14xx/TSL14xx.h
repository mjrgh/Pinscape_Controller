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
 *  data from the sensor.  Plunger/tslxxSensor.h implements the next
 *  level up, which is the implementation of the generic plunger sensor
 *  interface for TSL14xx sensors.  That's still an abstract class, since
 *  it only provides the plunger class specialization for these sensor
 *  types, without any image analysis component.  The final concrete 
 *  classes are in Plunger/edgeSensor.h and Plunger/barCodeSensor.h,
 *  which add the image processing that analyzes the image data to 
 *  determine the plunger position.)
 *
 *  Our API is based on a double-buffered asynchronous read.  The caller
 *  can access a completed buffer, containing the pixels from the last image 
 *  frame, while the sensor is transferring data asynchronously (using the 
 *  microcontroller's DMA capability) into the other buffer.  Each time a
 *  new read is started, we swap buffers, making the last completed buffer 
 *  available to the client and handing the other buffer to the DMA
 *  controller to fill asynchronously.
 *  
 *  The photodiodes in these sensors gather light very rapidly, allowing
 *  for extremely short exposure times.  The "shutter" is electronic;
 *  a signal on the pulse input resets the pixels and begins an integration
 *  period, and a subsequent signal ends the integration and transfers the
 *  pixel voltages to the hold capacitors.  Minimum exposure times are less
 *  than a millisecond.  The actual timing is under software control, since
 *  we determine the start and end of the integration period via the pulse
 *  input.  Longer integration periods gather more light, like a longer
 *  exposure on a conventional camera.  For our purposes in the Pinscape
 *  Controller, we want the highest possible frame rate, as we're trying to 
 *  capture the motion of a fast-moving object (the plunger).  The KL25Z 
 *  can't actually keep up with shortest integration time the sensor can 
 *  achieve - the limiting factor is the KL25Z ADC, which needs at least
 *  2.5us to collect each sample.  The sensor transfers pixels to the MCU 
 *  serially, and each pixel is transferred as an analog voltage level, so 
 *  we have to collect one ADC sample per pixel.  Our maximum frame rate 
 *  is therefore determined by the product of the minimum ADC sample time 
 *  and the number of pixels.  
 *
 *  The fastest operating mode for the KL25Z ADC is its "continuous"
 *  mode, where it automatically starts taking a new sample every time
 *  it completes the previous one.  The fastest way to transfer the
 *  samples to memory in this mode is via the hardware DMA controller.
 *  
 *  It takes a pretty tricky setup to make this work.  I don't like tricky 
 *  setups - I prefer something easy to understand - but in this case it's
 *  justified because of the importance in this application of maximizing 
 *  the frame rate.  I'm pretty sure there's no other way to even get close 
 *  to the rate we can achieve with the continuous ADC/DMA combination.
 *  The ADC/DMA mode gives us pixel read times of about 2us, vs a minimum 
 *  of about 14us for the next best method I've found.  Using this mode, we 
 *  can read the TSL1410R's 1280 pixels at full resolution in about 2.5ms.  
 *  That's a frame rate of 400 frames per second, which is fast enough to 
 *  capture a fast-moving plunger with minimal motion blur.
 *
 *  Note that some of the sensors in this series (TSL1410R, TSL1412S) have
 *  a "parallel" readout mode that lets them physically deliver two pixels
 *  at once the MCU, via separate physical connections.  This could provide 
 *  a 2X speedup on an MCU equipped with two independent ADC samplers.  
 *  Unfortunately, the KL25Z is not so equipped; even though it might appear
 *  at first glance to support multiple ADC "channels", all of the channels
 *  internally connect to a single ADC sampler, so the hardware can ultimately
 *  perform only one conversion at a time.  Paradoxically, using the sensor's
 *  parallel mode is actually *slower* with a KL25Z than using its serial
 *  mode, because we can only maintain the higher throughput of the KL25Z
 *  ADC's "continuous sampling mode" by reading all samples thorugh a single
 *  channel.
 *
 *  Here's the tricky approach we use:
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
        : adc_dma(DMAch_ADC), 
          clkUp_dma(DMAch_CLKUP), 
          clkDn_dma(DMAch_CLKDN),
          si(siPin), 
          clock(clockPin), 
          ao(aoPin, true),
          nPixSensor(nPixSensor)
    {
        // start the sample timer with an arbitrary zero point of 'now'
        t.start();
        
        // allocate our double pixel buffers
        pix1 = new uint8_t[nPixSensor*2];
        pix2 = pix1 + nPixSensor;
        
        // put the first DMA transfer into the first buffer (pix1)
        pixDMA = 0;
        running = false;

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
        
        IF_DIAG(mainLoopIterCheckpt[9] += uint32_t(mainLoopTimer.read_us() - tDiag0);)
    }
    
    // Wait for the current capture to finish
    void wait()
    {
        while (running) { }
    }
    
    // Is the latest reading ready?
    bool ready() const { return !running; }
    
    // Is a DMA transfer in progress?
    bool dmaBusy() const { return running; }
        
    // Clock through all pixels to clear the array.  Pulses SI at the
    // beginning of the operation, which starts a new integration cycle.
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
    // end of transfer notification
    void transferDone()
    {
        // stop the ADC sampler
        ao.stop();
            
        // clock out one extra pixel to leave A1 in the high-Z state
        clock = 1;
        clock = 0;
        
        // add this sample to the timing statistics (for diagnostics and
        // performance measurement)
        uint32_t now = t.read_us();
        totalTime += uint32_t(now - t0);
        nRuns += 1;
        
        // the sampler is no long running
        running = false;
        
        // note the ending time of the transfer
        tDone = now;
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
    AltAnalogIn ao;           // GPIO pin for sensor AO (analog output)
    
    // number of pixels in the physical sensor array
    int nPixSensor;           // number of pixels in physical sensor array

    // pixel buffers - we keep two buffers so that we can transfer the
    // current sensor data into one buffer via DMA while we concurrently
    // process the last buffer
    uint8_t *pix1;            // pixel array 1
    uint8_t *pix2;            // pixel array 2
    
    // Timestamps of pix1 and pix2 arrays, in microseconds, in terms of the 
    // ample timer (this->t).
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
    uint32_t t0;              // start time (us) of current sample
    uint32_t tInt;            // start time (us) of current integration period
    uint32_t tDone;           // end time of latest finished transfer
    uint64_t totalTime;       // total time consumed by all reads so far
    uint32_t nRuns;           // number of runs so far
};
 
#endif /* TSL14XX_H */
