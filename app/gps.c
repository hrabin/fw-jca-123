#include "os.h"
#include "app.h"
#include "buf.h"
#include "gps_io.h"
#include "util.h"
#include "gps.h"
#include "nmea.h"
#include "rtc.h"
#include "log.h"

LOG_DEF("GPS");

// extern void tracer_new_valid_stamp (void);

#define THIS_SOURCE_ID 9

// maximalni pocet znaku mezi "$" a <CR><LF> je podle specifikace 79, ale Quectel ma i vic (zatim zaznamenano nejvic 81)
// takze dame nejakou rezervu
#define GPS_BUF_SIZE    128
buf_def(gps_buf, GPS_BUF_SIZE);  // na jednu celou zpravu

static const ascii NMEA_PREFIX[] = "$G";
#define NMEA_PREFIX_LEN 2

static const char GPS_ID    = 'P'; // GP: Global Positioning System receiver
static const char MIXED_ID  = 'N'; // GN: Mixed GPS and GLONASS data, according to IEIC 61162-1
static const char GLONASS_ID= 'L'; // GL: GLONASS, according to IEIC 61162-1

static const ascii STR_GGA[] = "GGA";
static const ascii STR_GSA[] = "GSA";
static const ascii STR_GSV[] = "GSV";
static const ascii STR_RMC[] = "RMC";
static const ascii STR_VTG[] = "VTG";
static const ascii STR_GLL[] = "GLL";

#define GPS_NMEA_REQUIRED (GPS_NMEA_RMC | GPS_NMEA_GGA | GPS_NMEA_GSV) // | GPS_NMEA_VTG)

static gps_stamp_t  gps_stamp_tmp; // pracovni zaznam, nemusi byt komplet
gps_stamp_t gps_stamp_last;        // posledni uplny zaznam, muze byt neplatna poloha
gps_stamp_t gps_stamp_last_valid;  // posledni platny zaznam

static os_timer_t fix_tm = 0;

#define SLEEP_WAKEUP        (3600*10) // 1x za hodinu probudit [100ms]
#define SLEEP_TIMEOUT       (3*60*10) // max 3 minuty zkousim sehnat polohu ve sleepu [100ms]
#define SLEEP_DELAY           (15*10) // zpozdeni skutecneho vypnuti (cas na zpresneni polohy)
#define GPS_MAX_WAKEUP_TIME (3600*10) // maximalni doba 'docasneho' probuzeni
static u16  sleep_tmr   = 0;
static u16  error_cnt   = 0;    // pocitam pocet chybnych poloh
static u16  sleep_postpone = 0; // keep wake up timer
static bool sleep_mode = false;
static bool gps_pwr_on_state = false;
static bool gps_rx_ok = false; // detekce, ze GPS posila nejaka platna data
static u8   gps_unsolicited = 0;
static u8   gps_nbsat_visible = 0;
static u8   glonass_nbsat_visible = 0;
static bool glonass_used = false;
static bool gps_init_rq = true;
static u8   gps_jamming = 0;

static u16 pmtk_ack = 0;

os_timer_t gps_reset_tm = 0;        //casovac od gps_reset [sec]

static char _hex_to_a(char ch)
{
    ch &= 0xF;
    if (ch >= 10)
        return (ch - 10 + 'A');
    return ch + '0';
}

void gps_send_text(const ascii *text)
{
    u8 chsum = '$';

    LOG_DEBUGL(3, "TX: \"%s\"", text);

    gps_tx_char('\r');
    gps_tx_char('\n');
    while (*text != '\0')
    {
        chsum ^= *text;
        gps_tx_char(*text);
        text++;
    }
    gps_tx_char('*');
    gps_tx_char(_hex_to_a((chsum>>4) & 0xF));
    gps_tx_char(_hex_to_a(chsum & 0xF));
    gps_tx_char('\r');
    gps_tx_char('\n');
}

static bool pmtk_cmd(const ascii *cmd)
{
    u16 wait = 10;

    pmtk_ack = 0;
    gps_send_text(cmd);
    while (wait--)
    {
        if (pmtk_ack)
            return (true);

        OS_DELAY(10);
    }
    LOG_ERROR("cmd failed");
    return (false);
}

static bool gps_set_baudrate(void)
{
    u16 retry = 2;
    u16 i;

    while (retry--)
    {
        gps_io_port_init(115200);
        OS_DELAY(10);
        gps_rx_ok = false;
        for (i=0; i<20; i++)
        {
            OS_DELAY(100);
            if (gps_rx_ok)
            {
                LOG_DEBUGL(2, "baudrate 115200");
                return (true);
            }
        }
        gps_io_port_init(9600);
        OS_DELAY(10);
        gps_rx_ok = false;

        for (i=0; i<20; i++)
        {
            OS_DELAY(100);
            if (gps_rx_ok)
            {
                LOG_WARNING("baudrate 9600");
                break;
            }
        }
        if (! gps_rx_ok)
        {
            LOG_ERROR("communication failed");
            return (false);
        }
        // Quectel proprietary command
        gps_send_text("$PQBAUD,W,115200");
        //response: "$PQBAUD,W,OK*40"
        // There is no response returned if the baud rate is changed to a different value.
        // ale vypada to, ze neodpovi nikdy
        OS_DELAY(100);
        // continue to retest new baudrate
    }
    // if not successfull continue with 9600
    return (true);
}

bool gps_init (void)
{
    memset ((u8 *)&gps_stamp_tmp,        0, sizeof(gps_stamp_t));
    memset ((u8 *)&gps_stamp_last,       0, sizeof(gps_stamp_t));
    memset ((u8 *)&gps_stamp_last_valid, 0, sizeof(gps_stamp_t));
    gps_io_init();

    gps_pwr_on_state = false;
    glonass_used = false;
    sleep_mode = true;
    gps_io_reset(OFF);
    gps_sleep_enable(true); // vychozi stav je sleep
    gps_temporary_start_tmout(10*60); // try to get coordinates just afret start

    gps_init_rq = true;

    gps_unsolicited = GPS_NMEA_PMTK;
    return (true);
}

void gps_reset (void)
{
    gps_reset_tm = os_timer_get();
    gps_io_reset(ON);
    OS_DELAY(100);
    gps_io_reset(OFF);
    error_cnt=0;
}

void gps_suspend (void)
{
    LOG_DEBUGL(4, "suspend");
    gps_io_pwr (OFF);
    gps_pwr_on_state = false;
    gps_nbsat_visible    = 0;
    glonass_nbsat_visible = 0;
    gps_stamp_last.nbsat = 0;
    gps_stamp_last.fix = 0;
}

void gps_wakeup (void)
{
    LOG_DEBUGL(4, "wakeup");
    if (gps_pwr_on_state == false)
    {
        gps_io_pwr (ON);
        gps_pwr_on_state = true;
    }
}

bool gps_on (void)
{
    return (gps_pwr_on_state);
}

bool gps_glonass_used (void)
{
    return (glonass_used);
}

void gps_sleep_enable (bool state)
{   // je mozne usnout
    // GPS vypnem az po chvili
    if (state)
    {   // povolit usporny rezim
        LOG_DEBUGL(1, "sleep ON");
        sleep_tmr = SLEEP_TIMEOUT - SLEEP_DELAY;
    }
    else
    {
        LOG_DEBUGL(1, "sleep OFF");
        sleep_tmr = 0;
        sleep_postpone = 0;
        gps_wakeup ();
    }
    sleep_mode = state;
}

bool gps_get_stored_stamp (gps_stamp_t *dest)
{
    memcpy ((u8 *)dest, (u8 *)&gps_stamp_last_valid, sizeof(gps_stamp_t));
    if (dest->fix == 0)
        return (false);
    return (true);
}

bool gps_get_current_stamp (gps_stamp_t *dest)
{
    memcpy ((u8 *)dest, (u8 *)&gps_stamp_last, sizeof(gps_stamp_t));
    if (dest->fix == 0)
        return (false);

    return (true);
}

u32 gps_valid_stamp_age (void)
{
    return (os_timer_get() - fix_tm);
}

void gps_set_unso (u8 mask)
{
    gps_unsolicited = mask;
}

static __inline void gps_data_parser (buf_t *inbuf)
{
    static u8 valid_data=0;
    ascii *data;
    bool echo_it=false;
    ascii protocol_id;

    data = buf_data(inbuf);

    // detekce prefixu, musi to zacinat "$G"
    if (stricmp2(data, NMEA_PREFIX) != 0)
    {
        if (gps_unsolicited & GPS_NMEA_PMTK)
            echo_it=true;
        if (stricmp2(data, "$PMTK") == 0)
        {
            data+=5;
            if (strncmp(data,"SPF",3) == 0)
            {
                data+=3;
                switch (data[1])
                {
                case '1': gps_jamming=0; return;
                case '2': gps_jamming=1; break;
                case '3': gps_jamming=2; break;
                }
                goto echo;
            }
            else if (strncmp(data,"001",3) == 0)
            {
                pmtk_ack = 1;
                goto echo;
            }
            else
            {
                goto echo;
            }
        }
        else if (stricmp2(data, "$PQ") == 0)
        {
            goto echo;
        }
        return;
    }

    gps_rx_ok = true;
    data+=NMEA_PREFIX_LEN;

    protocol_id = *data;

    if ((protocol_id != GPS_ID)
     && (protocol_id != GLONASS_ID)
     && (protocol_id != MIXED_ID))
        return;

    data++;

    if (stricmp2(data, STR_GGA)==0)
    {
        if (nmea_parse_gga(&gps_stamp_tmp, data))
            valid_data |= GPS_NMEA_GGA;
        if (gps_unsolicited & GPS_NMEA_GGA)
            echo_it=true;
    }
    else if (stricmp2(data, STR_GSA) == 0)
    {
        if (gps_unsolicited & GPS_NMEA_GSA)
            echo_it=true;
        valid_data |= GPS_NMEA_GSA;
    }
    else if (stricmp2(data, STR_GSV) == 0)
    {
        u8 nbsat;
        bool is_glonass;

        nmea_parse_gsv(&nbsat, &is_glonass, protocol_id, data);

        if (is_glonass)
        {
            glonass_used = true;
            glonass_nbsat_visible = nbsat;
        }
        else
        {
            gps_nbsat_visible = nbsat;
        }
        valid_data |= GPS_NMEA_GSV;
        if (gps_unsolicited & GPS_NMEA_GSV)
            echo_it=true;
    }
    else if (stricmp2(data, STR_RMC) == 0)
    {
        if (nmea_parse_rmc(&gps_stamp_tmp, data))
            valid_data |= GPS_NMEA_RMC;
        if (gps_unsolicited & GPS_NMEA_RMC)
            echo_it=true;
    }
    else if (stricmp2(data, STR_VTG) == 0)
    {
        if (gps_unsolicited & GPS_NMEA_VTG)
            echo_it=true;
        valid_data |= GPS_NMEA_VTG;
    }
    else if (stricmp2(data, STR_GLL) == 0)
    {
        if (gps_unsolicited & GPS_NMEA_GLL)
            echo_it=true;
        valid_data |= GPS_NMEA_GLL;
    }
    else if (stricmp2(data, "TXT") == 0)
    {
    }
    else
    {
        echo_it=true;
    }

    if ((valid_data & GPS_NMEA_REQUIRED) == GPS_NMEA_REQUIRED)
    {
        if (
            (gps_stamp_tmp.lat_sec >   90 * 6000 * 60)
         || (gps_stamp_tmp.lat_sec <  -90 * 6000 * 60)
         || (gps_stamp_tmp.lon_sec >  180 * 6000 * 60)
         || (gps_stamp_tmp.lon_sec < -180 * 6000 * 60)
         )
        {
            LOG_ERROR("invalid position");
            gps_stamp_tmp.lat_sec = 0;
            gps_stamp_tmp.lon_sec = 0;
        }

        if ((gps_stamp_last.lat_sec == gps_stamp_tmp.lat_sec)
         && (gps_stamp_last.lon_sec == gps_stamp_tmp.lon_sec))
        {
            error_cnt++;
        }
        else
        {
            error_cnt=0;
        }

        memcpy ((u8 *)&gps_stamp_last, (u8 *)&gps_stamp_tmp, sizeof(gps_stamp_t));

        if (gps_stamp_last.fix > 0)
        {
            memcpy ((u8 *)&gps_stamp_last_valid, (u8 *)&gps_stamp_last, sizeof(gps_stamp_t));
            fix_tm = os_timer_get();
            if (sleep_mode)
            {
                if (sleep_tmr < (SLEEP_TIMEOUT - SLEEP_DELAY))
                    sleep_tmr = (SLEEP_TIMEOUT - SLEEP_DELAY);
            }
        }
        memset ((u8 *)&gps_stamp_tmp, 0 , sizeof(gps_stamp_t));
        valid_data = 0;
    }

echo:
    if (echo_it)
    {
        log_lock();
        OS_PUTTEXT (NL);
        OS_PRINTF ("%s", buf_data(inbuf));   // never use received data as a format string
        log_unlock();
    }
}

bool gps_fix_ok (void)
{
    if (gps_stamp_last.fix)
    {
        if (gps_pwr_on_state)
            return (true);
    }
    return (false);
}

void gps_maintenance (void)
{
    os_timer_t now = os_timer_get();

    if (error_cnt > 120) // priblizne pocet sekund
    {   // 2 minuty nejaky problem
        if ((now > (gps_reset_tm  + 10 * OS_TIMER_MINUTE))
         && (now > fix_tm + (5 * OS_TIMER_MINUTE)))
        {   //
            LOG_WARNING("forced reset");
            gps_reset();
        }
    }
    if (gps_init_rq)
    {
        if (gps_pwr_on_state == true)
        {
            if (gps_set_baudrate())
            {   // nastaveni baudrate OK
                // poslu dalsi konfiguraci
                pmtk_cmd("$PMTK838,1"); // jamming detection enable
            }
            gps_init_rq = false;
        }
    }
}

void gps_task (void)
{   // fast task
#define GPS_RX_IDLE  0
#define GPS_RX_DATA  1
#define GPS_RX_CHSUM 2
    static u8 rx_state = 0;
    static u8 chsum=0;
    static u8 rx_chsum=0;
    u8 rx_char;

    while (gps_rx_char(&rx_char))
    {
        if ((rx_char == '\n') || (rx_char == '\r'))
            rx_char = '\0';
        else if (rx_char == '$')
        {
            rx_state = GPS_RX_DATA;
            chsum=rx_char;
            buf_clear(&gps_buf);
        }
        switch (rx_state)
        {
        case GPS_RX_IDLE:
            continue;
        case GPS_RX_DATA:
            if (rx_char == '*')
            {
                rx_state = GPS_RX_CHSUM;
                rx_chsum=0;
                break;
            }
            chsum ^= rx_char;
            break;

        case GPS_RX_CHSUM:
            if (rx_char == '\0')
                break;
            rx_chsum <<= 4;
            rx_chsum += ascii_to_hex ((u8)rx_char);
            break;

        default: // ERROR
            rx_state = GPS_RX_IDLE;
            continue;
        }

        if (! buf_append_char (&gps_buf, rx_char))
        {
            LOG_ERROR("buf overflow");
            buf_clear(&gps_buf);
            rx_state=GPS_RX_IDLE;
            continue;
        }
        if (rx_char != '\0')
            continue;

        // we have whole line
        // check CRC
        if (chsum != rx_chsum)
        {
            printf("[GPS CHSUM ERR]");
        }
        else
        {
            gps_data_parser (&gps_buf);
        }
        rx_state=0;
        buf_clear(&gps_buf);
    }

}

void gps_tick (void)
{   // 100ms
    if (sleep_mode)
    {
        if (sleep_postpone)
        {   // prikaz podrzet zapnutou GPS
            sleep_postpone--;
        }
        else
        {
            sleep_tmr++;
        }

        if (sleep_tmr > SLEEP_WAKEUP)
        {
            gps_wakeup();
            sleep_tmr=0;
        }
        else if (sleep_tmr == SLEEP_TIMEOUT)
        {   //
            gps_suspend();
        }
    }
}

static void _gps_temporary_start(void)
{   // podezreni na pohyb, docasny start GPS
    // zapne a po ziskani polohy zas usne, pokud neni pouzito gps_temporary_start_tmout()
    if (! sleep_mode)
        return; // ma to smysl jen ve sleepu
    sleep_tmr=0;
    if (gps_pwr_on_state == true)
        return; // je to probuzene, asi pravidelny test
    gps_wakeup();
}

void gps_temporary_start_tmout(u32 seconds)
{
    u32 tm = seconds * 10; // tick timer 10Hz

    if (tm > GPS_MAX_WAKEUP_TIME)
        return;

    _gps_temporary_start();

    if (tm > sleep_postpone)
    {   // pokud je volano vicekrat v kratke dobe, tak se vezme delsi cas
        LOG_DEBUGL(2, "temporary start %d s", seconds);
        sleep_postpone = tm;
    }
}

u16 gps_get_speed (void)
{
    if (! gps_fix_ok())
        return (0);
    // we use speed * 10
    return ((gps_stamp_last.speed+5)/10);
}

u8 gps_get_nbsat (bool in_use)
{
    if (in_use)
        return (gps_stamp_last.nbsat);
    if(gps_stamp_last.nbsat>gps_nbsat_visible)
        return (gps_stamp_last.nbsat); // tohle by se nemelo nikdy stat, ale pro jistotu
    return (gps_nbsat_visible);
}

u8 gps_get_glonass_nbsat (bool in_use)
{
    if (! glonass_used)
        return (0);
    if (in_use)
        return (0); // nevime kolik je pouzivanych
    return (glonass_nbsat_visible);
}

/*
void gps_debug_show_info (void)
{
    DBG_PRINTF("# GPS: gps_pwr_on_state=%d, sleep_mode=%d, sleep_postpone=%d, sleep_tmr=%d\r\n",
        gps_pwr_on_state, sleep_mode, sleep_postpone, sleep_tmr);
}
*/
