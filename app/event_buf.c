#include "common.h"
#include "event.h"
#include "event_buf.h"
#include "log.h"

#include <stdarg.h>

LOG_DEF("EB");

void event_buf_init (event_buf_t *buf, event_buf_item_t *items, eb_ptr_t size)
{
	int i;
	buf->size  = size;
	buf->items = items;
	buf->start_index= 0;
	buf->last_index = EVENT_BUF_INDEX_NIL;
	buf->test_index = 0xFF;

	for (i=0; i<size; i++)
	{
		items[i].flag = 0;
		items[i].next_index = EVENT_BUF_INDEX_NIL;
		items[i].prev_index = EVENT_BUF_INDEX_NIL;
	}
}

static eb_ptr_t _buf_find_free (event_buf_t *buf)
{
	int i;

	for (i=0; i<(buf->size); i++)
	{
		if (++(buf->test_index) >= (buf->size))
			buf->test_index=0;

		if (buf->items[buf->test_index].flag == 0)
			return (buf->test_index);
	}
	return (EVENT_BUF_INDEX_NIL);
}

static bool _item_active (event_buf_t *buf, eb_ptr_t buf_index)
{
	if ((buf->items[buf_index].flag & (EVENT_BUF_FLAG_USED | EVENT_BUF_FLAG_ACTIVE)) == (EVENT_BUF_FLAG_USED | EVENT_BUF_FLAG_ACTIVE))
		return (true);
	return (false);
}

static eb_ptr_t _buf_replace_prio (event_buf_t *buf, event_prio_e prio)
{
	int i;
	eb_ptr_t buf_index = buf->start_index;
	event_prio_e lowest_prio = EVENT_PRIO_HI;

	// nejdrive najdu nejnizsi prioritu v bufferu
	for (i=0; i<(buf->size); i++)
	{
		if (buf->items[buf_index].flag == 0)
		{
			LOG_ERROR("inconsistent");
			return (buf_index); // tohle by se stat nemelo
		}

		if (buf->items[buf_index].prio < lowest_prio)
			lowest_prio = buf->items[buf_index].prio;

		if (buf->items[buf_index].next_index == EVENT_BUF_INDEX_NIL)
			break;
		buf_index = buf->items[buf_index].next_index;
	}
	if (lowest_prio>prio)
		return (EVENT_BUF_INDEX_NIL); // vsechno v bufferu ma vyssi prioritu

	if (lowest_prio == prio)
	{	// vsechno v bufferu ma stejnou prioritu
		// preplacneme nejstarsi zaznam
		return (buf->start_index);
	}
	
	// najdu nejstarisi zaznam s nejnizsi prioritou "lowest_prio"
	buf_index = buf->start_index;
	for (i=0; i<(buf->size); i++)
	{
		if (buf->items[buf_index].prio == lowest_prio)
		{
			return (buf_index);
		}
		if (buf->items[buf_index].next_index == EVENT_BUF_INDEX_NIL)
			break;
		buf_index = buf->items[buf_index].next_index;
	}
	return (EVENT_BUF_INDEX_NIL);
}

static void _delete_item (event_buf_t *buf, eb_ptr_t index)
{
	OS_ASSERT (index < (buf->size), "_delete_item() index>=(buf->size)");

 	LOG_DEBUGL(LOG_SELECT_EB, "delete item %d", index);

	if (buf->items[index].flag == 0)
	{
 		LOG_ERROR("clear, i=%d", index);
		return; 
	}

	buf->items[index].flag = 0;
 	if (index != buf->start_index)
	{	// it is not first record
		OS_ASSERT (buf->items[index].prev_index < (buf->size), "_delete_item() prev_index>=(buf->size)");
		buf->items[ buf->items[index].prev_index ].next_index = buf->items[index].next_index;
		if (buf->last_index == index)
		{	// it is last one
			buf->last_index = buf->items[index].prev_index;
			OS_ASSERT (buf->items[index].next_index == EVENT_BUF_INDEX_NIL, "_delete_item() next_index != EVENT_BUF_INDEX_NIL");
			return; 
		}
		OS_ASSERT (buf->items[index].next_index < (buf->size), "_delete_item() next_index >=(buf->size)");
		buf->items[ buf->items[index].next_index ].prev_index = buf->items[index].prev_index;
		return; 
	}
	// it is firs record
	if (index != buf->last_index) 	
	{	// not the last
		buf->start_index = buf->items[index].next_index;
 		OS_ASSERT (buf->items[buf->start_index].prev_index == index, "_delete_item() prev_index != index");
		buf->items[ buf->start_index ].prev_index = EVENT_BUF_INDEX_NIL;
	}
	else
	{	// first and also last record
		buf->last_index=EVENT_BUF_INDEX_NIL;
	}
}

bool event_buf_empty (event_buf_t *buf)
{
	int i;
	eb_ptr_t buf_index=buf->start_index;
	for (i=0; i<(buf->size); i++)
	{
		if (_item_active(buf, buf_index)
		 && ((buf->items[buf_index].flag & (EVENT_BUF_FLAG_PROCESSING | EVENT_BUF_FLAG_ZOMBIE)) == 0))
		{	// 
			return (false);
		}
		if (buf->items[buf_index].next_index == EVENT_BUF_INDEX_NIL)
				break;
		buf_index = buf->items[buf_index].next_index;
		OS_ASSERT (buf_index < (buf->size), "user_buf_empty() buf_index>=(buf->size)");
	}
	return (true);
}

bool event_buf_add_event (event_buf_t *buf, event_t *event, event_prio_e prio, u16 user)
{
	eb_ptr_t wr_index = EVENT_BUF_INDEX_NIL;
	
	if (event == NULL)
		return (false);
	
	if (user == EVENT_USER_NONE)
		return (false);
	
	if (buf->last_index == EVENT_BUF_INDEX_NIL)
	{	// no need to search free space, buffer is empty
		wr_index = buf->start_index;
		OS_ASSERT (wr_index != EVENT_BUF_INDEX_NIL, "user_buf_add_event(), buf->start_index" );
	}
	else if ((wr_index = _buf_find_free(buf)) != EVENT_BUF_INDEX_NIL)
	{	// found free space
		LOG_DEBUGL(LOG_SELECT_EB, "wr=%d", wr_index);
	}
	else if ((wr_index = _buf_replace_prio(buf, prio)) != EVENT_BUF_INDEX_NIL)
	{	// found used space with lower priority
		LOG_DEBUGL(LOG_SELECT_EB, "replace prio wr=%d", wr_index);
		// event which are not "active" delete quietly
 		if (buf->items[wr_index].flag & EVENT_BUF_FLAG_ACTIVE)
		{
			LOG_ERROR("EVENT DISCARD, n=%d", buf->items[wr_index].event.cnt);
		}
		_delete_item (buf, wr_index);		
	}
	else
	{
		LOG_ERROR ("full");	
		return (false);
	}

	// we have place for store
	memset (&(buf->items[wr_index]), 0, sizeof(event_buf_item_t));
	buf->items[wr_index].next_index = EVENT_BUF_INDEX_NIL; // new event is always the last

	memcpy (&(buf->items[wr_index].event), event, sizeof(event_t));
	buf->items[wr_index].prio      = prio;
	buf->items[wr_index].user      = user;
	buf->items[wr_index].flag      = EVENT_BUF_FLAG_USED | EVENT_BUF_FLAG_ACTIVE;

	// add to chain
	if (buf->last_index != EVENT_BUF_INDEX_NIL)
	{
		buf->items[buf->last_index].next_index = wr_index;
	}
	buf->items[wr_index].prev_index = buf->last_index;
	buf->last_index = wr_index;

	OS_ASSERT ((buf->items[wr_index].prev_index != wr_index), "user_buf prev_index == wr_index");
	OS_ASSERT ((buf->items[wr_index].next_index != wr_index), "user_buf next_index == wr_index");

	return (true);
}

bool event_buf_restore_events (event_buf_t *buf, u8 retry_limit)
{
	bool result=true;
	int i;
	eb_ptr_t buf_index=buf->start_index;

	for (i=0; i<(buf->size); i++)
	{
		if (buf->items[buf_index].flag & (EVENT_BUF_FLAG_PROCESSING | EVENT_BUF_FLAG_ZOMBIE))
		{
			buf->items[buf_index].flag &= ~(EVENT_BUF_FLAG_PROCESSING | EVENT_BUF_FLAG_ZOMBIE);
			if (_item_active(buf, buf_index))
			{	// it is active and was communicated 
				if (retry_limit != EVENT_BUF_UNLIMITED)
				{	//
					if (++(buf->items[buf_index].cnt) >= retry_limit)
					{
						LOG_DEBUGL(LOG_SELECT_EB, "FAILED, nr=%d", buf->items[buf_index].event.cnt);
						_delete_item (buf, buf_index);
						result = false; // as signalization "something discarded"
					}
					else
					{
						printf ("[retry %d of %d]", buf->items[buf_index].cnt, retry_limit-1);
					}
				}
			}
			else
			{
				LOG_ERROR("restore inactive");
			}
		}
		if (buf->items[buf_index].next_index == EVENT_BUF_INDEX_NIL)
			break;
		buf_index = buf->items[buf_index].next_index;
	}
	return (result);
}

void event_buf_done_events (event_buf_t *buf, bool delivered)
{
	eb_ptr_t i;
	eb_ptr_t buf_index=buf->start_index;

	LOG_DEBUGL(LOG_SELECT_EB, "done events");

	for (i=0; i<(buf->size); i++)
	{
		if (buf->items[buf_index].flag & EVENT_BUF_FLAG_PROCESSING)
		{
			buf->items[buf_index].flag &= ~EVENT_BUF_FLAG_PROCESSING;
			if (_item_active(buf, buf_index))
			{	// it is active and was in communication 
				if (delivered)
				{	// 
					LOG_DEBUGL(LOG_SELECT_EB, "delivered, nr=%d", buf->items[buf_index].event.cnt);
					_delete_item (buf, buf_index);
				}
				else
				{
					LOG_DEBUGL(LOG_SELECT_EB, "FAILED, nr=%d", buf->items[buf_index].event.cnt);
					buf->items[buf_index].flag |= EVENT_BUF_FLAG_ZOMBIE;
				}
			}
			else
			{
				LOG_ERROR("done inactive");
			}
		}
		if (buf->items[buf_index].next_index == EVENT_BUF_INDEX_NIL)
			break;
		buf_index = buf->items[buf_index].next_index;
	}
}

bool event_buf_get_event (event_buf_t *buf, event_t *event, u16 *user, event_prio_e *prio)
{
	eb_ptr_t i;
	eb_ptr_t buf_index=buf->start_index;
	
	if (user == NULL)
		return (false);

	for (i=0; i<(buf->size); i++)
	{
		if (_item_active(buf, buf_index)
		 && ((buf->items[buf_index].flag & (EVENT_BUF_FLAG_PROCESSING | EVENT_BUF_FLAG_ZOMBIE)) == 0))
		{	// it is active and communication did not start
			if (event != NULL)
				memcpy (event, &(buf->items[buf_index].event), sizeof(event_t));
			if (prio != NULL)
				*prio = buf->items[buf_index].prio;
			
			if (*user == EVENT_USER_NONE)
			{	// dont mind user
				*user = buf->items[buf_index].user;
			}

			if (*user == buf->items[buf_index].user)
			{	// ok, we have record
				buf->items[buf_index].flag |= EVENT_BUF_FLAG_PROCESSING;
				return (true);
			}
		}
		if (buf->items[buf_index].next_index == EVENT_BUF_INDEX_NIL)
			break;
		buf_index = buf->items[buf_index].next_index;
	}
	return (false);
}

void event_buf_for_each (event_buf_t *buf, pfunc_for_each pfunc, int numargs, ... )
{	// will call function "pfunc" for each record and erase it when returns "false"
	va_list args;
	int i;
	eb_ptr_t buf_index=buf->start_index;

	for (i=0; i<(buf->size); i++)
	{
		if (buf->items[buf_index].flag & EVENT_BUF_FLAG_USED)
		{	// 
			va_start (args, numargs);		
			if (pfunc(&(buf->items[buf_index].event), args) == false)
			{	// record erase request
				_delete_item (buf, buf_index);
			}
			va_end(args);

		}

		if (buf->items[buf_index].next_index == EVENT_BUF_INDEX_NIL)
				break;
		buf_index = buf->items[buf_index].next_index;
		OS_ASSERT (buf_index<(buf->size), "user_buf_for_each() buf_index>=(buf->size)");
	}

}

void event_buf_info (event_buf_t *buf)
{
	int i;
 	eb_ptr_t buf_index=buf->start_index;

	OS_PRINTF(NL);
 	OS_PRINTF("buf_start_index = %d" NL, buf->start_index);
 	OS_PRINTF("buf_last_index  = %d" NL, buf->last_index);

	for (i=0; i<(buf->size); i++)
	{
		if (buf->items[buf_index].flag & EVENT_BUF_FLAG_PROCESSING)
		{
			OS_PRINTF("|%02x|", buf_index); 
		}
		else if (buf->items[buf_index].flag)
		{
			OS_PRINTF("%02x", buf_index); 
		}
		else 
		{
			OS_PRINTF("(%02x)", buf_index); 
		}

		if (buf->items[buf_index].next_index == EVENT_BUF_INDEX_NIL)
				break;
		buf_index = buf->items[buf_index].next_index;
		OS_PRINTF("->");
	}

	for (i=0; i<(buf->size); i++)
	{
		OS_ASSERT (buf_index < (buf->size), "user_buf_info() buf_index>=(buf->size)");
  		if (buf->items[i].flag)
		{	// 
			OS_PRINTF(NL);
			OS_PRINTF("buf: %d, flag=0x%02x", i, buf->items[i].flag);
			OS_PRINTF(", n=%d", buf->items[i].event.cnt);
			OS_PRINTF(", type=%d", buf->items[i].event.id);
			OS_PRINTF(", next=%d", buf->items[i].next_index);
			OS_PRINTF(", prev=%d", buf->items[i].prev_index);
			OS_DELAY(10);
		}
	}
}


