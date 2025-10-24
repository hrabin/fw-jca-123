#include "common.h"
#include "cfg.h"
#include "hw_info.h"
#include "log.h"
#include "main.h"
#include "net.h"
#include "storage.h"
#include "update.h"
#include "util.h"


LOG_DEF("UPDATE");

#define _DATA_CHUNK_SIZE 256
#define _SERVER_TMOUT (2000 * OS_TIMER_MS)

static os_timer_t _task_timer = 0;
static os_timer_t _now;

static bool _update_rq = false;
static bool _packet_ack = false;
static ip_addr_t _server_ip;
static u16 _server_port;
static u8 _aes_key[32];
static int _phase;

#define _ERR_LIMIT (100)
static int _err_cnt  = 0;


static void _dly(u32 sec)
{
    _task_timer = _now + sec * OS_TIMER_SECOND;
}

static bool _wait_for_ack(void)
{
    os_timer_t start;
    _now = os_timer_get();
    start = _now;

    while (_now < (start + _SERVER_TMOUT))
    {
        OS_DELAY(10);
        _now = os_timer_get();

        
        if (! _packet_ack)
            continue; // no response yet
        
        // yes, we have got response
        _packet_ack = false;
        return (true);
    }
    LOG_WARNING("NO ACK");
    return (false);
}

static bool _send_packet(u8 *data, size_t len)
{
    udp_packet_t packet;
    packet.src_port    = _server_port;
    packet.dst_port    = _server_port;
    packet.dst_ip.addr = _server_ip.addr;
    packet.datalen     = len;
    packet.data        = data;

    _packet_ack = false;

    if (! net_udp_tx (&packet))
        return (false);

    if (_wait_for_ack())
    {
        _dly(0);
        return (true);
    }
    _dly(5);
	return (false);
}

static bool _update_req(void)
{
    update_rq_packet_t data;

    data.hdr.packet_id = PACKET_ID_UPDATE_REQ;
    data.hdr.unit_id = HW_INFO.addr;
    data.ver = (SW_VERSION_MAJOR<<16) + (SW_VERSION_MINOR << 8) + SW_VERSION_PATCH;
	
	return (_send_packet((u8 *)&data, sizeof(data)));
}

static bool _update_phase(void)
{
    update_rq_data_packet_t data;
    
    data.hdr.packet_id = PACKET_ID_GET_CHUNK;
    data.hdr.unit_id = HW_INFO.addr;
    data.chunk_id = _phase-1;

	return (_send_packet((u8 *)&data, sizeof(data)));
}

static void _write_chunk(u32 chunk_id, u8 *data)
{
    u32 offset = chunk_id * UPDATE_CHUNK_SIZE;

	if (! storage_write_fw(offset, data, UPDATE_CHUNK_SIZE))
		LOG_ERROR("Write failed to 0x%x", offset);
}

bool update_packet_rx (u8 *data, u16 len, u16 port)
{
    if (port != _server_port)
        return (false);

    if (!_update_rq)
        return (false);

    if (len < sizeof(update_hdr_t))
        return (false);

    update_hdr_t *hdr = (update_hdr_t *)data;

    if (hdr->unit_id != HW_INFO.addr)
        return (false);

    switch (hdr->packet_id)
    {
    case PACKET_ID_UPDATE_RES:
        if (_phase == 0)
        {
            update_res_packet_t *res = (update_res_packet_t *)data;

            if (res->result != UPDATE_RESULT_START)
            {
                LOG_INFO("no FW");
                _update_rq = false;
            }
        }
        break;

    case PACKET_ID_CHUNK_RES:
        {
            update_res_data_packet_t *res = (update_res_data_packet_t *)data;
            u32 chunk_id = _phase-1;

            if (len < sizeof(*res))
                return (false);

            if (res->chunk_id != chunk_id)
                return (false);

            if (res->status == UPDATE_CHUNK_STATUS_OK)
            {
                _write_chunk(chunk_id, res->data);
            }
            else if (res->status == UPDATE_CHUNK_STATUS_DONE)
            {
                _write_chunk(chunk_id, res->data);
                LOG_INFO("update DONE !");
                _update_rq = false;
				main_flash();
            }
            else
            {
                return (false);
            }
        }
        break;

    default:
        return (false);
    }
    _packet_ack = true;
    return (true);
}

bool update_start(void)
{
    ascii cfg[CFG_ITEM_SIZE];
    buf_t buf;

    if (_update_rq)
    {
        LOG_ERROR("update busy");
        return (false);
    }

    buf_init(&buf, cfg, sizeof(cfg));

    if (! cfg_read(&buf, CFG_ID_UPDATE_SERVER_ADDR, ACCESS_SYSTEM))
        return (false);

    net_get_target_ip (&_server_ip.addr, &_server_port, cfg);

    buf_clear(&buf);
    if (! cfg_read(&buf, CFG_ID_UPDATE_SERVER_KEY, ACCESS_SYSTEM))
        return (false);
    
    hex_to_bin(_aes_key, cfg, sizeof(_aes_key));
#warning "TODO: AES unused"

    _err_cnt = 0;
    _phase = 0;
    _update_rq = true;
    LOG_INFO("update start OK");
    return (true);
}

void update_task(void)
{
    if (! _update_rq)
        return;

    if ( _err_cnt >= _ERR_LIMIT)
    {
        LOG_ERROR("update FAILED");
        _update_rq = false;
    }
    _now = os_timer_get();

    if (_now < _task_timer)
        return;

    _dly(1);

    if (! net_connect())
    {
        _dly(30);
        return;
    }

    LOG_DEBUG("update task %d", _phase);

    if (_phase == 0)
    {
        if (! _update_req())
        {
            _err_cnt++;
            return;
        }

        _phase++;
        return;
    }

    if (! _update_phase())
    {
        _err_cnt++;
        return;
    }

    _phase++;
    _err_cnt=0;

    if (_update_rq == false)
        LOG_INFO("update DONE");
}
