#include "display.h"

static uint16_t ft_color, bg_color;
static LCD_FONT_SIZE font;

void display_init(void)
{
	lcd_init(LCD_DISPLAY_NORMAL, 0, 0);

	ft_color = 0x0000;        //RGB565黑色
	bg_color = 0xffff;        //RGB565白色
	font = LCD_FONT_SIZE_6x8;//字体大小6x8
}

void display_String(uint8_t x, uint8_t y, const char ch[])
{
	lcd_ShowString(x, y, ch, ft_color, bg_color, font);
}
