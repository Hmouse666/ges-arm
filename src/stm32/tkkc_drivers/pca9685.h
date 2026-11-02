#ifndef __PCA9685_H
#define __PCA9685_H

#include "main.h"

/* ==========================================================================
 * PCA9685 驱动(16 路 PWM 芯片, 舵机就插在它上面)
 *   —— 改成用"软件 I2C"(普通 IO 一位一位模拟), 不再用硬件 I2C2
 *
 * ★★ 为什么要改 ★★
 *   原来 PCA9685 接在硬件 I2C2 上(PB10 = SCL, PB11 = SDA); 现在 PB10/PB11 要给
 *   FPGA 发来的手势数据当串口用(USART3: PB11 收, PB10 发), 一个脚干不了两件事,
 *   所以舵机模块改接到这两个脚上:
 *
 *        PCA9685 的 SCL  ->  PA6
 *        PCA9685 的 SDA  ->  PA7
 *
 *   ★ 你要动的就是这两根线(把模块的 SCL/SDA 从 PB10/PB11 换到 PA6/PA7), 别的都不变。
 *     PA6/PA7 原来是液晶屏的 RES/SDA, 屏幕搬到 SPI2 之后这两个脚就空出来了。
 *
 *   接线:  模块 VCC  -> 建议接 STM32 的 3.3V
 *          模块 GND  -> STM32 的 GND(★ 一定要共地)
 *          模块 V+   -> 舵机电源(单独供 5V, 别从 STM32 上取, 舵机一起动电流很大)
 *          舵机信号线 -> 模块上的 PWM0~PWM15 里的一路
 *   ★ 建议 VCC 接 3.3V: 模块的上拉电阻是接到 VCC 的, 接 5V 的话 I2C 两根线上就是 5V,
 *     而 PA6/PA7 不是"5V 容忍"的脚, 长期这么用不好。接 3.3V 芯片照样工作。
 *
 *   想换脚: 只改下面那 4 行就行, 驱动里别的地方一个字都不用动。
 * ========================================================================== */

/* ---- 软件 I2C 用的两个脚(想换脚只改这 4 行) ---- */
#define PCA9685_SCL_PORT    GPIOA
#define PCA9685_SCL_PIN     GPIO_PIN_6
#define PCA9685_SDA_PORT    GPIOA
#define PCA9685_SDA_PIN     GPIO_PIN_7

/* ---- 模拟 I2C 的快慢: 每个电平保持多少"空循环"(1 次大约 0.1us) ----
 * PCA9685 的 I2C 最快 1MHz, 这里跑几十 kHz 就非常稳, 也不会明显拖慢主循环。
 * 如果遇到"初始化偶尔失败/舵机偶尔抖一下"(通信不稳), 把这个数加大到 60 试试。 */
#define PCA9685_I2C_DELAY   30

/* 从机地址: 模块上 A0~A5 都接地时是 0x40 */
#define PCA9685_ADDR        0x40

/* 软件 I2C 发送时用的 8 位地址(7 位地址左移一位, 最低位是读/写标志) */
#define PCA9685_ADDR8       (PCA9685_ADDR << 1)

/* 用到的寄存器地址 */
#define PCA9685_MODE1       0x00        /* 模式寄存器1: 休眠、重启、地址自增 */
#define PCA9685_MODE2       0x01        /* 模式寄存器2: 输出方式 */
#define PCA9685_LED0_ON_L   0x06        /* 通道0 的起始寄存器(每路占4个) */
#define PCA9685_ALL_LED_ON_L 0xFA       /* 一次性控制全部 16 路 */
#define PCA9685_PRESCALE    0xFE        /* 分频寄存器: 决定 PWM 频率 */

/* 函数原型 */
uint8_t PCA9685_Init(void);                         /* 0 = 成功, 1 = 模块没应答(查 PA6/PA7 接线) */
void    PCA9685_SetFreq(uint16_t freq);             /* 设置频率, 舵机一般 50Hz */
void    PCA9685_SetPWM(uint8_t ch, uint16_t off);   /* off: 0~4095, 4096 = 不发脉冲 */
void    PCA9685_AllOff(void);                       /* 16 路全部关闭 */

#endif /* __PCA9685_H */
