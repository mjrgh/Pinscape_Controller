// Define the configuration variable USB get/set mapper.  We use
// macros for the get/set operations to allow for common source
// code for the two operations.  main.cpp #includes this file twice:
// once for the SET function and once for the GET function.  main.cpp
// redefines the v_xxx macros according to the current inclusion mode.
//
// This is a little tricky to follow because of the macros, but the
// benefit is that the get and set functions automatically stay in
// sync in terms of the variable types and byte mappings in the USB
// messages, since they're both generated automatically from the
// same code.
//
// The SET function is called directly from the corresponding USB
// protocol message to set one variable.  The data buffer is simply
// the data passed in from the USB message.
//
// The GET function is called in a loop from our configuration
// variable reporting function.  The report function loops through
// each variable in turn to generate a series of reports.  The
// caller in this case fills in data[1] with the variable ID, and
// it also fills in data[2] with the current index being queried
// for the array variables (buttons, outputs).  We fill in the
// rest of the data[] bytes with the current variable value(s),
// encoded for the USB protocol message.


void v_func
{
    switch (data[1])
    {
        // ********** UNRECOGNIZED VARIABLE IDs **********
        // For any variable ID we don't recognize, we'll ignore SET
        // requests and return all zeroes on QUERY requests.  This
        // provides sensible default behavior if a newer version of 
        // the config tool is used with an older version of the 
        // firwmare.  Because of the default all-zero query response,
        // new variable added over time should use zero values as
        // the standard defaults whenever possible.  Note that the
        // config tool can also use QUERY VARIABLE 0 to determine
        // the number of variables supported by the firmware it's
        // talking to, if it needs to know whether or not a 
        // particular variable exists (a variable exists if its ID
        // is within the range returned by the QUERY 0 call).
        // 
    default:
        break;
        
        
        // ********** DESCRIBE CONFIGURATION VARIABLES **********
    case 0:
        v_byte_ro(22, 2);       // number of SCALAR variables
        v_byte_ro(6, 3);        // number of ARRAY variables
        break;
        
        // ********** SCALAR VARIABLES **********
        
    case 1:
        // USB identification (Vendor ID, Product ID)
        v_ui16(usbVendorID, 2);
        v_ui16(usbProductID, 4);
        break;
        
    case 2:
        // Pinscape Controller unit number (nominal unit number, 1-16)
        if_msg_valid(data[2] >= 1 && data[2] <= 16)
            v_byte(psUnitNo, 2);
        break;
        
    case 3:
        // Joystick report settings
        v_byte(joystickEnabled, 2);
        v_byte(joystickAxisFormat, 3);
        v_ui32(jsReportInterval_us, 4);
        
#if VAR_MODE_SET
        // apply a default if the report interval is zero
        if (cfg.jsReportInterval_us == 0)
            cfg.jsReportInterval_us = 8333;
#endif
        break;
        
    case 4:
        // Accelerometer options
        v_byte(accel.orientation, 2);
        v_byte(accel.range, 3);
        v_byte(accel.autoCenterTime, 4);
        v_byte(accel.stutter, 5);
        break;

    case 5:
        // Plunger sensor type
        v_byte(plunger.sensorType, 2);
        v_byte(plunger.param1, 3);
        break;
        
    case 6:
        // Plunger sensor pin assignments
        v_byte(plunger.sensorPin[0], 2);
        v_byte(plunger.sensorPin[1], 3);
        v_byte(plunger.sensorPin[2], 4);
        v_byte(plunger.sensorPin[3], 5);
        break;
        
    case 7:
        // Plunger calibration button and indicator light pin assignments
        v_byte(plunger.cal.features, 2);
        v_byte(plunger.cal.btn, 3);
        v_byte(plunger.cal.led, 4);
        break;
        
    case 8:
        // ZB Launch Ball setup
        v_byte(plunger.zbLaunchBall.port, 2);
        v_byte(plunger.zbLaunchBall.keytype, 3);
        v_byte(plunger.zbLaunchBall.keycode, 4);
        v_ui16(plunger.zbLaunchBall.pushDistance, 5);
        break;
        
    case 9:
        // TV ON setup
        v_byte(TVON.statusPin, 2);
        v_byte(TVON.latchPin, 3);
        v_byte(TVON.relayPin, 4);
        v_ui16(TVON.delayTime, 5);
        break;
        
    case 10:
        // TLC5940NT PWM controller chip setup
        v_byte(tlc5940.nchips, 2);
        v_byte(tlc5940.sin, 3);
        v_byte(tlc5940.sclk, 4);
        v_byte(tlc5940.xlat, 5);
        v_byte(tlc5940.blank, 6);
        v_byte(tlc5940.gsclk, 7);
        break;
        
    case 11:
        // 74HC595 shift register chip setup
        v_byte(hc595.nchips, 2);
        v_byte(hc595.sin, 3);
        v_byte(hc595.sclk, 4);
        v_byte(hc595.latch, 5);
        v_byte(hc595.ena, 6);
        break;
        
    case 12:
        // Disconnect reboot timeout
        v_byte(disconnectRebootTimeout, 2);
        break;
        
    case 13:
        // plunger calibration
        v_ui16(plunger.cal.zero, 2);
        v_ui16(plunger.cal.max, 4);
        v_byte(plunger.cal.tRelease, 6);
        v_byte(plunger.cal.calibrated, 7);
        break;
        
    case 14:
        // expansion board configuration
        v_byte(expan.typ, 2);
        v_byte(expan.vsn, 3);
        v_byte(expan.ext[0], 4);
        v_byte(expan.ext[1], 5);
        v_byte(expan.ext[2], 6);
        break;
        
    case 15:
        // night mode configuration
        v_byte(nightMode.btn, 2);
        v_byte(nightMode.flags, 3);
        v_byte(nightMode.port, 4);
        break;
        
    case 16:
        // shift button configuration
        v_byte(shiftButton.idx, 2);
        v_byte(shiftButton.mode, 3);
        break;
        
    case 17:
        // IR sensor and emitter setup
        v_byte(IR.sensor, 2);
        v_byte(IR.emitter, 3);
        break;
        
    case 18:
        // plunger auto-zeroing time
        v_byte(plunger.autoZero.flags, 2);
        v_byte(plunger.autoZero.t, 3);
        break;
        
    case 19:
        // Plunger filters - jitter window size, reversed orientation.
        // The reversed orientation byte always has bit 0x80 set to indicate
        // that the feature is supported in this version.
        v_ui16(plunger.jitterWindow, 2);
        v_byte_ro(cfg.plunger.reverseOrientation | 0x80, 4);
        v_byte_wo(plunger.reverseOrientation, 4);
        break;
        
    case 20:
        // bar-code plunger setup
        v_ui16(plunger.barCode.startPix, 2);
        break;
        
    case 21:
        // TLC59116 PWM controller setup
        v_ui16(tlc59116.chipMask, 2);
        v_byte(tlc59116.sda, 4);
        v_byte(tlc59116.scl, 5);
        v_byte(tlc59116.reset, 6);
        break;
        
    case 22:
        // plunger raw configuration
        v_ui16(plunger.cal.raw0, 2);
        v_ui16(plunger.cal.raw1, 4);
        v_ui16(plunger.cal.raw2, 6);
        break;
        
    // case N: // new scalar variable
    //
    // !!! ATTENTION !!!
    // UPDATE CASE 0 ABOVE WHEN ADDING A NEW VARIABLE!!!

    
    // ********** SPECIAL DIAGNOSTIC VARIBLES **********
    //
    // This is a set of variables that act like the array variables
    // below.  However, these are generally read-only, and since they
    // don't contain restorable configuration data, they're not 
    // included in the variable counts reported by a "variable 0"
    // query above.
    case 220:
#if !VAR_MODE_SET && ENABLE_DIAGNOSTICS
        {
            uint32_t a;
            switch (data[2])
            {
                case 1:
                    // main loop, average iteration time in us
                    a = uint32_t(mainLoopIterTime/mainLoopIterCount);
                    v_ui32_ro(a, 3);
                    break;
                    
                case 2:
                    // incoming message average processing time in us
                    a = uint32_t(mainLoopMsgTime/mainLoopMsgCount);
                    v_ui32_ro(a, 3);
                    break;
                
                case 3:
                    // PWM update polling routine, average time per call in us
                    a = uint32_t(polledPwmTotalTime/polledPwmRunCount);
                    v_ui32_ro(a, 3);
                    break;
                
                case 4:
                    // LedWiz flash update routine, average time per call in us
                    a = uint32_t(wizPulseTotalTime/wizPulseRunCount);
                    v_ui32_ro(a, 3);
                    break;
                    
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                    // main loop checkpoint N, time in us
                    a = uint32_t(mainLoopIterCheckpt[data[2]-5]/mainLoopIterCount);
                    v_ui32_ro(a, 3);
                    break;
                    
                case 30:
                    a = (plungerSensor != 0 ? plungerSensor->getAvgScanTime() : 0);
                    v_ui32_ro(a, 3);
                    break;                    
            }
        }
#endif
        break;
        
    // ********** ARRAY VARIABLES **********


    // case N: // new array variable
    //
    // !!! ATTENTION !!!
    // UPDATE CASE 0 ABOVE WHEN ADDING A NEW ARRAY VARIABLE!!!
    
    case 250:
        // IR command code - high 32 bits
        {
            int idx = data[2];
            if (idx == 0)
            {
                v_byte_ro(MAX_IR_CODES, 3);
            }
            else if (idx > 0 && idx <= MAX_IR_CODES)
            {
                --idx;
                v_ui32(IRCommand[idx].code.hi, 3);
            }
        }
        break;
    
    case 251:
        // IR command code - protocol and low 32 bits
        {
            int idx = data[2];
            if (idx == 0)
            {
                v_byte_ro(MAX_IR_CODES, 3);
            }
            else if (idx > 0 && idx <= MAX_IR_CODES)
            {
                --idx;
                v_byte(IRCommand[idx].protocol, 3);
                v_ui32(IRCommand[idx].code.lo, 4);
            }
        }
        break;
    
    case 252:
        // IR command descriptor
        {
            int idx = data[2];
            if (idx == 0)
            {
                v_byte_ro(MAX_IR_CODES, 3);
            }
            else if (idx > 0 && idx <= MAX_IR_CODES)
            {
                --idx;
                v_byte(IRCommand[idx].flags, 3);
                v_byte(IRCommand[idx].keytype, 4);
                v_byte(IRCommand[idx].keycode, 5);
            }
        }
        break;
    
    case 253:
        // extended button setup
        {
            // get the index and check if it's in range
            int idx = data[2];
            if (idx == 0)
            {
                // index 0 on query retrieves number of slots
                v_byte_ro(MAX_BUTTONS, 3);
            }
            else if (idx > 0 && idx <= MAX_BUTTONS)
            {
                // adjust to an array index
                --idx;
                
                // transfer the values
                v_byte(button[idx].typ2, 3);
                v_byte(button[idx].val2, 4);
                v_byte(button[idx].IRCommand2, 5);
            }                
        }
        break;

    case 254:
        // button setup
        {
            // get the button number
            int idx = data[2];
            
            // if it's in range, set the button data
            if (idx == 0)
            {
                // index 0 on query retrieves number of slots
                v_byte_ro(MAX_BUTTONS, 3);
            }
            else if (idx > 0 && idx <= MAX_BUTTONS)
            {
                // adjust to an array index
                --idx;
                
                // transfer the values
                v_byte(button[idx].pin, 3);
                v_byte(button[idx].typ, 4);
                v_byte(button[idx].val, 5);
                v_byte(button[idx].flags, 6);
                v_byte(button[idx].IRCommand, 7);
            }
        }
        break;
        
    case 255:
        // LedWiz output port setup
        {
            // get the port number
            int idx = data[2];
            
            // if it's in range, set the port data
            if (idx == 0)
            {
                // index 0 on query retrieves number of slots
                v_byte_ro(MAX_OUT_PORTS, 3);
            }
            else if (idx > 0 && idx <= MAX_OUT_PORTS)
            {
                // adjust to an array index
                --idx;
                
                // set the values
                v_byte(outPort[idx].typ, 3);
                v_byte(outPort[idx].pin, 4);
                v_byte(outPort[idx].flags, 5);
                v_byte(outPort[idx].flipperLogic, 6);
            }
        }
        break;
    }
}

