#ifndef EVENT_H
#define EVENT_H

#include "type.h"
#include "rtc.h"

#define EVENT_USER_NONE 0xFFFF

enum {
    EVENT_ID_BOOT_UP,
    EVENT_ID_SHUT_DOWN,
    EVENT_ID_POWER_FAIL,
    EVENT_ID_POWER_RECOVERY,
    EVENT_ID_LO_BATT,
    EVENT_ID_LO_BATT_RECOVERY,
    EVENT_ID_SET,
    EVENT_ID_UNSET,
    EVENT_ID_ALARM,
    EVENT_ID_ALARM_CANCEL,
    EVENT_ID_ALARM_TIMEOUT,
    EVENT_ID_FAULT,
    EVENT_ID_FAULT_RECOVERY,
    EVENT_ID_JAMMING_ACT,
    EVENT_ID_JAMMING_DACT,

    EVENT_ID_SIZE
};
typedef u8 event_id_e;

enum {
    EVENT_SOURCE_UNKNOWN = 0,
    EVENT_SOURCE_UNIT,
    EVENT_SOURCE_KEY,
    EVENT_SOURCE_DOOR,
    EVENT_SOURCE_INPUT1,
    EVENT_SOURCE_SHOCK,
    EVENT_SOURCE_LOCK,
    EVENT_SOURCE_CELL,
    EVENT_SOURCE_BATTERY,
    EVENT_SOURCE_ADMIN,
    EVENT_SOURCE_USER1,
    EVENT_SOURCE_USER2,
    EVENT_SOURCE_USER3,
    EVENT_SOURCE_USER4,

    EVENT_SOURCE_SIZE
};
typedef u8 event_source_e;

enum {
    EVENT_CHANNEL_UNIT,
    EVENT_CHANNEL_IO,
    EVENT_CHANNEL_RF,
    EVENT_CHANNEL_SMS,
    EVENT_CHANNEL_CALL,
    EVENT_CHANNEL_DATA,

    EVENT_CHANNEL_SIZE
};
typedef u8 event_channel_e;

enum {
    EVENT_GROUP_ALARM,
    EVENT_GROUP_FAULT,
    EVENT_GROUP_CONTROL,
    EVENT_GROUP_INFO,
};
typedef u8 event_group_e;

enum {
    EVENT_PRIO_LO,
    EVENT_PRIO_STD,
    EVENT_PRIO_HI,
};

typedef u8 event_prio_e;

typedef struct
{
    rtc_t time;
    u16 cnt;
    event_id_e id;
    event_source_e source;
} event_t;

bool event_init(void);

bool event_create_ext (event_id_e e, event_source_e s, event_channel_e ch, rtc_t *event_time);
bool event_create (event_id_e e, event_source_e s);
void event_task(void);
void event_comm_task(void);

#endif // ! EVENT_H

