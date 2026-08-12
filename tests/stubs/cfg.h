// Stub for host-side testing — configuration not available.
#ifndef CFG_H
#define CFG_H

#include "type.h"
#include "buf.h"

#define CFG_ITEM_SIZE 62

typedef u16 cfg_id_t;
typedef u8  access_auth_t;

#define ACCESS_SYSTEM (8)

#define CFG_ID_APN          (0)
#define CFG_ID_SIM_PIN      (0)
#define CFG_ID_CMS_SERVER_ADDR  (0)
#define CFG_ID_CMS_ACCOUNT  (0)

#define CFG_ID_TEXT_EVENT_NAME_000   (1000)
#define CFG_ID_TEXT_EVENT_NAME_001   (1001)
#define CFG_ID_TEXT_EVENT_NAME_002   (1002)
#define CFG_ID_TEXT_EVENT_NAME_003   (1003)
#define CFG_ID_TEXT_EVENT_NAME_004   (1004)
#define CFG_ID_TEXT_EVENT_NAME_005   (1005)
#define CFG_ID_TEXT_EVENT_NAME_006   (1006)
#define CFG_ID_TEXT_EVENT_NAME_007   (1007)
#define CFG_ID_TEXT_EVENT_NAME_008   (1008)
#define CFG_ID_TEXT_EVENT_NAME_010   (1010)
#define CFG_ID_TEXT_EVENT_NAME_011   (1011)
#define CFG_ID_TEXT_EVENT_NAME_012   (1012)
#define CFG_ID_TEXT_EVENT_NAME_013   (1013)
#define CFG_ID_TEXT_EVENT_NAME_014   (1014)
#define CFG_ID_TEXT_EVENT_NAME_015   (1015)
#define CFG_ID_TEXT_EVENT_NAME_016   (1016)

#define CFG_ID_TEXT_SOURCE_NAME_000  (1100)
#define CFG_ID_TEXT_SOURCE_NAME_001  (1101)
#define CFG_ID_TEXT_SOURCE_NAME_002  (1102)
#define CFG_ID_TEXT_SOURCE_NAME_003  (1103)
#define CFG_ID_TEXT_SOURCE_NAME_004  (1104)
#define CFG_ID_TEXT_SOURCE_NAME_005  (1105)
#define CFG_ID_TEXT_SOURCE_NAME_006  (1106)
#define CFG_ID_TEXT_SOURCE_NAME_007  (1107)
#define CFG_ID_TEXT_SOURCE_NAME_008  (1108)
#define CFG_ID_ADMIN_NAME            (1109)
#define CFG_ID_USER1_NAME            (1110)
#define CFG_ID_USER2_NAME            (1111)
#define CFG_ID_USER3_NAME            (1112)
#define CFG_ID_USER4_NAME            (1113)

static inline bool cfg_read(buf_t *dest, cfg_id_t id, access_auth_t auth)
{
    (void)dest;
    (void)id;
    (void)auth;
    return (false);
}

static inline ascii *cfg_read_static(cfg_id_t id)
{
    (void)id;
    return (NULL);
}

#endif
