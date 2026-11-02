#ifndef __KEY_H_
#define __KEY_H_

#include "main.h"
#include <stdint.h>

/* ==================== 按键编号 ====================
 *
 * 4 个按键都是一头接 GND, 用芯片内部上拉(不用外接电阻)。
 *
 *      KEY_1 = PC14     KEY_2 = PC13     KEY_3 = PC15     KEY_4 = PA9
 *
 * ★ 现在 APP.c 里一个按键都没用到(舵机跟 FPGA 的状态, 电机1 跟 X 轴),
 *   这里只负责把按键脚配上拉 —— 以后想用哪个键, 直接在 APP.c 里写
 *   Key_GetEvent(KEY_x) 就行。
 *   (PA0/PA1 给电机1, PA8 给屏幕 RES, PA11 给屏幕 CS, PB10/PB11 给 FPGA 串口,
 *    PA6/PA7 给舵机模块的软件 I2C, 这些脚都不在按键表里)
 *
 * 接的脚不一样: 改 key.c 里的 key_port[] / key_pin[] 就行 */
#define KEY_1       0       /* PC14 */
#define KEY_2       1       /* PC13 */
#define KEY_3       2       /* PC15 */
#define KEY_4       3       /* PA9  */
#define KEY_NUM     4       /* 按键总数 */

/* 消抖次数: 每调用一次Key_Scan记1次, 连续记满这么多次才算按下了 */
#define KEY_DEBOUNCE_CNT    5

/* ==================== 接口函数 ==================== */
void    Key_Init(void);             /* 按键初始化: 引脚配成输入+上拉 */
void    Key_Scan(void);             /* 按键扫描: 主循环里周期性调用 */
uint8_t Key_GetEvent(uint8_t key);  /* 取按键事件: 按下了返回1, 读取后自动清除 */

#endif /* __KEY_H_ */
