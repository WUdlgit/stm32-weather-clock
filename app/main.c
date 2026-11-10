#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "board.h"
#include "console.h"
#include "FreeRTOS.h"
#include "image.h"
#include "led.h"
#include "page.h"
#include "portmacro.h"
#include "st7789.h"
#include "ui.h"
#include "stm32f4xx.h"
#include "task.h"
#include "upgrade_entry.h"


static void main_init(void *param)
{
    board_init();
    ui_init();
    welcome_page_display();
    wifi_init();
    wifi_page_display();
    wifi_wait_connect();
    main_page_display();
    main_loop_init();

    vTaskDelete(NULL);
}

int main(void)
{
    board_lowlevel_init();
    upgrade_entry_init();

    xTaskCreate(main_init, "main_init", 1024, NULL, 9, NULL);

    vTaskStartScheduler();

    while (1)
    {
        ;
    }
}

void vAssertCalled(const char *file, int line)
{
    printf("Assert Called: %s(%d)\n", file, line);
    while (1)
        ;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("Stack Overflowed: %s\n", pcTaskName);
    configASSERT(0);
}

void vApplicationMallocFailedHook(void)
{
    printf("Malloc Failed\n");
    configASSERT(0);
}
