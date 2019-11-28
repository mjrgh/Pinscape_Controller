// Potentiometer plunger sensor
//
// This file implements our generic plunger sensor interface for a
// potentiometer.  The potentiometer resistance must be linear in 
// position.  To connect physically, wire the fixed ends of the
// potentiometer to +3.3V and GND (respectively), and connect the 
// wiper to an ADC-capable GPIO pin on the KL25Z.  The wiper voltage 
// that we read on the ADC will vary linearly with the wiper position.
// Mechanically attach the wiper to the plunger so that the wiper moves
// in lock step with the plunger.
//
// In practice, the ADC readings from a potentiometer can be noisy,
// varying by around 1% from reading to reading when the slider is
// stationary.  One way to improve this is to use longer sampling times
// in the ADC to improve the accuracy of the sampling.  We can tolerate
// quite long ADC sampling times because even the slow modes are quite
// a lot faster than the result rate we require.  Another way to reduce
// noise is to apply some low-pass filtering.  The simplest low-pass 
// filter is to average a number of samples together.  Since our ADC
// sampling rate (even with long conversions) is quite a lot faster than
// the needed output rate, we can simply average samples over the time
// scale where we need discrete outputs.
//
// Note: even though this class is specifically for potentiometers, it
// could also be used with any other type of sensor that represents its
// position reading as a single analog voltage level that varies linearly
// with the position, such as an LVDT.  Linearity is key; for example,
// this class isn't suitable for the old Sharp reflected IR distance 
// sensors, as those have non-linear voltage responses, and the software
// would have to compensate for that to yield good results.
// 

#include "plunger.h"
#include "AltAnalogIn.h"

class PlungerSensorPot: public PlungerSensor
{
public:
    // Our native readings are taken as 16-bit ADC samples, so
    // our native scale is an unsigned 16-bit int, 0..65535.
    //
    // Initialize the ADC to take continuous samples, interrupting us
    // when each conversion finishes so that we can collect the result
    // in an ISR.  For the sampling mode, use long conversions with
    // 24 ADCK cycles and 8x averaging; this gives us conversion times
    // of about 37.33us.
    //
    PlungerSensorPot(PinName ao) : 
        PlungerSensor(65535), 
        pot(ao, true, 24, 8)  // continuous, 24-cycle long samples, 8x averaging -> 37.33us/sample
    {
        // calibrate the ADC for best accuracy
        pot.calibrate();
        
        // clear the timing statistics
        totalConversionTime = 0;
        nSamples = 0;

        // start with everything zeroed
        history_write_idx = 0;
        running_sum = 0;
        for (int i = 0 ; i < countof(history); ++i)
            history[i] = 0;
            
        // set the initial timestamp to the arbitrary epoch on the timer
        current_timestamp = 0;
        
        // Set up an interrupt handler to collect the ADC results.  The
        // ADC will trigger the interrupt on each completed sample.
        isrThis = this;
        NVIC_SetVector(ADC0_IRQn, (uint32_t)&irq_handler_static);
        NVIC_EnableIRQ(ADC0_IRQn);
        pot.enableInterrupts();
        
        // Start the first asynchronous ADC sample.  The ADC will run
        // continuously once started, and we'll collect samples in the ISR.
        pot.start();
        timer.start();
    }
    
    virtual void init() 
    {
    }
    
    // samples are always ready
    virtual bool ready() { return true; }
    
    // read the sensor
    virtual bool readRaw(PlungerReading &r)
    {
        // read the current sample components atomically
        __disable_irq();
        
        // figure the current average reading over the history window
        r.pos = running_sum / countof(history);
        r.t = current_timestamp;
            
        // done with the atomic read
        __enable_irq();
        
        // we always have a result available
        return true;
    }
    
    // Figure the average scan time in microseconds
    virtual uint32_t getAvgScanTime() 
    { 
        // The effective time per sample is the raw sampling interval
        // times the averaging window size.
        if (nSamples == 0) 
            return 0;
        else
            return static_cast<uint32_t>(totalConversionTime/nSamples) * countof(history);
    }
        
private:
    // analog input for the pot wiper
    AltAnalogIn_16bit pot;
    
    // timer for input timestamps
    Timer timer;
    
    // total sampling time and number of samples, for computing scan times
    uint64_t totalConversionTime;
    uint32_t nSamples;

    // interrupt handler
    static PlungerSensorPot *isrThis;
    static void irq_handler_static(void) { isrThis->irq_handler(); }

    void irq_handler()
    {
        // read the next sample
        uint16_t sample = pot.read_u16();
        
        // deduct the outgoing sample from the running sum
        running_sum -= history[history_write_idx];
        
        // add the new sample into the running sum
        running_sum += sample;
        
        // store the new sample in the history
        history[history_write_idx++] = sample;
        
        // wrap the history index at the end of the window
        if (history_write_idx >= countof(history))
            history_write_idx = 0;
            
        // calculate the elapsed time since the last sample
        uint32_t now = timer.read_us();
        totalConversionTime += now - current_timestamp;
        ++nSamples;
        
        // update the reading timestamp
        current_timestamp = now;
    }
    
    // Running sum of readings.  This is the sum of the readings in the
    // rolling 5ms window.
    uint32_t running_sum;
    
    // Rolling window of readings, for the averaging filter.  Our 
    // sampling time is about 37.33us; 128 of these add up to about
    // 4.8ms, which is a good interval between samples for our
    // internal tracking and sending USB data to the PC.
    uint16_t history[128];
    int history_write_idx;
    
    // current average reading and scan time
    uint32_t current_timestamp;
};
