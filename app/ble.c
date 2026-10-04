#include "common.h"
#include "ble.h"
#include "log.h"
#include "gpio.h"

LOG_DEF("BLE");

#define _BUF_SIZE TTY_BUF_SIZE

static tty_parse_callback_t _rx_callback = NULL;

static bool _connected = false;
static char _tx_buf[_BUF_SIZE];
static size_t _tx_buf_rd_idx = 0;
static size_t _tx_buf_wr_idx = 0;

static char _rx_buf[_BUF_SIZE];
static size_t _rx_len = 0;

static void _clear_buf(void)
{
    _tx_buf_rd_idx = _tx_buf_wr_idx;
}

// ---- module presence test ----

#define _TEST_CMD        "AT+VERSION"
// match the *answer*, not the echo: the module echoes "AT+VERSION", but only
// the reply carries the '=' ("+VERSION=JDY-25M-V1.731")
#define _TEST_REPLY      "+VERSION="
#define _BOOT_DELAY_MS   200   // module boot time after the reset is released
#define _TEST_TMOUT_MS   500
#define _TEST_LINE_SIZE  64

static void _ble_uart_put(const ascii *text)
{   // raw write; no PWRC toggling (that is what the BLE= console command does)
    while (*text != '\0')
        HW_BLE_UART_PUTCHAR(*text++);

    HW_BLE_UART_PUTCHAR('\r');
    HW_BLE_UART_PUTCHAR('\n');
}

static bool _ble_comm_test(void)
{
    ascii line[_TEST_LINE_SIZE];
    size_t n = 0;
    os_timer_t timeout;
    int ch;

    // drop whatever the module said while it was booting
    while (HW_BLE_UART_GETCHAR() >= 0)
        ;

    _ble_uart_put(_TEST_CMD);

    timeout = os_timer_get() + _TEST_TMOUT_MS;
    while (os_timer_get() < timeout)
    {
        ch = HW_BLE_UART_GETCHAR();
        if (ch < 0)
        {
            OS_DELAY(1);
            continue;
        }

        if ((ch == '\r') || (ch == '\n'))
        {
            if (n == 0)
                continue;   // empty line

            line[n] = '\0';
            n = 0;
            LOG_DEBUGL(5, "test: \"%s\"", line);

            if (strstr(line, _TEST_REPLY) != NULL)
                return (true);

            continue;   // the module may answer with more than one line
        }

        if (n < (sizeof(line) - 1))
            line[n++] = (char)ch;
    }

    return (false);
}

bool ble_init(tty_parse_callback_t callback)
{
    HW_BLE_RST_INIT;
    HW_BLE_RST_LOW;
    HW_BLE_PWRC_INIT;
    HW_BLE_PWRC_HI;
    HW_BLE_UART_INIT(115200); // The module default is 9600
    _rx_callback = callback;
    OS_DELAY(1);
    HW_BLE_RST_HI;

    // Verify the module is really there and answering.  Nothing else is
    // reading UART4 at this point (ble_task() runs later, from task_app_slow),
    // so the reply can be collected here directly.
    OS_DELAY(_BOOT_DELAY_MS);

    if (! _ble_comm_test())
    {
        LOG_ERROR("no answer to " _TEST_CMD);
        return (false);
    }

    return (true);
}

static void _process_response(char *data, size_t len)
{   // here we parse only module related AT command responses
    // i.e. :
    // +VERSION=JDY-25M-V1.731

    if (data[0] != '+')
    {
        LOG_DEBUGL(3, "BLE: %s", data);
        return; // dont mind any response which does not begin with "+"
    }

    data++; // skip the "+"
    if (strcmp(data, "CONNECTED") == 0)
    {
        _clear_buf();
        _connected = true;
        LOG_DEBUG("connected");
    }
    else if (strcmp(data, "DISCONNECT") == 0)
    {
        _connected = false;
        LOG_DEBUG("disconnected");
    }
    else
    {
        LOG_INFO("BLE: %s", data);
    }
}

bool ble_connected(void)
{
    return (_connected);
}

void ble_cmd(const ascii *buf)
{
    HW_BLE_PWRC_LOW;
    OS_DELAY(100);
    while (*buf != '\0')
    {
        if (! HW_BLE_UART_PUTCHAR(*buf))
        {
            LOG_ERROR("send failed");
            break;
        }
        buf++;
    }
    HW_BLE_UART_PUTCHAR('\r');
    HW_BLE_UART_PUTCHAR('\n');

    OS_DELAY(100);
    HW_BLE_PWRC_HI;
}

void ble_send(const void *buf, size_t count)
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
            return; // overflow
        _tx_buf[idx] = *ptr++;
        _tx_buf_wr_idx = idx;
    }
}

void ble_task(void)
{
    // RX task
    s16 ch;
    u16 n = 0;

    // process UART RX data
    while ((ch = HW_BLE_UART_GETCHAR()) >= 0)
    {
        if ((ch == '\r') || (ch == '\n'))
        {
            ch = '\0';
        }
        _rx_buf[_rx_len] = ch;

        if (ch =='\0')
        {
            if (_rx_len>0)
            {
                _process_response(_rx_buf, _rx_len);

                if (_rx_callback != NULL)
                {
                    if (_connected)
                        _rx_callback(_rx_buf);
                }

                _rx_len = 0;
            }
        }
        else if (_rx_len < (_BUF_SIZE-1))
            _rx_len++;
    }

    // TX task
    if (! _connected)
        return;

    if (_tx_buf_wr_idx != _tx_buf_rd_idx)
    {
        size_t idx = _tx_buf_rd_idx + 1;
        ch = _tx_buf[_tx_buf_rd_idx];

        if (idx >= _BUF_SIZE)
            idx = 0;

        if (! HW_BLE_UART_PUTCHAR(ch))
            return;

        _tx_buf_rd_idx = idx;

        n++;
        if ((ch == '\n') && (n > 128))
            return; // add some chunking
    }
}
