#include "mbed.h"
#include "USBJoystick.h"
#include "MMA8451Q.h"
#include "tsl1410r.h"
#include "FreescaleIAP.h"
#include "crc32.h"

// customization of the joystick class to expose connect/suspend status
class MyUSBJoystick: public USBJoystick
{
public:
    MyUSBJoystick(uint16_t vendor_id, uint16_t product_id, uint16_t product_release) 
        : USBJoystick(vendor_id, product_id, product_release)
    {
        connected_ = false;
        suspended_ = false;
    }
    
    int isConnected() const { return connected_; }
    int isSuspended() const { return suspended_; }
    
protected:
    virtual void connectStateChanged(unsigned int connected) 
        { connected_ = connected; }
    virtual void suspendStateChanged(unsigned int suspended)
        { suspended_ = suspended; }

    int connected_;
    int suspended_; 
};

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

// Non-volatile memory structure.  We store persistent a small
// amount of persistent data in flash memory to retain calibration
// data between sessions.
struct NVM
{
    // checksum - we use this to determine if the flash record
    // has been initialized
    uint32_t checksum;

    // signature value
    static const uint32_t SIGNATURE = 0x4D4A522A;
    static const uint16_t VERSION = 0x0002;
    
    // stored data (excluding the checksum)
    struct
    {
        // signature and version - further verification that we have valid 
        // initialized data
        uint32_t sig;
        uint16_t vsn;
        
        // direction - 0 means unknown, 1 means bright end is pixel 0, 2 means reversed
        uint8_t dir;

        // plunger calibration min and max
        int plungerMin;
        int plungerMax;
    } d;
};

int main(void)
{
    // turn off our on-board indicator LED
    led1 = 1;
    led2 = 1;
    led3 = 1;
    
    // set up a flash memory controller
    FreescaleIAP iap;
    
    // use the last sector of flash for our non-volatile memory structure
    int flash_addr = (iap.flash_size() - SECTOR_SIZE);
    NVM *flash = (NVM *)flash_addr;
    NVM cfg;
    
    // check for valid flash
    bool flash_valid = (flash->d.sig == flash->SIGNATURE 
                        && flash->d.vsn == flash->VERSION
                        && flash->checksum == CRC32(&flash->d, sizeof(flash->d)));
                      
    // Number of pixels we read from the sensor on each frame.  This can be
    // less than the physical pixel count if desired; we'll read every nth
    // piexl if so.  E.g., with a 1280-pixel physical sensor, if npix is 320,
    // we'll read every 4th pixel.  VP doesn't seem to have very high
    // resolution internally for the plunger, so it's probably not necessary
    // to use the full resolution of the sensor - about 160 pixels seems
    // perfectly adequate.  We can read the sensor faster (and thus provide
    // a higher refresh rate) if we read fewer pixels in each frame.
    const int npix = 160;

    // if the flash is valid, load it; otherwise initialize to defaults
    if (flash_valid) {
        memcpy(&cfg, flash, sizeof(cfg));
        printf("Flash restored: plunger min=%d, max=%d\r\n", 
            cfg.d.plungerMin, cfg.d.plungerMax);
    }
    else {
        printf("Factory reset\r\n");
        cfg.d.sig = cfg.SIGNATURE;
        cfg.d.vsn = cfg.VERSION;
        cfg.d.plungerMin = 0;
        cfg.d.plungerMax = npix;
    }
    
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
    
    // Create the joystick USB client.  Light the on-board indicator LED
    // red while connecting, and change to green after we connect.
    led1 = 0;
    MyUSBJoystick js(0xFAFA, 0x00F7, 0x0001);
    led1 = 1;
    led2 = 0;

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
    
    // start the first CCD integration cycle
    ccd.clear();

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
                    cfg.d.plungerMax = 0;
                    cfg.d.plungerMin = npix;
                }
                break;
                
            case 3:
                // Already in calibration mode - pushing the button in this
                // state doesn't change the current state, but we won't leave
                // this state as long as it's held down.  We can simply do
                // nothing here.
                break;
            }
        }
        else
        {
            // Button released.  If we're in calibration mode, and
            // the calibration time has elapsed, end the calibration
            // and save the results to flash.
            //
            // Otherwise, return to the base state without saving anything.
            // If the button is released before we make it to calibration
            // mode, it simply cancels the attempt.
            if (calBtnState == 3
                && calBtnTimer.read_ms() - calBtnDownTime > 17500)
            {
                // exit calibration mode
                calBtnState = 0;
                
                // Save the current configuration state to flash, so that it
                // will be preserved through power off.  Update the checksum
                // first so that we recognize the flash record as valid.
                cfg.checksum = CRC32(&cfg.d, sizeof(cfg.d));
                iap.erase_sector(flash_addr);
                iap.program_flash(flash_addr, &cfg, sizeof(cfg));
                
                // the flash state is now valid
                flash_valid = true;
            }
            else if (calBtnState != 3)
            {
                // didn't make it to calibration mode - cancel the operation
                calBtnState = 0;
            }
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
            if (calBtnLit) {
                calBtnLed = 1;
                led1 = 0;
                led2 = 0;
                led3 = 1;
            }
            else {
                calBtnLed = 0;
                led1 = 1;
                led2 = 1;
                led3 = 0;
            }
        }
        
        // read the plunger sensor
        int znew = z;
        uint16_t pix[npix];
        ccd.read(pix, npix);

        // get the average brightness at each end of the sensor
        long avg1 = (long(pix[0]) + long(pix[1]) + long(pix[2]) + long(pix[3]) + long(pix[4]))/5;
        long avg2 = (long(pix[npix-1]) + long(pix[npix-2]) + long(pix[npix-3]) + long(pix[npix-4]) + long(pix[npix-5]))/5;
        
        // figure the midpoint in the brightness; multiply by 3 so that we can
        // compare sums of three pixels at a time to smooth out noise
        long midpt = (avg1 + avg2)/2 * 3;
        
        // Work from the bright end to the dark end.  VP interprets the
        // Z axis value as the amount the plunger is pulled: the minimum
        // is the rest position, the maximum is fully pulled.  So we 
        // essentially want to report how much of the sensor is lit,
        // since this increases as the plunger is pulled back.
        int si = 1, di = 1;
        if (avg1 < avg2)
            si = npix - 2, di = -1;

        // scan for the midpoint     
        uint16_t *pixp = pix + si;           
        for (int n = 1 ; n < npix - 1 ; ++n, pixp += di)
        {
            // if we've crossed the midpoint, report this position
            if (long(pixp[-1]) + long(pixp[0]) + long(pixp[1]) < midpt)
            {
                // note the new position
                int pos = n;
                
                // if the bright end and dark end don't differ by enough, skip this
                // reading entirely - we must have an overexposed or underexposed frame
                if (labs(avg1 - avg2) < 0x3333)
                    break; 
                
                // Calibrate, or apply calibration, depending on the mode.
                // In either case, normalize to a 0-127 range.  VP appears to
                // ignore negative Z axis values.
                if (calBtnState == 3)
                {
                    // calibrating - note if we're expanding the calibration envelope
                    if (pos < cfg.d.plungerMin)
                        cfg.d.plungerMin = pos;   
                    if (pos > cfg.d.plungerMax)
                        cfg.d.plungerMax = pos;
                        
                    // normalize to the full physical range while calibrating
                    znew = int(float(pos)/npix * 127);
                }
                else
                {
                    // running normally - normalize to the calibration range
                    if (pos < cfg.d.plungerMin)
                        pos = cfg.d.plungerMin;
                    if (pos > cfg.d.plungerMax)
                        pos = cfg.d.plungerMax;
                    znew = int(float(pos - cfg.d.plungerMin)
                        / (cfg.d.plungerMax - cfg.d.plungerMin + 1) * 127);
                }
                
                // done
                break;
            }
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

        // store the updated joystick coordinates
        x = xnew;
        y = ynew;
        z = znew;
        
        // if we're in USB suspend or disconnect mode, spin
        if (js.isSuspended() || !js.isConnected())
        {
            // go dark (turn off the indicator LEDs)
            led2 = 1;
            led3 = 1;
            led1 = 1;
            
            // wait until we're connected and come out of suspend mode
            while (js.isSuspended() || !js.isConnected())
            {
                // spin for a bit
                wait(1);
                
                // if we're not suspended, flash red; otherwise stay dark
                if (!js.isSuspended())
                    led1 = !led1;
            }
        }

        // Send the status report.  Note one of the axes needs to be
        // reversed, because the native accelerometer reports seem to
        // assume that the card is component side down; we have to
        // reverse one or the other axis to account for the reversed
        // coordinate system.  It doesn't really matter which one,
        // but reversing Y seems to give intuitive results when viewed
        // in the Windows joystick control panel.  Note that the 
        // coordinate system we report is ultimately arbitrary, since
        // Visual Pinball has preference settings that let us set up
        // axis reversals and a global rotation for the joystick.
        js.update(x, -y, z, 0);
        
        // show a heartbeat flash in blue every so often if not in 
        // calibration mode
        if (calBtnState < 2 && hbTimer.read_ms() - t0Hb > 1000) 
        {
            if (js.isSuspended())
            {
                // suspended - turn off the LEDs entirely
                led1 = 1;
                led2 = 1;
                led3 = 1;
            }
            else if (!js.isConnected())
            {
                // not connected - flash red
                hb = !hb;
                led1 = (hb ? 0 : 1);
                led2 = 1;
                led3 = 1;
            }
            else if (flash_valid)
            {
                // connected, NVM valid - flash blue/green
                hb = !hb;
                led1 = 1;
                led2 = (hb ? 0 : 1);
                led3 = (hb ? 1 : 0);
            }
            else
            {
                // connected, factory reset - flash yellow/green
                hb = !hb;
                led1 = (hb ? 0 : 1);
                led2 = 0;
                led3 = 0;
            }
            
            // reset the heartbeat timer
            hbTimer.reset();
            t0Hb = hbTimer.read_ms();
        }
    }
}
