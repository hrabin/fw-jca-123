#include "common.h"
#include "app.h"
#include "cfg.h"
#include "gps.h"
#include "gps_buffer.h"
#include "log.h"
#include "modem_main.h"
#include "net.h"
#include "system.h"
#include "tracer.h"
#include "tracer_h02.h"

LOG_DEF("tracer");

#define _LOG_DEBUGL(...) LOG_DEBUGL(LOG_SELECT_TRACER, __VA_ARGS__)

#define DEVICE_HAS_SHOCK_START 1
#define TRACER_SIA 0  // enable SIA protocol

#if TRACER_SIA 
  #include "tracer_sia.h"
#endif // TRACER_SIA

#define LIMIT_TRACE_END      3600
#define LIMIT_TRACE_INTERVAL 3600

#define CFG_FORMAT "%" PRIu32 ",%d,%d,%d,%d,%d,%d"

static u32  server_ip     = 0;
static u16  server_port   = 0;

static os_timer_t tracer_point_time = 0;
static u16  tracer_store_period  = 0;

#define TRACER_PERIOD_DEFAULT          (10*OS_TIMER_SECOND) // period of point storing
static u16  tracer_store_period_normal  = TRACER_PERIOD_DEFAULT;
#define TRACER_PERIOD_ROAMING_DEFAULT  (10*OS_TIMER_SECOND) // period of point storing in roaming
static u16  tracer_store_period_roaming  = TRACER_PERIOD_ROAMING_DEFAULT;
#define TRACER_END_WAIT_TIME_DEFAULT   (20*OS_TIMER_SECOND) // 
static u16  tracer_end_wait_time = TRACER_END_WAIT_TIME_DEFAULT;
#define TRACER_END_WAIT_POWER_FAIL     (20*OS_TIMER_SECOND) // waiting to finish tracking after main power loss

#define TRACER_PROTO_H02  0 // 
#define TRACER_PROTO_SIA  1 // compatible to SIA-DSC (DC9)
#define TRACER_PROTO_LAST TRACER_PROTO_SIA // number of supported protocols

#define TRACER_PROTO_DEFAULT  (TRACER_PROTO_H02) 
static u8   tracer_protocol = 0;

#define TRACER_START_SPEED_DEFAULT  3 // km/h - start track when speed reaches this limit
static u8   tracer_start_speed = TRACER_START_SPEED_DEFAULT;

#define TRACER_END_NORMAL     0 // 
#define TRACER_END_POWER_FAIL 1 // instant track end when main power lost
#define TRACER_END_DEFAULT TRACER_END_POWER_FAIL
static u8   tracer_end_mode = TRACER_END_DEFAULT;

#define TRACER_SEND_MODE_ONLINE 0 // send online
#define TRACER_SEND_MODE_BULK   1 // send complete track when finished
#define TRACER_SEND_MODE_OFF    2 // dont send data
static u8   tracer_send_mode = TRACER_SEND_MODE_ONLINE;

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

static u32  unit_id = 0;

#if DEVICE_HAS_SHOCK_START == 1
bool tracer_shock_trace_active  = false; 
static bool tracer_shock_stop_rq  = false;
static os_timer_t tracer_shock_start_tmr = 0;
#endif // DEVICE_HAS_SHOCK_START == 1

bool tracer_easy_fix = false; // force tracking when zero speed (with fix)

static bool _external_start = false;

// common buffer for one packet for all protocols
u8 tracer_packet_buffer[TRACER_PACKET_BUFFER_SIZE];

// 
typedef void (*pfunc_server_reinit)            (u32 new_id);
typedef void (*pfunc_server_new_track)       (u16 track_id);
typedef bool (*pfunc_server_packet_ready)    (void);
typedef void (*pfunc_server_packet_done)     (void);
typedef void (*pfunc_server_get_packet)      (u8 *dest);
typedef u16  (*pfunc_server_packet_size)     (void);
typedef bool (*pfunc_server_packet_reply_ok) (u8 *data, u16 len);
typedef void (*pfunc_server_new_point)       (gps_stamp_t *pos, u16 track, bool last, track_info_t *info);

pfunc_server_reinit          server_reinit = NULL; 
pfunc_server_new_track       server_new_track = NULL;
pfunc_server_packet_ready    server_packet_ready = NULL;
pfunc_server_packet_done     server_packet_done = NULL;
pfunc_server_packet_size     server_packet_size = NULL;
pfunc_server_packet_reply_ok server_packet_reply_ok = NULL;
pfunc_server_new_point       server_new_point = NULL;

static void pfunc_reinit (u8 protocol)
{
    switch (protocol)
    {
  #if TRACER_SIA
    case TRACER_PROTO_SIA:
        server_reinit          = (pfunc_server_reinit)&tr_sia_reinit;
        server_new_track       = (pfunc_server_new_track)&tr_sia_new_track;
        server_packet_ready    = (pfunc_server_packet_ready)&tr_sia_packet_ready;
        server_packet_done     = (pfunc_server_packet_done)&tr_sia_packet_done;
        server_packet_size     = (pfunc_server_packet_size)&tr_sia_packet_size;
        server_packet_reply_ok = (pfunc_server_packet_reply_ok)&tr_sia_packet_reply_ok;
        server_new_point       = (pfunc_server_new_point)&tr_sia_new_point;
        break;
  #endif // TRACER_SIA

    case TRACER_PROTO_H02:
    default:
        server_reinit          = (pfunc_server_reinit)&tracer_h02_reinit;
        server_new_track       = (pfunc_server_new_track)&tracer_h02_new_track;
        server_packet_ready    = (pfunc_server_packet_ready)&tracer_h02_packet_ready;
        server_packet_done     = (pfunc_server_packet_done)&tracer_h02_packet_done;
        server_packet_size     = (pfunc_server_packet_size)&tracer_h02_packet_size;
        server_packet_reply_ok = (pfunc_server_packet_reply_ok)&tracer_h02_packet_reply_ok;
        server_new_point       = (pfunc_server_new_point)&tracer_h02_new_point;
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

    if (gps_buf_init (&track, &info.dw))
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
    unit_id = new_id;

    memset(tracer_packet_buffer, 0, sizeof(tracer_packet_buffer));
    
    if (server_reinit != NULL)
        server_reinit(new_id);
}

u32 tracer_unit_id (void)
{
    return (unit_id);
}

bool tracer_reinit (void)
{
    ascii cfg[CFG_ITEM_SIZE];
    buf_t buf;

    u32 a;
    int b,c,d,r,s,e;

    buf_init(&buf, cfg, sizeof(cfg));

    if (cfg_read(&buf, CFG_ID_TRACER_ADDR, ACCESS_SYSTEM))
        net_get_target_ip (&server_ip, &server_port, cfg);

    // some default to override invalid config
    tracer_store_period_normal  = TRACER_PERIOD_DEFAULT;
    tracer_store_period_roaming = TRACER_PERIOD_ROAMING_DEFAULT;
    tracer_end_wait_time        = TRACER_END_WAIT_TIME_DEFAULT;
    tracer_protocol             = TRACER_PROTO_DEFAULT;
    tracer_start_speed          = TRACER_START_SPEED_DEFAULT;
    tracer_send_mode            = TRACER_SEND_MODE_ONLINE;
    tracer_point_time           = 0;

    buf_clear(&buf);
    if (cfg_read(&buf, CFG_ID_TRACER_PARAM, ACCESS_SYSTEM))
    {
        a=0; b=0; c=0; d=0; r=0;
        switch (sscanf(cfg, CFG_FORMAT, &a, &b, &r, &c, &d, &s, &e))
        {
        case 7:
            if ((e>=0) && (e<=1))
                tracer_send_mode = e;
        case 6:
            if ((s>0) && (s<100))
                tracer_start_speed = s;
        case 5:
            if (d<=TRACER_PROTO_LAST)
                tracer_protocol = d;
        case 4:
            if ((c>=1) && (c<=LIMIT_TRACE_END))
                tracer_end_wait_time = c*OS_TIMER_SECOND;
        case 3:
            if ((r>=1) && (r<=LIMIT_TRACE_INTERVAL))
                tracer_store_period_roaming = r*OS_TIMER_SECOND;
        case 2:
            if ((b>=1) && (b<=LIMIT_TRACE_INTERVAL))
                tracer_store_period_normal = b*OS_TIMER_SECOND;
        case 1:
            // at least unit_id, its ok
            break;
        default:
            LOG_ERROR("CFG mismatch");
            break;
        }
    }
    _LOG_DEBUGL("ID=%d, proto=%d, mode=%d", a, tracer_protocol, tracer_send_mode);
    pfunc_reinit(tracer_protocol);

    tracer_server_reinit (a);
    
    tracer_packet_ack = false;

    return (true);
}

static bool tracer_save_config (u32 id, u16 period, u16 period_roaming, u16 wait_time, u8 protocol, u8 limit_speed, u8 end_mode)
{
    ascii cfg[CFG_ITEM_SIZE];
    buf_t buf;

    if (id>=0xFFFFFF)
        return (false);

    buf_init(&buf, cfg, sizeof(cfg));

    sprintf (cfg, CFG_FORMAT, id, period, period_roaming, wait_time, protocol, limit_speed, end_mode);
    cfg_write(CFG_ID_TRACER_PARAM, &buf, ACCESS_SYSTEM);
    tracer_server_reinit (id);

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
            return; 
        }
        // enable_stored_stamp = false;
        tracer_stop_tmr = os_timer_get() + tracer_end_wait_time;
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
    tracer_stop_tmr = os_timer_get() + 21*tracer_store_period_normal;
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
            gps_buf_hard_erase();   // this takes up to 20s ! 
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

    if (server_packet_ready())
    {
        switch (tracer_send_mode)
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
                server_packet_done();
                return; 
            }
            packet.src_port    = server_port;
            packet.dst_port    = server_port;
            packet.dst_ip.addr = server_ip;
            packet.datalen     = server_packet_size();
            packet.data        = tracer_packet_buffer;

            tracer_packet_ack = false; // avoid previous ACK to be accepted now
            net_udp_tx (&packet);

            if (wait_for_ack())
            {
                send_retry = 0;
                server_packet_done();
                gps_buf_delivered_all ();
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
        if (gps_buf_empty())
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

            if (gps_buf_read_next(&pos, &t, &p, &f, &(info.dw)) >= 0)
            {
                last = (f & GPS_FLAG_LAST) ? true : false;
                server_new_point (&pos, t, last, &info);
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
        server_new_track(track);
    }

    point++;
    _LOG_DEBUGL("point %d", point);
    valid_pos = gps_get_current_stamp(&pos);
    flags |= valid_pos      ? GPS_FLAG_VALID : 0;
    flags |= tracer_stop_rq ? GPS_FLAG_LAST  : 0;
    
    info.dw  = 0;
    info.s.driver_id  = user_id;
    info.s.track_type = (system_io_state & SYSTEM_IO_TRACK) ? 1:0;
    if (system_io_state & SYSTEM_IO_PANIC) // default IO_DOOR
        info.s.inputs |= (1<<0);
    if (system_io_state & SYSTEM_IO_INP1) // 
        info.s.inputs |= (1<<1);
    if (system_io_state & SYSTEM_IO_INP2) // 
        info.s.inputs |= (1<<2);
    if (system_io_state & SYSTEM_IO_DOOR) //
        info.s.inputs |= (1<<4);

    info.s.outputs = 0; // app_main_outputs_status();

    gps_buf_store_position (&pos, track, point, flags, info.dw);
}

bool tracer_packet_rx (u8 *data, u16 len, u16 port)
{
    if (port != server_port)
        return (false);

    if (! server_packet_reply_ok (data, len))
        return (false);
    tracer_packet_ack = true;
    return (true);
}

bool tracer_set_id (u32 id)
{
    return (tracer_save_config(id, tracer_store_period_normal/OS_TIMER_SECOND, tracer_store_period_roaming/OS_TIMER_SECOND, tracer_end_wait_time/OS_TIMER_SECOND, tracer_protocol, tracer_start_speed, tracer_send_mode));
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
        if ((system_int_state & SYSTEM_INT_BATT_LOW)
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
         || (gps_get_speed() >= tracer_start_speed))
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
        tracer_store_period = tracer_store_period_roaming;
    else
        tracer_store_period = tracer_store_period_normal;

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

