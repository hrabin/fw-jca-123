#include "common.h"
#include "cfg.h"
#include <getopt.h>

const cfg_t DEFAULT_CFG = {
	false,
	false
};

cfg_t cfg;


void cfg_print_help (void)
{
	OS_PRINTF("\t -e 1/0   .. encrypt using AES, default %d\n", DEFAULT_CFG.encrypt ? 1 : 0);
	OS_PRINTF("\t -x       .. use HEX output instead of BIN\n");
	OS_PRINTF("\t -n       .. fw name\n");
	OS_PRINTF("\t -h       .. this help\n");
}

static bool _configure( cfg_t *cfg, int argc, char *argv[] )
{
	struct option opt[] = {
		{ "encrypt", 1, NULL, 'e' },
		{ "hex",     0, NULL, 'x' },
		{ "name",    1, NULL, 'n' },
		{ "help",    0, NULL, 'h' },
		{ NULL,      0, NULL, 0 }
	};
	int	o;

	memcpy(cfg, &DEFAULT_CFG, sizeof(cfg_t));

	while ( (o = getopt_long( argc, argv, "e:xn:h", opt, NULL )) != -1 ) 
	{
		switch ( o ) {
			case 'e':
				cfg->encrypt = atol(optarg) ? true : false;
				break;

			case 'x':
				cfg->hex = true;
				break;
			case 'n':
				strncpy(cfg->fw_name, optarg, CFG_TEXT_LEN);
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
