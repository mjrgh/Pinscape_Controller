#include "NewMalloc.h"

// Overload operator new to call our custom malloc.  This ensures that
// all 'new' allocations throughout the program (including library code)
// go through our private allocator.
void *operator new(size_t siz) { return xmalloc(siz); }
void *operator new[](size_t siz) { return xmalloc(siz); }

// Since we don't do bookkeeping to track released memory, 'delete' does
// nothing.  In actual testing, this routine appears to never be called.
// If it *is* ever called, it will simply leave the block in place, which
// will make it unavailable for re-use but will otherwise be harmless.
void operator delete(void *ptr) { }
