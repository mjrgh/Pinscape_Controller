#if defined TARGET_KL25Z || defined TARGET_KL46Z
#include "SimpleDMA.h"



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

    // disable the channel while we're setting it up
    DMAMUX0->CHCFG[_channel] = 0;
    
    // set the DONE flag on the channel
    DMA0->DMA[_channel].DSR_BCR = DMA_DSR_BCR_DONE_MASK;

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
    
    DMA0->DMA[_channel].SAR = _source;
    DMA0->DMA[_channel].DAR = _destination;
    DMAMUX0->CHCFG[_channel] = _trigger;
    DMA0->DMA[_channel].DCR = config;      
    DMA0->DMA[_channel].DSR_BCR = length;
    
    // Start - set the ENBL bit in the DMAMUX channel config register
    DMAMUX0->CHCFG[_channel] |= DMAMUX_CHCFG_ENBL_MASK;

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
