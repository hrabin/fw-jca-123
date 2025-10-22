#ifndef BLE_H
#define BLE_H

#include "type.h"
#include "tty.h"

bool ble_init(tty_parse_callback_t callback);
bool ble_connected(void);
void ble_cmd(const ascii *buf);
void ble_send(const void *buf, size_t count);
void ble_task(void);

#endif // ! BLE_H
