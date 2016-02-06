#if defined(TARGET_KLXX) || defined(TARGET_K20D50M)

#include "AltAnalogIn.h"
#include "clk_freqs.h"

// Maximum ADC clock for KL25Z in 12-bit mode.  The data sheet says this is
// limited to 18MHz, but we seem to get good results at higher rates.  The
// data sheet is actually slightly vague on this because it's only in the
// table for the 16-bit ADC, even though the ADC we're using is a 12-bit ADC,
// which seems to have slightly different properties.  So there's room to
// think the data sheet omits the data for the 12-bit ADC.
#define MAX_FADC_12BIT      25000000

#define CHANNELS_A_SHIFT     5          // bit position in ADC channel number of A/B mux
#define ADC_CFG1_ADLSMP      0x10       // long sample time mode
#define ADC_SC2_ADLSTS(mode) (mode)     // long sample time select - bits 1:0 of CFG2

#ifdef TARGET_K20D50M
static const PinMap PinMap_ADC[] = {
    {PTC2, ADC0_SE4b, 0},
    {PTD1, ADC0_SE5b, 0},
    {PTD5, ADC0_SE6b, 0},
    {PTD6, ADC0_SE7b, 0},
    {PTB0, ADC0_SE8,  0},
    {PTB1, ADC0_SE9,  0},
    {PTB2, ADC0_SE12, 0},
    {PTB3, ADC0_SE13, 0},
    {PTC0, ADC0_SE14, 0},
    {PTC1, ADC0_SE15, 0},
    {NC,   NC,        0}
};
#endif

AltAnalogIn::AltAnalogIn(PinName pin, bool enabled)
{
    // do nothing if explicitly not connected
    if (pin == NC)
        return;
    
    // figure our ADC number
    ADCnumber = (ADCName)pinmap_peripheral(pin, PinMap_ADC);
    if (ADCnumber == (ADCName)NC) {
        error("ADC pin mapping failed");
    }
    
    // figure our multiplexer channel (A or B)
    ADCmux = (ADCnumber >> CHANNELS_A_SHIFT) ^ 1;

    // enable the ADC0 clock in the system control module
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;

    // enable the port clock gate for the port containing our GPIO pin
    uint32_t port = (uint32_t)pin >> PORT_SHIFT;
    SIM->SCGC5 |= 1 << (SIM_SCGC5_PORTA_SHIFT + port);
        
    // Figure the maximum clock frequency.  In 12-bit mode or less, we can 
    // run the ADC at up to 18 MHz per the KL25Z data sheet.  (16-bit mode
    // is limited to 12 MHz.)
    int clkdiv = 0;
    uint32_t ourfreq = bus_frequency();
    for ( ; ourfreq > MAX_FADC_12BIT ; ourfreq /= 2, clkdiv += 1) ;
    
    // Set the "high speed" configuration only if we're right at the bus speed
    // limit.  This bit is somewhat confusingly named, in that it actually
    // *slows down* the conversions.  "High speed" means that the *other*
    // options are set right at the limits of the ADC, so this option adds
    // a few extra cycle delays to every conversion to compensate for living
    // on the edge.
    uint32_t adhsc_bit = (ourfreq == MAX_FADC_12BIT ? ADC_CFG2_ADHSC_MASK : 0);
    
    printf("ADCnumber=%d, cfg2_muxsel=%d, bus freq=%ld, clkdiv=%d\r\n", ADCnumber, ADCmux, bus_frequency(), clkdiv);

    // set up the ADC control registers 

    ADC0->CFG1 = ADC_CFG1_ADIV(clkdiv)  // Clock Divide Select (as calculated above)
               | ADC_CFG1_MODE(1)       // Sample precision = 12-bit
               | ADC_CFG1_ADICLK(0);    // Input Clock = bus clock

    ADC0->CFG2 = adhsc_bit              // High-Speed Configuration, if needed
               | ADC_CFG2_ADLSTS(3);    // Long sample time mode 3 -> 6 ADCK cycles total
               
    ADC0->SC2 = ADC_SC2_REFSEL(0);      // Default Voltage Reference
    
    ADC0->SC3 = 0;                      // Calibration mode off, single sample, averaging disabled

    // map the GPIO pin in the system multiplexer to the ADC
    pinmap_pinout(pin, PinMap_ADC);
    
    // figure our 'start' mask - this is the value we write to the SC1A register
    // to initiate a new sample
    startMask = ADC_SC1_ADCH(ADCnumber & ~(1 << CHANNELS_A_SHIFT));
}

#endif //defined TARGET_KLXX
