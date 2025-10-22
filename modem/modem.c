#include "common.h"
#include "modem.h"
#include "modem_main.h"
#include "modem_at.h"
#include "app.h"
#include "const.h"

#include "parse.h"
#include "cfg.h"
#include "log.h"

LOG_DEF("MODEM");

#if (MODEM_ENABLED == 1)

static const modem_config_t MODEM_CONFIG[] = {
	{"AT+CFUN?",       "+CFUN: 1",      "AT+CFUN=1",      MODEM_TIMEOUT_S},
	{"AT+CMEE?",       "+CMEE: 1",      "AT+CMEE=1",      MODEM_TIMEOUT_S}, // report error numbers
	{"AT+CMGF?",       "+CMGF: 0",      "AT+CMGF=0",      MODEM_TIMEOUT_S}, // switch SMS to PDU

	{NULL, NULL, "AT+CGATT=0", MODEM_TIMEOUT_M}, // dont connect data yet
	{NULL, NULL, "AT+CEREG=1", MODEM_TIMEOUT_S},

	{NULL,NULL,NULL,0} // end of table
};


static void _set_offline(modem_t *m)
{
	m->flags &= ~( MODEM_FLAG_NET_READY | MODEM_FLAG_DATA_READY|
	               MODEM_FLAG_ROAMING | MODEM_FLAG_CALL_STOP | MODEM_FLAG_LTE );
	m->error_counter+=1;
	m->signal_level=0;
}

static u8 _reg_parse(const char *p)
{
    int l = strlen(p);
    u8 reg = 0;

    if (l == 1)
    {
        reg = p[0] - '0';
    }
    else if ((l == 3) && (p[1] == ','))
    {
        reg = p[2] - '0';
    }
    return (reg);
}

static u8 _signal_level_correction (u8 level)
{	// puvodni rozsah 0..31 prevedu na 0..100
	const u8 GSM_LEVEL_TABLE[]={0,10,20,30,40,50,60,70,80,90,100};
	u8 tmp;

	tmp= (level+2)*10/31;
	if (tmp>10) tmp=10;

	return (GSM_LEVEL_TABLE[tmp]);
}

static bool _ok_test(modem_t *m)
{
	return(modem_at_ok_cmd (m, "!AT"));
}

static bool _modem_ready(modem_t *m)
{
	if (_ok_test(m))
		return (true);

	OS_DELAY(500);
	
	if (_ok_test(m))
		return (true);
	return (false);
}

static bool _check_pin (modem_t *m)
{
	int retry=5;

	while (retry--)
	{
		modem_at_ok_cmd (m, "AT+CPIN?");
		if (m->flags & MODEM_FLAG_PIN_READY)
		{	// PIN OK
			return (true);
		}
		OS_DELAY (500);
	}
	return (false);
}

static bool _init_sim(modem_t *m, const ascii *pin)
{
	if (_check_pin(m))
	{
		m->sim = MODEM_SIM_ST_PIN_OFF_READY;
		return (true);
	}
	// TODO: need to check number of attempts to enter PIN
	//       and then enter PIN only in case there is 3 attempts left 
	// for BG95 it is AT+QPINC?	resp: +QPINC: "SC",3,10 \r\n  +QPINC: "P2",3,10
	return (false);
}

static void _power_off(modem_t *m)
{
 	modem_at_lock(m);	
	_set_offline(m);
	m->pfunc_off();
	m->sim = MODEM_SIM_ST_PIN_INIT_RQ;
 	modem_at_unlock(m);
}

char *modem_parse_pattern(const char *s, const char *pattern)
{
	char *p = parse_pattern(s, pattern);

	if (p == NULL)
		return (NULL);
	
	while (*p == ' ')
	{
		p++;
	}
	return (p);
}

void modem_hw_init(modem_t *m)
{
    m->pfunc_io_init();
    modem_at_init(m);
}

void modem_hw_on(modem_t *modem)
{
    modem->pfunc_on();
}

void modem_hw_off(modem_t *modem)
{
    modem->pfunc_off();
}

void modem_rx_task(modem_t *modem)
{
    u8 ch;
    while (modem->pfunc_rx_char(&ch))
    {
        modem_at_rx(modem, ch);
    }
}

bool modem_parse_urc(modem_t *m)
{
	u32 tmp;
	char *src = m->at.rx_buf;
	const char *p = NULL;

	// LOG_DEBUG("urc: %s", src);

	if ((p = modem_parse_pattern(src, "+CME ERROR:")) != NULL)
	{
		switch (atoi(p))
		{
		case 13:  // SIM failure
			m->flags &= ~(MODEM_FLAG_PIN_READY);
			m->sim = MODEM_SIM_ST_ERROR;
			_set_offline(m);
			break;

		case 515: // wait
			break;

		default:
			m->error_counter++;

		}
		m->at.flags |= AT_ST_ERROR;
		m->at.last_error_result = atoi(p);
		return(true);
	}

	if ((p = modem_parse_pattern(src, "+CMS ERROR:")) != NULL)
	{
		// LOG_INFO(src);
		m->error_counter+=2;
		m->at.last_error_result = atoi(p);
		m->at.flags |= AT_ST_ERROR;
		return(true);
	}

	if ((p = modem_parse_pattern(src, "+CREG:")) != NULL)
	{ 	// network status
		// unsolicited "+CREG: n" where 'n' is new status
		// but response "+CREG: a,n", where 'a' mode
		u8 creg = _reg_parse(p);

		switch (creg)
		{
		case 0:
			LOG_DEBUGL(1, "NETWORK FAILED");
			_set_offline(m);
			break;

		case 1:
			LOG_DEBUGL(6, "NETWORK READY");
			m->flags |= MODEM_FLAG_NET_READY;
			m->flags &= ~(MODEM_FLAG_ROAMING);
			break;

		case 2:
			LOG_DEBUGL(2, "NETWORK SEARCHING");
			_set_offline(m);
			break;

		case 3:
			LOG_DEBUGL(1, "NETWORK DENIED");
			_set_offline(m);
			break;

		case 4:
			LOG_WARNING("NETWORK UNKNOWN STATE");
			_set_offline(m);
			break;

		case 5:
			LOG_DEBUGL(1, "ROAMING OK");
			m->flags |= (MODEM_FLAG_NET_READY | MODEM_FLAG_ROAMING);
			break;

		default:
			LOG_ERROR("NETWORK STATE ERROR");
			m->error_counter++;
			break;
		}
		return(true);
	}

	if ((p = modem_parse_pattern(src, "+CEREG:")) != NULL)
	{	// LTE network status
		u8 cereg = _reg_parse(p);
		switch (cereg)
		{
		case 1:	// home network
		case 5:	// roaming
			m->flags |= MODEM_FLAG_LTE;
			break;

		default:
			m->flags &= ~MODEM_FLAG_LTE;

		}
		return(true);
	}
	
	if ((p = modem_parse_pattern(src, "+CGREG:")) != NULL)
	{	// GPRS state
		u8 cgreg = _reg_parse(p);
		switch (cgreg)
		{
		case 1:	// home network
		case 5:	// roaming
			m->flags |= MODEM_FLAG_DATA_READY;
			break;

		default:
			m->flags &= ~MODEM_FLAG_DATA_READY;

		}
		return(true);
	}

	if ((p = modem_parse_pattern(src, "+CMTI:")) != NULL)
	{ 	// incoming SMS, +CMTI: "SM",2
		if (modem_parse_pattern(p, "\"ME\""))
		{	// sometimes stored to ME even if disabled, wtf ?
			LOG_WARNING("SMS storage \"ME\"");
			m->sms_storage_me=true;
		}
		else
		{
			m->sms_storage_me=false;
		}
		m->error_counter=0;
		app_main_led_single(APP_LED_SMS_INCOMMING);
		m->sms_counter = atoi(modem_at_param_pos(p,1));
		
		// SARA-R4 a SIM5320 give "0", wtf ?
 		if (m->sms_counter == 0)
 			m->sms_counter++;

		LOG_DEBUGL(3, "New SMS,n=%d",m->sms_counter);
		return(true);
	}

	if ((p = modem_parse_pattern(src, "+CDSI:")) != NULL)
	{	// SMS delivery report, +CDSI: "SM",1
		// mode "DS=2" (AT+CNMI=2,1,0,2,0) // does not work on SIMCOM

		m->sms_counter = atoi(modem_at_param_pos(p,1));
		LOG_DEBUGL(3, "New DS,n=%d", m->sms_counter);
		return(true);
	}

	/*if ((p = modem_parse_pattern(src, "+CDS:")) != NULL)
	{	// SMS delivery report in mode "DS=1" (AT+CNMI=2,1,0,1,0)
		// +CDS: 33
		// 079124608123451806AC0C91247037343898115052316354801150523163948000

		m->unso_pdu_rx=1;
		LOG_DEBUGL(3, "New DS,n=%d", m->sms_counter);
		return(true);
	}*/

	if ((p = modem_parse_pattern(src, "+CPMS:")) != NULL)
	{	// SMS memory SM(SR) usage
		// "AT+CPMS?"     --> "+CPMS: "SM",1,25,"SM",1,25,"SM",1,25"
		// "AT+CPMS="SM"" --> "+CPMS: 0,25,0,25,0,25"
		if (m->sms_counter)
		{
			LOG_WARNING("OLD SMS,n=%d", m->sms_counter);
			return(true);
		}

		m->sms_counter = atoi(p);
		return(true);
	}

	if ((!stricmp(src, "NO CARRIER")) || 	//
		(!stricmp(src, "BUSY"))	   || 	//
		(!stricmp(src, "NO ANSWER")))    	//
	{
		m->flags |= MODEM_FLAG_CALL_STOP;
		// modem_call_end (m);
		return(true);
	}

 	if ((p = modem_parse_pattern(src, "RING")) != NULL)
	{	// if CLIP does not work, use CLCC
		m->error_counter=0; // incomming call, it means MODEM is OK
		app_main_led_single(APP_LED_CALL_INCOMMING);
		m->flags |= MODEM_FLAG_CLCC_RQ;
		return(true);
	}
 	if ((p = modem_parse_pattern(src, "+CLIP:")) != NULL)
	{	// Calling line identity presentation
		// +CLIP: "+420777123456",145,"",,"TEL1",0
		ascii phone[MAX_PHONENUM_LEN];
		m->error_counter=0;
		app_main_led_single(APP_LED_CALL_INCOMMING);
		LOG_DEBUGL(1, src);
		parse_string(phone, p, sizeof(phone));
		// modem_call_incomming (m, phone);
		return(true);
	}

 	if ((p = modem_parse_pattern(src, "+CSQ:")) != NULL)
	{	// gsm signal level 0..31, 99
		tmp = atoi(p);
		if (tmp<=31)
		{	//
			m->signal_level=_signal_level_correction(tmp); // (tmp*100/31);
		}
		return(true);
	}

/* 	if ((p = modem_parse_pattern(src, "+CLCC:")) != NULL)
	{	// List current calls, +CLCC: 1,0,x,0,0,"+420777123456",129
		// x: 0=active, 1=held, 2=dialing, 3=alerting, 4 =incomming, 5=waiting
		// tmp = atoi(m->at_rx_buf+11);
		ascii phone[MAX_PHONENUM_LEN] = {0};
		
		p = modem_at_param_pos(src, 5);
		parse_string(phone, p, sizeof(phone));

		tmp = atoi(modem_at_param_pos(src,2));
		switch (tmp)
		{
		case 0: // active
			modem_call_update_state(m, CALL_ESTABLISHED);
			break;
		case 2: // dialing
			modem_call_update_state(m, CALL_DIALING);
			break;
		case 3: // alerting 
			modem_call_update_state(m, CALL_ALERTING);
			break;
		case 4: // incomming
			// modem_call_incomming (m, phone);
			break;
		default:
			break;
		}
		return(true);
	} */

 	if ((p = modem_parse_pattern(src, "+CPIN:")) != NULL)
	{
		if (!stricmp(p, "READY"))
		{
			m->flags |= MODEM_FLAG_PIN_READY;
		}
		else
		{
			m->flags &= ~MODEM_FLAG_PIN_READY;
		}
		return(true);
	}

	if (m->pfunc_urc != NULL)
	{
		return (m->pfunc_urc(m));
	}
	return (false);
}

void modem_tx_data(modem_t *m, const u8 *data, size_t len)
{
	while (len--)
	{
		m->pfunc_tx_char(*data);
		data++;
	}
}


bool modem_config_table(modem_t *m, const modem_config_t *table)
{
	const modem_config_t *p = table;
	bool cfg_ok = true;

	if (p == NULL)
		return(false);

 	modem_at_lock(m);

	while (p->set != NULL)
	{
		if (p->ask != NULL)
		{
			OS_DELAY(100);
			modem_at_cmd_nolock (m, p->ask);
			if (! (modem_at_response (m, p->result, AT_ST_OK | AT_ST_ERROR, p->timeout) & AT_ST_OK))
			{
				LOG_WARNING("no response to \"%s\"", p->ask);
				cfg_ok = false;
				break;
			}
			if ((m->at.flags & AT_ST_USER_STR) == 0)
			{
				LOG_WARNING("bad response to \"%s\"", p->ask);
				cfg_ok = false;
				break;
			}
		}
		p++;
	}
	modem_at_unlock(m);

	// second round for commands without reading state
	p = table;
	while (p->set != NULL)
	{
		if (p->ask == NULL)
		{	// now process which was not processed in first round
			OS_DELAY(100);
			if (! modem_at_ok_cmd(m, p->set))
			{
				LOG_ERROR("cfg \"%s\" failed", p->set);
			}
		}
		p++;
	}

	if (cfg_ok)
		return (true);

	LOG_WARNING("reconfig");
	p = table;

	while (p->ask != NULL)
	{
		if (p->set != NULL)
		{
			if (! modem_at_ok_cmd(m, p->set))
			{
				LOG_ERROR("cfg \"%s\" failed", p->set);
			}
		}
		p++;
	}
	return (true);
}

bool modem_start(modem_t *m)
{
	int repeat;

	_set_offline(m);
	m->flags &= ~(MODEM_FLAG_PIN_READY);

	if (! _modem_ready(m))
		return (false);

	repeat=3;
	while (repeat)
	{
		repeat--;

		if (! modem_at_ok_cmd(m, "!ATE0"))	// switch off local echo
			continue;

		/*if (m->model == MODEM_MODEL_UNKNOWN)
		{
			if (! modem_main_detect_type(m))
				continue;
		}*/
		if ((! modem_at_ok_cmd(m, "AT+CREG=1")) ||	// give network status changes
			(! modem_at_ok_cmd(m, "AT+CGREG=1")))	// give DATA status change
			continue;

		if (! _init_sim(m, "1234"))
		{
			LOG_ERROR("SIM init failed");
			break;
		}

		if (! modem_config_table(m, MODEM_CONFIG))
			continue;

		// modem_at_ok_cmd(m, "@AT+CLIP=1"); // get CLIP instead of "RING" // does not work on BG95

		if (! modem_at_ok_cmd(m, "!AT+CNMI=2,1,0,2,0"))
		{	// give info about incomming SMS
			if (! modem_at_ok_cmd(m, "AT+CNMI=2,1,0,1,0")) // some modems cant use CNMI ds=2
				continue;

			LOG_ERROR("CNMI using ds mode 1");
		}

		if (m->pfunc_init != NULL)
		{
			m->pfunc_init(m);
		}
		LOG_DEBUGL(1, "init OK");

		modem_at_ok_cmd(m, "AT+CREG?");
		return (true);
	}
	LOG_ERROR("init failed");
	return (false);
}

bool modem_check(modem_t *m)
{
	if (! _ok_test(m))
	{
		OS_DELAY(100);
		if (! _ok_test(m))
		{	// try again to be sure 
			LOG_ERROR("modem check FAILED,1");
			return (false);
		}
	}
	if (m->error_counter >= 30)
	{
		LOG_ERROR("error_counter reached %d", m->error_counter);
		m->error_counter = 0;
		_power_off(m);
		return (false);
	}
	if (! modem_at_ok_cmd (m, "!AT+CREG?;+CEREG?"))
		goto error;

	if (! modem_at_ok_cmd (m, "!AT+CSQ;+CGREG?;+CPIN?"))
		goto error;

	if (! modem_at_ok_cmd (m, "!AT+CPMS=\"SM\",\"SM\",\"SM\""))
		goto error;

	return (true);

error:
	LOG_ERROR("modem check FAILED,2");
	return (false);
}

void modem_data_rx_process(modem_t * m)
{
	if (m->flags & MODEM_FLAG_DATA_READY)
	{
		if (m->pfunc_udp_rx_task != NULL)
		{
			m->pfunc_udp_rx_task(m);
		}
	}
}

bool modem_apn_init(modem_t *m, const ascii *apn)
{
	ascii buf[64];
	
	modem_at_ok_cmd(m, "@AT+CGACT=0,1"); // <state>,<cid>
	
	snprintf(buf, sizeof(buf), "AT+CGDCONT=1,\"IP\",\"%s\"", apn);
	// AT+CGDCONT=<cid>[,<PDP_type>[,<APN>[,<PDP_addr>[,<data_comp>[,<head_comp>[,<IPv4AddrAlloc>]]]]]]

	if (! modem_at_ok_cmd(m, buf))
		return (false);

	if (! modem_at_ok_cmd(m, "@@AT+CGACT=1,1"))
		return (false);

	return (true);
}

bool modem_data_connect(modem_t *m)
{
	os_timer_t timeout;

	if (! modem_at_ok_cmd(m, "@@AT+CGATT=1"))
		return (false);

	timeout = os_timer_get() + 10*OS_TIMER_SECOND;
 	modem_at_lock(m);
	while (os_timer_get() <= timeout)
	{
		modem_at_cmd_nolock (m, "AT+CGATT?");
		if (modem_at_response (m, "+CGATT: 1", AT_ST_OK | AT_ST_ERROR, MODEM_TIMEOUT_S) & AT_ST_USER_STR)
			break;

		OS_DELAY(500);
	}
 	modem_at_unlock(m);

	timeout = os_timer_get() + 10*OS_TIMER_SECOND;
	while (os_timer_get() <= timeout)
	{
		modem_at_ok_cmd(m, "!AT+CGREG?");

		if (m->flags & MODEM_FLAG_DATA_READY)
		{
			if (m->pfunc_udp_init != NULL)
				return (m->pfunc_udp_init(m));
			return (true);
		}

		OS_DELAY(500);
	}

	return (false);
}

void modem_data_disconnect(modem_t *m)
{
	if ((m->flags & MODEM_FLAG_DATA_READY) == 0)
		return;

	modem_at_ok_cmd(m, "@AT+CGATT=0");
	m->flags &= ~(MODEM_FLAG_DATA_READY);
}

#else //  (MODEM_ENABLED == 1)

//dummy functions
bool modem_hw_init(modem_t *modem)
{
    LOG_INFO("HW OFF");
}

void modem_start(modem_t *modem)
{
}

#endif // (MODEM_ENABLED != 1)

