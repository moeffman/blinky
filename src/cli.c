// © 2024 Oskar Arnudd

// Firmware includes
#include "cli.h"

// Library includes
#include "rcc.h"
#include "nvic.h"
#include "gpio.h"
#include "ringbuffer.h"
#include "usart.h"
#include "utils.h"

static ring_buffer_t ring_buffer_temp = {0U};
static uint8_t data_buffer_temp[RING_BUFFER_SIZE] = {0U};
static ring_buffer_t ring_buffer_data = {0U};
static uint8_t data_buffer[RING_BUFFER_SIZE] = {0U};

static usart_state_t usart_state = USART_STATE_IDLE;

static command_t command_history[10] = {0U};
static int8_t command_index = 0;

static int (*restart_handler)(void);

static const command_entry_t command_table[] = {
    { "rs", cli_restart },
    { "help", cli_print_help },
    { "memdump", cli_memdump_hex },
    { "memdumphex", cli_memdump_hex },
    { "mdh", cli_memdump_hex },
    { "memdumpbin", cli_memdump_bin },
    { "mdb", cli_memdump_bin },
    { 0, 0 }
};

void cli_init(int (*restart_function)(void))
{
    // Setting the restart_handler, used by the command "rs"
    restart_handler = restart_function;

    // Creating ring_buffers
    ring_buffer_create(&ring_buffer_temp, data_buffer_temp, RING_BUFFER_SIZE);
    ring_buffer_create(&ring_buffer_data, data_buffer, RING_BUFFER_SIZE);

    if(!(RCC->IOPENR & RCC_IO_GPIOA)){
	RCC->IOPENR |= RCC_IO_GPIOA;
    }
    // Using GPIOA_2 and GPIOA_3
    gpio_config_t cfg;
    cfg.mode = GPIO_MODER_AF;
    cfg.speed = GPIO_OSPEEDR_HIGH;
    cfg.af = GPIO_AF1;

    gpio_set(GPIOA, &cfg, BIT2 | BIT3);

    // Enable USART clock
    RCC->APBENR1 |= RCC_APB1_USART2;

    // RXNEIE enabled
    USART2->CR1 |= USART_CR1_RXNEIE;

    // RE enabled
    USART2->CR1 |= USART_CR1_RE;

    // TE enabled
    USART2->CR1 |= USART_CR1_TE;

    // BRR USARTDIV = FREQ/BAUDRATE
    // 16Mhz / 115200 = 139 = 0x8B
    USART2->BRR = 0x8B;

    // USART enabled
    USART2->CR1 |= USART_CR1_UE;

    // Enable USART in NVIC
    NVIC->ISER0 = NVIC_USART2_LPUART2;
}

void cli_deinit(void)
{
    // Reset GPIOA
    if(!(RCC->IOPENR & RCC_IO_GPIOA)){
	RCC->IOPENR |= RCC_IO_GPIOA;
    }

    gpio_config_t cfg;
    gpio_config_reset(&cfg);
    gpio_set(GPIOA, &cfg, 0xFFFF);
    GPIOA->MODER = 0xEBFFFFFF;

    // Reset USART
    RCC->APBENR1 |= RCC_APB1_USART2;

    USART2->CR1 = 0;
    USART2->BRR = 0;

    // Disable USART in NVIC
    NVIC->ICER0 = NVIC_USART2_LPUART2;
}

void cli_clear(void)
{
    usart_send_bytes(USART2, (uint8_t*)"\033[2J", 4);
}

void cli_home(void)
{
    usart_send_bytes(USART2, (uint8_t*)"\033[H", 3);
}

void cli_cursive(void)
{
    usart_send_bytes(USART2, (uint8_t*)"\033[3m", 4);
    usart_send_bytes(USART2, (uint8_t*)"\033[4m", 4);
}

void cli_normal(void)
{
    usart_send_bytes(USART2, (uint8_t*)"\033[23m", 5);
    usart_send_bytes(USART2, (uint8_t*)"\033[24m", 5);
}

void cli_print(const char* string)
{
    for(uint32_t i = 0; string[i]; i++){
	usart_send_byte(USART2, string[i]);
    }
}

void cli_printline(const char* string)
{
    for(uint32_t i = 0; string[i]; i++){
	usart_send_byte(USART2, string[i]);
    }
    usart_send_byte(USART2, '\r');
    usart_send_byte(USART2, '\n');
}

void cli_print_number(uint32_t number)
{
    if(number == 0){
        usart_send_byte(USART2, 48);
        return;
    }

    uint8_t arr[12] = {0U};
    uint32_t remainder;
    int32_t index = 11;

    while(number > 0){
	remainder = number % 10;
	number /= 10;
	arr[index] = remainder + 48; // 48-57 is ascii 0-9
	index--;
    }
    usart_send_bytes(USART2, arr, 12);
}

void cli_newline(void)
{
    usart_send_byte(USART2, '\n');
    usart_send_byte(USART2, '\r');
}

void cli_backspace(void)
{
    if(ring_buffer_delete(&ring_buffer_data)){
	usart_send_byte(USART2, '\b');
	usart_send_byte(USART2, 32);
	usart_send_byte(USART2, '\b');	
    }
}

void cli_tokenize(ring_buffer_t* ring_buffer, command_t tokens, char* token_length)
{
    uint8_t token = 0;
    uint8_t symbol = 0;

    while(!ring_buffer_empty(ring_buffer)){
        ring_buffer_read(ring_buffer, (uint8_t*)&tokens[token][symbol]);
        if(symbol == 0 && tokens[token][symbol] == ' '){
            continue;
        }

        // Right now this ignores the token. Should be an error either way.
        if(symbol >= COMMAND_MAX_LENGTH-1 || token >= COMMAND_MAX_AMOUNT){
            cli_newline();
            cli_print("Invalid command");

            // This should flush the ring buffer to avoid relics in next message
            ring_buffer_flush(ring_buffer);
            return;
        }

        if(tokens[token][symbol] == ' ' || ring_buffer_empty(ring_buffer)){
            // token is complete
            if(ring_buffer_empty(ring_buffer)){
                tokens[token][symbol+1] = '\0';
            }else{
                tokens[token][symbol] = '\0';
            }
            symbol = 0;
            token++;
        }
        else{
            symbol++;
        }
    }

    *token_length = token;
}

void cli_parse_command(command_t tokens, char token_length)
{
    if(token_length < 1){
	return;
    }

    char* args = (token_length > 1) ? tokens[1] : 0;

    for(int i = 0; command_table[i].command != 0; i++){
	if(utils_strings_match(tokens[0], command_table[i].command)){
            cli_newline();
	    command_table[i].handler(args);
	    return;
	}
    }
    cli_newline();
    cli_print("Invalid command.");
}

__attribute__((weak)) bool cli_parse_application_command(command_t tokens, char token_length)
{
    return false;
}

void cli_restart(const char *args)
{
    restart_handler();
}

void cli_memdump_hex(const char *args)
{
    if(args[0] != '0' || args[1] != 'x'){
        cli_print("Invalid memory format (0xAABBCCDD)");
        return;
    }

    uint32_t dec = utils_hexstring_to_dec((uint8_t *)&args[2]);

    cli_dump_hex_from_address(dec);
}

void cli_memdump_bin(const char *args)
{
    if(args[0] != '0' || args[1] != 'x'){
        cli_print("Invalid memory format (0xAABBCCDD)");
        return;
    }

    uint32_t dec = utils_hexstring_to_dec((uint8_t *)&args[2]);

    cli_dump_bin_from_address(dec);
}

void cli_print_help(const char *args)
{
    cli_print_library_help();
    cli_print_application_help();
}

void cli_print_library_help()
{
    cli_newline();
    cli_print("-------------------------------------------");
    cli_newline();
    cli_newline();
    cli_print("**********************");
    cli_newline();
    cli_print("** Default commands **");
    cli_newline();
    cli_print("**********************");
    cli_newline();
    cli_newline();
    cli_print("rs            - Restarts the program");
    cli_newline();
    cli_newline();
    cli_print("memdump       - Prints the memory at specified address");
    cli_newline();
    cli_print("                Example: memdump 0x50000000");
    cli_newline();
    cli_print("-------------------------------------------");
    cli_newline();
    cli_newline();
}

__attribute__((weak)) void cli_print_application_help();

void cli_dump_hex_from_address(uint32_t address)
{
    uint8_t hex[9] = {0U};

    // TODO: Implement is_valid_address properly
    /*if(memorymap_is_valid_address(address)){*/
        usart_send_byte(USART2, '0');
        usart_send_byte(USART2, 'x');

        if(utils_dec_to_hexstring(M32(address), hex)){
            usart_send_bytes(USART2, hex, 8);
        }
    /*}else{*/
    /*    cmd_print("Address outside of mapped memory");*/
    /*}*/
}

void cli_dump_bin_from_address(uint32_t address)
{
    uint8_t bin[33] = {48};

    /*if(memorymap_is_valid_address(address)){*/
        if(utils_dec_to_binarystring(M32(address),(char*) bin, 32)){
            cli_print(" -----------------------------------------------");
            cli_newline();
            for(int i = 31; i >= 16; i--){
                if(i == 31){
                    cli_print("|");
                }
                cli_print_number(i);
                cli_print("|");
            }
            cli_newline();
            for(int i = 31; i >= 16; i--){
                if(i == 31){
                    cli_print("| ");
                }
                usart_send_byte(USART2, bin[31 - i]);
                cli_print("| ");
            }
            cli_newline();
            cli_print(" -----------------------------------------------");
            cli_newline();
            for(int i = 15; i >= 0; i--){
                if(i == 15){
                    cli_print("|");
                }
                cli_print_number(i);
                if(i < 11){
                    cli_print("| ");
                }else{
                    cli_print("|");
                }
            }
            cli_newline();
            for(int i = 15; i >= 0; i--){
                if(i == 15){
                    cli_print("| ");
                }
                usart_send_byte(USART2, bin[31 - i]);
                cli_print("| ");
            }
            cli_newline();
            cli_print(" -----------------------------------------------");
        }
    /*}else{*/
    /*    cmd_print("Address outside of mapped memory");*/
    /*}*/
}

void cli_process_input(void){
    // If we received new data from USART
    uint8_t byte;
    while(!ring_buffer_empty(&ring_buffer_temp)){
	if(ring_buffer_read(&ring_buffer_temp, &byte)){
	    switch(usart_state){

		case USART_STATE_IDLE:
		    if(byte == 27){
			usart_state = USART_STATE_ESC;
		    }
		    else if(byte == '\r'){
			// Parse all data and respond
			command_t tokens = {0U};
			char token_length;
			cli_tokenize(&ring_buffer_data, tokens, &token_length);

			// TODO:Do not save repeated commands seperately

			if(tokens[0][0]){ // Not a blank enter press
			    /*utils_command_cpy(command_history[command_index], tokens);*/

			    if(!cli_parse_application_command(tokens, token_length)){
				// Only parse default defaults if commands was not found in application implementation
				cli_parse_command(tokens, token_length);
			    }
			}
			cli_newline();
			cli_print("> ");
		    }else if(byte == 127){
			cli_backspace();
		    }else if(byte == '\t'){
			// Ignore tabs
		    }else{
			// Save byte
			if(!ring_buffer_write(&ring_buffer_data, byte)){
			    ring_buffer_flush(&ring_buffer_data);
			    return;
			}
			// Mirror character to console
			usart_send_byte(USART2, byte);
		    }
		break;

		case USART_STATE_ESC:
		    if(byte == '['){
			usart_state = USART_STATE_BRACKET;
		    } else {
			usart_state = USART_STATE_IDLE;
		    }
		break;

		case USART_STATE_BRACKET:
		    usart_state = USART_STATE_IDLE;

		    switch(byte){
			case 'A':
			    if(command_index >= 0){
				uint8_t len = utils_strlen(command_history[command_index][0]);
				for(int i = 0; i < len; i++){
				    cli_backspace();
				}
				cli_print(command_history[command_index][0]);

				/*if(command_history[command_index][1] != 0){*/
				/*    cli_print(" ");*/
				/*    cli_print(command_history[command_index][1]);*/
				/*}*/

				for(int i = 0; command_history[command_index][0][i]; i++){
				    if(!ring_buffer_write(&ring_buffer_data, command_history[command_index][0][i])){
					ring_buffer_flush(&ring_buffer_data);
					return;
					}
				    }
			    }
			    // Up arrow, Previous command
			break;
			case 'B':
			    // Down arrow, Next command
			break;
			case 'C':
			    // Right arrow, no functionality for now
			break;
			case 'D':
			    // Left arrow, no functionality for now
			break;
		    }
		break;
	    }
	}
    }
}

void USART2_LPUART2_IRQHandler(void) {
    // Checking if interupt is due to data being recieved
    if(USART2->ISR & USART_ISR_RXNE){

	// Reading the byte
	uint8_t byte = USART2->RDR;

	// If the overrun flag is set, for now just clear it
	if(USART2->ISR & USART_ISR_ORE){
	    USART2->ICR |= USART_ISR_ORE;
	    return;
	}
	
	ring_buffer_write(&ring_buffer_temp, byte);
    }
}
