#ifndef MULTI_SMS_H
#define MULTI_SMS_H

#include "type.h"

#define MSMS_BUF_LEN    10

enum {
    MS_SMS_ERROR = 0,
    MS_SMS_WAIT  = 1,
    MS_SMS_DONE  = 2
};



extern void ms_init (void);
extern u16 ms_incomming (ascii *sms_text, u16 len, u8 id, u8 order, u8 total);
extern u16 ms_get (ascii **sms_text, u8 id);
extern void ms_delete (u8 id);


#endif // MULTI_SMS_H
