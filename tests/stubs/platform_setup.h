// Stub for host-side testing — no platform setup needed.
#ifndef PLATFORM_SETUP_H
#define PLATFORM_SETUP_H

#include "type.h"
#include "util.h"

#define PACK __attribute__((packed))
#define PACK_BEGIN
#define PACK_END

#define NL "\r\n"

#define ABS(x) (((x) < 0) ? 0 - (x) : (x))

#endif
