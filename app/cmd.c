#include "common.h"

#include "alarm.h"
#include "analog.h"
#include "ble.h"
#include "buf.h"
#include "cfg.h"
#include "cmd.h"
#include "gps.h"
#include "gpreg.h"
#include "flash_lib.h"
#include "hw_info.h"
#include "io.h"
#include "log.h"
#include "modem_main.h"
#include "net.h"
#include "parse.h"
#include "power.h"
#include "rtc.h"
#include "system.h"
#include "tracer.h"
#include "util.h"
#include "update.h"
#include "wdog.h"

LOG_DEF("CMD");

#define _CMD_CHAIN_SEPARATOR ';'

#define _LOCK_TIME 400 // [ms] lock/unlock impulse time

#define _CMD CMD_DEF

#define _CMD_TABLE_VALUES \
    /* Set of system commands */ \
    _CMD("DBG",       NULL,           _cmd_dbg,     0, ACCESS_SYSTEM,   NULL), \
    _CMD("AT",        NULL,           _cmd_at,      0, ACCESS_SYSTEM,  "send AT-command"), \
    _CMD("BLE",       NULL,           _cmd_ble,     0, ACCESS_SYSTEM,  "send cmd to BLE module"), \
    _CMD("DL",        _cmd_dl,        _cmd_dl_set,  0, ACCESS_SYSTEM,  "set debug level[,select]"), \
    _CMD("ECHO",      _cmd_echo,      _cmd_echo_set,0, ACCESS_SYSTEM,  "get/set modem echo"), \
    /* Set of normal user commands */ \
    _CMD("AUTH",      _cmd_auth,      _cmd_auth_set,0, ACCESS_NONE,    "authorize channel AUTH=\"pass\""), \
    _CMD("CFG",       _cmd_cfg,       _cmd_cfg_set, 0, ACCESS_USER,    "CFG=<n>[,\"value\"]"), \
    _CMD("GPS",       _cmd_gps,       _cmd_gps_set, 0, ACCESS_USER,    "get GPS coordinates"), \
    _CMD("HELP",      _cmd_help,      NULL,         0, ACCESS_NONE,    "Show this help text"), \
    _CMD("IO",        _cmd_io,        NULL,         0, ACCESS_USER,    "get IO state"), \
    _CMD("LOCK",      _cmd_lock,      NULL,         0, ACCESS_USER,    "lock pulse"), \
    _CMD("UNLOCK",    _cmd_unlock,    NULL,         0, ACCESS_USER,    "unlock pulse"), \
    _CMD("NET",       _cmd_net,       NULL,         0, ACCESS_USER,    "Get network info"), \
    _CMD("REBOOT",    NULL,        _cmd_reboot_set, 0, ACCESS_ADMIN,   "REBOOT=<n>"), \
    _CMD("RTC",       _cmd_rtc,       NULL,         0, ACCESS_USER,    "get RTC time"), \
    _CMD("SET",       _cmd_set,       NULL,         0, ACCESS_USER,    "switch to set"), \
    _CMD("STATUS",    _cmd_status,    NULL,         0, ACCESS_USER,    "Get status info"), \
    _CMD("UNSET",     _cmd_unset,     NULL,         0, ACCESS_USER,    "switch to unset"), \
    _CMD("UPDATE",    _cmd_update,    NULL,         0, ACCESS_ADMIN,   "request to update FW from server"), \
    _CMD("VER",       _cmd_ver,       NULL,         0, ACCESS_NONE,    "Request product information"), \

static const cmd_t _CMD_TABLE[];

const char CMD_ERR_MSG_INVALID_PARAM[]   = "INVALID PARAMETER";
const char CMD_ERR_MSG_MISSING_PARAM[]   = "MISSING PARAMETER";
const char CMD_ERR_MSG_UNKNOWN_COMMAND[] = "UNKNOWN COMMAND";

typedef struct {

    OS_SEMAPHORE(mutex);
  #define _RESULT_BUF_SIZE (2048)
    char        result_buf_mem[_RESULT_BUF_SIZE];
    buf_t       result_buf; // spare memory by using one result buffer

} cmd_processing_t;

static cmd_processing_t _cmd_processing;

extern bool shock_debug;
extern void event_debug(void);

static bool _cmd_dbg(buf_t *result, const struct _cmd_t *cmd,  const char **pptext, access_t *access)
{
    long num;

    if (access->auth < ACCESS_ADMIN)
        return (false);

    if (! cmd_fetch_num(&num, pptext))
        return (false);
  
    switch (num)
    {
    case 1: return net_connect(); 
    case 2: modem_main_data_disconnect(); break;
    case 3:
        {
            udp_packet_t p;
            p.dst_ip.addr = (56<<24) + (216<<16) + (2<<8) + 81;
            p.src_ip.addr = 0; // unused
            p.data=(u8 *)"TEST";
            p.dst_port=8085;
            p.src_port=8085;
            p.datalen=5;
            return net_udp_tx(&p);
        }
    case 10:
        return (tracer_test());

    case 20: // simulate incomming SMS 
        if (! cmd_fetch_separator(pptext))
            return (false);

        // DBG=20,1234 gps
        return cmd_sms_process (result,  (ascii *)*pptext, "+420777123456", strlen(*pptext));

    case 30:
        alarm_trigger();
        break;
    case 40:
        shock_debug ^= 1;
        break;

    case 50:
        event_debug();
        break;

    default:
            break;
    }
    return (true);
}


static void _cmd_mutex_lock(void)
{
    OS_SEMAPHORE_TAKE(_cmd_processing.mutex);
}

static void _cmd_mutex_unlock(void)
{
    OS_SEMAPHORE_GIVE(_cmd_processing.mutex);
}

buf_t *cmd_get_buf(void)
{
    _cmd_mutex_lock();

    buf_clear(&_cmd_processing.result_buf);

    return (&_cmd_processing.result_buf);
}

void cmd_return_buf(buf_t *buf)
{
    OS_ASSERT(buf == &_cmd_processing.result_buf, "cmd_return_buf()");

    buf_clear(&_cmd_processing.result_buf);

    _cmd_mutex_unlock();
}

void cmd_string(buf_t *result, const char *text)
{
    buf_append_fmt(result, "\"%s\"", text);
}

void cmd_nl(buf_t *result)
{
    buf_append_str(result, CMD_NEW_LINE);
}

void cmd_reply(buf_t *result, const cmd_t *cmd)
{
    buf_append_fmt(result, "%s: ", cmd->text);
}

bool cmd_fetch_num(cmd_int_t *dest, const char **pptext)
{
    char *p;
    long num;

    if ((p = parse_number(&num, *pptext)) == NULL)
    {
        return (false);
    }

    *pptext = p;
    *dest = num;
    return (true);
}

bool cmd_fetch_separator(const char **pptext)
{
    char *p;

    if ((p = parse_separator(*pptext)) == NULL)
    {
        return (false);
    }

    *pptext = p;
    return (true);
}

bool cmd_fetch_string(char *dest, const char **pptext, size_t limit)
{
    char *p;

    if ((p = parse_string(dest, *pptext, limit)) == NULL)
    {
        return (false);
    }
    *pptext = p;
    return (true);
}

bool cmd_fetch_addr(addr_t *dest, const char **pptext)
{
    char *p;

    if ((p = parse_u64((u64 *)dest, *pptext)) == NULL)
    {
        return (false);
    }
    *pptext = p;
    return (true);
}

static void _skip_spaces(const char **pptext)
{
    while (**pptext == ' ')
    {
        (*pptext)++;
    }
}

__WEAK void cmd_main_ver(buf_t *result)
{
}

static bool _cmd_ver(buf_t *result, const cmd_t *cmd, access_t *access)
{
    buf_append_str(result, cmd->text);
    buf_append_fmt(result, ": %s v%d.%d.%d", HW_INFO.hw_name, SW_VERSION_MAJOR, SW_VERSION_MINOR, SW_VERSION_PATCH);
    cmd_main_ver(result);

    cmd_nl(result);
    return (true);
}

static bool _cmd_at(buf_t *result, const struct _cmd_t *cmd,  const char **pptext, access_t *access)
{
    modem_main_at_cmd(result, *pptext);
    return (true);
}

static bool _cmd_ble(buf_t *result, const struct _cmd_t *cmd,  const char **pptext, access_t *access)
{
    // BLE=AT+VERSION
    // BLE=AT+PIN
    
    // for binding :
    // BLE=AT+TYPE2
    
    ble_cmd(*pptext);
    return (true);
}

static void _add_state(buf_t *result)
{
    const ascii *s, *a;
    switch (system_state())
    {
    case SECTION_ST_UNSET:    s = "UNSET";         break;
    case SECTION_ST_SET_PART: s = "Partially SET"; break;
    case SECTION_ST_SET:      s = "SET";           break;
    default :                s = "failure";       break;
    }
    switch (alarm_state())
    {
    case ALARM_IDLE:         a = "OFF";     break;
    case ALARM_ACTIVE:       a = "ACTIVE!"; break;
    case ALARM_TIMEOUT:      a = "MEMORY";  break;
    default :                a = "failure"; break;
    }
    buf_append_fmt(result, "STATE %s, ALARM %s ", s, a);
}

static void _add_rtc(buf_t *result)
{
    rtc_t t;

    rtc_get_time(&t);
    buf_append_fmt(result, "%02d/%d/%d %d:%02d:%02d", t.year, t.month, t.day, t.hour, t.minute, t.second);
}

static const ascii *_cmd_auth_string(access_auth_t auth)
{
    if (auth >= ACCESS_SYSTEM)
        return ("SYSTEM");

    if (auth >= ACCESS_ADMIN )
        return ("ADMIN");

    if (auth >= ACCESS_USER )
        return ("USER");

    return ("NONE");
}

static bool _cmd_auth(buf_t *result, const cmd_t *cmd, access_t *access)
{
    buf_append_str(result, cmd->text);
    buf_append_fmt(result, ": %s", _cmd_auth_string(access->auth));
    cmd_main_ver(result);

    cmd_nl(result);
    return (true);
}

static bool _cmd_auth_set(buf_t *result, const struct _cmd_t *cmd,  const char **pptext, access_t *access)
{
    char tmp[CFG_ITEM_SIZE];

    if (! cmd_fetch_string(tmp, pptext, sizeof(tmp)))
        return (false);

    if (cmd_get_password_access(access, tmp) == 0)
        return (false);

    return (true);
}

static bool _cmd_cfg(buf_t *result, const cmd_t *cmd, access_t *access)
{   
    cfg_id_t cfg_id;
    int i;
    for (i=0; i<1000; i++)
    {
        if ((cfg_id = cfg_table_get_id(i)) == CFG_ID_END)
            break;

        if (! cfg_table_access_read(cfg_id, access->auth))
            continue;

        buf_append_fmt(result, "%s:%d,\"", cmd->text, cfg_id);
        cfg_read(result, cfg_id, access->auth);
        buf_append_str(result, "\"");
        cmd_nl(result);
    }
    return (true);
}

static bool _cmd_cfg_set(buf_t *result, const struct _cmd_t *cmd,  const char **pptext, access_t *access)
{
    char tmp[CFG_ITEM_SIZE];
    buf_t buf;
    cmd_int_t cfg_id;

    if (! cmd_fetch_num(&cfg_id, pptext))
        return (false);
    
    if (! cmd_fetch_separator(pptext))
    {
        bool ret;
        buf_append_fmt(result, "%s:%d,\"", cmd->text, cfg_id);
        ret = cfg_read(result, cfg_id, access->auth);
        buf_append_str(result, "\"");

        cmd_nl(result);
        return (ret);
    }

    if (! cmd_fetch_string(tmp, pptext, sizeof(tmp)))
    {
        return (false);
    }
    buf_init_full(&buf, tmp, sizeof(tmp));
    return (cfg_write(cfg_id, &buf, access->auth));
}

static bool _cmd_echo(buf_t *result, const cmd_t *cmd, access_t *access)
{
    buf_append_fmt(result, "%s: %d", cmd->text, modem_echo() ? 1 : 0);
    return (true);
}

static bool _cmd_echo_set(buf_t *result, const struct _cmd_t *cmd,  const char **pptext, access_t *access)
{
    long num;

    if (! cmd_fetch_num(&num, pptext))
        return (false);

    modem_echo_set(num ? true : false);
    return (true);
}

static bool _cmd_gps(buf_t *result, const cmd_t *cmd, access_t *access)
{
    gps_stamp_t pos;
    s32 lat_sec, lon_sec;
    u32 lat_dg,lon_dg; 
    u32 lat_sub_dg,lon_sub_dg;
    float f;

    char symb_lon='E';
    char symb_lat='N';

    buf_append_fmt(result, "%s: ", cmd->text);

    if ((! gps_get_stored_stamp(&pos))
     || (pos.fix < 1))
    {
        buf_append_str(result, "UNKNOWN" CMD_NEW_LINE);
        return (true);
    }

    // print this format :
    //  https://www.google.com/m?q=50.163324+15.004705&site=maps
    if (pos.lon_sec >= 0)
    {
        lon_sec = pos.lon_sec;
    }
    else
    {
        lon_sec = 0-pos.lon_sec;
        symb_lon = 'W';
    }

    if (pos.lat_sec >= 0)
    {
        lat_sec = pos.lat_sec;
    }
    else
    {
        lat_sec = 0-pos.lat_sec;
        symb_lat = 'S';
    }
    lon_dg     =  lon_sec / (6000UL*60);
    lon_sub_dg = (lon_sec % (6000UL*60)) / (6000UL*60/10000);
    lat_dg     =  lat_sec / (6000UL*60);
    lat_sub_dg = (lat_sec % (6000UL*60)) / (6000UL*60/10000);
    
    if (rtc_valid(&pos.time))
    {   // we have got time info
        rtc_t *t = (rtc_t *)&pos.time;
        buf_append_fmt (result,"[GMT ", t->hour,t->minute);

        if (gps_valid_stamp_age() > 24 * 60 * 60 * OS_TIMER_SECOND) // older then day, add date
            buf_append_fmt (result,"%d/%d/%02d ", t->day,t->month,t->year);
        buf_append_fmt (result,"%d:%02d", t->hour,t->minute);
        buf_append_str (result,"] ");
    }

    buf_append_fmt (result, "https://www.google.com/m?q=%c%d.%04u%c%d.%04u&site=maps ",
            symb_lat, lat_dg, lat_sub_dg,
            symb_lon, lon_dg, lon_sub_dg
            );

    f = pos.speed;
    buf_append_fmt (result, "speed=%.1f, alt=%d", f/10, pos.alt);
    
    cmd_nl(result);
    return (true);
}


static bool _cmd_gps_set(buf_t *result, const struct _cmd_t *cmd,  const char **pptext, access_t *access)
{
    long num;

    if (! cmd_fetch_num(&num, pptext))
        return (false);
  
    gps_set_unso(num & 0xFF);
    gps_temporary_start_tmout(60*60);

    return (true);
}

static bool _cmd_reboot_set(buf_t *result, const struct _cmd_t *cmd,  const char **pptext, access_t *access)
{
    long num;

    if (! cmd_fetch_num(&num, pptext))
        return (false);

    switch (num)
    {
    case 0:
        flash_flush_cache();
        OS_PRINTF("reboot" NL);
        OS_FLUSH();
        wdog_reset(GPREG_BOOT_REBOOT);
        break;
    case 1:
        OS_PRINTF("reboot to BL" NL);
        OS_FLUSH();
        wdog_reset(GPREG_BOOT_STAY_IN_BOOT);
        break;
    default:
        break;
    }
    return (false);
}

static bool _cmd_dl(buf_t *result, const cmd_t *cmd, access_t *access)
{
    buf_append_fmt(result, "%s: %d,%d", cmd->text, log_debug_level, log_debug_select);
    cmd_nl(result);
    return (true);
}

static bool _cmd_dl_set(buf_t *result, const struct _cmd_t *cmd,  const char **pptext, access_t *access)
{
    long num;

    if (! cmd_fetch_num(&num, pptext))
        return (false);

    log_debug_level = num;
    if (cmd_fetch_separator(pptext))
    {
        if (cmd_fetch_num(&num, pptext))
            log_debug_select = num;
    }
    return (true);
}

static bool _cmd_io(buf_t *result, const cmd_t *cmd, access_t *access)
{
    u32 inp, out;
    int i;
    bool next = false;

    buf_append_fmt(result, "%s: ", cmd->text);

    inp = io_inp_state(); //  & ~IO_SHOCK_MASK;
    out = io_out_state();

    buf_append_str(result, "active: ");

    for (i=0; i<IO_INPUT_SIZE; i++)
    {
        if ((inp & (1 << i)) == 0)
            continue;

        if (next)
            buf_append_str(result, ",");

        buf_append_str(result, io_inp_name(i));
        next = true;
    }

    for (i=0; i<IO_OUTPUT_SIZE; i++)
    {
        if ((out & (1 << i)) == 0)
            continue;

        if (next)
            buf_append_str(result, ",");

        buf_append_str(result, io_out_name(i));
        next = true;
    }
    if (next == false)
    {
        buf_append_str(result, "none");
    }

    // buf_append_fmt(result, "inp %04x", io_inp_state());

    cmd_nl(result);
    return (true);
}

static bool _cmd_lock(buf_t *result, const cmd_t *cmd, access_t *access)
{
    // bypass the lock input to avoid alarm in armed state
    io_inp_set_bypass(IO_LOCK_IN, true);
    io_set_out(IO_LOCK_OUT, true);
    OS_DELAY(_LOCK_TIME);
    io_set_out(IO_LOCK_OUT, false);
    OS_DELAY(10); // let some time for pullup resistor work
    io_inp_set_bypass(IO_LOCK_IN, false);
    return (true);
}

static bool _cmd_unlock(buf_t *result, const cmd_t *cmd, access_t *access)
{
    // also unset to avoid alarm
    if (system_state() != SECTION_ST_UNSET)
    {
        system_unset(access);
    }
    io_set_out(IO_UNLOCK_OUT, true);
    OS_DELAY(_LOCK_TIME);
    io_set_out(IO_UNLOCK_OUT, false);
    OS_DELAY(10);
    return (true);
}

static bool _cmd_rtc(buf_t *result, const cmd_t *cmd, access_t *access)
{
    buf_append_fmt(result, "%s: ", cmd->text);
    _add_rtc(result);

    cmd_nl(result);
    return (true);
}

static bool _cmd_set(buf_t *result, const cmd_t *cmd, access_t *access)
{
    if (system_state() == SECTION_ST_SET) 
    {
        _add_state(result);
    }
    else
    {
        system_set(access);
        buf_append_fmt(result, "%s OK", cmd->text);
    }

    cmd_nl(result);
    return (true);
}

static bool _cmd_unset(buf_t *result, const cmd_t *cmd, access_t *access)
{
    if (system_state() == SECTION_ST_UNSET) 
    {
        _add_state(result);
    }
    else
    {
        system_unset(access);
        buf_append_fmt(result, "%s OK", cmd->text);
    }

    cmd_nl(result);
    return (true);
}

static bool _cmd_update(buf_t *result, const cmd_t *cmd, access_t *access)
{
    return (update_start());
}

static bool _cmd_net(buf_t *result, const cmd_t *cmd, access_t *access)
{
    buf_append_str(result, "NET: ");
    if (modem_main_state() == MODEM_MAIN_STATE_NET_OK)
    {
        buf_append_str(result, modem_main_lte() ? "LTE" : "GSM");
    }
    else
    {
        buf_append_str(result, "no");
    }
    // buf_append_fmt(result, "NET: %s", modem_main_state() == MODEM_MAIN_STATE_NET_OK ? "OK" : "no");
    cmd_nl(result);
    buf_append_fmt(result, "DATA: %s", udp_ready() ? "OK" : "no");
    cmd_nl(result);
    udp_info(result);

    cmd_nl(result);
    return (true);
}

static bool _cmd_status(buf_t *result, const cmd_t *cmd, access_t *access)
{
    _add_state(result);
    cmd_nl(result);
    buf_append_fmt(result, "PWR: %.1f V, BAT: %.2f V", analog_main_v(), analog_batt_v());
    if (! power_batt_ok())
    {
        buf_append_fmt(result, " test FAIL");
    }
    cmd_nl(result);
    buf_append_fmt(result, "T: %.1f C", analog_temp_c());

    cmd_nl(result);
    return (true);
}

static void _cmd_build_help(const cmd_t *table, buf_t *result, bool full, access_auth_t auth)
{
    int i;

    if (result == NULL)
    {
        return;
    }
    for (i = 0; ; i++)
    {
        if (table[i].text == NULL)
            break;

  #if CMD_USE_HELP
        if ((table[i].help == NULL)
         || (strlen(table[i].help) == 0))
            continue;
  #endif // CMD_USE_HELP

        if (table[i].auth > auth)
            continue;

        if (strlen(table[i].text) == 0)
            break;

        buf_append_str(result, table[i].text);

  #if CMD_USE_HELP
        if (full)
        {
            buf_append_str(result, ": ");
            buf_append_str(result, table[i].help);
        }
  #endif // CMD_USE_HELP
        cmd_nl(result);
    }
    return;
}

static bool _cmd_help(buf_t *result, const cmd_t *cmd, access_t *access)
{
    _cmd_build_help (_CMD_TABLE, result, true, access->auth);
    return (true);
}

static const cmd_t _CMD_TABLE[] = {
    _CMD_TABLE_VALUES

    _CMD(NULL, NULL, 0, 0, ACCESS_NONE, NULL) // command list termination
};

static bool _cmd_parse_keyword(char *dest, size_t limit, const char **pptext)
{
    char *p;

    if ((p = parse_string(dest, *pptext, limit)) == NULL)
    {
        return (false);
    }

    *pptext = p;
    return (true);
}

static bool _cmd_item_read(buf_t *result, const cmd_item_t *item, const char *cmd_text)
{
    if (item->read == NULL)
    {
        return (cmd_error(result, CMD_ERR_MSG_INVALID_PARAM));
    }

    buf_append_fmt(result, "%s: \"%s\",", cmd_text, item->id);
    item->read(result);

    cmd_nl(result);
    return (true);
}

static bool _cmd_item(buf_t *result, const cmd_item_t *item, const char *cmd_text, const char **pptext)
{
    if (pptext == NULL)
    {
        return (_cmd_item_read(result, item, cmd_text));
    }
    _skip_spaces(pptext);

    if (**pptext == '\0')
    {
        return (_cmd_item_read(result, item, cmd_text));
    }

    if (item->write != NULL)
    {
        if (! cmd_fetch_separator(pptext))
        {
            return (false);
        }

        return (item->write(result, pptext));
    }

    return (false);
}

bool cmd_read_all_items(buf_t *result, const cmd_item_t *items, const char *cmd_text)
{
    for (; items->id != NULL; items++)
    {
        if (items->read == NULL)
        {
            continue;
        }
        _cmd_item(result, items, cmd_text, NULL);
    }
    return (true);
}

bool cmd_rw_item(buf_t *result, const cmd_item_t *items, const char *cmd_text, const char **pptext)
{
    char buf[16];
    int l = buf_length(result);

    if (! _cmd_parse_keyword(buf, sizeof(buf), pptext))
    {
        goto error;
    }

    for (; items->id != NULL; items++)
    {
        if (strnicmp(items->id, buf, sizeof(buf)) == 0)
        {
            if (! _cmd_item(result, items, cmd_text, pptext))
            {
                goto error;
            }
            return (true);
        }
    }

error:
    if (l == buf_length(result))
    {
        cmd_error (result, CMD_ERR_MSG_INVALID_PARAM);
    }
    return (false);
}

static const cmd_t *_find_cmd(const cmd_t *table, const char **text, access_auth_t auth)
{
    int i, l, text_l, found, found_id;

    found = 0;
    text_l = 0;

    // compute inpud command text length (text_l)
    for (i = 0; i<CMD_MAX_LEN ; i++)
    {
        if (! is_char((*text)[i]))
        {
            text_l = i;
            break;
        }
    }

    // search for complete text
    for (i = 0; ; i++)
    {
        if (table[i].text == NULL)
            break;

        if ((l = strlen(table[i].text)) == 0)
            break;

        if (table[i].auth > auth)
            continue;
       
        if ((text_l < l) && (text_l > 0))
        {   // detect shortcu versions of command
            if (strnicmp(*text, table[i].text, text_l) == 0)
            {   // text found
                // now next character should be OK
                found++;
                found_id = i;
            }
        }
        if (strnicmp(*text, table[i].text, l) == 0)
        {   // text found
            // now next character should be OK
            if (! is_char((*text)[l]))
            {
                (*text) += l;
                return (&table[i]);
            }
        }
    }

    if (found == 1)
    {   // found exactly one match
        // we have partial match (shortcut) like ST instead of STATUS
        (*text) += text_l;
        return (&table[found_id]);
    }
    return (NULL);
}

bool cmd_init(void)
{
    memset (&_cmd_processing, 0, sizeof(cmd_processing_t));

    OS_SEMAPHORE_INIT(_cmd_processing.mutex);

    if (! buf_init(&_cmd_processing.result_buf, _cmd_processing.result_buf_mem, sizeof(_cmd_processing.result_buf_mem)))
    {
        return (false);
    }

    return (true);
}

bool cmd_error(buf_t *result, const char *text)
{
    if (result == NULL)
    {
        return (false);
    }

    buf_append_str(result, "ERROR");

    if (text != NULL)
    {
        buf_append_fmt(result, ": \"%s\"", text);
    }

    cmd_nl(result);
    return (false);
}

static bool _process_cmd(buf_t *result, const cmd_t *cmd, const char **pptext, access_t *access)
{
    _skip_spaces(pptext);

    if (**pptext == '=')
    {   // parameter included
        if (cmd->pfunc_set == NULL)
        {   // but not enabled for this command
            return(cmd_error(result, CMD_ERR_MSG_INVALID_PARAM));
        }
        (*pptext)++;
        _skip_spaces(pptext);
        return (cmd->pfunc_set(result, cmd, pptext, access));
    }
    else if ((**pptext != '?') && (**pptext != '\0') && (**pptext != _CMD_CHAIN_SEPARATOR))
    {
        OS_PRINTF(NL);
        OS_PRINTF("separtor: %02x", **pptext);
        OS_PRINTF(NL);
        return(cmd_error(result, "illegal parameter"));
    }

    // parameter not included
    if (cmd->pfunc == NULL)
    {   // but parameter required
        return(cmd_error(result, CMD_ERR_MSG_MISSING_PARAM));
    }

    return (cmd->pfunc(result, cmd, access));
}

bool cmd_process_text(buf_t *result, const char *ptext, access_t *access)
{
    _skip_spaces(&ptext);

    while (strnlen(ptext, 1))
    {
        const cmd_t *cmd;
        if ((cmd = _find_cmd(_CMD_TABLE, &ptext, access->auth)) == NULL)
        {
            return(cmd_error(result, CMD_ERR_MSG_UNKNOWN_COMMAND));
        }
        if (! _process_cmd(result, cmd, &ptext, access))
        {
            return (false);
        }

        // maybe continue next command
        _skip_spaces(&ptext);
        
        if (*ptext != _CMD_CHAIN_SEPARATOR)
        {   // valid separator required
            break;
        }
        ptext++;
        _skip_spaces(&ptext);
    }

    return (true);
}

static bool _pass_valid(buf_t *buf)
{
    if (buf_length(buf) < CMD_MIN_PASSWD_LENGTH)
        return (false);
    // TODO: other password validation conditions ?
    return (true);
}

size_t cmd_get_password_access(access_t *access, const ascii *text)
{    // return number of characters used for password
    typedef struct {
        cfg_id_e cfg_id;
        access_auth_t auth;
        event_source_e source;
    } passwd_table_t;

    // define possible passwords to compare
    static const passwd_table_t PASSWD_TABLE[] = {
        {CFG_ID_PASSWD_ADMIN,  ACCESS_ADMIN, EVENT_SOURCE_ADMIN},
        {CFG_ID_PASSWD_SYSTEM, ACCESS_SYSTEM, EVENT_SOURCE_ADMIN},
        {CFG_ID_USER1_PASS,    ACCESS_USER, EVENT_SOURCE_USER1},
        {CFG_ID_USER2_PASS,    ACCESS_USER, EVENT_SOURCE_USER2},
        {CFG_ID_USER3_PASS,    ACCESS_USER, EVENT_SOURCE_USER3},
        {CFG_ID_USER4_PASS,    ACCESS_USER, EVENT_SOURCE_USER4},

        {0, ACCESS_NONE} // end of table
    };
    const passwd_table_t *pt;
    ascii code[CFG_ITEM_SIZE];
    buf_t buf;

    access->auth = ACCESS_NONE;
    access->source = EVENT_SOURCE_UNKNOWN;

    buf_init(&buf, code, sizeof(code));

    pt = &PASSWD_TABLE[0];
    while (pt->cfg_id != 0)
    {
        buf_clear(&buf);
        if (! cfg_read(&buf, pt->cfg_id, ACCESS_SYSTEM))
        {
            return (0);
        }

        if (_pass_valid(&buf))
        {
            if (strnicmp(text, code, buf_length(&buf)) == 0)
            {   // we have found some valid password
                access->auth = pt->auth;
                access->source = pt->source;
                return (buf_length(&buf));
            }
        }
        pt++;
    }
    return (0);
}

bool cmd_sms_process (buf_t *result, ascii *sms_text, ascii *phone_num, u16 sms_len)
{
    const ascii *ptext = sms_text;
    access_t access = {ACCESS_NONE};
    
    _skip_spaces(&ptext);
    ptext += cmd_get_password_access(&access, ptext);
    
    if (access.auth <= ACCESS_NONE)
    {
        LOG_ERROR("invalid code");
        // TODO: detect access by phone_num ?
        return (false);
    }
    _skip_spaces(&ptext);

    cmd_process_text(result, ptext, &access);
    _add_rtc(result);
    return (true);
}


