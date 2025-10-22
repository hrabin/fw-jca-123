#ifndef CFG_H
#define	CFG_H

#include "type.h"

#define	CFG_TEXT_LEN 64

typedef struct {
	bool encrypt;
	bool hex;
	ascii fw_name[CFG_TEXT_LEN];
} cfg_t;

extern cfg_t cfg;

extern void cfg_print_help (void);
extern bool configure(int argc, char *argv[]);

#endif // ! CFG_H

