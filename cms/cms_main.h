#ifndef CMS_MAIN_H
#define CMS_MAIN_H

#ifndef NO_PCO

#define CMS_IPS (2) // pocet IP adres jednoho PCO

extern bool cms_init(void);
extern bool cms_reinit(void);
extern bool cms_new_event(event_t *event);
extern u16  cms_cid_translate_event (u16 event);
extern u16  cms_cid_translate_source (u16 src);
extern bool cms_get_next_event (event_t *event, u8 p);
extern bool cms_error (void);
extern void cms_tracking_start (void);
extern void cms_tracking_stop (void);
extern void cms_sms_delivery (u8 sms_id, bool result);
extern u8   cms_main_delivered(u8 p, u16 eid, u8 rest_mask);
extern void cms_main_not_delivered (u8 p, u16 eid);
extern void cms_main_discard (u8 mask, u16 eid);
extern bool cms_udp_rx (u8 *data, u16 len);
extern bool cms_main_process(void);
extern void cms_busy_task (void);
extern void cms_main_tick(void);

#else // ~NO_PCO
  __inline bool cms_init(void) {OS_PUTTEXT("[OFF]");return (true);}
  __inline bool cms_main_process(void) {return (true);}
  #define cms_reinit()
  #define cms_main_tick()

#endif // NO_PCO

#endif // ~CMS_MAIN_H

