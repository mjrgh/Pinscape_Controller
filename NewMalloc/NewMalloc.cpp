#include "mbed.h"
#include "NewMalloc.h"

extern void diagLED(int, int, int);

// Custom memory allocator.  We use our own version of malloc() for more
// efficient memory usage, and to provide diagnostics if we run out of heap.
//
// We can implement a more efficient malloc than the library can because we
// can make an assumption that the library can't: allocations are permanent.
// The normal malloc has to assume that allocations can be freed, so it has
// to track blocks individually.  For the purposes of this program, though,
// we don't have to do this because virtually all of our allocations are 
// de facto permanent.  We only allocate dyanmic memory during setup, and 
// once we set things up, we never delete anything.  This means that we can 
// allocate memory in bare blocks without any bookkeeping overhead.
//
// In addition, we can make a larger overall pool of memory available in
// a custom allocator.  The RTL malloc() seems to have a pool of about 3K 
// to work with, even though there really seems to be at least 8K left after 
// reserving a reasonable amount of space for the stack.

// halt with a diagnostic display if we run out of memory
void HaltOutOfMem()
{
    printf("\r\nOut Of Memory\r\n");
    // halt with the diagnostic display (by looping forever)
    for (;;)
    {
        diagLED(1, 0, 0);
        wait_us(200000);
        diagLED(1, 0, 1);
        wait_us(200000);
    }
}

// For our custom malloc, we take advantage of the known layout of the
// mbed library memory management.  The mbed library puts all of the
// static read/write data at the low end of RAM; this includes the
// initialized statics and the "ZI" (zero-initialized) statics.  The
// malloc heap starts just after the last static, growing upwards as
// memory is allocated.  The stack starts at the top of RAM and grows
// downwards.  
//
// To figure out where the free memory starts, we simply call the system
// malloc() to make a dummy allocation the first time we're called, and 
// use the address it returns as the start of our free memory pool.  The
// first malloc() call presumably returns the lowest byte of the pool in
// the compiler RTL's way of thinking, and from what we know about the
// mbed heap layout, we know everything above this point should be free,
// at least until we reach the lowest address used by the stack.
//
// The ultimate size of the stack is of course dynamic and unpredictable.
// In testing, it appears that we currently need a little over 1K.  To be
// conservative, we'll reserve 2K for the stack, by taking it out of the
// space at top of memory we consider fair game for malloc.
//
// Note that we could do this a little more low-level-ly if we wanted.
// The ARM linker provides a pre-defined extern char[] variable named 
// Image$$RW_IRAM1$$ZI$$Limit, which is always placed just after the
// last static data variable.  In principle, this tells us the start
// of the available malloc pool.  However, in testing, it doesn't seem
// safe to use this as the start of our malloc pool.  I'm not sure why,
// but probably something in the startup code (either in the C RTL or 
// the mbed library) is allocating from the pool before we get control. 
// So we won't use that approach.  Besides, that would tie us even more
// closely to the ARM compiler.  With our malloc() probe approach, we're
// at least portable to any compiler that uses the same basic memory
// layout, with the heap above the statics and the stack at top of 
// memory; this isn't universal, but it's very typical.

extern "C" {
    void *$Sub$$malloc(size_t);
    void *$Super$$malloc(size_t);
    void $Sub$$free(void *);
};

// override the system malloc
void *$Sub$$malloc(size_t siz)
{
    return xmalloc(siz);
}

// custom allocator pool
static char *xmalloc_nxt = 0;
size_t xmalloc_rem = 0;

// custom allocator
void *xmalloc(size_t siz)
{
    // initialize the pool if we haven't already
    if (xmalloc_nxt == 0)
    {
        // do a dummy allocation with the system malloc() to find where
        // the free pool starts
        xmalloc_nxt = (char *)$Super$$malloc(4);
        
        // figure the amount of space we can use - we have from the base
        // of the pool to the top of RAM, minus an allowance for the stack
        const uint32_t TopOfRAM = 0x20003000UL;
        const uint32_t StackSize = 2*1024;
        xmalloc_rem = TopOfRAM - StackSize - uint32_t(xmalloc_nxt);
    }
    
    // align to a dword boundary
    siz = (siz + 3) & ~3;
    
    // make sure we have enough space left for this chunk
    if (siz > xmalloc_rem)
        HaltOutOfMem();
        
    // carve the chunk out of the remaining free pool
    char *ret = xmalloc_nxt;
    xmalloc_nxt += siz;
    xmalloc_rem -= siz;
    
    // return the allocated space
    return ret;
}

// Remaining free memory
size_t mallocBytesFree() 
{
    return xmalloc_rem;
}

