// © 2024 Oskar Arnudd

// Firmware headers
#include "main.h"
#include "led.h"
#include "irdecoder.h"

// Library headers
#include "syscfg.h"
#include "cli.h"
#include "rcc.h"
#include "usart.h"
#include "utils.h"

// Standard library headers
#include <stdint.h>
#include <stdbool.h>

static void jump_to_bootloader(const char* args);
static void deinit(void);

static command_callback_t ir_commands[] = {
    { 0, 0, led_toggle_verbosity},
    { 1, "binary", led_set_pattern},
    { 2, "wave", led_set_pattern},
    { 3, "alternating", led_set_pattern},
    { 4, "bounce", led_set_pattern},
    { 5, 0, 0},
    { 6, 0, 0},
    { 7, 0, 0},
    { 8, 0, 0},
    { 9, 0, 0},
    { 10, 0, led_toggle},
    { 11, "+", led_toggle_pattern},
    { 12, "-", led_toggle_pattern},
    { 13, 0, led_speed_increase},
    { 14, 0, led_speed_decrease},
    { 15, 0, 0},
    { 16, 0, 0},
    { 17, 0, 0},
    { 18, 0, 0},
    { 19, 0, 0},
};

static const command_entry_t command_table[] = {
    { "pattern", led_toggle_pattern },
    { "faster", led_speed_increase },
    { "slower", led_speed_decrease },
    { "speed", led_speed_set },
    { "power", led_toggle },
    { "print", led_toggle_verbosity },
    { "flash", jump_to_bootloader },
    { 0, 0 }
};

int main(void)
{
    init();

    irdecoder_set_commands(ir_commands, 20);

    while(1){
	cli_process_input();
	irdecoder_process();
    }
}

void init(void)
{
    // Enable IRQs
    __asm volatile ("cpsie i");

    cli_init(main);
    led_init();
    led_set_message_cb(cli_printline);
    irdecoder_init();
    /*bt_init();*/

    cli_clear();
    cli_home();
    cli_cursive();
    cli_print("Blinky REPL v0.1");
    cli_normal();
    cli_newline();
    cli_newline();
    cli_print("> ");
}

static void deinit(void)
{
    led_deinit();
    irdecoder_deinit();
    /*bt_deinit();*/
    cli_deinit();
    rcc_reset_all();
}

static void jump_to_bootloader(const char* args)
{
    // Disable IRQs
    __asm volatile ("cpsid i");

    uint32_t* bl_vector = (uint32_t*)(0x08000000);
    uint32_t* sram_vector = (uint32_t*)(0x20000000);

    RCC->APBENR2 |= RCC_APB2_SYSCFG;
    // Mirroring RAM onto 0x00000000
    SYSCFG_CFGR1 &= ~0x3;

    for(uint32_t i = 0; i < 0xC0 / 4; i++){
        sram_vector[i] = bl_vector[i];
    }

    uint32_t bl_stack = bl_vector[0];
    uint32_t bl_reset = bl_vector[1];

    if (bl_reset < 0x08000000 || bl_reset > 0x08004000){
        cli_print("Invalid reset location. Aborting\r\n");
        return;
    }

    if (bl_stack < 0x20000000 || bl_stack > 0x20009000){
        cli_print("Invalid stack location. Aborting\r\n");
        cli_print_number(bl_stack);
        cli_print("\r\n");
        while(1);
        return;
    }

    deinit();

    // Set MSP to applications stack
    __asm volatile ("msr msp, %0" :: "r" (bl_stack));

    // Jump to bootloader reset handler
    void (*bl_reset_handler)(void) = (void (*)(void)) bl_reset;
    bl_reset_handler();
}

bool cli_parse_application_command(command_t tokens, char token_length)
{
    if(token_length < 1){
	return false;
    }

    char* args = (token_length > 1) ? tokens[1] : 0;

    for(int i = 0; command_table[i].command != 0; i++){
	if(utils_strings_match(tokens[0], command_table[i].command)){
	    cli_newline();
	    cli_newline();
	    command_table[i].handler(args);
	    return true;
	}
    }
    return false;
}

void cli_print_application_help(void)
{
    cli_print("**************************");
    cli_newline();
    cli_print("** Application commands **");
    cli_newline();
    cli_print("**************************");
    cli_newline();
    cli_newline();
    cli_print("speed+        - Increases the speed");
    cli_newline();
    cli_newline();
    cli_print("speed-        - Decreases the speed");
    cli_newline();
    cli_newline();
    cli_print("mode          - Changes the mode");
    cli_newline();
    cli_print("-------------------------------------------");
    cli_newline();
    cli_newline();
}
