#include <stdint.h>

#define kbd_ACTION_NOP 0
#define kbd_ACTION_KEY_DOWN 1
#define kbd_ACTION_KEY_UP 2


void kbd_init(void);
uint8_t _kbd_can_long_sleep(void);
void _kbd_config_pins_for_wake_up(void);
uint16_t kbd_scan(void);
