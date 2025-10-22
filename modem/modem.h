#ifndef _MODEM_H
#define _MODEM_H

#include "common.h"
#include "buf.h"
#include "udp.h"

#define MODEM_ENABLED 1

#define MODEM_TIMEOUT_S    (5 * 1000)
#define MODEM_TIMEOUT_M   (10 * 1000)
#define MODEM_TIMEOUT_L   (60 * 1000)
#define MODEM_TIMEOUT_LL (900 * 1000)
#define MODEM_TIMEOUT_NET (300 * 1000)

// flags
#define MODEM_FLAG_ECHO        (1 << 0)
#define MODEM_FLAG_PIN_READY   (1 << 1) 
#define MODEM_FLAG_NET_READY   (1 << 2)
#define MODEM_FLAG_DATA_READY  (1 << 3)
#define MODEM_FLAG_ROAMING     (1 << 4)
#define MODEM_FLAG_CALL_STOP   (1 << 5)
#define MODEM_FLAG_CLCC_RQ     (1 << 6)
#define MODEM_FLAG_CCLK_RQ     (1 << 7)
#define MODEM_FLAG_LTE         (1 << 8)
#define MODEM_FLAG_FAIL        (1 << 9)
#define MODEM_FLAG_TEST        (1 << 30)

typedef enum {
    MODEM_MODEL_UNKNOWN,
    MODEM_MODEL_BG95, 

    MODEM_MODELS // size limit only
} modem_model_e;

typedef struct {

    const char *ask;     // read command
    const char *result;  // expected result
    const char *set;     // setup command
    const u16   timeout; //

} modem_config_t;

#define MODEM_AT_RX_BUF_SIZE 2048 // space for whole UDP packet (HEX)

typedef struct _modem_at_t {

    OS_SEMAPHORE(semaphore);
    u32 flags;
    ascii rx_buf[MODEM_AT_RX_BUF_SIZE];
    ascii response[MODEM_AT_RX_BUF_SIZE];
    u16 buf_len;
    u16 response_len;
    u16 last_error_result;
    bool busy;
    const ascii *user_str;

} modem_at_t;

typedef enum {
    MODEM_SIM_ST_PIN_INIT_RQ=0, // init didnt start
    MODEM_SIM_ST_ERROR,         // probably no SIM 
    MODEM_SIM_ST_PIN_CNT,       // needed PIN, but less than 3 attempts left
    MODEM_SIM_ST_PIN_OFF_READY, // PIN not required 
    MODEM_SIM_ST_PIN_READY      // PIN entered and is OK
} modme_sim_state_e;

typedef struct _modem_t {
    modem_at_t at;
    modme_sim_state_e sim;
    u32 flags;
    u32 error_counter;
    u32 signal_level;
    u32 sms_counter;
    u32 sms_error_counter;
    bool sms_storage_me;
    bool sleep;

    // io functions
    void (*pfunc_io_init)(void);
    void (*pfunc_on)(void);
    void (*pfunc_off)(void);
    void (*pfunc_sleep)(void);
    void (*pfunc_wakeup)(void);
    bool (*pfunc_tx_char)(u8 c);
    bool (*pfunc_rx_char)(u8 *c);

    // processing functions
    bool (*pfunc_init)(struct _modem_t *m);
    bool (*pfunc_check)(struct _modem_t *m); 
    bool (*pfunc_urc)(struct _modem_t *m);
    bool (*pfunc_udp_init)(struct _modem_t *m);
    bool (*pfunc_udp_send)(struct _modem_t *m, udp_packet_t *packet);
    void (*pfunc_udp_rx_task)(struct _modem_t *m);

} modem_t;

char *modem_parse_pattern(const char *s, const char *pattern);

void modem_hw_init(modem_t *m);
void modem_hw_on(modem_t *m);
void modem_hw_off(modem_t *m);
void modem_rx_task(modem_t *m);
bool modem_parse_urc(modem_t *m);
void modem_tx_data(modem_t *m, const u8 *data, size_t len);
bool modem_config_table(modem_t *m, const modem_config_t *table);
bool modem_start(modem_t *m);
bool modem_check(modem_t *m);
bool modem_apn_init(modem_t *m, const ascii *apn);
bool modem_data_connect(modem_t *m);
void modem_data_disconnect(modem_t *m);
void modem_data_rx_process(modem_t * m);

#endif // !_MODEM_H
