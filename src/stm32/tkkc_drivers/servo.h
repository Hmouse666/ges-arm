#ifndef __SERVO_H_
#define __SERVO_H_

#include "main.h"
#include "pca9685.h"

/* ==========================================================================
 * 舵机驱动(在 PCA9685 上面封一层, 用起来只要给"角度")
 *
 * 角度怎么填(可以填负数, 正负就是正反转):
 *       0   = 中位            -> 1500us
 *      +30  = 正着转 30 度     -> 1800us
 *      -30  = 反着转 30 度     -> 1200us
 *   能填的范围是 -90 ~ +90(对应 500us ~ 2500us), 超出会自动卡在头上, 防止撞机械限位。
 *   哪个舵机装反了, 只要把它的角度符号反过来填就行了。
 *
 *   如果舵机行程更大, 先用 Servo_SetRange(400, 2600) 把脉宽范围放宽,
 *   -90~+90 就自动对应到 400~2600us, 可调行程跟着变大。
 *
 * 两种转法:
 *   1) 立刻转:  Servo_SetAngle(通道, 角度);
 *   2) 慢慢转:  Servo_SetAngleSlow(通道, 角度, 每步几度, 每步几毫秒);
 *              然后在主循环里反复调用 Servo_MoveUpdate(), 舵机就会一小步一小步转过去。
 *              机械臂要"丝滑、不卡顿、不突然甩大角度"就用这种。
 *
 * 其它常用:
 *   Servo_Init();                 初始化(50Hz, 先不出脉冲)
 *   Servo_SetStartAngle(1, -30);  让 1 号舵机停在 -30 度, 并把这里当成缓动起点
 *   Servo_SetPulse(1, 1500);      也可以直接给高电平时间(1500us)
 *   Servo_GetAngle(1);            现在(库认为)在多少度(-90~+90)
 *   Servo_IsMoving(1);            1 = 还在慢慢转, 0 = 已经到位
 *   Servo_Off(1);                 1 号舵机不再受力
 *   Servo_OffAll();               全部松开
 * ========================================================================== */

#define SERVO_CH_NUM        16      /* 一块 PCA9685 有 16 路 */
#define SERVO_CENTER_ANGLE  90      /* 填 0 度时, 芯片内部的绝对角度(1500us 中位) */
#define SERVO_ANGLE_MAX     90      /* 能填的最大角度 */
#define SERVO_ANGLE_MIN     (-90)   /* 能填的最小角度 */

/* ==================== 接口函数 ==================== */
uint8_t Servo_Init(void);                                   /* 0 = 成功, 1 = 失败 */
void    Servo_SetRange(uint16_t min_us, uint16_t max_us);   /* 改脉宽范围 */
void    Servo_SetAngle(uint8_t ch, int16_t angle);          /* 立刻转到 -90~+90 度 */
void    Servo_SetPulse(uint8_t ch, uint16_t us);            /* 直接给脉宽(us) */
void    Servo_Off(uint8_t ch);                              /* 单路松开 */
void    Servo_OffAll(void);                                 /* 全部松开 */

/* ==================== 慢慢转(缓动)接口 ==================== */

/* 让某个舵机停在这个角度(立刻输出脉冲), 同时把这个角度记成缓动的起点 */
void    Servo_SetStartAngle(uint8_t ch, int16_t angle);

/* 设置缓动目标: 从当前角度出发, 每 step_ms 毫秒转 step_deg 度, 慢慢转到 angle */
void    Servo_SetAngleSlow(uint8_t ch, int16_t angle, uint8_t step_deg, uint16_t step_ms);

/* 缓动发动机: 必须放在主循环里反复调用, 不调用舵机就不会动 */
void    Servo_MoveUpdate(void);

int16_t Servo_GetAngle(uint8_t ch);                         /* 当前角度(-90~+90) */
uint8_t Servo_IsMoving(uint8_t ch);                         /* 1 = 还在转, 0 = 到位了 */

#endif /* __SERVO_H_ */
