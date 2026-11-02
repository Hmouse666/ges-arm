#include "led.h"
#include "tim.h"        /* LED_7/LED_8 (PB8/PB9) 由 TIM4_CH3/CH4 输出PWM */

/* 6个普通IO灯: LED_1~LED_6, 顺序和 led.h 里的 LED_1~LED_6 一一对应 */
static GPIO_TypeDef *const led_port[GPIO_LED_NUM] = {GPIOA, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB};
static const uint16_t      led_pin[GPIO_LED_NUM]  = {
    GPIO_PIN_15,        /* LED_1 -> PA15 */
    GPIO_PIN_3,         /* LED_2 -> PB3  */
    GPIO_PIN_4,         /* LED_3 -> PB4  */
    GPIO_PIN_5,         /* LED_4 -> PB5  */
    GPIO_PIN_6,         /* LED_5 -> PB6  */
    GPIO_PIN_7,         /* LED_6 -> PB7  */
};

/**
 * @brief       设置PWM灯的亮度(呼吸灯就是靠这个函数不停改亮度)
 * @param       n       LED编号, 只有 LED_7(PB8=TIM4_CH3) 和 LED_8(PB9=TIM4_CH4) 有效
 * @param       duty    亮度 0~LED_DUTY_MAX, 0=熄灭, LED_DUTY_MAX=最亮
 * @note        如果这两个灯是低电平点亮(占空比越大反而越暗), 把 duty 改成 LED_DUTY_MAX - duty
 */
void Led_SetDuty(uint8_t n, uint16_t duty)
{
    if (duty > LED_DUTY_MAX)
    {
        duty = LED_DUTY_MAX;
    }

    if (n == LED_7)
    {
        __HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_3, duty);      /* PB8 */
    }
    else if (n == LED_8)
    {
        __HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_4, duty);      /* PB9 */
    }
}

/**
 * @brief       读取PWM灯当前的亮度
 * @param       n       LED编号, 只有 LED_7(PB8) 和 LED_8(PB9) 有效
 * @retval      亮度 0~LED_DUTY_MAX
 */
uint16_t Led_GetDuty(uint8_t n)
{
    if (n == LED_7)
    {
        return (uint16_t)__HAL_TIM_GetCompare(&htim4, TIM_CHANNEL_3);   /* PB8 */
    }

    if (n == LED_8)
    {
        return (uint16_t)__HAL_TIM_GetCompare(&htim4, TIM_CHANNEL_4);   /* PB9 */
    }

    return 0;
}

/**
 * @brief       读取第n个灯的当前状态
 * @param       n       LED编号(LED_1~LED_8)
 * @retval      1=亮, 0=灭
 * @note        普通IO灯直接读引脚电平; PWM灯(亮度>0就算亮)读CCR
 */
uint8_t Led_GetState(uint8_t n)
{
    if (n < LED_1 || n > LED_NUM)
    {
        return 0;
    }

    if (n >= LED_7)
    {
        return (Led_GetDuty(n) > 0) ? 1 : 0;
    }

    return (HAL_GPIO_ReadPin(led_port[n - 1], led_pin[n - 1]) == LED_ON) ? 1 : 0;
}

/**
 * @brief       LED初始化
 * @note        只把 LED_1~LED_6 这6个引脚配成推挽输出;
 *              PB8/PB9 是 TIM4_CH3/CH4 的复用输出, 已经由 MX_TIM4_Init() 配好了,
 *              这里绝对不能再配它们, 否则PWM出不到引脚上, 呼吸灯就会失效
 *              PB3/PB4是JTAG引脚, CubeMX已经在HAL_MspInit里关掉JTAG(保留SWD), 可以直接当普通IO用
 */
void Led_Init(void)
{
    uint8_t i;
    GPIO_InitTypeDef LedInit = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    LedInit.Mode  = GPIO_MODE_OUTPUT_PP;
    LedInit.Pull  = GPIO_NOPULL;
    LedInit.Speed = GPIO_SPEED_FREQ_LOW;

    for (i = 0; i < GPIO_LED_NUM; i++)
    {
        LedInit.Pin = led_pin[i];
        HAL_GPIO_Init(led_port[i], &LedInit);
    }

    Led_AllOff();
}

/**
 * @brief       点亮第n个灯
 * @param       n   LED编号(LED_1~LED_8)
 */
void Led_On(uint8_t n)
{
    if (n < LED_1 || n > LED_NUM)
    {
        return;
    }

    if (n >= LED_7)
    {
        Led_SetDuty(n, LED_DUTY_MAX);               /* PWM灯: 占空比拉满就是点亮 */
    }
    else
    {
        HAL_GPIO_WritePin(led_port[n - 1], led_pin[n - 1], LED_ON);
    }
}

/**
 * @brief       熄灭第n个灯
 * @param       n   LED编号(LED_1~LED_8)
 */
void Led_Off(uint8_t n)
{
    if (n < LED_1 || n > LED_NUM)
    {
        return;
    }

    if (n >= LED_7)
    {
        Led_SetDuty(n, 0);                          /* PWM灯: 占空比0就是熄灭 */
    }
    else
    {
        HAL_GPIO_WritePin(led_port[n - 1], led_pin[n - 1], LED_OFF);
    }
}

/**
 * @brief       翻转第n个灯(亮变灭, 灭变亮)
 * @param       n   LED编号(LED_1~LED_8)
 */
void Led_Toggle(uint8_t n)
{
    if (n < LED_1 || n > LED_NUM)
    {
        return;
    }

    if (n >= LED_7)
    {
        Led_SetDuty(n, (Led_GetDuty(n) == 0) ? LED_DUTY_MAX : 0);
    }
    else
    {
        HAL_GPIO_TogglePin(led_port[n - 1], led_pin[n - 1]);
    }
}

/**
 * @brief       8个灯全部点亮
 */
void Led_AllOn(void)
{
    uint8_t i;

    for (i = 0; i < GPIO_LED_NUM; i++)
    {
        HAL_GPIO_WritePin(led_port[i], led_pin[i], LED_ON);
    }

    Led_SetDuty(LED_7, LED_DUTY_MAX);
    Led_SetDuty(LED_8, LED_DUTY_MAX);
}

/**
 * @brief       8个灯全部熄灭
 */
void Led_AllOff(void)
{
    uint8_t i;

    for (i = 0; i < GPIO_LED_NUM; i++)
    {
        HAL_GPIO_WritePin(led_port[i], led_pin[i], LED_OFF);
    }

    Led_SetDuty(LED_7, 0);
    Led_SetDuty(LED_8, 0);
}
