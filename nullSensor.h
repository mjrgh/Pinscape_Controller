// Null plunger sensor
//
// This file defines a class that provides the plunger sensor interface
// that the main program expects, but with no physical sensor underneath.

const int npix = JOYMAX;

class PlungerSensor
{
public:
    PlungerSensor() { }
    
    void init() { }
    int lowResScan() { return 0; }
    bool highResScan(int &pos) { return false; }
    void sendExposureReport(USBJoystick &) { }
};

