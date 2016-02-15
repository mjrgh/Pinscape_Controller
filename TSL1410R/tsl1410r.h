// DMA VERSION - NOT WORKING

// I'm saving this code for now, since it was somewhat promising but doesn't
// quite work.  The idea here was to read the ADC via DMA, operating the ADC
// in continuous mode.  This speeds things up pretty impressively (by about 
// a factor of 3 vs having the MCU read each result from the ADC sampling 
// register), but I can't figure out how to get a stable enough signal out of 
// it.  I think the problem is that the timing isn't precise enough in detecting 
// when the DMA completes each write.  We have to clock the next pixel onto the 
// CCD output each time we complete a sample, and we have to do so quickly so 
// that the next pixel charge is stable at the ADC input pin by the time the 
// ADC sample interval starts.  I'm seeing a ton of noise, which I think means 
// that the new pixel isn't ready for the ADC in time. 
//
// I've tried a number of approaches, none of which works:
//
// - Skip every other sample, so that we can spend one whole sample just 
// clocking in the next pixel.  We discard the "odds" samples that are taken
// during pixel changes, and use only the "even" samples where the pixel is
// stable the entire time.  I'd think the extra sample would give us plenty
// of time to stabilize the next pixel, but it doesn't seem to work out that
// way.  I think the problem might be that the latency of the MCU responding
// to each sample completion is long enough relative to the sampling interval
// that we can't reliably respond to the ADC done condition fast enough.  I've
// tried basing the sample completion detection on the DMA byte counter and
// the ADC interrupt.  The DMA byte counter is updated after the DMA transfer
// is done, so that's probably just too late in the cycle.  The ADC interrupt
// should be concurrent with the DMA transfer starting, but in practice it 
// still doesn't give us good results.
//
// - Use DMA, but with the ADC in single-sample mode.  This bypasses the latency
// problem by ensuring that the ADC doesn't start a new sample until we've
// definitely finished clocking in the next pixel.  But it defeats the whole
// purpose by eliminating the speed improvement - the speeds are comparable to
// doing the transfers via the MCU.  This surprises me because I'd have expected
// that the DMA would run concurrently with the MCU pixel clocking code, but
// maybe there's enough bus contention between the MCU and DMA in this case that
// there's no true overlapping of the operation.  Or maybe the interrupt dispatch
// adds enough overhead to negate any overlapping.  I haven't actually been able
// to get good data out of this mode, either, but I gave up early because of the
// lack of any speed improvement.

/*
 *  TSL1410R interface class.
 *
 *  This provides a high-level interface for the Taos TSL1410R linear CCD array sensor.
 */
 
#include "mbed.h"
#include "config.h"
#include "AltAnalogIn.h"
#include "SimpleDMA.h"
 
#ifndef TSL1410R_H
#define TSL1410R_H
#define TSL1410R_DMA

// For faster GPIO on the clock pin, we write the IOPORT registers directly.
// PORT_BASE gives us the memory mapped location of the IOPORT register set
// for a pin; PINMASK gives us the bit pattern to write to the registers.
//
// - To turn a pin ON:  PORT_BASE(pin)->PSOR |= PINMASK(pin)
// - To turn a pin OFF: PORT_BASE(pin)->PCOR |= PINMASK(pin)
// - To toggle a pin:   PORT_BASE(pin)->PTOR |= PINMASK(pin)
//
// When used in a loop where the port address and pin mask are cached in
// local variables, this runs at the same speed as the FastIO library - about 
// 78ns per pin write on the KL25Z.  Not surprising since it's doing the same
// thing, and the compiler should be able to reduce a pin write to a single ARM
// instruction when the port address and mask are in local register variables.
// The advantage over the FastIO library is that this approach allows for pins
// to be assigned dynamically at run-time, which we prefer because it allows for
// configuration changes to be made on the fly rather than having to recompile
// the program.
#define GPIO_PORT(pin)        (((unsigned int)(pin)) >> PORT_SHIFT)
#define GPIO_PORT_BASE(pin)   ((FGPIO_Type *)(FPTA_BASE + GPIO_PORT(pin) * 0x40))
#define GPIO_PINMASK(pin)     gpio_set(pin)
 
class TSL1410R
{
public:
    TSL1410R(int nPixSensor, PinName siPin, PinName clockPin, PinName ao1Pin, PinName ao2Pin) 
        : nPixSensor(nPixSensor), si(siPin), clock(clockPin), ao1(ao1Pin), ao2(ao2Pin)
    {
        // we're in parallel mode if ao2 is a valid pin
        parallel = (ao2Pin != NC);
        
        // remember the clock pin port base and pin mask for fast access
        clockPort = GPIO_PORT_BASE(clockPin);
        clockMask = GPIO_PINMASK(clockPin);
        
        // clear out power-on random data by clocking through all pixels twice
        clear();
        clear();
        
        // set up our DMA channel for reading from our analog in pin
        ao1.initDMA(&adc_dma);
        
        // Set up our DMA channel for writing the sensor SCLK - we use the PTOR
        // (toggle) register to flip the bit on each write.  To pad the timing
        // to the rate required by the CCD, do a no-op 0 write to PTOR after
        // each toggle.  This gives us a 16-byte buffer, which we can make
        // circular in the DMA controller.
        static const uint32_t clkseq[] = { clockMask, 0, clockMask, 0 };
        clk_dma.destination(&clockPort->PTOR, false, 32);
        clk_dma.source(clkseq, true, 32, 16);   // set up our circular source buffer
        clk_dma.trigger(Trigger_ADC0);          // software trigger
        clk_dma.setCycleSteal(false);           // do the entire transfer on each trigger
        
        totalTime = 0.0; nRuns = 0; // $$$
    }
    
    float totalTime; int nRuns; // $$$

    // ADC interrupt handler - on each ADC event, 
    static TSL1410R *instance;
    static void _aiIRQ() { }

    // Read the pixels.
    //
    // 'n' specifies the number of pixels to sample, and is the size of
    // the output array 'pix'.  This can be less than the full number
    // of pixels on the physical device; if it is, we'll spread the
    // sample evenly across the full length of the device by skipping
    // one or more pixels between each sampled pixel to pad out the
    // difference between the sample size and the physical CCD size.
    // For example, if the physical sensor has 1280 pixels, and 'n' is
    // 640, we'll read every other pixel and skip every other pixel.
    // If 'n' is 160, we'll read every 8th pixel and skip 7 between
    // each sample.
    // 
    // The reason that we provide this subset mode (where 'n' is less
    // than the physical pixel count) is that reading a pixel is the most
    // time-consuming part of the scan.  For each pixel we read, we have
    // to wait for the pixel's charge to transfer from its internal smapling
    // capacitor to the CCD's output pin, for that charge to transfer to
    // the KL25Z input pin, and for the KL25Z ADC to get a stable reading.
    // This all takes on the order of 20us per pixel.  Skipping a pixel
    // only requires a clock pulse, which takes about 350ns.  So we can
    // skip 60 pixels in the time it takes to sample 1 pixel.
    //
    // We clock an SI pulse at the beginning of the read.  This starts the
    // next integration cycle: the pixel array will reset on the SI, and 
    // the integration starts 18 clocks later.  So by the time this method
    // returns, the next sample will have been integrating for npix-18 clocks.  
    // That's usually enough time to allow immediately reading the next
    // sample.  If more integration time is required, the caller can simply
    // sleep/spin for the desired additional time, or can do other work that
    // takes the desired additional time.
    //
    // If the caller has other work to tend to that takes longer than the
    // desired maximum integration time, it can call clear() to clock out
    // the current pixels and start a fresh integration cycle.
    void read(register uint16_t *pix, int n)
    {
        Timer t; t.start(); //float tDMA, tPix; // $$$
        
        // get the clock pin pointers into local variables for fast access
        register volatile uint32_t *clockPTOR = &clockPort->PTOR;
        register const uint32_t clockMask = this->clockMask;
        
        // start the next integration cycle by pulsing SI and one clock
        si = 1;
        clock = 1;
        si = 0;
        clock = 0;
        
        // figure how many pixels to skip on each read
        int skip = nPixSensor/n - 1;
        
///$$$
static int done=0;
if (done++ == 0) printf("nPixSensor=%d, n=%d, skip=%d, parallel=%d\r\n", nPixSensor, n, skip, parallel);

        // read all of the pixels
        int dst;
        if (parallel)
        {
            // Parallel mode - read pixels from each half sensor concurrently.
            // Divide 'n' (the output pixel count) by 2 to get the loop count,
            // since we're going to do 2 pixels on each iteration.
            for (n /= 2, dst = 0 ; dst < n ; ++dst)
            {
                // Take the clock high.  The TSL1410R will connect the next
                // pixel pair's hold capacitors to the A01 and AO2 lines 
                // (respectively) on the clock rising edge.
                *clockPTOR = clockMask;

                // Start the ADC sampler for AO1.  The TSL1410R sample 
                // stabilization time per the data sheet is 120ns.  This is
                // fast enough that we don't need an explicit delay, since
                // the instructions to execute this call will take longer
                // than that.
                ao1.start();
                
                // take the clock low while we're waiting for the reading
                *clockPTOR = clockMask;
                
                // Read the first half-sensor pixel from AO1
                pix[dst] = ao1.read_u16();
                
                // Read the second half-sensor pixel from AO2, and store it
                // in the destination array at the current index PLUS 'n',
                // which you will recall contains half the output pixel count.
                // This second pixel is halfway up the sensor from the first 
                // pixel, so it goes halfway up the output array from the
                // current output position.
                ao2.start();
                pix[dst + n] = ao2.read_u16();
                
                // Clock through the skipped pixels
                for (int i = skip ; i > 0 ; --i) 
                {
                    *clockPTOR = clockMask;
                    *clockPTOR = clockMask;
                    *clockPTOR = 0;         // pad the timing with an extra nop write
                }
            }
        }
        else
        {
            // serial mode - read all pixels in a single file

            // clock in the first pixel
            clock = 1;
            clock = 0;
            
            // start the ADC DMA transfer
            ao1.startDMA(pix, n, true);
            
            // We do 4 clock PTOR writes per clocked pixel (the skipped pixels 
            // plus the pixel we actually want to sample), at 32 bits (4 bytes) 
            // each, giving 16 bytes per pixel for the overall write.
            int clk_dma_len = (skip+1)*16;
            clk_dma.start(clk_dma_len);
            
            // start the first sample
            ao1.start();
            
            // read all pixels
            for (dst = n*2 ; dst > 0 ; dst -= 2)
            {
                // wait for the current ADC sample to finish
                while (adc_dma.remaining() >= dst) { }
                
                // start the next analog read while we're finishing the DMA transfers
                ao1.start();
                
                // re-arm the clock DMA
                //clk_dma.restart(clk_dma_len);
            }
            
            // wait for the DMA transfer to finish
            while (adc_dma.isBusy()) { }
            
            // apply the 12-bit to 16-bit rescaling to all values
            for (int i = 0 ; i < n ; ++i)
                pix[i] <<= 4;
        }
        
//$$$
if (done==1) printf(". done: dst=%d\r\n", dst);
        
        // clock out one extra pixel to leave A1 in the high-Z state
        *clockPTOR = clockMask;
        *clockPTOR = clockMask;
        
        if (n >= 64) { totalTime += t.read(); nRuns += 1; } // $$$
    }

    // Clock through all pixels to clear the array.  Pulses SI at the
    // beginning of the operation, which starts a new integration cycle.
    // The caller can thus immediately call read() to read the pixels 
    // integrated while the clear() was taking place.
    void clear()
    {
        // get the clock pin pointers into local variables for fast access
        register FGPIO_Type *clockPort = this->clockPort;
        register uint32_t clockMask = this->clockMask;

        // clock in an SI pulse
        si = 1;
        clockPort->PSOR = clockMask;
        si = 0;
        clockPort->PCOR = clockMask;
        
        // if in serial mode, clock all pixels across both sensor halves;
        // in parallel mode, the pixels are clocked together
        int n = parallel ? nPixSensor/2 : nPixSensor;
        
        // clock out all pixels
        for (int i = 0 ; i < n + 1 ; ++i) {
            clock = 1; // $$$clockPort->PSOR = clockMask;
            clock = 0; // $$$clockPort->PCOR = clockMask;
        }
    }

private:
    SimpleDMA adc_dma;        // DMA controller for reading the analog input
    SimpleDMA clk_dma;        // DMA controller for the sensor SCLK (writes the PTOR register to toggle the clock bit)
    char *dmabuf;             // buffer for DMA transfers
    int nPixSensor;           // number of pixels in physical sensor array
    DigitalOut si;            // GPIO pin for sensor SI (serial data) 
    DigitalOut clock;         // GPIO pin for sensor SCLK (serial clock)
    FGPIO_Type *clockPort;    // IOPORT base address for clock pin - cached for fast writes
    uint32_t clockMask;       // IOPORT register bit mask for clock pin
    AltAnalogIn ao1;          // GPIO pin for sensor AO1 (analog output 1) - we read sensor data from this pin
    AltAnalogIn ao2;          // GPIO pin for sensor AO2 (analog output 2) - 2nd sensor data pin, when in parallel mode
    bool parallel;            // true -> running in parallel mode (we read AO1 and AO2 separately on each clock)
};
 
#endif /* TSL1410R_H */

