#ifndef _NEWMALLOC_H_
#define _NEWMALLOC_H_

#include "mbed.h"

// our custom memory allocator
void *xmalloc(size_t);

// Number of free bytes remaning
size_t mallocBytesFree();

#endif
