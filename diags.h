// Pinscape Controller diagnostics

// Enable/disable diagnostics.
//
// Set to non-zero to enable diagnostics and performance counters.
// The counters can generally be queried via the QUERY VARIABLE
// protocol message, using variable 220.  See USBProtocol.h for
// details.
//
// The diagnostic counters add a little run-time overhead, so we
// generally disable them in release builds.  They're meant mostly for
// development and debugging purposes, since there's not usually
// anything you can do in terms of configuration that would affect
// them.  They're mostly a function of the structure of the firmware
// code, so they can be useful when working on the code for things
// like checking for timing bottlenecks.
//
// Note that you can view the diagnostics via the Windows config tool,
// if they're enabled.  If it finds valid values, it displays them on 
// the front page, in the device listing section.  It doesn't mention
// the diagnostics if they're missing, since that's the normal case.
//
#define ENABLE_DIAGNOSTICS  0


// cover code with this to enable only when diagnostics are on
#if ENABLE_DIAGNOSTICS
# define IF_DIAG(code)  code
#else
# define IF_DIAG(code)
#endif
