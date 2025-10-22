#ifndef EVENT_BUF_H
#define EVENT_BUF_H

#include "type.h"
#include "event.h"

#define EVENT_BUF_FLAG_USED       BIT(0)   // item valid, not empty
#define EVENT_BUF_FLAG_ACTIVE     BIT(1)   // item active for processing
#define EVENT_BUF_FLAG_PROCESSING BIT(2)   // item used in last communication 
#define EVENT_BUF_FLAG_ZOMBIE     BIT(3)   // possible later reactivation


#define EVENT_BUF_INDEX_NIL 0xFF
#define EVENT_BUF_UNLIMITED 0xFF

typedef u8 eb_ptr_t;

typedef struct
{
    event_t event;
    eb_ptr_t next_index;
    eb_ptr_t prev_index;
    event_prio_e prio:4;
    u8 flag:4;
    u16 user;
    u8 cnt;
} event_buf_item_t;

typedef struct
{
    eb_ptr_t size;
    eb_ptr_t start_index;
    eb_ptr_t last_index;
    eb_ptr_t test_index;
    event_buf_item_t *items;
} event_buf_t;

typedef bool (*pfunc_for_each) (event_t *event, ... );

void event_buf_init (event_buf_t *buf, event_buf_item_t *items, eb_ptr_t size);
bool event_buf_empty (event_buf_t *buf);
bool event_buf_add_event (event_buf_t *buf, event_t *event, event_prio_e prio, u16 user);
bool event_buf_restore_events (event_buf_t *buf, u8 retry_limit);
void event_buf_done_events (event_buf_t *buf, bool delivered);
bool event_buf_get_event (event_buf_t *buf, event_t *event, u16 *user, event_prio_e *prio);
void event_buf_for_each (event_buf_t *buf, pfunc_for_each pfunc, int numargs, ... );
void event_buf_info (event_buf_t *buf);

#endif  // ! EVENT_BUF_H
