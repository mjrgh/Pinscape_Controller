// Pinscape Controller DMA channel assignments
//
// The SimpleDMA class has provisions to assign channel numbers
// automatically as transfers are performed, but auto assignment
// doesn't play well with "channel linking", which we use in the
// TSL1410R code.  Channel linking requires us to know the hardware
// channel number of the link target before any transfers start.
// The only good way to do this is to pre-assign specific numbers.
// And once we start doing this, it's easiest to assign numbers
// for everyone.
//

#ifndef DMAChannels_H
#define DMAChannels_H

// --------------------------------------------------------------
//
// PWM controllers
//
const int DMAch_TLC5940 = 0;    // TLC5940 PWM controller chips


// --------------------------------------------------------------
//
// Plunger Sensors
//
// Plunger sensors are mutually exclusive since there's only one
// in any given system, so any channels assigned for one type of 
// sensor can be reused by other sensor types.

// TSL1410R CCD plunger sensor
const int DMAch_CLKUP   = 1;    // Clock Up signal generator
const int DMAch_ADC     = 2;    // ADC (analog input) sample transfer
const int DMAch_CLKDN   = 3;    // Clock Down signal generator

#endif
