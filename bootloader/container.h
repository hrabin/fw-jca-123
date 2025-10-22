#ifndef CONTAINER_T
#define CONTAINER_T

#include "type.h"

#define CONTAINER_FW_NAME_LEN (16)

typedef struct
{
/* 0*/ u32 data_size;       // size including padding
/* 4*/ u32 data_chsum;      // raw data chsum
/* 8*/ u32 generated_for_hw_num; // vyrobni cislo, pro ktere je kontejner generovan (0 .. bez omezeni)
/*12*/ u16 device_id;      //
/*14*/ u16 hw_version_min; // 
/*16*/ u16 hw_version_max; // 
/*18*/ u8  fw_name[CONTAINER_FW_NAME_LEN]; // jmeno firmware
/*34*/ bool aes;      // 
/*35*/ u8  res[512 - 35 - 4];
       u32 hdr_chsum; // kontrolni soucet hlavicky

} container_hdr_t;

bool container_valid(u8 *data, int len);
bool container_compatible (container_hdr_t *container);

#endif // ! CONTAINER_T
