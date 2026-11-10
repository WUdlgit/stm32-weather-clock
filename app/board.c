#include "board.h"

#include <stdio.h>

#include "console.h"
#include "rtc.h"
#include "key.h"
#include "led.h"
#include "led_desc.h"
#include "st7789.h"
#include "ui.h"
#include "aht20.h"
#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"


void board_lowlevel_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
}

void board_init(void)
{
    led_init(led1);
    led_init(led2);
    led_init(led3);
    console_init();

    if (!rtc_init())
        printf("[RTC] init failed\r\n");

    if (!aht20_init())
        printf("[AHT20] init failed\r\n");

    printf("[SYS] Build Date: %s %s\n", __DATE__, __TIME__);
}

int fputc(int c, FILE *stream)
{
    while (!USART_GetFlagStatus(USART1, USART_FLAG_TXE))
        ;
    USART_SendData(USART1, c);
    return c;
}

/* GCC(newlib-nano) 的 printf 走 _write，而不是 fputc；不实现它串口就没输出 */
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        while (!USART_GetFlagStatus(USART1, USART_FLAG_TXE))
            ;
        USART_SendData(USART1, (uint8_t)ptr[i]);
    }
    return len;
}

static struct led_desc _led1 = {GPIOB, GPIO_Pin_0, Bit_RESET, Bit_SET};
static struct led_desc _led2 = {GPIOB, GPIO_Pin_1, Bit_RESET, Bit_SET};
static struct led_desc _led3 = {GPIOE, GPIO_Pin_9, Bit_RESET, Bit_SET};
led_desc_t led1 = &_led1;
led_desc_t led2 = &_led2;
led_desc_t led3 = &_led3;
