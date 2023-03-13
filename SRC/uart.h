#include <stdint.h>

void uart_send(uint8_t *data, uint8_t size);
void uart_send1(uint8_t data);
void uart_send_event(uint16_t event);
