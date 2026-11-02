#ifndef __TASK_MODE_H
#define __TASK_MODE_H

#include "main.h"

/* 模式编号: 0 = 默认, 1/2/3 = TASK1/2/3, 4 = 手动任务, 5 = 调速模式 */
#define TASK_MODE_DEFAULT   0u
#define TASK_MODE_1         1u
#define TASK_MODE_2         2u
#define TASK_MODE_3         3u
#define TASK_MODE_4         4u
#define TASK_MODE_SLIDER    5u
#define TASK_MODE_NONE      0xFFu

/* 初始化任务模式模块。放在 Gesture_Init() 之后调用。 */
void    TaskMode_Init(void);

/* 串口收到一整帧后调用。TASK1上升进入手势任务, TASK1下降时从任务1回到任务2。
 * 解析 TASK1/TASK2/TASK3 和 manual ready, 不直接切执行器。 */
void    TaskMode_OnFrame(const char *line);

/* 这一帧是否包含任务控制关键字(TASK1/2/3 或 manual ready)。 */
uint8_t TaskMode_IsControlFrame(const char *line);

/* 主循环取一次切换请求; 没有请求返回 TASK_MODE_NONE。 */
uint8_t TaskMode_TakeRequest(void);

/* 停完旧任务后, 由 APP.c 提交新模式。 */
void    TaskMode_SetMode(uint8_t mode);
uint8_t TaskMode_Current(void);

/* 新任务初始化完成后调用: 记录当前手势帧号, 等下一帧新手势再开放控制。 */
void    TaskMode_EnterDone(void);

/* 主循环调用: 检查通信超时 / 等待新手势。 */
void    TaskMode_Poll(void);

/* 1 = 新任务已经可以运行, 且手势控制已经重新开放。 */
uint8_t TaskMode_GestureReady(void);

/* 某个任务不需要手势时, 在进入前设为 0; 默认所有任务都使用手势。 */
void    TaskMode_SetUseGesture(uint8_t use);

/* 刷屏幕上的 TASK1/TASK2/TASK3/TASK4 四行。 */
void    TaskMode_Show(void);

#endif /* __TASK_MODE_H */
