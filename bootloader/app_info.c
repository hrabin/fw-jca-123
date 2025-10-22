#include "common.h"
#include "app_info.h"

const app_info_t APP_INFO __attribute__((section(".APP_INFO"))) __attribute__((used)) = {
	.size           = 0xFFFFFFFF,
	.crc            = 0xFFFFFFFF,
	.device_id      = 0xFFFF,
	.hw_version_min = 0xFFFF,
	.hw_version_max = 0xFFFF
};



u32 app_crc(u8 *data, int len) 
{
	int i;
	u32 b, crc, mask;

	crc = 0xFFFFFFFF;

	while (len--) 
	{
		b = *data++;

		crc = crc ^ b;
		for (i = 7; i >= 0; i--) 
		{   // Do eight times.
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}
	return ~crc;
}
