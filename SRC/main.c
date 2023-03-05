#include "debug.h"
#include "keypad.h"
#include "sysclock.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Delay_Init();
    USART_Printf_Init(115200);
    printf("SystemClk:%d\r\n", SystemCoreClock);

    printf("======== Keypad Toggle TEST ========\r\n");
    kbd_init();
    systick_init();

    while(1)
    {
        int32_t t0;
        int32_t t1;
        t0 = ticks_us();
        uint16_t event = kbd_scan();
        t1 = ticks_us();
        // printf("scan time: %d us\n", ticks_diff(t1, t0));
        uint8_t event_type = (event >> 8) & 0xFF;
        uint8_t key_code = event & 0xFF;
        if (event_type != kbd_ACTION_NOP) {
            if (event_type == kbd_ACTION_KEY_DOWN) {
                printf("%hu Pressed.\n", key_code);
            } else {
                printf("%hu Released.\n", key_code);
            }
        }
        kbd_try_sleep_until_next_key_press();
    }
}
