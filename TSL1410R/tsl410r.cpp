#include "mbed.h"
#include "tsl1410r.h"

TSL1410R::TSL1410R(PinName siPort, PinName clockPort, PinName aoPort)
    : si(siPort), clock(clockPort), ao(aoPort)
{
    // clear out power-on noise by clocking through all pixels twice
    clear();
    clear();
}

void TSL1410R::clear()
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

void TSL1410R::read(uint16_t *pix, int n)
{
    // start the next integration cycle by pulsing SI and one clock
    si = 1;
    clock = 1;
    clock = 0;
    si = 0;
        
    // figure how many pixels to skip on each read
    int skip = nPix/n - 1;

    // read the pixels
    for (int src = 0, dst = 0 ; src < nPix ; ++src)
    {
        // read this pixel
        pix[dst++] = ao.read_u16();
        
        // clock in the next pixel
        clock = 1;
        clock = 0;
        
        // clock skipped pixels
        for (int i = 0 ; i < skip ; ++i, ++src) {
            clock = 1;
            clock = 0;
        }
    }
    
    // clock out one extra pixel to leave A1 in the high-Z state
    clock = 1;
    clock = 0;
}
