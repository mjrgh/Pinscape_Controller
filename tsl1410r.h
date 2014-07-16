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

    // Integrate light and read the pixels.  Fills in pix[] with the pixel values,
    // scaled 0-0xffff.  n is the number of pixels to read; if this is less than
    // the total number of pixels npix, we'll read every mth pixel, where m = npix/n.
    // E.g., if you want 640 pixels out of 1280 on the sensor, we'll read every
    // other pixel.  If you want 320, we'll read every fourth pixel.
    // Before reading, we'll pause for integrate_us additional microseconds during
    // the integration phase; use 0 for no additional integration time. 
    void read(uint16_t *pix, int n, int integrate_us);

    // clock through all pixels to clear the array
    void clear();

    // number of pixels in the array
    static const int nPix = 1280;
    
    
private:
    DigitalOut si;
    DigitalOut clock;
    AnalogIn ao;
};
 
#endif /* TSL1410R_H */
