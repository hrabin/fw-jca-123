#ifndef CMD_H
#define CMD_H

#include "common.h"
#include "buf.h"
#include "text.h"
#include "access.h"


#define CMD_NEW_LINE NL

#define CMD_USE_HELP 1

#define CMD_MAX_LEN (64) // max text length of single command 


typedef enum {

    CMD_ASYNC_NONE = 0,
	CMD_ASYNC_OFF,
	CMD_ASYNC_REBOOT,

    CMD_ASYNC_SIZE

} cmd_async_e;


extern const char CMD_ERR_MSG_BUSY[];
extern const char CMD_ERR_MSG_INVALID_PARAM[];
extern const char CMD_ERR_MSG_MISSING_PARAM[];
extern const char CMD_ERR_MSG_NOT_AVAILABLE[];
extern const char CMD_ERR_MSG_NOT_IMPLEMENTED[];
extern const char CMD_ERR_MSG_UNHANDLED[];
extern const char CMD_ERR_MSG_UNKNOWN_COMMAND[];

typedef struct _cmd_t {

    const char *text;
    bool (*pfunc)(buf_t *result, const struct _cmd_t *cmd, access_t *access);
    bool (*pfunc_set)(buf_t *result, const struct _cmd_t *cmd,  const char **pptext, access_t *access);
	u32 flags;
	access_auth_t auth;
#if CMD_USE_HELP == 1
    const char *help;
#endif // CMD_USE_HELP == 1
} cmd_t;

#if CMD_USE_HELP == 1
  #define CMD_DEF(cmd,fn,fns,flags,auth,help) {cmd,fn,fns,flags,auth,help}
#else // CMD_USE_HELP == 1
  #define CMD_DEF(cmd,fn,fns,flags,auth,help) {cmd,fn,fns,flags,auth}
#endif // CMD_USE_HELP != 1


typedef struct {

    const char *id;
    void (*read)(buf_t *result);
    bool (*write)(buf_t *result, const char **pptext);

} cmd_item_t;

typedef s32 cmd_int_t;

typedef bool (*cmd_proxy_pfunc_t)(buf_t *result, const char *text);

void cmd_string(buf_t *result, const char *text);
void cmd_not_available(buf_t *result);
void cmd_nl(buf_t *result);
void cmd_reply(buf_t *result, const cmd_t *cmd);
bool cmd_fetch_num(cmd_int_t *dest, const char **pptext);
bool cmd_fetch_separator(const char **pptext);
bool cmd_fetch_string(char *dest, const char **pptext, size_t limit);
bool cmd_fetch_addr(addr_t *dest, const char **pptext);
bool cmd_off(buf_t *result);
bool cmd_reboot(buf_t *result);

buf_t *cmd_get_buf(void);
void cmd_return_buf(buf_t *buf);

bool cmd_read_all_items(buf_t *result, const cmd_item_t *items, const char *cmd_text);
bool cmd_rw_item(buf_t *result, const cmd_item_t *items, const char *cmd_text, const char **pptext);
#define cmd_read_item cmd_rw_item
bool cmd_error(buf_t *result, const char *text);


bool cmd_init(void);
size_t cmd_get_password_access(access_t *access, const ascii *text);
bool cmd_process (buf_t *result, buf_t *cmd_text);
bool cmd_process_text(buf_t *result, const char *text, access_t *access);
bool cmd_sms_process (buf_t *result, ascii *sms_text, ascii *phone_num, u16 sms_len);

#endif // ! CMD_H

