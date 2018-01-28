// New PWM 
//
// This is a replacement for the mbed PwmOut class.  It's both stripped
// down and beefed up.  It's stripped down to just the functionality we 
// need in the Pinscape code, and to a purely KL25Z implementation, which
// allows for a smaller memory footprint per instance.  It's beefed up to
// correct a number of problems in the mbed implementation.  
//
// Note that this class isn't quite API-compatible with the mbed version.
// We make the channel/TPM unit structure explicit, and we put the period()
// method (to change the PWM cycle time) on the unit object rather than the
// channel.  We do this to emphasize in the API that the period is a property
// of the unit (which contains multiple channels) rather than the channel.
// The mbed library is misleading when it pretends that the period is a
// property of the channel, since this confusingly suggests that a channel's
// period can be set independently.  It can't; the period can only be set for
// the whole group of channels controlled by a unit.
//
// Improvements over the mbed version:
//
// 1. We provide an alternative, non-glitching version of write().  The mbed
// version of write(), and our default version with the same name, causes a 
// glitch on every write by resetting the TPM counter, which cuts the cycle
// short and causes a momentary drop in brightness (from the short cycle) 
// that's visible if an LED is connected.  This is particularly noticeable 
// when doing a series of rapid writes, such as when fading a light on or off.
//
// We offer a version of write() that doesn't reset the counter, avoiding the 
// glitch.  This version skips the counter reset that the default version does.
//
// But this must be used with caution, because there's a whole separate
// problem if you don't reset the counter, which is why the mbed library
// does this by default.  The KL25Z hardware only allows the value register
// to be written once per PWM cycle; if it's written more than once, the
// second and subsequent writes are simply ignored, so those updates will
// be forever lost.  The counter reset, in addition to casuing the glitch,
// resets the cycle and thus avoids the one-write-per-cycle limitation.
// Callers using the non-glitchy version must take care to time writes so
// that there's only one per PWM period.  Or, alternatively, they can just
// be sure to repeat updates periodically to ensure that the last update is
// eventually applied.
//
// 2. We optimize the TPM clock pre-scaler to maximize the precision of the
// output period, to get as close as possible to the requested period.  The
// base mbed code uses a fixed pre-scaler setting with a fixed 750kHz update
// frequency, which means the period can be set in 1.333us increments.  The
// hardware is capable of increments as small as .02us.  The tradeoff is that
// the higher precision modes with smaller increments only allow for limited
// total period lengths, since the cycle counter is 16 bits: the maximum
// period at a given clock increment is 65535 times the increment.  So the
// mbed default of 1.333us increments allows for periods of up to 87ms with
// 1.333us precision, whereas the maximum precision of .02us increments only
// allows for a maximum period of 1.36ms.
//
// To deal with this tradeoff, we choose the scaling factor each time the
// period is changed, using the highest precision (smallest time increment,
// or lowest pre-scaling clock divider) available for the requested period.
// 
// Similar variable pre-scaling functionality is available with the FastPWM
// class.
//
// 3. We properly handle the shared clock in the TPM units.  The mbed library
// doesn't, nor does FastPWM.
//
// The period/frequency of a PWM channel on the KL25Z is a function of the
// TPM unit containing the channel, NOT of the channel itself.  A channel's
// frequency CANNOT be set independently; it can only set for the entire 
// group of channels controlled through the same TPM unit as the target
// channel.
//
// The mbed library and FastPWM library pretend that the period can be set
// per channel.  This is misleading and bug-prone, since an application that
// takes the API at its word and sets a channel's frequency on the fly won't
// necessarily realize that it just changed the frequency for all of the other
// channels on the same TPM.  What's more, the change in TPM period will
// effectively change the duty cycle for all channels attached to the PWM,
// since it'll update the counter modulus, so all channels on the same TPM
// have to have their duty cycles reset after any frequency change.
//
// This implementation changes the API design to better reflect reality.  We
// expose a separate object representing the TPM unit for a channel, and we
// put the period update function on the TPM unit object rather than on the
// channel.  We also automatically update the duty cycle variable for all
// channels on a TPM when updating the frequency, to maintain the original
// duty cycle (or as close as possible, after rounding error).
//
// Applications that need to control the duty cycle on more than one channel
// must take care to ensure that the separately controlled channels are on 
// separate TPM units.  The KL25Z offers three physical TPM units, so there
// can be up to three independently controlled periods.  The KL25Z has 10
// channels in total (6 on unit 0, 2 on unit 1, 2 on unit 2), so the remaining
// 7 channels have to share their periods with their TPM unit-mates.


#ifndef _NEWPWMOUT_H_
#define _NEWPWMOUT_H_

#include <mbed.h>
#include <pinmap.h>
#include <PeripheralPins.h>
#include <clk_freqs.h>

// TPM Unit.  This corresponds to one TPM unit in the hardware.  Each
// unit controls 6 channels; a channel corresponds to one output pin.
// A unit contains the clock input, pre-scaler, counter, and counter 
// modulus; these are shared among all 6 channels in the unit, and
// together determine the cycle time (period) of all channels in the
// unit.  The period of a single channel can't be set independently;
// a channel takes its period from its unit.
//
// Since the KL25Z hardware has a fixed set of 3 TPM units, we have
// a fixed array of 3 of these objects.
class NewPwmUnit
{
public:
    NewPwmUnit()
    {
        // figure our unit number from the singleton array position
        int tpm_n = this - unit;
        
        // start with all channels disabled
        activeChannels = 0;
        
        // get our TPM unit hardware register base
        tpm = (TPM_Type *)(TPM0_BASE + 0x1000*tpm_n);
        
        // Determine which clock input we're using.  Save the clock
        // frequency for later use when setting the PWM period, and 
        // set up the SIM control register for the appropriate clock
        // input.  This setting is global, so we really only need to
        // do it once for all three units, but it'll be the same every
        // time so it won't hurt (except for a little redundancy) to
        // do it again on each unit constructor.
        if (mcgpllfll_frequency()) {
            SIM->SOPT2 |= SIM_SOPT2_TPMSRC(1); // Clock source: MCGFLLCLK or MCGPLLCLK
            sysClock = mcgpllfll_frequency();
        } else {
            SIM->SOPT2 |= SIM_SOPT2_TPMSRC(2); // Clock source: ExtOsc
            sysClock = extosc_frequency();
        }
    }
    
    // enable a channel
    void enableChannel(int ch)
    {
        // if this is the first channel we're enabling, enable the
        // unit clock gate
        if (activeChannels == 0)
        {
            // enable the clock gate on the TPM unit
            int tpm_n = this - unit;
            SIM->SCGC6 |= 1 << (SIM_SCGC6_TPM0_SHIFT + tpm_n);
            
            // set a default period of 20ms
            period(20.0e-3f);
        }
        
        // add the channel bit to our collection
        activeChannels |= (1 << ch);
    }
    
    // Set the period for the unit.  This updates all channels associated
    // with the unit so that their duty cycle is scaled properly to the
    // period counter.
    void period(float seconds)
    {        
        // First check to see if we actually need to change anything.  If
        // the requested period already matches the current period, there's
        // nothing to do.  This will avoid unnecessarily resetting any
        // running cycles, which could cause visible flicker.
        uint32_t freq = sysClock >> (tpm->SC & TPM_SC_PS_MASK);
        uint32_t oldMod = tpm->MOD;
        uint32_t newMod = uint32_t(seconds*freq) - 1;
        if (newMod == oldMod && (tpm->SC & TPM_SC_CMOD_MASK) == TPM_SC_CMOD(1))
            return;
    
        // Figure the minimum pre-scaler needed to allow this period.  The
        // unit counter is 16 bits, so the maximum cycle length is 65535
        // ticks.  One tick is the system clock tick time multiplied by
        // the pre-scaler.  The scaler comes in powers of two from 1 to 128.
        
        // start at scaler=0 -> divide by 1
        int ps = 0;
        freq = sysClock;
        
        // at this rate, the maximum period is 65535 ticks of the system clock
        float pmax = 65535.0f/sysClock;
        
        // Now figure how much we have to divide the system clock: each
        // scaler step divides by another factor of 2, which doubles the
        // maximum period.  Keep going while the maximum period is below
        // the desired period, but stop if we reach the maximum per-scale
        // value of divide-by-128.
        while (ps < 7 && pmax < seconds)
        {
            ++ps;
            pmax *= 2.0f;
            freq /= 2;
        }

        // Before writing the prescaler bits, we have to disable the
        // clock (CMOD) bits in the status & control register.  These
        // bits might take a while to update, so spin until they clear.
        while ((tpm->SC & 0x1F) != 0)
            tpm->SC &= ~0x1F;

        // Reset the CnV (trigger value) for all active channels to
        // maintain each channel's current duty cycle.
        for (int i = 0 ; i < 6 ; ++i)
        {
            // if this channel is active, reset it
            if ((activeChannels & (1 << i)) != 0)
            {
                // figure the old duty cycle, based on the current
                // channel value and the old modulus
                uint32_t oldCnV = tpm->CONTROLS[i].CnV;
                float dc = float(oldCnV)/float(oldMod + 1);
                if (dc > 1.0f) dc = 1.0f;
                
                // figure the new value that maintains the same duty
                // cycle with the new modulus
                uint32_t newCnV = uint32_t(dc*(newMod + 1));
                
                // if it changed, write the new value
                if (newCnV != oldCnV)
                    tpm->CONTROLS[i].CnV = newCnV;
            }
        }

        // reset the unit counter register
        tpm->CNT = 0;
        
        // set the new clock period
        tpm->MOD = newMod = uint32_t(seconds*freq) - 1;
        
        // set the new pre-scaler bits and set clock mode 01 (enabled, 
        // increments on every LPTPM clock)
        tpm->SC = TPM_SC_CMOD(1) | TPM_SC_PS(ps);
    }
    
    // wait for the end of the current cycle
    void waitEndCycle()
    {
        // clear the overflow flag
        tpm->SC |= TPM_SC_TOF_MASK;
        
        // The flag will be set at the next overflow
        while (!(tpm->SC & TPM_SC_TOF_MASK)) ;
    }
    
    // hardware register base
    TPM_Type *tpm;
    
    // Channels that are active in this unit, as a bit mask:
    // 1<<n is our channel n.
    uint8_t activeChannels;
    
    // fixed array of unit singletons
    static NewPwmUnit unit[3];
    
    // system clock frequency
    static uint32_t sysClock;
};


class NewPwmOut
{
public:
    NewPwmOut(PinName pin)
    {
        // determine the TPM unit number and channel
        PWMName pwm = (PWMName)pinmap_peripheral(pin, PinMap_PWM);
        MBED_ASSERT(pwm != (PWMName)NC);
        unsigned int port = (unsigned int)pin >> PORT_SHIFT;
        
        // decode the port ID into the TPM unit and channel number
        tpm_n = (pwm >> TPM_SHIFT);
        ch_n  = (pwm & 0xFF);
        
        // enable the clock gate on the port (PTx)
        SIM->SCGC5 |= 1 << (SIM_SCGC5_PORTA_SHIFT + port);
        
        // enable the channel on the TPM unit
        NewPwmUnit::unit[tpm_n].enableChannel(ch_n);

        // set the channel control register:
        //   CHIE                = 0    = interrupts disabled
        //   MSB:MBA:ELSB:ELSA   = 1010 = edge-aligned PWM
        //   DMA                 = 0    = DMA off
        TPM_Type *tpm = getUnit()->tpm;
        tpm->CONTROLS[ch_n].CnSC = (TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK);
                
        // wire the pinout
        pinmap_pinout(pin, PinMap_PWM);
    }
    
    float read()
    {
        TPM_Type *tpm = getUnit()->tpm;
        float v = float(tpm->CONTROLS[ch_n].CnV)/float(tpm->MOD + 1);
        return v > 1.0f ? 1.0f : v;
    }
    
    void write(float val)
    {
        // do the glitch-free write
        glitchFreeWrite(val);
        
        // Reset the counter.  This is a workaround for a hardware problem
        // on the KL25Z, namely that the CnV register can only be written
        // once per PWM cycle.  Any subsequent attempt to write it in the
        // same cycle will be lost.  Resetting the counter forces the end
        // of the cycle and makes the register writable again.  This isn't
        // an ideal workaround because it causes visible brightness glitching
        // if the caller writes new values repeatedly, such as when fading
        // lights in or out.
        TPM_Type *tpm = getUnit()->tpm;
        tpm->CNT = 0;    
    }

    // Write a new value without forcing the current PWM cycle to end.
    // This results in glitch-free writing during fades or other series
    // of rapid writes, BUT with the giant caveat that the caller MUST NOT
    // write another value before the current PWM cycle ends.  Doing so
    // will cause the later write to be lost.  Callers using this must 
    // take care, using mechanisms of their own, to limit writes to once
    // per PWM cycle.
    void glitchFreeWrite(float val)
    {
        // limit to 0..1 range
        val = (val < 0.0f ? 0.0f : val > 1.0f ? 1.0f : val);
    
        // Write the duty cycle register.  The argument value is a duty
        // cycle on a normalized 0..1 scale; for the hardware, we need to
        // renormalize to the 0..MOD scale, where MOD is the cycle length 
        // in clock counts.  
        TPM_Type *tpm = getUnit()->tpm;
        tpm->CONTROLS[ch_n].CnV = (uint32_t)((float)(tpm->MOD + 1) * val);
    }
    
    // Wait for the end of a cycle
    void waitEndCycle() { getUnit()->waitEndCycle(); }
    
    // Get my TPM unit object.  This can be used to change the period.
    inline NewPwmUnit *getUnit() { return &NewPwmUnit::unit[tpm_n]; }
    
protected:
    // TPM unit number and channel number
    uint8_t tpm_n;
    uint8_t ch_n;
};

#endif
