#ifndef EVENT_MEMORY_H
#define EVENT_MEMORY_H

#include "type.h"
#include "event.h"

bool event_memory_init(void);
bool event_memory_store(event_t *event);
bool event_memory_read(event_t *event, size_t back_index);


#endif // ! EVENT_MEMORY_H

