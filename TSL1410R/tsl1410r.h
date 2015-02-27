/*
 *  TSL1410R interface class.
 *
 *  This provides a high-level interface for the Taos TSL1410R linear CCD array sensor.
 */
 
 #include "mbed.h"
 #include "FastIO.h"
 #include "FastAnalogIn.h"
 
 #ifndef TSL1410R_H
 #define TSL1410R_H
 
template <PinName siPin, PinName clockPin> class TSL1410R
{
public:
    // set up the analog in port for reading the currently selected 
    // pixel value
    TSL1410R(PinName aoPin) : ao(aoPin)
    {
        // disable continuous conversion mode in FastAnalogIn - since we're
        // reading discrete pixel values, we want to control when the samples
        // are taken rather than continuously averaging over time
        ao.disable();

        // clear out power-on noise by clocking through all pixels twice
        clear();
        clear();
    }

    // Read the pixels.
    //
    // 'n' specifies the number of pixels to sample, and is the size of
    // the output array 'pix'.  This can be less than the full number
    // of pixels on the physical device; if it is, we'll spread the
    // sample evenly across the full length of the device by skipping
    // one or more pixels between each sampled pixel to pad out the
    // difference between the sample size and the physical CCD size.
    // For example, if the physical sensor has 1280 pixels, and 'n' is
    // 640, we'll read every other pixel and skip every other pixel.
    // If 'n' is 160, we'll read every 8th pixel and skip 7 between
    // each sample.
    // 
    // The reason that we provide this subset mode (where 'n' is less
    // than the physical pixel count) is that reading a pixel is the most
    // time-consuming part of the scan.  For each pixel we read, we have
    // to wait for the pixel's charge to transfer from its internal smapling
    // capacitor to the CCD's output pin, for that charge to transfer to
    // the KL25Z input pin, and for the KL25Z ADC to get a stable reading.
    // This all takes on the order of 20us per pixel.  Skipping a pixel
    // only requires a clock pulse, which takes about 350ns.  So we can
    // skip 60 pixels in the time it takes to sample 1 pixel.
    //
    // We clock an SI pulse at the beginning of the read.  This starts the
    // next integration cycle: the pixel array will reset on the SI, and 
    // the integration starts 18 clocks later.  So by the time this method
    // returns, the next sample will have been integrating for npix-18 clocks.  
    // That's usually enough time to allow immediately reading the next
    // sample.  If more integration time is required, the caller can simply
    // sleep/spin for the desired additional time, or can do other work that
    // takes the desired additional time.
    //
    // If the caller has other work to tend to that takes longer than the
    // desired maximum integration time, it can call clear() to clock out
    // the current pixels and start a fresh integration cycle.
    void read(uint16_t *pix, int n) { read(pix, n, 0, 0, 0); }
    
    // Read with interval callback.  We'll call the callback the given
    // number of times per read cycle.
    void read(uint16_t *pix, int n, void (*cb)(void *ctx), void *cbctx, int cbcnt)
    {
        // start the next integration cycle by pulsing SI and one clock
        si = 1;
        clock = 1;
        si = 0;
        clock = 0;
        
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
                // clock in and read the next pixel
                clock = 1;
                ao.enable();
                wait_us(1);
                clock = 0;
                wait_us(11);
                pix[dst++] = ao.read_u16();
                ao.disable();
                
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

    // Clock through all pixels to clear the array.  Pulses SI at the
    // beginning of the operation, which starts a new integration cycle.
    // The caller can thus immediately call read() to read the pixels 
    // integrated while the clear() was taking place.
    void clear()
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

    // number of pixels in the array
    static const int nPix = 1280;
    
    
private:
    FastOut<siPin> si;
    FastOut<clockPin> clock;
    FastAnalogIn ao;
};
 
#endif /* TSL1410R_H */
