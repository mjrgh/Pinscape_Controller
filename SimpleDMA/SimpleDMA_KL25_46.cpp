#if defined TARGET_KL25Z || defined TARGET_KL46Z
#include "SimpleDMA.h"

// DMA - Register Layout Typedef (from mbed MKL25Z4.h)
// Break out the DMA[n] register array as a named struct,
// so that we can create pointers to it.
typedef struct {
    __IO uint32_t SAR;                               /**< Source Address Register, array offset: 0x100, array step: 0x10 */
    __IO uint32_t DAR;                               /**< Destination Address Register, array offset: 0x104, array step: 0x10 */
    union {                                          /* offset: 0x108, array step: 0x10 */
      __IO uint32_t DSR_BCR;                           /**< DMA Status Register / Byte Count Register, array offset: 0x108, array step: 0x10 */
      struct {                                         /* offset: 0x108, array step: 0x10 */
             uint8_t RESERVED_0[3];
        __IO uint8_t DSR;                                /**< DMA_DSR0 register...DMA_DSR3 register., array offset: 0x10B, array step: 0x10 */
      } DMA_DSR_ACCESS8BIT;
    };
    __IO uint32_t DCR;                               /**< DMA Control Register, array offset: 0x10C, array step: 0x10 */
} DMA_Reg_Type;
typedef struct {
    union {                                          /* offset: 0x0 */
        __IO uint8_t REQC_ARR[4];                        /**< DMA_REQC0 register...DMA_REQC3 register., array offset: 0x0, array step: 0x1 */
    };
    uint8_t RESERVED_0[252];
    DMA_Reg_Type DMA[4];
} MyDMA_Type;


SimpleDMA *SimpleDMA::irq_owner[4] = {NULL};

void SimpleDMA::class_init()
{
    static bool inited = false;
    if (!inited)
    {
        NVIC_SetVector(DMA0_IRQn, (uint32_t)&irq_handler0);
        NVIC_SetVector(DMA1_IRQn, (uint32_t)&irq_handler1);
        NVIC_SetVector(DMA2_IRQn, (uint32_t)&irq_handler2);
        NVIC_SetVector(DMA3_IRQn, (uint32_t)&irq_handler3);
        NVIC_EnableIRQ(DMA0_IRQn);
        NVIC_EnableIRQ(DMA1_IRQn);
        NVIC_EnableIRQ(DMA2_IRQn);
        NVIC_EnableIRQ(DMA3_IRQn);
        inited = true;
    }
}

SimpleDMA::SimpleDMA(int channel) 
{
    class_init();

    // remember the channel (-1 means we automatically select on start())
    this->channel(channel);
       
    // Enable DMA
    SIM->SCGC6 |= 1<<1;     // Enable clock to DMA mux
    SIM->SCGC7 |= 1<<8;     // Enable clock to DMA
    
    // use the "always" software trigger by default
    trigger(Trigger_ALWAYS);
   
    // presume no link channels
    linkMode = 0;
    linkChannel1 = 0;
    linkChannel2 = 0;
}

int SimpleDMA::start(uint32_t length, bool wait)
{
    if (auto_channel)
        _channel = getFreeChannel();
    else if (!wait && isBusy())
        return -1;
    else {
        while (isBusy());
    }
    
    if (length > DMA_DSR_BCR_BCR_MASK)
        return -1;

    irq_owner[_channel] = this;
    
    // get pointers to the register locations
    volatile uint8_t *chcfg = &DMAMUX0->CHCFG[_channel];
    volatile DMA_Reg_Type *dmareg = (volatile DMA_Reg_Type *)&DMA0->DMA[_channel];

    // disable the channel while we're setting it up
    *chcfg = 0;
    
    // set the DONE flag on the channel
    dmareg->DSR_BCR = DMA_DSR_BCR_DONE_MASK;

    uint32_t config = 
        DMA_DCR_EINT_MASK 
        | DMA_DCR_ERQ_MASK 
        | DMA_DCR_CS_MASK
        | ((source_inc & 0x01) << DMA_DCR_SINC_SHIFT) 
        | ((destination_inc & 0x01) << DMA_DCR_DINC_SHIFT)
        | ((linkChannel1 & 0x03) << DMA_DCR_LCH1_SHIFT)
        | ((linkChannel2 & 0x03) << DMA_DCR_LCH2_SHIFT)
        | ((linkMode & 0x03) << DMA_DCR_LINKCC_SHIFT);
        
    switch (source_size) 
    {
    case 8:
        config |= 1 << DMA_DCR_SSIZE_SHIFT;
        break;

    case 16:
        config |= 2 << DMA_DCR_SSIZE_SHIFT; 
        break;
    }

    switch (destination_size) 
    {
    case 8:
        config |= 1 << DMA_DCR_DSIZE_SHIFT;
        break;

    case 16:
        config |= 2 << DMA_DCR_DSIZE_SHIFT; 
        break;
    }
    
    dmareg->SAR = _source;
    dmareg->DAR = _destination;
    *chcfg = _trigger;
    dmareg->DCR = config;      
    dmareg->DSR_BCR = length;
    
    // Start - set the ENBL bit in the DMAMUX channel config register
    *chcfg |= DMAMUX_CHCFG_ENBL_MASK;

    return 0;
}

void SimpleDMA::link(SimpleDMA &dest, bool all)
{
    linkChannel1 = dest._channel;
    linkChannel2 = 0;
    linkMode = all ? 3 : 2;
}

void SimpleDMA::link(SimpleDMA &dest1, SimpleDMA &dest2)
{
    linkChannel1 = dest1._channel;
    linkChannel2 = dest2._channel;
    linkMode = 1;
}


bool SimpleDMA::isBusy( int channel ) 
{
    if (channel == -1)
        channel = _channel;

    // The busy bit doesn't seem to work as expected.  Just check if 
    // counter is at zero - if not, treat it as busy.
    //return (DMA0->DMA[_channel].DSR_BCR & (1<<25) == 1<<25);
    return (DMA0->DMA[channel].DSR_BCR & 0xFFFFF);
}

/*****************************************************************/
uint32_t SimpleDMA::remaining(int channel) 
{
    // note that the BCR register always reads with binary 1110
    // (if the configuration is correct) or 1111 (if there's an
    // error in the configuration) in bits 23-20, so we need
    // to mask these out - only keep bits 19-0 (low-order 20 
    // bits = 0xFFFFF)
    return (DMA0->DMA[channel < 0 ? _channel : channel].DSR_BCR & 0xFFFFF);
}

/*****************************************************************/
void SimpleDMA::irq_handler(void) 
{
    DMAMUX0->CHCFG[_channel] = 0;
    DMA0->DMA[_channel].DSR_BCR |= DMA_DSR_BCR_DONE_MASK;
    _callback.call();
}

void SimpleDMA::irq_handler0( void ) 
{
    if (irq_owner[0] != NULL)
        irq_owner[0]->irq_handler();
}

void SimpleDMA::irq_handler1( void ) 
{
    if (irq_owner[1] != NULL)
        irq_owner[1]->irq_handler();
}

void SimpleDMA::irq_handler2( void ) 
{
    if (irq_owner[2] != NULL)
        irq_owner[2]->irq_handler();
}

void SimpleDMA::irq_handler3( void ) 
{
    if (irq_owner[3] != NULL)
        irq_owner[3]->irq_handler();
}
#endif
