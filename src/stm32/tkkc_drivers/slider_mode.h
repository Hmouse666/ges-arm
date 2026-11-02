#ifndef __SLIDER_MODE_H
#define __SLIDER_MODE_H

#include "main.h"

/* 调速模式独立模块: 只负责模式状态和 VALUE 显示。 */
void    SliderMode_Init(void);
void    SliderMode_Enter(void);
void    SliderMode_Step(void);
void    SliderMode_Exit(void);
uint8_t SliderMode_IsActive(void);
void    SliderMode_Show(void);

#endif /* __SLIDER_MODE_H */
