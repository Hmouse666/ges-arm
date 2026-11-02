#ifndef __TKKC_ST7735_H_
#define __TKKC_ST7735_H_

#include <stdint.h>
#include "tkkc_font.h"

#define ST7735_LCD_WIDE                 (128)                   //屏幕宽
#define ST7735_LCD_HIGH                 (160)                   //屏幕高

enum LCD_DISPLAY_DIR {
    LCD_DISPLAY_NORMAL,                 //默认状态

    LCD_DISPLAY_RORARE_90,              //旋转90度
    
    LCD_DISPLAY_RORARE_180,             //旋转180度

    LCD_DISPLAY_RORARE_270,             //旋转270度
};

typedef enum LCD_FONT_SIZE {
    LCD_FONT_SIZE_8x16,                 //8x16字体

    LCD_FONT_SIZE_6x8,                  //6x8字体
} LCD_FONT_SIZE;

void lcd_init(enum LCD_DISPLAY_DIR dir, uint8_t x_offset, uint8_t y_offset);

void lcd_FullColor(const uint16_t color);

void lcd_ShowChar(uint8_t x, uint8_t y, const char ch, uint16_t ft_color, uint16_t bg_color, LCD_FONT_SIZE size);

void lcd_ShowString(uint8_t x, uint8_t y, const char ch[], uint16_t ft_color, uint16_t bg_color, LCD_FONT_SIZE size);

void lcd_ShowInt(uint8_t x, uint8_t y, int num, uint8_t len, uint16_t ft_color, uint16_t bg_color, LCD_FONT_SIZE size);

void lcd_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t integer_len, uint8_t fractional_len, uint16_t ft_color, uint16_t bg_color, LCD_FONT_SIZE size);
#endif
