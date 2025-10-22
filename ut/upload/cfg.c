#include "common.h"
#include "cfg.h"
#include <getopt.h>

const cfg_t DEFAULT_CFG = {
	"/dev/ttyUSB0",
	"OK",
};

cfg_t cfg;


void cfg_print_help (void)
{
	OS_PRINTF("\t -d       .. serial device, default \"%s\"\n", DEFAULT_CFG.device);
	OS_PRINTF("\t -r       .. OK response, default \"%s\"\n", DEFAULT_CFG.response);
	OS_PRINTF("\t -h       .. this help\n");
}

static bool _configure( cfg_t *cfg, int argc, char *argv[] )
{
	struct option opt[] = {
		{ "device",   1, NULL, 'd' },
		{ "response", 1, NULL, 'r' },
		{ "help",    0, NULL, 'h' },
		{ NULL,      0, NULL, 0 }
	};
	int	o;

	memcpy(cfg, &DEFAULT_CFG, sizeof(cfg_t));

	while ( (o = getopt_long( argc, argv, "d:r:h", opt, NULL )) != -1 ) 
	{
		switch ( o ) {
			case 'd':
				strncpy(cfg->device, optarg, sizeof(cfg->device));
				break;

			case 'r':
				strncpy(cfg->response, optarg, sizeof(cfg->response));
				break;

			case 'h':
				return (false);

			default:
				return (false);
		}
	}
	return (true);
}

bool configure (int argc, char *argv[])
{
	return (_configure (&cfg, argc, argv));
}
