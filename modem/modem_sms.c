#include "os.h"

#include "sms.h"
#include "multi_sms.h"
#include "modem_main.h"
#include "modem_sms.h"
#include "modem.h"
#include "modem_at.h"
#include "const.h"
#include "text_lib.h"
#include "app.h"
#include "util.h"
#include "log.h"

LOG_DEF("M_SMS");

#include "pdu/pdu.h"

void modem_sms_init (modem_t * m)
{

}

bool send_pdu_now (modem_t *m, u8 *pdu_raw_data, int data_len, u8 *sms_id)
{
    ascii tmp_buf[32];
    bool result = false;
    int retry = 3;

    if (data_len < 0)
        return (false);

    modem_at_lock (m);
    while (retry--)
    {
        app_main_led_single (APP_LED_SMS_SENDING);
        sprintf (tmp_buf, "AT+CMGS=%d", data_len); 
        modem_at_cmd_nolock (m, tmp_buf);
        // result is ">" then awaiting PDU string
        if (modem_at_response (m, NULL, AT_ST_RAB, MODEM_TIMEOUT_S))
        {   // timeout for ">" is 1s
            // send only "PDU" (no any other characters i.e. "PDU\r") 
            modem_at_raw (m, (u8 *)pdu_raw_data, strlen((ascii *)pdu_raw_data));
            modem_at_raw (m, (u8 *)"\x1A", 1); // ctrl-Z == ack
            if (modem_at_read_line_lite (m, tmp_buf, 16, "+CMGS: ", MODEM_TIMEOUT_LL))
            {   // got "+CMGS", so SMS sent
                // remember SMS ID for status report if requested
                int id=atoi(tmp_buf+7);
                LOG_DEBUGL(4, "%s", tmp_buf);
                *sms_id = id&0xFF;

                OS_DELAY (1000);
                result = true;
                break; 
            }
            else
            {
                LOG_ERROR("send_pdu_now(), code=%d", m->at.last_error_result);
                // +CMS ERROR:  41 == "Temporary Failure", may be that the device is not registered on any networ
                // +CMS ERROR:  42 == network congestion Retry later
                // +CMS ERROR:  96 == Mandatory information missing
                // +CMS ERROR: 330 == SMSC address unknown
                // +CMS ERROR: 331 == No network service
                switch (m->at.last_error_result)
                {
                case 331: // tuhle chybu to obcas hlasilo i kdyz na siti byl, normalne zpracoval prichozi SMS a pak NEeposlal odpoved
                    // jenze 331 to hlasi i kdyz je nesmyslne cislo, takze radsi nebudu provadet init
                    break;
                }
            }
        }
        modem_at_raw (m,(u8 *)"\x1B", 1); // ESC    == prerusit
        if (m->at.last_error_result == 515)
        {
            OS_DELAY (5000);
        }
        OS_DELAY (1000);
    }
    modem_at_unlock (m);
    return (result);
}

static u16 _memncpy (u8 *dest, u8 *src, u16 len, u16 limit)
{
    u16 i;
    for (i=0; i<limit; i++)
    {
        if (len--)
        {
            *(dest++) = *(src++);
        }
        else
        {
            return (i);
        }
    }
    return (limit);
}

bool modem_sms_send_now (modem_t *m, sms_struct_t *sms)
{
    pdu_t pdu;
    ascii *raw_data;
    int pdu_len;
    u16 tmp;

    u8 *data_ptr;
    s16 len;
    u8 max_sms_len;
    u8 i;

    memset(&pdu, 0, sizeof(pdu));

    raw_data = (ascii *)OS_MEM_ALLOC(PDU_MAX_LENGTH);
    pdu.content  = (ascii *)OS_MEM_ALLOC(PDU_MAX_SMS_LEN+1);
    OS_ASSERT(raw_data != NULL, "mem");
    OS_ASSERT(pdu.content != NULL, "mem");

    pdu.tel_num = sms->tel_num;
    pdu.count   = 1;
    pdu.sr      = sms->sr;

    data_ptr = sms->data; // zdroj dat

    if (sms->type == SMS_TYPE_AUTO)
    {   // decide what coding to use UTF8/ASCII
        if (text_is_ascii((ascii *)sms->data))  // TODO: this is not enough, need to use coding table 
            sms->type = SMS_TYPE_TEXT7;
        else
            sms->type = SMS_TYPE_UTF8;
    }
    pdu.data_type = PDU_DATA_RAW;

    switch (sms->type)
    {
    case SMS_TYPE_TEXT7:
        sms->len = strlen ((ascii *)sms->data);
        if ((sms->len) > 160)
            max_sms_len=153;
        else
            max_sms_len=160;

        pdu.type = PDU_TYPE_TEXT7;
        break;

    case SMS_TYPE_DATA8:
        pdu.type = PDU_TYPE_TEXT8;
        if ((sms->len) > 140)
            max_sms_len=134;
        else
            max_sms_len=140;
        break;

    case SMS_TYPE_UTF8:
        // we have data in UTF8 so we nned to convert to UCS2
        // now we need compute number of characters
        sms->len = text_utf8_to_ucs2 (NULL, (ascii *)data_ptr, NULL, 512);
        // continue to UCS2
    case SMS_TYPE_UCS2:
        pdu.type = PDU_TYPE_UCS2;

        if ((sms->len) > 70)
            max_sms_len=67;
        else
            max_sms_len=70;
        break;

    default:
        goto g_s_s_false;
    }

    // max_sms_len is limit for characters in one part of SMS
    // compute how many SMS we need  
    pdu.count=0;
    for (len=0; len<(sms->len); len+=max_sms_len)
    {
        pdu.count++;
    }
    if (pdu.count==0)
        pdu.count=1; // empty SMS also send

    data_ptr = sms->data;

    len = sms->len; // overall number of characters to send (encoding independent)
    for (i=1; i<=pdu.count; i++)
    {
        switch (sms->type)
        {
        case SMS_TYPE_UTF8: // encode to UCS2
            len -= text_utf8_to_ucs2 (pdu.content, (ascii *)data_ptr, &tmp, max_sms_len);
            data_ptr += tmp;
            pdu.size = tmp*2;
            break;

        case SMS_TYPE_UCS2: //  we have data prepared int UCS2
            pdu.size = _memncpy ((u8 *)pdu.content, data_ptr, len<<1, max_sms_len<<1);
            data_ptr+=pdu.size;
            len -= (pdu.size / 2);  // update missing length
            break;

        default:
            pdu.size = _memncpy ((u8 *)pdu.content, data_ptr, len, max_sms_len);
            pdu.content[pdu.size]='\0';
            data_ptr+=pdu.size;
            len -= pdu.size;  // update missing length
        }

        pdu.nr=i;
        OS_PRINTF("[SMS %d of %d, l=%d]", i, pdu.count, pdu.size);

        pdu_len = pdu_encode(raw_data, &pdu);
        LOG_DEBUGL(LOG_SELECT_PDU, "PDU: \"%s\"", raw_data);

        // PDU content prepared, send it
        if (!send_pdu_now (m, (u8 *)raw_data, pdu_len, &(sms->id)))
        {
g_s_s_false:
            OS_MEM_FREE (pdu.content);
            OS_MEM_FREE (raw_data);
            app_main_led_single (APP_LED_SMS_ERROR);
            m->sms_error_counter++;
            return (false);
        }
    }

    OS_MEM_FREE (pdu.content);
    OS_MEM_FREE (raw_data);
    m->sms_error_counter=0;
    return (true); 
}


bool modem_sms_unso_pdu_parse (modem_t * m, ascii *pdu_data)
{   // parse unsolicited PDU
    // its onlny needed  for GSM modems which cant set AT+CNMI=x,x,x,2,x
    pdu_t   pdu;
    s16 msg_len=0;

    pdu_init(&pdu);
    pdu.tel_num=(ascii *)OS_MEM_ALLOC (MAX_PHONENUM_LEN);
    pdu.content=(ascii *)OS_MEM_ALLOC (PDU_MAX_SMS_LEN+1);
    OS_ASSERT(pdu.tel_num != NULL, "mem");
    OS_ASSERT(pdu.content != NULL, "mem");

    msg_len = pdu_decode(&pdu, pdu_data); 
    if ((msg_len>0)
     && (pdu.type == PDU_TYPE_SR))
    {   // 
        modem_main_delivery_report (*(pdu.content), *(pdu.content+1)); 
    }

    OS_MEM_FREE (pdu.content);
    OS_MEM_FREE (pdu.tel_num);
    return (true);
}

bool modem_sms_read (modem_t * m, sms_struct_t *sms)
{
    pdu_t   pdu;
    ascii  *raw_data;

    u32 response;
    u16 sms_id;
    s16 msg_len=0;
    bool sms_ready=false;   

    pdu_init(&pdu);
    raw_data=(ascii *)OS_MEM_ALLOC(PDU_MAX_LENGTH);
    pdu.content=(ascii *)OS_MEM_ALLOC(PDU_MAX_SMS_LEN+1);
    OS_ASSERT(raw_data != NULL, "mem");
    OS_ASSERT(pdu.content != NULL, "mem");

    pdu.tel_num = sms->tel_num;

#define _tmp_buf raw_data 
    *pdu.tel_num='\0';

    pdu.type    = PDU_TYPE_UNKNOWN;
    
    if (m->sms_storage_me)
    {   // some modems sometimes receive SMS into "ME" memory even if disabled
        // (noticed on some Telit modems)
        modem_at_ok_cmd (m, "AT+CPMS=\"ME\"");
        m->sms_storage_me=false;
    }
    modem_at_lock(m);
    modem_at_cmd_nolock (m, "AT+CMGL=4");
    if (modem_at_read_line_lite (m, _tmp_buf, 32, "+CMGL: ", 200))
    {
        sms_id = atoi (_tmp_buf+7);
        LOG_DEBUGL(7, "sms_id=%d", sms_id);
        if (modem_at_read_line_lite (m, raw_data, PDU_MAX_LENGTH, NULL, 200))
        {   // PDU read ok
            LOG_DEBUGL(LOG_SELECT_PDU, "PDU: \"%s\"", raw_data);
            msg_len = pdu_decode(&pdu, raw_data); // WARNING: it may reallocate sms.data in case multiple parts SMS
            OS_PRINTF("[t: %s]", pdu.tel_num); 

            // process only one SMS at time, so wait for OK
            do 
            {   // using "+CMGL" extent waiting, some modems need it ...
                response = modem_at_response (m, (ascii *)"+CMGL: ", AT_ST_OK|AT_ST_ERROR|AT_ST_USER_STR, MODEM_TIMEOUT_L);
                if (response & AT_ST_OK)
                    break;
                m->at.flags &= ~(AT_ST_OK|AT_ST_ERROR|AT_ST_USER_STR);
            }
            while (response & AT_ST_USER_STR); // loop while "+CMGL" incoming

            if (response & AT_ST_OK)
            {   // got OK == end of list
                if (msg_len>=0)
                    sms_ready=true;

                m->sms_counter--;
            }
            else
            {
                LOG_ERROR("SMS reading");
            }
            // erase the SMS
            sprintf (_tmp_buf,"AT+CMGD=%d", sms_id);
            modem_at_cmd_nolock (m, _tmp_buf);
        }
        else
        {   // no any PDU data
            m->sms_counter=0;
        }
    }
    else
    {   // no any response, this is error
        LOG_ERROR("CMGL, no response");
        m->sms_counter=0;
    }
    modem_at_unlock(m);
    // dont need pdu.raw_data any more
    OS_MEM_FREE (raw_data);

    if (sms_ready)
    {   // reading OK, check if it is split message 
        if (pdu.count)
        {   // this is long SMS divided into multiple SMS
            switch ( ms_incomming (pdu.content, pdu.size, pdu.id, pdu.nr, pdu.count) )
            {
            case MS_SMS_WAIT:
                sms_ready=false;
                break;

            case MS_SMS_DONE:
                LOG_DEBUG("Long SMS READY");
                // WARNING: ms_get reallocate memory for long SMS
                msg_len = ms_get ((ascii **)&(pdu.content), pdu.id); 
                ms_delete (pdu.id);
                break;

            default: // MS_SMS_ERROR
                sms_ready=false;
            }
        }
    }

    if ((sms_ready) &&  // 
        (msg_len>0))    // zero length SMS may be used for time synchronization
    {
        if (pdu.type == PDU_TYPE_SR)
            sms->type=SMS_TYPE_SR;
        else if (pdu.type == PDU_TYPE_TEXT8)
            sms->type=SMS_TYPE_DATA8; // data SMS
        else
            sms->type=SMS_TYPE_UTF8;

        sms->data    = (u8 *)pdu.content;
        sms->len     = msg_len;

        sms->time.year  = pdu.timestamp[0];
        sms->time.month = pdu.timestamp[1];
        sms->time.day   = pdu.timestamp[2];
        sms->time.hour  = pdu.timestamp[3];
        sms->time.minute= pdu.timestamp[4];
        sms->time.second= pdu.timestamp[5];

        // dont free memory, return the data
        return (true);
    }
    // no processing, so free memory
    if (pdu.content != NULL)
        OS_MEM_FREE (pdu.content);
    return (false);
}



bool modem_sms_delete (modem_t *m, u16 n)
{
    bool result=false;
    ascii *tmp_buf;

    if (NULL == (tmp_buf=(ascii *)OS_MEM_ALLOC (32)))
        return (false);
    
    sprintf (tmp_buf,"\r\nAT+CMGD=%d\r\n", n);  
    modem_at_lock(m);
    modem_at_cmd_nolock (m, tmp_buf);

    if (modem_at_response (m, NULL, AT_ST_OK | AT_ST_ERROR, MODEM_TIMEOUT_M) & AT_ST_OK)
        result=true;

    OS_MEM_FREE (tmp_buf);
    modem_at_unlock(m);
    return (result);
}

