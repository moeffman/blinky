// © 2024 Oskar Arnudd

#ifndef BT_H
#define BT_H

#include <stdint.h>

void bt_init(void);

void bt_deinit(void);

void bt_send_string(char* string);

void bt_send_byte(uint8_t byte);

#endif
