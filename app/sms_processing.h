#ifndef SMS_PROCESSING_H
#define SMS_PROCESSING_H

#include "type.h"

void sms_reinit (void);
bool sms_process (void);
bool sms_send_to_user(u16 user, buf_t *data);

#endif // SMS_PROCESSING_H

