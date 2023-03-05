#include "keypad.h"
#include "sysclock.h"
#include "ch32v00x.h"

#define ROW_COUNT 4
static const GPIO_TypeDef *row_port[ROW_COUNT] = {
    GPIOC,
    GPIOC,
    GPIOC,
    GPIOC
};
static const uint8_t row_pin[ROW_COUNT] = {
    GPIO_Pin_1,
    GPIO_Pin_3,
    GPIO_Pin_5,
    GPIO_Pin_7
};
#define COL_COUNT 4
static const GPIO_TypeDef *col_port[COL_COUNT] = {
    GPIOD,
    GPIOC,
    GPIOC,
    GPIOC
};
static const uint8_t col_pin[COL_COUNT] = {
    GPIO_Pin_2,
    GPIO_Pin_6,
    GPIO_Pin_4,
    GPIO_Pin_2
};
#define DEBOUNCE_TIME_MS 10
#define MIN_SCAN_TIME_MS 20
#define MAX_SCAN_TIME_MS 100
#define U8_NOT_FOUND 255

static uint8_t key_status[ROW_COUNT * COL_COUNT] = { 0 };

#define get_bit(x,pos) (!(!(x&(1<<pos))))
#define set0_bit(x, pos) (x&=(~(1<<pos)))
#define set1_bit(x, pos) (x|=(1<<pos))
#define key_code(row,col) (row*COL_COUNT+col)
#define get_key_status(row,col) (!(!(key_status[row*COL_COUNT+col])))
#define set_key_status(row,col,val) (key_status[row*COL_COUNT+col]=val)
#define mk_event(act,key) ((act<<8)|(key&0xFF))

static int32_t pending = -1;
static uint8_t last_row = 0;
static uint8_t last_col = 0;
static uint8_t all_key_released = 1;

static uint8_t get_actived_bit(uint8_t val) {
    int8_t count = 0;
    int8_t pos = -1;
    uint8_t mask = 1;
    for (uint8_t i = 0; i < 8; i++) {
        if (val & mask) {
            count ++;
            pos = i;
        }
        mask <<= 1;
    }
    if (count == 1) {
        return pos;
    } else {
        return U8_NOT_FOUND;
    }
}

static uint8_t find_first_actived_bit(uint8_t val) {
    uint8_t mask = 1;
    for (uint8_t i = 0; i < 8; i++) {
        if (val & mask) {
            return i;
        }
        mask <<= 1;
    }
    return U8_NOT_FOUND;
}

static void write_pin(const GPIO_TypeDef *port, uint8_t pin, BitAction val) {
    GPIO_WriteBit((GPIO_TypeDef *)port, (uint16_t) pin, val);
}

static uint8_t read_pin(const GPIO_TypeDef *port, uint8_t pin) {
    return GPIO_ReadInputDataBit((GPIO_TypeDef *)port, (uint16_t) pin);
}

static void init_output_push_pull(const GPIO_TypeDef *port, uint8_t pin) {
    // open drain output
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = (uint16_t) pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init((GPIO_TypeDef *)port, &GPIO_InitStructure);
}

static void init_input_pull_up(const GPIO_TypeDef *port, uint8_t pin) {
    // pullup input
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = (uint16_t) pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init((GPIO_TypeDef *)port, &GPIO_InitStructure);
}

static uint8_t scan_row() {
    // return bit 1 means pressed
    uint8_t val = 0;
    for (uint8_t i = 0; i < COL_COUNT; i++) {
        init_output_push_pull(col_port[i], col_pin[i]);
        write_pin(col_port[i], col_pin[i], Bit_RESET);
    }
    for (uint8_t i = 0; i < ROW_COUNT; i++) {
        init_input_pull_up(row_port[i], row_pin[i]);
    }
    for (int8_t i = ROW_COUNT - 1; i >= 0; i--) {
        val <<= 1;
        val |= !(read_pin(row_port[i], row_pin[i]));
    }
    return val;
}

static uint8_t scan_col() {
    // return bit 1 means pressed
    uint8_t val = 0;
    for (uint8_t i = 0; i < ROW_COUNT; i++) {
        init_output_push_pull(row_port[i], row_pin[i]);
        write_pin(row_port[i], row_pin[i], Bit_RESET);
    }
    for (uint8_t i = 0; i < COL_COUNT; i++) {
        init_input_pull_up(col_port[i], col_pin[i]);
    }
    for (int8_t i = COL_COUNT - 1; i >= 0; i--) {
        val <<= 1;
        val |= !(read_pin(col_port[i], col_pin[i]));
    }
    return val;
}


void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI7_0_IRQHandler(void)
{
    // printf("EXTI Wake_up\r\n");
    EXTI_ClearITPendingBit(
        EXTI_Line0 |
        EXTI_Line1 |
        EXTI_Line2 |
        EXTI_Line3 |
        EXTI_Line4 |
        EXTI_Line5 |
        EXTI_Line6 |
        EXTI_Line7
    );
}

void kbd_init(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource1);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource3);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource5);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource7);
}

void kbd_try_sleep_until_next_key_press(void) {
    int32_t awu_value = 0;
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    if (pending < 0 && all_key_released) {
        // long sleep
        awu_value = MAX_SCAN_TIME_MS;
        /*所有的 GPIO 口都可以被配置外部中断输入通道,
        但一个外部中断输入通道最多只能映射到一个
        GPIO 引脚上,且外部中断通道的序号必须和 GPIO 端口的位号一致,比如 PA1(或 PC1、PD1 等)只
        能映射到 EXTI1 上, 且 EXTI1 只能接受 PA1、 PC1 或 PD1 等其中之一的映射, 两方都是一对一的关系。*/
        // interupt on ROW
        for (uint8_t i = 0; i < COL_COUNT; i++) {
            init_output_push_pull(col_port[i], col_pin[i]);
            write_pin(col_port[i], col_pin[i], Bit_RESET);
        }
        for (uint8_t i = 0; i < ROW_COUNT; i++) {
            init_input_pull_up(row_port[i], row_pin[i]);
        }
        EXTI_InitStructure.EXTI_Line = EXTI_Line1 | EXTI_Line3 | EXTI_Line5 | EXTI_Line7;
        EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
        EXTI_InitStructure.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStructure);
        NVIC_InitTypeDef NVIC_InitStructure = {0};
        NVIC_InitStructure.NVIC_IRQChannel = EXTI7_0_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
        // pause
        PWR_EnterSTANDBYMode(PWR_STANDBYEntry_WFI);
        USART_Printf_Init(115200);
        // resume
        NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
        NVIC_Init(&NVIC_InitStructure);
    } else {
        awu_value = MIN_SCAN_TIME_MS;
        // short sleep
        EXTI_InitStructure.EXTI_Line = EXTI_Line9;
        EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Event;
        EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
        EXTI_InitStructure.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStructure);
        RCC_LSICmd(ENABLE);
        while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);
        PWR_AWU_SetPrescaler(PWR_AWU_Prescaler_256); // 500hz
        awu_value = awu_value * 500 / 1000;
        awu_value = (awu_value > 0x3F) ? 0x3F : awu_value;
        awu_value = (awu_value < 0) ? 0 : awu_value;
        PWR_AWU_SetWindowValue(awu_value);
        PWR_AutoWakeUpCmd(ENABLE);
        // pause
        PWR_EnterSTANDBYMode(PWR_STANDBYEntry_WFE);
        pending = ticks_add(pending, -awu_value); // skip systick
        USART_Printf_Init(115200);
        // resume
        PWR_AutoWakeUpCmd(DISABLE);
        RCC_LSICmd(DISABLE);
    }
    EXTI_InitStructure.EXTI_LineCmd = DISABLE;
    EXTI_Init(&EXTI_InitStructure);
}

uint16_t kbd_scan(void) {
    int32_t now = ticks_ms();
    if (pending >= 0 && ticks_diff(now, pending) < DEBOUNCE_TIME_MS) {
        return mk_event(kbd_ACTION_NOP, 0);
    }
    uint8_t row_now = scan_row();
    uint8_t col_now = scan_col();
    uint8_t row_chg = row_now ^ last_row;
    uint8_t col_chg = col_now ^ last_col;
    if (row_chg > 0 || col_chg > 0) {
        if (pending < 0) {
            // change dectected
            pending = now;
        } else {
            // try to find the changed key
            if (row_chg == 0) {
                if (get_actived_bit(row_now) != U8_NOT_FOUND) {
                    row_chg = row_now;
                }
            } else if (col_chg == 0) {
                if (get_actived_bit(col_now) != U8_NOT_FOUND) {
                    col_chg = col_now;
                }
            }
            if (row_chg > 0 && col_chg > 0) {
                uint8_t row = find_first_actived_bit(row_chg);
                uint8_t col = find_first_actived_bit(col_chg);
                uint8_t val = get_bit(row_now, row) & get_bit(col_now, col);
                last_row = row_now;
                last_col = col_now;
                all_key_released = 0;
                pending = -1;
                if (val) {
                    // pressed
                    if (!get_key_status(row, col)) {
                        set_key_status(row, col, 1);
                        return mk_event(kbd_ACTION_KEY_DOWN, key_code(row, col));
                    }
                } else {
                    if (get_key_status(row, col)) {
                        set_key_status(row, col, 0);
                        return mk_event(kbd_ACTION_KEY_UP, key_code(row, col));
                    }
                }
            } // can't find out which key changed, just ignore.
        }
    } else if (pending >= 0 && ticks_diff(now, pending) >= MAX_SCAN_TIME_MS) {
        pending = -1;
    }
    if (!all_key_released && row_now == 0 && col_now == 0) {
        // release all keys
        for (uint8_t r = 0; r < ROW_COUNT; r++) {
            for (uint8_t c = 0; c < COL_COUNT; c++) {
                if (get_key_status(r, c)) {
                    set_key_status(r, c, 0);
                    return mk_event(kbd_ACTION_KEY_UP, key_code(r, c));
                }
            }
        }
        all_key_released = 1;
    }
}
