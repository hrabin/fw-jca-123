#ifndef ACCESS_H
#define ACCESS_H

#include "common.h"

#define ACCESS_NONE   BIT(0)
#define ACCESS_USER   BIT(1)
#define ACCESS_ADMIN  BIT(2)
#define ACCESS_SYSTEM BIT(3)


#define ACCESS_UART_TIMEOUT (3600 * OS_TIMER_SECOND)
#define ACCESS_BLE_TIMEOUT (120 * OS_TIMER_SECOND)


typedef u8 access_auth_t;

typedef struct {
    access_auth_t auth;
    os_timer_t time;
    // TODO: source_id
    // TODO: channel_id
} access_t;

#endif // ! ACCESS_H

