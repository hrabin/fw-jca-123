#include "common.h"
#include "app.h"
#include "cfg.h"
#include "gps.h"
#include "parse.h"
#include "tracer_buffer.h"
#include "log.h"
#include "modem_main.h"
#include "net.h"
#include "system.h"
#include "tracer.h"
#include "tracer_proto.h"

LOG_DEF("tracer");

#define _LOG_DEBUGL(...) LOG_DEBUGL(LOG_SELECT_TRACER, __VA_ARGS__)

#define DEVICE_HAS_SHOCK_START 1

#define LIMIT_TRACE_END      3600
#define LIMIT_TRACE_INTERVAL 3600

static u32  server_ip     = 0;
static u16  server_port   = 0;

static os_timer_t tracer_point_time = 0;
static u16  tracer_store_period  = 0;

#define TRACER_PERIOD_DEFAULT          (10) // [s] period of point storing
#define TRACER_PERIOD_ROAMING_DEFAULT  (10) // [s] period of point storing in roaming
#define TRACER_END_WAIT_TIME_DEFAULT   (20) // [s] 
#define TRACER_END_WAIT_POWER_FAIL     (20*OS_TIMER_SECOND) // waiting to finish tracking after main power loss

#define TRACER_PROTO_H02  0 //
#define TRACER_PROTO_SIA  1 // compatible to SIA-DSC (DC9)
#define TRACER_PROTO_LAST TRACER_PROTO_SIA // number of supported protocols

#define TRACER_PROTO_DEFAULT  (TRACER_PROTO_H02)

#define TRACER_UNIT_ID_MAX 0x7FFFFFFE // parse_number() is signed

#define TRACER_START_SPEED_DEFAULT  3 // km/h - start track when speed reaches this limit

#define TRACER_END_NORMAL     0 //
#define TRACER_END_POWER_FAIL 1 // instant track end when main power lost
#define TRACER_END_DEFAULT TRACER_END_NORMAL
static u8   tracer_end_mode = TRACER_END_DEFAULT; // not part of CFG_ID_TRACER_PARAM

#define TRACER_SEND_MODE_ONLINE 0 // send online
#define TRACER_SEND_MODE_BULK   1 // send complete track when finished
#define TRACER_SEND_MODE_OFF    2 // dont send data

#define TRACER_START_MODE_KEY   (1 << 0) // start tracking on ignition (KEY) input
#define TRACER_START_MODE_SHOCK (1 << 1) // start tracking on shock/activity
#define TRACER_START_MODE_ALL   (TRACER_START_MODE_KEY | TRACER_START_MODE_SHOCK)

// Setup parameters, i.e. the layout of CFG_ID_TRACER_PARAM:
//   <unit_id>,<period>,<period_roaming>,<wait_time>,<protocol>,<start_speed>,
//   <send_mode>,<start_mode>
// Times are kept in seconds, exactly as written in the config; use _sec() to
// convert to OS timer units.
typedef struct {
    u32 unit_id;        // server/unit id
    u16 period;         // [s] point storing period
    u16 period_roaming; // [s] point storing period while roaming
    u16 wait_time;      // [s] wait for the track to finish
    u8  protocol;       // TRACER_PROTO_*
    u8  start_speed;    // [km/h] start a track above this speed
    u8  send_mode;      // TRACER_SEND_MODE_*
    u8  start_mode;     // TRACER_START_MODE_* flags
} tracer_param_t;

#define TRACER_PARAM_DEFAULT {                       \
    .unit_id        = 0,                             \
    .period         = TRACER_PERIOD_DEFAULT ,        \
    .period_roaming = TRACER_PERIOD_ROAMING_DEFAULT, \
    .wait_time      = TRACER_END_WAIT_TIME_DEFAULT,  \
    .protocol       = TRACER_PROTO_DEFAULT,          \
    .start_speed    = TRACER_START_SPEED_DEFAULT,    \
    .send_mode      = TRACER_SEND_MODE_ONLINE,       \
    .start_mode     = TRACER_START_MODE_ALL,         \
}

static tracer_param_t _p = TRACER_PARAM_DEFAULT;

static inline os_timer_t _sec(u16 s)
{   // seconds -> OS timer units
    return ((os_timer_t)s * OS_TIMER_SECOND);
}

static bool tracer_stop_rq=false;

static os_timer_t tracer_stop_tmr = 0;    // timer for tracking end delay
static os_timer_t comm_sleep_tmr = 0;
static os_timer_t track_last_fix_tmr = 0; // time of last valid GPS point

static bool tracer_packet_ack = false;
#define NO_FIX_PROBLEM_LIMIT  (30*OS_TIMER_SECOND)
#define NO_FIX_RESET_LIMIT   (100*OS_TIMER_SECOND)
static bool no_fix_reset_enable = true; // enable only one forced GPS reset in one track

static u16  point = 0;
static u16  track = 0;
static u8   user_id = 0;

static u16  new_track_id_set = 0;

static os_timer_t driver_waiting_tmr = 0;
static bool tracing_active = false;

#if DEVICE_HAS_SHOCK_START == 1
bool tracer_shock_trace_active  = false;
static bool tracer_shock_stop_rq  = false;
static os_timer_t tracer_shock_start_tmr = 0;
#endif // DEVICE_HAS_SHOCK_START == 1

bool tracer_easy_fix = false; // force tracking when zero speed (with fix)

static bool _external_start = false;

// common buffer for one packet for all protocols
u8 tracer_packet_buffer[TRACER_PACKET_BUFFER_SIZE];

// protocol interface — set by pfunc_reinit()
static const tracer_proto_t *proto = NULL;

static void pfunc_reinit(u8 protocol)
{
    switch (protocol)
    {
#if TRACER_SIA
    case TRACER_PROTO_SIA:
        proto = &tracer_proto_sia;
        break;
#endif
    case TRACER_PROTO_H02:
    default:
        proto = &tracer_proto_h02;
        break;
    }
}

static void tracer_comm_sleep(u16 tm)
{
    comm_sleep_tmr = os_timer_get() + (tm * OS_TIMER_SECOND);
}

bool tracer_init (void)
{   // main init after boot
    track_info_t info;

    pfunc_reinit(TRACER_PROTO_DEFAULT);

    tracer_reinit();

    if (tracer_buf_init (&track, &info.dw))
    {   // restore last driver and track type
        tracer_set_user_id(info.s.driver_id);
        if (info.s.track_type)
            system_io_state |= SYSTEM_IO_TRACK;

        return (true);
    }
    return (false);
}

void tracer_set_track_id (u16 id)
{   // setting track-id possible only by erasing all data
    // normally we dont need it
    if (id == 0)
        id++;

    new_track_id_set = id;
    tracer_comm_sleep(1);
}

u16 tracer_get_track_id (void)
{
    return (track);
}

static void tracer_server_reinit (u32 new_id)
{
    _p.unit_id = new_id;

    memset(tracer_packet_buffer, 0, sizeof(tracer_packet_buffer));

    if (proto != NULL)
        proto->reinit(new_id);
}

u32 tracer_unit_id (void)
{
    return (_p.unit_id);
}

// ---- CFG_ID_TRACER_PARAM -------------------------------------------------
// The config is a comma separated list of numbers, one per tracer_param_t
// field, in the order declared above.  A field missing from the string keeps
// its default, and an out of range value is reported and ignored while the
// fields that follow are still applied.

static bool _num_next(const char **s, parse_number_t *n)
{   // move to the next number; false once the string is exhausted
    const char *p = parse_number(n, *s);
    const char *sep;

    if (p == NULL)
        return (false);

    sep = parse_separator(p);
    *s = (sep != NULL) ? sep : p;   // keep the tail for the terminator check
    return (true);
}

static parse_number_t _num_get(const char **s, s32 min, s32 max, s32 def)
{
    parse_number_t n;

    if (! _num_next(s, &n))
        return (def);

    if ((n < min) || (n > max))
    {
        return (def);
    }
    return (n);
}

static void _param_load(const ascii *value)
{
    const char *s = value;

   _p.unit_id        = _num_get(&s, 0, TRACER_UNIT_ID_MAX,    0);
   _p.period         = _num_get(&s, 1, LIMIT_TRACE_INTERVAL,  TRACER_PERIOD_DEFAULT);
   _p.period_roaming = _num_get(&s, 1, LIMIT_TRACE_INTERVAL,  TRACER_PERIOD_ROAMING_DEFAULT);
   _p.wait_time      = _num_get(&s, 1, LIMIT_TRACE_END,       TRACER_END_WAIT_TIME_DEFAULT);
   _p.protocol       = _num_get(&s, 0, TRACER_PROTO_LAST,     TRACER_PROTO_DEFAULT);
   _p.start_speed    = _num_get(&s, 1, 99,                    TRACER_START_SPEED_DEFAULT);
   _p.send_mode      = _num_get(&s, 0, TRACER_SEND_MODE_OFF,  TRACER_SEND_MODE_ONLINE);
   _p.start_mode     = _num_get(&s, 0, TRACER_START_MODE_ALL, TRACER_START_MODE_ALL);

    if (! parse_terminator(s))
        LOG_WARNING("tracer param: unexpected \"%s\"", s);
}

bool tracer_reinit (void)
{
    ascii cfg[CFG_ITEM_SIZE];
    buf_t buf;

    buf_init(&buf, cfg, sizeof(cfg));

    if (cfg_read(&buf, CFG_ID_TRACER_ADDR, ACCESS_SYSTEM))
        net_get_target_ip (&server_ip, &server_port, cfg);

    // start from the defaults, then let the config override them
    _p = (tracer_param_t)TRACER_PARAM_DEFAULT;
    tracer_point_time = 0;

    buf_clear(&buf);
    if (cfg_read(&buf, CFG_ID_TRACER_PARAM, ACCESS_SYSTEM))
        _param_load (buf_data(&buf));

    _LOG_DEBUGL("ID=%d, proto=%d, send=%d, start=%d",
                (int)_p.unit_id, _p.protocol, _p.send_mode, _p.start_mode);

    pfunc_reinit(_p.protocol);
    tracer_server_reinit (_p.unit_id);

    tracer_packet_ack = false;

    return (true);
}

static bool tracer_save_config (const tracer_param_t *p)
{
    ascii cfg[CFG_ITEM_SIZE];
    buf_def(buf, CFG_ITEM_SIZE);

    if (p->unit_id > TRACER_UNIT_ID_MAX)
        return (false);

    // <unit_id>,<period>,<period_roaming>,<wait_time>,<protocol>,
    // <start_speed>,<send_mode>,<start_mode>
    snprintf (cfg, sizeof(cfg), "%" PRIu32 ",%u,%u,%u,%u,%u,%u,%u",
              p->unit_id, p->period, p->period_roaming, p->wait_time,
              p->protocol, p->start_speed, p->send_mode, p->start_mode);

    buf_append_str(&buf, cfg);   // buf_init() alone leaves length 0
    if (! cfg_write(CFG_ID_TRACER_PARAM, &buf, ACCESS_SYSTEM))
        return (false);

    tracer_server_reinit (p->unit_id);
    return (true);
}

bool tracer_is_active (void)
{
    if (tracing_active)
        return (true);
    if (system_io_state & SYSTEM_IO_TRACING)
        return (true);
    return (false);
}

static void tracer_activate (void)
{
    tracing_active = true;
    // dont set SYSTEM_IO_TRACING now, keep it for first point as filter
}

static void tracer_deactivate (void)
{
    tracer_stop_rq = false;
    tracing_active = false;
    system_io_state &= ~SYSTEM_IO_TRACING;
}

static bool _tracer_start_now (void)
{
    if (tracer_stop_tmr)
    {
        tracer_stop_tmr = 0;
        // tracking dint stop yet, continue
        return (false);
    }
    if (tracer_stop_rq)
    {
        LOG_ERROR("stop_rq");
        tracer_stop_rq = false;
    }
    if (tracer_is_active())
        return (false);

    if (! tracer_server_setup_ok())
    {
        LOG_ERROR ("bad setup");
        return (false);
    }
    gps_sleep_enable (false);

    track_last_fix_tmr = os_timer_get();
    no_fix_reset_enable = true;
    //
    point=0;

    // set first point time
    tracer_point_time = os_timer_get() + (5 * OS_TIMER_SECOND);

    tracer_activate();
    return (true);
}

static void _tracer_stop_now (void)
{
    // stop request, give some timeout
    if (tracer_is_active())
    {
        if (point == 0)
        {
            tracer_deactivate();
            gps_sleep_enable (true);
            return;
        }
        // enable_stored_stamp = false;
        tracer_stop_tmr = os_timer_get() + _sec(_p.wait_time);
    }
    else
    {
        gps_sleep_enable (true);
    }
}

void tracer_start (void)
{
    if (_tracer_start_now())
        _external_start = true;
}

void tracer_key_start (void)
{
    if ((_p.start_mode & TRACER_START_MODE_KEY) == 0)
        return;

    tracer_start();
}

void tracer_stop (void)
{
    _tracer_stop_now();
    _external_start = false;
}

bool tracer_test (void)
{   // short tracking start
    if (tracer_is_active())
        return (true);
    if (! tracer_server_setup_ok())
        return (false);

    tracer_start();
    tracer_stop_tmr = os_timer_get() + 21 * _sec(_p.period);
    return (true);
}

bool tracer_server_setup_ok (void)
{
    if ((server_ip == 0) || (server_port == 0))
        return (false);
    if (tracer_unit_id() == 0)
        return (false);
    return (true);
}

bool tracer_setup_ok (void)
{
    return (tracer_server_setup_ok());
}

bool tracer_wait_for_driver_id (void)
{
    if (driver_waiting_tmr)
        return (true);
    return (false);
}

bool wait_for_ack (void)
{
    #define  TMOUT_MAX  5*OS_TIMER_SECOND
    #define  TMOUT_MIN  200*OS_TIMER_MS

    static u32 tmout = TMOUT_MAX / 2;

    os_timer_t now = os_timer_get();;
    os_timer_t start = now;
    bool result = false;

    while (now < (start + tmout))
    {
        OS_DELAY(10);
        now = os_timer_get();

        if (! tracer_packet_ack)
            continue; // no response yet

        // yes, we have got response
        tracer_packet_ack = false;

        u32 tm = now - start;
        _LOG_DEBUGL("ACK in %d", tm);
        tmout = (now - start);
        result = true;
        break;
    }
    tmout = tmout + (tmout>>1); // update timeout by factor 1.5
    if (tmout > TMOUT_MAX)
        tmout = TMOUT_MAX;
    if (tmout < TMOUT_MIN)
        tmout = TMOUT_MIN;

    return (result);
}

void tracer_comm_process (void)
{
    #define MAX_SEND_RETRY 5
    static u8 send_retry = 0;

    if (os_timer_get() < comm_sleep_tmr)
        return;

    if (new_track_id_set)
    {   // request for track-id change (erase data)
        if (! tracer_is_active())
        {
            tracer_buf_hard_erase();   // this takes up to 20s !
            track = new_track_id_set-1;
            new_track_id_set = 0;
            return;

        }
    }

    if (! tracer_server_setup_ok())
    {
        _LOG_DEBUGL("setup not ok");
        tracer_comm_sleep(60);
        return;
    }

    if (proto->packet_ready())
    {
        switch (_p.send_mode)
        {
        case TRACER_SEND_MODE_OFF:
            _LOG_DEBUGL("T:OFF");
            tracer_comm_sleep(60);
            return;
        case TRACER_SEND_MODE_BULK:
            // wait till end of current track
            if (tracer_is_active())
            {
                tracer_comm_sleep(5);
                return;
            }
        }
        if (net_connect())
        {
            udp_packet_t packet;

            if ((server_ip == 0) || (server_port == 0))
            {
                tracer_reinit();
                tracer_comm_sleep(60);
                proto->packet_done();
                return;
            }
            packet.src_port    = server_port;
            packet.dst_port    = server_port;
            packet.dst_ip.addr = server_ip;
            packet.datalen     = proto->packet_size();
            packet.data        = tracer_packet_buffer;

            tracer_packet_ack = false; // avoid previous ACK to be accepted now
            net_udp_tx (&packet);

            if (wait_for_ack())
            {
                send_retry = 0;
                proto->packet_done();
                tracer_buf_delivered_all ();
                return;
            }
            LOG_ERROR ("NO ACK");
            tracer_comm_sleep(1);
        }
        else
        {
            LOG_ERROR ("cant send packet");
            tracer_comm_sleep(5);
        }
        if (send_retry < MAX_SEND_RETRY)
        {
            send_retry++;
            return;
        }
        // sending not successful, keep stored in FLASH
        _LOG_DEBUGL("keep trying");
        // retry after some time (not too often)
        tracer_comm_sleep(30);
    }
    else
    {   // nothing pending, so check for old data from FLASH
        if (tracer_buf_empty())
        {
            tracer_comm_sleep(5);
        }
        else
        {
            track_info_t info;
            gps_stamp_t pos;
            u16 t,p;
            u8 f;
            bool last;

            if (tracer_buf_read_next(&pos, &t, &p, &f, &(info.dw)) >= 0)
            {
                last = (f & TRACER_FLAG_LAST) ? true : false;
                proto->new_point (&pos, t, last, &info);
                // tracer_comm_sleep(1);
            }
            else
            {   // this should never happen, only in case some memory problem
                LOG_ERROR("read next");
                tracer_comm_sleep(10);
            }
        }
    }
}

void tracer_new_valid_stamp (void)
{
//  enable_stored_stamp = true;
    track_last_fix_tmr = os_timer_get();
}

u8 _map_inputs(void)
{
    u8 result = 0;

    if (system_io_state & SYSTEM_IO_PANIC) // default INP_DOOR
        result |= (1<<0);
    if (system_io_state & SYSTEM_IO_INP1) //
        result |= (1<<1);
    if (system_io_state & SYSTEM_IO_INP2) //
        result |= (1<<2);
    if (system_io_state & SYSTEM_IO_KEY) //
        result |= (1<<3);
    if (system_io_state & SYSTEM_IO_DOOR) //
        result |= (1<<4);
    if (system_io_state & SYSTEM_IO_SHOCK) //
        result |= (1<<5);

    return (result);
}

void tracer_new_point (void)
{
    gps_stamp_t pos;
    track_info_t info;
    u8 flags=0;
    bool valid_pos;

    if (point == 0)
    {   // first point of new track
        track++;
        _LOG_DEBUGL("new track %d", track);
        // now system reaction (like move info)
        system_io_state |= SYSTEM_IO_TRACING;
        proto->new_track(track);
    }

    point++;
    _LOG_DEBUGL("point %d", point);
    valid_pos = gps_get_current_stamp(&pos);
    flags |= valid_pos      ? TRACER_FLAG_VALID : 0;
    flags |= tracer_stop_rq ? TRACER_FLAG_LAST  : 0;

    info.dw  = 0;
    info.s.driver_id  = user_id;
    info.s.track_type = (system_io_state & SYSTEM_IO_TRACK) ? 1:0;
    
    info.s.inputs = _map_inputs();
    info.s.outputs = 0; // app_main_outputs_status();

    if (system_int_state & SYSTEM_INT_POWER_FAIL)
        info.s.res |= (1<<1); // main power failure

    tracer_buf_store_position (&pos, track, point, flags, info.dw);
}

bool tracer_packet_rx (u8 *data, u16 len, u16 port)
{
    if (port != server_port)
        return (false);

    if (! proto->packet_reply_ok (data, len))
        return (false);
    tracer_packet_ack = true;
    return (true);
}

bool tracer_set_id (u32 id)
{
    _p.unit_id = id;
    return (tracer_save_config(&_p));
}

void tracer_set_user_id (u8 id)
{
    user_id = id;
    driver_waiting_tmr = 0;
}

u8 tracer_get_user_id (void )
{
    return (user_id);
}

#if DEVICE_HAS_SHOCK_START == 1

static bool tracer_power_stop(void)
{
    if (system_int_state & SYSTEM_INT_POWER_FAIL)
    {
        if ((system_int_state & (SYSTEM_INT_BATT_LOW | SYSTEM_INT_BATT_FAIL))
         || (tracer_end_mode == TRACER_END_POWER_FAIL))
        {
            return (true);
        }
    }
    return (false);
}

void tracer_shock_start(bool state)
{
    if (state)
    {   // activity detected
        if ((_p.start_mode & TRACER_START_MODE_SHOCK) == 0)
            return;

        if ((tracer_shock_trace_active == false)
         && (! tracer_power_stop()))
        {   // ok, lets wait for some speed
            if (! tracer_is_active())
                app_main_led_single(0x03, 8);
            tracer_shock_start_tmr = os_timer_get() + 60*OS_TIMER_SECOND;
            LOG_INFO("SHOCK START RQ");
        }
        tracer_shock_stop_rq = false;
    }
    else if (tracer_shock_trace_active)
    {   // no reason for tracking any more
        // wait for zero speed to end of track
        tracer_shock_stop_rq  = true;
    }
}

static __inline void tracer_shock_task(void)
{   // detect accelerometer activity for start tracking
    if (_external_start)
        return;

    if (tracer_power_stop())
    {   // no main power cant start
        if (tracer_shock_trace_active)
        {
            LOG_WARNING("STOP OK, POWER");
            tracer_shock_trace_active = false; // force end, don wait for zero speed
            _tracer_stop_now();
        }
        return;
    }

    if (os_timer_get() < tracer_shock_start_tmr)
    {
        gps_temporary_start_tmout(30); // keep GPS on
        if ((tracer_easy_fix  && (gps_fix_ok()))
         || (gps_get_speed() >= _p.start_speed))
        {
            LOG_INFO("SHOCK START OK, speed=%d", gps_get_speed());
            tracer_shock_start_tmr = 0;
            tracer_shock_trace_active = true;
            _tracer_start_now();

        }
    }
    else if (tracer_shock_stop_rq)
    {
        if (gps_get_speed() < 2)
        {   // met also for no fix
            LOG_INFO("STOP OK");
            tracer_shock_trace_active = false;
            tracer_shock_stop_rq = false;
            _tracer_stop_now();
        }
    }
}
#else // DEVICE_HAS_SHOCK_START ==1
  #define   tracer_shock_task()
#endif // ~DEVICE_HAS_SHOCK_START != 1


void tracer_task (void)
{   // call aprox every 100ms
    os_timer_t now;
    tracer_shock_task();

    if (! tracer_is_active())
    {
        return;
    }

    now = os_timer_get();
    if (driver_waiting_tmr)
    {
        if (now > driver_waiting_tmr)
            driver_waiting_tmr = 0;
    }

    if (tracer_stop_tmr)
    {
        if (now > tracer_stop_tmr)
        {
            tracer_stop_tmr = 0;
            tracer_stop_rq = true;
            tracer_point_time = now + OS_TIMER_SECOND;  // do the last point quicker
        }
        if (system_int_state & SYSTEM_INT_POWER_FAIL)
        {   // force quicker end when lost power
            if (tracer_stop_tmr > now + TRACER_END_WAIT_POWER_FAIL)
                tracer_stop_tmr = now + TRACER_END_WAIT_POWER_FAIL;
        }
    }

    if (tracer_wait_for_driver_id())
    {   // waiting for driver
        // TODO: do some signalization
    }

    // workaround for some GPS issues
    if (now - track_last_fix_tmr > NO_FIX_PROBLEM_LIMIT)
    {
        if (no_fix_reset_enable)
        {   // there was no GPS reset in this track
            if ((gps_valid_stamp_age() < 10*OS_TIMER_MINUTE)
             && (gps_valid_stamp_age() >  1*OS_TIMER_MINUTE))
            {   //
                no_fix_reset_enable = false;
                LOG_ERROR ("fix problem,reset");
                gps_reset();
            }
            if (now - track_last_fix_tmr > NO_FIX_RESET_LIMIT)
            {   //
                no_fix_reset_enable = false;
                LOG_ERROR ("fix error,reset");
                gps_reset();
            }
        }
    }

    if (now < tracer_point_time)
        return; // no time for new point yet

    if (modem_main_roaming())
        tracer_store_period = _sec(_p.period_roaming);
    else
        tracer_store_period = _sec(_p.period);

    if (tracer_store_period < (1 * OS_TIMER_SECOND))
        tracer_store_period = (1 * OS_TIMER_SECOND); // this should never happen

    tracer_point_time += tracer_store_period;

    if (tracer_point_time < now)
        tracer_point_time = now + tracer_store_period; // too big delay reset time

    tracer_new_point();

    if (tracer_stop_rq)
    {
        tracer_stop_rq = false;
        tracer_deactivate();
        _LOG_DEBUGL("track end");
        gps_sleep_enable (true);
    }
}

