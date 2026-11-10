#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "st7789.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "stm32f4xx.h"

static QueueHandle_t ui_message_t;

static void ui_func(void *param);

void ui_init(void)
{
    ui_message_t = xQueueCreate(10, sizeof(struct ui_message));
    xTaskCreate(ui_func, "ui_func", 1024, NULL, 9, NULL);
}

void ui_fill_color(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    struct ui_message ui_message;

    ui_message.action = 1;

    ui_message.data.fill_color.x = x;
    ui_message.data.fill_color.y = y;
    ui_message.data.fill_color.width = width;
    ui_message.data.fill_color.height = height;
    ui_message.data.fill_color.color = color;

    xQueueSend(ui_message_t, (void *)&ui_message, portMAX_DELAY);
}

void ui_write_string(uint16_t x, uint16_t y, char *str, uint16_t color, uint16_t bg_color, const font_t *font)
{
    struct ui_message ui_message;

    ui_message.action = 2;

    ui_message.data.write_string.x = x;
    ui_message.data.write_string.y = y;
    ui_message.data.write_string.color = color;
    ui_message.data.write_string.bg_color = bg_color;
    ui_message.data.write_string.font = font;

    ui_message.data.write_string.str = pvPortMalloc(strlen(str) + 1);
    if (ui_message.data.write_string.str == NULL)
    {
        return;
    }

    for (int i = 0; i < strlen(str) + 1; i++)
    {
        ui_message.data.write_string.str[i] = str[i];
    }

    if (xQueueSend(ui_message_t, (void *)&ui_message, portMAX_DELAY) != pdPASS)
    {
        vPortFree(ui_message.data.write_string.str);
    }
}

void ui_draw_image(uint16_t x, uint16_t y, const image_t *image)
{
    struct ui_message ui_message;

    ui_message.action = 3;

    ui_message.data.draw_image.x = x;
    ui_message.data.draw_image.y = y;
    ui_message.data.draw_image.image = image;

    xQueueSend(ui_message_t, (void *)&ui_message, portMAX_DELAY);
}

static void ui_func(void *param)
{
    st7789_init();

    struct ui_message ui_message;

    while (1)
    {
        if (xQueueReceive(ui_message_t, (void *)&ui_message, portMAX_DELAY))
        {
            switch (ui_message.action)
            {
                case 1:
                    st7789_fill_color(ui_message.data.fill_color.x, ui_message.data.fill_color.y, ui_message.data.fill_color.width, ui_message.data.fill_color.height, ui_message.data.fill_color.color);
                    break;
                case 2:
                    st7789_write_string(ui_message.data.write_string.x, ui_message.data.write_string.y, ui_message.data.write_string.str, ui_message.data.write_string.color, ui_message.data.write_string.bg_color, ui_message.data.write_string.font);

                    vPortFree(ui_message.data.write_string.str);
                    break;
                case 3:
                    st7789_draw_image(ui_message.data.draw_image.x, ui_message.data.draw_image.y, ui_message.data.draw_image.image);
                    break;
            }
        }
    }
}
