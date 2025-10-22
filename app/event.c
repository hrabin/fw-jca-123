#include "common.h"
#include "const.h"
#include "event.h"
#include "event_buf.h"
#include "log.h"
#include "cfg.h"
#include "cmd.h"
#include "sms.h"
#include "sms_processing.h"
#include "rtc.h"

LOG_DEF("EVENT");
OS_MUTEX(event_mutex);

#define SMS_BUF_SIZE 20
event_buf_item_t sms_items[SMS_BUF_SIZE];
event_buf_t sms_buf;

#define _EVENT_BUF_SIZE 32
// primary event buffer (fifo)
static event_t _event_buffer[_EVENT_BUF_SIZE];
static int _buf_rd_ptr = 0;
static int _buf_wr_ptr = 0;

typedef struct {
    u8 id;
    u8 group;
    cfg_id_t text_id;
} event_setup_t;

static const event_setup_t EVENT_SETUP[] = {
    {EVENT_ID_BOOT_UP, EVENT_GROUP_INFO, CFG_ID_TEXT_EVENT_NAME_000},
    {EVENT_ID_SHUT_DOWN, EVENT_GROUP_INFO, CFG_ID_TEXT_EVENT_NAME_001},
    {EVENT_ID_POWER_FAIL, EVENT_GROUP_FAULT, CFG_ID_TEXT_EVENT_NAME_002},
    {EVENT_ID_POWER_RECOVERY, EVENT_GROUP_FAULT, CFG_ID_TEXT_EVENT_NAME_003},
    {EVENT_ID_LO_BATT, EVENT_GROUP_FAULT , CFG_ID_TEXT_EVENT_NAME_004},
    {EVENT_ID_LO_BATT_RECOVERY, EVENT_GROUP_FAULT, CFG_ID_TEXT_EVENT_NAME_005},
    {EVENT_ID_SET, EVENT_GROUP_CONTROL, CFG_ID_TEXT_EVENT_NAME_006},
    {EVENT_ID_UNSET, EVENT_GROUP_CONTROL , CFG_ID_TEXT_EVENT_NAME_007},
    {EVENT_ID_ALARM, EVENT_GROUP_ALARM, CFG_ID_TEXT_EVENT_NAME_008},
    {EVENT_ID_ALARM_CANCEL, EVENT_GROUP_ALARM, CFG_ID_TEXT_EVENT_NAME_010},
    {EVENT_ID_ALARM_TIMEOUT, EVENT_GROUP_ALARM, CFG_ID_TEXT_EVENT_NAME_011},
    {EVENT_ID_FAULT, EVENT_GROUP_FAULT, CFG_ID_TEXT_EVENT_NAME_012},
    {EVENT_ID_FAULT_RECOVERY, EVENT_GROUP_FAULT, CFG_ID_TEXT_EVENT_NAME_013},
    {EVENT_ID_JAMMING_ACT, EVENT_GROUP_INFO, CFG_ID_TEXT_EVENT_NAME_014},
    {EVENT_ID_JAMMING_DACT, EVENT_GROUP_INFO, CFG_ID_TEXT_EVENT_NAME_015},
};

typedef struct {
    u8 id;
    cfg_id_t text_id;
} event_source_setup_t;

static const event_source_setup_t EVENT_SOURCE_SETUP[] = {
    {EVENT_SOURCE_UNKNOWN, CFG_ID_TEXT_SOURCE_NAME_000},
    {EVENT_SOURCE_UNIT, CFG_ID_TEXT_SOURCE_NAME_001},
    {EVENT_SOURCE_KEY, CFG_ID_TEXT_SOURCE_NAME_002},
    {EVENT_SOURCE_DOOR, CFG_ID_TEXT_SOURCE_NAME_003},
    {EVENT_SOURCE_INPUT1, CFG_ID_TEXT_SOURCE_NAME_004},
    {EVENT_SOURCE_SHOCK, CFG_ID_TEXT_SOURCE_NAME_005},
    {EVENT_SOURCE_LOCK, CFG_ID_TEXT_SOURCE_NAME_006},
    {EVENT_SOURCE_CELL, CFG_ID_TEXT_SOURCE_NAME_007},
    {EVENT_SOURCE_BATTERY, CFG_ID_TEXT_SOURCE_NAME_008},
    {EVENT_SOURCE_USER1, CFG_ID_USER1_NAME},
    {EVENT_SOURCE_USER2, CFG_ID_USER2_NAME},
    {EVENT_SOURCE_USER3, CFG_ID_USER3_NAME},
    {EVENT_SOURCE_USER4, CFG_ID_USER4_NAME},
};


/*
static void _buffer_lock (void)
{
    OS_MUTEX_LOCK(event_mutex);
}

static void _buffer_unlock (void)
{
    OS_MUTEX_UNLOCK(event_mutex);
}
*/

static const ascii *_event_name(event_id_e id)
{
    static ascii cfg[CFG_ITEM_SIZE];
    buf_t buf;

    OS_ASSERT(id < EVENT_ID_SIZE, "EVENT_ID_SIZE");
    OS_ASSERT(id == EVENT_SETUP[id].id, "EVENT_SETUP corrupted");
    buf_init(&buf, cfg, sizeof(cfg));
    
    if (! cfg_read(&buf, EVENT_SETUP[id].text_id, ACCESS_SYSTEM))
        return ("error");

    return (cfg);
}

static const ascii *_source_name(event_source_e id)
{
    static ascii cfg[CFG_ITEM_SIZE];
    buf_t buf;

    OS_ASSERT(id < EVENT_SOURCE_SIZE, "EVENT_SOURCE_SIZE");
    OS_ASSERT(id == EVENT_SOURCE_SETUP[id].id, "EVENT_SOURCE_SETUP corrupted");
    buf_init(&buf, cfg, sizeof(cfg));
    
    if (! cfg_read(&buf, EVENT_SOURCE_SETUP[id].text_id, ACCESS_SYSTEM))
        return ("error");

    return (cfg);
}

bool event_init(void)
{
    OS_MUTEX_INIT(event_mutex);
    memset(&_event_buffer, 0, sizeof(_event_buffer));
    event_buf_init(&sms_buf, sms_items, SMS_BUF_SIZE);

    return (true);
}

bool event_create_ext (event_id_e e, event_source_e s, event_channel_e ch, rtc_t *event_time)
{   // new event 
    // add it to primary buffer and proccess it later from event_task()
    static u16 event_cnt = 0;
    event_t *event;
    unsigned int ptr = _buf_wr_ptr;

    event = &_event_buffer[ptr];
    if (++ptr == _EVENT_BUF_SIZE)
        ptr = 0;

    if (ptr == _buf_wr_ptr)
    {
        LOG_ERROR("buffer overflow");
        return (false);
    }
    if (event_time == NULL)
    {   // time not specified, use current time
        rtc_get_time(&event->time);
    }
    else
    {
        memcpy(&event->time, event_time, sizeof(event->time));
    }
    LOG_DEBUG("E=%d, S=%d", e, s);
    event->cnt    = ++event_cnt;
    event->id     = e;
    event->source = s;
    _buf_wr_ptr = ptr; // atomic pointer update
    return (true);
}

bool event_create (event_id_e e, event_source_e s)
{
    return (event_create_ext(e, s, EVENT_CHANNEL_UNIT, NULL));
}

void event_task(void)
{
    unsigned int ptr = _buf_rd_ptr;

    OS_ASSERT(_buf_wr_ptr < _EVENT_BUF_SIZE, "_buf_wr_ptr");

    // processing primary event buffer 
    while (ptr != _buf_wr_ptr)
    {
        event_t *e;
        event_group_e g;
        ascii cfg[CFG_ITEM_SIZE];
        buf_t buf;

        OS_ASSERT(_buf_rd_ptr < _EVENT_BUF_SIZE, "_buf_rd_ptr");
        e = &_event_buffer[_buf_rd_ptr];
        OS_ASSERT(e->id < EVENT_ID_SIZE, "e->id");
        g = EVENT_SETUP[e->id].group;

        buf_init(&buf, cfg, sizeof(cfg));
        if (cfg_read(&buf, CFG_ID_EVENT_SETUP_000 + g, ACCESS_SYSTEM))
        {
            const ascii *p = cfg;
            cmd_int_t user;
            u8 limit=30;

            while (limit--)
            {
                switch (*p++)
                {
                case '\0':
                    break;

                case 'S':
                    if (cmd_fetch_num(&user, &p))
                    {
                        LOG_DEBUG("sms to user %d", user);
                        event_buf_add_event(&sms_buf, e, EVENT_PRIO_STD, user);
                    }
                    continue;

                case 'V':
                    if (cmd_fetch_num(&user, &p))
                    {
                        LOG_DEBUG("call to user %d", user);
                        // event_buf_add_event(&voice_buf, e, EVENT_PRIO_STD, user);
                    }
                    continue;

                case ',':
                    continue;

                default:
                    break;
                }
                break;
            }

        }

        LOG_DEBUG("buf %d, %s, %s", _buf_rd_ptr, _event_name(e->id), _source_name(e->source));

        if (++ptr == _EVENT_BUF_SIZE)
            ptr = 0;
        _buf_rd_ptr = ptr; // atomic pointer update
    }
}

static void _buf_add_event(buf_t *buf, event_t *e)
{
    rtc_t *t = &e->time;
    rtc_t now;

    rtc_get_time(&now);

    // buf_append_str(buf, );
    if ((now.day == t->day) || (now.month == t->month))
        buf_append_fmt(buf, "%d:%02d:%02d", t->hour, t->minute, t->second);
    else
        buf_append_fmt(buf, "%02d/%d/%d %d:%02d:%02d", t->year, t->month, t->day, t->hour, t->minute, t->second);

    buf_append_str(buf, " ");
    buf_append_str(buf, _event_name(e->id));
    buf_append_str(buf, ", ");
    buf_append_str(buf, _source_name(e->source));
}

void event_comm_task(void)
{
    event_t event;
    u16 user = EVENT_USER_NONE; 

    if (event_buf_get_event(&sms_buf, &event, &user, NULL))
    {
        buf_t sms_text;

        if (! buf_init (&sms_text, NULL, MAX_SMS_LEN))
            return;

        cfg_read(&sms_text, CFG_ID_DEVICE_NAME, ACCESS_SYSTEM);

        _buf_add_event(&sms_text, &event);

        OS_DELAY(2000); // get some time to collect more events
        
        while (event_buf_get_event(&sms_buf, &event, &user, NULL))
        {
            buf_append_str(&sms_text, ";" NL);
            _buf_add_event(&sms_text, &event);
        }

        LOG_DEBUG("sending sms for %d: \"%s\"", user, buf_data(&sms_text));

        if (sms_send_to_user(user, &sms_text))
            event_buf_done_events(&sms_buf, true);

        buf_free (&sms_text);
    }
}

void event_debug(void)
{
    event_buf_info(&sms_buf);
}
