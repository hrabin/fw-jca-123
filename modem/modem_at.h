#ifndef MODEM_AT_H
#define MODEM_AT_H

#include "type.h"
#include "modem.h"

#define     AT_ST_OK            (1 << 0)    // "OK"
#define     AT_ST_ERROR         (1 << 1)    // "ERROR"
#define     AT_ST_USER_STR      (1 << 2)
#define     AT_ST_RAB           (1 << 3)    // ">" (Right Angle Bracket)
#define     AT_ST_RESET         (1 << 4)    // reset response flags
#define     AT_ST_READY         (1 << 5)    // 
#define     AT_ST_LINE          (1 << 6)    // new line detected

const ascii *modem_at_param_pos(const ascii * data, u16 n);

void modem_at_init(modem_t *m);
void modem_at_rx(modem_t *m, u8 rx_char);
void modem_at_lock(modem_t *m);
void modem_at_unlock(modem_t *m);
bool modem_at_busy(modem_t *m);
bool modem_at_cmd_nolock(modem_t *g, const ascii *at_cmd);
bool modem_at_cmd(modem_t *m, const ascii *at_cmd);
bool modem_at_ok_cmd(modem_t *g, const ascii *at_cmd);
bool modem_at_ok_cmd_nolock(modem_t * m, const ascii * at_cmd);
bool modem_at_response_ok(modem_t *m, const ascii *at_cmd, const ascii *user_str);
bool modem_at_cmd_get_response(modem_t *m, buf_t *dest, const ascii *at_cmd, const ascii *user_str);
u16 modem_at_read_line (modem_t *m, buf_t *dest, const ascii *user_str, u32 timeout);
u16 modem_at_read_line_lite (modem_t *g, ascii *dest, u16 limit, const ascii *user_str, u32 timeout);
ascii *modem_at_get_respone(modem_t *m, u32 timeout);
void modem_at_raw (modem_t *g, u8 *data, u16 len);
u32 modem_at_response(modem_t *m, const ascii *user_str, u32 flags, u32 timeout);


#endif // MODEM_AT_H
