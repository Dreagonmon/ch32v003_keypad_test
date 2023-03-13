#include "uart.h"
#include "ch32v00x.h"

#define EVENT_START        0xFF
#define EVENT_NOP          0xFF
#define EVENT_END          0xFE

void uart_send(uint8_t *data, uint8_t size) {
    while ((size--) > 0){
        while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
        USART_SendData(USART1, *(data++));
    }
}

void uart_send1(uint8_t data) {
    while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
    USART_SendData(USART1, data);
}

void uart_send_event(uint16_t event) {
    uart_send1(EVENT_START);
    uart_send1(EVENT_NOP);
    uart_send1((event >> 8) & 0xFF);
    uart_send1(event & 0xFF);
    uart_send1(EVENT_END);
    uart_send1('\n');
}
