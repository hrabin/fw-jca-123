#ifndef SERVER_H
#define SERVER_H

#include "type.h"

void server_init(void);
void server_send(const void *buf, size_t count);
void server_task(void);

#endif // ! SERVER_H
