// © 2024 Oskar Arnudd

// Firmware headers
#include "bt.h"

// Library headers
#include "rcc.h"
#include "nvic.h"
#include "gpio.h"
#include "usart.h"

void bt_init(void)
{
    //
    // GPIO
    //
    if(!(RCC->IOPENR & RCC_IO_GPIOB)){
	RCC->IOPENR |= RCC_IO_GPIOB; // Enable GPIOB RCC clock if not already enabled
    }

    gpio_config_t cfg;
    gpio_config_reset(&cfg);

    cfg.mode = GPIO_MODER_AF;
    cfg.speed = GPIO_OSPEEDR_HIGH;
    cfg.af = GPIO_AF4;

    // Setting PB9->USART3_RX and PB10->USART3_TX
    gpio_set(GPIOB, &cfg, PIN9 | PIN10);

    //
    // USART2
    //
    if(!(RCC->APBENR1 & RCC_APB1_USART3)){
	RCC->APBENR1 |= RCC_APB1_USART3; // Enable USART3 RCC clock if not already enabled
    }

    USART3->CR1 &= ~USART_CR1_UE; // Disable USART while configuring
    USART3->CR1 |= USART_CR1_RE; // Enable receiving
    USART3->CR1 |= USART_CR1_TE; // Enable transmitting
    USART3->CR1 |= USART_CR1_RXNEIE; // Enable interrupt on received data

    // BRR USARTDIV = FREQ/BAUDRATE
    USART3->BRR = 0x683; // 16Mhz / 9600 = ~1667 = 0x683

    USART3->CR1 |= USART_CR1_UE; // Enable USART

    //
    // NVIC
    //
    NVIC->ISER0 = NVIC_USART3_6_LPUART1; // Enable USART3
}

void bt_deinit(void)
{

}

void bt_send_string(char* string)
{
    for(uint32_t i = 0; string[i]; i++){
	bt_send_byte(string[i]);
    }
}

void bt_send_byte(uint8_t byte)
{
    // Wait for USART2 TDR
    while(!(USART3->ISR & USART_ISR_TXE));

    // Respecting TE minimum period
    while(!(USART3->ISR & USART_ISR_TEACK));

    // Writing data to TDR
    USART3->TDR = byte;

    // Waiting for Transmission Complete Flag
    while(!(USART3->ISR & USART_ISR_TC));
}

void USART3_6_LPUART1_IRQHandler(void)
{
    if(USART3->ISR & USART_ISR_RXNE){
        uint8_t data = USART3->RDR;
	bt_send_byte(data);
    }
}
