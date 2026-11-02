#ifndef __LED_H_
#define __LED_H_

#include "main.h"
#include <stdint.h>

/* ==================== LED编号 ==================== */
#define LED_1       1       /* PA15 */
#define LED_2       2       /* PB3  */
#define LED_3       3       /* PB4  */
#define LED_4       4       /* PB5  */
#define LED_5       5       /* PB6  */
#define LED_6       6       /* PB7  */
#define LED_7       7       /* PB8  -> TIM4_CH3, PWM灯 */
#define LED_8       8       /* PB9  -> TIM4_CH4, PWM灯 */
#define LED_NUM     8       /* LED总数 */

#define GPIO_LED_NUM    6   /* 前6个灯是普通IO灯, 后2个灯由TIM4的PWM驱动 */

/* 亮灭电平: 只对 LED_1~LED_6(普通IO) 有效, 如果板子接法相反把这两个宏对调 */
#define LED_ON      GPIO_PIN_SET
#define LED_OFF     GPIO_PIN_RESET

/* PWM灯的亮度: TIM4的ARR=99, 所以 0=完全熄灭, LED_DUTY_MAX=最亮(100%占空比) */
#define LED_DUTY_MAX    100

/* ==================== 接口函数 ==================== */
void Led_Init(void);                        /* LED初始化: 配置LED_1~LED_6这6个普通IO, 并全部熄灭 */
void Led_On(uint8_t n);                     /* 点亮第n个灯(n = LED_1~LED_8) */
void Led_Off(uint8_t n);                    /* 熄灭第n个灯(n = LED_1~LED_8) */
void Led_Toggle(uint8_t n);                 /* 翻转第n个灯(n = LED_1~LED_8) */
void Led_AllOn(void);                       /* 全部点亮 */
void Led_AllOff(void);                      /* 全部熄灭 */
void Led_SetDuty(uint8_t n, uint16_t duty); /* 设置亮度, 只对PWM灯LED_7/LED_8有效(呼吸灯用) */
uint16_t Led_GetDuty(uint8_t n);            /* 读取亮度, 只对PWM灯LED_7/LED_8有效 */
uint8_t Led_GetState(uint8_t n);            /* 读取第n个灯的状态: 1=亮, 0=灭(屏幕上显示用) */

#endif /* __LED_H_ */
