// Stub for host-side testing — the real watchdog needs the RTOS.
// Shadows sdk/hal/wdog.h (tests/stubs is first on the include path).
#ifndef WDOG_H
#define WDOG_H

static inline void wdog_task_feed_current(void) { }

#endif
