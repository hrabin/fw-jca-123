#include "os.h"
#include "app.h"
#include "buf.h"
#include "gps_io.h"
#include "util.h"
#include "gps.h"
#include "rtc.h"
#include "log.h"

LOG_DEF("GPS");

// extern void tracer_new_valid_stamp (void);

#define	THIS_SOURCE_ID 9

// maximalni pocet znaku mezi "$" a <CR><LF> je podle specifikace 79, ale Quectel ma i vic (zatim zaznamenano nejvic 81)
// takze dame nejakou rezervu
#define	GPS_BUF_SIZE	128
buf_def(gps_buf, GPS_BUF_SIZE);  // na jednu celou zpravu
#define	PARM_SZ_LIMIT 	32
static ascii param_buf[PARM_SZ_LIMIT]; // pri parsovani prichozich dat, aby se to nemuselo furt alokovat

static const ascii NMEA_PREFIX[] = "$G";
#define	NMEA_PREFIX_LEN 2

static const char GPS_ID    = 'P'; // GP: Global Positioning System receiver
static const char MIXED_ID  = 'N'; // GN: Mixed GPS and GLONASS data, according to IEIC 61162-1
static const char GLONASS_ID= 'L'; // GL: GLONASS, according to IEIC 61162-1

static const ascii STR_GGA[] = "GGA";
static const ascii STR_GSA[] = "GSA";
static const ascii STR_GSV[] = "GSV";
static const ascii STR_RMC[] = "RMC";
static const ascii STR_VTG[] = "VTG";
static const ascii STR_GLL[] = "GLL";

#define	GPS_NMEA_REQUIRED (GPS_NMEA_RMC | GPS_NMEA_GGA | GPS_NMEA_GSV) // | GPS_NMEA_VTG)

static gps_stamp_t	gps_stamp_tmp; // pracovni zaznam, nemusi byt komplet
gps_stamp_t	gps_stamp_last;        // posledni uplny zaznam, muze byt neplatna poloha
gps_stamp_t	gps_stamp_last_valid;  // posledni platny zaznam

static os_timer_t fix_tm = 0;

#define	SLEEP_WAKEUP        (3600*10) // 1x za hodinu probudit [100ms]
#define	SLEEP_TIMEOUT       (3*60*10) // max 3 minuty zkousim sehnat polohu ve sleepu [100ms]
#define	SLEEP_DELAY           (15*10) // zpozdeni skutecneho vypnuti (cas na zpresneni polohy)
#define	GPS_MAX_WAKEUP_TIME (3600*10) // maximalni doba 'docasneho' probuzeni
static u16  sleep_tmr   = 0;
static u16  error_cnt   = 0;    // pocitam pocet chybnych poloh
static u16  sleep_postpone = 0; // keep wake up timer
static bool sleep_mode = false;
static bool gps_pwr_on_state = false;
static bool gps_rx_ok = false; // detekce, ze GPS posila nejaka platna data
static u8   gps_unsolicited = 0;
static u8   gps_nbsat_visible = 0; 
static u8   glonass_nbsat_visible = 0; 
static bool glonass_used = false;
static bool gps_init_rq = true;
static u8   gps_jamming = 0;


static u16 pmtk_ack = 0;

os_timer_t gps_reset_tm = 0;	    //casovac od gps_reset [sec]

static int _get_param_pos (ascii *src, int n)
{ 
	int l=0;
	int i;

	for (i=0; i<GPS_BUF_SIZE; i++)
	{
		if (n==0)
		{
			break;
		}
		l++;
		if (*src==',' )
		{
			n--;
		}
		if (n<=0) 
			break;

		if (*(src++)=='\0')
			return (-1);
	}
	return (l);
}

static int _get_nparam (ascii *dest, ascii *src, s16 n, u16 limit)
{ 
	s16 l=-1;
	u16 i;
	bool copy_ena=false;

	for (i=0; i<GPS_BUF_SIZE; i++)
	{
		if (!copy_ena)
		{
			if (n==0)
			{
				copy_ena=true;
				l=0;
			}
		}

		if (*src==',' )
		{
			n--;
		}
		else if (copy_ena)
		{
			if ((limit--)>=1)
			{
				*(dest++)=*(src);
				l++;
			}
			else
				break;
		}

		if (n<0) 
			break;

		if (*(src++)=='\0')
			break;
	}
	*dest='\0';
	return (l);
}

static char _hex_to_a(char ch)
{
	ch &= 0xF;
	if (ch >= 10)
		return (ch - 10 + 'A');
	return ch + '0';
}

void gps_send_text(const ascii *text)
{
	u8 chsum = '$'; 

	LOG_DEBUGL(3, "TX: \"%s\"", text);

	gps_tx_char('\r');
	gps_tx_char('\n');
	while (*text != '\0')
	{
		chsum ^= *text;
		gps_tx_char(*text);
		text++;
	}
	gps_tx_char('*');
	gps_tx_char(_hex_to_a((chsum>>4) & 0xF));
	gps_tx_char(_hex_to_a(chsum & 0xF));
	gps_tx_char('\r');
	gps_tx_char('\n');
}

static bool pmtk_cmd(const ascii *cmd)
{
	int wait = 10;

	pmtk_ack = 0;
	gps_send_text(cmd);
	while (wait--)
	{
		if (pmtk_ack)
			return (true);

		OS_DELAY(10);
	}
	LOG_ERROR("cmd failed");
	return (false);
}

static bool gps_set_baudrate(void)
{
	int retry = 2;
	int i;

	while (retry--)
	{
		gps_io_port_init(115200);
		OS_DELAY(10);
		gps_rx_ok = false;
		for (i=0; i<20; i++)
		{
			OS_DELAY(100);
			if (gps_rx_ok)
			{
				LOG_DEBUGL(2, "baudrate 115200");
				return (true);
			}
		}
		gps_io_port_init(9600);
		OS_DELAY(10);
		gps_rx_ok = false;

		for (i=0; i<20; i++)
		{
			OS_DELAY(100);
			if (gps_rx_ok)
			{
				LOG_WARNING("baudrate 9600");
				break;
			}
		}
		if (! gps_rx_ok)
		{
			LOG_ERROR("communication failed");
			return (false);
		}
		// Quectel proprietary command
		gps_send_text("$PQBAUD,W,115200");
		//response: "$PQBAUD,W,OK*40"
		// There is no response returned if the baud rate is changed to a different value.
		// ale vypada to, ze neodpovi nikdy
		OS_DELAY(100);
		// continue to retest new baudrate
	}
	// if not successfull continue with 9600
	return (true);
}

bool gps_init (void)
{
	memset ((u8 *)&gps_stamp_tmp,        0, sizeof(gps_stamp_t));
	memset ((u8 *)&gps_stamp_last,       0, sizeof(gps_stamp_t));
	memset ((u8 *)&gps_stamp_last_valid, 0, sizeof(gps_stamp_t));
	gps_io_init();

	gps_pwr_on_state = false;
	glonass_used = false;
	sleep_mode = true;
	gps_io_reset(OFF);
	gps_sleep_enable(true); // vychozi stav je sleep
	gps_temporary_start_tmout(10*60); // try to get coordinates just afret start

	gps_init_rq = true;

	gps_unsolicited = GPS_NMEA_PMTK;
	return (true);
}

void gps_reset (void)
{
 	gps_reset_tm = os_timer_get();
	gps_io_reset(ON);
	OS_DELAY(100);
	gps_io_reset(OFF);
	error_cnt=0;
}

void gps_suspend (void)
{
	LOG_DEBUGL(4, "suspend");
	gps_io_pwr (OFF);
	gps_pwr_on_state = false;
	gps_nbsat_visible    = 0;
	glonass_nbsat_visible = 0;
	gps_stamp_last.nbsat = 0;
	gps_stamp_last.fix = 0;
}

void gps_wakeup (void)
{
	LOG_DEBUGL(4, "wakeup");
	if (gps_pwr_on_state == false)
	{
		gps_io_pwr (ON);
		gps_pwr_on_state = true;
	}
}

bool gps_on (void)
{
	return (gps_pwr_on_state);
}

bool gps_glonass_used (void) 
{
	return (glonass_used);
}

void gps_sleep_enable (bool state)
{	// je mozne usnout
	// GPS vypnem az po chvili
	if (state)
	{	// povolit usporny rezim
		LOG_DEBUGL(1, "sleep ON");
		sleep_tmr = SLEEP_TIMEOUT - SLEEP_DELAY;
	}
	else
	{
		LOG_DEBUGL(1, "sleep OFF");
		sleep_tmr = 0;
		sleep_postpone = 0;
		gps_wakeup ();
	}
	sleep_mode = state;
}

bool gps_get_stored_stamp (gps_stamp_t *dest)
{
	memcpy ((u8 *)dest, (u8 *)&gps_stamp_last_valid, sizeof(gps_stamp_t));
	if (dest->fix == 0)
		return (false);
	return (true);
}

bool gps_get_current_stamp (gps_stamp_t *dest)
{
	memcpy ((u8 *)dest, (u8 *)&gps_stamp_last, sizeof(gps_stamp_t));
	if (dest->fix == 0)
		return (false);

	return (true);
}

u32 gps_valid_stamp_age (void)
{
	return (os_timer_get() - fix_tm);
}

void gps_set_unso (u8 mask)
{
	gps_unsolicited = mask;
}

static u16 knots_to_10kmh (ascii *data)
{	//  1 knot (kt) == 1.85200 km/h (kph)
	int a,b;
	s32 tmp = 0;

	switch (sscanf(data, "%d.%1d", &a, &b))
	{
	case 2: 
		tmp=b;
	case 1: 
		tmp += 10 * a; // kt*10
		tmp *= 1852;   // kmh*10000
		tmp /= 1000;   // kmh*10
		return (tmp);
	}
	return (0);
}

u32 text_get_u32_param (ascii *data, u8 tab)
{
	int pos;
	if ((pos = _get_param_pos(data, tab)) > 0)
	{
		return (atol(data+pos));
	}
	return (0);
}

static __inline void gps_data_parser (buf_t *inbuf)
{
	static u8 valid_data=0;
	ascii *data;
	ascii *buf;
	bool echo_it=false;
	ascii protocol_id;

	data = buf_data(inbuf);
	
	buf = param_buf;
	
	// detakce prefixu, musi to zacinat "$G"
	if (stricmp2(data, NMEA_PREFIX) != 0)
	{	// vubec to neni NMEA prefix
		if (gps_unsolicited & GPS_NMEA_PMTK)
			echo_it=true;
		if (stricmp2(data, "$PMTK") == 0)
		{	// mediatek proprietary message (quectel chipset)
			data+=5;
			if (strncmp(data,"SPF",3) == 0)
			{	// hlaseni zaruseni
				// napr: "$PMTKSPF,1*5A"
				data+=3;
				switch (data[1])
				{
				case '1': // No jamming, healthy status
					gps_jamming=0;
					return;
				case '2': // Warning status
					gps_jamming=1;
					break;
				case '3': // Critical status
					gps_jamming=2;
					break;
				}
				goto echo;
			}
			else if (strncmp(data,"001",3) == 0)
			{	// je to "ACK"
				// napr: "$PMTK001,838,3,1"
				pmtk_ack = 1; // TODO: zatim neresim typ odpovedi
				goto echo;
			}
			else
			{
				goto echo;
			}
		}
		else if (stricmp2(data, "$PQ") == 0)
		{	// quectel proprietary message
			goto echo;
		}
		return;
	}

	gps_rx_ok = true;
	data+=NMEA_PREFIX_LEN;

	protocol_id = *data;

	if ((protocol_id != GPS_ID)
	 && (protocol_id != GLONASS_ID)
	 && (protocol_id != MIXED_ID))
		return; // neznamy typ druzice
	
	data++; // preskocim az na ID dat

	if (stricmp2(data, STR_GGA)==0)
	{	// $GPGGA,132321.000,5043.78920,N,01510.60985,E,0,06,1.6,553.47,M,44.6,M,,*6B
		// $GPGGA,235952.000,0000.00000,N,00000.00000,E,0,00,99.0,082.00,M,18.0,M,,*54
		// $GPGGA,163816.000,5043.2596,N,01510.5021,E,1,6,1.29,533.4,M,44.6,M,,*55
		// $GPGGA,162345.533,,,,,0,0,,,M,,M,,*4A
		// 1=time, 2,3=lat, 4,5=lon, 6=fix, 7=sat, 8=acc, 9=alt, 

		// time and date get from RMC

		// LAT
		if (_get_nparam (buf, data, 2, PARM_SZ_LIMIT) > 8) 
		{	// lat "5043.7892"
			buf[9]='\0';  // omezim pocet cifer 
			gps_stamp_tmp.lat_sec = atoi (buf+2) * (60*100);
			gps_stamp_tmp.lat_sec += atoi (buf+5) * 6 / 10;
			buf[2]='\0'; // ted chci cele minuty prevest na sekundy
			gps_stamp_tmp.lat_sec += atoi (buf) * (60*60*100);
		}
		if (_get_nparam (buf, data, 3, PARM_SZ_LIMIT) > 0) // N
		{
			if (buf[0] != 'N') 
				gps_stamp_tmp.lat_sec = 0 - gps_stamp_tmp.lat_sec;
		}
		// LON
		if (_get_nparam (buf, data, 4, PARM_SZ_LIMIT) > 9) 
		{	// 01510.6099
			buf[10]='\0';  // omezim pocet cifer 
			gps_stamp_tmp.lon_sec = atoi (buf+3) * (60*100);
			gps_stamp_tmp.lon_sec += atoi (buf+6) * 6 / 10;
			buf[3]='\0'; // ted chci cele minuty
			gps_stamp_tmp.lon_sec += atoi (buf) * (60*60*100);
		}
		if (_get_nparam (buf, data, 5, PARM_SZ_LIMIT) > 0) // E
		{
			if (buf[0] != 'E') 
				gps_stamp_tmp.lon_sec = 0 - gps_stamp_tmp.lon_sec;
		}
		// Q: nebudou se hadat informace z ruznych druzic GN/GP ? 
		// A: nebudou, posila to s MIXED_ID
		gps_stamp_tmp.fix      = text_get_u32_param (data, 6); // fix, 0=invalid, 1=GNS, 2=GPS (info L70-R)
		gps_stamp_tmp.nbsat    = text_get_u32_param (data, 7); // nbsat (in use)
		gps_stamp_tmp.accuracy = text_get_u32_param (data, 8); // acc
		gps_stamp_tmp.alt      = text_get_u32_param (data, 9); // alt

		valid_data |= GPS_NMEA_GGA;
		if (gps_unsolicited & GPS_NMEA_GGA)
			echo_it=true;
	}
	else if (stricmp2(data, STR_GSA) == 0)
	{	// bylo by asi dobry "fix" zpracovavat z GSA (3)
		// tady totiz je 1=No fix, 2=2D fix, 3=3D fix (info L70-R)
		if (gps_unsolicited & GPS_NMEA_GSA)
			echo_it=true;
		valid_data |= GPS_NMEA_GSA;
	}
	else if (stricmp2(data, STR_GSV) == 0)
	{	// 
		// $GPGSV,3,1,12,21,78,155,,16,51,300,19,29,34,088,14,06,32,281,17*7E
		// $GPGSV,3,2,12,18,23,150,,27,21,278,30,31,12,213,22,03,11,283,23*75
		// $GPGSV,3,3,12,05,11,033,,08,10,331,,25,03,145,,22,01,176,*72
		// $GLGSV,3,1,09,83,59,295,25,73,46,246,22,82,42,062,,74,36,318,15*66
		// $GLGSV,3,2,09,67,34,084,,66,20,026,,80,14,198,,68,14,137,*62
		// $GLGSV,3,3,09,84,11,268,*5C
		//
		if (gps_unsolicited & GPS_NMEA_GSV)
			echo_it=true;
		// z GSV mne zajima pocet satelitu
		// tady je pocet viditelnych, nikoli pouzivanych satelitu
		// if (_get_nparam (buf, data, 3, PARM_SZ_LIMIT) > 0)
		// 	gps_stamp_tmp.nbsat=atoi(buf);
		if (protocol_id == GLONASS_ID)
		{
			glonass_used = true;
			glonass_nbsat_visible = text_get_u32_param (data, 3);
		}
		else
		{
			gps_nbsat_visible = text_get_u32_param (data, 3);
		}
		valid_data |= GPS_NMEA_GSV;
		// printf("[nbsat=%d]", gps_stamp_tmp.nbsat);

	}
	else if (stricmp2(data, STR_RMC) == 0)
	{	// 
		// $GPRMC,063646.000,A,5043.759,N,01510.607,E,0.1,0.0,050407,0.0,W*71
		// $GPRMC,000011.000,V,0000.000,N,00000.000,E,0.0,0.0,140399,0.0,W*6B
		// $GPRMC,181120.000,A,5043.2565,N,01510.5001,E,0.31,64.34,081012,,,A*5F
		// $GNRMC,160556.000,A,5043.2764,N,01510.5055,E,0.88,21.64,120613,,,A*42
		//
		if (_get_nparam (buf, data, 1, PARM_SZ_LIMIT) >= 6) // time "063646"
		{
			gps_stamp_tmp.time.second=atoi(buf+4);
			buf[4]='\0';
			gps_stamp_tmp.time.minute=atoi(buf+2);
			buf[2]='\0';
			gps_stamp_tmp.time.hour=atoi(buf);
		}

		if (_get_nparam (buf, data, 7, PARM_SZ_LIMIT)) // speed in knots
		{	// 1 knot (kt) == 1.85200 km/h (kph)
			gps_stamp_tmp.speed = knots_to_10kmh(buf);
		}

		if (_get_nparam (buf, data, 9, PARM_SZ_LIMIT) == 6) // date "050407"
		{
			gps_stamp_tmp.time.year=atoi(buf+4);
			buf[4]='\0';
			gps_stamp_tmp.time.month=atoi(buf+2);
			buf[2]='\0';
			gps_stamp_tmp.time.day=atoi(buf);
		}

		if (_get_nparam (buf, data, 8, PARM_SZ_LIMIT) > 0)
			gps_stamp_tmp.angle=atoi(buf); // 0..365

		if (gps_unsolicited & GPS_NMEA_RMC)
			echo_it=true;
		valid_data |= GPS_NMEA_RMC;
	}
	else if (stricmp2(data, STR_VTG) == 0)
	{
		// $GPVTG,0.00,T,,M,0.00,N,0.00,K,N*32
		// $GPVTG,289.29,T,,M,0.06,N,0.11,K,A*33
		// pozor, L-70-R nema v defaultu VTG zapnute !
 		if (_get_nparam (buf, data, 7, PARM_SZ_LIMIT) > 0) // speed km/h
		{
 			// gps_stamp_tmp.speed=atoi(buf);
// 			printf ("[VTG a=%dkmh]", atoi(buf));
		}

		if (gps_unsolicited & GPS_NMEA_VTG)
			echo_it=true;
		valid_data |= GPS_NMEA_VTG;
	}
	else if (stricmp2(data, STR_GLL) == 0)
	{
		if (gps_unsolicited & GPS_NMEA_GLL)
			echo_it=true;
		valid_data |= GPS_NMEA_GLL;
	}
	else if (stricmp2(data, "TXT") == 0)
	{	// LC86L: $GPTXT,01,01,02,ANTSTATUS=OPEN*2B
	}
	else
	{	// neco uplne jineho, neznam
		echo_it=true;
	}
	if ((valid_data & GPS_NMEA_REQUIRED) == GPS_NMEA_REQUIRED)
	{	// mam kompletni udaj o poloze

		/*if (gps_stamp_tmp.fix>0)
		{	// protoze L10 udavala obcas nesmyslny cas, pockame az na "fix"
			// aby byla jistota, ze cas je OK
			// smysluplnost udaje o case se navic kontroluje ve funkci "rtc_sync_time"
			rtc_lib_sync_time_utc (&(gps_stamp_tmp.time), true);
		}*/

		// nejaka zakladni kontrola, ze to vypada jako poloha:
 		if (
			(gps_stamp_tmp.lat_sec >   90 * 6000 * 60)
		 || (gps_stamp_tmp.lat_sec <  -90 * 6000 * 60)
		 || (gps_stamp_tmp.lon_sec >  180 * 6000 * 60)
		 || (gps_stamp_tmp.lon_sec < -180 * 6000 * 60)
		 )
		{	// nesmyslna poloha, likviduju
			LOG_ERROR("invalid position");
			gps_stamp_tmp.lat_sec = 0;
			gps_stamp_tmp.lon_sec = 0;
		}

/*		if (gps_stamp_last.time.dw == gps_stamp_tmp.time.dw)
		{	// tohle je zasadni chyba, stoji cas z GPS
			error_cnt+=10; // davam vyssi vahu
		}*/

		if ((gps_stamp_last.lat_sec == gps_stamp_tmp.lat_sec)
		 && (gps_stamp_last.lon_sec == gps_stamp_tmp.lon_sec))
		{	// pocet stejnych poloh (vcetne nulovych)
			error_cnt++;
		}
		else
		{
			error_cnt=0;
		}

		// prekopiruju pracovni zaznam do pouzivane oblasti
		memcpy ((u8 *)&gps_stamp_last, (u8 *)&gps_stamp_tmp, sizeof(gps_stamp_t));

		if (gps_stamp_last.fix > 0)
		{
			memcpy ((u8 *)&gps_stamp_last_valid, (u8 *)&gps_stamp_last, sizeof(gps_stamp_t));
			// tracer_new_valid_stamp();
			fix_tm = os_timer_get();
			if (sleep_mode)
			{	// ve sleepu ziskam polohu a muzu vypnout // 
				// gps_suspend(); // ale ne ihned
				// pockam jeste par sekund na zpresneni polohy
				if (sleep_tmr < (SLEEP_TIMEOUT - SLEEP_DELAY))
					sleep_tmr = (SLEEP_TIMEOUT - SLEEP_DELAY);
			}
			// geofence_evaluate(gps_stamp_last_valid.lon_sec / 36, gps_stamp_last_valid.lat_sec / 36);  
		}
		memset ((u8 *)&gps_stamp_tmp, 0 , sizeof(gps_stamp_t));
		valid_data = 0;
	}

echo:
 	if (echo_it)
	{
		log_lock();
		OS_PUTTEXT (NL);
		OS_PUTTEXT (buf_data(inbuf)) ;
		log_unlock();
	}
}

bool gps_fix_ok (void)
{
	if (gps_stamp_last.fix)
	{
		if (gps_pwr_on_state)
			return (true);
	}
	return (false);
}

void gps_maintenance (void)
{
	os_timer_t now = os_timer_get();

	if (error_cnt > 120) // priblizne pocet sekund
	{	// 2 minuty nejaky problem
		if ((now > (gps_reset_tm  + 10 * OS_TIMER_MINUTE))
 		 && (now > fix_tm + (5 * OS_TIMER_MINUTE)))
		{	// 
			LOG_WARNING("forced reset");
			gps_reset();
		}
	}
	if (gps_init_rq)
	{
		if (gps_pwr_on_state == true)
		{
			if (gps_set_baudrate())
			{	// nastaveni baudrate OK
				// poslu dalsi konfiguraci
				pmtk_cmd("$PMTK838,1"); // jamming detection enable
			}
			gps_init_rq = false;
		}
	}
}

void gps_task (void)
{	// fast task
#define	GPS_RX_IDLE  0
#define	GPS_RX_DATA  1
#define	GPS_RX_CHSUM 2
	static u8 rx_state = 0;
	static u8 chsum=0;
	static u8 rx_chsum=0;
	u8 rx_char;

	while (gps_rx_char(&rx_char))
	{
		if ((rx_char == '\n') || (rx_char == '\r'))
			rx_char = '\0';
		else if (rx_char == '$')
		{
			rx_state = GPS_RX_DATA;
			chsum=rx_char;
			buf_clear(&gps_buf);
		}
		switch (rx_state)
		{
		case GPS_RX_IDLE:
			continue;
		case GPS_RX_DATA:
			if (rx_char == '*')
			{
				rx_state = GPS_RX_CHSUM;
				rx_chsum=0;
				break;
			}
			chsum ^= rx_char;
			break;

		case GPS_RX_CHSUM:
			if (rx_char == '\0')
				break;
			rx_chsum <<= 4;
			rx_chsum += ascii_to_hex ((u8)rx_char);
			break;

		default: // ERROR
			rx_state = GPS_RX_IDLE;
			continue;
		}

		if (! buf_append_char (&gps_buf, rx_char))
		{
			LOG_ERROR("buf overflow");
			buf_clear(&gps_buf);
			rx_state=GPS_RX_IDLE;
			continue;
		}
		if (rx_char != '\0')
			continue;
		
		// we have whole line 
		// check CRC
		if (chsum != rx_chsum)
		{
			printf("[GPS CHSUM ERR]");
		}
 		else
		{
			gps_data_parser (&gps_buf);
		}
		rx_state=0;
		buf_clear(&gps_buf);
	}

}

void gps_tick (void)
{ 	// 100ms
	if (sleep_mode)
	{
		if (sleep_postpone)
		{	// prikaz podrzet zapnutou GPS
			sleep_postpone--;
		}
		else
		{
			sleep_tmr++;
		}

		if (sleep_tmr > SLEEP_WAKEUP)
		{
			gps_wakeup();
			sleep_tmr=0;
		}
		else if (sleep_tmr == SLEEP_TIMEOUT)
		{	// 
			gps_suspend();
		}
	}
}

void gps_temporary_start(void)
{	// podezreni na pohyb, docasny start GPS
	// zapne a po ziskani polohy zas usne, pokud neni pouzito gps_temporary_start_tmout()
	if (! sleep_mode)
		return; // ma to smysl jen ve sleepu
	sleep_tmr=0;
	if (gps_pwr_on_state == true)
		return; // je to probuzene, asi pravidelny test
	gps_wakeup();
}

void gps_temporary_start_tmout(u32 seconds)
{
	u32 tm = seconds * 10; // tick timer 10Hz

	if (tm > GPS_MAX_WAKEUP_TIME)
		return;

	gps_temporary_start();

	if (tm > sleep_postpone)
	{	// pokud je volano vicekrat v kratke dobe, tak se vezme delsi cas
		sleep_postpone = tm;
	}
}

u16 gps_get_speed (void)
{
	if (! gps_fix_ok())
		return (0);
	// we use speed * 10
	return ((gps_stamp_last.speed+5)/10);
}

u8 gps_get_nbsat (bool in_use)
{
	if (in_use)
		return (gps_stamp_last.nbsat);
	if(gps_stamp_last.nbsat>gps_nbsat_visible)
		return (gps_stamp_last.nbsat); // tohle by se nemelo nikdy stat, ale pro jistotu
	return (gps_nbsat_visible);
}

u8 gps_get_glonass_nbsat (bool in_use)
{
	if (! glonass_used)
		return (0);
	if (in_use)
		return (0); // nevime kolik je pouzivanych
	return (glonass_nbsat_visible);
}

/*
void gps_debug_show_info (void)
{
	DBG_PRINTF("# GPS: gps_pwr_on_state=%d, sleep_mode=%d, sleep_postpone=%d, sleep_tmr=%d\r\n", 
		gps_pwr_on_state, sleep_mode, sleep_postpone, sleep_tmr); 
}
*/
