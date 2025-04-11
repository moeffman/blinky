// © 2024 Oskar Arnudd

// Firmware headers
#include "irdecoder.h"

// Library headers
#include "rcc.h"
#include "nvic.h"
#include "gpio.h"
#include "tim.h"
#include "exti.h"

static uint8_t command = 0xFF;
static uint32_t bit_times[32] = {0};
static uint8_t bit_time_index = 0;

// TODO: Waste of space and time, make better
static uint8_t ir_to_command[70] = {
    [IR_KP_0] = 0,
    [IR_KP_1] = 1,
    [IR_KP_2] = 2,
    [IR_KP_3] = 3,
    [IR_KP_4] = 4,
    [IR_KP_5] = 5,
    [IR_KP_6] = 6,
    [IR_KP_7] = 7,
    [IR_KP_8] = 8,
    [IR_KP_9] = 9,
    [IR_PW] = 10,
    [IR_CH_UP] = 11,
    [IR_CH_DN] = 12,
    [IR_VL_UP] = 13,
    [IR_VL_DN] = 14,
    [IR_DP_LE] = 15,
    [IR_DP_RI] = 16,
    [IR_DP_UP] = 17,
    [IR_DP_DN] = 18,
    [IR_DP_OK] = 19,
};

static command_callback_t ir_commands[20];

void irdecoder_set_commands(const command_callback_t* commands, uint8_t count)
{
    for(uint8_t i = 0; i < count; i++){
	ir_commands[i] = commands[i];
    }
}

void irdecoder_process(void)
{
    if(command != 0xFF){
	uint8_t i = ir_to_command[command];
	if(ir_commands[i].command){
	    ir_commands[i].command(ir_commands[i].arg);
	}
	command = 0xFF;
    }
}

void irdecoder_init(void)
{
    RCC->IOPENR |= RCC_IO_GPIOD;

    gpio_config_t cfg;
    gpio_config_reset(&cfg);
    cfg.mode = GPIO_MODER_INPUT;
    cfg.pupd = GPIO_PUPDR_PULLUP;
    gpio_set(GPIOD, &cfg, PIN9);

    EXTI->EXTICR3 &= ~EXTI_CR3_EXTI1_MSK;
    EXTI->EXTICR3 |= EXTI_CR3_EXTI1(0x3); // PD9 on EXTI9
    EXTI->RTSR1 |= EXTI_RTSR1_RT9; // Enabling rising edge trigger
    EXTI->FTSR1 |= EXTI_FTSR1_FT9; // Enabling falling edge trigger
    EXTI->IMR1 |= EXTI_IMR1_IM9; // Unmask interrupt
    NVIC->ISER0 = NVIC_EXTI4_15;

    // TIMER
    RCC->APBENR2 |= RCC_APB2_TIM16;
    TIM16->CR1 = 0;
    TIM16->PSC = 0x1F;
    TIM16->ARR = 3500;
    TIM16->DIER |= TIM_DIER_UIE;
    TIM16->EGR |= TIM_EGR_UG;
    TIM16->SR = 0;
    NVIC->ISER0 = NVIC_TIM16_FDCAN_IT0;
}

void irdecoder_deinit(void)
{
    RCC->IOPENR |= RCC_IO_GPIOD;

    gpio_config_t cfg;
    gpio_config_reset(&cfg);
    gpio_set(GPIOD, &cfg, 0xFFFF); // Setting GPIOD to reset state

    EXTI->EXTICR3 &= ~EXTI_CR3_EXTI1_MSK; // Disable PD9 EXTI interrupt
    EXTI->RTSR1 &= ~EXTI_RTSR1_RT9; // Disabling rising edge trigger
    EXTI->FTSR1 &= ~EXTI_FTSR1_FT9; // Disabling falling edge trigger
    EXTI->IMR1 &= ~EXTI_IMR1_IM9; // Mask interrupt
    NVIC->ICER0 = NVIC_EXTI4_15;

    // TIMER
    RCC->APBENR2 |= RCC_APB2_TIM16;
    TIM16->CR1 = 0;
    TIM16->PSC = 0;
    TIM16->ARR = 0xFFFF;
    TIM16->DIER = 0;
    TIM16->EGR = 0;
    TIM16->SR = 0;
    NVIC->ICER0 = NVIC_TIM16_FDCAN_IT0;
}

void EXTI4_15_IRQHandler(void)
{
    if(EXTI->FPR1 & EXTI_FPR1_FPIF9){
	EXTI->FPR1 = EXTI_FPR1_FPIF9;

	uint32_t count = TIM16->CNT;

	if(count > 200 && count < 1000 && bit_time_index < 32){
	    bit_times[bit_time_index++] = count;
	}
    }

    if(EXTI->RPR1 & EXTI_RPR1_RPIF9){
	EXTI->RPR1 = EXTI_RPR1_RPIF9;

	TIM16->CR1 |= TIM_CR1_CEN;
	*(uint16_t*)&TIM16->CNT = 0;
    }
}

void TIM16_FDCAN_IT0_IRQHandler(void)
{
    if(TIM16->SR & TIM_SR_UIF){
	TIM16->SR &= ~TIM_SR_UIF;
	*(uint16_t*)&TIM16->CNT = 0;
	TIM16->CR1 &= ~TIM_CR1_CEN;

	uint32_t msg = 0;
	for(uint8_t i = 0; i < 32; i++){
	    msg |= ((bit_times[i] < 500) ? 0 : 1) << (31 - i);
	}

	for(uint8_t i = 0; i < 32; i++){
	    bit_times[i] = 0;
	}
	bit_time_index = 0;

	uint8_t addr = 0xFF & (msg >> 24);
	if(addr != ADDRESS){
	    command = 0xFF;
	    return;
	}
	uint8_t addr_inv = 0xFF & (msg >> 16);
	uint8_t cmnd = 0xFF & (msg >> 8);
	uint8_t cmnd_inv = 0xFF & msg;

	if(((addr ^ addr_inv) == 0xFF) && ((cmnd ^ cmnd_inv) == 0xFF)){
	    command = 0;
	    for (uint8_t i = 0; i < 8; i++) {
		if (cmnd & (1 << i)) {
		    command |= (1 << (7 - i));
		}
	    }
	}
    }
}
