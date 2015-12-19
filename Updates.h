// UPDATES
//
// This is a record of new features and changes in recent versions.
//

// January 2016
//
// Dynamic configuration:  all configuration options are now handled dynamically,
// through the Windows config tool.  In earlier versions, most configuration options
// were set through compile-time constants, which made it necessary for everyone
// who wanted to customize anything to create a private branched version of the
// source repository, edit the source code, and compile their own binary.  This
// was cumbersome, and required way too much technical knowledge to be worth the
// trouble to a lot of people.  The goal of the new approach is that everyone can
// use the same standard binary build, and set options from the Windows tool.
//
// TSL1410R and 1412R parallel mode support:  these sensors are physically built
// out of two separate pixel arrays, which can be read independently.  Past
// versions only supported "serial" mode pixel transfer, where we read all of 
// the first array's pixels before reading any of the second array's pixels.
// In parallel mode, we can read pixels from both arrays at the same time.  The
// limiting factor in image read speed is the amount of time it takes for the
// ADC to transfer charge from a pixel and stabilize on a reading.  The KL25Z
// has multiple ADC hardware channels, so we can read multiple analog values
// concurrently - it takes the same amount of time for one ADC reading to
// stabilize as two readings.  So by reading from the two sensor sections 
// concurrently, we can essentially double the transfer speed.  Faster pixel
// transfer allows for more accurate motion tracking when the plunger is
// moving at high speed, allowing for more realistic plunger action on the
// virtual side.
//
// 
