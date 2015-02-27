#if 0
// this file is no longer used - the method bodies are no in the header,
// which was necessary because of the change to a template class, which
// itself was necessary because of the use of the FastIO library

#include "mbed.h"
#include "tsl1410r.h"

template <PinName siPin, PinName clockPin> TSL1410R<siPin, clockPin>::
    TSL1410R<siPin, clockPin>(PinName aoPort) : ao(aoPort)
{
    // clear out power-on noise by clocking through all pixels twice
    clear();
    clear();
}

template <PinName siPin, PinName clockPin> void TSL1410R<siPin, clockPin>::clear()
{
    // clock in an SI pulse
    si = 1;
    clock = 1;
    clock = 0;
    si = 0;
    
    // clock out all pixels
    for (int i = 0 ; i < nPix + 1 ; ++i) {
        clock = 1;
        clock = 0;
    }
}

template <PinName siPin, PinName clockPin> void TSL1410R<siPin, clockPin>::
    read(uint16_t *pix, int n, void (*cb)(void *ctx), void *cbctx, int cbcnt)
{
    // start the next integration cycle by pulsing SI and one clock
    si = 1;
    clock = 1;
    clock = 0;
    si = 0;
        
    // figure how many pixels to skip on each read
    int skip = nPix/n - 1;
    
    // figure the callback interval
    int cbInterval = nPix;
    if (cb != 0)
        cbInterval = nPix/(cbcnt+1);

    // read all of the pixels
    for (int src = 0, dst = 0 ; src < nPix ; )
    {
        // figure the end of this callback interval
        int srcEnd = src + cbInterval;
        if (srcEnd > nPix)
            srcEnd = nPix;
        
        // read one callback chunk of pixels
        for ( ; src < srcEnd ; ++src)
        {
            // read this pixel
            pix[dst++] = ao.read_u16();
        
            // clock in the next pixel
            clock = 1;
            clock = 0;
            
            // clock skipped pixels
            for (int i = 0 ; i < skip ; ++i, ++src) 
            {
                clock = 1;
                clock = 0;
            }
        }
        
        // call the callback, if we're not at the last pixel
        if (cb != 0 && src < nPix)
            (*cb)(cbctx);
    }
    
    // clock out one extra pixel to leave A1 in the high-Z state
    clock = 1;
    clock = 0;
}

#endif /* 0 */
