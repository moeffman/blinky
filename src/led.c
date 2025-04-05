// © 2024 Oskar Arnudd

// Firmware headers
#include "led.h"
#include "cli.h"

// Library headers
#include "rcc.h"
#include "nvic.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"

static led_state_t led_state = {0};

static void sn_send_data(uint16_t data);
static void led_binary(uint16_t count);
static void led_wave(uint16_t count);
static void led_alternating(uint16_t count);
static void led_bounce(uint16_t count);
static void led_refresh_speed(void);

// Animations
static const uint16_t wave_pattern[8] = {
    0x0707, 0x0E0E, 0x1C1C, 0x3838, 0x7070, 0xE0E0, 0xC1C1, 0x8383
};

static const uint16_t bounce_pattern[28] = {
    0x3, 0x6, 0xC, 0x18, 0x30, 0x60, 0xC0, 0x180, 0x300,
    0x600, 0xC00, 0x1800, 0x3000, 0x6000, 0xC000, 0x6000,
    0x3000, 0x1800, 0xC00, 0x600, 0x300, 0x180, 0xC0,
    0x60, 0x30, 0x18, 0xC, 0x6
};

void led_init(void)
{
    if(!(RCC->IOPENR & RCC_IO_GPIOB)){
	RCC->IOPENR |= RCC_IO_GPIOB;
    }

    gpio_config_t cfg;
    gpio_config_reset(&cfg);

    cfg.mode = GPIO_MODER_AF;
    cfg.pupd = GPIO_PUPDR_PULLDOWN;
    cfg.type = GPIO_OTYPER_PUSHPULL;
    cfg.af   = GPIO_AF0;
    cfg.speed = GPIO_OSPEEDR_VERYLOW;

    // Setting PB3->SPI1_CLK and PB5->SPI1_MOSI
    gpio_set(GPIOB, &cfg, PIN3 | PIN5);

    cfg.mode = GPIO_MODER_OUTPUT;
    cfg.type = GPIO_OTYPER_PUSHPULL;
    cfg.pupd = GPIO_PUPDR_PULLDOWN;
    cfg.speed = GPIO_OSPEEDR_VERYLOW;

    // Setting PB4->Latch
    gpio_set(GPIOB, &cfg, PIN4);

    if(!(RCC->APBENR2 & RCC_APB2_SPI1)){
	RCC->APBENR2 |= RCC_APB2_SPI1;
    }

    SPI1->CR1 = 0;
    SPI1->CR2 = 0;
    SPI1->CR1 |= SPI_CR1_MSTR | SPI_CR1_LSBFIRST | SPI_CR1_SSI | SPI_CR1_SSM;
    SPI1->CR1 |= SPI_CR1_BR(0x7); // fPCLK/256 = 62500Hz
    SPI1->CR2 |= SPI_CR2_DS(0xF); // 16-bit data size

    SPI1->CR1 |= SPI_CR1_SPE;

    led_state_reset();
    led_reset();

    // Enabling TIM14 clock
    if(!(RCC->APBENR2 & RCC_APB2_TIM14)){
        RCC->APBENR2 |= RCC_APB2_TIM14;
    }

    // Update interrupt enabled
    TIM14->DIER |= TIM_DIER_UIE;

    // Re-initialize counter
    TIM14->EGR |= TIM_EGR_UG;

    // Prescaler set to 15999 (16MHz / 1MHz)-1
    TIM14->PSC = 0x0F;

    // Auto-reload value set to 999, making it fire the interrupt every millisecond
    TIM14->ARR = 999;

    // Counter enabled
    TIM14->CR1 |= TIM_CR1_CEN;

    // Enable TIM14 in NVIC
    NVIC->ISER0 = NVIC_TIM14;
}

void led_deinit(void)
{
    led_reset();

    if(!(RCC->IOPENR & RCC_IO_GPIOB)){
	RCC->IOPENR |= RCC_IO_GPIOB;
    }

    gpio_config_t cfg;
    gpio_config_reset(&cfg);
    gpio_set(GPIOB, &cfg, 0xFFFF);

    if(!(RCC->APBENR2 & RCC_APB2_SPI1)){
	RCC->APBENR2 |= RCC_APB2_SPI1;
    }

    SPI1->CR1 = 0;
    SPI1->CR2 = 0;

    // Enabling TIM14 clock
    if(!(RCC->APBENR2 & RCC_APB2_TIM14)){
        RCC->APBENR2 |= RCC_APB2_TIM14;
    }

    TIM14->DIER &= ~TIM_DIER_UIE;
    TIM14->PSC = 0;
    TIM14->ARR = 0xFFFF;
    TIM14->CR1 &= ~TIM_CR1_CEN;

    // Disable TIM14 in NVIC
    NVIC->ICER0 = NVIC_TIM14;
}

void led_update(void)
{
    if(!led_state.active){
	return;
    }

    if (--led_state.tick > 0){
	return;
    }

    // Resetting tick
    led_state.tick = led_state.speed;

    switch (led_state.pattern) {
	case PATTERN_BINARY:
	    led_binary(led_state.count++);
	    break;
	case PATTERN_WAVE:
	    led_wave(led_state.count++ % 8);
	    break;
	case PATTERN_ALTERNATING:
	    led_alternating(led_state.count++);
	    break;
	case PATTERN_BOUNCE:
	    led_bounce(led_state.count++ % 28);
	    if (led_state.count >= LED_BOUNCE_RESET) {
		led_state.count = 0; // Resetting led counter on a multiple of 11 to avoid
				     // "jumping" when led_counter overflows
	    }
	    break;
	default:
	    cli_printline("Invalid pattern");
	    break;
    }
}
void led_toggle_pattern(const char* args)
{
    led_reset();
    led_state.count = 0;

    switch (led_state.pattern) {
	case PATTERN_BINARY:
	    if(args[0] == '-'){
		led_state.pattern = PATTERN_BOUNCE;
		cli_printline("Changing pattern to: Bounce");
	    }else{
		led_state.pattern = PATTERN_WAVE;
		cli_printline("Changing pattern to: Wave");
	    }
	    break;
	case PATTERN_WAVE:
	    if(args[0] == '-'){
		led_state.pattern = PATTERN_BINARY;
		cli_printline("Changing pattern to: Binary");
	    }else{
		led_state.pattern = PATTERN_ALTERNATING;
		cli_printline("Changing pattern to: Alternating");
	    }
	    break;
	case PATTERN_ALTERNATING:
	    if(args[0] == '-'){
		led_state.pattern = PATTERN_WAVE;
		cli_printline("Changing pattern to: Wave");
	    }else{
		led_state.pattern = PATTERN_BOUNCE;
		cli_printline("Changing pattern to: Bounce");
	    }
	    break;
	case PATTERN_BOUNCE:
	    if(args[0] == '-'){
		led_state.pattern = PATTERN_ALTERNATING;
		cli_printline("Changing pattern to: Alternating");
	    }else{
		led_state.pattern = PATTERN_BINARY;
		cli_printline("Changing pattern to: Binary");
	    }
	    break;
    }
}

void led_set_pattern(const char* args)
{
    if(cli_strings_match(args, "wave") && led_state.pattern != PATTERN_WAVE){
	led_state.pattern = PATTERN_WAVE;
	cli_printline("Changing pattern to: Wave");
    }else if(cli_strings_match(args, "alternating") && led_state.pattern != PATTERN_ALTERNATING){
	led_state.pattern = PATTERN_ALTERNATING;
	cli_printline("Changing pattern to: Alternating");
    }else if(cli_strings_match(args, "bounce") && led_state.pattern != PATTERN_BOUNCE){
	led_state.pattern = PATTERN_BOUNCE;
	cli_printline("Changing pattern to: Bounce");
    }else if(cli_strings_match(args, "binary") && led_state.pattern != PATTERN_BINARY){
	led_state.pattern = PATTERN_BINARY;
	cli_printline("Changing pattern to: Binary");
    }else{
	return;
    }
    led_reset();
    led_state.count = 0;
}

void led_speed_increase(const char* args)
{
    switch (led_state.speed) {
	case SPEED_SLOWER:
	    led_state.speed = SPEED_SLOW;
	    cli_printline("Changing speed to: Slow");
	    break;
	case SPEED_SLOW:
	    led_state.speed = SPEED_NORMAL;
	    cli_printline("Changing speed to: Normal");
	    break;
	case SPEED_NORMAL:
	    led_state.speed = SPEED_FAST;
	    cli_printline("Changing speed to: Fast");
	    break;
	case SPEED_FAST:
	    led_state.speed = SPEED_FASTER;
	    cli_printline("Changing speed to: Fastest");
	    break;
	case SPEED_FASTER:
	    cli_printline("Already at fastest speed..");
	    return;
	    break;
    }
    led_refresh_speed();
}

void led_speed_decrease(const char* args)
{
    switch (led_state.speed){
	case SPEED_SLOWER:
	    cli_printline("Already at slowest speed..");
	    return;
	break;
	case SPEED_SLOW:
	    led_state.speed = SPEED_SLOWER;
	    cli_printline("Changing speed to: Slowest");
	break;
	case SPEED_NORMAL:
	    led_state.speed = SPEED_SLOW;
	    cli_printline("Changing speed to: Slow");
	break;
	case SPEED_FAST:
	    led_state.speed = SPEED_NORMAL;
	    cli_printline("Changing speed to: Normal");
	break;
	case SPEED_FASTER:
	    led_state.speed = SPEED_FAST;
	    cli_printline("Changing speed to: Fast");
	break;
    }
    led_refresh_speed();
}

void led_speed_set(const char* args)
{
    uint32_t speed = cli_string_to_number(args);

    if(speed < 0 || speed > 5){
	cli_print("Speed not in bounds (1 - 5)");
	return;
    }

    switch(speed){
	case 1:
	    led_state.speed = SPEED_SLOWER;
	    cli_printline("Changing speed to: Slowest");
	break;
	case 2:
	    led_state.speed = SPEED_SLOW;
	    cli_printline("Changing speed to: Slow");
	break;
	case 3:
	    led_state.speed = SPEED_NORMAL;
	    cli_printline("Changing speed to: Normal");
	break;
	case 4:
	    led_state.speed = SPEED_FAST;
	    cli_printline("Changing speed to: Fast");
	break;
	case 5:
	    led_state.speed = SPEED_FASTER;
	    cli_printline("Changing speed to: Faster");
	break;
    }
    led_refresh_speed();
}

static void led_refresh_speed(void)
{
    if(led_state.tick > led_state.speed){
	led_state.tick = led_state.speed;
    }
}

void led_stop(const char* args)
{
    cli_print("Stopping.");
    led_state.active = false;
    led_reset();
    cli_newline();
}

void led_start(const char* args)
{
    cli_print("Starting.");
    led_state.active = true;
    cli_newline();
}

void led_toggle(const char* args)
{
    if(led_state.active){
	led_stop(0);
    }else{
	led_start(0);
    }
}

static void led_binary(uint16_t count)
{
    sn_send_data(count);
}

static void led_wave(uint16_t count)
{
    sn_send_data(wave_pattern[count]);
}

static void led_alternating(uint16_t count)
{
    sn_send_data((count % 2) ? 0xF0F0 : 0x0F0F);
}

static void led_bounce(uint16_t count)
{
    sn_send_data(bounce_pattern[count]);
}

void led_reset(void)
{
    led_state.tick = 1;
    sn_send_data(0);
}

static void sn_send_data(uint16_t data)
{
    uint32_t timeout = 100000;
    while (!(SPI1->SR & SPI_SR_TXE) && timeout--) {
        if (!timeout) {
            // SPI not ready
            SPI1->CR1 &= ~SPI_CR1_SPE; // Disable
            SPI1->CR1 |= SPI_CR1_SPE; // Re-enable
            return;
        }
    }
    SPI1->DR = data;
    timeout = 100000;
    while (SPI1->SR & SPI_SR_BSY && timeout--) {
        if (!timeout) return;
    }
    // Pulse latch
    GPIOB->BSRR = BIT4;
    GPIOB->BSRR = BIT4 << 16;
}

void led_state_reset(void)
{
    led_state.pattern = PATTERN_BINARY;
    led_state.speed = SPEED_SLOW;
    led_state.tick = led_state.speed;
    led_state.count = 0;
    led_state.active = true;
}

// This interrupt fires every millisecond
void TIM14_IRQHandler(void)
{
    // Clearing update interrupt flag
    TIM14->SR &= ~TIM_SR_UIF;

    led_update();
}
