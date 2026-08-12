#include "os.h"
#include "event_type.h"
#include "util.h"
#include "rtc_lib.h"
#include "m_debug.h"

#include "cms_main.h"
#include "cms_sia_ip.h"
#include "gps.h"
#include "storage_interpreter.h"
#include "sms_lib.h"



#include    <stdio.h>

#define                THIS_SOURCE_ID  53

#define RETRY_DEFAULT 5

#define MIN_TIMEOUT     10  //==1s 
#define MAX_TIMEOUT     100 //==10s 
#define CMS_REPLY_ACK   BIT0
#define CMS_REPLY_NACK  BIT1
// static volatile u8  status=0;
// static volatile u16 ack_id=0;

const unsigned int SIA_IP_CRC_TABLE[]= {
    0x0000,0xc0c1,0xc181,0x0140,0xc301,0x03c0,0x0280,0xc241,
    0xc601,0x06c0,0x0780,0xc741,0x0500,0xc5c1,0xc481,0x0440,
    0xcc01,0x0cc0,0x0d80,0xcd41,0x0f00,0xcfc1,0xce81,0x0e40,
    0x0a00,0xcac1,0xcb81,0x0b40,0xc901,0x09c0,0x0880,0xc841,
    0xd801,0x18c0,0x1980,0xd941,0x1b00,0xdbc1,0xda81,0x1a40,
    0x1e00,0xdec1,0xdf81,0x1f40,0xdd01,0x1dc0,0x1c80,0xdc41,
    0x1400,0xd4c1,0xd581,0x1540,0xd701,0x17c0,0x1680,0xd641,
    0xd201,0x12c0,0x1380,0xd341,0x1100,0xd1c1,0xd081,0x1040,
    0xf001,0x30c0,0x3180,0xf141,0x3300,0xf3c1,0xf281,0x3240,
    0x3600,0xf6c1,0xf781,0x3740,0xf501,0x35c0,0x3480,0xf441,
    0x3c00,0xfcc1,0xfd81,0x3d40,0xff01,0x3fc0,0x3e80,0xfe41,
    0xfa01,0x3ac0,0x3b80,0xfb41,0x3900,0xf9c1,0xf881,0x3840,
//  0x2800,0xe8c1,0xe981,0x2940,0xbe01,0x2bc0,0x2a80,0xea41, // tady byla chyba v tabulce v norme !
    0x2800,0xe8c1,0xe981,0x2940,0xeb01,0x2bc0,0x2a80,0xea41,
    0xee01,0x2ec0,0x2f80,0xef41,0x2d00,0xedc1,0xec81,0x2c40,
    0xe401,0x24c0,0x2580,0xe541,0x2700,0xe7c1,0xe681,0x2640,
    0x2200,0xe2c1,0xe381,0x2340,0xe101,0x21c0,0x2080,0xe041,
    0xa001,0x60c0,0x6180,0xa141,0x6300,0xa3c1,0xa281,0x6240,
    0x6600,0xa6c1,0xa781,0x6740,0xa501,0x65c0,0x6480,0xa441,
    0x6c00,0xacc1,0xad81,0x6d40,0xaf01,0x6fc0,0x6e80,0xae41,
    0xaa01,0x6ac0,0x6b80,0xab41,0x6900,0xa9c1,0xa881,0x6840,
    0x7800,0xb8c1,0xb981,0x7940,0xbb01,0x7bc0,0x7a80,0xba41,
    0xbe01,0x7ec0,0x7f80,0xbf41,0x7d00,0xbdc1,0xbc81,0x7c40,
    0xb401,0x74c0,0x7580,0xb541,0x7700,0xb7c1,0xb681,0x7640,
    0x7200,0xb2c1,0xb381,0x7340,0xb101,0x71c0,0x7080,0xb041,
    0x5000,0x90c1,0x9181,0x5140,0x9301,0x53c0,0x5280,0x9241,
    0x9601,0x56c0,0x5780,0x9741,0x5500,0x95c1,0x9481,0x5440,
    0x9c01,0x5cc0,0x5d80,0x9d41,0x5f00,0x9fc1,0x9e81,0x5e40,
    0x5a00,0x9ac1,0x9b81,0x5b40,0x9901,0x59c0,0x5880,0x9841,
    0x8801,0x48c0,0x4980,0x8941,0x4b00,0x8bc1,0x8a81,0x4a40,
    0x4e00,0x8ec1,0x8f81,0x4f40,0x8d01,0x4dc0,0x4c80,0x8c41,
    0x4400,0x84c1,0x8581,0x4540,0x8701,0x47c0,0x4680,0x8641,
    0x8201,0x42c0,0x4380,0x8341,0x4100,0x81c1,0x8081,0x4040,
};

// normalni vypocet CRC
/*u16 calc_crc1(u16 crc, u8 ch)
{
    int i;

    for (i = 0; i < 8; i++) 
    {
        ch ^= crc & 1;     // PROCESS LSB
        crc >>= 1; 
        if (ch & 1)
            crc ^= 0xA001; // IF LSB SET,ADD FEEDBACK
        ch >>= 1;          // GO TO NEXT BIT
    }
    return crc;
}*/

// pres predvypocitanou tabulku (rychlejsi)
static u16 calc_crc2(u16 crc, u8 ch)
{
    return (crc >> 8) ^ (SIA_IP_CRC_TABLE[ch ^ (crc & 0xff)]);
}


static u16 calc_crc (u8 *data)
{
    u16 i;
    u16 crc=0;
    u8 ch;


    for (i=0; i<CMS_SIA_IP_MAX_MSG_LEN; i++)
    {
        ch = (*data++);
        if ((ch == '\0')    // cerstve sestavena zprava
         || (ch == 0x0D))   // normalni konec zpravy
            break;

//      crc = calc_crc1(crc, ch); 
        crc = calc_crc2(crc, ch); 
    }
    m_decho_n ("SIA calc_crc()", crc, LOG_SELECT_SIA);
    return (crc);
}

static const ascii *sia_dcs_translate_event (u16 event)
{
    return ("XX");
}

static u16 sia_dcs_translate_source (u16 source)
{   // pry totez jako u CID
    return (cms_cid_translate_source(source));
}

static bool sia_add_dcs_msg (buf_t *dest, u32 object_id, event_t *event)
{   
    const ascii *code;

    if ((code = sia_dcs_translate_event(event->evt)) == NULL)
        return (false);
    
    // 130225 ZAR: pred object-id nema byt "sharp" (ten je pouze v prvni casti, pred "["))
    // 130820 Vrobovec: "sharp" opět vrátit (nové doporučení)
    buf_append_fmt(dest, "#%X|Nri01", object_id); // 01 je cislo sekce, tady je to konstanta
    buf_append_fmt(dest, "/%s%X", code, sia_dcs_translate_source(event->src));
    
    buf_append_str(dest, "^");
    text_append_source (dest, event->src);
    buf_append_fmt(dest, "^");
    return (true);
}

static bool sia_msg_add_eventtext (buf_t *dest, event_t *event)
{
    if (! text_event_exist(event->evt))
        return (false);

    buf_append_str(dest, "[I");
    text_append_event (dest, event->evt);
    buf_append_str(dest, "]");
    return (true);
}

static bool sia_msg_add_eventtime (buf_t *dest, event_t *event)
{   // cas eventu
    rtc_t *t;

    t = &event->time;
    buf_append_fmt (dest, "[H%d:%02d:%02d]", t->s.hour, t->s.minute, t->s.second);
    return (true);
}

static bool sia_msg_add_timestamp (buf_t *dest) //, event_t *event)
{   // cas nemuzeme vzit z eventu (musi byt aktualni)
    // The CSR shall validate the timestamp against its own GMT reference. 
    // Encrypted messages with a GMT difference from the CSR greater than +20/-40 seconds shall be rejected with a NAK packet that contains the current GMT time known to the CSR in the data field
    rtc_t t;

//  rtc_convert_time (&t, event->s.time);
    rtc_get_time(&t);
    // The format of the timestamp is: <_HH:MM:SS,MM-DD-YYYY>.
    // length of the timestamp field is exactly 20 characters.
    buf_append_fmt (dest, "_%02d:%02d:%02d,%02d-%02d-%d", t.s.hour, t.s.minute, t.s.second, t.s.month, t.s.day, 2000+t.s.year);
    // _13:14:15,02-15-2006
    return (true);
}

u16  cms_sia_ip_build_msg (cms_sia_ip_state_t *s, buf_t *dataout, event_t *e, ascii *data)
{
    u8 *data_ptr;
    u16 crc;
    u16 out_len=0;
//  u16 encrypt_start;

    // message template (SIA-DC-09):
    // <LF><crc><0LLL>
    // <"id"><seq><Rrcvr><Lpref><#acct>[<pad>|...data...][x…data…]<timestamp>
    // <CR>
    buf_adjust (dataout, SIA_IDX_DATA);
    buf_append_char (dataout,  '"');
    if (s->enable_encrypt)
    {   // navic hvezdicka
        buf_append_char (dataout,  '*');
    }
    buf_append_fmt (dataout, "SIA-DCS\"%04dL%X#%" SCNlX "[", s->cnt, s->prefix, s->object_id);
    data_ptr = (u8 *)(buf_data(dataout)+SIA_IDX_DATA); // ukazatel na zacatek dat
//  encrypt_start = buf_length(dataout); // zde zacina oblast, kterou je mozne sifrovat AES
    if (! sia_add_dcs_msg(dataout, s->object_id, e))
    {
        m_error ("cms_sia_ip_build_msg() adding msg");
        return (0);
    }
    buf_append_char (dataout,  ']');// ukonceni datoveho pole zpravy
    
    if (s->enable_event_name)
    {   // pridame jmeno eventu 
        sia_msg_add_eventtext(dataout, e);
    }
    if (data != NULL)
    {   // najaka doplnkova informace
        buf_append_str(dataout, data);
    }
    if (s->enable_event_time)
    {   // pridame nestandardni info o case eventu
        sia_msg_add_eventtime(dataout, e);
    }
    if (s->enable_time_stamp)
    {   // pridame lokalni cas
        sia_msg_add_timestamp(dataout);
    }
    if (log_select & (1<<LOG_SELECT_SIA))
    {
        OS_PRINTF ("\r\n");
        OS_PRINTF ("%s\r\n", buf_data(dataout) + SIA_IDX_DATA);
    }
    // provedeme sifrovani
    if (s->enable_encrypt)
    {   // {{{ zapnute to je
        // zpravu [aaa|bbb]ccc<x0D> prevedem na [pad|aaa|bbb]ccc<x0D> a pak se zasifruje vse za "[" az k <x0D>
        // prijimac ma povinnost podporovat 3 varianty AES 128/192/256
        // a pak se to prevede na HEX (tedy dvojnasobna delka)
/*      ascii *buf;
        s16 data_len;
        s16 encrypt_len;
        u16 i;      
        u8 pad_len;

        data_len = buf_length(dataout)-encrypt_start;
        if (data_len<10)
        {   // nejaky nesmysl, toto by se nemelo nikdy stat
            return (0);
        }
        // nejdrive vlozit padding
        pad_len = 16 - ((data_len+SEPARATOR_LEN)&0xF); // "+SEPARATOR_LEN" - pridavame znak '|'

        encrypt_len=pad_len+SEPARATOR_LEN+data_len;
        // vejde se mi tam zprava+pad a to cele v hexu ?
        if (encrypt_start + 2*encrypt_len + 1 >=CMS_SIA_IP_MAX_MSG_LEN)
        {   // no nevejde :(
            m_error ("pco_sia_ip_build_msg() encrypt");
            return (0);
        }

        // When a message is already an even multiple of 16 bytes, 16 pad bytes shall be added to the message
        // Pad data shall be pseudo-random bytes which vary from one message to the next. 
        // This data will consist of binary values 0-255, except that it shall not contain the ASCII values for the character "|" (124, x7C), "[" (91, x5B) or "]" (93, x5D).
        
        if ((buf = (ascii *)OS_MEM_ALLOC(encrypt_len)) == NULL)
            return (0);
        // prekopiruju do pomcneho bufferu, protoze se to pak bude prevadet na hexdump
        memcpy(buf+pad_len+SEPARATOR_LEN, buf_data(dataout)+encrypt_start, data_len);
        // vlozime nahodu
        for (i=0;i<pad_len;i++)
        {
            buf[i]= sia_random();
        }
        buf[pad_len]='|'; // oddelovaci znak
        
        // ted skutecne zasifrovat
        if (! sia_encrypt((u8 *)buf, encrypt_len, encryption_key, cfg->specific.sia_ip.key_type))
        {
            OS_MEM_FREE(buf);
            return (0);
        }

        // In the encrypted region of the message, each byte shall be encoded for transmission as two ASCII characters (0-9, A-F) representing the hexadecimal value of the encrypted byte.
        // takze konverze na hex-dump
        buf_adjust (dataout, encrypt_start);
        for (i=0;i<encrypt_len;i++)
        {
            buf_append_char (dataout,  hex_to_ascii(buf[i]>>4));
            buf_append_char (dataout,  hex_to_ascii(buf[i]&0x0F));
        }
        OS_MEM_FREE(buf);
        if (log_select & (1<<LOG_SELECT_SIA))
        {
            data_ptr[buf_length(dataout)] = '\0';
            OS_PRINTF ("ENC:\r\n%s\r\n", buf_data(dataout) + SIA_IDX_DATA);
        }*/
    }   // }}}

    out_len  = buf_length(dataout) - SIA_IDX_DATA; // logicka delka dat
    crc      = calc_crc(data_ptr);

    OS_ASSERT ((SIA_IDX_DATA+out_len) < CMS_SIA_IP_MAX_MSG_LEN, "pco_sia_ip_build_msg() len");

    data_ptr = (u8 *)buf_data(dataout);
    data_ptr[0]='\x0A'; // LF

    // od verze 2013 je CRC jako %04X
    // pritom drive bylo binarne 16b BIG-ENDIAN
    // to by bylo nekompatibilni, ale kaslem na starou verzi ...
    
    // CRC je tam ulozeno jako big-endian
    sprintf((ascii *)(data_ptr+SIA_IDX_CRC), "%04X", crc);
    sprintf((ascii *)(data_ptr+SIA_IDX_LEN), "0%03X", out_len);
    *(data_ptr+SIA_IDX_DATA) = '"'; // predchozim sprintf se prepsala uvozovka nulou
    buf_append_char (dataout,  '\x0d'); // CR
    
    if (log_select & (1<<LOG_SELECT_SIA))
    {
        m_dump("\r\nHDR: ", data_ptr, SIA_IDX_DATA);
    }

    return (buf_length(dataout));
}

bool cms_sia_ip_rx (cms_sia_ip_state_t *s, u8 *udpdata, u16 len)
{
    if (len < 20)
        return (false);

    if (udpdata[0] != 0x0A)
        return (false); // kazda zprava musi zacinat '0x0A'

    if (len > CMS_SIA_IP_MAX_MSG_LEN)
    {
        m_error("cms_sia_ip_rx() too long");
        return (false);
    }
    else
    {
        int a,b;
        u16 crc;
        
        udpdata[len] = '\0'; // aby se s tim lepe pracovalo (bylo tu '0x0D')

        if (log_select & (1<<LOG_SELECT_SIA))
        {
            OS_PRINTF("\r\n");
            OS_PRINTF("DATA: %s ", udpdata+1);
        }

        if (sscanf((char *)udpdata+SIA_IDX_CRC, "%04X%04X", &a, &b) != 2)
            return (false);

        crc = a&0xFFFF;
        if (crc != calc_crc(udpdata + SIA_IDX_DATA))
        {
            m_decho_n ("CRC failed, RX crc", crc, LOG_SELECT_SIA);
            return (false);
        }
        len-=(SIA_IDX_DATA+1);
        if (len != b)
        {
            m_decho_n ("LEN failed, RX ", len, LOG_SELECT_SIA);
            return (false);
        }
    }
    // je to platna SIA DC9 zprava
    return (true);
}


bool cms_sia_ip_reply_wait (cms_sia_ip_state_t *s, u8 uri)
{
    return (false);
}
