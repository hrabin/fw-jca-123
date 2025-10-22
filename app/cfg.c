#include "common.h"
#include "cfg.h"
#include "log.h"
#include "storage.h"
#include "app.h"
#include "log_select.h"

LOG_DEF("CFG");

typedef struct {
    char data[CFG_ITEM_SIZE];
    u16 chsum;
} cfg_t;

static cfg_t cfg_buf;

OS_SEMAPHORE(cfg_semaphore); 

static inline void cfg_semaphore_take (void)
{
    OS_SEMAPHORE_TAKE (cfg_semaphore);
}

static inline void cfg_semaphore_give (void)
{
    OS_SEMAPHORE_GIVE (cfg_semaphore);
}

static u16 _cfg_chsum(char *data, int len)
{
    u16 chsum = 0xaabc;

    while (len--)
    {
        chsum += *data;
        data++;
    }
    return (chsum);
}

static bool _cfg_valid(cfg_t *cfg)
{
    if (_cfg_chsum(cfg->data, CFG_ITEM_SIZE) != cfg->chsum)
        return (false);

    // TODO: check data content ?
    return (true);
}

bool cfg_init(void)
{
    OS_SEMAPHORE_INIT (cfg_semaphore);
    OS_ASSERT(sizeof(cfg_t) == 64, "sizeof(cfg_t) != 64");
    return (true);
}

bool cfg_write(cfg_id_t id, buf_t *src, access_auth_t auth)
{
    bool result = false;
    
    if (buf_length(src) > CFG_ITEM_SIZE)
        return (false);

    if (! cfg_table_access_write(id, auth))
    {
        LOG_ERROR("no WR access to %d", id);
        return (false);
    }
    LOG_DEBUGL(LOG_SELECT_CFG, "WR %d", id);

    cfg_semaphore_take();
    memset(&cfg_buf, 0, sizeof(cfg_buf));
    memcpy(cfg_buf.data, buf_data(src), buf_length(src));
    cfg_buf.chsum = _cfg_chsum(cfg_buf.data, CFG_ITEM_SIZE);
    result = storage_write_cfg(id * sizeof(cfg_t), (u8 *)&cfg_buf, sizeof(cfg_t));
    cfg_semaphore_give();
    app_reinit_req(id);
    return (result);
}

bool cfg_read(buf_t *dest, cfg_id_t id, access_auth_t auth)
{
    bool result = false;

    LOG_DEBUGL(LOG_SELECT_CFG, "RD %d", id);

    if (! cfg_table_access_read(id, auth))
    {
        LOG_ERROR("no RD access");
        return (false);
    }
        
    cfg_semaphore_take();

    if (storage_read_cfg((u8 *)&cfg_buf, id * sizeof(cfg_t), sizeof(cfg_t)))
    {
        result = true;

        if (_cfg_valid(&cfg_buf))
        {
            buf_append_str(dest, cfg_buf.data);
        }
        else
        {
            ascii tmp[CFG_ITEM_SIZE];

            result = cfg_table_default(tmp, id);
            if (result)
                buf_append_str(dest, tmp);
            else
                buf_append_fmt(dest, "<missing %d>", id);
        }
    }
    cfg_semaphore_give();
    return (result);
}

s32 cfg_read_nparam(cfg_id_t id, int n)
{   // read n-th numeric parameter from cfg
    buf_def(buf, CFG_ITEM_SIZE);
    ascii *p;

    if (! cfg_read(&buf, id, ACCESS_SYSTEM))
        return (false); 

    p = buf_data(&buf);

    while (*p != '\0')
    {
        if (n <= 0)
        {
            s32 value;
            if (sscanf(p, "%" SCNd32, &value) == 1)
                return (value);

            break;
        }
        if (*p == ',')
            n--;
        p++;
    }
    return (0);
}

