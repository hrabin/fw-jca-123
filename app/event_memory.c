#include "common.h"
#include "event_memory.h"
#include "storage.h"
#include "log.h"

LOG_DEF("EM");

#include <assert.h>

#define _EVENT_MEM_SIZE (64)

#define _EM_DEBUG( ... ) //  LOG_DEBUG(__VA_ARGS__)

static volatile u32 _event_memory_ptr = 0; // current write pointer

static u32 _ptr_inc(u32 ptr)
{
    ptr += _EVENT_MEM_SIZE;
    if (ptr > (STORAGE_EVENT_MEM_SPACE - _EVENT_MEM_SIZE))
    {
        ptr = 0;
    }
    return (ptr);
}

static u32 _ptr_dec(u32 ptr)
{
    if (ptr >= _EVENT_MEM_SIZE)
        return (ptr - _EVENT_MEM_SIZE);
    
    ptr = STORAGE_EVENT_MEM_SPACE - _EVENT_MEM_SIZE;
    return (ptr);
}

bool event_memory_init(void)
{
    event_t e;
    u32 cnt_max = 0;
    u32 ptr_max = 0;

    static_assert(_EVENT_MEM_SIZE >= sizeof(event_t), "_EVENT_MEM_SIZE mismatch");
    static_assert((STORAGE_EVENT_MEM_SPACE % _EVENT_MEM_SIZE) == 0, "STORAGE_EVENT_MEM_SPACE mismatch");

    for (u32 i=0; i<(STORAGE_EVENT_MEM_SPACE - _EVENT_MEM_SIZE); i += _EVENT_MEM_SIZE)
    {
        if (! storage_read_event((u8 *)&e, i, sizeof(event_t)))
        {
            LOG_ERROR("read %u failed", i);
            return (false);
        }
        if (! event_valid(&e))
        {
            _event_memory_ptr = i;
            event_set_cnt(cnt_max);
            _EM_DEBUG("empty ptr=%u", _event_memory_ptr);
            return (true);
        }
        if (e.cnt > cnt_max)
        {
            cnt_max = e.cnt;
            ptr_max = i;
        }
    }
    event_set_cnt(cnt_max);
    _event_memory_ptr = _ptr_inc(ptr_max);
    _EM_DEBUG("ptr=%u", _event_memory_ptr);
    return (true);
}


bool event_memory_store(event_t *event)
{
    u32 ptr = _event_memory_ptr;
    _event_memory_ptr =  _ptr_inc(_event_memory_ptr);
   
    OS_ASSERT(ptr <= (STORAGE_EVENT_MEM_SPACE - _EVENT_MEM_SIZE),"ptr mismatch");
    _EM_DEBUG("wr ptr=%u", _event_memory_ptr);

    return (storage_write_event(ptr, (u8 *)event, sizeof(event_t)));
}

bool event_memory_read(event_t *event, size_t back_index)
{
    u32 ptr = _event_memory_ptr;

    for (u32 i=0; i<(STORAGE_EVENT_MEM_SPACE - _EVENT_MEM_SIZE); i += _EVENT_MEM_SIZE)
    {
        ptr = _ptr_dec(ptr);
        
        if (back_index == 0)
        {
            _EM_DEBUG("rd ptr=%u", ptr);
            if (storage_read_event((u8 *)event, ptr, sizeof(event_t)))
            {
                return (event_valid(event));
            }
            break;
        }
        back_index--;
    }
    return (false);
}


