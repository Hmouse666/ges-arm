#ifndef __CLAW_H
#define __CLAW_H

#include "main.h"

/* ==========================================================================
 * 爪子(舵机) —— 跟 FPGA 的"抓取 / 松开"状态夹紧、张开
 *
 *   舵机插在 PCA9685 的第 CLAW_SERVO_CH 路(PCA9685 走软件 I2C: SCL=PA6, SDA=PA7);
 *   两个角度和转动快慢就是下面这几个宏, 想改抓取角度只改 CLAW_ANGLE_GRAB。
 *   状态要连续 GEST_CONFIRM_FRAMES 帧(或 GEST_CONFIRM_MS 毫秒)才认 —— 防抖在
 *   fpga_gesture.c 里做, 这里拿到的就是防抖之后的状态。
 * ========================================================================== */

#define CLAW_SERVO_CH       2       /* 爪子舵机接在 PCA9685 的第几路(0~15) */
#define CLAW_ANGLE_OPEN     (-40)   /* "松开"时的角度(上电也是这个角度) */
#define CLAW_ANGLE_GRAB     40      /* "抓取"时的角度 */
#define CLAW_STEP_DEG       5       /* 转动快慢: 每 CLAW_STEP_MS 毫秒转几度 */
#define CLAW_STEP_MS        10

uint8_t Claw_Init(void);        /* 初始化 PCA9685 + 摆到"松开"角度; 1 = 初始化失败 */
void    Claw_Task(void);        /* 主循环里反复调: 跟手势状态张开 / 夹紧 */
void    Claw_Update(void);      /* 主循环里反复调: 舵机缓动(必须一直调) */
int16_t Claw_Angle(void);       /* 当前角度(屏幕显示用) */

#endif /* __CLAW_H */
