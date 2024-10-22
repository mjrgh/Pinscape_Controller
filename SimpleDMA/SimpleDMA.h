#ifndef SIMPLEDMA_H
#define SIMPLEDMA_H

#ifdef RTOS_H
#include "rtos.h"
#endif

#include "mbed.h"
#include "SimpleDMA_KL25.h"
#include "SimpleDMA_KL46.h"
#include "SimpleDMA_LPC1768.h"


/**
* SimpleDMA, DMA made simple! (Okay that was bad)
*
* A class to easily make basic DMA operations happen. Not all features
* of the DMA peripherals are used, but the main ones are: From and to memory
* and peripherals, either continiously or triggered
*/
class SimpleDMA {
public:
/**
* Constructor
*
* @param channel - optional parameter which channel should be used, default is automatic channel selection
*/
SimpleDMA(int channel = -1);

/**
* Set the source of the DMA transfer
*
* Autoincrement increments the pointer after each transfer. If the source
* is an array this should be true, if it is a peripheral or a single memory
* location it should be false.
*
* The source can be any pointer to any memory location. Automatically
* the wordsize is calculated depending on the type, if required you can
* also override this.
*
* @param pointer - pointer to the memory location
* @param autoinc - should the pointer be incremented by the DMA module
* @param size - wordsize in bits (optional, generally can be omitted)
* @return - 0 on success
*/
template<typename Type>
void source(Type* pointer, bool autoinc, int size = sizeof(Type) * 8) {
    _source = (uint32_t)pointer;
    source_inc = autoinc;
    source_size = size;
}

/**
* Set the destination of the DMA transfer
*
* Autoincrement increments the pointer after each transfer. If the source
* is an array this should be true, if it is a peripheral or a single memory
* location it should be false.
*
* The destination can be any pointer to any memory location. Automatically
* the wordsize is calculated depending on the type, if required you can
* also override this.
*
* @param pointer - pointer to the memory location
* @param autoinc - should the pointer be incremented by the DMA module
* @param size - wordsize in bits (optional, generally can be omitted)
* @return - 0 on success
*/
template<typename Type>
void destination(Type* pointer, bool autoinc, int size = sizeof(Type) * 8) {
    _destination = (uint32_t)pointer;
    destination_inc = autoinc;
    destination_size = size;
}


/**
* Set the trigger for the DMA operation
*
* In SimpleDMA_[yourdevice].h you can find the names of the different triggers.
* Trigger_ALWAYS is defined for all devices, it will simply move the data
* as fast as possible. Used for memory-memory transfers. If nothing else is set
* that will be used by default.
*
* @param trig - trigger to use
* @param return - 0 on success
*/
void trigger(SimpleDMA_Trigger trig) {
    _trigger = trig;
}

/**
* Set the DMA channel
*
* Generally you will not need to call this function, the constructor does so for you
*
* @param chan - DMA channel to use, -1 = variable channel (highest priority channel which is available)
*/
void channel(int chan);
int getChannel() { return _channel; }

/**
* Start the transfer
*
* @param length - number of BYTES to be moved by the DMA
*/
int start(uint32_t length, bool wait);

/**
* Prepare a transfer.  This sets everything up for a transfer, but leaves it up
* to the caller to trigger the start of the transfer.  This gives the caller
* precise control over the timing of the transfer, for transfers that must be
* synchronized with other functions.  To start the DMA transfer, the caller
* must simply "OR" DMAMUX_CHCFG_ENBL_MASK into the byte at the returned 
* address.
*/
volatile uint8_t *prepare(uint32_t length, bool wait);

/**
* Is the DMA channel busy
*
* @param channel - channel to check, -1 = current channel
* @return - true if it is busy
*/
bool isBusy( int channel = -1 );

/**
* Number of bytes remaining in running transfer.  This reads the controller
* register with the remaining byte count, which the hardware updates each
* time it completes a destination transfer.
*/
uint32_t remaining(int channel = -1);

/**
* Attach an interrupt upon completion of DMA transfer or error
*
* @param function - function to call upon completion (may be a member function)
*/
void attach(void (*function)(void)) {
    _callback.attach(function);
    }
    
template<typename T>
    void attach(T *object, void (T::*member)(void)) {
        _callback.attach(object, member);
    }
    
/**
* Link to another channel.  This triggers the given destination
* channel when a transfer on this channel is completed.  If 'all' 
* is true, the link occurs after the entire transfer is complete 
* (i.e., the byte count register in this channel reaches zero).
* Otherwise, the link is triggered once for each transfer on this
* channel.
*/
void link(SimpleDMA &dest, bool all = false);

/**
* Link to two other channels.  This triggers the 'dest1' channel
* once for each transfer on this channel, and then triggers the
* 'dest2' channel once when the entire transfer has been completed
* (i.e., the byte count register on this channel reaches zero).
*/
void link(SimpleDMA &dest1, SimpleDMA &dest2);


#ifdef RTOS_H
/**
* Start a DMA transfer similar to start, however block current Thread
* until the transfer is finished
*
* When using this function only the current Thread is halted.
* The Thread is moved to Waiting state: other Threads will continue
* to run normally. 
*
* This function is only available if you included rtos.h before 
* including SimpleDMA.h.
*
* @param length - number of BYTES to be moved by the DMA
*/
void wait(int length) {
    id = Thread::gettid();
    this->attach(this, &SimpleDMA::waitCallback);
    this->start(length);
    Thread::signal_wait(0x1);
}
#endif

protected:
uint8_t _channel;
SimpleDMA_Trigger _trigger;
uint32_t _source;
uint32_t _destination;
uint8_t source_size;
uint8_t destination_size;
uint8_t linkChannel1;
uint8_t linkChannel2;
bool source_inc : 1;
bool destination_inc : 1;
bool auto_channel : 1;
uint8_t linkMode : 2;


//IRQ handlers
FunctionPointer _callback;
void irq_handler(void);

static SimpleDMA *irq_owner[DMA_CHANNELS];

static void class_init();
static void irq_handler0( void ); 

#if DMA_IRQS > 1
static void irq_handler1( void );
static void irq_handler2( void );
static void irq_handler3( void );
#endif

//Keep searching until we find a non-busy channel, start with lowest channel number
int getFreeChannel(void);

#ifdef RTOS_H
osThreadId id;
void waitCallback(void) {
    osSignalSet(id, 0x1);    
}
#endif
};
#endif
