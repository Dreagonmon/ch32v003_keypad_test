#include "ch32v00x.h"
#include "interrupt.h"
#include "keypad.h"

#define MIN_SLEEP_TIME_MS 20
const uint32_t EXTI_Line_All = EXTI_Line0 | EXTI_Line1 | EXTI_Line2 |
    EXTI_Line3 | EXTI_Line4 | EXTI_Line5 | EXTI_Line6 | EXTI_Line7;

void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI7_0_IRQHandler(void)
{
    // printf("EXTI Wake_up\r\n");
    EXTI_ClearITPendingBit(EXTI_Line_All);
}

void int_init(void) {
    /*所有的 GPIO 口都可以被配置外部中断输入通道,
    但一个外部中断输入通道最多只能映射到一个
    GPIO 引脚上,且外部中断通道的序号必须和 GPIO 端口的位号一致,比如 PA1(或 PC1、PD1 等)只
    能映射到 EXTI1 上, 且 EXTI1 只能接受 PA1、 PC1 或 PD1 等其中之一的映射, 两方都是一对一的关系。*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource0);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource3);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource5);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource7);
}


void int_sleep(void) {
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    if (_kbd_can_long_sleep()) {
        // long sleep
        _kbd_config_pins_for_wake_up();
        EXTI_InitStructure.EXTI_Line = EXTI_Line_All;
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
        int32_t awu_value = MIN_SLEEP_TIME_MS;
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
        // pending = ticks_add(pending, -awu_value); // skip systick
        USART_Printf_Init(115200);
        // resume
        PWR_AutoWakeUpCmd(DISABLE);
        RCC_LSICmd(DISABLE);
    }
    EXTI_InitStructure.EXTI_LineCmd = DISABLE;
    EXTI_Init(&EXTI_InitStructure);
}
