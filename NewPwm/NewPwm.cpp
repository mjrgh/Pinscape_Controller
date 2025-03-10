// New PWM Out implementation

#include "NewPwm.h"

// TPM Unit singletons.  We have three singletons corresponding
// to the three physical TPM units in the hardware.
NewPwmUnit NewPwmUnit::unit[3];

// System clock rate, in Hz
uint32_t NewPwmUnit::sysClock;

// Default PWM period for new channels, in seconds
float NewPwmUnit::defaultPeriod = 0.001f;

