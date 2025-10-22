#ifndef UPDATE_H
#define UPDATE_H

#include "type.h"

typedef struct {
    u32 packet_id;
    u32 unit_id;
} update_hdr_t;

typedef struct {
    update_hdr_t hdr;
    u32 ver; // current version info
} update_rq_packet_t;

#define UPDATE_RESULT_NO_FW (0x00000000) // no newer FW, cant update
#define UPDATE_RESULT_START (0x000000FF) // update in proggress

typedef struct {
    update_hdr_t hdr;
    u32 ver; // version going to update
    u32 result;
} update_res_packet_t;

typedef struct {
    update_hdr_t hdr;
    u32 chunk_id;
} update_rq_data_packet_t;

#define UPDATE_CHUNK_SIZE 256
#define UPDATE_CHUNK_STATUS_ERROR 0 // 
#define UPDATE_CHUNK_STATUS_DONE  1 // this is last chunk
#define UPDATE_CHUNK_STATUS_OK    2 // not the last chun

typedef struct {
    update_hdr_t hdr;
    u32 chunk_id:24;
    u32 status:8;
    u8 data[UPDATE_CHUNK_SIZE];
} update_res_data_packet_t;

#define PACKET_ID_UPDATE_REQ (0x00AA)
#define PACKET_ID_UPDATE_RES (0xF0AA)
#define PACKET_ID_GET_CHUNK  (0x01AA)
#define PACKET_ID_CHUNK_RES  (0xF1AA)

bool update_packet_rx (u8 *data, u16 len, u16 port);
bool update_start(void);
void update_task(void);


#endif // ! UPDATE_H

