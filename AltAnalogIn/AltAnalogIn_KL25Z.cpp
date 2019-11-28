#if defined(TARGET_KLXX) || defined(TARGET_K20D50M)

#include "AltAnalogIn.h"
#include "clk_freqs.h"

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

// statics
int AltAnalogIn::lastMux = -1;
uint32_t AltAnalogIn::lastId = 0;

AltAnalogIn::AltAnalogIn(PinName pin, bool continuous, int long_sample_clocks, int averaging, int sample_bits)
{
    // set our unique ID
    static uint32_t nextID = 1;
    id = nextID++;
    
    // presume no DMA or interrupts
    dma = 0;
    sc1_aien = 0;
    
    // do nothing if explicitly not connected
    if (pin == NC)
        return;
            
    // validate the sample bit size, and figure the ADC_xxBIT code for it
    uint32_t adc_xxbit = ADC_8BIT;
    switch (sample_bits)
    {
    case 8:
        adc_xxbit = ADC_8BIT;
        break;
        
    case 10:
        adc_xxbit = ADC_10BIT;
        break;
        
    case 12:
        adc_xxbit = ADC_12BIT;
        break;
        
    case 16:
        adc_xxbit = ADC_16BIT;
        break;
        
    default:
        error("invalid sample size for AltAnalogIn - must be 8, 10, 12, or 16 bits");
    }
    
    // validate the long sample mode
    uint32_t cfg1_adlsmp = ADC_CFG1_ADLSMP;
    uint32_t cfg2_adlsts = ADC_CFG2_ADLSTS(3);
    switch (long_sample_clocks)
    {
    case 0:
        // disable long sample mode
        cfg1_adlsmp = 0;
        cfg2_adlsts = ADC_CFG2_ADLSTS(3);
        break;
        
    case 6:
        cfg1_adlsmp = ADC_CFG1_ADLSMP;  // enable long sample mode
        cfg2_adlsts = ADC_CFG2_ADLSTS(3);  // Long sample time mode 3 -> 6 ADCK cycles total
        break;
        
    case 10:
        cfg1_adlsmp = ADC_CFG1_ADLSMP;  // enable long sample mode
        cfg2_adlsts = ADC_CFG2_ADLSTS(2); // Long sample time mode 2 -> 10 ADCK cycles total
        break;
        
    case 16:
        cfg1_adlsmp = ADC_CFG1_ADLSMP;  // enable long sample mode
        cfg2_adlsts = ADC_CFG2_ADLSTS(1); // Long sample time mode 1 -> 16 ADCK cycles total
        break;
        
    case 24:
        cfg1_adlsmp = ADC_CFG1_ADLSMP;  // enable long sample mode
        cfg2_adlsts = ADC_CFG2_ADLSTS(0); // Long sample time mode 0 -> 24 ADCK cycles total
        break;
        
    default:
        error("invalid long sample mode clock count - must be 0 (disabled), 6, 10, 16, or 24");
    }
    
    // figure the averaging bits
    uint32_t sc3_avg = 0;
    switch (averaging)
    {
    case 0:
    case 1:
        // 0/1 = no averaging
        sc3_avg = 0;
        break;
        
    case 4:
        sc3_avg = ADC_SC3_AVGE | ADC_SC3_AVGS_4;
        break;
        
    case 8:
        sc3_avg = ADC_SC3_AVGE | ADC_SC3_AVGS_8;
        break;
        
    case 16:
        sc3_avg = ADC_SC3_AVGE | ADC_SC3_AVGS_16;
        break;
        
    case 32:
        sc3_avg = ADC_SC3_AVGE | ADC_SC3_AVGS_32;
        break;
        
    default:
        error("invalid ADC averaging count: must be 1, 4, 8, 16, or 32");
    }
    
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
    uint32_t maxfreq = sample_bits <= 12 ? MAX_FADC_12BIT : MAX_FADC_16BIT;
    for ( ; adcfreq > maxfreq ; adcfreq /= 2, clkdiv += 1) ;
    
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
    
    // map the GPIO pin in the system multiplexer to the ADC
    pinmap_pinout(pin, PinMap_ADC);
    
    // set up the ADC control registers - these are common to all users of this class
    
    ADC0->CFG1 = ADC_CFG1_ADIV(clkdiv)    // Clock Divide Select (as calculated above)
               | cfg1_adlsmp              // Long sample time
               | ADC_CFG1_MODE(adc_xxbit) // Sample precision
               | ADC_CFG1_ADICLK(0);      // Input Clock = bus clock

    ADC0->CFG2 = adhsc_bit                // High-Speed Configuration, if needed
               | cfg2_adlsts;             // long sample time mode
               
    // Figure our SC1 register bits
    sc1 = ADC_SC1_ADCH(ADCnumber & ~(1 << CHANNELS_A_SHIFT))
        | sc1_aien;

    // figure our SC2 register bits
    sc2 = ADC_SC2_REFSEL(0);              // Default Voltage Reference

    // Set our SC3 bits.  The defaults (0 bits) are calibration mode off,
    // single sample, averaging disabled.
    sc3 = (continuous ? ADC_SC3_CONTINUOUS : 0) // enable continuous mode if desired
        | sc3_avg;                        // sample averaging mode bits
}

void AltAnalogIn::calibrate()
{
    // Select our channel to set up the MUX and SC2/SC3 registers.  This
    // will set up the clock source and sample time we'll use to take
    // actual samples.
    selectChannel();
    
    // Make sure DMA is disabled on the channel, so that we can see COCO.
    // Also make sure that software triggering is in effect.
    ADC0->SC2 &= ~(ADC_SC2_DMAEN | ADC_SC2_ADTRG);
    
    // clear any past calibration results
    ADC0->SC3 |= ADC_SC3_CALF;
    
    // select 32X averaging mode for highest accuracy, and begin calibration
    ADC0->SC3 = (sc3 & ~ADC_SC3_AVGS_MASK) | ADC_SC3_AVGS_32 | ADC_SC3_CAL;
    
    // Wait for calibration to finish, but not more than 10ms, just in 
    // case something goes wrong in the setup.
    Timer t;
    t.start();
    uint32_t t0 = t.read_us();
    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0 && static_cast<uint32_t>(t.read_us() - t0) < 10000) ;
    
    // debugging
    // printf("ADC calibration %s, run time %u us\r\n", 
    //     (ADC0->SC3 & ADC_SC3_CALF) != 0 ? "error" : "ok",
    //     static_cast<uint32_t>(t.read_us() - t0));
    
    // Check results
    if ((ADC0->SC3 & ADC_SC3_CALF) == 0)
    {
        // Success - calculate the plus-side calibration results and store
        // in the PG register.  (This procedure is from reference manual.)
        uint16_t sum = 0;
        sum += ADC0->CLP0;
        sum += ADC0->CLP1;
        sum += ADC0->CLP2;
        sum += ADC0->CLP3;
        sum += ADC0->CLP4;
        sum += ADC0->CLPS;
        sum /= 2;
        sum |= 0x8000;
        ADC0->PG = sum;
        
        // do the same for the minus-side results
        sum = 0;
        sum += ADC0->CLM0;
        sum += ADC0->CLM1;
        sum += ADC0->CLM2;
        sum += ADC0->CLM3;
        sum += ADC0->CLM4;
        sum += ADC0->CLMS;
        sum /= 2;
        sum |= 0x8000;
        ADC0->MG = sum;
    }
    
    // Clear any error (this is one of those perverse cases where we clear
    // a bit in a peripheral by writing 1 to the bit)
    ADC0->SC3 |= ADC_SC3_CALF;
    
    // restore our normal SC2 and SC3 settings
    ADC0->SC2 = sc2;
    ADC0->SC3 = sc3;
    
    // un-select the channel so that we reset all registers next time
    unselectChannel();
}

void AltAnalogIn::enableInterrupts()
{
    sc1_aien = ADC_SC1_AIEN;
    sc1 |= ADC_SC1_AIEN;
}

void AltAnalogIn::initDMA(SimpleDMA *dma)
{
    // remember the DMA interface object
    this->dma = dma;
    
    // set to read from the ADC result register
    dma->source(&ADC0->R[0], false, 8);
    
    // set to trigger on the ADC
    dma->trigger(Trigger_ADC0);

    // enable DMA in our SC2 bits
    sc2 |= ADC_SC2_DMAEN;
}

void AltAnalogIn::setTriggerTPM(int tpmUnitNumber)
{
    // select my channel
    selectChannel();

    // set the hardware trigger for the ADC to the specified TPM unit
    SIM->SOPT7 = ADC0ALTTRGEN | ADC0TRGSEL_TPM(tpmUnitNumber);
    
    // set the ADC to hardware trigger mode
    ADC0->SC2 = sc2 | ADC_SC2_ADTRG;

    // set SC1a and SC1b
    ADC0->SC1[0] = sc1;
    ADC0->SC1[1] = sc1;
}

#endif //defined TARGET_KLXX
