/*
 *  TSL1410R interface class.
 *
 *  This provides a high-level interface for the Taos TSL1410R linear CCD array sensor.
 */
 
 #include "mbed.h"
 
 #ifndef TSL1410R_H
 #define TSL1410R_H
 
class TSL1410R
{
public:
    // set up with the two DigitalOut ports (SI and clock), and the
    // analog in port for reading the currently selected pixel value
    TSL1410R(PinName siPort, PinName clockPort, PinName aoPort);

    // Read the pixels.  Fills in pix[] with the pixel values, scaled 0-0xffff.
    // n is the number of pixels to read; if this is less than the physical
    // array size (npix), we'll read every mth pixel, where m = npix/n.  E.g.,
    // if you want 640 pixels out of 1280 on the sensor, we'll read every
    // other pixel.  If you want 320, we'll read every fourth pixel.
    //
    // We clock an SI pulse at the beginning of the read.  This starts the
    // next integration cycle: the pixel array will reset on the SI, and 
    // the integration starts 18 clocks later.  So by the time this returns,
    // the next sample will have been integrating for npix-18 clocks.  In
    // many cases this is enough time to allow immediately reading the next
    // sample; if more integration time is required, the caller can simply
    // sleep/spin for the desired additional time, or can do other work that
    // takes the desired additional time.  
    //
    // If the caller has other work to tend to that takes longer than the
    // desired maximum integration time, it can call clear() to clock out
    // the current pixels and start a fresh integration cycle.
    void read(uint16_t *pix, int n);

    // Clock through all pixels to clear the array.  Pulses SI at the
    // beginning of the operation, which starts a new integration cycle.
    // The caller can thus immediately call read() to read the pixels 
    // integrated while the clear() was taking place.
    void clear();

    // number of pixels in the array
    static const int nPix = 1280;
    
    
private:
    DigitalOut si;
    DigitalOut clock;
    AnalogIn ao;
};
 
#endif /* TSL1410R_H */
