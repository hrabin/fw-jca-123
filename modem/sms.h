#ifndef SMS_H
#define SMS_H

#include "type.h"
#include "rtc.h"

#define SMS_MAX_PHONE_LEN 20


typedef enum {
	SMS_TYPE_TEXT7=0,
	SMS_TYPE_DATA8,
	SMS_TYPE_UCS2,
//	SMS_TYPE_RAW8,
	SMS_TYPE_UTF8,
	SMS_TYPE_AUTO,
	SMS_TYPE_SR,	// status report
	SMS_TYPE_UNKNOWN
} sms_type_t;

typedef struct {	// struktura pro sms
	u8 *data;		// obecna data, libovolne delky (pripadne rozdeli na vic SMS)
	ascii tel_num[SMS_MAX_PHONE_LEN]; // 
	u16 len;		//  
	sms_type_t type;// format vstupnich dat, vystup vzdy UTF8 nebo SR
	u8 id;			// identifikace odchozi SMS - cekani na dorucenku
	bool sr;		// status report request 
	rtc_t time;		// cas z dorucene SMS
} sms_struct_t;


#endif // SMS_H
