#ifndef ALTANALOGIN_H
#define ALTANALOGIN_H

// This is a modified version of Scissors's FastAnalogIn, customized 
// for the needs of the Pinscape linear image sensor interfaces.  This
// class has a bunch of features to make it even faster than FastAnalogIn,
// including support for 8-bit and 12-bit resolution modes, continuous
// sampling mode, coordination with DMA to move samples into memory
// asynchronously, and client selection of the ADC timing modes.
//
// We need all of this special ADC handling because the image sensors
// have special timing requirements that we can only meet with the
// fastest modes offered by the KL25Z ADC.  The image sensors all
// operate by sending pixel data as a serial stream of analog samples,
// so the minimum time to read a frame is approximately <number of
// pixels in the frame> times <ADC sampling time per sample>.  The
// sensors we currently support vary from 1280 to 1546 pixels per frame.
// With the fastest KL25Z modes, that works out to about 3ms per frame,
// which is just fast enough for our purposes.  Using only the default
// modes in the mbed libraries, frame times are around 30ms, which is
// much too slow to accurately track a fast-moving plunger.
//
// This class works ONLY with the KL25Z.
//
// Important!  This class can't coexist at run-time with the standard
// mbed library version of AnalogIn, or with the original version of 
// FastAnalogIn.  All of these classes program the ADC configuration 
// registers with their own custom settings.  These registers are a 
// global resource, and the different classes all assume they have 
// exclusive control, so they don't try to coordinate with anyone else 
// programming the registers.  A program that uses AltAnalogIn in one 
// place will have to use AltAnalogIn exclusively throughout the 
// program for all ADC interaction.  (It *is* okay to statically link
// the different classes, as long as only one is actually used at
// run-time.  The Pinscape software does this, and selects the one to
// use at run-time according to which plunger class is selected.)

/*
 * Includes
 */
#include "mbed.h"
#include "pinmap.h"
#include "SimpleDMA.h"

// KL25Z definitions
#if defined TARGET_KLXX

// Maximum ADC clock for KL25Z in <= 12-bit mode - 18 MHz per the data sheet
#define MAX_FADC_12BIT      18000000

// Maximum ADC clock for KL25Z in 16-bit mode - 12 MHz per the data sheet
#define MAX_FADC_16BIT      12000000

#define CHANNELS_A_SHIFT     5          // bit position in ADC channel number of A/B mux
#define ADC_CFG1_ADLSMP      0x10       // long sample time mode
#define ADC_SC1_AIEN         0x40       // interrupt enable
#define ADC_SC2_ADLSTS(mode) (mode)     // long sample time select - bits 1:0 of CFG2
#define ADC_SC2_DMAEN        0x04       // DMA enable
#define ADC_SC2_ADTRG        0x40       // Hardware conversion trigger
#define ADC_SC3_CONTINUOUS   0x08       // continuous conversion mode
#define ADC_SC3_AVGE         0x04       // averaging enabled
#define ADC_SC3_AVGS_4       0x00       // 4-sample averaging
#define ADC_SC3_AVGS_8       0x01       // 8-sample averaging
#define ADC_SC3_AVGS_16      0x02       // 16-sample averaging
#define ADC_SC3_AVGS_32      0x03       // 32-sample averaging
#define ADC_SC3_CAL          0x80       // calibration - set to begin calibration
#define ADC_SC3_CALF         0x40       // calibration failed flag

#define ADC_8BIT             0          // 8-bit resolution
#define ADC_12BIT            1          // 12-bit resolution
#define ADC_10BIT            2          // 10-bit resolution
#define ADC_16BIT            3          // 16-bit resolution

// SIM_SOPT7 - enable alternative conversion triggers
#define ADC0ALTTRGEN         0x80

// SIM_SOPT7 ADC0TRGSEL bits for TPMn, n = 0..2
#define ADC0TRGSEL_TPM(n)    (0x08 | (n))  // select TPMn overflow


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
     * @param continuous true to enable continue sampling mode
     * @param long_sample_clocks long sample mode: 0 to disable, ADC clock count to enable (6, 10, 16, or 24)
     * @param averaging number of averaging cycles (1, 4, 8, 16, 32)
     * @param sample_bits sample size in bits (8, 10, 12, 16)
     */
    AltAnalogIn(PinName pin, bool continuous = false, int long_sample_clocks = 0, int averaging = 1, int sample_bits = 8);
    
    ~AltAnalogIn( void )
    {
    }
    
    // Calibrate the ADC.  Per the KL25Z reference manual, this should be
    // done after each CPU reset to get the best accuracy from the ADC.
    //
    // The calibration process runs synchronously (blocking) and takes
    // about 2ms.  Per the reference manual guidelines, we calibrate
    // using the same timing parameters configured in the constructor,
    // but we use the maximum averaging rounds.
    //
    // The calibration depends on the timing parameters, so if multiple
    // AltAnalogIn objects will be used in the same application, the
    // configuration established for one object might not be ideal for 
    // another.  The advice in the reference manual is to calibrate once
    // at the settings where the highest accuracy will be needed.  It's
    // also possible to capture the configuration data from the ADC
    // registers after a configuration and restore them later by writing
    // the same values back to the registers, for relatively fast switching
    // between calibration sets, but that's beyond the scope of this class.
    void calibrate();
    
    // Initialize DMA.  This connects the ADC port to the given DMA
    // channel.  This doesn't actually initiate a transfer; this just
    // connects the ADC to the DMA channel for later transfers.  Use
    // the DMA object to set up a transfer, and use one of the trigger
    // modes (e.g., start() for software triggering) to initiate a
    // sample.
    void initDMA(SimpleDMA *dma);
    
    // Enable interrupts.  This doesn't actually set up a handler; the
    // caller is responsible for that.  This merely sets the ADC registers
    // so that the ADC generates an ADC0_IRQ interrupt request each time
    // the sample completes.
    //
    // Note that the interrupt handler must read from ADC0->R[0] before
    // returning, which has the side effect of clearning the COCO (conversion
    // complete) flag in the ADC registers.  When interrupts are enabled,
    // the ADC asserts the ADC0_IRQ interrupt continuously as long as the
    // COCO flag is set, so if the ISR doesn't explicitly clear COCO before
    // it returns, another ADC0_IRQ interrupt will immediate occur as soon
    // as the ISR returns, so we'll be stuck in an infinite loop of calling
    // the ISR over and over.
    void enableInterrupts();
        
    // Start a sample.  This sets the ADC multiplexer to read from
    // this input and activates the sampler.
    inline void start()
    {
        // select my channel
        selectChannel();
        
        // set our SC1 bits - this initiates the sample
        ADC0->SC1[1] = sc1;
        ADC0->SC1[0] = sc1;
    }

    // Set the ADC to trigger on a TPM channel, and start sampling on
    // the trigger.  This can be used to start ADC samples in sync with a 
    // clock signal we're generating via a TPM.  The ADC is triggered each 
    // time the TPM counter overflows, which makes it trigger at the start 
    // of each PWM period on the unit.
    void setTriggerTPM(int tpmUnitNumber);
    
    // stop sampling
    void stop()
    {
        // set the channel bits to binary 11111 to disable sampling
        ADC0->SC1[0] = 0x1F;
    }
    
    // Resume sampling after a pause.
    inline void resume()  
    {
        // restore our SC1 bits
        ADC0->SC1[1] = sc1;
        ADC0->SC1[0] = sc1;
    }    
    
    // Wait for the current sample to complete.
    //
    // IMPORTANT!  DO NOT use this if DMA is enabled on the ADC.  It'll
    // always gets stuck in an infinite loop, because the CPU will never
    // be able to observe the COCO bit being set when DMA is enabled.  The
    // reason is that the DMA controller always reads its configured source
    // address when triggered.  The DMA source address for the ADC is the 
    // ADC result register ADC0->R[0], and reading that register by any 
    // means clears COCO.  And the DMA controller ALWAYS gets to it first,
    // so the CPU will never see COCO set when DMA is enabled.  It doesn't
    // matter whether or not a DMA transfer is actually running, either -
    // it's enough to merely enable DMA on the ADC.
    inline void wait()
    {
        while (!isReady()) ;
    }
    
    // Is the sample ready?
    //
    // NOTE: As with wait(), the CPU will NEVER observe the COCO bit being
    // set if DMA is enabled on the ADC.  This will always return false if
    // DMA is enabled.  (Not our choice - it's a hardware feature.)
    inline bool isReady()
    {
        return (ADC0->SC1[0] & ADC_SC1_COCO_MASK) != 0;
    }

    
private:
    uint32_t id;                // unique ID
    SimpleDMA *dma;             // DMA controller, if used
    char ADCnumber;             // ADC number of our input pin
    char ADCmux;                // multiplexer for our input pin (0=A, 1=B)
    uint32_t sc1;               // SC1 register settings for this input
    uint32_t sc1_aien;
    uint32_t sc2;               // SC2 register settings for this input
    uint32_t sc3;               // SC3 register settings for this input
    
    // Switch to this channel if it's not the currently selected channel.
    // We do this as part of start() (software triggering) or any hardware
    // trigger setup.
    static int lastMux;
    static uint32_t lastId;
    void selectChannel()
    {
        // update the MUX bit in the CFG2 register only if necessary
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
        if (id != lastId) 
        {
            // set our ADC0 SC2 and SC3 configuration bits
            ADC0->SC2 = sc2;
            ADC0->SC3 = sc3;
        
            // we're the active one now
            lastId = id;
        }
    }
    
    // Unselect the channel.  This clears our internal flag for which
    // configuration was selected last, so that we restore settings on
    // the next start or trigger operation.
    void unselectChannel() { lastId = 0; }
};

// 8-bit sampler subclass
class AltAnalogIn_8bit : public AltAnalogIn
{
public:
    AltAnalogIn_8bit(PinName pin, bool continuous = false, int long_sample_clocks = 0, int averaging = 1) :
        AltAnalogIn(pin, continuous, long_sample_clocks, averaging, 8) { }

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
};

// 16-bit sampler subclass
class AltAnalogIn_16bit : public AltAnalogIn
{
public:
    AltAnalogIn_16bit(PinName pin, bool continuous = false, int long_sample_clocks = 0, int averaging = 1) :
        AltAnalogIn(pin, continuous, long_sample_clocks, averaging, 16) { }

    /** Returns the raw value
    *
    * @param return Unsigned integer with converted value
    */
    inline uint16_t read_u16()
    {
        // wait for the hardware to signal that the sample is completed
        wait();
    
        // return the result register value
        return (uint16_t)ADC0->R[0];
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
};

#endif
