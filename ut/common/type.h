#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>

#define     ON              1
#define     OFF             0

typedef char                ascii;
typedef unsigned char       byte;

typedef int8_t              s8;
typedef int16_t             s16;
typedef int32_t             s32;
typedef int64_t             s64;

typedef uint8_t             u8;
typedef uint16_t            u16;
typedef uint32_t            u32;
typedef uint64_t            u64;

#ifndef KB
	#define KB 1024
#endif

#endif // ! TYPE_H
