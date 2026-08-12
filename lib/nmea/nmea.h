#ifndef NMEA_H
#define NMEA_H

#include "type.h"
#include "rtc_type.h"

// Canonical GPS data from NMEA sentences.
// Used as the parameter type for all nmea_parse_*() functions.
// Application layer typedefs this as gps_stamp_t (see gps.h).

typedef struct {
	s32 lon_sec;      // longitude, seconds*100, negative = West
	s32 lat_sec;      // latitude, seconds*100, negative = South
	u32 speed:15;     // km/h * 10
	u32 angle:9;      // 0..359 degrees
	u32 accuracy:8;   // meters
	s16 alt;          // meters
	u8  fix;          // 0=invalid, 1=GPS, 2=DGPS
	u8  nbsat;        // satellites in use
	rtc_t time;        // UTC time of fix
} __attribute__((packed)) nmea_data_t;

// Parse $--GGA sentence. Fills lat_sec, lon_sec, fix, nbsat, accuracy, alt.
// Returns true if the position fields were valid.
bool nmea_parse_gga(nmea_data_t *data, const char *sentence);

// Parse $--RMC sentence. Fills time, speed, angle.
// Returns true if meaningful data was extracted.
bool nmea_parse_rmc(nmea_data_t *data, const char *sentence);

// Parse $--GSV sentence. Returns number of visible satellites.
// Sets *is_glonass based on protocol_id character ('L' = GLONASS).
void nmea_parse_gsv(u8 *nbsat_visible, bool *is_glonass,
                    char protocol_id, const char *sentence);

// Get n-th comma-separated parameter as u32.
u32  nmea_parse_u32(const char *sentence, int param_n);

// Compute NMEA checksum (XOR of all bytes between '$' and '*').
u8   nmea_checksum_compute(const char *s, int len);

#endif // ! NMEA_H
