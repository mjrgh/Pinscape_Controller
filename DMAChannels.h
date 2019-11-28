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
// Plunger Sensors
//
// Plunger sensors are mutually exclusive since there's only one
// in any given system, so any channels assigned for one type of 
// sensor can be reused by other sensor types.

// TSL1410R linear optical array plunger sensor
const int DMAch_TSL_CLKUP   = 1;    // Clock Up signal generator
const int DMAch_TSL_ADC     = 2;    // ADC (analog input) sample transfer
const int DMAch_TSL_CLKDN   = 3;    // Clock Down signal generator

// TDC1103 linear CCD plunger sensor
const int DMAch_TDC_ADC     = 2;  // ADC (analog input) sample transfer

// --------------------------------------------------------------
//
// Free channels - not currently assigned.
//
const int DMAch_Unused0 = 0;


#endif
