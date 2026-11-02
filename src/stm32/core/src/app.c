/**
  ******************************************************************************
  * @file    APP.c
  * @brief   应用初始化、主循环调度、屏幕显示和指示灯。
  *
  *  接线:
  *    FPGA 串口 : PB11 = USART3_RX, PB10 = USART3_TX, 115200 8N1
  *    屏幕      : SCL=PB13  SDA=PB15  CS=PA11  DC=PB14  RES=PA8
  *    PCA9685   : SCL=PA6   SDA=PA7(软件 I2C), 爪子接第 2 路
  *    电机1     : PUL=PA0   DIR=PA1   EN=PA2
  *    电机2     : PUL=PB0   DIR=PB1   EN=PA3
  *    电机3     : PUL=PB6   DIR=PB8   EN=PB9
  *    电机4     : PUL=PA12  DIR=PB3   EN=PB5
  ******************************************************************************
  */

#include "main.h"
#include "usart.h"
#include "key.h"
#include "led.h"
#include "display.h"
#include "fpga_gesture.h"
#include "gesture_motor.h"
#include "claw_servo.h"
#include "task_mode.h"
#include "task1.h"
#include "task2.h"
#include "task3.h"
#include "task4.h"
#include "slider_mode.h"

/* ==================== 显示参数 ==================== */

#define SHOW_X0                 0       /* 显示起始列 */
#define SHOW_Y_ANGLE            GEST_Y_ANGLE
#define SHOW_Y_ERR              144     /* 统一 8x16: 最后一行为错误提示 */
#define SHOW_MS                 100     /* 显示刷新间隔(ms) */
#define APP_LCD_ALWAYS          0       /* 1 = 始终刷新显示 */

/* ==================== 屏幕显示 ==================== */

/* 显示舵机角度和错误信息。 */
static void app_show_task(void)
{
    static uint32_t last = 0;

    if ((HAL_GetTick() - last) < SHOW_MS)
    {
        return;
    }
    last = HAL_GetTick();

    lcd_ShowString(SHOW_X0, SHOW_Y_ANGLE, "ANGLE:", 0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);
    lcd_ShowInt(SHOW_X0 + 48, SHOW_Y_ANGLE, ClawServo_GetAngle(), 4,
                0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);

    if ((SliderMode_IsActive() == 0u) &&
        (Gesture_SliderValueValid() == 0u))
    {
        lcd_ShowString(SHOW_X0, SHOW_Y_ERR,
                       (ClawServo_HasError() != 0) ? "PCA9685 ERR!" : "            ",
                       0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);
    }
}

/* ==================== 初始化和主循环 ==================== */

/* 初始化外设、电机和舵机模块。 */
static void app_init(void)
{
    Led_Init();
    display_init();
    Key_Init();

    ClawServo_Init();
    GestureMotor_Init();
    Gesture_Init();
    TaskMode_Init();
    SliderMode_Init();
}

/* ==================== 任务模式 ==================== */

/* APP.c 只负责调度; 任务 1/2/3 的具体实现分别在 task1.c / task2.c / task3.c。 */

/* 进入新任务时调用一次。任务2是自动往返任务, 不等待新手势数据。 */
static void app_task_enter(uint8_t mode)
{
    /* 任务2、任务4和调速模式不等待手势门控。 */
    TaskMode_SetUseGesture(
        ((mode == TASK_MODE_2) ||
         (mode == TASK_MODE_4) ||
         (mode == TASK_MODE_SLIDER)) ? 0u : 1u);

    switch (mode)
    {
        case TASK_MODE_1:
            Task1_Enter();
            break;

        case TASK_MODE_2:
            Task2_Enter();
            break;

        case TASK_MODE_3:
            Task3_Enter();
            break;

        case TASK_MODE_4:
            Task4_Enter();
            break;

        case TASK_MODE_SLIDER:
            SliderMode_Enter();
            break;

        default:
            break;
    }

    /* 任务2/3的硬件动作会关闭共享的TIM3。任务1/3使用手势跟随时,
     * 必须在每次切入后重新启动TIM3, 否则目标会更新但不会产生步进脉冲。 */
    if ((mode == TASK_MODE_1) || (mode == TASK_MODE_3))
    {
        GestureMotor_RestartTimer();
    }
}

/* 退出旧任务时调用一次。公共电机/舵机仍由 app_task_switch() 统一停。 */
static void app_task_exit(uint8_t mode)
{
    switch (mode)
    {
        case TASK_MODE_1:
            Task1_Exit();
            break;

        case TASK_MODE_2:
            Task2_Exit();
            break;

        case TASK_MODE_3:
            Task3_Exit();
            break;

        case TASK_MODE_4:
            Task4_Exit();
            break;

        case TASK_MODE_SLIDER:
            SliderMode_Exit();
            break;

        default:
            break;
    }
}

static void app_task_step(uint8_t mode)
{
    switch (mode)
    {
        case TASK_MODE_1:
            Task1_Step();
            break;

        case TASK_MODE_2:
            Task2_Step();
            break;

        case TASK_MODE_3:
            Task3_Step();
            break;

        case TASK_MODE_4:
            Task4_Step();
            break;

        case TASK_MODE_SLIDER:
            SliderMode_Step();
            break;

        case TASK_MODE_DEFAULT:
        default:
            /* 默认模式不运行手势控制: 没有识别到任务 1 时电机保持停止。 */
            break;
    }
}

/* 收到任务请求后, 先停住旧执行器, 再进入新任务。
 * 新手势数据到来之前 TaskMode_GestureReady() 为 0, 新任务不会拿旧手势乱动。 */
static void app_task_switch(void)
{
    uint8_t req;
    uint8_t old_mode;

    req = TaskMode_TakeRequest();
    if (req == TASK_MODE_NONE)
    {
        return;
    }

    old_mode = TaskMode_Current();
    app_task_exit(old_mode);

    GestureMotor_Stop();
    ClawServo_Hold();

    TaskMode_SetMode(req);
    app_task_enter(req);
    TaskMode_EnterDone();
}

/* 按运动状态刷新显示。 */
static void app_display_task(void)
{
#if APP_LCD_ALWAYS
    Gesture_Show();
    app_show_task();
#else
    if (GestureMotor_IsIdle() != 0u)
    {
        Gesture_Show();
        app_show_task();
    }
#endif
}

/* 更新数据指示灯。 */
static void app_led_task(void)
{
    if (Gesture_HasData() != 0)
    {
        Led_On(LED_1);
    }
    else
    {
        Led_Off(LED_1);
    }
}

/* 应用主循环。 */
void app_main(void)
{
    app_init();

    while (1)
    {
        Gesture_Poll();
        app_task_switch();
        TaskMode_Poll();

        if (TaskMode_GestureReady() != 0u)
        {
            app_task_step(TaskMode_Current());
        }

        ClawServo_Update();
        app_display_task();
        TaskMode_Show();
        SliderMode_Show();
        app_led_task();
    }
}
