#include "common.h"
#include "cfg.h"
#include "cmd.h"
#include "const.h"
#include "log.h"
#include "modem_main.h"
#include "sms.h"
#include "text_lib.h"

LOG_DEF("SMSP");

static u32 sms_counter = 0;

void sms_reinit (void)
{

}

void sms_parse(sms_struct_t *sms)
{
    buf_t sms_result;
    sms_struct_t result_sms;

    LOG_DEBUG("SMS %d from %s", ++sms_counter, sms->tel_num);

    if (sms->len == 0)
        return; // zero length is not valid SMS (may be used for time sync)

    // TODO: detect loop
    
    if (! buf_init (&sms_result, NULL, MAX_SMS_LEN))
    {
        return;
    }

    if (sms->type == SMS_TYPE_UTF8)
        OS_PRINTF ("D: \"%s\"" NL, (ascii *)sms->data);
    else
        OS_PRINTF ("<DATA>" NL);

    if (cmd_sms_process (&sms_result, (ascii *)sms->data, sms->tel_num, sms->len))
    {   //
        OS_DELAY (100);

        if ((buf_length(&sms_result)) > 0)
        {
            memcpy(result_sms.tel_num, sms->tel_num, MAX_PHONENUM_LEN);

            result_sms.data = (u8 *)buf_data(&sms_result);
            result_sms.type = SMS_TYPE_AUTO;
            result_sms.sr   = false;

            if (text_is_valid_phone(result_sms.tel_num))
                modem_main_sms_send (&result_sms);
        }
        else
        {   // no result, dont reply anything
        }
    }

    buf_free (&sms_result);
}

void sms_report_prosess(u8 sr_id, u8 sr_result)
{
    LOG_DEBUG("SR: %02x,%02x", sr_id, sr_result); 
}

bool sms_process (void)
{
    if (modem_main_sms_incomming ())
    {
        sms_struct_t sms;
        if (modem_main_sms_read (&sms))
        {   // "true" means data ready in newly allocated memory
            if (sms.type==SMS_TYPE_SR)
                sms_report_prosess (*(sms.data), *(sms.data+1));
            else
                sms_parse (&sms); // received standard SMS, process it

            // there was allocated memory which we dont need any more
            OS_MEM_FREE (sms.data);
        }
        return (false);
    }
    // if there was "false", no memory allocated
    return (true);
}

bool sms_send_to_user(u16 user, buf_t *data)
{
    sms_struct_t sms;
    buf_t buf;

    OS_ASSERT(user < 4, "user >= 4");

    buf_init(&buf, sms.tel_num, MAX_PHONENUM_LEN);
    if (! cfg_read(&buf, CFG_ID_USER1_PHONE + user, ACCESS_SYSTEM))
        return (false);

    if (! text_is_valid_phone(sms.tel_num))
    {
        if (sms.tel_num[0] == '\0')
            LOG_WARNING("empty phone number");
        else
            LOG_ERROR("invalid phone number");

        return (true); // return "true" == dont retry sending
    }

    sms.data = (u8 *)buf_data(data);
    sms.type = SMS_TYPE_AUTO;
    sms.sr   = false;
    modem_main_sms_send (&sms);
    return (true);
}


