#ifndef  PLATFORM_SETUP_H
#define  PLATFORM_SETUP_H

#include "type.h"

#include <stdio.h>
#include <stdlib.h>


#define OS_GETCHAR()                getChar()

#define ASSIGN(p,t) if(p){while(1);}
#define ASSERT(p,t) if(p){while(1);}

#define PACK __attribute__((packed))
#define PACK_BEGIN
#define PACK_END

#define OS_PLATFORM_NAME        "STM32xx"

#define OS_MEM_ALLOC malloc
#define OS_MEM_FREE free

#define NL "\r\n"

#define OS_PUTTEXT(x) printf(x)
#define OS_PRINTF     printf
#define OS_PRINTFE(...) fprintf(stderr, __VA_ARGS__)
#define OS_WARNING(...) { fprintf(stderr, "WARNING:" __VA_ARGS__); fprintf(stderr, NL); }
#define OS_ERROR(...) { fprintf(stderr, "ERROR:" __VA_ARGS__); fprintf(stderr, NL); }
#define OS_FATAL(...) { fprintf(stderr, "FATAL ERROR:" __VA_ARGS__); fprintf(stderr, NL); exit(-1) ;}

#define OS_DELAY(x) usleep(1000*x)

#endif //  PLATFORM_SETUP_H
