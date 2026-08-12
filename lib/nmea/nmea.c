#include "common.h"
#include "nmea.h"
#include "parse.h"

#define PARAM_BUF_SIZE 32

// ---- internal helpers ----

static int param_pos(const char *src, int n)
{
	int l = 0;

	while (*src != '\0')
	{
		if (n == 0)
			break;
		l++;
		if (*src == ',')
			n--;
		if (n <= 0)
			break;
		src++;
	}
	return (n == 0 ? l : -1);
}

static int get_param(char *dest, const char *src, int n, u16 limit)
{
	int len = 0;
	bool copy = false;

	while (*src != '\0')
	{
		if (!copy)
		{
			if (n == 0)
			{
				copy = true;
				len = 0;
			}
		}

		if (*src == ',')
		{
			n--;
			if (n < 0)
				break;
		}
		else if (copy)
		{
			if (limit-- > 0)
				*dest++ = *src, len++;
			else
				break;
		}
		src++;
	}
	*dest = '\0';
	return (copy ? len : -1);
}

static u16 knots_to_10kmh(const char *s)
{
	int a, b;
	s32 tmp = 0;

	switch (sscanf(s, "%d.%1d", &a, &b))
	{
	case 2: tmp = b;
	case 1:
		tmp += 10 * a;
		tmp *= 1852;
		tmp /= 1000;
		return (tmp);
	}
	return (0);
}

// ---- public API ----

u32 nmea_parse_u32(const char *sentence, int param_n)
{
	int pos;

	if ((pos = param_pos(sentence, param_n)) > 0)
		return (atol(sentence + pos));
	return (0);
}

bool nmea_parse_gga(nmea_data_t *stamp, const char *sentence)
{
	char buf[PARAM_BUF_SIZE];
	s32 lat, lon;

	// LAT (param 2) — "ddmm.mmmm"
	if (get_param(buf, sentence, 2, sizeof(buf) - 1) <= 8)
		return false;
	buf[9] = '\0';
	lat  = atoi(buf + 2) * (60 * 100);
	lat += atoi(buf + 5) * 6 / 10;
	buf[2] = '\0';
	lat += atoi(buf) * (60 * 60 * 100);

	// N/S hemisphere (param 3)
	if (get_param(buf, sentence, 3, sizeof(buf) - 1) > 0
	    && buf[0] != 'N')
		lat = -lat;

	// LON (param 4) — "dddmm.mmmm"
	if (get_param(buf, sentence, 4, sizeof(buf) - 1) <= 9)
		return false;
	buf[10] = '\0';
	lon  = atoi(buf + 3) * (60 * 100);
	lon += atoi(buf + 6) * 6 / 10;
	buf[3] = '\0';
	lon += atoi(buf) * (60 * 60 * 100);

	// E/W hemisphere (param 5)
	if (get_param(buf, sentence, 5, sizeof(buf) - 1) > 0
	    && buf[0] != 'E')
		lon = -lon;

	stamp->lat_sec  = lat;
	stamp->lon_sec  = lon;
	stamp->fix      = nmea_parse_u32(sentence, 6);
	stamp->nbsat    = nmea_parse_u32(sentence, 7);
	stamp->accuracy = nmea_parse_u32(sentence, 8);
	stamp->alt      = nmea_parse_u32(sentence, 9);

	return true;
}

bool nmea_parse_rmc(nmea_data_t *stamp, const char *sentence)
{
	char buf[PARAM_BUF_SIZE];
	int len;

	// Time (param 1) — "hhmmss.sss"
	if ((len = get_param(buf, sentence, 1, sizeof(buf) - 1)) >= 6)
	{
		stamp->time.second = atoi(buf + 4);
		buf[4] = '\0';
		stamp->time.minute = atoi(buf + 2);
		buf[2] = '\0';
		stamp->time.hour   = atoi(buf);
	}

	// Speed (param 7) — knots
	if (get_param(buf, sentence, 7, sizeof(buf) - 1) > 0)
		stamp->speed = knots_to_10kmh(buf);

	// Date (param 9) — "ddmmyy"
	if (get_param(buf, sentence, 9, sizeof(buf) - 1) == 6)
	{
		stamp->time.year  = atoi(buf + 4);
		buf[4] = '\0';
		stamp->time.month = atoi(buf + 2);
		buf[2] = '\0';
		stamp->time.day   = atoi(buf);
	}

	// Angle (param 8)
	stamp->angle = nmea_parse_u32(sentence, 8);

	return true;
}

void nmea_parse_gsv(u8 *nbsat_visible, bool *is_glonass,
                    char protocol_id, const char *sentence)
{
	*is_glonass = (protocol_id == 'L');
	*nbsat_visible = nmea_parse_u32(sentence, 3);
}

u8 nmea_checksum_compute(const char *s, int len)
{
	u8 chsum = 0;
	while (len--)
		chsum ^= *s++;
	return (chsum);
}
