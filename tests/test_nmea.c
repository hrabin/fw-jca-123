#include "test.h"
#include "nmea.h"
#include <string.h>

// ---- nmea_parse_u32 ----

TEST(nmea_parse_u32_basic)
{
	//              0       1         2     3
	const char *s = "$GPGGA,132321,5043.789,N,01510.609,E,1,06,1.6,553,M";

	ASSERT_EQ(nmea_parse_u32(s, 6), 1);     // fix quality
	ASSERT_EQ(nmea_parse_u32(s, 7), 6);     // satellites
	ASSERT_EQ(nmea_parse_u32(s, 8), 1);     // HDOP — atol("1.6") = 1
}

TEST(nmea_parse_u32_empty_field)
{
	const char *s = "$GPGGA,,,,,,,,";

	ASSERT_EQ(nmea_parse_u32(s, 0), 0);
	ASSERT_EQ(nmea_parse_u32(s, 6), 0);
	ASSERT_EQ(nmea_parse_u32(s, 99), 0);    // out of range
}

// ---- nmea_parse_gga ----

TEST(nmea_parse_gga_fix_ok)
{
	nmea_data_t d;
	memset(&d, 0, sizeof(d));

	// $GPGGA,132321.000,5043.78920,N,01510.60985,E,1,06,1.6,553.47,M,44.6,M,,*6B
	const char *s = "$GPGGA,132321,5043.7892,N,01510.6098,E,1,06,1.6,553,M,44.6,M,,";

	bool ok = nmea_parse_gga(&d, s);
	ASSERT(ok);

	// Latitude: 50°43.7892'N → 50*360000 + 43*6000 + 7892*6/10
	// = 18000000 + 258000 + 4735 = 18262735
	ASSERT(d.lat_sec > 0);
	ASSERT(d.lon_sec > 0);
	ASSERT_EQ(d.fix, 1);
	ASSERT_EQ(d.nbsat, 6);
	ASSERT_EQ(d.accuracy, 1);   // "1.6" — atol stops at '.'
	ASSERT_EQ(d.alt, 553);
}

TEST(nmea_parse_gga_south_west)
{
	nmea_data_t d;
	memset(&d, 0, sizeof(d));

	// S = negative lat, W = negative lon
	const char *s = "$GPGGA,120000,3300.0000,S,01800.0000,W,2,08,0.9,100,M,50,M,,";

	bool ok = nmea_parse_gga(&d, s);
	ASSERT(ok);
	ASSERT(d.lat_sec < 0);
	ASSERT(d.lon_sec < 0);
	ASSERT_EQ(d.fix, 2);
	ASSERT_EQ(d.nbsat, 8);
	ASSERT_EQ(d.alt, 100);
}

TEST(nmea_parse_gga_no_fix)
{
	nmea_data_t d;
	memset(&d, 0xFF, sizeof(d));  // fill with garbage

	// fix=0, empty position fields
	const char *s = "$GPGGA,235952.000,0000.00000,N,00000.00000,E,0,00,99.0,082.00,M,18.0,M,,";

	bool ok = nmea_parse_gga(&d, s);
	ASSERT(ok);   // still parses OK, just fix=0
	ASSERT_EQ(d.fix, 0);
	ASSERT_EQ(d.nbsat, 0);
}

// ---- nmea_parse_rmc ----

TEST(nmea_parse_rmc_full)
{
	nmea_data_t d;
	memset(&d, 0, sizeof(d));

	// $GPRMC,181120.000,A,5043.2565,N,01510.5001,E,0.31,64.34,081012,,,A
	const char *s = "$GPRMC,181120,A,5043.2565,N,01510.5001,E,0.31,64.34,081012,,,A";

	bool ok = nmea_parse_rmc(&d, s);
	ASSERT(ok);

	ASSERT_EQ(d.time.hour, 18);
	ASSERT_EQ(d.time.minute, 11);
	ASSERT_EQ(d.time.second, 20);
	ASSERT_EQ(d.time.day, 8);
	ASSERT_EQ(d.time.month, 10);
	ASSERT_EQ(d.time.year, 12);
	ASSERT(d.speed > 0);     // 0.31 knots → ~5 km/h*10
	ASSERT_EQ(d.angle, 64);
}

TEST(nmea_parse_rmc_invalid)
{
	nmea_data_t d;
	memset(&d, 0, sizeof(d));

	// V = void (invalid)
	const char *s = "$GPRMC,000011.000,V,0000.000,N,00000.000,E,0.0,0.0,140399,0.0,W";

	bool ok = nmea_parse_rmc(&d, s);
	ASSERT(ok);   // still parses — validity checked via fix, not here

	ASSERT_EQ(d.time.minute, 0);
	ASSERT_EQ(d.speed, 0);
}

// ---- nmea_parse_gsv ----

TEST(nmea_parse_gsv_gps)
{
	u8 nbsat;
	bool is_glonass;

	// $GPGSV,3,1,12,21,78,155,,16,51,300,19,29,34,088,14,06,32,281,17
	const char *s = "$GPGSV,3,1,12,21,78,155,,16,51,300,19,29,34,088,14,06,32,281,17";

	nmea_parse_gsv(&nbsat, &is_glonass, 'P', s);

	ASSERT_EQ(is_glonass, false);
	ASSERT_EQ(nbsat, 12);    // param 3 = total visible
}

TEST(nmea_parse_gsv_glonass)
{
	u8 nbsat;
	bool is_glonass;

	const char *s = "$GLGSV,3,1,09,83,59,295,25,73,46,246,22,82,42,062,,74,36,318,15";

	nmea_parse_gsv(&nbsat, &is_glonass, 'L', s);

	ASSERT_EQ(is_glonass, true);
	ASSERT_EQ(nbsat, 9);
}

// ---- nmea_checksum_compute ----

TEST(nmea_checksum)
{
	// "GPGGA,132321" — XOR of each char
	// G^P^G^G^A^,^1^3^2^3^2^1
	const char *s = "GPGGA,132321";
	int len = strlen(s);

	u8 cs = nmea_checksum_compute(s, len);

	// Verify manually: G(0x47)^P(0x50)=0x17, ^G=0x50, ^G=0x17, ^A=0x56,
	// ^,(0x2C)=0x7A, ^1(0x31)=0x4B, ^3=0x78, ^2=0x4A, ^3=0x79, ^2=0x4B, ^1=0x7A
	ASSERT_EQ(cs, 0x7A);
}

// ---- end-to-end: parse a complete position from real-world sentences ----

TEST(nmea_full_position)
{
	nmea_data_t d;
	memset(&d, 0, sizeof(d));

	// Real GGA + RMC pair from Prague
	const char *gga = "$GPGGA,132321.000,5043.78920,N,01510.60985,E,1,06,1.6,553.47,M,44.6,M,,";
	const char *rmc = "$GPRMC,132322.000,A,5043.7892,N,01510.6098,E,0.31,64.34,081012,,,A";

	bool ok_gga = nmea_parse_gga(&d, gga);
	bool ok_rmc = nmea_parse_rmc(&d, rmc);

	ASSERT(ok_gga);
	ASSERT(ok_rmc);
	ASSERT_EQ(d.fix, 1);
	ASSERT_EQ(d.nbsat, 6);
	ASSERT(d.lat_sec > 0);
	ASSERT(d.lon_sec > 0);
	ASSERT_EQ(d.time.hour, 13);
	ASSERT_EQ(d.time.minute, 23);
	ASSERT_EQ(d.time.second, 22);
	ASSERT_EQ(d.speed, 5);   // 0.31 knots * 1.852 * 10 ≈ 5.7 → 5
}
