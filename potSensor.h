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
    }
    
    virtual void init() 
    {
    }
    
    virtual bool highResScan(float &pos)
    {
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
        // each, so 5 readings is about 150us.  This is plenty fast enough
        // for even a low-res scan.
        pos = (pot.read() + pot.read() + pot.read() + pot.read() + pot.read())/5.0;
        return true;
    }
    
    virtual bool lowResScan(float &pos)
    {
        // Since we have only one analog input to sample, our read time is
        // very fast compared to the image sensor alternatives, so there's no
        // need to do anything different for a faster low-res scan.  Simply
        // take a normal high-res reading.
        return highResScan(pos);
    }
        
private:
    AnalogIn pot;
};
