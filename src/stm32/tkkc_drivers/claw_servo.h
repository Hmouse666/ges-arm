#ifndef __CLAW_SERVO_H
#define __CLAW_SERVO_H

#include "main.h"

/* ==========================================================================
 * 爪子舵机应用模块
 *
 * 在 servo.c 的角度接口上面封装爪子的开/合角度、缓动参数和手势控制。
 *
 * 使用:
 *   ClawServo_Init();    初始化 PCA9685 和爪子初始角度
 *   ClawServo_Task();    主循环反复调用, 根据手势决定目标角度
 *   ClawServo_Update();  主循环反复调用, 推进舵机缓动
 *   ClawServo_GetAngle(); 当前爪子舵机角度(-90~+90)
 *   ClawServo_HasError(); 1 = PCA9685 初始化失败
 * ========================================================================== */

void    ClawServo_Init(void);
void    ClawServo_Task(void);
void    ClawServo_Hold(void);
void    ClawServo_Grab(void);
void    ClawServo_Open(void);
uint8_t ClawServo_IsMoving(void);
void    ClawServo_Update(void);
int16_t ClawServo_GetAngle(void);
uint8_t ClawServo_HasError(void);

#endif /* __CLAW_SERVO_H */
