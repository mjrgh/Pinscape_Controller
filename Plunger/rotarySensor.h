// Plunger sensor implementation for rotary absolute encoders
//
// This implements the plunger interfaces for rotary absolute encoders.  A
// rotary encoder measures the angle of a rotating shaft.  An absolute encoder
// is one where the microcontroller can ask the sensor for its current angular
// position at any time.  (As opposed to incremental encoders, which don't have
// any notion of their current position, but can only signal the host on each
// change in position.)
//
// 
// For plunger sensing, we can convert the plunger's linear motion into angular
// motion using a mechanical link between the plunger rod and a rotating shaft 
// positioned at a fixed point, somewhere nearby, but away from the plunger's 
// axis of motion:
//
//    =X=======================|===   <- plunger, X = connector attachment point
//      \
//       \                            <- connector between plunger and shaft
//        \
//         *                          <- rotating shaft, at a fixed position
//
// As the plunger moves, the angle of the connector relative to the fixed
// shaft position changes in a predictable way, so we can infer the plunger's
// linear position at any given time by measuring the current rotational
// angle of the shaft.
//
// The mechanical diagram above is, obviously, simplified for ASCII art's sake.
// What's not shown is that the distance between the rotating shaft and the
// "X" connection point on the plunger varies as the plunger moves, so the
// mechanical linkage requires some way to accommodate that changing length.
// If the connector is a rigid rod, it has to be able to slide at one or
// the other connection points.  Alternatively, rather than using a rigid
// linkage, we can use a spring or elastic band.  We leave these details up 
// to the mechanical design, since the software isn't affected by that, as 
// long as the basic relationship between linear and angular motion as shown
// in the diagram is achieved.
//
//
// Translating the angle to a linear position
//
// There are two complications to translating the angular reading back to
// a linear plunger position.
//
// 1. We have to consider the sensor's zero point to be arbitrary, because
// these sorts of sensors don't typically give the user a way to align the
// zero point at a desired physical position.  The zero point will just be
// wherever it ends up after installation.  The zero point could easily end 
// up being somewhere in the middle of the plunger's travel range, which
// means that readings might "wrap" - e.g., we might see a series of readings 
// when the plunger is moving in one direction like this: 4050, 4070, 4090, 
// 14, 34 (note how we "wrapped" past some maximum angle reading for the
// sensor and went back to zero, then continued from there).
//
// To deal with this, we have to make a couple of assumptions:
//
//   - The park position is at about 1/6 of the overall travel range
//   - The total angular travel range is less than one full revolution
//
// With those assumptions in hand, we can bias the raw readings to the
// park position, and then take them modulo the raw scale.  That will
// ensure that readings wrap properly, regardless of where the raw zero
// point lies.
//
// 2. Going back to the original diagram, you can see that there's some
// trigonometry required to interpret the sensor's angular reading as a
// linear position on the plunger axis, which is of course what we need
// to report to the PC software.
//
// Let's use the vertical line between the plunger and the rotation point
// as the zero-degree reference point.  To figure the plunger position, 
// we need to figure the difference between the raw angle reading and the
// zero-degree point; call this theta.  Let L be the position of the plunger
// relative to the vertical reference point, let D be the length of the 
// vertical reference point line, and let H by the distance from the rotation 
// point to the plunger connection point.  This is a right triangle with 
// hypotenuse H and sides L and D.  D is a constant, because the rotation 
// point never moves, and the plunger never moves vertically.  Thus we can
// calculate D = H*cos(theta) and L = H*sin(theta).  D is a constant, so
// we can figure H = D/cos(theta) hence L = D*sin(theta)/cos(theta) or
// D*tan(theta).  If we wanted to know the true position in real-world
// units, we'd have to know D, but only need arbitrary linear units, so
// we can choose whatever value for D we find convenient: in particular,
// a value that gives us the desired range and resolution for the final
// result.
//
// Note that the tangent diverges at +/-90 degrees, but that's okay,
// because the mechanical setup we've described is inherently constrained
// to stay well within those limits.  This would even be true for an 
// arbitrarily long range of motion along the travel axis, but we don't
// even have to worry about that since we have such a well-defined range
// of travel (of only about 3") to track.
//
// There's still one big piece missing here: we somehow have to know where
// that vertical zero point lies.  That's something we can only learn by
// calibration.  Unfortunately, we don't have a good way to detect this
// directly.  We *could* ask the user to look inside the cabinet and press
// a button when the needle is straight up, but that seems too cumbersome
// for the user, not to mention terribly imprecise.  So we'll approach this
// from the other direction: we'll assume a particular placement of the
// rotation point relative to the travel range, and we'll provide
// installation instructions to achieve that assumed alignment.
//
// The full range we actually have after calibration consists of the park
// position and the maximum retracted position.  We could in principle also
// calibrate the maximum forward position, but that can't be read as reliably
// as the other two, because the barrel spring makes it difficult for the 
// user to be sure they've pushed it all the way forward.  Since we can 
// extract the information we need from the park and max retract positions,
// it's better to rely on those alone and not ask for information that the
// user can't as easily provide.  Given these positions, AND the assumption
// that the rotation point is at the midpoint of the plunger travel range,
// we can do some grungy trig work to come up with a formula for the angle 
// between the park position and the vertical:
//
//    let C1 = 1 1/32" (distance from midpoint to park),
//        C2 = 1 17/32" (distance from midpoint to max retract),
//        C = C2/C1 = 1.48484849,
//        alpha = angle from park to vertical,
//        beta = angle from max retract to vertical
//        theta = alpha + beta = angle from park to max retract, known from calibration,
//        T = tan(theta);
//
//    then
//        alpha = atan(sqrt(4*T*T*C + C^2 + 2*C + 1) - C - 1)/(2*T*C))
//
// Did I mention this was grungy?  At any rate, everything going into that
// last equation is either constant or known from the calibration, so we 
// can pre-compute alpha and store it after each calibration operation.
// And once we've computed alpha, we can easily translate an angle reading 
// from the sensor to an angle relative to the vertical, which we can plug 
// into D*tan(angle) to convert to a linear position on the plunger axis.
//
// The final step is to scale that linear position into joystick reporting
// units.  Those units are arbitrary, so we don't have to relate this to any
// real-world lengths.  We can simply figure a scaling factor that maps the
// physical range to map to roughly the full range of the joystick units.
//
// If you're wondering how we derived that ugly formula, read on.  Start
// with the basic relationships D*tan(alpha) = C1 and D*tan(beta) = C2.
// This lets us write tan(beta) in terms of tan(alpha) as 
// C2/C1*tan(alpha) = C*tan(alpha).  We can combine this with an identity
// for the tan of a sum of angles:
//
//    tan(alpha + beta) = (tan(alpha) + tan(beta))/(1 - tan(alpha)*tan(beta))
//
// to obtain:
//
//    tan(theta) = tan(alpha + beta) = (1 + C*tan(alpha))/(1 - C*tan^2(alpha))
//
// Everything here except alpha is known, so we now have a quadratic equation
// for tan(alpha).  We can solve that by cranking through the normal algorithm
// for solving a quadratic equation, arriving at the solution above.
//
//
// Choosing an install position
//
// There are two competing factors in choosing the optimal "D".  On the one
// hand, you'd like D to be as large as possible, to maximum linearity of the
// tan function used to translate angle to linear position.  Higher linearity
// gives us greater immunity to variations in the precise centering of the
// rotation axis in the plunger travel range.  tan() is pretty linear (that
// is, tan(theta) is approximately proportional to theta) for small theta, 
// within about +/- 30 degrees.  On the other hand, you'd like D to be as 
// small as possible so that we get the largest overall angle range.  Our 
// sensor has a fixed angular resolution, so the more of the overall circle 
// we use, the more sensor increments we have over the range, and thus the 
// better effective linear resolution.
//
// Let's do some calculations for various "D" values (vertical distance 
// between rotation point and plunger rod).  We'll base our calculations
// on the AEAT-6012 sensor's 12-bit angular resolution.
//
//     D         theta(max)   eff dpi   theta(park)
//  -----------------------------------------------
//    1 17/32"    45 deg       341       34 deg
//    2"          37 deg       280       27 deg
//    2 21/32"    30 deg       228       21 deg
//    3 1/4"      25 deg       190       17 deg
//    4 3/16"     20 deg       152       14 deg
//
// I'd consider 50 dpi to be the minimum for acceptable performance, 100 dpi
// to be excellent, and anything above 300 dpi to be diminishing returns.  So
// for a 12-bit sensor, 2" looks like the sweet spot.  It doesn't take us far
// outside of the +/-30 deg zone of tan() linearity, and it achieves almost 
// 300 dpi of effective linear resolution.  I'd stop there are not try to
// push the angular resolution higher with a shorter D; with the 45 deg
// theta(max) at D = 1-17/32", we'd get a lovely DPI level of 341, but at
// the cost of getting pretty non-linear around the ends of the plunger
// travel.  Our math corrects for the non-linearity, but the more of that
// correction we need, the more sensitive the whole contraption becomes to
// getting the sensor positioning exactly right.  The closer we can stay to
// the linear approximation, the more tolerant we are of inexact sensor
// positioning.
//
//
// Supported sensors
//
//  * AEAT-6012-A06.  This is a magnetic absolute encoder with 12-bit
//    resolution.  It linearly encodes one full (360 degree) rotation in 
//    4096 increments, so each increment represents 360/4096 = .088 degrees.
//
// The base class doesn't actually care much about the sensor type; all it
// needs from the sensor is an angle reading represented on an arbitrary 
// linear scale.  ("Linear" in the angle, so that one increment represents
// a fixed number of degrees of arc.  The full scale can represent one full
// turn but doesn't have to, as long as the scale is linear over the range
// covered.)  To add new sensor types, you just need to add the code to
// interface to the physical sensor and return its reading on an arbitrary
// linear scale.

#ifndef _ROTARYSENSOR_H_
#define _ROTARYSENSOR_H_

#include "FastInterruptIn.h"
#include "AEAT6012.h"

// The conversion from raw sensor reading to linear position involves a
// bunch of translations to different scales and unit systems.  To help
// keep things straight, let's give each scale a name:
//
// * "Raw" refers to the readings directly from the sensor.  These are
//   unsigned ints in the range 0..maxRawAngle, and represent angles in a
//   unit system where one increment equals 360/maxRawAngle degrees.  The
//   zero point is arbitrary, determined by the physical orientation
//   of the sensor.
//
// * "Biased" refers to angular units with a zero point equal to the
//   park position.  This uses the same unit size as the "raw" system, but
//   the zero point is adjusted so that 0 always means the park position.
//   Negative values are forward of the park position.  This scale is
//   also adjusted for wrapping, by ensuring that the value lies in the
//   range -(maximum forward excursion) to +(scale max - max fwd excursion).
//   Any values below or above the range are bumped up or down (respectively)
//   to wrap them back into the range.
//
// * "Linear" refers to the final linear results, in joystick units, on
//   the abstract integer scale from 0..65535 used by the generic plunger
//   base class.
// 
class PlungerSensorRotary: public PlungerSensor
{
public:
    PlungerSensorRotary(int maxRawAngle, float radiansPerSensorUnit) : 
        PlungerSensor(65535),
        maxRawAngle(maxRawAngle),
        radiansPerSensorUnit(radiansPerSensorUnit)
    {   
        // start our sample timer with an arbitrary zero point of now
        timer.start();
        
        // clear the timing statistics
        nReads = 0;
        totalReadTime = 0;
        
        // Pre-calculate the maximum forward excursion distance, in raw
        // units.  For our reference mechanical setup with "D" in a likely
        // range, theta(max) is always about 10 degrees higher than
        // theta(park).  10 degrees is about 1/36 of the overall circle,
        // which is the same as 1/36 of the sensor scale.  To be 
        // conservative, allow for about 3X that, so allow 1/12 of scale
        // as the maximum forward excursion.  For wrapping purposes, we'll
        // consider any reading outside of the range from -(excursion)
        // to +(maxRawAngle - excursion) to be wrapped.
        maxForwardExcursionRaw = maxRawAngle/12;
                
        // reset the calibration counters
        biasedMinObserved = biasedMaxObserved = 0;
    }
    
    // Restore the saved calibration at startup
    virtual void restoreCalibration(Config &cfg)
    {
        // only proceed if there's calibration data to retrieve
        if (cfg.plunger.cal.calibrated)
        {
            // we store the raw park angle in raw0
            rawParkAngle = cfg.plunger.cal.raw0;
            
            // we store biased max angle in raw1
            biasedMax = cfg.plunger.cal.raw1;
        }
        else
        {
            // Use the current sensor reading as the initial guess at the
            // park position.  The system is usually powered up with the
            // plunger at the neutral position, so this is a good guess in
            // most cases.  If the plunger has been calibrated, we'll restore
            // the better guess when we restore the configuration later on in
            // the initialization process.
            rawParkAngle = 0;
            readSensor(rawParkAngle);

            // Set an initial wild guess at a range equal to +/-35 degrees.
            // Note that this is in the "biased" coordinate system - raw
            // units, but relative to the park angle.  The park angle is
            // about -25 degrees in this setup.
            biasedMax = (35 + 25) * maxRawAngle/360;        
        }
            
        // recalculate the vertical angle
        updateAlpha();
    }
    
    // Begin calibration
    virtual void beginCalibration(Config &)
    {
        // Calibration starts out with the plunger at the park position, so
        // we can take the current sensor reading to be the park position.
        rawParkAngle = 0;
        readSensor(rawParkAngle);
        
        // Reset the observed calibration counters
        biasedMinObserved = biasedMaxObserved = 0;
    }
    
    // End calibration
    virtual void endCalibration(Config &cfg)
    {
        // apply the observed maximum angle
        biasedMax = biasedMaxObserved;
        
        // recalculate the vertical angle
        updateAlpha();

        // save our raw configuration data
        cfg.plunger.cal.raw0 = static_cast<uint16_t>(rawParkAngle);
        cfg.plunger.cal.raw1 = static_cast<uint16_t>(biasedMax);
        
        // Refigure the range for the generic code
        cfg.plunger.cal.min = biasedAngleToLinear(biasedMinObserved);
        cfg.plunger.cal.max = biasedAngleToLinear(biasedMaxObserved);
        cfg.plunger.cal.zero = biasedAngleToLinear(0);
    }
    
    // figure the average scan time in microseconds
    virtual uint32_t getAvgScanTime() 
    { 
        return nReads == 0 ? 0 : static_cast<uint32_t>(totalReadTime / nReads);
    }
        
    // read the sensor
    virtual bool readRaw(PlungerReading &r)
    {
        // note the starting time for the reading
        uint32_t t0 = timer.read_us();
        
        // read the angular position
        int angle;
        if (!readSensor(angle))
            return false;

        // Refigure the angle relative to the raw park position.  This
        // is the "biased" angle.
        angle -= rawParkAngle;
        
        // Adjust for wrapping.
        //
        // An angular sensor reports the position on a circular scale, for
        // obvious reasons, so there's some point along the circle where the
        // angle is zero.  One tick before that point reads as the maximum
        // angle on the scale, so we say that the scale "wraps" at that point.
        //
        // To correct for this, we can look to the layout of the mechanical
        // setup to constrain the values.  Consider anything below the maximum
        // forward exclusion to be wrapped on the low side, and consider
        // anything outside of the complementary range on the high side to
        // be wrapped on the high side.
        if (angle < -maxForwardExcursionRaw)
            angle += maxRawAngle;
        else if (angle >= maxRawAngle - maxForwardExcursionRaw)
            angle -= maxRawAngle;
            
        // Note if this is the highest/lowest observed reading on the biased 
        // scale since the last calibration started.
        if (angle > biasedMaxObserved)
            biasedMaxObserved = angle;
        if (angle < biasedMinObserved)
            biasedMinObserved = angle;
            
        // figure the linear result
        r.pos = biasedAngleToLinear(angle);
                
        // Set the timestamp on the reading to right now
        uint32_t now = timer.read_us();
        r.t = now;
        
        // count the read statistics
        totalReadTime += now - t0;
        nReads += 1;        
        
        // success
        return true;
    }
    
private:
    // Read the underlying sensor - implemented by the hardware-specific
    // subclasses.  Returns true on success, false if the sensor can't
    // be read.  The angle is returned in raw sensor units.
    virtual bool readSensor(int &angle) = 0;

    // Convert a biased angle value to a linear reading
    int biasedAngleToLinear(int angle)
    {
        // Translate to an angle relative to the vertical, in sensor units
        float theta = static_cast<float>(angle)*radiansPerSensorUnit - alpha;
        
        // Calculate the linear position relative to the vertical.  Zero
        // is right at the intersection of the vertical line from the
        // sensor rotation center to the plunger axis; positive numbers
        // are behind the vertical (more retracted).
        int linearPos = static_cast<int>(tanf(theta) * linearScaleFactor);
        
        // Finally, figure the offset.  The vertical is the halfway point
        // of the plunger motion, so we want to put it at half of the raw
        // scale of 0..65535.
        return linearPos + 32767;
    }

    // Update the estimation of the vertical angle, based on the angle
    // between the park position and maximum retraction point.
    void updateAlpha()
    {
        // See the comments at the top of the file for details on this
        // formula.  This figures the angle between the park position
        // and the vertical by applying the known constraints of the
        // mechanical setup: the known length of a standard plunger,
        // and the requirement that the rotation axis be placed at
        // roughly the midpoint of the plunger travel.
        const float C = 1.4848489f; // 1-17/32" / 1-1/32"
        float maxInRadians = static_cast<float>(biasedMax) * radiansPerSensorUnit;
        float T = tanf(maxInRadians);
        alpha = atanf((sqrtf(4*T*T*C + C*C + 2*C + 1) - C - 1)/(2*T*C));

        // While we're at it, figure the linear conversion factor.  Alpha
        // represents the angle from the park position to the midpoint,
        // which in the real world represents about 31/32", or just less
        // then 1/3 of the overall travel.  We want to normalize this to
        // the corresponding fraction of our 0..65535 abstract linear unit
        // system.  To avoid overflow, normalize to a slightly smaller
        // scale.
        const float safeMax = 60000.0f;
        const float alphaInLinearUnits = safeMax * .316327f; // 31/22" / 3-1/16"
        linearScaleFactor = static_cast<int>(alphaInLinearUnits / tanf(alpha));
    }

    // Maximum raw angular reading from the sensor.  The sensor's readings
    // will always be on a scale from 0..maxRawAngle.
    int maxRawAngle;
    
    // Radians per sensor unit.  This is a constant for the sensor.
    float radiansPerSensorUnit;
    
    // Pre-calculated value of the maximum forward excursion, in raw units.
    int maxForwardExcursionRaw;
    
    // Raw reading at the park position.  We use this to handle "wrapping",
    // if the sensor's raw zero reading position is within the plunger travel
    // range.  All readings are taken to be within 
    int rawParkAngle;
    
    // Biased maximum angle.  This is the angle at the maximum retracted
    // position, in biased units (sensor units, relative to the park angle).
    int biasedMax;
    
    // Mininum and maximum angle observed since last calibration start, on 
    // the biased scale
    int biasedMinObserved;
    int biasedMaxObserved;
    
    // The "alpha" angle - the angle between the park position and the
    // vertical line between the rotation axis and the plunger.  This is
    // represented in radians.
    float alpha;
    
    // The linear scaling factor, applied in our trig calculation from
    // angle to linear position.  This corresponds to the distance from
    // the rotation center to the plunger rod, but since the linear result
    // is in abstract joystick units, this distance is likewise in abstract
    // units.  The value isn't chosen to correspond to any real-world 
    // distance units, but rather to yield a joystick result that takes
    // advantage of most of the available axis range, to minimize rounding
    // errors when converting between scales.
    float linearScaleFactor;

    // timer for input timestamps and read timing measurements
    Timer timer;
    
    // read timing statistics
    uint64_t totalReadTime;
    uint64_t nReads;
    
    // Keep track of when calibration is in progress.  The calibration
    // procedure is usually handled by the generic main loop code, but
    // in this case, we have to keep track of some of the raw sensor
    // data during calibration for our own internal purposes.
    bool calibrating;
};

// Specialization for the AEAT-601X sensors
template<int nDataBits> class PlungerSensorAEAT601X : public PlungerSensorRotary
{
public:
    PlungerSensorAEAT601X(PinName csPin, PinName clkPin, PinName doPin) :
        PlungerSensorRotary((1 << nDataBits) - 1, 6.283185f/((1 << nDataBits) - 1)),
        aeat(csPin, clkPin, doPin) 
    {
        // Make sure the sensor has had time to finish initializing.
        // Power-up time (tCF) from the data sheet is 20ms for the 12-bit
        // version, 50ms for the 10-bit version.
        wait_ms(nDataBits == 12 ? 20 :
            nDataBits == 10 ? 50 :
            50);
    }

    // read the angle
    virtual bool readSensor(int &angle)
    {
        angle = aeat.readAngle();
        return true;
    }
        
protected:
    // physical sensor interface
    AEAT601X<nDataBits> aeat;
};

#endif
