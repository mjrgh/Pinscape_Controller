#include "mbed.h"
#include "pinscape.h"
#include "potSensor.h"

// 'this' object for IRQ callback
PlungerSensorPot *PlungerSensorPot::isrThis;
