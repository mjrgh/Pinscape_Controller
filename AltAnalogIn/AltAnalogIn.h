#ifndef ALTANALOGIN_H
#define ALTANALOGIN_H

// This is a slightly modified version of Scissors's FastAnalogIn.
// 
// This version is optimized for reading from multiple inputs.  The KL25Z has 
// multiple ADC channels, but the multiplexer hardware only allows sampling one
// at a time.  The entire sampling process from start to finish is serialized 
// in the multiplexer, so we unfortunately can't overlap the sampling times
// for multiple channels - we have to wait in sequence for the sampling period
// on each channel, one after the other.
//
// The base version of FastAnalogIn uses the hardware's continuous conversion
// feature to speed up sampling.  When sampling multiple inputs, that feature
// becomes useless, and in fact the way FastAnalogIn uses it creates additional
// overhead for multiple input sampling.  But FastAnalogIn still has some speed
// advantages over the base mbed AnalogIn implementation, since it sets all of
// the other conversion settings to the fastest options.  This version keeps the
// other speed-ups from FastAnalogIn, but dispenses with the continuous sampling.

/*
 * Includes
 */
#include "mbed.h"
#include "pinmap.h"

#if !defined TARGET_LPC1768 && !defined TARGET_KLXX && !defined TARGET_LPC408X && !defined TARGET_LPC11UXX && !defined TARGET_K20D5M
    #error "Target not supported"
#endif

 /** A class similar to AnalogIn, only faster, for LPC1768, LPC408X and KLxx
 *
 * AnalogIn does a single conversion when you read a value (actually several conversions and it takes the median of that).
 * This library runns the ADC conversion automatically in the background.
 * When read is called, it immediatly returns the last sampled value.
 *
 * LPC1768 / LPC4088
 * Using more ADC pins in continuous mode will decrease the conversion rate (LPC1768:200kHz/LPC4088:400kHz).
 * If you need to sample one pin very fast and sometimes also need to do AD conversions on another pin,
 * you can disable the continuous conversion on that ADC channel and still read its value.
 *
 * KLXX
 * Multiple Fast instances can be declared of which only ONE can be continuous (all others must be non-continuous).
 *
 * When continuous conversion is disabled, a read will block until the conversion is complete
 * (much like the regular AnalogIn library does).
 * Each ADC channel can be enabled/disabled separately.
 *
 * IMPORTANT : It does not play nicely with regular AnalogIn objects, so either use this library or AnalogIn, not both at the same time!!
 *
 * Example for the KLxx processors:
 * @code
 * // Print messages when the AnalogIn is greater than 50%
 *
 * #include "mbed.h"
 *
 * AltAnalogIn temperature(PTC2); //Fast continuous sampling on PTC2
 * AltAnalogIn speed(PTB3, 0);    //Fast non-continuous sampling on PTB3
 *
 * int main() {
 *     while(1) {
 *         if(temperature > 0.5) {
 *             printf("Too hot! (%f) at speed %f", temperature.read(), speed.read());
 *         }
 *     }
 * }
 * @endcode
 * Example for the LPC1768 processor:
 * @code
 * // Print messages when the AnalogIn is greater than 50%
 *
 * #include "mbed.h"
 *
 * AltAnalogIn temperature(p20);
 *
 * int main() {
 *     while(1) {
 *         if(temperature > 0.5) {
 *             printf("Too hot! (%f)", temperature.read());
 *         }
 *     }
 * }
 * @endcode
*/
class AltAnalogIn {

public:
     /** Create an AltAnalogIn, connected to the specified pin
     *
     * @param pin AnalogIn pin to connect to
     * @param enabled Enable the ADC channel (default = true)
     */
    AltAnalogIn( PinName pin, bool enabled = true );
    
    ~AltAnalogIn( void )
    {
    }
    
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
        
        // select our ADC channel in the control register - this initiates sampling
        // on the channel
        ADC0->SC1[0] = startMask;
    }
 

    
    /** Returns the raw value
    *
    * @param return Unsigned integer with converted value
    */
    inline uint16_t read_u16()
    {
        // wait for the hardware to signal that the sample is completed
        while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0);
    
        // return the result register value
        return (uint16_t)ADC0->R[0] << 4;  // convert 12-bit to 16-bit, padding with zeroes
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
    char ADCnumber;         // ADC number of our input pin
    char ADCmux;            // multiplexer for our input pin (0=A, 1=B)
    uint32_t startMask;
};

#endif
