#include "common.h"
#include "server.h"

#define _BUF_SIZE 1024

static bool _connected = false;
static u8 _tx_buf[_BUF_SIZE];
static size_t _tx_buf_rd_idx = 0;
static size_t _tx_buf_wr_idx = 0;

static void _connect_task(void)
{
}

void server_init(void)
{
}

void server_send(const void *buf, size_t count)
{   // WARNING: low level printf output, must keep minimal
    u8 *ptr = (u8 *)buf;
    if (! _connected)
        return;
    
    while (count--)
    {
        size_t idx;
        idx = _tx_buf_wr_idx + 1;
        if (idx >= _BUF_SIZE)
            idx = 0;
        if (idx == _tx_buf_rd_idx)
            return; // owerflow 
        _tx_buf[idx] = *ptr++;
        _tx_buf_wr_idx = idx;
    }
}

void server_task(void)
{
    if (_tx_buf_wr_idx == _tx_buf_rd_idx)
        return;

    if (! _connected)
    {
        _connect_task();
        return;
    }
}
