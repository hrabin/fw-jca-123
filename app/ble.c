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

bool ble_init(tty_parse_callback_t callback)
{
    HW_BLE_RST_INIT;
    HW_BLE_RST_LOW;
    HW_BLE_PWRC_INIT;
    HW_BLE_PWRC_HI;
    HW_BLE_UART_INIT(115200);
    _rx_callback = callback;
    OS_DELAY(1);
    HW_BLE_RST_HI;
    return (true);
}

static void _process_response(char *data, size_t len)
{   // here we parse only module related AT command responses
    // i.e. :
    // +VERSION=JDY-25M-V1.731

    if (data[0] != '+')
        return; // dont mind any response which does not begin with "+"

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
    int ch;
    int n = 0;
    
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
