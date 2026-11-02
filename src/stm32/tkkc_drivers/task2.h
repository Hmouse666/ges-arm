#ifndef __TASK2_H
#define __TASK2_H

#include <stdint.h>

void Task2_Enter(void);
void Task2_Step(void);
void Task2_Exit(void);

/* 任务2使用TIM3时, 由GestureMotor的TIM3中断转交给这里。 */
uint8_t Task2_TimerPulseActive(void);
void Task2_Timer3PulseIsr(void);

/* 任务2使用TIM4时, 由TIM4中断转交给这里。 */
void Task2_Timer4PulseIsr(void);

/* 任务3复用任务2的电机2/4硬件定时器脉冲输出。
 * 运动时间、同步方式和任务2组24完全相同。 */
void Task2_StartGroup24Motion(uint8_t direction,
                              uint32_t motor2_pulses,
                              uint32_t motor4_pulses);
uint8_t Task2_Group24MotionActive(void);

/* 任务3复用任务2的电机1/3硬件定时器脉冲输出。
 * 两路仍按任务2的方式同步启停, 圈数由调用者传入。 */
void Task2_StartGroup13Motion(uint8_t direction,
                              uint32_t motor1_pulses,
                              uint32_t motor3_pulses);
uint8_t Task2_Group13MotionActive(void);

/* 通用的立即停止接口; 保留旧接口给已有调用兼容。 */
void Task2_StopMotion(void);
void Task2_StopGroup24Motion(void);

#endif /* __TASK2_H */
