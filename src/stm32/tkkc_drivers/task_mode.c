/**
  ******************************************************************************
  * @file    task_mode.c
  * @brief   FPGA 任务标志解析、任务模式切换和屏幕显示
  *
  *  任务帧格式:
  *    [TASK1:0, TASK2:0, TASK3:0]
  *
  *  规则:
  *    1) 上电默认模式 0, 保持现有手势控制行为;
  *    2) 只在 TASKx 的 0 -> 1 上升沿产生切换请求;
  *    3) 第一帧有效任务数据只建立基准, 不触发切换;
 *    4) TASKx 保持 1 不重复切换; TASK1 从1变0时, 如果当前在任务1,
 *       自动回到任务2循环, 没有手势识别时继续往返动作;
  *    5) 多个任务同时上升时, 固定优先级 TASK1 > TASK2 > TASK3;
  *    6) 收到 manul ready! / manual ready! 进入任务 4, TASK1/2/3 上升时退出任务 4。
  *    7) 收到 [SLIDER] 进入独立的调速模式。
  *
 *  任务2是自动往返任务, 进入后立即执行。其它使用手势的任务在 Enter 完成后,
 *  再等一帧新的手势数据, 然后才允许 Step 使用手势控制。
  ******************************************************************************
  */

#include "task_mode.h"
#include "fpga_gesture.h"
#include "display.h"
#include <string.h>

#define TASK_FLAG_1             0x01u
#define TASK_FLAG_2             0x02u
#define TASK_FLAG_3             0x04u

#define TASK_TIMEOUT_MS         1000u   /* 超过这么久没任务帧 -> 重连后重新取基准 */
#define TASK_SHOW_MS            200u    /* 屏幕刷新间隔 */

/* 全屏统一使用 8x16, 每行间隔 16 像素。 */
#define TASK_Y_1                80u
#define TASK_ROW_STEP           16u
#define TASK_ROW_COUNT          4u
#define TASK_ROW_FONT           LCD_FONT_SIZE_8x16
#define TASK_CLEAR_ROWS         5u      /* 80~159, 清掉旧字号留下的残留 */

static uint8_t  s_flags;                 /* 最近一次有效任务标志 */
static uint8_t  s_prev_flags;            /* 上一帧任务标志, 用来找上升沿 */
static uint8_t  s_baseline;              /* 1 = 已经取过第一帧基准 */
static uint8_t  s_flags_valid;           /* 1 = 收到过有效任务帧 */
static uint8_t  s_link_lost;             /* 1 = 任务帧超时, 屏幕保持最后的 0/1 */

static uint8_t  s_pending = TASK_MODE_NONE;
static uint8_t  s_mode = TASK_MODE_DEFAULT;
static uint8_t  s_use_gesture = 1u;
static uint8_t  s_wait_gesture;
static uint8_t  s_gesture_ready = 1u;
static uint32_t s_gesture_state_seq;
static uint32_t s_last_rx_tick;
static uint8_t  s_display_dirty = 1u;
static uint8_t  s_display_clear = 1u;

/* 读 TASK1/TASK2/TASK3 的值, 必须是一个单独的 0/1, 后面只能跟分隔符。 */
static uint8_t TaskMode_ParseFlag(const char *line, const char *key, uint8_t *out)
{
    const char *p;
    char ch;

    p = strstr(line, key);
    if (p == 0)
    {
        return 0;
    }

    p += strlen(key);
    while ((*p == ' ') || (*p == '\t'))
    {
        p++;
    }

    if ((*p != '0') && (*p != '1'))
    {
        return 0;
    }

    *out = (uint8_t)(*p - '0');
    p++;

    while ((*p == ' ') || (*p == '\t'))
    {
        p++;
    }

    ch = *p;
    if ((ch != ',') && (ch != ']') && (ch != '\0') &&
        (ch != '\r') && (ch != '\n'))
    {
        return 0;                           /* 例如 TASK1:10, 不认 */
    }

    return 1;
}

/* 不区分大小写地在一行里找 ASCII 关键字。 */
static char TaskMode_ToLower(char c)
{
    if ((c >= 'A') && (c <= 'Z'))
    {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static uint8_t TaskMode_ContainsNoCase(const char *line, const char *key)
{
    const char *p;
    const char *q;

    if ((line == 0) || (key == 0) || (*key == '\0'))
    {
        return 0u;
    }

    while (*line != '\0')
    {
        p = line;
        q = key;

        while ((*p != '\0') && (*q != '\0') &&
               (TaskMode_ToLower(*p) == *q))
        {
            p++;
            q++;
        }

        if (*q == '\0')
        {
            return 1u;
        }

        line++;
    }

    return 0u;
}

static uint8_t TaskMode_IsManualReady(const char *line)
{
    if ((TaskMode_ContainsNoCase(line, "manul ready") != 0u) ||
        (TaskMode_ContainsNoCase(line, "manual ready") != 0u))
    {
        return 1u;
    }

    return 0u;
}

uint8_t TaskMode_IsControlFrame(const char *line)
{
    if (line == 0)
    {
        return 0u;
    }

    if (TaskMode_IsManualReady(line) != 0u)
    {
        return 1u;
    }

    if (strstr(line, "[SLIDER]") != 0)
    {
        return 1u;
    }

    if ((strstr(line, "TASK1:") != 0) ||
        (strstr(line, "TASK2:") != 0) ||
        (strstr(line, "TASK3:") != 0))
    {
        return 1u;
    }

    return 0u;
}

void TaskMode_Init(void)
{
    s_flags        = 0u;
    s_prev_flags   = 0u;
    s_baseline     = 0u;
    s_flags_valid  = 0u;
    s_link_lost    = 0u;
    s_pending      = TASK_MODE_NONE;
    s_mode         = TASK_MODE_DEFAULT;
    s_use_gesture  = 1u;
    s_wait_gesture = 0u;
    s_gesture_ready = 1u;                   /* 默认模式保持现在的行为 */
    s_gesture_state_seq = Gesture_StateSeq();
    s_last_rx_tick = HAL_GetTick();
    s_display_dirty = 1u;
    s_display_clear = 1u;
}

void TaskMode_OnFrame(const char *line)
{
    uint8_t t1;
    uint8_t t2;
    uint8_t t3;
    uint8_t has_task;
    uint8_t has_manual;
    uint8_t has_slider;
    uint8_t now;
    uint8_t rise;
    uint8_t fall;
    uint8_t req;
    uint8_t old_flags;
    uint8_t was_lost;
    uint8_t was_valid;

    if (line == 0)
    {
        return;
    }

    has_manual = TaskMode_IsManualReady(line);
    has_slider = (strstr(line, "[SLIDER]") != 0) ? 1u : 0u;

    /* TASK1/2/3 三个字段必须同时存在且都合法, 否则整帧任务标志丢弃。 */
    has_task = ((TaskMode_ParseFlag(line, "TASK1:", &t1) != 0) &&
                (TaskMode_ParseFlag(line, "TASK2:", &t2) != 0) &&
                (TaskMode_ParseFlag(line, "TASK3:", &t3) != 0)) ? 1u : 0u;

    if ((has_task == 0u) && (has_manual == 0u) && (has_slider == 0u))
    {
        return;
    }

    /* 如果 manual ready 先于第一帧 TASK1/2/3 到达, 就把任务标志基准设为全 0。
     * 这样之后 TASK1/2/3 第一次 0 -> 1 仍能正常退出任务 4。 */
    if ((has_manual != 0u) && (s_baseline == 0u))
    {
        s_prev_flags = 0u;
        s_baseline   = 1u;
    }

    req = TASK_MODE_NONE;

    if (has_task != 0u)
    {
        now = 0u;
        if (t1 != 0u)
        {
            now |= TASK_FLAG_1;
        }
        if (t2 != 0u)
        {
            now |= TASK_FLAG_2;
        }
        if (t3 != 0u)
        {
            now |= TASK_FLAG_3;
        }

        old_flags = s_flags;
        was_lost  = s_link_lost;
        was_valid = s_flags_valid;

        s_flags        = now;
        s_flags_valid  = 1u;
        s_link_lost    = 0u;
        s_last_rx_tick = HAL_GetTick();

        if ((old_flags != now) || (was_lost != 0u) || (was_valid == 0u))
        {
            s_display_dirty = 1u;
        }

        /* 上电第一帧只做基准, 不切模式。 */
        if (s_baseline == 0u)
        {
            s_prev_flags = now;
            s_baseline   = 1u;
        }
        else
        {
            rise = (uint8_t)(now & (uint8_t)(~s_prev_flags));
            fall = (uint8_t)(s_prev_flags & (uint8_t)(~now));

            if (rise != 0u)
            {
                /* 多个任务同时上升时固定优先 TASK1 > TASK2 > TASK3。 */
                if ((rise & TASK_FLAG_1) != 0u)
                {
                    req = TASK_MODE_1;
                }
                else if ((rise & TASK_FLAG_2) != 0u)
                {
                    req = TASK_MODE_2;
                }
                else
                {
                    req = TASK_MODE_3;
                }

                /* 已经在任务 N 里时, 重复 0 -> 1 不重新 Enter。 */
                if (req == s_mode)
                {
                    req = TASK_MODE_NONE;
                }
            }
            else if (((fall & TASK_FLAG_1) != 0u) &&
                     (s_mode == TASK_MODE_1))
            {
                /* 手势任务标志消失: 自动回到任务2, 继续电机循环。 */
                req = TASK_MODE_2;
            }

            s_prev_flags = now;
        }
    }

    if (req != TASK_MODE_NONE)
    {
        /* 其他任务优先于 manual ready: 识别到 TASKx 时退出任务 4。 */
        s_pending       = req;
        s_gesture_ready = 0u;
    }
    else if ((has_manual != 0u) && (s_mode != TASK_MODE_4))
    {
        s_pending       = TASK_MODE_4;
        s_gesture_ready = 0u;
    }
    else if ((has_slider != 0u) && (s_mode != TASK_MODE_SLIDER))
    {
        s_pending       = TASK_MODE_SLIDER;
        s_gesture_ready = 0u;
    }

    if ((has_manual != 0u) || (has_slider != 0u))
    {
        s_last_rx_tick = HAL_GetTick();
    }
}

uint8_t TaskMode_TakeRequest(void)
{
    uint8_t req = s_pending;

    if (req == TASK_MODE_NONE)
    {
        return TASK_MODE_NONE;
    }

    s_pending = TASK_MODE_NONE;
    s_gesture_ready = 0u;
    return req;
}

void TaskMode_SetMode(uint8_t mode)
{
    s_mode = mode;
    s_display_dirty = 1u;
}

uint8_t TaskMode_Current(void)
{
    return s_mode;
}

void TaskMode_EnterDone(void)
{
    if (s_use_gesture != 0u)
    {
        Gesture_ResetValid();
        s_gesture_state_seq = Gesture_StateSeq();
        s_wait_gesture  = 1u;
        s_gesture_ready = 0u;
    }
    else
    {
        s_wait_gesture  = 0u;
        s_gesture_ready = 1u;
    }
}

void TaskMode_Poll(void)
{
    /* 任务帧断线: 保持当前模式, 下次恢复时重新取基准, 避免误切换。 */
    if ((s_flags_valid != 0u) &&
        ((HAL_GetTick() - s_last_rx_tick) >= TASK_TIMEOUT_MS))
    {
        s_flags_valid = 0u;
        s_link_lost   = 1u;
        s_baseline    = 0u;
        s_display_dirty = 1u;
    }

    /* 进入新任务后, 必须等一帧新的手势数据才算切换完成。 */
    if (s_wait_gesture != 0u)
    {
        if ((Gesture_StateSeq() != s_gesture_state_seq) &&
            (Gesture_IsValid() != 0u))
        {
            s_wait_gesture  = 0u;
            s_gesture_ready = 1u;
        }
    }
}

uint8_t TaskMode_GestureReady(void)
{
    return s_gesture_ready;
}

void TaskMode_SetUseGesture(uint8_t use)
{
    s_use_gesture = (use != 0u) ? 1u : 0u;

    if (s_use_gesture == 0u)
    {
        s_wait_gesture  = 0u;
        s_gesture_ready = 1u;
    }
}

static void TaskMode_MakeLine(uint8_t index, char *line)
{
    uint8_t flag;

    if (index < 3u)
    {
        flag = (uint8_t)((s_flags >> index) & 0x01u);
    }
    else
    {
        /* TASK4 没有 FPGA 的 0/1 位, 用“当前是否处于任务 4”显示。 */
        flag = (s_mode == TASK_MODE_4) ? 1u : 0u;
    }

    line[0] = (s_mode == (uint8_t)(index + 1u)) ? '>' : ' ';
    line[1] = 'T';
    line[2] = 'A';
    line[3] = 'S';
    line[4] = 'K';
    line[5] = (char)('1' + index);
    line[6] = ':';
    line[7] = (char)('0' + flag);
    line[8] = ' ';
    line[9] = ' ';
    line[10] = '\0';
}

void TaskMode_Show(void)
{
    static uint32_t last = 0u;
    char line[11];
    uint16_t color;
    uint32_t now;
    uint8_t i;
    uint8_t y;

    if (s_display_dirty == 0u)
    {
        return;
    }

    now = HAL_GetTick();
    if ((now - last) < TASK_SHOW_MS)
    {
        return;
    }

    /* 第一次显示前把任务区域整块清成白色。
     * 这样即使之前烧录过 8x16 的版本, 也不会留下大小不一致的旧像素。 */
    if (s_display_clear != 0u)
    {
        for (i = 0u; i < TASK_CLEAR_ROWS; i++)
        {
            lcd_ShowString(0u, (uint8_t)(TASK_Y_1 + (i * TASK_ROW_STEP)),
                           "                ", 0x0000u, 0xFFFFu, TASK_ROW_FONT);
        }
        s_display_clear = 0u;
    }

    /* TASK1~TASK4 用完全相同的字体、起点和行距。 */
    for (i = 0u; i < TASK_ROW_COUNT; i++)
    {
        TaskMode_MakeLine(i, line);
        color = (s_mode == (uint8_t)(i + 1u)) ? 0xF800u : 0x0000u;
        y = (uint8_t)(TASK_Y_1 + (i * TASK_ROW_STEP));
        lcd_ShowString(0u, y, line, color, 0xFFFFu, TASK_ROW_FONT);
    }

    s_display_dirty = 0u;
    last = now;
}
