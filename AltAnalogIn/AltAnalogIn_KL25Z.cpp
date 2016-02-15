#if defined(TARGET_KLXX) || defined(TARGET_K20D50M)

#include "AltAnalogIn.h"
#include "clk_freqs.h"

//$$$AltAnalogIn *AltAnalogIn::intInstance = 0;

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

AltAnalogIn::AltAnalogIn(PinName pin, bool continuous)
{
    // set our unique ID
    static uint32_t nextID = 1;
    id = nextID++;
    
    // presume no DMA 
    dma = 0;
    
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
    uint32_t adcfreq = bus_frequency();
    for ( ; adcfreq > MAX_FADC_12BIT ; adcfreq /= 2, clkdiv += 1) ;
    
    // The "high speed configuration" bit is required if the ADC clock 
    // frequency is above a certain threshold.  The actual threshold is 
    // poorly documented: the reference manual only says that it's required
    // when running the ADC at "high speed" but doesn't define how high
    // "high" is.  The only numerical figure I can find is in the Freescale
    // ADC sample time calculator tool (a Windows program downloadable from
    // the Freescale site), which has a little notation on the checkbox for
    // the ADHSC bit that says to use it when the ADC clock is 8 MHz or
    // higher.
    //
    // Note that this bit is somewhat confusingly named.  It doesn't mean
    // "make the ADC go faster".  It actually means just the opposite.
    // What it really means is that the external clock is running so fast 
    // that the ADC has to pad out its sample time slightly to compensate,
    // by adding a couple of extra clock cycles to each sampling interval.
    const uint32_t ADHSC_SPEED_LIMIT = 8000000;
    uint32_t adhsc_bit = (adcfreq >= ADHSC_SPEED_LIMIT ? ADC_CFG2_ADHSC_MASK : 0);
    
    // $$$
    printf("ADCnumber=%d, cfg2_muxsel=%d, bus freq=%ld, clkdiv=%d, adc freq=%d, high speed config=%s\r\n", 
        ADCnumber, ADCmux, bus_frequency(), clkdiv, adcfreq, adhsc_bit ? "Y" : "N");//$$$

    // map the GPIO pin in the system multiplexer to the ADC
    pinmap_pinout(pin, PinMap_ADC);
    
    // set up the ADC control registers - these are common to all users of this class
    
    ADC0->CFG1 = ADC_CFG1_ADIV(clkdiv)  // Clock Divide Select (as calculated above)
               //| ADC_CFG1_ADLSMP        // Long sample time
               | ADC_CFG1_MODE(1)       // Sample precision = 12-bit
               | ADC_CFG1_ADICLK(0);    // Input Clock = bus clock

    ADC0->CFG2 = adhsc_bit              // High-Speed Configuration, if needed
               //| ADC_CFG2_ADLSTS(0);    // Long sample time mode 0 -> 24 ADCK cycles total
               //| ADC_CFG2_ADLSTS(1);    // Long sample time mode 1 -> 16 ADCK cycles total
               //| ADC_CFG2_ADLSTS(2);    // Long sample time mode 2 -> 10 ADCK cycles total
               | ADC_CFG2_ADLSTS(3);    // Long sample time mode 2 -> 6 ADCK cycles total
               
    // Figure our SC1 register bits
    sc1 = ADC_SC1_ADCH(ADCnumber & ~(1 << CHANNELS_A_SHIFT));

    // figure our SC2 register bits
    sc2 = ADC_SC2_REFSEL(0);            // Default Voltage Reference

    // Set our SC3 bits.  The defaults (0 bits) are calibration mode off,
    // single sample, averaging disabled.
    sc3 = (continuous ? ADC_SC3_CONTINUOUS : 0);    // enable continuous mode if desired
}

void AltAnalogIn::initDMA(SimpleDMA *dma)
{
    // remember the DMA interface object
    this->dma = dma;
    
    // set to read from the ADC result register
    dma->source(&ADC0->R[0], false, 16);
    
    // set to trigger on the ADC
    dma->trigger(Trigger_ADC0);

    // enable DMA in our SC2 bits
    sc2 |= ADC_SC2_DMAEN;
}


#endif //defined TARGET_KLXX
