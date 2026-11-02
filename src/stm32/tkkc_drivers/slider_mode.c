/**
  ******************************************************************************
  * @file    slider_mode.c
  * @brief   调速模式的独立状态和屏幕显示
  *
  * 屏幕格式与 TASK1~TASK4 相同: 8x16 字体, 每行 16 像素。
  * 进入调速模式后显示:
  *     >SLIDER:<value>
  * 箭头表示当前模式, 整行使用红色; 未进入时显示黑色 " SLIDER:"。
  ******************************************************************************
  */

#include "slider_mode.h"
#include "fpga_gesture.h"
#include "display.h"

#define SLIDER_ROW_X          0u
#define SLIDER_ROW_Y          144u
#define SLIDER_VALUE_X        64u
#define SLIDER_VALUE_LEN      6u
#define SLIDER_ROW_FONT       LCD_FONT_SIZE_8x16
#define SLIDER_TEXT_COLOR     0x0000u
#define SLIDER_ACTIVE_COLOR   0xF800u
#define SLIDER_BG_COLOR       0xFFFFu
#define SLIDER_SHOW_MS        200u

static uint8_t  s_active;
static uint8_t  s_display_dirty = 1u;
static uint32_t s_value_seq;

void SliderMode_Init(void)
{
    s_active = 0u;
    s_display_dirty = 1u;
    s_value_seq = Gesture_SliderValueSeq();
}

void SliderMode_Enter(void)
{
    s_active = 1u;
    s_display_dirty = 1u;
    s_value_seq = Gesture_SliderValueSeq();
}

void SliderMode_Step(void)
{
    if (Gesture_SliderValueSeq() != s_value_seq)
    {
        s_value_seq = Gesture_SliderValueSeq();
        s_display_dirty = 1u;
    }
}

void SliderMode_Exit(void)
{
    s_active = 0u;
    s_display_dirty = 1u;
}

uint8_t SliderMode_IsActive(void)
{
    return s_active;
}

void SliderMode_Show(void)
{
    static uint32_t last = 0u;
    uint16_t color;
    uint32_t now;

    if (s_display_dirty == 0u)
    {
        return;
    }

    now = HAL_GetTick();
    if ((now - last) < SLIDER_SHOW_MS)
    {
        return;
    }

    color = (s_active != 0u) ? SLIDER_ACTIVE_COLOR : SLIDER_TEXT_COLOR;
    lcd_ShowString(SLIDER_ROW_X, SLIDER_ROW_Y,
                   (s_active != 0u) ? ">SLIDER:" : " SLIDER:",
                   color, SLIDER_BG_COLOR, SLIDER_ROW_FONT);

    if (Gesture_SliderValueValid() != 0u)
    {
        lcd_ShowInt(SLIDER_VALUE_X, SLIDER_ROW_Y, Gesture_SliderValue(),
                    SLIDER_VALUE_LEN,
                    color, SLIDER_BG_COLOR, SLIDER_ROW_FONT);
    }
    else
    {
        lcd_ShowString(SLIDER_VALUE_X, SLIDER_ROW_Y, "    --",
                       color, SLIDER_BG_COLOR, SLIDER_ROW_FONT);
    }

    s_display_dirty = 0u;
    last = now;
}
