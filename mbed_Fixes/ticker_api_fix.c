#include <stddef.h>
#include "ticker_api.h"

void $Sub$$ticker_insert_event(const ticker_data_t *const data, ticker_event_t *obj, timestamp_t timestamp, uint32_t id) {
    /* disable interrupts for the duration of the function */
    __disable_irq();

    // initialise our data
    obj->timestamp = timestamp;
    obj->id = id;

    /* Go through the list until we either reach the end, or find
       an element this should come before (which is possibly the
       head). */
    ticker_event_t *prev = NULL, *p = data->queue->head;
    while (p != NULL) {
        /* check if we come before p */
        if ((int)(timestamp - p->timestamp) < 0) {
            break;
        }
        /* go to the next element */
        prev = p;
        p = p->next;
    }

    /* if we're at the end p will be NULL, which is correct */
    // BUG FIX: do this BEFORE calling set_interrupt(), to ensure
    // that the list is in a consistent state if set_interrupt()
    // happens to call the event handler, and the event handler
    // happens to call back here to re-queue the event.  Such
    // things aren't hypothetical: this exact thing will happen
    // if a Ticker object gets more than one cycle behind.  The
    // inconsistent state of the list caused crashes.
    obj->next = p;

    /* if prev is NULL we're at the head */
    if (prev == NULL) {
        data->queue->head = obj;
        data->interface->set_interrupt(timestamp);
    } else {
        prev->next = obj;
    }

    __enable_irq();
}

