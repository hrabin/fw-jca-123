#include "common.h"
#include "app_info.h"

const app_info_t APP_INFO __attribute__((section(".APP_INFO"))) = {
	.size           = 0xFFFFFFFF,
	.crc            = 0xFFFFFFFF,
	.device_id      = 1,
	.hw_version_min = 1,
	.hw_version_max = 1
};

