// © 2024 Oskar Arnudd

#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct ring_buffer_t ring_buffer_t;

#define COMMAND_MAX_LENGTH (16)
#define COMMAND_MAX_AMOUNT (3)

typedef char command_t[COMMAND_MAX_AMOUNT][COMMAND_MAX_LENGTH];

typedef struct{
    const char* command;
    void (*handler)(const char* args);
} command_entry_t;

void cli_init(int (*restart_function)(void));

void cli_deinit(void);

void cli_clear(void);

void cli_home(void);

void cli_cursive(void);

void cli_normal(void);

void cli_print(const char *string);

void cli_printline(const char *string);

void cli_print_number(uint32_t number);

void cli_newline(void);

void cli_backspace(void);

void cli_tokenize(ring_buffer_t* ring_buffer, command_t tokens, char* token_length);

void cli_parse_command(command_t tokens, char token_length);

void cli_process_input(void);

bool cli_parse_application_command(command_t tokens, char token_length);

void cli_memdump_bin(const char* args);

void cli_memdump_hex(const char *args);

void cli_restart(const char* args);

void cli_print_help(const char* args);

void cli_print_library_help();

void cli_print_application_help();

void cli_print_welcome_message(void);

void cli_dump_hex_from_address(uint32_t address);

void cli_dump_bin_from_address(uint32_t address);

void cli_led_config(char* function, char* led);

#endif
