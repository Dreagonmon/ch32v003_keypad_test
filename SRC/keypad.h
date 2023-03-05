#include <stdint.h>

#define kbd_ACTION_NOP 0
#define kbd_ACTION_KEY_DOWN 1
#define kbd_ACTION_KEY_UP 2


void kbd_init(void);
void kbd_try_sleep_until_next_key_press(void);
uint16_t kbd_scan(void);
