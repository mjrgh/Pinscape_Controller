/*
 *  TSL1410R/TSL1412R interface class.
 *
 *  This provides a high-level interface for the Taos TSL1410R and
 *  TSL1412R linear CCD array sensors.  (The 1410R and 1412R are
 *  identical except for the pixel array size, which the caller 
 *  provides as a parameter when instantiating the class.)
 *
 *  For fast reads of the pixel file from the sensor, we use the KL25Z's 
 *  DMA capability.  The method we use is very specific to the KL25Z 
 *  hardware and is pretty tricky.  These are attributes I don't normally 
 *  like, but the speedup is amazing, enough to justify the complex 
 *  design.  I don't think there's any other way to even get close to 
 *  this kind of read speed for this sensor with this MCU.  (And I've 
 *  tried!)  Reading the sensor quickly is more than just academic,
 *  too: the plunger moves very fast during a release motion, so we 
 *  have to be able to take correspondingly fast pictures to get 
 *  clean images without motion blur.  Before the speedup of this DMA
 *  approach, images captured during release motions had a lot of 
 *  motion blur, and even aliasing from the sinusoidal motion when
 *  the plunger bounces back and forth off the springs.  The speedup
 *  gets us over the threshold where we can capture images with very
 *  little blur, so we can track the motion much more precisely even
 *  at release speeds.  This lets us determine the plunger position
 *  more precisely and more quickly, which improves responsiveness in
 *  the pinball simulator on the PC.
 *
 *  Here's our approach.  
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
 *   - The ADC sample completes, which triggers channel 1, the 
 *     "Clock Up" channel.  This performs one transfer of the 
 *     clock GPIO bit to the clock PSOR register, taking the clock
 *     high, which causes the CCD to move the next pixel onto AO.
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
 *  is important, because it ensures that we don't toggle the clock
 *  bit too fast.  The CCD has a minimum pulse duration of 50ns for
 *  the clock signal.  The DMA controller is so fast that we could
 *  toggle the clock faster than this limit if we did the Up and 
 *  Down transfers adjacently.  
 *
 *  Note also that it's important that Clock Up be the first operation,
 *  because the ADC is in continuous mode, meaning that it starts
 *  taking a new sample immediately upon finishing the previous one.
 *  So when the ADC DMA signal fires, the new sample is just starting.
 *  We therefore have to get the next pixel onto the sampling pin as
 *  quickly as possible.  The CCD sensor's "analog output settling
 *  time" is 120ns - this is the time for a new pixel voltage to 
 *  stabilize on AO after a clock rising edge.  So assuming that the
 *  ADC raises the DMA signal immediately, and the DMA controller
 *  responds within a couple of MCU clock cycles, we should have the
 *  new pixel voltage stable on the sampling pin by about 200ns after
 *  the new ADC sample cycle starts.  The sampling cycle with our
 *  current parameters is about 2us, so the voltage level is stable
 *  for 90% of the cycle.  
 *
 *  Also, it's okay that the ADC sample transfer doesn't happen until 
 *  after the Clock Up DMA transfer.  The ADC output register holds the 
 *  last result until the next sample completes, so we have about 2us 
 *  to grab it.  The first Clock Up DMA transfer only takes a couple 
 *  of clocks - order of 100ns - so we get to it with time to spare.
 *
 *  (Note that it's tempting to try to handle the clock with a single 
 *  DMA channel, by using the PTOR "toggle output" to do TWO writes:
 *  one to toggle the clock up and another to toggle it down.  But
 *  I haven't found a good way to do this.  The problem is that the
 *  DMA controller can only do one transfer per trigger in the fully
 *  autonomous mode we're using, and we need to do two writes.  In
 *  fact, we'd really need to do three or four writes: we'd have to
 *  throw in one or two no-op writes (of all zeroes) between the two
 *  toggles, for time padding to ensure that we meet the minimum 50ns
 *  pulse width for the TSL1410R clock signal.  But it's the same
 *  issue whether it's two writes or four.  The DMA controller does
 *  have a "continuous" mode that does an entire transfer on a single
 *  trigger, but it can't reset itself after such a transfer, so CPU
 *  intervention would be required on every ADC cycle to set up the
 *  next clock write.  We could do that with an interrupt, but given
 *  the 2us cycle time, an interrupt would create a ton of CPU load, 
 *  and probably isn't even enough time to reliably complete each
 *  interrupt service call before the next cycle.  Fortunately, at
 *  the moment we only have one other module in the whole system
 *  using DMA at all - the TLC5940 PWM controller interface, which
 *  only needs one channel.  So with the four available channels in
 *  the hardware, we can afford to use three of them here.)
 */
 
#include "mbed.h"
#include "config.h"
#include "AltAnalogIn.h"
#include "SimpleDMA.h"
#include "DMAChannels.h"
 
#ifndef TSL1410R_H
#define TSL1410R_H


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
 
class TSL1410R
{
public:
    TSL1410R(int nPixSensor, PinName siPin, PinName clockPin, PinName ao1Pin, PinName /*ao2Pin*/) 
        : adc_dma(DMAch_ADC), 
          clkUp_dma(DMAch_CLKUP), 
          clkDn_dma(DMAch_CLKDN),
          si(siPin), 
          clock(clockPin), 
          ao1(ao1Pin, true),
          nPixSensor(nPixSensor)
    {
        // allocate our double pixel buffers
        pix1 = new uint8_t[nPixSensor*2];
        pix2 = pix1 + nPixSensor;
        
        // put the first DMA transfer into the first buffer (pix1)
        pixDMA = 0;

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
        ao1.initDMA(&adc_dma);

        // Set up our chain of linked DMA channel:
        //   ADC sample completion triggers Clock Up
        //   ...which triggers the ADC transfer
        //   ... which triggers Clock Down
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
        clkDn_dma.attach(this, &TSL1410R::transferDone);

        // clear the timing statistics        
        totalTime = 0.0; 
        nRuns = 0;
    }
    
    // end of transfer notification
    void transferDone()
    {
        // stop the ADC sampler
        ao1.stop();
            
        // clock out one extra pixel to leave A1 in the high-Z state
        clock = 1;
        clock = 0;

        // stop the clock
        t.stop();
        
        // count the statistics
        totalTime += t.read();
        nRuns += 1;
    }
    
    // Get the stable pixel array.  This is the image array from the
    // previous capture.  It remains valid until the next startCapture()
    // call, at which point this buffer will be reused for the new capture.
    void getPix(uint8_t * &pix, int &n)
    {
        // return the pixel array that ISN'T assigned to the DMA
        pix = pixDMA ? pix1 : pix2;
        n = nPixSensor;
    }
    
    // Start an image capture from the sensor.  This waits for any previous 
    // capture to finish, then starts a new one and returns immediately.  The
    // new capture proceeds autonomously via the DMA hardware, so the caller
    // can continue with other processing during the capture.
    void startCapture()
    {
        // wait for the previous transfer to finish
        while (adc_dma.isBusy())  { }
        
        // swap to the other DMA buffer
        pixDMA ^= 1;
        
        // start timing this transfer
        t.reset();
        t.start();
        
        // set up the active pixel array as the destination buffer for 
        // the ADC DMA channel
        adc_dma.destination(pixDMA ? pix2 : pix1, true);

        // start the DMA transfers
        clkDn_dma.start(nPixSensor*4);
        adc_dma.start(nPixSensor);
        clkUp_dma.start(nPixSensor*4);
            
        // start the next integration cycle by pulsing SI and one clock
        si = 1;
        clock = 1;
        si = 0;
        clock = 0;
        
        // clock in the first pixel
        clock = 1;
        clock = 0;

        // Start the ADC sampler.  The ADC will read samples continuously
        // until we tell it to stop.  Each sample completion will trigger 
        // our linked DMA channel, which will store the next sample in our
        // pixel array and pulse the CCD serial data clock to load the next
        // pixel onto the analog sampler pin.  This will all happen without
        // any CPU involvement, so we can continue with other work.
        ao1.start();
    }
    
    // Clock through all pixels to clear the array.  Pulses SI at the
    // beginning of the operation, which starts a new integration cycle.
    // The caller can thus immediately call read() to read the pixels 
    // integrated while the clear() was taking place.
    void clear()
    {
        // clock in an SI pulse
        si = 1;
        clockPort->PSOR = clockMask;
        si = 0;
        clockPort->PCOR = clockMask;
        
        // clock out all pixels
        for (int i = 0 ; i < nPixSensor + 1 ; ++i) 
        {
            clock = 1;
            clock = 0;
        }
    }
    
    // get the timing statistics
    void getTimingStats(float &totalTime, uint32_t &nRuns)
    {
        totalTime = this->totalTime;
        nRuns = this->nRuns;
    }

private:
    // DMA controller interfaces
    SimpleDMA adc_dma;        // DMA channel for reading the analog input
    SimpleDMA clkUp_dma;      // "Clock Up" channel
    SimpleDMA clkDn_dma;      // "Clock Down" channel

    // Sensor interface pins
    DigitalOut si;            // GPIO pin for sensor SI (serial data) 
    DigitalOut clock;         // GPIO pin for sensor SCLK (serial clock)
    GPIO_Type *clockPort;     // IOPORT base address for clock pin - cached for DMA writes
    uint32_t clockMask;       // IOPORT register bit mask for clock pin
    AltAnalogIn ao1;          // GPIO pin for sensor AO (analog output)
    
    // number of pixels in the physical sensor array
    int nPixSensor;           // number of pixels in physical sensor array

    // pixel buffers - we keep two buffers so that we can transfer the
    // current sensor data into one buffer via DMA while we concurrently
    // process the last buffer
    uint8_t *pix1;            // pixel array 1
    uint8_t *pix2;            // pixel array 2
    
    // DMA target buffer.  This is the buffer for the next DMA transfer.
    // 0 means pix1, 1 means pix2.  The other buffer contains the stable 
    // data from the last transfer.
    uint8_t pixDMA;

    // timing statistics
    Timer t;                  // timer - started when we start a DMA transfer
    float totalTime;          // total time consumed by all reads so far
    uint32_t nRuns;           // number of runs so far
};
 
#endif /* TSL1410R_H */
