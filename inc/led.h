// © 2024 Oskar Arnudd

#ifndef LED_H
#define LED_H

#include <stdint.h>
#include <stdbool.h>

typedef enum{
    PATTERN_BINARY = 0,
    PATTERN_WAVE = 1,
    PATTERN_ALTERNATING = 2,
    PATTERN_BOUNCE = 3,
} led_pattern_t;

typedef enum{
    SPEED_SLOWER = 1000,
    SPEED_SLOW = 500,
    SPEED_NORMAL = 250,
    SPEED_FAST = 125,
    SPEED_FASTER = 62,
} led_speed_t;

typedef struct{
    bool active;
    uint16_t tick;
    uint16_t count;
    led_pattern_t pattern;
    led_speed_t speed;
} led_state_t;

#define LED_BOUNCE_RESET 252

void led_init(void);

void led_deinit(void);

void led_update(void);

void led_toggle_pattern(const char* args);

void led_set_pattern(const char* args);

void led_speed_increase(const char* args);

void led_speed_decrease(const char* args);

void led_speed_set(const char* args);

void led_stop(const char* args);

void led_start(const char* args);

void led_toggle(const char* args);

void led_reset(void);

void led_state_reset(void);

#endif
