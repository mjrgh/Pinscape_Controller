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
// Although this class is nominally for potentiometers, it will also
// work with any other type of sensor that provides a single analog 
// voltage level that maps linearly to the position, such as an LVDT.


class PlungerSensorPot: public PlungerSensor
{
public:
    PlungerSensorPot(PinName ao) : pot(ao)
    {
        // start our sample timer with an arbitrary zero point of now
        timer.start();
    }
    
    virtual void init() 
    {
    }
    
    // read the sensor
    virtual bool read(PlungerReading &r)
    {
        // get the starting time of the sampling
        uint32_t t0 = timer.read_us();
        
        // Take a few readings and use the average, to reduce the effect
        // of analog voltage fluctuations.  The voltage range on the ADC
        // is 0-3.3V, and empirically it looks like we can expect random
        // voltage fluctuations of up to 50 mV, which is about 1.5% of
        // the overall range.  We try to quantize at about the mm level
        // (in terms of the plunger motion range), which is about 1%.
        // So 1.5% noise is big enough to be visible in the joystick
        // reports.  Averaging several readings should help smooth out
        // random noise in the readings.
        //
        // Readings through the standard AnalogIn class take about 30us
        // each, so taking 5 readings takes about 150us.  This is fast
        // enough to resolve even the fastest plunger motiono with no
        // aliasing.
        r.pos = uint16_t((
            uint32_t(pot.read_u16())
            + uint32_t(pot.read_u16()) 
            + uint32_t(pot.read_u16())
            + uint32_t(pot.read_u16())
            + uint32_t(pot.read_u16())
            ) / 5U);
            
        // Get the elapsed time of the sample, and figure the indicated
        // sample time as the midpoint between the start and end times.
        // (Note that the timer might overflow the uint32_t between t0 
        // and now, in which case it will appear that now < t0.  The
        // calculation will always work out right anyway, because it's 
        // effectively performed mod 2^32-1.)
        uint32_t dt = timer.read_us() - t0;
        r.t = t0 + dt/2;
        
        // add the current sample to our timing statistics
        totScanTime += dt;
        nScans += 1;
            
        // success
        return true;
    }
    
    // figure the average scan time in microseconds
    virtual uint32_t getAvgScanTime() { return uint32_t(totScanTime/nScans); }
        
private:
    // analog input for the pot wiper
    AnalogIn pot;
    
    // timer for input timestamps
    Timer timer;
    
    // total sensor scan time in microseconds, and number of scans completed
    long long totScanTime;
    int nScans;
};
