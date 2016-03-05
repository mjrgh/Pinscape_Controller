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


void v_func(uint8_t *data)
{
    switch (data[1])
    {
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
        // Enable/disable joystick
        v_byte(joystickEnabled, 2);
        break;
        
    case 4:
        // Accelerometer orientation
        v_byte(orientation, 2);
        break;

    case 5:
        // Plunger sensor type
        v_byte(plunger.sensorType, 2);
        break;
        
    case 6:
        // Plunger sensor pin assignments
        v_pin(plunger.sensorPin[0], 2);
        v_pin(plunger.sensorPin[1], 3);
        v_pin(plunger.sensorPin[2], 4);
        v_pin(plunger.sensorPin[3], 5);
        break;
        
    case 7:
        // Plunger calibration button and indicator light pin assignments
        v_pin(plunger.cal.btn, 2);
        v_pin(plunger.cal.led, 3);
        break;
        
    case 8:
        // ZB Launch Ball setup
        v_byte(plunger.zbLaunchBall.port, 2);
        v_byte(plunger.zbLaunchBall.btn, 3);
        v_ui16(plunger.zbLaunchBall.pushDistance, 4);
        break;
        
    case 9:
        // TV ON setup
        v_pin(TVON.statusPin, 2);
        v_pin(TVON.latchPin, 3);
        v_pin(TVON.relayPin, 4);
        v_ui16(TVON.delayTime, 5);
        break;
        
    case 10:
        // TLC5940NT PWM controller chip setup
        v_byte(tlc5940.nchips, 2);
        v_pin(tlc5940.sin, 3);
        v_pin(tlc5940.sclk, 4);
        v_pin(tlc5940.xlat, 5);
        v_pin(tlc5940.blank, 6);
        v_pin(tlc5940.gsclk, 7);
        break;
        
    case 11:
        // 74HC595 shift register chip setup
        v_byte(hc595.nchips, 2);
        v_pin(hc595.sin, 3);
        v_pin(hc595.sclk, 4);
        v_pin(hc595.latch, 5);
        v_pin(hc595.ena, 6);
        break;
        
    case 12:
        // button setup
        {
            // get the button number
            int idx = data[2];
            
            // if it's in range, set the button data
            if (idx > 0 && idx <= MAX_BUTTONS)
            {
                // adjust to an array index
                --idx;
                
                // set the values
                v_byte(button[idx].pin, 3);
                v_byte(button[idx].typ, 4);
                v_byte(button[idx].val, 5);
                v_byte(button[idx].flags, 6);
            }
        }
        break;
        
    case 13:
        // LedWiz output port setup
        {
            // get the port number
            int idx = data[2];
            
            // if it's in range, set the port data
            if (idx > 0 && idx <= MAX_OUT_PORTS)
            {
                // adjust to an array index
                --idx;
                
                // set the values
                v_byte(outPort[idx].typ, 3);
                v_byte(outPort[idx].pin, 4);
                v_byte(outPort[idx].flags, 5);
            }
            else if (idx == 254)
            {
                // special ports
                idx -= 254;
                v_byte(specialPort[idx].typ, 3);
                v_byte(specialPort[idx].pin, 4);
                v_byte(specialPort[idx].flags, 5);
            }
        }
        break;
        
    case 14:
        // Disconnect reboot timeout
        v_byte(disconnectRebootTimeout, 2);
        break;
        
    case 15:
        // plunger calibration
        v_ui16(plunger.cal.zero, 2);
        v_ui16(plunger.cal.max, 4);
        v_byte(plunger.cal.tRelease, 6);
        break;
        
    case 16:
        // expansion board configuration
        v_byte(expan.nMain, 2);
        v_byte(expan.nPower, 3);
        v_byte(expan.nChime, 4);
        break;
    }
}

