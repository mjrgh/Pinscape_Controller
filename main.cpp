#include "mbed.h"
#include "USBJoystick.h"
#include "MMA8451Q.h"
#include "tsl1410r.h"
#include "FreescaleIAP.h"

// on-board RGB LED elements - we use these for diagnostics
PwmOut led1(LED1), led2(LED2), led3(LED3);

// calibration button - switch input and LED output
DigitalIn calBtn(PTE29);
DigitalOut calBtnLed(PTE23);

static int pbaIdx = 0;

// on/off state for each LedWiz output
static uint8_t wizOn[32];

// profile (brightness/blink) state for each LedWiz output
static uint8_t wizVal[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static float wizState(int idx)
{
    if (wizOn[idx]) {
        // on - map profile brightness state to PWM level
        uint8_t val = wizVal[idx];
        if (val >= 1 && val <= 48)
            return 1.0 - val/48.0;
        else if (val >= 129 && val <= 132)
            return 0.0;
        else
            return 1.0;
    }
    else {
        // off
        return 1.0;
    }
}

static void updateWizOuts()
{
    led1 = wizState(0);
    led2 = wizState(1);
    led3 = wizState(2);
}

struct AccPrv
{
    AccPrv() : x(0), y(0) { }
    float x;
    float y;
    
    double dist(AccPrv &b)
    {
        float dx = x - b.x, dy = y - b.y;
        return sqrt(dx*dx + dy*dy);
    }
};

int main(void)
{
    // turn off our on-board indicator LED
    led1 = 1;
    led2 = 1;
    led3 = 1;
    
    // plunger calibration data
    const int npix = 320;
    int plungerMin = 0, plungerMax = npix;
    
    // plunger calibration button debounce timer
    Timer calBtnTimer;
    calBtnTimer.start();
    int calBtnDownTime = 0;
    int calBtnLit = false;
    
    // Calibration button state:
    //  0 = not pushed
    //  1 = pushed, not yet debounced
    //  2 = pushed, debounced, waiting for hold time
    //  3 = pushed, hold time completed - in calibration mode
    int calBtnState = 0;
    
    // set up a timer for our heartbeat indicator
    Timer hbTimer;
    hbTimer.start();
    int t0Hb = hbTimer.read_ms();
    int hb = 0;
    
    // set a timer for accelerometer auto-centering
    Timer acTimer;
    acTimer.start();
    int t0ac = acTimer.read_ms();
    
    // set up a timer for reading the plunger sensor
    Timer ccdTimer;
    ccdTimer.start();
    int t0ccd = ccdTimer.read_ms();
    
#if 0
    // DEBUG
    Timer ccdDbgTimer;
    ccdDbgTimer.start();
    int t0ccdDbg = ccdDbgTimer.read_ms();
#endif

    // Create the joystick USB client.  Light the on-board indicator LED
    // red while connecting, and change to green after we connect.
    led1 = 0.75;
    USBJoystick js(0xFAFA, 0x00F7, 0x0001);
    led1 = 1;
    led2 = 0.75;
    
    // create the accelerometer object
    const int MMA8451_I2C_ADDRESS = (0x1d<<1);
    MMA8451Q accel(PTE25, PTE24, MMA8451_I2C_ADDRESS);
    
    // create the CCD array object
    TSL1410R ccd(PTE20, PTE21, PTB0);

    // recent accelerometer readings, for auto centering
    int iAccPrv = 0, nAccPrv = 0;
    const int maxAccPrv = 5;
    AccPrv accPrv[maxAccPrv];

    // last accelerometer report, in mouse coordinates
    int x = 127, y = 127, z = 0;
    
    // raw accelerator centerpoint, on the unit interval (-1.0 .. +1.0)
    float xCenter = 0.0, yCenter = 0.0;    

    // we're all set up - now just loop, processing sensor reports and 
    // host requests
    for (;;)
    {
        // Look for an incoming report.  Continue processing input as
        // long as there's anything pending - this ensures that we
        // handle input in as timely a fashion as possible by deferring
        // output tasks as long as there's input to process.
        HID_REPORT report;
        while (js.readNB(&report) && report.length == 8)
        {
            uint8_t *data = report.data;
            if (data[0] == 64) 
            {
                // LWZ-SBA - first four bytes are bit-packed on/off flags
                // for the outputs; 5th byte is the pulse speed (0-7)
                //printf("LWZ-SBA %02x %02x %02x %02x ; %02x\r\n",
                //       data[1], data[2], data[3], data[4], data[5]);

                // update all on/off states
                for (int i = 0, bit = 1, ri = 1 ; i < 32 ; ++i, bit <<= 1)
                {
                    if (bit == 0x100) {
                        bit = 1;
                        ++ri;
                    }
                    wizOn[i] = ((data[ri] & bit) != 0);
                }
    
                // update the physical outputs
                updateWizOuts();
                
                // reset the PBA counter
                pbaIdx = 0;
            }
            else 
            {
                // LWZ-PBA - full state dump; each byte is one output
                // in the current bank.  pbaIdx keeps track of the bank;
                // this is incremented implicitly by each PBA message.
                //printf("LWZ-PBA[%d] %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
                //       pbaIdx, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

                // update all output profile settings
                for (int i = 0 ; i < 8 ; ++i)
                    wizVal[pbaIdx + i] = data[i];

                // update the physical LED state if this is the last bank                    
                if (pbaIdx == 24)
                    updateWizOuts();

                // advance to the next bank
                pbaIdx = (pbaIdx + 8) & 31;
            }
        }
       
        // check for plunger calibration
        if (!calBtn)
        {
            // check the state
            switch (calBtnState)
            {
            case 0: 
                // button not yet pushed - start debouncing
                calBtnTimer.reset();
                calBtnDownTime = calBtnTimer.read_ms();
                calBtnState = 1;
                break;
                
            case 1:
                // pushed, not yet debounced - if the debounce time has
                // passed, start the hold period
                if (calBtnTimer.read_ms() - calBtnDownTime > 50)
                    calBtnState = 2;
                break;
                
            case 2:
                // in the hold period - if the button has been held down
                // for the entire hold period, move to calibration mode
                if (calBtnTimer.read_ms() - calBtnDownTime > 2050)
                {
                    // enter calibration mode
                    calBtnState = 3;
                    
                    // reset the calibration limits
                    plungerMax = 0;
                    plungerMin = npix;
                }
                break;
            }
        }
        else
        {
            // Button released.  If we're not already in calibration mode,
            // reset the button state.  Once calibration mode starts, it sticks
            // until the calibration time elapses.
            if (calBtnState != 3)
                calBtnState = 0;
            else if (calBtnTimer.read_ms() - calBtnDownTime > 32500)
                calBtnState = 0;
        }       
        
        // light/flash the calibration button light, if applicable
        int newCalBtnLit = calBtnLit;
        switch (calBtnState)
        {
        case 2:
            // in the hold period - flash the light
            newCalBtnLit = (((calBtnTimer.read_ms() - calBtnDownTime)/250) & 1);
            break;
            
        case 3:
            // calibration mode - show steady on
            newCalBtnLit = true;
            break;
            
        default:
            // not calibrating/holding - show steady off
            newCalBtnLit = false;
            break;
        }
        if (calBtnLit != newCalBtnLit)
        {
            calBtnLit = newCalBtnLit;
            calBtnLed = (calBtnLit ? 1 : 0);
        }
        
        // read the plunger sensor
        int znew = z;
        /* if (ccdTimer.read_ms() - t0ccd > 33) */
        {
            // read the sensor at reduced resolution
            uint16_t pix[npix];
            ccd.read(pix, npix, 0);
            
#if 0
            // debug - send samples every 5 seconds
            if (ccdDbgTimer.read_ms() - t0ccdDbg > 5000)
            {
                for (int i = 0 ; i < npix ; ++i)
                    printf("%x ", pix[i]);
                printf("\r\n\r\n");
            
                ccdDbgTimer.reset();
                t0ccdDbg = ccdDbgTimer.read_ms();
            }
#endif
    
            // check which end is the brighter - this is the "tip" end
            // of the plunger
            long avg1 = (long(pix[0]) + long(pix[1]) + long(pix[2]) + long(pix[3]) + long(pix[4]))/5;
            long avg2 = (long(pix[npix-1]) + long(pix[npix-2]) + long(pix[npix-3]) + long(pix[npix-4]) + long(pix[npix-5]))/5;
            
            // figure the midpoint in the brightness
            long midpt = (avg1 + avg2)/2 * 3;
            
            // Work from the bright end to the dark end.  VP interprets the
            // Z axis value as the amount the plunger is pulled: the minimum
            // is the rest position, the maximum is fully pulled.  So we 
            // essentially want to report how much of the sensor is lit,
            // since this increases as the plunger is pulled back.
            int si = 1, di = 1;
            if (avg1 < avg2)
                si = npix - 1, di = -1;

            // scan for the midpoint                
            for (int n = 1, i = si ; n < npix - 1 ; ++n, i += di)
            {
                // if we've crossed the midpoint, report this position
                if (long(pix[i-1]) + long(pix[i]) + long(pix[i+1]) < midpt)
                {
                    // note the new position
                    int pos = abs(i - si);
                    
                    // Calibrate, or apply calibration, depending on the mode.
                    // In either case, normalize to a 0-127 range.  VP appears to
                    // ignore negative Z axis values.
                    if (calBtnState == 3)
                    {
                        // calibrating - note if we're expanding the calibration envelope
                        if (pos < plungerMin)
                            plungerMin = pos;   
                        if (pos > plungerMax)
                            plungerMax = pos;
                            
                        // normalize to the full physical range while calibrating
                        znew = int(float(pos)/npix * 127);
                    }
                    else
                    {
                        // running normally - normalize to the calibration range
                        if (pos < plungerMin)
                            pos = plungerMin;
                        if (pos > plungerMax)
                            pos = plungerMax;
                        znew = int(float(pos - plungerMin)/(plungerMax - plungerMin + 1) * 127);
                    }
                    
                    // done
                    break;
                }
            }
            
            // reset the timer
            ccdTimer.reset();
            t0ccd = ccdTimer.read_ms();
        } 
        
        // read the accelerometer
        float xa, ya;
        accel.getAccXY(xa, ya);
        
        // check for auto-centering every so often
        if (acTimer.read_ms() - t0ac > 1000) 
        {
            // add the sample to the history list
            accPrv[iAccPrv].x = xa;
            accPrv[iAccPrv].y = ya;
            
            // store the slot
            iAccPrv += 1;
            iAccPrv %= maxAccPrv;
            nAccPrv += 1;
            
            // If we have a full complement, check for stability.  The
            // raw accelerometer input is in the rnage -4096 to 4096, but
            // the class cover normalizes to a unit interval (-1.0 .. +1.0).
            const float accTol = .005;
            if (nAccPrv >= maxAccPrv
                && accPrv[0].dist(accPrv[1]) < accTol
                && accPrv[0].dist(accPrv[2]) < accTol
                && accPrv[0].dist(accPrv[3]) < accTol
                && accPrv[0].dist(accPrv[4]) < accTol)
            {
                // figure the new center
                xCenter = (accPrv[0].x + accPrv[1].x + accPrv[2].x + accPrv[3].x + accPrv[4].x)/5.0;
                yCenter = (accPrv[0].y + accPrv[1].y + accPrv[2].y + accPrv[3].y + accPrv[4].y)/5.0;
            }
            
            // reset the auto-center timer
            acTimer.reset();
            t0ac = acTimer.read_ms();
        }
        
        // adjust for our auto centering
        xa -= xCenter;
        ya -= yCenter;
        
        // confine to the unit interval
        if (xa < -1.0) xa = -1.0;
        if (xa > 1.0) xa = 1.0;
        if (ya < -1.0) ya = -1.0;
        if (ya > 1.0) ya = 1.0;

        // figure the new mouse report data
        int xnew = (int)(127 * xa);
        int ynew = (int)(127 * ya);
        
        // send an update if the position has changed
        // if (xnew != x || ynew != y || znew != z)
        {
            x = xnew;
            y = ynew;
            z = znew;

            // Send the status report.  Note that the X axis needs to be
            // reversed, becasue the native accelerometer reports seem to
            // assume that the card is component side down.
            js.update(x, -y, z, 0);
        }
        
        // show a heartbeat flash in blue every so often
        if (hbTimer.read_ms() - t0Hb > 1000) 
        {
            // invert the blue LED state
            hb = !hb;
            led3 = (hb ? .5 : 1);
            
            // reset the heartbeat timer
            hbTimer.reset();
            t0Hb = hbTimer.read_ms();
        }
    }
}
