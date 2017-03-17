// IRPwmOut - KL25Z ONLY
//
// This is a lightweight reimplementation of PwmOut for KL25Z only.
// It's basically a bare-metal version of the parts of PwmOut we
// need.  There are two main differences from the base class:
//
// - Since we're KL25Z only, we can be less abstract about the
//   hardware register interface, allowing us to store fewer
//   internal pointers.  This makes our memory footprint smaller.
//
// - We allow for higher precision in setting the PWM period for
//   short cycles, by setting the TPM pre-scaler to 1 and using
//   fractional microsecond counts.  The KL25Z native clock rate
//   is 48MHz, which translates to 20ns ticks - meaning we can get
//   .02us precision in our PWM periods.  The base mbed library 
//   sets the pre-scaler to 64, allowing only 1.33us precision.
//   The reason it does this is the tradeoff of precision against
//   maximum cycle length.  The KL25Z TPM has a 16-bit counter,
//   so the longest cycle is 65536 * one clock.  With pre-scaling
//   at 1, this is a maximum 1.365ms cycle, whereas it's 87ms with
//   the pre-scaler at the mbed setting of 64.  That's a good
//   default for most applications; for our purposes, though,
//   we're always working with cycles in the 35kHz to 40kHz range,
//   so our cycles are all sub-millisecond.

#ifndef _IRPWMOUT_H_
#define _IRPWMOUT_H_

#include <mbed.h>
#include <pinmap.h>
#include <PeripheralPins.h>
#include <clk_freqs.h>

class IRPwmOut
{
public:
    IRPwmOut(PinName pin)
    {
        // determine the channel
        PWMName pwm = (PWMName)pinmap_peripheral(pin, PinMap_PWM);
        MBED_ASSERT(pwm != (PWMName)NC);
        unsigned int port  = (unsigned int)pin >> PORT_SHIFT;
        unsigned int tpm_n = (pwm >> TPM_SHIFT);
        this->ch_n  = (pwm & 0xFF);
        
        // get the system clock rate    
        if (mcgpllfll_frequency()) {
            SIM->SOPT2 |= SIM_SOPT2_TPMSRC(1); // Clock source: MCGFLLCLK or MCGPLLCLK
            pwm_clock = mcgpllfll_frequency() / 1000000.0f;
        } else {
            SIM->SOPT2 |= SIM_SOPT2_TPMSRC(2); // Clock source: ExtOsc
            pwm_clock = extosc_frequency() / 1000000.0f;
        }
        
        // enable the clock gate on the port (PTx) and TPM module
        SIM->SCGC5 |= 1 << (SIM_SCGC5_PORTA_SHIFT + port);
        SIM->SCGC6 |= 1 << (SIM_SCGC6_TPM0_SHIFT + tpm_n);

        // fix the pre-scaler at divide-by-1 for maximum precision    
        const uint32_t clkdiv = 0;

        // set up the TPM registers: counter enabled, pre-scaler as set above,
        // no interrupts, high 'true', edge aligned
        tpm = (TPM_Type *)(TPM0_BASE + 0x1000 * tpm_n);
        tpm->SC = TPM_SC_CMOD(1) | TPM_SC_PS(clkdiv);
        tpm->CONTROLS[ch_n].CnSC = (TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK);
    
        // clear the count register to turn off the output
        tpm->CNT = 0;
        
        // default to 1ms period
        period_us(1000.0f);
        printf("IRPwmOut,  SC=%08lx, CnSC=%08lx\r\n",
            tpm->SC, tpm->CONTROLS[ch_n].CnSC);
    
        // wire the pinout
        pinmap_pinout(pin, PinMap_PWM);
    }
    
    float read()
    {
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
        tpm->CONTROLS[ch_n].CnV = (uint32_t)((float)(tpm->MOD + 1) * val);
    }
    
    void period_us(float us)
    {
        if (tpm->SC == (TPM_SC_CMOD(1) | TPM_SC_PS(0))
            && tpm->CONTROLS[ch_n].CnSC == (TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK))
            printf("period_us ok\r\n");
        else
            printf("period_us regs changed??? %08lx, %08lx\r\n",
                tpm->SC, tpm->CONTROLS[ch_n].CnSC);

        float dc = read();
        tpm->MOD = (uint32_t)(pwm_clock * (float)us) - 1;
        write(dc);
    }
    
protected:
    // hardware register base, and TPM channel number we're on
    TPM_Type *tpm;
    uint8_t ch_n;
    
    // global clock rate - store statically
    static float pwm_clock;
};

#endif
