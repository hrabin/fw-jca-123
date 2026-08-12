#ifndef GPS_H
#define GPS_H


#include "type.h"
#include "rtc.h"
#include "buf.h"
#include "nmea.h"

#define GPS_NMEA_GGA (1 << 0)
#define GPS_NMEA_GSA (1 << 1)
#define GPS_NMEA_GSV (1 << 2)
#define GPS_NMEA_RMC (1 << 3)
#define GPS_NMEA_VTG (1 << 4)
#define GPS_NMEA_GLL (1 << 5)
#define GPS_NMEA_PMTK (1 << 6)

#define GPS_NMEA_ALL (GPS_NMEA_GGA+GPS_NMEA_GSA+GPS_NMEA_GSV+GPS_NMEA_RMC+GPS_NMEA_VTG+GPS_NMEA_GLL)

typedef nmea_data_t gps_stamp_t;  // defined in lib/nmea/nmea.h

extern gps_stamp_t  gps_stamp_last;
extern gps_stamp_t  gps_stamp_last_valid;

extern bool gps_save_position (u16 mem_id, gps_stamp_t *pos);
extern bool gps_load_position (gps_stamp_t *dest, u16 mem_id);

extern bool gps_get_stored_stamp (gps_stamp_t *dest);
extern bool gps_get_current_stamp (gps_stamp_t *dest);
extern u32  gps_valid_stamp_age (void);
extern void gps_set_unso (u8 mask);
extern bool gps_init (void);
extern void gps_reset (void);
extern void gps_suspend (void);
extern void gps_sleep_enable (bool state);
extern void gps_wakeup (void);
extern bool gps_on (void);
extern bool gps_glonass_used (void);
extern bool gps_fix_ok (void);
extern void gps_maintenance (void);
extern void gps_task (void);
extern void gps_tick (void);
extern void gps_temporary_start(void);
extern void gps_temporary_start_tmout(u32 tm);
extern u16 gps_get_speed (void);
extern u8 gps_get_nbsat (bool in_use);
extern u8 gps_get_glonass_nbsat (bool in_use);

#endif  // ~GPS_H
