#include    "type.h"
#include    "platform_setup.h"
#include    "multi_sms.h"
#include    "log.h"

LOG_DEF("MS");

typedef struct {
    ascii  *buffer;     // data buffer (must be possible to reallocate)
    u8     order_num;   // order of sms
    u8     total;       // total number od SMS parts
    u8     id;          // id - serialization
    u8     len;         // length of current part of SMS 
} msms_t;

msms_t msms[MSMS_BUF_LEN];

void ms_init (void)
{
    u16 i;

    for (i=0; i<MSMS_BUF_LEN; i++)
    {
        msms[i].len=0;  // zero length means empty buffer 
        OS_MEM_FREE (msms[i].buffer);
        msms[i].buffer=NULL;
    }
}

s16 ms_get_free_buf (u8 id)
{
    u16 i;

    // try to find empty buffer 
    for (i=0; i<MSMS_BUF_LEN; i++)
    {
        if (msms[i].len == 0)
        {
            LOG_DEBUGL(LOG_SELECT_MS, "using memory %d", i);
            return (i);
        }
    }

    // empty not fount, clear old buffers
    LOG_DEBUGL(LOG_SELECT_MS, "erasing memory id %d!", id);
    for (i=0; i<MSMS_BUF_LEN; i++)
    {
        if (msms[i].id != id)
        {
            OS_MEM_FREE (msms[i].buffer);
            msms[i].buffer=NULL;
            msms[i].len=0;
        }
    }
    
    // now there should by free space 
    // if not, it means too much parts and cant be received
    for (i=0; i<MSMS_BUF_LEN; i++)
    {
        if (msms[i].len == 0)
        {
            return (i);
        }
    }

    return (-1);

}

s16 ms_find_in_buf (u8 id, u8 order)
{
    u16 i;
    for (i=0; i<MSMS_BUF_LEN; i++)
    {
        if ((msms[i].id==id) &&
            (msms[i].order_num==order))
        { 
            if (msms[i].buffer != NULL)
                return (i);
        }
    }
    return (-1);
}

u16 ms_incomming (ascii *sms_text, u16 len, u8 id, u8 order, u8 total)
{
    s16 i;
    // some next part of some SMS arrived 
 
    i = ms_get_free_buf(id);
    if (i<0)
    {   // no space in buffer 
        return (MS_SMS_ERROR);
    }
    if (len > 160)
    {   // that is some nonsense, should never happen 
        return (MS_SMS_ERROR);
    }
    if ((msms[i].buffer = (ascii *)OS_MEM_ALLOC (len+1)) == NULL)
        return (MS_SMS_ERROR);

    memcpy (msms[i].buffer, sms_text, len+1);
    LOG_DEBUGL(LOG_SELECT_MS, "msms.len=%d", len);
    msms[i].len = len;
    msms[i].order_num = order;
    msms[i].total = total;
    msms[i].id = id;

    // check if all parts received 
    for (i=1; i<=total; i++)
    {
        if (ms_find_in_buf (id, i) < 0)
            return (MS_SMS_WAIT);
    }

    // yes, all parts ready
    return (MS_SMS_DONE);
}

u16 ms_get (ascii **sms_text, u8 id)
{   // join all parts into single message 
    // sms_text is pointer to pointer to be able reallocate 
    u16 i;
    u16 num=0;
    u16 len=0;
    s16 mem;
    ascii *destptr;

    // check how much space is needed
    for (i=0; i<MSMS_BUF_LEN; i++)
    {
        if (msms[i].len)
        {
            if (msms[i].id==id)
            {
                len+=msms[i].len;
                num=msms[i].total; 
            }
        }
    }
    if (len==0)
    {
        LOG_ERROR("zero length");
        return (0);
    }

    LOG_DEBUGL(LOG_SELECT_MS, "space needed %d", len+1);

    // reallocate the buffer
    OS_MEM_FREE (*sms_text);
    if ((*sms_text = (ascii *)OS_MEM_ALLOC (len+1)) == NULL)
    {
        LOG_ERROR("allocate memory");
        return (0);
    }
    **sms_text = '\0';

    destptr = *sms_text;

    for (i=1; i<=num; i++)
    {   
        mem=ms_find_in_buf (id, i);
        if (mem<0)
        {
            LOG_ERROR("connecting SMS parts");
            return (0);
        }
        memcpy (destptr, msms[mem].buffer, msms[mem].len);
        LOG_DEBUGL(LOG_SELECT_MS, "build part %d", i);
        destptr+=msms[mem].len;
        *destptr='\0';
        OS_ASSERT ((destptr - *sms_text) <= len, "ms_get() OVERFLOW ");
    }
    return (len);
    

}

void ms_delete (u8 id)
{
    u16 i;
    for (i=0; i<MSMS_BUF_LEN; i++)
    {
        if (msms[i].id==id)
        {
            msms[i].id=0;
            msms[i].order_num=0;
            OS_MEM_FREE (msms[i].buffer);
            msms[i].buffer=NULL;
            msms[i].len=0;
        }
    }
}
