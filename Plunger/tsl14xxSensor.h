// Base class for TSL14xx-based plunger sensors.
//
// This provides a common base class for plunger sensors based on
// AMS/TAOS TSL14xx sensors (TSL1410R, TSL1412S, TSL1401CL).  The sensors
// in this series all work the same way, differing mostly in the number
// of pixels.  However, we have two fundamentally different ways of using
// these image sensors to detect position: sensing the position of the
// shadow cast by the plunger on the sensor, and optically reading a bar
// code telling us the location of the sensor along a scale.  This class
// provides the low-level pixel-sensor interface; subclasses provide the
// image analysis that figures the position from the captured image.


#ifndef _TSL14XXSENSOR_H_
#define _TSL14XXSENSOR_H_

#include "plunger.h"
#include "edgeSensor.h"
#include "barCodeSensor.h"
#include "TSL14xx.h"

class PlungerSensorImageInterfaceTSL14xx: public PlungerSensorImageInterface
{
public:
    PlungerSensorImageInterfaceTSL14xx(int nativePix, PinName si, PinName clock, PinName ao)
        : PlungerSensorImageInterface(nativePix), sensor(nativePix, si, clock, ao)
    {
    }
        
    // is the sensor ready?
    virtual bool ready() { return sensor.ready(); }
    
    // is a DMA transfer in progress?
    virtual bool dmaBusy() { return sensor.dmaBusy(); }
    
    virtual void init()
    {
        sensor.clear();
    }
    
    // get the average sensor scan time
    virtual uint32_t getAvgScanTime() { return sensor.getAvgScanTime(); }
    
protected:
    virtual void getStatusReportPixels(
        uint8_t* &pix, uint32_t &t, int axcTime, int extraTime)
    {
        // The sensor's internal buffering scheme makes it a little tricky
        // to get the requested timing, and our own double-buffering adds a
        // little complexity as well.  To get the exact timing requested, we
        // have to work with the buffering pipeline like so:
        //
        // 1. Call startCapture().  This waits for any in-progress pixel
        // transfer from the sensor to finish, then executes a HOLD/SI pulse
        // on the sensor.  The HOLD/SI takes a snapshot of the current live
        // photo receptors to the sensor shift register.  These pixels have
        // been integrating starting from before we were called; call this
        // integration period A.  So the shift register contains period A.
        // The HOLD/SI then grounds the photo receptors, clearing their
        // charge, thus starting a new integration period B.  After sending
        // the HOLD/SI pulse, startCapture() begins a DMA transfer of the
        // shift register pixels (period A) to one of our two buffers (call
        // it the EVEN buffer).
        //
        // 2. Wait for the current transfer (period A to the EVEN buffer)
        // to finish.  The minimum integration time is the time of one
        // transfer cycle, so this brings us to the minimum time for 
        // period B.
        //
        // 3. Now pause for the reqeusted extra delay time.  Period B is 
        // still running at this point (it keeps going until we start a 
        // new capture), so this pause adds the requested extra time to 
        // period B's total integration time.  This brings period B to
        // exactly the requested total time.
        //
        // 4. Call startCapture() to end period B, move period B's pixels
        // to the sensor's shift register, and begin period C.  This 
        // switches DMA buffers, so the EVEN buffer (with period A) is now 
        // available, and the ODD buffer becomes the DMA target for the 
        // period B pixels.
        //
        // 5. Wait for the period B pixels to become available, via
        // waitPix().  This waits for the DMA transfer to complete and
        // hands us the latest (ODD) transfer buffer.
        //       
        sensor.startCapture(axcTime);       // begin transfer of pixels from incoming period A, begin integration period B
        sensor.wait();                      // wait for scan of A to complete, as minimum integration B time
        wait_us(long(extraTime) * 100);     // add extraTime (0.1ms == 100us increments) to integration B time
        sensor.startCapture(axcTime);       // begin transfer of pixels from integration period B, begin period C; period A pixels now available
        
        // wait for the DMA transfer of period B to finish, and get the 
        // period B pixels
        sensor.waitPix(pix, t);
    }
    
    // reset after a status report
    virtual void resetAfterStatusReport(int axcTime)
    {
        // It takes us a while to send all of the pixels, since we have
        // to break them up into many USB reports.  This delay means that
        // the sensor has been sitting there integrating for much longer
        // than usual, so the next frame read will be overexposed.  To
        // mitigate this, make sure we don't have a capture running,
        // then clear the sensor and start a new capture.
        sensor.wait();
        sensor.clear();
        sensor.startCapture(axcTime);
    }

    // the low-level interface to the TSL14xx sensor
    TSL14xx sensor;
};

// ---------------------------------------------------------------------
//
// Subclass for the large sensors, such as TSL1410R (1280 pixels)
// and TSL1412S (1536 pixels).
//
// For the large sensors, pixel transfers take a long time: about
// 2.5ms on the 1410R and 1412S.  This is much longer than our main
// loop time, so we don't want to block other work to do a transfer.
// Instead, we want to do our transfers asynchronously, so that the
// main loop can keep going while a transfer proceeds.  This is
// possible via our DMA double buffering.  
//
// This scheme gives us three images in our pipeline at any given time:
//
//  - a live image integrating light on the photo receptors on the sensor
//  - the prior image being held in the sensor's shift register and being
//    transferred via DMA into one of our buffers
//  - the image before that in our other buffer
//
// Integration of a live image starts when we begin the transfer of the
// prior image.  Integration ends when we start the next transfer after
// that.  So the total integration time, which is to say the exposure
// time, is the time between consecutive transfer starts.  It's important
// for this time to be consistent from image to image, because that
// determines the exposure level.  We use polling from the main loop
// to initiate new transfers, so the main loop is responsible for
// polling frequently during the 2.5ms transfer period.  It would be
// more consistent if we did this in an interrupt handler instead,
// but that would complicate things considerably since our image
// analysis is too time-consuming to do in interrupt context.
//
class PlungerSensorTSL14xxLarge: public PlungerSensorImageInterfaceTSL14xx
{
public:
    PlungerSensorTSL14xxLarge(int nativePix, PinName si, PinName clock, PinName ao)
        : PlungerSensorImageInterfaceTSL14xx(nativePix, si, clock, ao)
    {
    }

    virtual void readPix(uint8_t* &pix, uint32_t &t, int axcTime)
    {        
        // start reading the next pixel array (this waits for any DMA
        // transfer in progress to finish, ensuring a stable pixel buffer)
        sensor.startCapture(axcTime);

        // get the image array from the last capture
        sensor.getPix(pix, t);
        
    }
};        

// ---------------------------------------------------------------------
//
// Subclass for the small sensors, such as TSL1401CL (128 pixels).
//
// For the small sensors, we can't use the asynchronous transfer
// scheme we use for the large sensors, because the transfer times
// are so short that the main loop can't poll frequently enough to
// maintain consistent exposure times.  With the short transfer
// times, though, we don't need to do them asynchronously.
//
// Instead, each time we want to read the sensor, we do the whole
// integration and transfer synchronously, so that we can precisly
// control the total time.  This requires two transfers.  First,
// we start a transfer in order to set the exact starting time of
// an integration period: call it period A.  We wait for the
// transfer to complete, which fills a buffer with the prior
// integration period's pixels.  We don't want those pixels,
// because they started before we got here and thus we can't
// control how long they were integrating.  So we discard that
// buffer and start a new transfer.  This starts period B while
// transferring period A's pixels into a DMA buffer.  We want
// those period A pixels, so we wait for this transfer to finish.
//
class PlungerSensorTSL14xxSmall: public PlungerSensorImageInterfaceTSL14xx
{
public:
    PlungerSensorTSL14xxSmall(int nativePix, PinName si, PinName clock, PinName ao)
        : PlungerSensorImageInterfaceTSL14xx(nativePix, si, clock, ao)
    {
    }

    // read the image
    virtual void readPix(uint8_t* &pix, uint32_t &t, int axcTime)
    {
        // Clear the sensor.  This sends a HOLD/SI pulse to the sensor,
        // which ends the current integration period, starts a new one
        // (call the new one period A) right now, and clocks out all
        // of the pixels from the old cycle.  We want to discard these
        // pixels because they've been integrating from some time in
        // the past, so we can't control the exact timing of that cycle.
        // Clearing the sensor clocks the pixels out without waiting to
        // read them on DMA, so it's much faster than a regular transfer
        // and thus gives us the shortest possible base integration time
        // for period A.
        sensor.clear();
        
        // Start a real transfer.  This ends integration period A (the
        // one we just started), starts a new integration period B, and
        // begins transferring period A's pixels to memory via DMA.  We
        // use the auto-exposure time to get the optimal exposure.
        sensor.startCapture(axcTime);
        
        // wait for the period A pixel transfer to finish, and grab
        // its pixels
        sensor.waitPix(pix, t);
    }
};        


// -------------------------------------------------------------------------
//
// Concrete TSL14xx sensor types
//


// TSL1410R sensor - edge detection sensor
class PlungerSensorTSL1410R: public PlungerSensorEdgePos
{
public:
    PlungerSensorTSL1410R(PinName si, PinName clock, PinName ao)
        : PlungerSensorEdgePos(sensor, 1280), sensor(1280, si, clock, ao)
    {
    }
    
protected:
    PlungerSensorTSL14xxLarge sensor;
};

// TSL1412R - edge detection sensor
class PlungerSensorTSL1412R: public PlungerSensorEdgePos
{
public:
    PlungerSensorTSL1412R(PinName si, PinName clock, PinName ao)
        : PlungerSensorEdgePos(sensor, 1536), sensor(1536, si, clock, ao)
    {
    }
    
protected:
    PlungerSensorTSL14xxLarge sensor;
};

// TSL1401CL - bar code sensor
class PlungerSensorTSL1401CL: public PlungerSensorBarCode<7, 0, 1, 16>
{
public:
    PlungerSensorTSL1401CL(PinName si, PinName clock, PinName ao)
        : PlungerSensorBarCode(sensor, 128), sensor(128, si, clock, ao)
    {
    }
    
protected:
    PlungerSensorTSL14xxSmall sensor;
};


#endif
