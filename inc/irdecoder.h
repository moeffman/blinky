// © 2024 Oskar Arnudd

#ifndef IRDECODER_H
#define IRDECODER_H

#include <stdint.h>

#define ADDRESS (0x20)

typedef enum{
    IR_KP_0  = 0x10,
    IR_KP_1  = 0x11,
    IR_KP_2  = 0x12,
    IR_KP_3  = 0x13,
    IR_KP_4  = 0x14,
    IR_KP_5  = 0x15,
    IR_KP_6  = 0x16,
    IR_KP_7  = 0x17,
    IR_KP_8  = 0x18,
    IR_KP_9  = 0x19,
    IR_VL_UP = 0x2,
    IR_VL_DN = 0x3,
    IR_CH_UP = 0x0,
    IR_CH_DN = 0x1,
    IR_DP_LE = 0x7,
    IR_DP_RI = 0x6,
    IR_DP_UP = 0x40,
    IR_DP_DN = 0x41,
    IR_DP_OK = 0x44,
    IR_PW    = 0x8,
}ir_command;

typedef struct{
    char id;
    char* arg;
    void (*command)(const char*);
}command_callback_t;

void irdecoder_init(void);

void irdecoder_deinit(void);

void irdecoder_set_commands(const command_callback_t* commands, uint8_t count);

void irdecoder_process(void);

#endif
