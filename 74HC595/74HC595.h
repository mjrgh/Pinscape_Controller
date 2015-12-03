/* Copyright 2014 M J Roberts, MIT License
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software
* and associated documentation files (the "Software"), to deal in the Software without
* restriction, including without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or
* substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
* BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
* DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef HC595_INCLUDED
#define HC595_INCLUDED

#include "mbed.h"

// 74HC595 Interface
//
// We require four GPIO pins: 
//
//    sin - serial data
//    sclk - serial clock
//    latch - the LATCH signal, which transfers the internal shift register
//            bits to the physical output pin states
//    ena - the Enable signal
//
// Note that the physical !OE (output enable) pin on the 74HC595 is active-low.
// To allow for orderly startup that guarantees that outputs won't be pulsed
// (even briefly) during power-on, we require the !OE pin to be wired with a
// pull-up resistor to Vcc, and connected to our ENA GPIO pin via an inverter.
//
// Recommended wiring: connect the GPIO pin to the base of an NPN transistor
// through a 2.2K resistor, connect the collector the !OE pin on the 74HC595, 
// and connect the emitter to ground.  This will pull !OE to ground when we
// write a digital 1 to the ENA GPIO, enabling the outputs.
//
// We use simple bit-banging through plain DigitalOut pins to send serial
// data to the chips.  This is fast enough for our purposes, since we send
// only 8 bits per chip on each update (about 4us per chip per update), and
// we only update when we get a command from the PC host that changes an
// output state.  These updates are at USB speed, so the update interval is
// extremely long compared to the bit-banging time.  If we wanted to use
// these chips to implement PWM controlled by the microcontroller, or we
// simply wanted to use a very long daisy-chain, we'd probably have to use 
// a faster transfer mechanism, such as the SPIO controller.

class HC595
{
public:
    HC595(int nchips, PinName sin, PinName sclk, PinName latch, PinName ena) :
        nchips(nchips), sin(sin), sclk(sclk), latch(latch), ena(ena)
    {
        // turn off all pins initially
        this->sin = 0;
        this->sclk = 0;
        this->latch = 0;
        this->ena = 0;
        
        // allocate the state array
        state = new char[nchips*8];
        memset(state, 0, nchips*8);
        dirty = false;
    }
    
    // Initialize.  This must be called once at startup to clear the chips' 
    // shift registers and enable the physical outputs.  We clock a 0 bit (OFF
    // state) to each shift register position, latch the OFF states on the
    // outputs, and enable the chips.
    void init()
    {
        // set the internal state of all inputs
        memset(state, 0, nchips*8);
        dirty = false;
        
        // clock a 0 to each shift register bit (8 per chip)
        sin = 0;
        for (int i = 0 ; i < nchips*8 ; ++i)
        {
            sclk = 1;
            sclk = 0;
        }
        
        // latch the output data (this transfers the serial data register
        // bit for each pin to the actual output pin)
        latch = 1;
        latch = 0;
        
        // enable the outputs
        ena = 1;
    }
    
    // Set an output state.  This only sets the state internally; call
    // update() to apply changes to the physical outputs.
    void set(int idx, int val)
    {
        if (state[idx] != val)
        {
            state[idx] = val;
            dirty = true;
        }
    }
    
    // Apply updates.  This sends the current state of each pin to the
    // chips and latches the new settings.
    void update()
    {
        // if we have changes to apply, send the changes
        if (dirty)
        {
            // Clock out the new states.  Since the outputs are arranged
            // as shift registers, we have to clock out the bits in reverse
            // order of port numbers - the first bit we output will end up
            // in the last register after we clock out all of the other bits.
            // So clock out the last bit first and the first bit last.
            for (int i = nchips*8-1 ; i >= 0 ; --i)
            {
                sclk = 0;
                sin = state[i];
                sclk = 1;
            }
            
            // latch the new states
            latch = 1;
            sclk = 0;
            latch = 0;
            
            // outputs now reflect internal state
            dirty = false;
        }
    }
    
    
private:
    int nchips;         // number of chips in daisy chain
    bool dirty;         // do we have changes to send to the chips?
    DigitalOut sin;     // serial data pin
    DigitalOut sclk;    // serial clock pin
    DigitalOut latch;   // latch pin
    DigitalOut ena;     // enable pin
    char *state;        // array of current output states (0=off, 1=on)
};
        
#endif // HC595_INCLUDED
