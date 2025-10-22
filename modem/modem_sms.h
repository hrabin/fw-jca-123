#ifndef MODEM_SMS_H
#define MODEM_SMS_H

#include "type.h"
#include "modem.h"
#include "sms.h"

#define	MODEM_SMS_MAX 0xFF

extern void modem_sms_init (modem_t *m);
extern bool modem_sms_send_now (modem_t *m, sms_struct_t *sms);
extern bool modem_sms_unso_pdu_parse (modem_t *m, ascii *pdu_data);
extern bool modem_sms_read (modem_t *m, sms_struct_t *msg);
extern bool modem_sms_delete (modem_t *m, u16 n);

#endif // ! MODEM_SMS_H
