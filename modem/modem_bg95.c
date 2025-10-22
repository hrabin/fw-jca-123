#include "os.h"
#include "modem.h"
#include "parse.h"
#include "modem_at.h"
#include "modem_main.h"
#include "log.h"
#include "ip4.h"
#include "udp.h"

LOG_DEF("BG95");

#define	_RX_RETRY_TIME      (5 * OS_TIMER_SECOND)
#define	_SOCKET_TIMEOUT (15*60 * OS_TIMER_SECOND)

static os_timer_t timer_rx_timeout = 0; // timeout reset by incoming data
static u16 tx_cnt = 0; // number of sent packets from last received packet

typedef struct {
	os_timer_t timer;
	os_timer_t tmout; // socket timeout for close
	os_timer_t rx_time; // last rx packet time
	int         no_rx_cnt;
	ip_addr_t   ip;
	u16         port;
	bool        rx;
	bool 		ready;
} udp_socket_t;

#define	_SOCKETS (5)
static udp_socket_t udp_socket[_SOCKETS];
static u8 rx_udp_socket = 0;

/*
/ATI 
|Quectel
|BG95-M3
|Revision: BG95M3LAR02A03
 
/AT+QCFG=? 
|+QCFG: "nwscanmode",(0,1,3),(0,1)
|+QCFG: "servicedomain",(1,2),(0,1)
|+QCFG: "nwscanseq",(00-010203),(0,1)
|+QCFG: "band",(0-F),(0-0x100002000000000F0E189F),(0-0x10004200000000090E189F),(0,1)
|+QCFG: "bandrestore"
|+QCFG: "iotopmode",(0-2),(0,1)
|+QCFG: "celevel",(0-2)
|+QCFG: "urc/ri/ring",("off","pulse","always","auto","wave"),(1-2000),(1-10000),(1-10000),("off","on"),(1-5)
|+QCFG: "urc/ri/smsincoming",("off","pulse","always"),(1-2000),(1-5)
|+QCFG: "urc/ri/other",("off","pulse"),(1-2000),(1-5)
|+QCFG: "risignaltype",("respective","physical")
|+QCFG: "urc/delay",(0,1)
|+QCFG: "ledmode",(0,1,3)
|+QCFG: "gpio",<mode>,<pin>[,[<dir>,<pull>,<drv>]/[<val>][,<save>]]
|+QCFG: "airplanecontrol",(0,1)
|+QCFG: "cmux/urcport",(0-4)
|+QCFG: "apready",(0,1),(0,1),(100-3000)
|+QCFG: "nccconf",(0-1FF)
|+QCFG: "psm/enter",(0,1)
|+QCFG: "psm/urc",(0,1)
|+QCFG: "simeffect",(0,1)
|+QCFG: "lapiconf",(0-2),(0,1)
|+QCFG: "snrscan"[,(0-2)]
|+QCFG: "uartcfg",(0-15)
|+QCFG: "nasconfig",(0-7FFF)
|+QCFG: "irat/timer",(5-300),(5-20)
|+QCFG: "nb1/bandprior",<band_priority_seq>
|+QCFG: "bip/auth",(0-3)
|+QCFG: "timer",(3402)
|+QCFG: "timeupdate",(0,1)
|+QCFG: "emmcause"[,(0,1)]
|+QCFG: "sibinfo"
|+QCFG: "emmtimer"
|+QCFG: "msclass"[,((1-18)|(30-34)),(1-34)]
|+QCFG: "fgiconfig"[,(0-FFFFFFFF)]
|+QCFG: "sim/onchip"[,(0,1)[,(0,1)]]
|+QCFG: "cmux/signal",(26,49),(0,1),(0,1)
|+QCFG: "timesave",(0-2)
|+QCFG: "msc",(0-2)
|+QCFG: "sgsn",(0-2)
|+QCFG: "lte/bandprior",(1-43),(1-43),(1-43)
|+QCFG: "pa_info",<model>,<MID>,<PID>
|+QCFG: "cmux/flowctrl",(0,1)
|+QCFG: "fast/poweroff",<pin>,(0,1)
|+QCFG: "dbgctl",(0-2)
|+QCFG: "psm_rtc_adjust_ctrl",(0,1)
|OK
*/

#define	_CFG_SCAN_SEQ   "020103"  // 01 GSM, 02 eMTC 03 NB-IoT (default 020301)
#define	_CFG_SCAN_MODE  "1"  // 0 automatic, 1 GSM, 2 LTE, 3 LTE/NB (default ?)

static const modem_config_t MODEM_BG95_CONFIG[] = {
	{"AT+QURCCFG=\"urcport\"", "+QURCCFG: \"urcport\",\"uart1\"",      "AT+QURCCFG=\"urcport\",\"uart1\"", MODEM_TIMEOUT_S},
//  {"AT+QCFG=\"nwscanmode\"",  "+QCFG: \"nwscanmode\"," _CFG_SCAN_MODE,  "AT+QCFG=\"nwscanmode\"," _CFG_SCAN_MODE ",1", MODEM_TIMEOUT_S},
	{"AT+QCFG=\"nwscanseq\"",  "+QCFG: \"nwscanseq\"," _CFG_SCAN_SEQ,  "AT+QCFG=\"nwscanseq\"," _CFG_SCAN_SEQ ",1", MODEM_TIMEOUT_S},
	
	// some setup without response check
	{NULL,NULL,"AT+QSCLK=1",MODEM_TIMEOUT_S}, // enable sleep mode (DTR wakeup)
	{NULL,NULL,"AT+QJDR=1",MODEM_TIMEOUT_S}, // enable jamming detection

	{NULL,NULL,NULL,0} // end of table
};

static buf_t *_get_at_buf(void)
{
	static buf_t buf;

	if (! buf_init(&buf, NULL, 2028))
	{
		OS_ASSERT(false, "buf init");
	}
	return (&buf);
}

static void _return_at_buf(buf_t *buf)
{
	buf_free(buf);
}


bool modem_bg95_init(modem_t *m)
{
	LOG_DEBUGL(1, "init");
	bzero(&udp_socket, sizeof(udp_socket));

	if (! modem_config_table(m, MODEM_BG95_CONFIG))
		return (false);

	return (true);
}

bool modem_bg95_check(modem_t *m)
{
	return (true);
}

static void _udp_rx_hex(u8 socket, ascii *data, size_t len)
{
	buf_t *buf;
	int i, c;

	if (socket >= _SOCKETS)
	{
		LOG_ERROR("bad socket %d", socket);
		return; 
	}
	
	buf = _get_at_buf();
	
	// convert HEX to BIN
	for (i=0; i<len; i++)
	{
		if (sscanf(data, "%02x", &c) != 1)
			break;
		buf_append_char(buf, c);
		data+=2;
	}
	if (i == len)
	{
		udp_rx ((u8 *)buf_data(buf), len, &udp_socket[socket].ip, udp_socket[socket].port);
	}
	else
	{
		LOG_ERROR("RX %d != %d", i, len);
	}
	_return_at_buf(buf);
}

static void _socket_clear(void)
{
	bzero(&udp_socket, sizeof(udp_socket));
}

bool modem_bg95_urc(modem_t *m)
{
	os_timer_t now = os_timer_get();
	static size_t rx_udp_len = 0;

	char *src = m->at.rx_buf;
	char *p = NULL;

	if (rx_udp_len)
	{
		if (m->at.buf_len == 2*rx_udp_len+1) // HEX + '\0'
		{
			_udp_rx_hex(rx_udp_socket, src, rx_udp_len);
			udp_socket[rx_udp_socket].rx = true;
			udp_socket[rx_udp_socket].timer = now + _RX_RETRY_TIME;
			udp_socket[rx_udp_socket].no_rx_cnt = 0;
			udp_socket[rx_udp_socket].rx_time = now;

			timer_rx_timeout = now + 15*OS_TIMER_SECOND;
			tx_cnt = 0;
		}
		else
		{
			LOG_DEBUGL(1, "UDP: %d != %d", m->at.buf_len, 2*rx_udp_len+1);
		}
		rx_udp_len = 0;
		return (true);
	}

	if ((p = modem_parse_pattern(src, "+QIURC: \"recv\",")) != NULL)
	{	// +QIURC: "recv",0
	    parse_number_t n;

	    if (parse_number(&n, p) != NULL)
		{
			if ((n < _SOCKETS) && (n>=0))
			{	// cant send directly AT+QIRD, it may collide with AT+QISENDEX and it fails
				// TODO: may be improved by semaphore for RX
				udp_socket[n].rx = true;
			}
		}
		return (true);
	}
	if ((p = modem_parse_pattern(src, "+QIRD:")) != NULL)
	{	// incoming data
	    parse_number_t n;

	    if (parse_number(&n, p) != NULL)
		{
			if (n > 0)
			{
				rx_udp_len = n;
			}
			else
			{
				udp_socket[rx_udp_socket].rx = false;
			}
		}
		else
			LOG_ERROR("parse failed \"%s\"", p);

		return (true);
	}

	if ((p = modem_parse_pattern(src, "+QIURC: \"pdpdeact\",1")) != NULL)
	{
		m->flags &= ~MODEM_FLAG_DATA_READY;
		_socket_clear();
		return (true);
	}
	if ((p = modem_parse_pattern(src, "+QIOPEN: ")) != NULL)
	{	// +QIOPEN: <socket>,<result> // result 0 == OK
	    parse_number_t n;

	    if ((p = parse_number(&n, p)) != NULL)
		{
			u8 socket = n;

			if ((p = parse_modem_separator(p)) == NULL)
				return (true);

   			if ((p = parse_number(&n, p)) != NULL)
			{
				if ((n == 0) && (socket<_SOCKETS))
				{
					udp_socket[socket].ready = true;
				}
			}
		}
		return (true);
	}
	if ((p = modem_parse_pattern(src, "POWERED DOWN")) != NULL)
	{
		LOG_INFO("POWER DOWN");
		m->flags &= ~(MODEM_FLAG_NET_READY | MODEM_FLAG_DATA_READY|
	                  MODEM_FLAG_ROAMING | MODEM_FLAG_CALL_STOP | MODEM_FLAG_LTE );
		return (true);
	}
	if ((p = modem_parse_pattern(src, "+QENG: \"servingcell\",")) != NULL)
	{
		// TODO: parse statistics 
		// In the case of GSM mode:
		// +QENG: "servingcell",<state>[,<RAT>,<MCC>,<MNC>,<LAC>,<cellID>,<bsic>,<ARFCN>,<band>,<RxLev>,<txp>,<rla>,<DRX>,<c1>,<c2>,<GPRS>,<tch>,<ts>,<ta>,<MAIO>,<HSN>,<rxlevsub>,<rxlevfull>,<rxqualsub>,<rxqualfull>,<voicecodec>]
		// <RAT> "GSM" / "eMTC" / "NBIoT"
		// In the case of LTE Cat M1/Cat NB2 mode:
		// +QENG: "servingcell",<state>[,<RAT>,<is_tdd>,<MCC>,<MNC>,<cellID>,<PCI>,<EARFCN>,<freq_band_ind>,<UL_bandwidth>,<DL_bandwidth>,<TAC>,<RSRP>,<RSRQ>,<RSSI>,<SINR>,<srxlev>]
		return (true);
	}
	if ((p = modem_parse_pattern(src, "+QJDR: ")) != NULL)
	{
		if (strncmp(p, "NOJAMMING", 9) == 0)
			modem_main_jamming(false);
		else if (strncmp(p, "JAMMED", 6) == 0)
			modem_main_jamming(true);
		return (true);
	}
	return (false);
}

bool modem_bg95_udp_init(modem_t *m)
{
	buf_t *buf;
	bool result = false;

	bzero(&udp_socket, sizeof(udp_socket));

	if (! modem_at_response_ok(m, "AT+QIACT?", "+QIACT: 1,1"))
	{
		if (! modem_at_ok_cmd(m, "@@AT+QIACT=1")) // max timeout 150s !
			return (false);
	}
	if (! modem_at_ok_cmd(m, "AT+QICFG=\"dataformat\",1,1"))
		return (false);

	buf = _get_at_buf();

	if (modem_at_cmd_get_response(m, buf, "AT+QIDNSCFG=1", "+QIDNSCFG: 1,"))
	{	// +QIDNSCFG: 1,"123.66.165.1","123.66.165.2"
		// TODO: parse DNS IP to be able resolve names
		result = true;
	}
	timer_rx_timeout = os_timer_get() + 60*OS_TIMER_SECOND;
	tx_cnt = 0;
	_return_at_buf(buf);
	return (result);
}

static u8 _socket_id(ip_addr_t *ip, u16 port)
{
	udp_socket_t *s;

	static u8 socket = 0;
	int i;

	for (i=0; i<_SOCKETS; i++)
	{
		s = &udp_socket[i];

		if ((s->ip.addr == ip->addr) && (s->port == port))
			return (i); // socket already active
	}
	for (i=0; i<_SOCKETS; i++)
	{
		s = &udp_socket[i];
		if (s->ip.addr == 0)
			return (i); // found unused socket
	}
	if (++socket >= _SOCKETS)
		socket = 0;

	return (socket);
}

static void _socket_close(modem_t *m, u8 socket)
{
	ascii buf[32];
	snprintf(buf, sizeof(buf), "AT+QICLOSE=%d", socket); // tmout 10s (default)
	modem_at_ok_cmd(m, buf);
	bzero(&udp_socket[socket], sizeof(udp_socket_t));
}

static bool _udp_bind(modem_t *m, u8 *socket_id, ip_addr_t *ip, u16 port)
{
	os_timer_t now = os_timer_get();
	int local_port;
	u8 socket;

	bool result = false;
	buf_t *buf;
	u8 *a;
	udp_socket_t *s;

	OS_ASSERT(ip->addr != 0, "ip->addr == 0");

	socket = _socket_id(ip, port);
	*socket_id = socket;
	
	s = &udp_socket[socket];

	if ((now + _RX_RETRY_TIME) < s->timer)
		s->timer = now + _RX_RETRY_TIME;

	s->tmout = now + _SOCKET_TIMEOUT;
	s->no_rx_cnt++; // number of sent packets without anything received

	if ((s->ip.addr == ip->addr) && (s->port == port))
	{
		return (true);
	}
	
	if (s->ip.addr)
	{
		_socket_close(m, socket);
	}
	buf = _get_at_buf();

	buf_clear(buf);
	a = (u8 *)&(ip->addr);
	local_port = 8000 + socket;
	buf_append_fmt(buf, "@@AT+QIOPEN=1,%d,\"UDP\",\"%d.%d.%d.%d\",%d,%d,0", socket, a[0], a[1], a[2], a[3], port, local_port);
	// it says at first OK and then URC +QIOPEN: <n>,0
	s->ready = false;
	if (modem_at_ok_cmd(m, buf_data(buf)))
	{	// wait for URC +QIOPEN
		os_timer_t limit = now + 10*OS_TIMER_SECOND;
		while (now <= limit)
		{
			if (s->ready)
			{
				result = true;
				s->ip.addr = ip->addr;
				s->port = port;
				break;
			}
			OS_DELAY(10);
		}
	}
	_return_at_buf(buf);
	return (result);
}

bool modem_bg95_udp_send(modem_t *m, udp_packet_t *packet)
{
    buf_t *buf;
	u8 socket;
	bool result = false;

	if (packet->dst_ip.addr == 0)
		return (false);

	if (! _udp_bind(m, &socket, &(packet->dst_ip), packet->dst_port))
	{
		LOG_WARNING("retry bind");
		_socket_close(m, socket);
		OS_DELAY(100);
		if (! _udp_bind(m, &socket, &(packet->dst_ip), packet->dst_port))
		{
			LOG_ERROR("bind failed");
			return (false);
		}
	}
	if (packet->datalen > 512)
	{
		LOG_ERROR("size > 512"); // modem support max 512B
		return (false);
	}

	buf = _get_at_buf();
	// console_put_char_direct('>'); 
	tx_cnt++;
	
	buf_append_fmt(buf,"AT+QISENDEX=%d,\"", socket); // tmout 120s
    buf_append_hex(buf, packet->data, packet->datalen);
    buf_append_str(buf, "\",0");

 	modem_at_lock(m);
	modem_at_cmd_nolock(m, buf_data(buf));
	
	if (modem_at_response(m, "SEND OK", AT_ST_ERROR | AT_ST_USER_STR, MODEM_TIMEOUT_LL) & AT_ST_USER_STR)
		result = true;

 	modem_at_unlock(m);
	_return_at_buf(buf);

	if (! result)
	{
		LOG_DEBUGL(1, "send failed, close socket");
		_socket_close(m, socket);
	}
	return (true);
}

static void _socket_maintenace(modem_t *m)
{
	os_timer_t now = os_timer_get();
	int i;
	int limit;

	for (i=0; i<_SOCKETS; i++)
	{
		udp_socket_t *s = &udp_socket[i];

		if ((s->ip.addr != 0) && (s->port != 0))
		{
			if (now > s->tmout)
			{
				_socket_close(m, i);
				continue;
			}
			if ((s->no_rx_cnt  > 10) && (s->rx_time + 60*OS_TIMER_SECOND < now))
			{	// workaround (sending packets, but more than 60s nothing received)
				// looks like modem bug, it sometimes happen that is sends data but nothing received
				//
				// _socket_close(m, i); // this does not help
				LOG_ERROR("socket %d failed, reconnect", i);
				_socket_clear();
				modem_data_disconnect(m);
				continue;
			}
			if (now > s->timer)
			{
				s->rx = true;
				s->timer = now + 60*OS_TIMER_SECOND;
			}
		}

		limit = 5;
		while (s->rx && --limit)
		{	// fetch other waiting packets
			ascii buf[32];

			s->rx = false; // switch to true again by incoming data
			
			snprintf(buf, sizeof(buf), "@@AT+QIRD=%d", i); // tmout 120s
			rx_udp_socket = i;
	 		if (modem_at_ok_cmd(m, buf))
			{
				s->timer = now + 60*OS_TIMER_SECOND;
			}
			else
			{	// 
				s->ip.addr = 0;
			}
		}
	}
}

void modem_bg95_udp_rx_task(modem_t *m)
{
	_socket_maintenace(m);

	if (timer_rx_timeout)
	{
		if ((tx_cnt > 10)
		 && (os_timer_get() > timer_rx_timeout))
		{	// detected error state (sending but not receiving data)
			LOG_ERROR("data reconnect");
			_socket_clear();
			modem_data_disconnect(m);
			timer_rx_timeout = 0;
		}
	}

}

