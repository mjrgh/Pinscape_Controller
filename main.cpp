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
        : USBJoystick(vendor_id, product_id, product_release, false)
    {
        suspended_ = false;
    }
    
    int isConnected() { return configured(); }
    int isSuspended() const { return suspended_; }
    
protected:
    virtual void suspendStateChanged(unsigned int suspended)
        { suspended_ = suspended; }

    int suspended_; 
};

// On-board RGB LED elements - we use these for diagnostic displays.
DigitalOut ledR(LED1), ledG(LED2), ledB(LED3);

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
    ledR = wizState(0);
    ledG = wizState(1);
    ledB = wizState(2);
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

// Accelerometer handler
const int MMA8451_I2C_ADDRESS = (0x1d<<1);
class Accel
{
public:
    Accel(PinName sda, PinName scl, int i2cAddr, PinName irqPin)
        : mma_(sda, scl, i2cAddr), intIn_(irqPin)
    {
        // set the initial ball velocity to zero
        vx_ = vy_ = 0;
        
        // set the initial raw acceleration reading to zero
        xRaw_ = yRaw_ = 0;

        // enable the interrupt
        mma_.setInterruptMode(irqPin == PTA14 ? 1 : 2);
        
        // set up the interrupt handler
        intIn_.rise(this, &Accel::isr);
        
        // read the current registers to clear the data ready flag
        float z;
        mma_.getAccXYZ(xRaw_, yRaw_, z);

        // start our timers
        tGet_.start();
        tInt_.start();
    }
    
    void get(float &x, float &y, float &rx, float &ry) 
    {
         // disable interrupts while manipulating the shared data
         __disable_irq();
         
         // read the shared data and store locally for calculations
         float vx = vx_, vy = vy_, xRaw = xRaw_, yRaw = yRaw_;

         // reset the velocity
         vx_ = vy_ = 0;
         
         // get the time since the last get() sample
         float dt = tGet_.read_us()/1.0e6;
         tGet_.reset();
         
         // done manipulating the shared data
         __enable_irq();
         
         // calculate the acceleration since the last get(): a = dv/dt
         x = vx/dt;
         y = vy/dt;         
         
         // return the raw accelerometer data in rx,ry
         rx = xRaw;
         ry = yRaw;
     }    
    
private:
    // interrupt handler
    void isr()
    {
        // Read the axes.  Note that we have to read all three axes
        // (even though we only really use x and y) in order to clear
        // the "data ready" status bit in the accelerometer.  The
        // interrupt only occurs when the "ready" bit transitions from
        // off to on, so we have to make sure it's off.
        float z;
        mma_.getAccXYZ(xRaw_, yRaw_, z);
        
        // calculate the time since the last interrupt
        float dt = tInt_.read_us()/1.0e6;
        tInt_.reset();
        
        // Accelerate the model ball: v = a*dt.  Assume that the raw
        // data from the accelerometer reflects the average physical
        // acceleration over the interval since the last sample.
        vx_ += xRaw_ * dt;
        vy_ += yRaw_ * dt;
    }
    
    // current modeled ball velocity
    float vx_, vy_;
    
    // last raw axis readings
    float xRaw_, yRaw_;
    
    // underlying accelerometer object
    MMA8451Q mma_;
    
    // interrupt router
    InterruptIn intIn_;
    
    // timer for measuring time between get() samples
    Timer tGet_;
    
    // timer for measuring time between interrupts
    Timer tInt_;
};

int main(void)
{
    // turn off our on-board indicator LED
    ledR = 1;
    ledG = 1;
    ledB = 1;
    
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
    
    // Create the joystick USB client
    MyUSBJoystick js(0xFAFA, 0x00F7, 0x0003);

    // create the accelerometer object
    Accel accel(PTE25, PTE24, MMA8451_I2C_ADDRESS, PTA15);
    
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
        
        // light or flash the external calibration button LED, and 
        // do the same with the on-board blue LED
        if (calBtnLit != newCalBtnLit)
        {
            calBtnLit = newCalBtnLit;
            if (calBtnLit) {
                calBtnLed = 1;
                ledR = 1;
                ledG = 1;
                ledB = 1;
            }
            else {
                calBtnLed = 0;
                ledR = 1;
                ledG = 1;
                ledB = 0;
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
        float xa, ya, rxa, rya;
        accel.get(xa, ya, rxa, rya);
        
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
            ledG = 1;
            ledB = 1;
            ledR = 1;
            
            // wait until we're connected and come out of suspend mode
            for (uint32_t n = 0 ; js.isSuspended() || !js.isConnected() ; ++n)
            {
                // spin for a bit
                wait(1);
                
                // if we're suspended, do a brief red flash; otherwise do a long red flash
                if (js.isSuspended())
                {
                    // suspended - flash briefly ever few seconds
                    if (n % 3 == 0)
                    {
                        ledR = 0;
                        wait(0.05);
                        ledR = 1;
                    }
                }
                else
                {
                    // running, not connected - flash red
                    ledR = !ledR;
                }
            }
        }

        // Send the status report.  It doesn't really matter what
        // coordinate system we use, since Visual Pinball has config
        // options for rotations and axis reversals, but reversing y
        // at the device level seems to produce the most intuitive 
        // results for the Windows joystick control panel view, which
        // is an easy way to check that the device is working.
        js.update(x, -y, z, int(rxa*127), int(rya*127), 0);
        
        // show a heartbeat flash in blue every so often if not in 
        // calibration mode
        if (calBtnState < 2 && hbTimer.read_ms() - t0Hb > 1000) 
        {
            if (js.isSuspended())
            {
                // suspended - turn off the LEDs entirely
                ledR = 1;
                ledG = 1;
                ledB = 1;
            }
            else if (!js.isConnected())
            {
                // not connected - flash red
                hb = !hb;
                ledR = (hb ? 0 : 1);
                ledG = 1;
                ledB = 1;
            }
            else if (flash_valid)
            {
                // connected, NVM valid - flash blue/green
                hb = !hb;
                ledR = 1;
                ledG = (hb ? 0 : 1);
                ledB = (hb ? 1 : 0);
            }
            else
            {
                // connected, factory reset - flash yellow/green
                hb = !hb;
                ledR = (hb ? 0 : 1);
                ledG = 0;
                ledB = 1;
            }
            
            // reset the heartbeat timer
            hbTimer.reset();
            t0Hb = hbTimer.read_ms();
        }
    }
}
