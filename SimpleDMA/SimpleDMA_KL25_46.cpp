#if defined TARGET_KL25Z || defined TARGET_KL46Z
#include "SimpleDMA.h"



SimpleDMA *SimpleDMA::irq_owner[4] = {NULL};

SimpleDMA::SimpleDMA(int channel) {
    this->channel(channel);
       
    //Enable DMA
    SIM->SCGC6 |= 1<<1;     //Enable clock to DMA mux
    SIM->SCGC7 |= 1<<8;     //Enable clock to DMA
    
    trigger(Trigger_ALWAYS);
   
    NVIC_SetVector(DMA0_IRQn, (uint32_t)&irq_handler0);
    NVIC_SetVector(DMA1_IRQn, (uint32_t)&irq_handler1);
    NVIC_SetVector(DMA2_IRQn, (uint32_t)&irq_handler2);
    NVIC_SetVector(DMA3_IRQn, (uint32_t)&irq_handler3);
    NVIC_EnableIRQ(DMA0_IRQn);
    NVIC_EnableIRQ(DMA1_IRQn);
    NVIC_EnableIRQ(DMA2_IRQn);
    NVIC_EnableIRQ(DMA3_IRQn);
    
    // presume cycle-steal mode
    cycle_steal = true;
}

// Figure the circular buffer MOD bit pattern (SMOD or DMOD)
// for a given circular buffer size.  The buffer size is
// required to be a power of 2 from 16 to 256K.
static uint32_t modsize_to_modbits(uint32_t siz)
{
    // The bit pattern is such that 2^(bits+3), so we basically
    // need to take the base-2 log of the size.  Zero has the
    // special meaning that the circular buffer is disabled.
    uint32_t bits = 0;
    for (bits = 0 ; siz >= 16 ; siz >>= 1, ++bits) ;
    
    // return the result, which can't be over binary 1111
    return bits & 0xF;
}

int SimpleDMA::start(uint32_t length) {  
    if (auto_channel)
        _channel = getFreeChannel();
    else
        while(isBusy());
    
    if (length > DMA_DSR_BCR_BCR_MASK)
        return -1;

    irq_owner[_channel] = this;
    
    DMA0->DMA[_channel].SAR = _source;
    DMA0->DMA[_channel].DAR = _destination;
    DMA0->DMA[_channel].DSR_BCR = length;
    DMAMUX0->CHCFG[_channel] = _trigger;
    
    uint32_t config = 
        DMA_DCR_EINT_MASK 
        | DMA_DCR_ERQ_MASK 
        | (cycle_steal ? DMA_DCR_CS_MASK : 0)
        | (source_inc << DMA_DCR_SINC_SHIFT) 
        | (modsize_to_modbits(source_mod) << DMA_DCR_SMOD_SHIFT)
        | (destination_inc << DMA_DCR_DINC_SHIFT)
        | (modsize_to_modbits(destination_mod) << DMA_DCR_DMOD_SHIFT);
    switch (source_size) {
        case 8:
            config |= 1 << DMA_DCR_SSIZE_SHIFT;
            break;
        case 16:
            config |= 2 << DMA_DCR_SSIZE_SHIFT; 
            break;
    }
    switch (destination_size) {
        case 8:
            config |= 1 << DMA_DCR_DSIZE_SHIFT;
            break;
        case 16:
            config |= 2 << DMA_DCR_DSIZE_SHIFT; 
            break;
    }
    
    DMA0->DMA[_channel].DCR = config;      
           
    //Start
    DMAMUX0->CHCFG[_channel] |= 1<<7;
    
    return 0;
}

int SimpleDMA::restart(uint32_t length)
{
    DMA0->DMA[_channel].SAR = _source;
    DMA0->DMA[_channel].DAR = _destination;
    DMA0->DMA[_channel].DSR_BCR = length;
    DMAMUX0->CHCFG[_channel] |= 1<<7;
    return 0;
}

bool SimpleDMA::isBusy( int channel ) {
    //Busy bit doesn't work as I expect it to do, so just check if counter is at zero
    //return (DMA0->DMA[_channel].DSR_BCR & (1<<25) == 1<<25);
    if (channel == -1)
        channel = _channel;
    
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
void SimpleDMA::irq_handler(void) {
    DMAMUX0->CHCFG[_channel] = 0;
    DMA0->DMA[_channel].DSR_BCR |= DMA_DSR_BCR_DONE_MASK ; 
    _callback.call();
}

void SimpleDMA::irq_handler0( void ) {
    if (irq_owner[0]!=NULL)
        irq_owner[0]->irq_handler();
}

void SimpleDMA::irq_handler1( void ) {
    if (irq_owner[1]!=NULL)
        irq_owner[1]->irq_handler();
}

void SimpleDMA::irq_handler2( void ) {
    if (irq_owner[2]!=NULL)
        irq_owner[2]->irq_handler();
}

void SimpleDMA::irq_handler3( void ) {
    if (irq_owner[3]!=NULL)
        irq_owner[3]->irq_handler();
}
#endif