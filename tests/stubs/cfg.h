// Stub for host-side testing — configuration not available.
#ifndef CFG_H
#define CFG_H

#include "type.h"
#include "buf.h"

#define CFG_ITEM_SIZE 62

typedef u16 cfg_id_t;
typedef u8  access_auth_t;

#define ACCESS_SYSTEM (8)

#define CFG_ID_APN      (0)
#define CFG_ID_SIM_PIN  (0)

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
