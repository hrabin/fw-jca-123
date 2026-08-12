#ifndef CMS_MAIN_H
#define CMS_MAIN_H

#include "event.h"

#ifndef NO_CMS

bool cms_init(void);
bool cms_reinit(void);
bool cms_new_event(event_t *event);
void cms_udp_rx(u8 *data, u16 len);
void cms_main_process(void);
void cms_tick(void);

#else // ~NO_CMS

#define cms_init()      (true)
#define cms_reinit()    (true)
#define cms_new_event(e) (true)
#define cms_udp_rx(d,l)
#define cms_main_process()
#define cms_tick()

#endif // NO_CMS

#endif // ~CMS_MAIN_H
