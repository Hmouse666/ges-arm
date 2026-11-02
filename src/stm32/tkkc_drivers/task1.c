/**
  ******************************************************************************
  * @file    task1.c
  * @brief   任务 1 的独立实现
  *
  * 现在先直接复用原来的手势控制, 行为与拆分前一致。
  * 以后任务 1 的真实逻辑写在本文件里, 不改任务框架。
  ******************************************************************************
  */

#include "task1.h"
#include "claw_servo.h"
#include "fpga_gesture.h"
#include "gesture_motor.h"

static void Task1_GestureControl(void)
{
    ClawServo_Task();
    GestureMotor_Task();
}

/* 切换到任务 1 时调用一次。 */
void Task1_Enter(void)
{
    /* 在这里初始化任务 1 自己的状态。 */
}

/* 任务 1 运行期间反复调用。 */
void Task1_Step(void)
{
    Task1_GestureControl();
}

/* 离开任务 1 时调用一次。
 * 公共电机/舵机由 APP.c 的 app_task_switch() 统一停, 这里只管任务 1 自己的状态。 */
void Task1_Exit(void)
{
    /* 在这里清理任务 1 自己的状态。 */
}
