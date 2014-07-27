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

//
// Pinscape Controller
//
// "Pinscape" is the name of my custom-built virtual pinball cabinet.  I wrote this
// software to perform a number of tasks that I needed for my cabinet.  It runs on a
// Freescale KL25Z microcontroller, which is a small and inexpensive device that
// attaches to the host PC via USB and can interface with numerous types of external
// hardware.
//
// I designed the software and hardware in this project especially for Pinscape, but 
// it uses standard interfaces in Windows and Visual Pinball, so it should be
// readily usable in anyone else's VP-based cabinet.  I've tried to document the
// hardware in enough detail for anyone else to duplicate the entire project, and
// the full software is open source.
//
// The controller provides the following functions.  It should be possible to use
// any subet of the features without using all of them.  External hardware for any
// particular function can simply be omitted if that feature isn't needed.
//
//  - Nudge sensing via the KL25Z's on-board accelerometer.  Nudge accelerations are
//    processed into a physics model of a rolling ball, and changes to the ball's
//    motion are sent to the host computer via the joystick interface.  This is designed
//    especially to work with Visuall Pinball's nudge handling to produce realistic 
//    on-screen results in VP.  By doing some physics modeling right on the device, 
//    rather than sending raw accelerometer data to VP, we can produce better results
//    using our awareness of the real physical parameters of a pinball cabinet.
//    VP's nudge handling has to be more generic, so it can't make the same sorts
//    of assumptions that we can about the dynamics of a real cabinet.
//
//    The nudge data reports are compatible with the built-in Windows USB joystick 
//    drivers and with VP's own joystick input scheme, so the nudge sensing is almost 
//    plug-and-play.  There are no Windiows drivers to install, and the only VP work 
//    needed is to customize a few global preference settings.
//
//  - Plunger position sensing via an attached TAOS TSL 1410R CCD linear array sensor.  
//    The sensor must be wired to a particular set of I/O ports on the KL25Z, and must 
//    be positioned adjacent to the plunger with proper lighting.  The physical and
//    electronic installation details are desribed in the project documentation.  We read 
//    the CCD to determine how far back the plunger is pulled, and report this to Visual 
//    Pinball via the joystick interface.  As with the nudge data, this is all nearly
//    plug-and-play, in that it works with the default Windows USB drivers and works 
//    with the existing VP handling for analog plunger input.  A few VP settings are
//    needed to tell VP to allow the plunger.
//
//    Unfortunately, analog plungers are not well supported by individual tables,
//    so some work is required for each table to give it proper support.  I've tried
//    to reduce this to a recipe and document it in the project documentation.
//
//  - In addition to the CCD sensor, a button should be attached (also described in 
//    the project documentation) to activate calibration mode for the plunger.  When 
//    calibration mode is activated, the software reads the plunger position for about 
//    10 seconds when to note the limits of travel, and uses these limits to ensure
//    accurate reports to VP that properly report the actual position of the physical
//    plunger.  The calibration is stored in non-volatile memory on the KL25Z, so it's
//    only necessary to calibrate once - the calibration will survive power cycling
//    and reboots of the PC.  It's only necessary to recalibrate if the CCD sensor or
//    the plunger are removed and reinstalled, since the relative alignment of the
//    parts could cahnge slightly when reinstalling.
//
//  - LedWiz emulation.  The KL25Z can appear to the PC as an LedWiz device, and will
//    accept and process LedWiz commands from the host.  The software can turn digital
//    output ports on and off, and can set varying PWM intensitiy levels on a subset
//    of ports.  (The KL25Z can only provide 6 PWM ports.  Intensity level settings on
//    other ports is ignored, so non-PWM ports can only be used for simple on/off
//    devices such as contactors and solenoids.)  The KL25Z can only supply 4mA on its
//    output ports, so external hardware is required to take advantage of the LedWiz
//    emulation.  Many different hardware designs are possible, but there's a simple
//    reference design in the documentation that uses a Darlington array IC to
//    increase the output from each port to 500mA (the same level as the LedWiz),
//    plus an extended design that adds an optocoupler and MOSFET to provide very
//    high power handling, up to about 45A or 150W, with voltages up to 100V.
//    That will handle just about any DC device directly (wtihout relays or other
//    amplifiers), and switches fast enough to support PWM devices.
//
//    The device can report any desired LedWiz unit number to the host, which makes
//    it possible to use the LedWiz emulation on a machine that also has one or more
//    actual LedWiz devices intalled.  The LedWiz design allows for up to 16 units
//    to be installed in one machine - each one is invidually addressable by its
//    distinct unit number.
//
//    The LedWiz emulation features are of course optional.  There's no need to 
//    build any of the external port hardware (or attach anything to the output 
//    ports at all) if the LedWiz features aren't needed.  Most people won't have
//    any use for the LedWiz features.  I built them mostly as a learning exercise,
//    but with a slight practical need for a handful of extra ports (I'm using the
//    cutting-edge 10-contactor setup, so my real LedWiz is full!).


#include "mbed.h"
#include "USBJoystick.h"
#include "MMA8451Q.h"
#include "tsl1410r.h"
#include "FreescaleIAP.h"
#include "crc32.h"


// ---------------------------------------------------------------------------
//
// Configuration details
//

// Our USB device vendor ID, product ID, and version.  
// We use the vendor ID for the LedWiz, so that the PC-side software can
// identify us as capable of performing LedWiz commands.  The LedWiz uses
// a product ID value from 0xF0 to 0xFF; the last four bits identify the
// unit number (e.g., product ID 0xF7 means unit #7).  This allows multiple
// LedWiz units to be installed in a single PC; the software on the PC side
// uses the unit number to route commands to the devices attached to each
// unit.  On the real LedWiz, the unit number must be set in the firmware
// at the factory; it's not configurable by the end user.  Most LedWiz's
// ship with the unit number set to 0, but the vendor will set different
// unit numbers if requested at the time of purchase.  So if you have a
// single LedWiz already installed in your cabinet, and you didn't ask for
// a non-default unit number, your existing LedWiz will be unit 0.
//
// We use unit #7 by default.  There doesn't seem to be a requirement that
// unit numbers be contiguous (DirectOutput Framework and other software
// seem happy to have units 0 and 7 installed, without 1-6 existing).
// Marking this unit as #7 should work for almost everybody out of the box;
// the most common case seems to be to have a single LedWiz installed, and
// it's probably extremely rare to more than two.
const uint16_t USB_VENDOR_ID = 0xFAFA;
const uint16_t USB_PRODUCT_ID = 0x00F7;
const uint16_t USB_VERSION_NO = 0x0004;

// On-board RGB LED elements - we use these for diagnostic displays.
DigitalOut ledR(LED1), ledG(LED2), ledB(LED3);

// calibration button - switch input and LED output
DigitalIn calBtn(PTE29);
DigitalOut calBtnLed(PTE23);

// I2C address of the accelerometer (this is a constant of the KL25Z)
const int MMA8451_I2C_ADDRESS = (0x1d<<1);

// SCL and SDA pins for the accelerometer (constant for the KL25Z)
#define MMA8451_SCL_PIN   PTE25
#define MMA8451_SDA_PIN   PTE24

// Digital in pin to use for the accelerometer interrupt.  For the KL25Z,
// this can be either PTA14 or PTA15, since those are the pins physically
// wired on this board to the MMA8451 interrupt controller.
#define MMA8451_INT_PIN   PTA15


// ---------------------------------------------------------------------------
//
// LedWiz emulation
//

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

// ---------------------------------------------------------------------------
//
// Non-volatile memory (NVM)
//

// Structure defining our NVM storage layout.  We store a small
// amount of persistent data in flash memory to retain calibration
// data when powered off.
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


// ---------------------------------------------------------------------------
//
// Customization joystick subbclass
//

class MyUSBJoystick: public USBJoystick
{
public:
    MyUSBJoystick(uint16_t vendor_id, uint16_t product_id, uint16_t product_release) 
        : USBJoystick(vendor_id, product_id, product_release, true)
    {
        suspended_ = false;
    }
    
    // are we connected?
    int isConnected()  { return configured(); }
    
    // Are we in suspend mode?
    int isSuspended() const { return suspended_; }
    
protected:
    virtual void suspendStateChanged(unsigned int suspended)
        { suspended_ = suspended; }

    // are we suspended?
    int suspended_; 
};

// ---------------------------------------------------------------------------
// 
// Accelerometer (MMA8451Q)
//

// The MMA8451Q is the KL25Z's on-board 3-axis accelerometer.
//
// This is a custom wrapper for the library code to interface to the
// MMA8451Q.  This class encapsulates an interrupt handler and some
// special data processing to produce more realistic results in
// Visual Pinball.
//
// We install an interrupt handler on the accelerometer "data ready" 
// interrupt in order to ensure that we fetch each sample immediately
// when it becomes available.  Since our main program loop is busy
// reading the CCD virtually all of the time, it wouldn't be practical
// to keep up with the accelerometer data stream by polling.
//
// Visual Pinball is nominally designed to accept raw accelerometer
// data as nudge input, but in practice, this doesn't produce
// very realistic results.  VP simply applies accelerations from a
// physical accelerometer directly to its modeled ball(s), but the
// data stream coming from a real accelerometer isn't as clean as
// an idealized physics simulation.  The problem seems to be that the
// accelerometer samples capture instantaneous accelerations, not
// integrated acceleration over time.  In other words, adding samples 
// over time doesn't accurately reflect the actual net acceleration
// experienced.  The longer the sampling period, the greater the
// divergence between the sum of a series of samples and the actual
// net acceleration.  The effect in VP is to leave the ball with
// an unrealistically high residual velocity over the course of a
// nudge event.
//
// This is where our custom data processing comes into play.  Rather
// than sending raw accelerometer samples, we apply the samples to
// our own virtual model ball.  What we send VP is the accelerations
// experienced by the ball in our model, not the actual accelerations
// we read from the MMA8451Q.  Now, that might seem like an unnecessary
// middleman, because VP is just going to apply the accelerations to
// its own model ball.  But it's a useful middleman: what we can do
// in our model that VP can't do in its model is take into account
// our special knowledge of the physical cabinet configuration.  VP
// has to work generically with any sort of nudge input device, but
// we can make assumptions about what kind of physical environment
// we're operating in.
//
// The key assumption we make about our physical environment is that
// accelerations from nudges should net out to zero over intervals on
// the order of a couple of seconds.  Nudging a pinball cabinet makes
// the cabinet accelerate briefly in the nudge direction, then rebound,
// then re-rebound, and so on until the swaying motion damps out and
// the table returns roughly to rest.  The table doesn't actually go
// anywhere in these transactions, so the net acceleration experienced
// is zero by the time the motion has damped out.  The damping time
// depends on the degree of force of the nudge, but is a second or
// two in most cases.
//
// We can't just assume that all motion and/or acceleration must stop 
// in a second or two, though.  For one thing, the player can nudge
// the table repeatedly for long periods.  (Doing this too aggressivly
// will trigger a tilt, so there are limits, but a skillful player
// can keep nudging a table almost continuously without tilting it.)
// For another, a player could actually pick up one end of the table
// for an extended period, applying a continuous acceleration the
// whole time.
//
// The strategy we use to cope with these possibilities is to model a
// ball, rather like VP does, but with damping that scales with the
// current speed.  We'll choose a damping function that will bring
// the ball to rest from any reasonable speed within a second or two
// if there are no ongoing accelerations.  The damping function must
// also be weak enough that new accelerations dominate - that is,
// the damping function must not be so strong that it cancels out
// ongoing physical acceleration input, such as when the player
// lifts one end of the table and holds it up for a while.
//
// What we report to VP is the acceleration experienced by our model
// ball between samples.  Our model ball starts at rest, and our damping
// function ensures that when it's in motion, it will return to rest in
// a short time in the absence of further physical accelerations.  The
// sum or our reports to VP from a rest state to a subsequent rest state
// will thus necessarily equal exactly zero.  This will ensure that we 
// don't leave VP's model ball with any residual velocity after an 
// isolated nudge.
//
// We do one more bit of data processing: automatic calibration.  When
// we observe the accelerometer input staying constant (within a noise
// window) for a few seconds continously, we'll assume that the cabinet
// is at rest.  It's safe to assume that the accelerometer isn't
// installed in such a way that it's perfectly level, so at the
// cabinet's neutral rest position, we can expect to read non-zero
// accelerations on the x and y axes from the component along that
// axis of the Earth's gravity.  By watching for constant acceleration
// values over time, we can infer the reseting position of the device
// and take that as our zero point.  By doing this continuously, we
// don't have to assume that the machine is perfectly motionless when
// initially powered on - we'll organically find the zero point as soon
// as the machine is undisturbed for a few moments.  We'll also deal
// gracefully with situations where the machine is jolted so much in
// the course of play that its position is changed slightly.  The result
// should be to make the zeroing process reliable and completely 
// transparent to the user.
//

// point structure
struct FPoint
{
    float x, y;
    
    FPoint() { }
    FPoint(float x, float y) { this->x = x; this->y = y; }
    
    void set(float x, float y) { this->x = x; this->y = y; }
    void zero() { this->x = this->y = 0; }
    
    FPoint &operator=(FPoint &pt) { this->x = pt.x; this->y = pt.y; return *this; }
    FPoint &operator-=(FPoint &pt) { this->x -= pt.x; this->y -= pt.y; return *this; }
    FPoint &operator+=(FPoint &pt) { this->x += pt.x; this->y += pt.y; return *this; }
    FPoint &operator*=(float f) { this->x *= f; this->y *= f; return *this; }
    FPoint &operator/=(float f) { this->x /= f; this->y /= f; return *this; }
    float magnitude() const { return sqrt(x*x + y*y); }
    
    float distance(FPoint &b)
    {
        float dx = x - b.x;
        float dy = y - b.y;
        return sqrt(dx*dx + dy*dy);
    }
};


// accelerometer wrapper class
class Accel
{
public:
    Accel(PinName sda, PinName scl, int i2cAddr, PinName irqPin)
        : mma_(sda, scl, i2cAddr), intIn_(irqPin)
    {
        // remember the interrupt pin assignment
        irqPin_ = irqPin;

        // reset and initialize
        reset();
    }
    
    void reset()
    {
        // assume initially that the device is perfectly level
        center_.zero();
        tCenter_.start();
        iAccPrv_ = nAccPrv_ = 0;

        // reset and initialize the MMA8451Q
        mma_.init();
        
        // set the initial ball velocity to zero
        v_.zero();
        
        // set the initial raw acceleration reading to zero
        araw_.zero();
        vsum_.zero();

        // enable the interrupt
        mma_.setInterruptMode(irqPin_ == PTA14 ? 1 : 2);
        
        // set up the interrupt handler
        intIn_.rise(this, &Accel::isr);
        
        // read the current registers to clear the data ready flag
        float z;
        mma_.getAccXYZ(araw_.x, araw_.y, z);

        // start our timers
        tGet_.start();
        tInt_.start();
        tRest_.start();
    }
    
    void get(float &x, float &y, float &rx, float &ry) 
    {
         // disable interrupts while manipulating the shared data
         __disable_irq();
         
         // read the shared data and store locally for calculations
         FPoint vsum = vsum_, araw = araw_;
         
         // reset the velocity sum
         vsum_.zero();

         // get the time since the last get() sample
         float dt = tGet_.read_us()/1.0e6;
         tGet_.reset();
         
         // done manipulating the shared data
         __enable_irq();
         
         // check for auto-centering every so often
         if (tCenter_.read_ms() > 1000)
         {
             // add the latest raw sample to the history list
             accPrv_[iAccPrv_] = araw_;
             
             // commit the history entry
             iAccPrv_ = (iAccPrv_ + 1) % maxAccPrv;

             // if we have a full complement, check for stability
             if (nAccPrv_ >= maxAccPrv)
             {
                 // check if we've been stable for all recent samples
                 static const float accTol = .005;
                 if (accPrv_[0].distance(accPrv_[1]) < accTol
                     && accPrv_[0].distance(accPrv_[2]) < accTol
                     && accPrv_[0].distance(accPrv_[3]) < accTol
                     && accPrv_[0].distance(accPrv_[4]) < accTol)
                 {
                     // figure the new center as the average of these samples
                     center_.set(
                        (accPrv_[0].x + accPrv_[1].x + accPrv_[2].x + accPrv_[3].x + accPrv_[4].x)/5.0,
                        (accPrv_[0].y + accPrv_[1].y + accPrv_[2].y + accPrv_[3].y + accPrv_[4].y)/5.0);
                 }
             }
             else
             {
                // not enough samples yet; just up the count
                ++nAccPrv_;
             }
            
             // reset the timer
             tCenter_.reset();
         }

         // Calculate the velocity vector for the model ball.  Start
         // with the accumulated velocity from the accelerations since
         // the last reading.
         FPoint dv = vsum;

         // remember the previous velocity of the model ball
         FPoint vprv = v_;
         
         // If we have residual motion, check for damping.
         //
         // The dmaping we model here isn't friction - we leave that sort of
         // detail to the pinball simulator on the PC.  Instead, our form of
         // damping is just an attempt to compensate for measurement errors
         // from the accelerometer.  During a nudge event, we should see a
         // series of accelerations back and forth, as the table sways in
         // response to the push, rebounds from the sway, rebounds from the
         // rebound, etc.  We know that in reality, the table itself doesn't
         // actually go anywhere - it just sways, and when the swaying stops,
         // it ends up where it started.  If we use the accelerometer input
         // to do dead reckoning on the location of the table, we know that
         // it has to end up where it started.  This means that the series of
         // position changes over the course of the event should cancel out -
         // the displacements should add up to zero.  
         
          to model friction and other forces
         // on the ball.  Instead, the damping we apply is to compensate for
         // measurement errors in the accelerometer.  During a nudge event,
         // a real pinball cabinet typically ends up at the same place it
         // started - it sways in response to the nudge, but the swaying
         // quickly damps out and leaves the table unmoved.  You don't
         // typically apply enough force to actually pick up the cabinet
         // and move it, or slide it across the floor - and doing so would
         // trigger a tilt, in which case the ball goes out of play and we
         // don't really have to worry about how realistically it behaves
         // in response to the acceleration.
         if (vprv.magnitude() != 0)
         {
             // The model ball is moving.  If the current motion has been
             // going on for long enough, apply damping.  We wait a short
             // time before we apply damping to allow small continuous
             // accelerations (from tiling the table) to get the ball
             // rolling.
             if (tRest_.read_ms() > 100)
             {
             }
         }
         else
         {
             // the model ball is at rest; if the instantaneous acceleration
             // is also near zero, reset the rest timer
             if (dv.magnitude() < 0.025)
                 tRest_.reset();
         }
         
         // If the current velocity change is near zero, damp the ball's
         // velocity.  The idea is that the total series of accelerations 
         // from a nudge should net to zero, since a nudge doesn't
         // actually move the table anywhere.  
         // 
         // Ideally, this wouldn't be necessary, because the raw
         // accelerometer readings should organically add up to zero over
         // the course of a nudge.  In practice, the accelerometer isn't
         // perfect; it can only sample so fast, so it can't capture every
         // instantaneous change; and each reading has some small measurement
         // error, which becomes significant when many readings are added
         // together.  The damping is an attempt to reconcile the imperfect
         // measurements with what how expect the real physical system to
         // behave - we know what the outcome of an event should be, so we
         // adjust our measurements to get the expected outcome.
         //
         // If the ball's velocity is large at this point, assume that this
         // wasn't a nudge event at all, but a sustained inclination - as
         // though the player picked up one end of the table and held it
         // up for a while, to accelerate the ball down the sloped table.
         // In this case just reset the velocity to zero without doing
         // any damping, so that we don't pass through any deceleration
         // to the pinball simulation.  In this case we want to leave it
         // to the pinball simulation to do its own modeling of friction
         // or bouncing to decelerate the ball.  Our correction is only
         // realistic for brief events that naturally net out to neutral
         // accelerations.
         if (dv.magnitude() < .025)
         {
            // check the ball's speed
            if (v_.magnitude() < .25)
            {
                // apply the damping
                FPoint damp(damping(v_.x), damping(v_.y));
                dv -= damp;
                ledB = 0;
            }
            else
            {
                // the ball is going too fast - simply reset it
                v_ = dv;
                vprv = dv;
                ledB = 1;
            }
         }
         else
             ledB = 1;
         
         // apply the velocity change for this interval
         v_ += dv;
         
         // return the acceleration since the last update (change in velocity
         // over time) in x,y
         dv /= dt;
         x = (v_.x - vprv.x) / dt;
         y = (v_.y - vprv.y) / dt;
         
         // report the calibrated instantaneous acceleration in rx,ry
         rx = araw.x - center_.x;
         ry = araw.y - center_.y;
     }    
    
private:
    // velocity damping function
    float damping(float v)
    {
        // scale to -2048..2048 range, and get the absolute value
        float a = fabs(v*2048.0);
        
        // damp out small velocities immediately
        if (a < 20)
            return v;
        
        // calculate the cube root of the scaled value
        float r = exp(log(a)/3.0);
        
        // rescale
        r /= 2048.0;
        
        // apply the sign and return the result
        return (v < 0 ? -r : r);
    }

    // interrupt handler
    void isr()
    {
        // Read the axes.  Note that we have to read all three axes
        // (even though we only really use x and y) in order to clear
        // the "data ready" status bit in the accelerometer.  The
        // interrupt only occurs when the "ready" bit transitions from
        // off to on, so we have to make sure it's off.
        float x, y, z;
        mma_.getAccXYZ(x, y, z);

        // store the raw results
        araw_.set(x, y);
        zraw_ = z;
        
        // calculate the time since the last interrupt
        float dt = tInt_.read_us()/1.0e6;
        tInt_.reset();
        
        // Add the velocity to the running total.  First, calibrate the
        // raw acceleration to our centerpoint, then multiply by the time
        // since the last sample to get the velocity resulting from
        // applying this acceleration for the sample time.
        FPoint rdt((x - center_.x)*dt, (y - center_.y)*dt);
        vsum_ += rdt;
    }
    
    // underlying accelerometer object
    MMA8451Q mma_;
    
    // last raw acceleration readings
    FPoint araw_;
    float zraw_;
    
    // total velocity change since the last get() sample
    FPoint vsum_;
    
    // current modeled ball velocity
    FPoint v_;
    
    // timer for measuring time between get() samples
    Timer tGet_;
    
    // timer for measuring time between interrupts
    Timer tInt_;
    
    // time since last rest
    Timer tRest_;

    // calibrated center point - this is the position where we observe
    // constant input for a few seconds, telling us the orientation of
    // the accelerometer device when at rest
    FPoint center_;

    // timer for atuo-centering
    Timer tCenter_;
    
    // recent accelerometer readings, for auto centering
    int iAccPrv_, nAccPrv_;
    static const int maxAccPrv = 5;
    FPoint accPrv_[maxAccPrv];

    // interurupt pin name
    PinName irqPin_;
    
    // interrupt router
    InterruptIn intIn_;
};


// ---------------------------------------------------------------------------
//
// Clear the I2C bus for the MMA8451!.  This seems necessary some of the time
// for reasons that aren't clear to me.  Doing a hard power cycle has the same
// effect, but when we do a soft reset, the hardware sometimes seems to leave
// the MMA's SDA line stuck low.  Forcing a series of 9 clock pulses through
// the SCL line is supposed to clear this conidtion.
//
void clear_i2c()
{
    // assume a general-purpose output pin to the I2C clock
    DigitalOut scl(MMA8451_SCL_PIN);
    DigitalIn sda(MMA8451_SDA_PIN);
    
    // clock the SCL 9 times
    for (int i = 0 ; i < 9 ; ++i)
    {
        scl = 1;
        wait_us(20);
        scl = 0;
        wait_us(20);
    }
}

// ---------------------------------------------------------------------------
//
// Main program loop.  This is invoked on startup and runs forever.  Our
// main work is to read our devices (the accelerometer and the CCD), process
// the readings into nudge and plunger position data, and send the results
// to the host computer via the USB joystick interface.  We also monitor
// the USB connection for incoming LedWiz commands and process those into
// port outputs.
//
int main(void)
{
    // turn off our on-board indicator LED
    ledR = 1;
    ledG = 1;
    ledB = 1;
    
    // clear the I2C bus for the accelerometer
    clear_i2c();
    
    // Create the joystick USB client
    MyUSBJoystick js(USB_VENDOR_ID, USB_PRODUCT_ID, USB_VERSION_NO);

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
    // we'll read every 4th pixel.  It takes time to read each pixel, so the
    // fewer pixels we read, the higher the refresh rate we can achieve.
    // It's therefore better not to read more pixels than we have to.
    //
    // VP seems to have an internal resolution in the 8-bit range, so there's
    // no apparent benefit to reading more than 128-256 pixels when using VP.
    // Empirically, 160 pixels seems about right.  The overall travel of a
    // standard pinball plunger is about 3", so 160 pixels gives us resolution
    // of about 1/50".  This seems to take full advantage of VP's modeling
    // ability, and is probably also more precise than a human player's
    // perception of the plunger position.
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
    int hb = 0;
    uint16_t hbcnt = 0;
    
    // set a timer for accelerometer auto-centering
    Timer acTimer;
    acTimer.start();
    
    // create the accelerometer object
    Accel accel(MMA8451_SCL_PIN, MMA8451_SDA_PIN, MMA8451_I2C_ADDRESS, MMA8451_INT_PIN);
    
    // create the CCD array object
    TSL1410R ccd(PTE20, PTE21, PTB0);
    
    // last accelerometer report, in mouse coordinates
    int x = 127, y = 127, z = 0;

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
        
        // confine the accelerometer results to the unit interval
        if (xa < -1.0) xa = -1.0;
        if (xa > 1.0) xa = 1.0;
        if (ya < -1.0) ya = -1.0;
        if (ya > 1.0) ya = 1.0;

        // scale to our -127..127 reporting range
        int xnew = int(127 * xa);
        int ynew = int(127 * ya);

        // store the updated joystick coordinates
        x = xnew;
        y = ynew;
        z = znew;
        
        // Send the status report.  It doesn't really matter what
        // coordinate system we use, since Visual Pinball has config
        // options for rotations and axis reversals, but reversing y
        // at the device level seems to produce the most intuitive 
        // results for the Windows joystick control panel view, which
        // is an easy way to check that the device is working.
        //
        // $$$ button updates are for diagnostics, so we can see that the
        // device is sending data properly if the accelerometer gets stuck
        js.update(x, -y, z, int(rxa*127), int(rya*127), hb ? 0x5500 : 0xAA00);
        
        // show a heartbeat flash in blue every so often if not in 
        // calibration mode
        if (calBtnState < 2 && hbTimer.read_ms() > 1000) 
        {
            if (js.isSuspended() || !js.isConnected())
            {
                // suspended - turn off the LED
                ledR = 1;
                ledG = 1;
                ledB = 1;

                // show a status flash every so often                
                if (hbcnt % 3 == 0)
                {
                    // disconnected = red flash; suspended = red-red
                    for (int n = js.isConnected() ? 1 : 2 ; n > 0 ; --n)
                    {
                        ledR = 0;
                        wait(0.05);
                        ledR = 1;
                        wait(0.25);
                    }
                }
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
                //ledR = (hb ? 0 : 1);
                //ledG = 0;
                ledB = 1;
            }
            
            // reset the heartbeat timer
            hbTimer.reset();
            ++hbcnt;
        }
    }
}
