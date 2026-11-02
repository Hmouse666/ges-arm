#include "ustimer.h"

static uint8_t s_use_dwt;       /* 1 = DWT 周期计数器可用(正常情况下都是 1) */

void Us_Init(void)
{
    uint32_t a;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;     /* 打开跟踪单元 */
    DWT->CYCCNT = 0u;                                   /* 计数器清零 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;                /* 让周期计数器开始数 */

    /* 空转一小会儿(几十微秒), 看计数有没有往前走; 没走就退回 SysTick 那一路 */
    a = DWT->CYCCNT;
    for (volatile int i = 0; i < 2000; i++)
    {
        __NOP();
    }
    s_use_dwt = (DWT->CYCCNT != a) ? 1u : 0u;
}

uint32_t Us_Now(void)
{
    if (s_use_dwt != 0u)
    {
        return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
    }

    /* 兜底: HAL 的 1ms 节拍 + SysTick 当前值拼出微秒(SysTick 是倒着数的, 所以要 LOAD-VAL) */
    return (HAL_GetTick() * 1000u) +
           ((SysTick->LOAD - SysTick->VAL) / (SystemCoreClock / 1000000u));
}
