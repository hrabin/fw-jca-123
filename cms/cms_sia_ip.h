#ifndef CMS_SIA_IP_H
#define CMS_SIA_IP_H

#include "type.h"
#include "event_type.h"
#include "buf.h"

#define SIA_IDX_CRC     1
#define SIA_IDX_LEN     5
#define SIA_IDX_DATA    9

#define CMS_SIA_IP_MAX_MSG_LEN      240
#define CMS_SIA_IP_MAX_PACKET_LEN  (CMS_SIA_IP_MAX_MSG_LEN+SIA_IDX_DATA)
#define CMS_SIA_IP_NUM_URIS         2

typedef struct {
    u32 timeout;
    u32 object_id;
    u16 wait_tm[CMS_SIA_IP_NUM_URIS];   // doba cekani na odpoved (meni se dynamicky)
    u16 cnt;        // counter pro identifikaci paketu (1..9999)
    u16 prefix;     // 
    u8  retry_num[CMS_SIA_IP_NUM_URIS]; // hlavni/zalozni URL retry num
    u8  cms_num;    // cislo CMS ke kteremu patri tato struktura
    u8  enable_time_sync:1;
    u8  enable_encrypt:1;
    u8  enable_event_name:1;
    u8  enable_event_time:1;
    u8  enable_time_stamp:1;
} cms_sia_ip_state_t;

extern void cms_sia_ip_init (cms_sia_ip_state_t *s);
extern u16  cms_sia_ip_build_msg (cms_sia_ip_state_t *s, buf_t *dataout, event_t *e, ascii *data);
extern bool cms_sia_ip_rx (cms_sia_ip_state_t *s, u8 *udpdata, u16 len);
extern bool cms_sia_ip_reply_wait (cms_sia_ip_state_t *s, u8 uri);
extern void cms_sia_ip_tick (cms_sia_ip_state_t *s);

#endif // ~CMS_SIA_IP_H
