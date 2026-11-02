#include "key.h"

/* 按键引脚表: 顺序和 key.h 里的 KEY_1~KEY_4 一一对应
 * ★ 现在 APP.c 里没用到按键, 这里只是把按键脚配上拉。
 *   要换脚就改下面这张表(端口和引脚各改一处), 别的地方都不用动。 */
static GPIO_TypeDef *const key_port[KEY_NUM] = {GPIOC, GPIOC, GPIOC, GPIOA};
static const uint16_t      key_pin[KEY_NUM]  = {
    GPIO_PIN_14,        /* KEY_1 -> PC14 */
    GPIO_PIN_13,        /* KEY_2 -> PC13 */
    GPIO_PIN_15,        /* KEY_3 -> PC15 */
    GPIO_PIN_9,         /* KEY_4 -> PA9  */
};

static uint8_t key_cnt[KEY_NUM];    /* 消抖计数 */
static uint8_t key_event[KEY_NUM];  /* 按下事件标志 */

/**
 * @brief       按键初始化: 引脚配成输入+上拉(按键另一端接GND)
 * @note        GPIO时钟在MX_GPIO_Init里已经打开, 所以Key_Init要放在MX_GPIO_Init之后调用
 */
void Key_Init(void)
{
    uint8_t i;
    GPIO_InitTypeDef KeyInit = {0};

    KeyInit.Mode = GPIO_MODE_INPUT;
    KeyInit.Pull = GPIO_PULLUP;

    for (i = 0; i < KEY_NUM; i++)
    {
        KeyInit.Pin = key_pin[i];
        HAL_GPIO_Init(key_port[i], &KeyInit);

        key_cnt[i]   = 0;
        key_event[i] = 0;
    }
}

/**
 * @brief       按键扫描
 * @note        连续KEY_DEBOUNCE_CNT次读到的都是低电平才认为真的按下了, 中途有抖动会重新计数
 */
void Key_Scan(void)
{
    uint8_t i;

    for (i = 0; i < KEY_NUM; i++)
    {
        if (HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_RESET)    /* 按下是低电平 */
        {
            if (key_cnt[i] < KEY_DEBOUNCE_CNT)
            {
                key_cnt[i]++;

                if (key_cnt[i] == KEY_DEBOUNCE_CNT)     /* 刚好数满这一次, 产生一次按下事件 */
                {
                    key_event[i] = 1;
                }
            }
        }
        else                                            /* 松开了, 计数清零, 下次按下重新计数 */
        {
            key_cnt[i] = 0;
        }
    }
}

/**
 * @brief       取按键事件
 * @param       key     按键编号(KEY_1~KEY_4)
 * @retval      1=按下过一次, 0=没有
 * @note        事件读取后就清除了, 所以按一下只会返回一次1
 */
uint8_t Key_GetEvent(uint8_t key)
{
    if (key >= KEY_NUM)
    {
        return 0;
    }

    if (key_event[key])
    {
        key_event[key] = 0;
        return 1;
    }

    return 0;
}
