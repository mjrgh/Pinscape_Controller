// Toshiba TCD1103 linear image sensors
//
// This sensor is similar to the original TSL1410R in both its electronic
// interface and the theory of operation.  The details of the electronics
// are different enough that we can't reuse the same code at the hardware
// interface level, but the principle of operation is similar: the sensor
// provides a serial interface to a file of pixels transferred as analog
// voltage levels representing the charge collected.  
//
// As with the TSL1410R, we position the sensor so that the pixel row is
// aligned with the plunger axis, with a backlight, and we detect the plunger 
// position by looking for an edge between a light area (where the backlight
// is unobstructed) and a dark area (where the plunger rod is blocking the
// backlight).  The optical sensor area of the TSL1410R is large enough to
// cover the entire plunger travel distance, so the physical setup for that
// sensor is a simple matter of placing the sensor near the plunger, so that
// the plunger casts a shadow on the sensor.  The TCD1103, in contrast, has a 
// small optical sensor area, about 8mm long, so in this case we have to use
// a lens to reduce the image of the plunger by about 10X (from the 80mm
// plunger travel distance to the 8mm sensor size).  This makes the physical
// setup more complex, but it has the advantage of giving us a focused image,
// allowing for better precision in detecting the edge.  With the unfocused
// image used in the TSL1410R setup, the shadow was blurry over about 1/50".
// With a lens to focus the image, we could potentially get as good as 
// single-pixel resolution, which would give us about 1/500" resolution on 
// this 1500-pixel sensor.
//

#include "edgeSensor.h"
#include "TCD1103.h"

template <bool invertedLogicGates>
class PlungerSensorImageInterfaceTCD1103: public PlungerSensorImageInterface
{
public:
    // Note that the TCD1103 has 1500 actual image pixels, but the serial
    // interface provides 32 dummy elements on the front end (before the
    // first image pixel) and 14 dummy elements on the back end (after the
    // last image pixel), for a total of 1546 serial outputs.
    PlungerSensorImageInterfaceTCD1103(PinName fm, PinName os, PinName icg, PinName sh)
        : PlungerSensorImageInterface(1546), sensor(fm, os, icg, sh)
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
    virtual void readPix(uint8_t* &pix, uint32_t &t, int axcTime)
    {        
        // start reading the next pixel array (this waits for any DMA
        // transfer in progress to finish, ensuring a stable pixel buffer)
        sensor.startCapture(axcTime);

        // get the image array from the last capture
        sensor.getPix(pix, t);        
    }

    virtual void getStatusReportPixels(
        uint8_t* &pix, uint32_t &t, int axcTime, int extraTime)
    {
        // The sensor's internal buffering scheme makes it a little tricky
        // to get the requested timing, and our own double-buffering adds a
        // little complexity as well.  To get the exact timing requested, we
        // have to work with the buffering pipeline like so:
        //
        // 1. Call startCapture().  This waits for any in-progress pixel
        // transfer from the sensor to finish, then executes an SH/ICG pulse
        // on the sensor.  The pulse takes a snapshot of the current live
        // photo receptors to the sensor shift register.  These pixels have
        // been integrating starting from before we were called; call this
        // integration period A.  So the shift register contains period A.
        // The SH/ICG then grounds the photo receptors, clearing their
        // charge, thus starting a new integration period B.  After sending
        // the SH/ICG pulse, startCapture() begins a DMA transfer of the
        // shift register pixels (period A) to one of our two buffers (call
        // it the EVEN buffer).
        //
        // 2. Wait for the current transfer (period A to the EVEN buffer)
        // to finish.  The minimum integration time is the time of one
        // transfer cycle, so this brings us to the minimum time for 
        // period B.
        //
        // 3. Now pause for the requested extra delay time.  Period B is 
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
    TCD1103<invertedLogicGates> sensor;
};

template<bool invertedLogicGates>
class PlungerSensorTCD1103: public PlungerSensorEdgePos
{
public:
    // Note that the TCD1103 has 1500 actual image pixels, but the serial
    // interface provides 32 dummy elements on the front end (before the
    // first image pixel) and 14 dummy elements on the back end (after the
    // last image pixel), for a total of 1546 serial outputs.
    PlungerSensorTCD1103(PinName fm, PinName os, PinName icg, PinName sh)
        : PlungerSensorEdgePos(sensor, 1546), sensor(fm, os, icg, sh)
    {
    }
    
protected:
    PlungerSensorImageInterfaceTCD1103<invertedLogicGates> sensor;
};
