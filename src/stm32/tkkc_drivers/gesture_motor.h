#ifndef __GESTURE_MOTOR_H
#define __GESTURE_MOTOR_H

#include "main.h"

/* ==========================================================================
 * 电机随手势驱动模块
 *
 * 电机1 跟随 FPGA 手势 X 坐标; 电机2(向前)和电机4(向下)
 * 由同一深度坐标按不同比例独立跟随。
 * TIM3 脉冲中断、轴跟随状态和 X/深度轴优先级都封装在本模块内。
 *
 * 使用:
 *   GestureMotor_Init();   初始化电机、微秒计时和 TIM3 脉冲定时器
 *   GestureMotor_Task();   主循环反复调用, 根据手势更新两个轴
 *   GestureMotor_IsIdle(); 1 = 两个轴都已停止, 0 = 仍在运动
 * ========================================================================== */

void    GestureMotor_Init(void);
void    GestureMotor_RestartTimer(void);
void    GestureMotor_Task(void);
void    GestureMotor_Stop(void);
uint8_t GestureMotor_IsIdle(void);

#endif /* __GESTURE_MOTOR_H */
