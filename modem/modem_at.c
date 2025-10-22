#include "os.h"
#include "modem_at.h"
#include "modem_main.h"
#include "util.h"
#include "buf.h"
#include "log.h"

LOG_DEF("AT");


const ascii *modem_at_param_pos(const ascii * data, u16 n)
{
    u16 i;
    const ascii *data_ptr;

    data_ptr = data;
    
    for (i = 0; i < MODEM_AT_RX_BUF_SIZE; i++)
    {
        if (*data_ptr == '\0')
            break;

        if (*(data_ptr++) == ',')
        {
            n--;
            if (n == 0)
                return (data_ptr);
        }
    }
    return (data);
}

void modem_at_init(modem_t *m)
{
    memset(&(m->at), 0, sizeof(modem_at_t));
    OS_SEMAPHORE_INIT(m->at.semaphore);
}

static void modem_at_wakeup(modem_t *m)
{
    m->pfunc_wakeup();
    
    if (m->sleep)
    {   //
        LOG_DEBUGL(LOG_SELECT_SLEEP, "AT wakeup");
        OS_DELAY(100);
        m->sleep = false;
    }
}

void modem_at_lock(modem_t *m)
{   // 
    OS_SEMAPHORE_TAKE(m->at.semaphore);
    m->at.busy = true;
    modem_at_wakeup(m);
}


void modem_at_unlock(modem_t *m)
{
    // modem_at_sleep(m);
    m->at.busy = false;
    OS_SEMAPHORE_GIVE(m->at.semaphore);
} 


bool modem_at_busy(modem_t *m)
{
    return (m->at.busy);
}

static void _clr_response(modem_at_t *at)
{
    at->flags &= ~(AT_ST_OK | AT_ST_ERROR | AT_ST_USER_STR | AT_ST_RAB | AT_ST_LINE);
    at->response_len = 0;
    at->response[0] = '\0';
    at->user_str = NULL;
}

void modem_at_raw (modem_t *m, u8 *data, u16 len)
{
    m->at.flags &= ~(AT_ST_OK | AT_ST_ERROR | AT_ST_USER_STR | AT_ST_RAB);
    modem_tx_data (m, data, len);
}

bool modem_at_cmd_nolock(modem_t *m, const ascii *at_cmd)
{
    // modem_at_wakeup(m);

    if (m->flags & MODEM_FLAG_ECHO)
    {
        OS_PUTTEXT("\r\n/");
        OS_PUTTEXT(at_cmd);
        OS_PUTTEXT(" ");
    }

    _clr_response(&(m->at));
    m->at.last_error_result = 0;
    
    modem_tx_data (m, (u8 *)at_cmd, strlen(at_cmd));
    modem_tx_data (m, (u8 *)"\r\n", 2);
    return (true);
}

u32 modem_at_response(modem_t *m, const ascii *user_str, u32 flags, u32 timeout)
{
    modem_at_t *at = &(m->at);
    os_timer_t limit;

    if (flags & AT_ST_RESET)
        at->flags &= ~(flags);

    at->user_str = user_str;

    limit = OS_TIMER() + timeout;
    while (OS_TIMER() < limit)
    {
        if (flags & at->flags)
        {   // opravdu prislo neco z toho na co cekam
            return (at->flags);
        }
        OS_TASK_YIELD();
    }
    m->error_counter++;
    return (0);
}

static bool _at_cmd_and_response_ok(modem_t *m, buf_t *dest, const ascii *at_cmd, const ascii *user_str)
{
    bool result = false;
    u32 timeout = MODEM_TIMEOUT_M;
    u32 response;

    if (at_cmd == NULL) 
        return (true); 

    if (*at_cmd == '!')
    {
        timeout = MODEM_TIMEOUT_S;
        at_cmd++;
    }
    else if (*at_cmd == '@')
    {
        timeout = MODEM_TIMEOUT_L;
        at_cmd++;
        if (*at_cmd == '@')
        {
            timeout = MODEM_TIMEOUT_LL;
            at_cmd++;
        }
    }
    modem_at_cmd_nolock (m, at_cmd);
    response = modem_at_response (m, user_str, AT_ST_OK | AT_ST_ERROR, timeout);
    if (dest != NULL)
    {
        if ((user_str == NULL) // i want anything 
         || (response & AT_ST_USER_STR)) // or it is what requested 
        {
            buf_append_str(dest, m->at.response);
        }
    }
    if (response & AT_ST_OK)
    {
        if (user_str == NULL)
        {   //
            result = true;
        }
        else
        {   // 
            if (response & AT_ST_USER_STR)
                result = true;
        }
    }
    return (result);
}

bool modem_at_response_ok(modem_t *m, const ascii *at_cmd, const ascii *user_str)
{
    bool result = false;
    modem_at_lock(m);
    result = _at_cmd_and_response_ok(m, NULL, at_cmd, user_str);
    modem_at_unlock(m);
    return (result);
}

bool modem_at_ok_cmd(modem_t * m, const ascii * at_cmd)
{   
    bool result = false;

    modem_at_lock(m);
    result = _at_cmd_and_response_ok(m, NULL, at_cmd, NULL);
    modem_at_unlock(m);
    return (result);
}


bool modem_at_ok_cmd_nolock(modem_t * m, const ascii * at_cmd)
{
    return (_at_cmd_and_response_ok(m, NULL, at_cmd, NULL));
}

bool modem_at_cmd_get_response(modem_t *m, buf_t *dest, const ascii *at_cmd, const ascii *user_str)
{
    bool result = false;
    
    modem_at_lock(m);
    result = _at_cmd_and_response_ok(m, dest, at_cmd, user_str);
    modem_at_unlock(m);
    return (result);
}


u16 modem_at_read_line (modem_t *m, buf_t *dest, const ascii *user_str, u32 timeout)
{
    modem_at_t *at = &(m->at);
    os_timer_t limit;

    _clr_response(at);

    limit = OS_TIMER() + timeout;
    at->user_str = user_str;

    while (OS_TIMER() < limit)
    {
        if (at->flags & AT_ST_LINE)
        {   // 
            if ((at->user_str != NULL) && // i want exact answer 
                ((at->flags & AT_ST_USER_STR) == 0)) // and is it not what expected
            {   // continute to next row  ...
                at->flags &= ~(AT_ST_LINE); 
                continue;
            }
            if (dest != NULL)
            {
                buf_append_mem(dest, at->response_len, at->response);
            }
            return (at->flags & (AT_ST_LINE | AT_ST_USER_STR));
        }
        if (at->flags & AT_ST_ERROR)
            break;
        OS_TASK_YIELD();

    }
    m->error_counter++;
    return (0);
}

u16 modem_at_read_line_lite (modem_t *m, ascii *dest, u16 resp_limit, const ascii *user_str, u32 timeout)
{
    buf_t buf;

    if (dest == NULL)
        return (0);

    if (buf_init (&buf, dest, resp_limit) == false)
        return (0);

    return (modem_at_read_line(m, &buf, user_str, timeout));
}

void modem_at_rx(modem_t * m, u8 rx_char)
{
    modem_at_t *at = &(m->at);

    if (m->flags & MODEM_FLAG_ECHO)
    {
        if (((rx_char>=0x20) && (rx_char<127))
        || (rx_char=='\r') 
        || (rx_char=='\n'))
        {
            OS_PRINTF("%c", rx_char);
        }
        else
        {
            OS_PRINTF("[%02x]", rx_char);
        }

        if (rx_char=='\n')
            OS_PRINTF("|");
    }

    if ((rx_char == '\n') || (rx_char == '\r'))
        rx_char = '\0';

    at->rx_buf[at->buf_len] = rx_char;
    if (at->buf_len < (MODEM_AT_RX_BUF_SIZE -1)) 
        at->buf_len++;

    if (rx_char == '\0')
    {   // some string received
        if (at->buf_len > 1)
        {   // has some valid length
            at->flags |= AT_ST_LINE;

            // most common answers process first 
            if (! stricmp(at->rx_buf, "OK"))
            {
                at->flags |= AT_ST_OK;
                at->flags |= AT_ST_READY;
                goto modem_rx_exit; 
            }
            if (! stricmp (at->rx_buf, "ERROR"))
            {
                at->flags |= AT_ST_ERROR;
                goto modem_rx_exit;
            }
            if (modem_parse_urc(m))
            {   // 
                goto modem_rx_exit;
            }

            if (at->response_len == 0)
            {   // dont have respone yet
                if (at->user_str != NULL)
                {   // waiting for specific string
                    if (!strnicmp(at->rx_buf, at->user_str, strlen(at->user_str)))
                    {   // yes, it is what i want 
                        at->user_str = NULL;
                        memcpy(at->response, at->rx_buf, at->buf_len);
                        at->response_len = at->buf_len;
                        at->flags |= AT_ST_USER_STR;
                        OS_DELAY(2); // some time to process buffer ? 
                        // TODO: some mutex ?
                    }

                }
                else
                { 
                    memcpy(at->response, at->rx_buf, at->buf_len);
                    at->response_len = at->buf_len;
                    OS_DELAY(2); // some time to process buffer ? 
                    // TODO: some mutex ?
                }
            }
        }

        // continue processing next line 
        at->buf_len = 0;
    }
    else if (rx_char == '>')
        at->flags |= AT_ST_RAB;

    return;

modem_rx_exit:
    at->buf_len = 0;

    return;
}
