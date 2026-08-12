/// \section{Vzdálená komunikace na PCO}
/// Zařízení umí komunikovat na pulty centrální ochrany pomocí UDP a SMS.
/// \\ Podporovaný je typ JABLO-IP a JABLO-SMS.
///

#include "os.h"
#include "os_task.h"

#include "cms_event_buffer.h"
#include "event_defs.h"
#include "event_type.h"
#include "storage_interpreter.h"
#include "event_processing.h"
#include "buf.h"
#include "util.h"
#include "text_lib.h"
#include "net_main.h"
#include "app_main.h"
#include "alarm.h"
#include "m_debug.h"

#include "cms_main.h"
#include "cms_jablo_ip.h"
#include "cms_jablo_sms.h"
#include "udp_type.h"
#include "sms_lib.h"
#include "system.h"

#include <stdio.h>


#define	THIS_SOURCE_ID 23

#define	CMS_REINIT_WAIT      20 // [1s]
#define	CMS_BACKUP_SLEEP     10 // [1s]

// #define	CMS_PRINTF(...)
#define	CMS_PRINTF printf

#define	CMS_BUSY_WAIT  5 // 
#define	SMS_DELIVERY_TIMEOUT 18 // [10s]


#define	DONE_BREAK()						\
	if (  (result == CMS_RESULT_DONE)		\
	  ||  (result == CMS_RESULT_EMPTY))		\
		break;

// vysledek komunikace na PCO
typedef enum {
	CMS_RESULT_FAILED=0,    // nepovedeny pokus predani na PCO
	CMS_RESULT_DONE,        // komunikace OK
	CMS_RESULT_DONE_IMG,    // komunikace OK, ale vyjimka IMG neloguje doruceni
	CMS_RESULT_EMPTY,       // neni co predat, treba zadny CID u eventu
	CMS_RESULT_DELAYED,     // cekani na potvrzeni, napriklad drucenka SMS
	CMS_CHANNEL_BUSY,       // nelze komunikovat, komunikacni kanal je obsazen (napr. GSM data/hlas )
	CMS_RESULT_STOP	        // predavani cele sekvence ukoceno, napriklad chyba konfigurace
} cms_result_t;

// konfigurace systemu PCO
typedef struct {  // konfiguracni struktura PCO serveru
	u32 ip[CMS_IPS];
	u16 port[CMS_IPS];
	ascii phone[MAX_PHONENUM_LEN];
	u32 time_out_limit;
	u32 object_id;             // sice to neni standard, ale JA realne vyuziva >16bit
	u16 report_time[MAX_PCOS]; // perioda kontrolnich prenosu
	u16 report_time_w_key;     // perioda prenosu poloh pri jizde
	u8 sms_delivery_timeout;   // jednotka je 10s
#define	CMS_DEFAULT_RETRY 5
	u8 retry_cnt;
#define	CMS_DEFAULT_WAIT  30 // [s]
	u8 wait_tm;
#define	CMS_FLAG_ENABLE_SMS (1<<0)
#define	CMS_FLAG_ENABLE_IP  (1<<1)
#define	CMS_FLAG_V2_ENABLED (1<<2)
	u8 flags;
} cms_setup_t;

static cms_setup_t cms_setup;

// stavova strukturea (pro kazdy PCO)
typedef struct {
	u32 report_timer;     // casovac pravidelnych prenosu
	u32 time_out_timer;   // merim dobu jak dlouho mi trva predani na PCO [s] time_out_limit
	u16 sleep;            // cekat pred dalsi akci, rusi nova udalost
	u8 report_request:1;  // 
	u8 error_state:1;     // vycerpani pokusu o prenos
	u8 test:1;            // manualni vynuceni test prenosu 
	u8 force_sleep:1;     // natvrdo cekat pred dalsi akci
	u8 transfers_on:1;    // zapnuti funkce
	u8 tmout_active;      // 
	u8 fail_counter;
	u8 busy_cnt;          // timer cekani na uvolneni komunikacniho rozhrani
} cms_state_t;

static cms_state_t cms_state[MAX_PCOS];

// struktury sp[ecificke pro konkretni typ PCOn
static cms_jablo_ip_state_t jablo_ip_state; // podporuju pouze 1 PCO typu jablo-ip
static cms_jablo_sms_state_t jablo_sms_state; // podporuju pouze 1 PCO typu jablo-ip


typedef struct { // konfiguracni struktura udalosti
	u8 evt;        // kvuli prehlednosti a detekci konzistentni tabulky
	u8 priority:4; // 
	u8 cms_mask:4; // maska PCO na ktera budeme prenaset
	u16 cid;       // CID kod 
} cms_event_setup_t;

typedef struct { // prevod zdroje udalosti na CID kod
	u8  src;       // kvuli prehlednosti a detekci konzistentni tabulky
	u16 cid;       // CID kod 
} cms_source_table_t;

/// \subsection {CID kódy událostí} 
/// Názvy událostí viz \ref{EVENTS}.
/// \begin{itemize}
static const cms_event_setup_t CMS_EVENT_SETUP_TABLE[EVENT_LAST] = {
	{ EVENT_UNKNOWN,                             0,  0,  0 },/*{{{*/
	{ EVENT_BOOT_UP,                             1,  3,  0x1305 }, ///* ~w2, cid=~w5 
	{ EVENT_SHUT_DOWN,                           1,  0,  0x1308 },
	{ EVENT_LINE_ERROR,                          1,  0,  0},
	{ EVENT_LINE_OK,                             1,  0,  0},
	{ EVENT_POWER_FAIL,                          1,  3,  0x1301 }, ///* ~w2, cid=~w5 
	{ EVENT_POWER_RECOVERY,                      1,  3,  0x3301 }, ///* ~w2, cid=~w5 
	{ EVENT_LO_BATT,                             1,  3,  0x1384 }, ///* ~w2, cid=~w5 
	{ EVENT_LO_BATT_RECOVERY,                    1,  3,  0x3384 }, ///* ~w2, cid=~w5 
	{ EVENT_30M_NO_PWR,                          1,  0,  0x6301 }, 
	{ EVENT_INSTANT_ALARM,                       1,  3,  0x1130 }, ///* ~w2, cid=~w5 
	{ EVENT_INSTANT_ALARM_CANCEL,                1,  3,  0x1406 }, ///* ~w2, cid=~w5 
	{ EVENT_INSTANT_ALARM_TMOUT,                 1,  0,  0},
	{ EVENT_DELAYED_ALARM,                       1,  3,  0x1134 }, ///* ~w2, cid=~w5 
	{ EVENT_DELAYED_ALARM_CANCEL,                1,  3,  0x1406 }, ///* ~w2, cid=~w5 
	{ EVENT_DELAYED_ALARM_TMOUT,                 1,  0,  0},
	{ EVENT_SABOTAGE_ALARM,                      1,  3,  0x1144 }, ///* ~w2, cid=~w5 
	{ EVENT_SABOTAGE_ALARM_CANCEL,               1,  3,  0x1406 }, ///* ~w2, cid=~w5 
	{ EVENT_SABOTAGE_ALARM_TMOUT,                1,  0,  0},
	{ EVENT_FIRE_ALARM,                          1,  3,  0x1110 }, ///* ~w2, cid=~w5 
	{ EVENT_FIRE_ALARM_CANCEL,                   1,  3,  0x1406 }, ///* ~w2, cid=~w5 
	{ EVENT_FIRE_ALARM_TMOUT,                    1,  0,  0},
	{ EVENT_PANIC_ALARM,                         1,  3,  0x1120 }, ///* ~w2, cid=~w5 
	{ EVENT_PANIC_ALARM_CANCEL,                  1,  3,  0x1406 }, ///* ~w2, cid=~w5 
	{ EVENT_PANIC_ALARM_TMOUT,                   1,  0,  0},
	{ EVENT_FAULT,                               1,  3,  0x1300 }, ///* ~w2, cid=~w5 
	{ EVENT_FAULT_RECOVERY,                      1,  3,  0x3300 }, ///* ~w2, cid=~w5 
	{ EVENT_ARM,                                 1,  3,  0x3401 }, ///* ~w2, cid=~w5 
	{ EVENT_DISARM,                              1,  3,  0x1401 }, ///* ~w2, cid=~w5 
	{ EVENT_PART_ARM,                            1,  3,  0x3402 }, ///* ~w2, cid=~w5 
	{ EVENT_PART_DISARM,                         1,  3,  0x1402 }, ///* ~w2, cid=~w5 
	{ EVENT_JAMMING_ACT,                         1,  3,  0x1344 }, ///* ~w2, cid=~w5 
	{ EVENT_JAMMING_DACT,                        1,  3,  0x3344 }, ///* ~w2, cid=~w5 
	{ EVENT_NEW_CONFIGURATION,                   1,  3,  0x1416 }, ///* ~w2, cid=~w5 
	{ EVENT_CONTROL_REPORT,                      1,  0,  0},
	{ EVENT_ALIVE_REPORT1,                       1,  1,  0x1602 }, ///* ~w2, cid=~w5 
	{ EVENT_ALIVE_REPORT2,                       1,  2,  0x1602 },
	{ EVENT_GEO_OUT_PCO,                         1,  3,  0x1643 }, ///* ~w2, cid=~w5 
	{ EVENT_GEO_IN_PCO,                          1,  3,  0x3643 }, ///* ~w2, cid=~w5 
	{ EVENT_TRACKING,                            1,  1,  0x6601 }, ///* ~w2, cid=~w5 
	{ EVENT_RES40,                               1,  0,  0},
	{ EVENT_RES41,                               1,  0,  0},
	{ EVENT_RES42,                               1,  0,  0},
	{ EVENT_CREDIT_LOW,                          1,  0,  0},
	{ EVENT_24H_PERIPHERY_ACT,                   1,  3,  0x1133 }, ///* ~w2, cid=~w5
	{ EVENT_24H_PERIPHERY_DACT,                  1,  0,  0x3133 }, // nepouzito
	{ EVENT_LO_BATT_CU,                          1,  3,  0x1301 }, ///* ~w2, cid=~w5 
	{ EVENT_LO_BATT_CU_RECOVERY,                 1,  3,  0x3301 }, ///* ~w2, cid=~w5 
	{ EVENT_OVERCODE,                            1,  0,  0},
	{ EVENT_AUTH_OK,                             1,  3,  0x1422 }, ///* ~w2, cid=~w5 
	{ EVENT_NEW_TIME,                            1,  0,  0},
	{ EVENT_TEST_REPORT1,                        0,  1,  0},
	{ EVENT_TEST_REPORT2,                        0,  2,  0},
	{ EVENT_SESSION_OPEN,                        1,  0,  0},
	{ EVENT_SESSION_CLOSE,                       1,  0,  0},
	{ EVENT_BLOCKED,                             1,  3,  0x1323 }, ///* ~w2, cid=~w5 
	{ EVENT_UNBLOCKED,                           1,  3,  0x3323 }, ///* ~w2, cid=~w5 
	{ EVENT_GEO_OUT_SMS,                         1,  0,  0 },  ///* ~w2, cid=~w5
	{ EVENT_SINFO,                               1,  0,  0},
	{ EVENT_GEO_IN_SMS,                          1,  0,  0 },  ///* ~w2, cid=~w5
	{ EVENT_BACKUP_ACCU_ERROR,                   1,  0,  0},
	{ EVENT_BEEP,                                1,  0,  0},
	{ EVENT_PERIPHERY_LOST,                      1,  3,  0x1350 }, ///* ~w2, cid=~w5 
	{ EVENT_PERIPHERY_LOST_RECOVERY,             1,  3,  0x3350 }, ///* ~w2, cid=~w5 
	{ EVENT_HISTORY_DELETED,                     1,  0,  0},
	{ EVENT_LO_BACKUP_ACCU,                      1,  3,  0x1302 }, ///* ~w2, cid=~w5 
	{ EVENT_LO_BACKUP_ACCU_RECOVERY,             1,  3,  0x3302 }, ///* ~w2, cid=~w5 
	{ EVENT_DELAYED_ALARM_ACTIVATED,             1,  0,  0},
	{ EVENT_EMERGENCY_UNIMO,                     1,  3,  0x1423 }, ///* ~w2, cid=~w5 
	{ EVENT_TRACK_STOP,                          1,  3,  0x3655 }, ///* ~w2, cid=~w5 
	{ EVENT_TRACK_START,                         1,  3,  0x1655 }, ///* ~w2, cid=~w5
	{ EVENT_ERROR_DURING_ARM,                    0,  3,  0x1573 }, ///* ~w2, cid=~w5
	{ EVENT_LOGIN,                               1,  0,  0},
	{ EVENT_LOGOUT,                              1,  0,  0},
	{ EVENT_LEARN_MODE_START,                    1,  0,  0},
	{ EVENT_LEARN_MODE_STOP,                     1,  0,  0},
	{ EVENT_INSTANT_ALARM_ACTIVATED,             1,  0,  0},
	{ EVENT_INSTANT_ALARM_FORCE,                 1,  0,  0},
	{ EVENT_INSTANT_ALARM_DACT,                  1,  3,  0x3130 }, // zatim nepouzito
	{ EVENT_DELAYED_ALARM_DACT,                  1,  3,  0x3134 }, // zatim nepouzito
	{ EVENT_SABOTAGE_ALARM_DACT,                 1,  3,  0x3144 }, ///* ~w2, cid=~w5 
	{ EVENT_FIRE_ALARM_DACT,                     1,  3,  0x3110 }, // zatim nepouzito
	{ EVENT_PANIC_ALARM_DACT,                    1,  3,  0x3120 }  // zatim nepouzito
};/*}}}*/
/// \end{itemize}

/// \subsection {CID kódy zdrojů událostí} 
/// \begin{itemize}
#define	DEFAULT_CID_SOURCE	0x0701

static const cms_source_table_t CMS_SOURCE_TABLE[SOURCE_INP_COUNT] = {
	{ SOURCE_UNKNOWN, DEFAULT_CID_SOURCE },
	// 0x00xx bezdratove vstupy
	// 0x02xx dratove vstupy
	// 0x04xx ovladace
	{ SOURCE_SELF,       0x0701 },  ///* ~w2, cid=~w3 
	{ SOURCE_KEY,        0x0202 },  ///* ~w2, cid=~w3 
	{ SOURCE_DOOR,       0x0201 },  ///* ~w2, cid=~w3 
	{ SOURCE_INPUT1,     0x0204 },  ///* ~w2, cid=~w3 
	{ SOURCE_INPUT2,     0x0205 },  ///* ~w2, cid=~w3 
	{ SOURCE_INPUT3,     0x0206 },  ///* ~w2, cid=~w3 
	{ SOURCE_AUX1,       0x0220 },  ///* ~w2, cid=~w3 
	{ SOURCE_TRACER,     0x0221 },  ///* ~w2, cid=~w3 
	{ SOURCE_PANIC,      0x0222 },  ///* ~w2, cid=~w3 
	{ SOURCE_SHOCK,      0x0223 },  ///* ~w2, cid=~w3 
	{ SOURCE_VOLTAGE_DT, 0x0224 },  ///* ~w2, cid=~w3 
	{ SOURCE_LOCK,       0x0225 },  ///* ~w2, cid=~w3 
	{ SOURCE_COMM_GSM,   0x0731 },  ///* ~w2, cid=~w3 
	{ SOURCE_PCO,        0x0921 },  ///* ~w2, cid=~w3 
	{ SOURCE_TIMER,      0x0226 },  ///* ~w2, cid=~w3 
	{ SOURCE_CAN_KEY,    0x0210 },  ///* ~w2, cid=~w3 
	{ SOURCE_CAN_DOOR,   0x0207 },  ///* ~w2, cid=~w3 
	{ SOURCE_CAN_TRUNK,  0x0208 },  ///* ~w2, cid=~w3 
	{ SOURCE_CAN_BONNET, 0x0209 },  ///* ~w2, cid=~w3 
	{ SOURCE_CAN_LOCK,   0x0227 }   ///* ~w2, cid=~w3 

};
/// \end{itemize}

/* takto mel zdroje udalosi stary athos : {{{
	0x0701	,	// 0	Ustredna	
	0x0401	,	// 1	Bezdratovy ovladac 1
	0x0402	,	// 2	Bezdratovy ovladac 2	
	0x0403	,	// 3	Bezdratovy ovladac 3	
	0x0404	,	// 4	Bezdratovy ovladac 4	
	0x0201	,	// 5	Dratove cidlo 1	(DOOR)		
	0x0202	,	// 6	Dratove cidlo 2	(KEY)		
	0x0203	,	// 7	Dratove cidlo 3	(VCC)		
	0x0204	,	// 8	Dratove cidlo 4	(INP1)		
	0x0205	,	// 9  	Dratove cidlo 5	(dverni zamek)
	0x0206	,	//10 	Dratove cidlo 6	(INP2)
	0x0207	,	//11 	Dratove cidlo 7			
	0x0208	,	//12	Dratove cidlo 8			
	0x0209	,	//13	Dratove cidlo 9			
	0x0001	,	//14	Bezdratove cidlo 1		
	0x0002	,	//15	Bezdratove cidlo 2		
	0x0003	,	//16	Bezdratove cidlo 3		
	0x0004	,	//17	Bezdratove cidlo 4		
	0x0005	,	//18	Bezdratove cidlo 5		
	0x0006	,	//19	Bezdratove cidlo 6		
	0x0007	,	//20	Bezdratove cidlo 7		
	0x0008	,	//21	Bezdratove cidlo 8		
	0x0731	,	//22	Telefonni linka (gsmlink)	
	0x0732	,	//23***	Tel1
	0x0733	,	//24***	Tel2
	0x0734	,	//25***	Tel3
	0x0735	,	//26***	Tel4
	0x0736	,	//27***	Tel - usercode
	0x0737	,	//28***	Tel - mastercode
	0x0751	,	//29    SIM karta
}}} */

static bool active_tracking = false;

OS_SEMAPHORE(cms_semaphore);

static void cms_clr_timeout (u8 p, bool force);
static void cms_clr_error (u8 p);

u16 cms_cid_translate_source (u16 source)
{
	u16 id    = source & SOURCE_NUM_MASK;
	u16 group = source & SOURCE_GROUP_MASK;

	if (id>99)
		return (DEFAULT_CID_SOURCE);

	switch (group)
	{
	case SOURCE_INPUTS:
		if (id<SOURCE_INP_COUNT)
		{
			if (CMS_SOURCE_TABLE[id].src == id)
				return (CMS_SOURCE_TABLE[id].cid);
		}
		break; 
		// return (0x0200 + short_to_bcd(id));

	case SOURCE_USER:
	case SOURCE_USER_CALL:
	case SOURCE_USER_RFID:
		/// Uživatelé mají zdroj 0x0500 + id.
		return (0x0500 + short_to_bcd(id));

	case SOURCE_PERIPHERY:
		/// Bezdrátové periferie 0x0000 + id. Kde id 0..7 jsou klíčenky, 
		/// 8..19 interní detektory a 20..31 externí detektory.
		return (0x0000 + short_to_bcd(id));
	}

    return (DEFAULT_CID_SOURCE);
}

bool cms_init(void)
{	// jednorazovy init pri spusteni systemu
	OS_SEMAPHORE_INIT(cms_semaphore);
	cms_buf_init();
	// 	printf ("\r\nsizeof(cms_buffer_t)=%d ", sizeof(cms_buffer_t));
	return (cms_reinit());
}

/// \subsection{Nastavení komunikace} \label{CMS}
/// Související konfigurační položky jsou CFG22xx.
bool cms_reinit(void)
{	// volano pri zmene konfigurace

	// test PCO:
	// object-id: 0xAAA1-0xAAAA
	// IP: 195.39.77.155:10015
	// SMS: 777761089
	//
	// Impreza:
	// ID: 0x4D65 == 19813
	// IP: 194.169.224.101:8587
	// SMS: +420773446480

	// CFG2200=19813/3/5/30/10/600
	// CFG2201=194.169.224.101:8587
	// CFG2201=195.39.77.155:10015

	ascii buf[MAX_TEXT_LEN];
	int a,b,c,d,e,f,g,h;
	u8 p;

	memset (&cms_setup, 0, sizeof(cms_setup_t));

	if (! storip_get_text(buf, TEXT_CFG_CMS_MAIN))
		return (false);

	cms_setup.retry_cnt = CMS_DEFAULT_RETRY;
	cms_setup.wait_tm   = CMS_DEFAULT_WAIT;
	cms_setup.report_time_w_key = 5*60;

	if ((p = sscanf(buf, "%d/%d/%d/%d/%d/%d/%d/%d", &a, &b, &c, &d, &e, &f, &g, &h)) >= 6)
	{	// CFG2200=0/3/5/30/15/300/5
		/// \\Všechny čísla včetně Object-ID jsou uloženy dekadicky.
		/// První záznam "id" je object-ID.
		cms_setup.object_id = a;
		/// \\Význam jednotlivých BITů v "flags" 
		/// \\- BIT0 povolení přenosů na SMS PCO \\- BIT1 povolení přenosů na GPRS PCO
		/// \\- BIT2 zapnout V2 šifrování
		cms_setup.flags = b;
		/// \\Povolený interval pro počet opakování "retry" je 2 .. 30.
		if ((c>1) && (c<=30))
			cms_setup.retry_cnt = c;
		
		/// \\Povolený interval pro dobu čekání "wait" je 10 .. 255s.
		if ((d>=10) && (d<=255))
			cms_setup.wait_tm   = d;
		
		/// \\Povolený interval pro periodu pravidelných IP přenosů "period_ip" je 2 .. 1000m.
		if ((e>1) && (e<=1000))
			cms_setup.report_time[0] = e*60;

		/// \\Povolený interval pro periodu pravidelných SMS přenosů "period_sms" je 15 .. 1000m.
		if ((f>=15) && (f<1000))
			cms_setup.report_time[1] = f*60;
		
		/// \\Povolený interval pro periodu pravidelných přenosů při jízdě "period_w_key" je 0 .. 60m, 
		/// kde 0 znamená vypnuto.
		// Zcela nelogicky a nesmyslně je výchozí hodnota jedna minuta, 
		// takže při jízdě každou minutu komunikuje na PCO, tedy je to duplicitní přenos poloh ke knize jízd !
		if ((p>6) // nepovinny parametr, pokud neuveden, tak je to vypnute
		 && (g<=60)) // max 60 minut
		{	//
			cms_setup.report_time_w_key = g*60;
		}

		/// \\Povolený interval pro dobu vyhlaseni poruchy signalizovane sirenou "tmout" je
		if ((p>7) // nepovinny parametr, pokud neuveden, tak je to vypnute
		 && ((h>= 10 )&&(h<=( 7*24*60 ) ))) /// ~w3 až ~w5 minut, 0==vypnuto.
		{
			cms_setup.time_out_limit = h*60; // [s]
		}
	}

	// 	printf ("\r\nCMS SETUP: f=%04x ", cms_setup.flags);
	if (cms_setup.object_id == 0)
		cms_setup.flags = 0; /// \\Dokud je object-id nulové, tak je vypnutá komunikace.
	
	cms_state[0].transfers_on = cms_setup.flags & CMS_FLAG_ENABLE_IP  ? 1 : 0;
	cms_state[1].transfers_on = cms_setup.flags & CMS_FLAG_ENABLE_SMS ? 1 : 0;
	
	for (p=0; p<MAX_PCOS; p++)
	{
		cms_state[p].sleep = CMS_REINIT_WAIT;
		cms_state[p].test  = 0;
		cms_clr_error(p);
		cms_clr_timeout(p, true);

	}
	cms_setup.ip[0]=0;
	if (storip_get_text(buf, TEXT_CFG_CMS_URL1))
	{
		net_get_target_ip (&cms_setup.ip[0], &cms_setup.port[0], buf);
	}
	cms_setup.ip[1]=0;
	if (storip_get_text(buf, TEXT_CFG_CMS_URL2))
	{
		net_get_target_ip (&cms_setup.ip[1], &cms_setup.port[1], buf);
	}

	if (((cms_setup.ip[0]==0) || (cms_setup.port[0]==0))
	 && ((cms_setup.ip[1]==0) || (cms_setup.port[1]==0)))
		cms_state[0].transfers_on = 0; // zadna IP, nemuze to byt zapnute

	if (storip_get_phone(buf, TEXT_CFG_CMS_PHONE))
	{
		strncpy(cms_setup.phone, buf, MAX_PHONENUM_LEN);
	}
	else
	{	// neplatne tel cislo
		cms_state[1].transfers_on = 0; 
	}

	cms_setup.sms_delivery_timeout = SMS_DELIVERY_TIMEOUT;
	active_tracking = false;

#ifdef JABLO_IP_V2_SUPPORTED
	cms_jablo_clr_v2_key();
	if (storip_get_text(buf, TEXT_CFG_CMS_KEY))
	{
		if (cms_setup.flags & CMS_FLAG_V2_ENABLED)
		{
			cms_jablo_set_v2_key(buf);
		}
	}
#endif // JABLO_IP_V2_SUPPORTED
	// OS_PRINTF("\r\nCMS OBJECT ID=%d", cms_setup.object_id); // DEBUG
	return (true);
}

void event_semaphore_take(void)
{
	OS_SEMAPHORE_TAKE(cms_semaphore);
}

void event_semaphore_give(void)
{
	OS_SEMAPHORE_GIVE(cms_semaphore);
}

bool cms_new_event(event_t *event)
{
	u8 mask;
	OS_ASSERT(CMS_EVENT_SETUP_TABLE[event->evt].evt == event->evt, "inconsistent CMS_EVENT_SETUP_TABLE");

	event_semaphore_take();

	mask = CMS_EVENT_SETUP_TABLE[event->evt].cms_mask;

	if (app_main_data_disabled())
		mask &= ~1; // pokud nejsou povoleny data, tak to na prvni PCO ani nezkousim predat

	if (cms_buf_add_event(event, CMS_EVENT_SETUP_TABLE[event->evt].priority, mask))
	{
		CMS_PRINTF("\r\nCMS NEW: e=%d,s=%d ", event->evt, event->src);
	}
	else
	{
		CMS_PRINTF("\r\nCMS NEW SKIP: e=%d,s=%d ", event->evt, event->src);
	}

	event_semaphore_give();
	return (true);
}

static bool cms_empty(u8 p)
{
	return (cms_buf_empty(p));
}

static bool cms_is_active (u8 p)
{
	if (p>=MAX_PCOS)
		return (false);

	if (cms_state[p].transfers_on)
		return (true);

	if (cms_state[p].test)  // testem lze vynutit i komunikaci na vypnute PCO
		return (true);

	return (false);
}

__inline bool cms_has_backup (u8 p)
{	// jednoducha struktura, prvni PCO je hlavni, dalsi je zalohou
	return (p == 0 ? true : false);
}

__inline bool cms_is_backup(u8 p)
{	// jednoducha struktura, prvni PCO je hlavni, dalsi je zalohou
	return (p>0 ? true : false);
}

static __inline void cms_event (u8 p, u16 event)
{
	if (! cms_is_active(p))
		return;

	event_add_new (event, SOURCE_PCO, NULL, 0);
}

static void cms_clr_timeout (u8 p, bool force)
{
	if (p>=MAX_PCOS)
		return;

	if (cms_is_backup(p))      //
	{
		cms_clr_timeout (p-1, force); // staci kdyz stihne predat na zalohu, tak uklidnime poruchu
	}

	if ((cms_state[p].time_out_timer == 0)
			&& (force==false))
		return; // tak se to sice dorucilo, ale ne vcas, zustava chyba

	cms_state[p].time_out_timer = 0;

	if (cms_state[p].tmout_active == 0) 
		return; // timeout neni aktivni

	cms_state[p].tmout_active = 0;

	m_decho_n ("CMS timeout recovery,p", p+1, LOG_SELECT_CMS);
	// 	cms_event (p, EVENT_CMS_ERROR_RECOVERY);
	
	if (! cms_error())
	{	// uz zadne PCO neni v poruse
		system_int_state &= ~SYSTEM_INT_CMS_ERROR;
	}
	
}

static void cms_clr_error (u8 p)
{
	if (p>=MAX_PCOS)
		return;

	if (! cms_state[p].error_state)
		return;

	cms_state[p].error_state = 0;

	cms_jablo_ip_init (&jablo_ip_state);
	cms_jablo_sms_init (&jablo_sms_state);

	m_decho_n ("cms_clr_error(),p", p+1, LOG_SELECT_CMS);
	// 	cms_event (p , EVENT_CMS_FAULT_RECOVERY);

	// je potreba uklidnit i zalozni PCOcka, protoze jinak by to zustalo nafurt
	if (cms_has_backup (p))
	{
		cms_clr_error (p+1);   // pozor, rekurze
		cms_clr_timeout (p+1, true); // u zalohy zrusime i pripadny timeout, jinak by to signalizovalo poruchu
	}

}

static void cms_renew_events (u8 p)
{
	cms_buf_restore_events (p, CMS_BUF_UNLIMITED);
}

static void cms_discard_events(u8 p)
{
	event_t event;
	u8 i;

	// 	printf("[CMS DISCARD]");
	for (i=0; i<10; i++)
	{
		if (! cms_buf_get_event (&event, NULL, p))
			break;
	}
	cms_buf_done_error (p);
}

u16 cms_cid_translate_event (u16 event)
{
	if (event >= EVENT_LAST)
		return (0);
	return (CMS_EVENT_SETUP_TABLE[event].cid);
}

bool cms_get_next_event (event_t *event, u8 p)
{/*{{{*/
	bool result;
	u8   priority;

get_ev_repeat:
	result = cms_buf_get_event(event, &priority, p);

	if (result)
	{
		CMS_PRINTF ("[EID:%d", event->id);
		// printf ("[PCO%d: id:%d,E:%d,S:%d,N:%d]\r\n", p+1, event->s.id, event->s.type, event->s.src, event->s.sect);

		if (cms_cid_translate_event(event->evt)==0)
		{
			OS_PUTTEXT("(NO CID)]");
			cms_buf_delete_event (event->id, ALL_CMS);
			goto get_ev_repeat; //
		}
		OS_PUTTEXT("]");

		if (priority > 0)
		{	// pripadne zkratime timeout na nastaveny cas
			if (cms_state[p].time_out_timer>cms_setup.time_out_limit)
			{
				cms_state[p].time_out_timer = cms_setup.time_out_limit;
			}
		}
		// u SMS pultu hlidame dorucenky
 		if (p == 1)
 			cms_jablo_sms_new_sr (&jablo_sms_state, event->id,  false);
	}

	return (result);
}/*}}}*/

bool cms_error (void)
{	// vraci true pokud je nektere PCO v poruse
	int p;
	for (p=0; p<MAX_PCOS; p++)
	{
		if (cms_state[p].tmout_active && cms_state[p].transfers_on)
			return (true);
	}
	return (false);

}

static void cms_set_timeout (u8 p)
{	/*{{{*/
	if (p>=MAX_PCOS)
		return;

	if (cms_state[p].tmout_active) 
		return; // timeout uz je aktivni

	cms_state[p].tmout_active = 1;
	system_int_state |= SYSTEM_INT_CMS_ERROR;
	m_decho_n ("CMS timeout,p", p+1, LOG_SELECT_CMS);
	//  	cms_event (p, EVENT_CMS_ERROR);
}/*}}}*/

static void cms_reset_report_timer (u8 p)
{
	u32 a;

	a = cms_setup.report_time[p];

	if (a>=60) // prilis kratke casy neberu
	{	
		if (active_tracking)
		{	// probiha jizda, muze byt ale zapnute pravidelne posilani poloh 
			if ((p == 0) && (cms_setup.report_time_w_key>=60))
			{	
				a = cms_setup.report_time_w_key;
			}
			else
			{
				a = 0;
			}
		}
		if (alarm_active() && (p==0))
		{	// #151 v pripade poplachu a po poplachu posilame kazdou minutu
			a = 60;	
		}

		cms_state[p].report_timer = a;
	}
	// printf ("\r\nCMS(%d) rt=%d ", p, a); // DEBUG

	if (cms_state[p].error_state == 0)
	{	// PCO neni v chybe
		while (cms_has_backup(p))
		{	// existuje zalozni pco
			// tak musi cekat dokud nevypraskame pokusy na primarni
			if (cms_state[p+1].sleep < (cms_state[p].sleep + CMS_BACKUP_SLEEP)) // pokud to uz neni nastavene na delsi cas
				cms_state[p+1].sleep = cms_state[p].sleep + CMS_BACKUP_SLEEP; // je zbytecne kontrolovat prilis casto
			p++;
			cms_state[p].report_timer=0; // zastavit pravidelne prenosy na vsechny zalohy
		}
	}
}

void cms_tracking_start (void)
{	/// Při zahájení jízdy dojde k odhlášení od PCO a k zastavení pravidelných přenosů.
	/// Případně se zapne pravidelné předávání poloh na PCO, pokud je nastavená perioda "period_w_key".
	active_tracking = true;
 	cms_event (0, EVENT_TRACK_START); // hlasime zahajeni jizdy
}

void cms_tracking_stop(void)
{	/// Při ukončení jízdy se opět spustí pravidelné přemosy.
	active_tracking = false;
 	cms_event (0, EVENT_TRACK_STOP); // hlasime konec jizdy
}

void cms_sms_delivery (u8 sms_id, bool result)
{	// dorucenka SMS, bude to z nejakeho SMS pultu
	cms_jablo_sms_delivery (&jablo_sms_state, sms_id, result);
}

void cms_reports_process (u8 p)
{
	if (cms_state[p].report_request == 0)
		return;

	cms_state[p].report_request = 0;
	
	if (active_tracking)
	{	// probiha jizda, sledovani polohy (volitelna funkce)
		cms_event (p, EVENT_TRACKING);
	}
	else
	{
		cms_event (p, EVENT_ALIVE_REPORT1+p);
	}
}

__inline void cms_done (u8 p)
{
	cms_buf_done_events (p, true);
}

u8 cms_main_delivered(u8 p, u16 eid, u8 rest_mask)
{
	u8 clr_mask = (1<<p);

	if (cms_has_backup(p))
		clr_mask |= (1<<(p+1));
	if (cms_is_backup(p))
		clr_mask |= (1<<(p-1));

	return (clr_mask);
}

void cms_main_not_delivered (u8 p, u16 eid)
{
}

void cms_main_discard (u8 mask, u16 eid)
{
}


static cms_result_t jablo_ip_send (u8 p)
{	/*{{{*/
	udp_packet_t packet;
	cms_result_t result = CMS_RESULT_FAILED;
	u8   uri;
	u8   retry;
	bool sync_needed=true;

// 	if (cms_check_busy_channel(p, true))
// 		return (CMS_CHANNEL_BUSY);

	if (net_connect() == false)
	{
		m_error ("network interface connection failed ");
		// musim si nastavit, ze je rozhozena synchronizace, protoze pokud zalozni PCO je fyzicky totez
		// pak by to melo problemy s obnovou spojeni (pretahovani o synchronizaci citacu)
		cms_jablo_ip_sync_fail (&jablo_ip_state);
		// muze to byt docasna chyba
		return (CMS_RESULT_FAILED);
	}

	if ((packet.data = (u8 *)OS_MEM_ALLOC (JABLO_IP_MAX_PACKET_LEN)) == NULL)
		return (CMS_RESULT_FAILED);

	OS_PUTTEXT("SENDING ... ");

	for (uri=0; uri<JABLO_IP_NUM_URIS; uri++)
	{
		u8 enable_force_retry=true;
		
		packet.dst_ip.addr = cms_setup.ip[uri];
		packet.dst_port    = cms_setup.port[uri];

		if ((packet.dst_ip.addr == 0) || (packet.dst_port == 0))
			continue;

/*		if (net_get_target_ip ((u32 *)&(packet.dst_ip.addr), &(packet.dst_port), _url[uri]) == false)
		{
			continue;
		}*/
		sync_needed=true; // aby se vzdy poprve sestavil paket

		if (cms_state[p].error_state)
			retry = 2;
		else
			retry = 5; // JABLO_IP_CFG->packet_retry_num; 

		// maximalni doba zaseknuti tasku bude JABLO_IP_NUM_URIS * retry * 10[s], aktualne 100s
		while (retry--)
		{	// vic pokusu na predani zpravy v ramci jedne session
			// m_decho_n("pco_send() retry", retry, LOG_SELECT_PCO);
			packet.src_port=packet.dst_port;

			if ((sync_needed)
			 || (jablo_ip_state.sync_ok[uri] == false)) // dokud nejsou citace synchronizovany, potreba furt inkrementovat
			{	// zadost o synchronizaci nebo prvni zavolani (inkrementovat citace)
				// Pepa Balatka tvrdi, ze na opakovany paket mi Jaga odpovi
				// takze pro opakovani neni potreba znovu sestavovat paket, ale neplati pro stary comtransporter (nebo jak se to jmenuje)
 				cms_renew_events (p);
				packet.datalen = cms_jablo_ip_build_msg (packet.data, cms_setup.object_id, p, jablo_ip_state.cnt);
				// m_dump ("[CNT:", (u8 *)(JABLO_IP_STATE->c[s].cnt), 3); OS_PUTTEXT("]");
				// m_dump("\r\nIP_DATA:", packet.data, packet.datalen);
			}
			if (packet.datalen>0)
			{	// zprava sestavena, je co odesilat
				net_udp_send (&packet);
				if (cms_jablo_ip_reply_wait(&jablo_ip_state, uri, &sync_needed))
				{
					result = CMS_RESULT_DONE;
// 					net_reset_err_cnt(); // odpovida PCO, takze rozhrani funguje spravne
					eth.error_counter=0;
					break;
				}
				OS_PUTTEXT ("[RETRY]");
				if (enable_force_retry)
				{
					if (sync_needed)
					{	// jeste minimalne jeden pokus po synchronizaci
						enable_force_retry=false;
						retry++; // prislo "sync" pridame pokus
					}
				}
			}
			else
			{	// neni co poslat
				// tohle se stane treba kdyz neni zadny CID u eventu
				result = CMS_RESULT_EMPTY;
				break;
			}
// 			GSM_CHECK_COLLISION();
		}
		DONE_BREAK();

		jablo_ip_state.sync_ok[uri] = false;
	}
	OS_MEM_FREE (packet.data);
	return (result);
}	/*}}}*/

static cms_result_t jablo_sms_send (u8 p)
{/*{{{*/
	cms_result_t result = CMS_RESULT_FAILED;

	if (! sms_device_ready ())
	{
		m_error ("SMS device not ready");
		return (CMS_RESULT_FAILED);
	}
	else
	{
		sms_struct_t sms;
		if ((sms.data = (u8 *)OS_MEM_ALLOC(140)) == NULL)
		{
			m_error ("MALLOC FAILED cms_send()");
			return (CMS_RESULT_FAILED);
		}

		cms_renew_events (p);
		OS_DELAY(1000); // pockam chvili, kdyby se sypaly dalsi udalosti, tak to bude v jedne SMS

		cms_jablo_sms_sr_clean (&jablo_sms_state);
		sms.len = cms_jablo_sms_build_msg (sms.data, cms_setup.object_id, p);

		if (sms.len>0)
		{
			sms.type     = SMS_TYPE_DATA8;
			sms.sr       = true;
			sms.tel_num  = cms_setup.phone;
			if (sms_send_now(&sms, 0))
			{	// podarilo se odeslat SMS, aktivuju cekani na dorucenku
				cms_jablo_sms_sr_activate (&jablo_sms_state, sms.id,  cms_setup.sms_delivery_timeout); 
				result = CMS_RESULT_DELAYED;
				while (cms_has_backup(p++))
				{	// zalozni PCO musi cekat dokud nevyprsi dorucenka
					// je sice picovina davat zalohu po SMS, ale teoreticky je to mozne
					// pozor na jednotky, sleep je [100ms], ale time_out je [10s]
					cms_state[p].sleep = (cms_setup.sms_delivery_timeout)*100 + CMS_BACKUP_SLEEP;
				}
			}
		}
		else
		{
			result = CMS_RESULT_EMPTY; // tohle se stane treba kdyz neni zadny CID u eventu
		}
		OS_MEM_FREE (sms.data);
	}
	return (result);
}/*}}}*/

static cms_result_t cms_send (u8 p)
{
	CMS_PRINTF("\r\nCMS: processing %d:", p);
	switch (p)
	{	// typ PCO je pevne dany 
	case 0:
		return (jablo_ip_send(p));

	case 1:
		return (jablo_sms_send(p));
	}
	return (CMS_RESULT_FAILED);
}

void cms_print_state (buf_t *out)
{
	/// \subsection {Diagnostika} \label{ARCDINFO}
	if (cms_is_active(0) == false)
		return;  // vypnuto, nic nepisu
	/// Pokud je ARC zapnuto v konfiguraci, pak přidá do DINFO text  
	buf_append_str (out, NEXT_DELIMITER);
	buf_append_str (out, "ARC:"); /// ~q 

	if (cms_state[0].error_state)
	{
		buf_append_str (out, "ERROR"); /// ~q 
		return;
	}

	buf_append_str (out, "OK"); /// nebo ~q
}

bool cms_udp_rx (u8 *data, u16 len)
{
	if (cms_jablo_ip_rx (data, len))
	{
		// m_dump ("JPRX:", data, len);
		return (true);
	}
	return (false);
}

static void cms_fail(u8 p)
{
#define	CMS_FAIL_LIMIT (20)
	if (cms_state[p].error_state)
		return; // chyba uz je aktivni

	if (++(cms_state[p].fail_counter) >= CMS_FAIL_LIMIT)
	{
		cms_state[p].error_state = 1;
		m_decho_n ("cms_set_error(),p", p+1, LOG_SELECT_CMS);
	}
}

bool cms_main_process(void)
{	// komunikacni task
	// vraci "true" pokud je klid
	u8 p;
	bool reset_report_timer=false;


	for (p=0; p<MAX_PCOS; p++)
	{
		if (cms_state[p].sleep)
			continue;

		cms_state[p].force_sleep = false;

		if (cms_empty(p))
		{
			cms_reports_process (p);
			// cms_state[p].event_priority=0;
		}
		else
		{
			bool enable_timeout_reset=true;

			if (cms_is_active(p) == false)
			{
				cms_discard_events(p);
				continue;
			}

			cms_state[p].test=0; // test je vzdy jednorazovy
			reset_report_timer = false;
			if (cms_state[p].time_out_timer)
				enable_timeout_reset=false; // timeout uz bezi z minula, nesmim ovlivnit

			switch (cms_send (p))
			{/*{{{*/
				case CMS_RESULT_FAILED:
					reset_report_timer = true;
					if (cms_state[p].error_state)
						cms_buf_done_error (p); // pokud je PCO v poruse, neopakujem
					else
						cms_buf_retry (p, cms_setup.retry_cnt + 1);

					OS_PUTTEXT (TEXT_FAILED);
					cms_fail(p);
					cms_state[p].sleep = cms_setup.wait_tm;

					if (cms_setup.time_out_limit)
					{	// je zapnuty timeout
						// pokud zbyva malo casu, urychlime dalsi pokus
						s16 rest_of_time;

						rest_of_time = cms_state[p].time_out_timer;
						if (rest_of_time<0)
							rest_of_time=0;
						m_decho_n ("CMS time left t",rest_of_time, LOG_SELECT_CMS);
					}
					break;

				case CMS_RESULT_DELAYED:
					reset_report_timer = true;
					cms_buf_waiting (p);
					OS_PUTTEXT ("DELAYED\r\n");
					break;

				case CMS_RESULT_DONE:
					cms_done(p);
					reset_report_timer = true;
					cms_state[p].fail_counter=0;
					cms_clr_error (p);
					cms_clr_timeout (p, false);
					cms_state[p].time_out_timer=0;
					OS_PUTTEXT (TEXT_DONE);
					break;

				case CMS_RESULT_EMPTY: // nebylo co odeslat, asi chybi u dane udalosti CID nebo SIA kod
					// rozhodne neresetovat casovac pravidelnych prenosu
					cms_state[p].fail_counter=0;
					cms_buf_done_error (p);
					if (enable_timeout_reset)    // v tomto pokusu se aktivoval tiemeout, ale nebylo co odeslat
						cms_state[p].time_out_timer=0; // takze rusime
					OS_PUTTEXT ("[EMPTY]");
					OS_PUTTEXT (TEXT_DONE);
					break;

				case CMS_CHANNEL_BUSY:
					// chvili pockame, nezli natvrdo vynutime komunkaci na PCO
					cms_renew_events (p);
					cms_state[p].sleep = CMS_BUSY_WAIT; //
					OS_PUTTEXT ("CHANNEL BUSY\r\n");
					break;

				case CMS_RESULT_STOP: // trvala chyba, asi chyba konfigurace
					reset_report_timer = true;

				default:
					cms_fail(p);
					cms_buf_done_error (p); // trvala chyba, zadne opakovani
					OS_PUTTEXT (TEXT_FAILED);
					break;

			}/*}}}*/
			if (reset_report_timer)
			{
				cms_reset_report_timer (p);
			}
			return (false); // jednim pruchodem jen jedna komunikace
		}
	}
	return (true);
}

void cms_busy_task (void)
{	// volano, kdyz PCO na neco ceka 
	// muzeme se dat neco co je brzdeno v tomto tasku
}

void cms_main_tick(void)
{	// perioda 1s
	u8 p;

	for (p=0; p<MAX_PCOS; p++)
	{
		if (cms_state[p].sleep)
			cms_state[p].sleep--;

		if (cms_state[p].time_out_timer)
		{
			if (--cms_state[p].time_out_timer == 0)
			{	// docasovalo "generovat poruchu pri nepredani do"
				cms_set_timeout (p);
			}
		}

		if (cms_state[p].report_timer)
		{	// 
			cms_state[p].report_timer--;
			if (cms_state[p].report_timer==0)
			{
				cms_state[p].report_request = 1;
			}
		}
	}
	// kontrola timeoutu dorucenek
	cms_jablo_sms_tick (&jablo_sms_state);
}
