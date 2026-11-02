#ifndef __DISPLAY_H_
#define __DISPLAY_H_

#include "tkkc_st7735.h"

void display_init(void);

void display_String(uint8_t x, uint8_t y, const char ch[]);

#endif
