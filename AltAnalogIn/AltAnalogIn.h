#ifndef ALTANALOGIN_H
#define ALTANALOGIN_H

// This is a modified version of Scissors's FastAnalogIn, customized 
// for the needs of the Pinscape TSL1410R reader.  We use 8-bit samples
// to save memory (since we need to collect 1280 or 1536 samples,
// depending on the sensor subtype), and we use the fastest sampling
// parameters (determined through testing).  For maximum throughput,
// we put the ADC in continuous mode and read samples with a DMA
// channel.
//
// This modified version only works for the KL25Z.
//
// Important!  This class can't coexist in the same program with the 
// standard mbed library version of AnalogIn, or with the original 
// version of FastAnalogIn.  All of these classes program the ADC
// configuration registers with their own custom settings.  These
// registers are a global resource, and the different classes all
// assume they have exclusive control, so they don't try to coordinate
// with anyone else programming the registers.  A program that uses
// AltAnalogIn in one place will have to use AltAnalogIn exclusively
// throughout the program for all ADC interaction.

/*
 * Includes
 */
#include "mbed.h"
#include "pinmap.h"
#include "SimpleDMA.h"

// KL25Z definitions
#if defined TARGET_KLXX

// Maximum ADC clock for KL25Z in 12-bit mode - 18 MHz per the data sheet
#define MAX_FADC_12BIT      18000000

#define CHANNELS_A_SHIFT     5          // bit position in ADC channel number of A/B mux
#define ADC_CFG1_ADLSMP      0x10       // long sample time mode
#define ADC_SC1_AIEN         0x40       // interrupt enable
#define ADC_SC2_ADLSTS(mode) (mode)     // long sample time select - bits 1:0 of CFG2
#define ADC_SC2_DMAEN        0x04       // DMA enable
#define ADC_SC3_CONTINUOUS   0x08       // continuous conversion mode

#define ADC_8BIT             0          // 8-bit resolution
#define ADC_12BIT            1          // 12-bit resolution
#define ADC_10BIT            2          // 10-bit resolution
#define ADC_16BIT            3          // 16-bit resolution

#else
    #error "This target is not currently supported"
#endif

#if !defined TARGET_LPC1768 && !defined TARGET_KLXX && !defined TARGET_LPC408X && !defined TARGET_LPC11UXX && !defined TARGET_K20D5M
    #error "Target not supported"
#endif


class AltAnalogIn {

public:
     /** Create an AltAnalogIn, connected to the specified pin
     *
     * @param pin AnalogIn pin to connect to
     * @param enabled Enable the ADC channel (default = true)
     */
    AltAnalogIn(PinName pin, bool continuous = false);
    
    ~AltAnalogIn( void )
    {
    }
    
    // Initialize DMA.  This connects the analog in port to the
    // given DMA object.
    //
    // DMA transfers from the analog in port often use continuous
    // conversion mode.  Note, however, that we don't automatically
    // assume this - single sample mode is the default, which means
    // that you must manually start each sample.  If you want to use
    // continuous mode, you need to set that separately (via the 
    // constructor).
    void initDMA(SimpleDMA *dma);
        
    /** Start a sample.  This sets the ADC multiplexer to read from
    * this input and activates the sampler.
    */
    inline void start()
    {
        // update the MUX bit in the CFG2 register only if necessary
        static int lastMux = -1;
        if (lastMux != ADCmux) 
        {
            // remember the new register value
            lastMux = ADCmux;
        
            // select the multiplexer for our ADC channel
            if (ADCmux)
                ADC0->CFG2 |= ADC_CFG2_MUXSEL_MASK;
            else
                ADC0->CFG2 &= ~ADC_CFG2_MUXSEL_MASK;
        }
        
        // update the SC2 and SC3 bits only if we're changing inputs
        static uint32_t lastid = 0;
        if (id != lastid) 
        {
            // set our ADC0 SC2 and SC3 configuration bits
            ADC0->SC2 = sc2;
            ADC0->SC3 = sc3;
        
            // we're the active one now
            lastid = id;
        }
        
        // set our SC1 bits - this initiates the sample
        ADC0->SC1[0] = sc1;
    }
 
    // stop sampling
    void stop()
    {
        // set the channel bits to binary 11111 to disable sampling
        ADC0->SC1[0] = 0x1F;
    }
    
    // wait for the current sample to complete
    inline void wait()
    {
        while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0);
    }

    
    /** Returns the raw value
    *
    * @param return Unsigned integer with converted value
    */
    inline uint16_t read_u16()
    {
        // wait for the hardware to signal that the sample is completed
        wait();
    
        // return the result register value
        return (uint16_t)ADC0->R[0] << 8;  // convert 16-bit to 16-bit, padding with zeroes
    }
    
    /** Returns the scaled value
    *
    * @param return Float with scaled converted value to 0.0-1.0
    */
    float read(void)
    {
        unsigned short value = read_u16();
        return value / 65535.0f;
    }
    
    /** An operator shorthand for read()
    */
    operator float() { return read(); }

    
private:
    uint32_t id;                // unique ID
    SimpleDMA *dma;             // DMA controller, if used
    char ADCnumber;             // ADC number of our input pin
    char ADCmux;                // multiplexer for our input pin (0=A, 1=B)
    uint32_t sc1;               // SC1 register settings for this input
    uint32_t sc2;               // SC2 register settings for this input
    uint32_t sc3;               // SC3 register settings for this input
};

#endif
