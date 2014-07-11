#include "mbed.h"
#include "tls1410r.h"

TLS1410R::TLS1410R(PinName siPort, PinName clockPort, PinName aoPort)
    : si(siPort), clock(clockPort), ao(aoPort)
{
    // clear out power-on noise by clocking through all pixels twice
    clear();
    clear();
}

void TLS1410R::clear()
{
    // clock in an SI pulse
    si = 1;
    clock = 1;
    clock = 0;
    si = 0;
    
    // clock out all pixels
    for (int i = 0 ; i < nPix+1 ; ++i) {
        clock = 1;
        clock = 0;
    }
}

void TLS1410R::read(uint16_t *pix, int n, int integrate_us)
{
    // Start an integration cycle - pulse SI, then clock all pixels.  The
    // CCD will integrate light starting 18 clocks after the SI pulse, and
    // continues integrating until the next SI pulse, which cannot occur
    // until all pixels have been clocked.
    si = 1;
    clock = 1;
    clock = 0;
    si = 0;
    for (int i = 0 ; i < nPix+1 ; ++i) {
        clock = 1;
        clock = 0;
    }
        
    // delay by the specified additional integration time
    wait_us(integrate_us);
    
    // end the current integration cycle and hold the integrated values
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
        pix[dst++] = ao;
        
        // clock in the next pixel
        clock = 1;
        clock = 0;
        
        // clock skipped pixels
        for (int i = 0 ; i < skip ; ++i) {
            clock = 1;
            clock = 0;
        }
    }
    
    // clock out one extra pixel to make sure the device is ready for another go
    clock = 1;
    clock = 0;
}
