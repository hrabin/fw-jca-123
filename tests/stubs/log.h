// Stub for host-side testing — logging disabled.
#ifndef LOG_H
#define LOG_H

// firmware's log.h includes os.h — mirror that so OS_ASSERT is visible
#include "os.h"

#define LOG_DEF(tag)  static const char *TAG __attribute__((unused)) = tag

#define LOG_DEBUG(...)
#define LOG_DEBUGL(level, ...)
#define LOG_INFO(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)

#endif
