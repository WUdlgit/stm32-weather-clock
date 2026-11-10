#ifndef __UI_H__
#define __UI_H__

#include <stdint.h>

#include "st7789.h"

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t color;
}fill_color_t;

typedef struct
{
    uint16_t x;
    uint16_t y;
    char *str;
    uint16_t color;
    uint16_t bg_color;
    const font_t *font;
}write_string_t;

typedef struct
{
    uint16_t x;
    uint16_t y;
    const image_t *image;
}draw_image_t;

struct ui_message
{
    char action;

    union
    {
        fill_color_t fill_color;
        write_string_t write_string;
        draw_image_t draw_image;
    }data;

};

void ui_init(void);
void ui_fill_color(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void ui_write_string(uint16_t x, uint16_t y, char *str, uint16_t color, uint16_t bg_color, const font_t *font);
void ui_draw_image(uint16_t x, uint16_t y, const image_t *image);

#endif /* __UI_H__ */
