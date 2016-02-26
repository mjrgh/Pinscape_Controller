// Global definitions

#ifndef PINSCAPE_H
#define PINSCAPE_H

// custom malloc - includes diagnostics if we run out of memory
void *xmalloc(size_t siz);

// diagnostic LED display
void diagLED(int r, int g, int b);

// count of elements in array
#define countof(x) (sizeof(x)/sizeof((x)[0]))

#endif
