#include "debug.h"
#include "uart.h"
#include "keypad.h"
#include "sysclock.h"
#include "interrupt.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Delay_Init();
    USART_Printf_Init(115200);
    printf("## SystemClk:%d\r\n", SystemCoreClock);

    printf("## ======== Keypad CH32V003 ========\r\n");
    systick_init();
    int_init();
    kbd_init();

    uint8_t sleep_disabled = 2;
    int32_t sleep_enable_time = ticks_add(ticks_ms(), 1000);

    while(1)
    {
        uint16_t event = kbd_scan();
        uint8_t event_type = (event >> 8) & 0xFF;
        uint8_t key_code = event & 0xFF;
        if (event_type != kbd_ACTION_NOP) {
            printf("## %X %X\n", event_type, key_code);
            uart_send_event(event);
        }
        if (sleep_disabled > 0) {
            int32_t now = ticks_ms();
            if (ticks_diff(now, sleep_enable_time) >= 0) sleep_disabled = 0;
            if (sleep_disabled == 0) printf("## sleep enabled\n");
        }
        if (!sleep_disabled) {
            // delay enable sleep, allow debugger to attach at boot.
            int_sleep();
        }
    }
}
