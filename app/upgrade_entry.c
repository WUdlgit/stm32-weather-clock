#include "upgrade_entry.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"
#include <stdio.h>
#include <stdbool.h>

/* Shared contract with Bootloader: RTC BKP19R, consumed once after reset. */
#define BOOT_REQUEST_MAGIC 0x49504155U
#define HOLD_TIME_MS 2000U

static void request_upgrade(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    taskENTER_CRITICAL();
    uint32_t previous = PWR->CR & PWR_CR_DBP;
    PWR->CR |= PWR_CR_DBP;
    (void)PWR->CR;
    __DSB();
    RTC->BKP19R = BOOT_REQUEST_MAGIC;
    uint32_t stored = RTC->BKP19R;
    __DSB();
    if (!previous)
        PWR->CR &= ~PWR_CR_DBP;
    taskEXIT_CRITICAL();
    if (stored != BOOT_REQUEST_MAGIC)
    {
        printf("[UPGRADE] request storage failed\r\n");
        return;
    }
    printf("[UPGRADE] KEY2 held; resetting into Bootloader\r\n");
    vTaskDelay(pdMS_TO_TICKS(30));
    NVIC_SystemReset();
}

static void upgrade_key_task(void *argument)
{
    (void)argument;
    bool pressed = false;
    bool handled = false;
    TickType_t start = 0;
    for (;;)
    {
        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) == Bit_RESET)
        {
            if (!pressed)
            {
                pressed = true;
                start = xTaskGetTickCount();
            }
            else if (!handled && (TickType_t)(xTaskGetTickCount() - start) >= pdMS_TO_TICKS(HOLD_TIME_MS))
            {
                handled = true;
                request_upgrade();
            }
        }
        else
        {
            pressed = false;
            handled = false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void upgrade_entry_init(void)
{
    GPIO_InitTypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &gpio);
    BaseType_t created = xTaskCreate(upgrade_key_task, "upgrade_key", 256, NULL, 2, NULL);
    configASSERT(created == pdPASS);
}
