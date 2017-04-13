#include "TinyDigitalIn.h"

// Dummy port for NC (not connected) ports.  This is simply a memory
// location that always reads as all ones.  This lets us set up a pointer
// in the instance so that we can read it as though it were really
// connected to a port, but the port will always read as pulled up.
const uint32_t TinyDigitalIn::pdir_nc = 0xFFFFFFFF;
