/*
 *  TSL1410R interface class.
 *
 *  This provides a high-level interface for the Taos TSL1410R linear CCD array sensor.
 */
 
#include "mbed.h"
#include "config.h"
#include "AltAnalogIn.h"
 
#ifndef TSL1410R_H
#define TSL1410R_H

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
        
        totalTime = 0.0; nRuns = 0; // $$$
    }
    
    float totalTime; int nRuns; // $$$
    
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
        Timer t; t.start();//$$$
        
        // get the clock pin pointers into local variables for fast access
        register volatile uint32_t *clockPSOR = &clockPort->PSOR;
        register volatile uint32_t *clockPCOR = &clockPort->PCOR;
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
                *clockPSOR = clockMask;

                // Start the ADC sampler for AO1.  The TSL1410R sample 
                // stabilization time per the data sheet is 120ns.  This is
                // fast enough that we don't need an explicit delay, since
                // the instructions to execute this call will take longer
                // than that.
                ao1.start();
                
                // take the clock low while we're waiting for the reading
                *clockPCOR = clockMask;
                
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
                    *clockPSOR = clockMask;
                    *clockPCOR = clockMask;
                }
            }
        }
        else
        {
            // serial mode - read all pixels in a single file

            // clock in the first pixel
            *clockPSOR = clockMask;
            *clockPCOR = clockMask;
            
            // clock through the pixels            
            for (dst = 0 ; dst < n ; ++dst)
            {
                // Read this sample
                ao1.start();
                pix[dst] = ao1.read_u16();
                
                // clock in the next pixel
                for (int i = skip ; i >= 0 ; --i) 
                {
                    *clockPSOR = clockMask;
                    *clockPCOR = clockMask;
                    *clockPCOR = clockMask;
                }
            }

            // we're done - stop the ADC sampler            
            ao1.stop();
        }
        
//$$$
//static int xxx = 0; xxx = (xxx+1)%2500; if (xxx==0) printf("tPix=%f, tDMA=%f, diff=%f\r\n", tPix*1000, tDMA*1000, (tDMA - tPix)*1000);
if (done==1) printf(". done: dst=%d\r\n", dst);
        
        // clock out one extra pixel to leave A1 in the high-Z state
        clock = 1;
        clock = 0;
        
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
    SimpleDMA dma;            // DMA controller for reading the analog input
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

