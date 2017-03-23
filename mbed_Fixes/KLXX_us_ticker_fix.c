#include <stddef.h>
#include "us_ticker_api.h"

// Bug fix: if scheduling an event in the past, schedule it for
// the very near future rather than invoking the handler directly.
// For Tickers and other recurring events, invoking the handler
// can cause significant recursion, since the handler might try
// to schedule the next event, which will end up back here, which
// will call the handler again, and so on.  Forcing the event
// into the future prevents this recursion and ensures bounded
// stack use.  The effect will be the same either way: the handler
// will be called late, since we can't actually travel back in time
// and call it in the past.  But this way we don't blow the stack
// if we have a high-frequency recurring event that has gotten
// significantly behind (because of a long period with interrupts
// disabled, say).
extern void $Super$$us_ticker_set_interrupt(timestamp_t);
void $Sub$$us_ticker_set_interrupt(timestamp_t timestamp) 
{
    // If the event was in the past, schedule it for almost (but not
    // quite) immediately.  This prevents the base version from recursing
    // into the handler; instead, we'll schedule an interrupt as for any
    // other future event.
    int tcur = us_ticker_read();
    int delta = (int)((uint32_t)timestamp - tcur);
    if (delta <= 0)
        timestamp = tcur + 2;
        
    // call the base handler
    $Super$$us_ticker_set_interrupt(timestamp);
}
