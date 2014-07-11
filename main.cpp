#include "mbed.h"
#include "USBJoystick.h"
#include "MMA8451Q.h"
#include "tls1410r.h"

PwmOut led1(LED1), led2(LED2), led3(LED3);
DigitalOut out1(PTE29);



static int pbaIdx = 0;

// on/off state for each LedWiz output
static uint8_t ledOn[32];

// profile (brightness/blink) state for each LedWiz output
static uint8_t ledVal[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static double ledState(int idx)
{
    if (ledOn[idx]) {
        // on - map profile brightness state to PWM level
        uint8_t val = ledVal[idx];
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

static void updateLeds()
{
    led1 = ledState(0);
    led2 = ledState(1);
    led3 = ledState(2);
}

int main(void)
{
    led1 = 1;
    led2 = 1;
    led3 = 1;
    Timer timer;

    // set up a timer for spacing USB reports   
    timer.start();
    float t0 = timer.read_ms();    
    float tout1 = timer.read_ms();

    // Create the joystick USB client.  Show a read LED while connecting, and
    // change to green when connected.
    led1 = 0.75;
    USBJoystick js(0xFAFA, 0x00F7, 0x0001);
    led1 = 1;
    led2 = 0.75;
    
    // create the accelerometer object
    const int MMA8451_I2C_ADDRESS = (0x1d<<1);
    MMA8451Q accel(PTE25, PTE24, MMA8451_I2C_ADDRESS);
    printf("MMA8451 ID: %d\r\n", accel.getWhoAmI());
    
    // create the CCD array object
    TLS1410R ccd(PTE20, PTE21, PTB0);

    // process sensor reports and LedWiz requests forever
    int x = 0, y = 127, z = 0;
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
            if (data[0] == 64) {
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
                    ledOn[i] = ((data[ri] & bit) != 0);
                }
    
                // update the physical LED state
                updateLeds();
                
                // reset the PBA counter
                pbaIdx = 0;
            }
            else {
                // LWZ-PBA - full state dump; each byte is one output
                // in the current bank.  pbaIdx keeps track of the bank;
                // this is incremented implicitly by each PBA message.
                //printf("LWZ-PBA[%d] %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
                //       pbaIdx, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

                // update all output profile settings
                for (int i = 0 ; i < 8 ; ++i)
                    ledVal[pbaIdx + i] = data[i];

                // update the physical LED state if this is the last bank                    
                if (pbaIdx == 24)
                    updateLeds();

                // advance to the next bank
                pbaIdx = (pbaIdx + 8) & 31;
            }
        }
        
#if 1
        // check the accelerometer
        {
            // read the accelerometer
            float xa = accel.getAccX();
            float ya = accel.getAccY();
            
            // figure the new joystick position
            int xnew = (int)(127 * xa);
            int ynew = (int)(127 * ya);
            
            // send an update if the position has changed
            if (xnew != x || ynew != y)
            {
                x = xnew;
                y = ynew;

                // send the status report
                js.update(x, y, z, 0);
            }
        }
#else
        // Send a joystick report if it's been long enough since the
        // last report        
        if (timer.read_ms() - t0 > 250)
        {
            // send the current joystick status report
            js.update(x, y, z, 0);

            // update our internal joystick position record
            x += dx;
            y += dy;
            z += dz;
            if (x > xmax || x < xmin) {
                dx = -dx;
                x += 2*dx;
            }
            if (y > ymax || y < ymin) {
                dy = -dy;
                y += 2*dy;
            }
            if (z > zmax || z < zmin) {
                dz = -dz;
                z += 2*dz;
            }

            // note the time of the last report
            t0 = timer.read_ms();
        }            
#endif

        // pulse E29
        if (timer.read_ms() - tout1 > 2000)
        {
            out1 = !out1;
            tout1 = timer.read_ms();
        }
    }    
}
