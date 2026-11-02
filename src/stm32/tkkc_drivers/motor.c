#include "motor.h"

/* ==========================================================================
 * 步进电机驱动(从 test3 工程移植)
 *
 * 引脚表在 motor.h 的"① 引脚配置"里, 这里是实际发脉冲的代码。
 * 用的就是 test3 那套脚(和你的接线一致):
 *      电机1: PUL=PA0   DIR=PA1   EN=PA2
 *      电机2: PUL=PB0   DIR=PB1   EN=PA3
 *      电机3: PUL=PB6   DIR=PB8   EN=PB9   
 *      电机4: PUL=PA12  DIR=PB3   EN=PB5   
 *
 * ★ 这些脚里有 8 个在本工程里还连着按键/屏幕/LED, 一共做了哪些让步、
 *   为什么 PA3 必须二选一, motor.h 开头的表里写得很清楚, 动手前先看一眼。
 *
 * 说明: 这里故意保留原来的 if / else 写法(没用查表),
 * 因为 Motor_Run() 是靠"空循环数时钟周期"来定脉冲频率的,
 * 一旦改写结构, 编译器生成的循环指令条数就会变, 电机转速跟着变。
 * 保持原样可以让电机的转速和 test3 里一模一样。
 *
 * ★ 两个电机同时跟手(比如电机1 跟 X、电机4 跟 Y)的时候, 用 Motor_Step() 发脉冲:
 *   它一次只发一个脉冲就返回(不阻塞), 主循环每转一圈给两个电机各发一个 ——
 *   谁也不用等谁, 两轴同时跟手也不会抖。用意见 motor.h 的"⑧"。
 *   (Motor_Run() 那一版是阻塞的: 一整片脉冲发完才返回, 两个电机一起用就会互相等。)
 * ========================================================================== */

/**
  * @brief  把一个引脚配成推挽输出(Motor_Init 里用, 只在开机时跑一次)
  */
static void Motor_PinInit(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin   = pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &gpio);
}

void Motor_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* ---------------- 12 个脚全部配成推挽输出 ---------------- */
    Motor_PinInit(MOTOR1_PUL_PORT, MOTOR1_PUL_PIN);
    Motor_PinInit(MOTOR1_DIR_PORT, MOTOR1_DIR_PIN);
    Motor_PinInit(MOTOR1_EN_PORT,  MOTOR1_EN_PIN);

    Motor_PinInit(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN);
    Motor_PinInit(MOTOR2_DIR_PORT, MOTOR2_DIR_PIN);
    Motor_PinInit(MOTOR2_EN_PORT,  MOTOR2_EN_PIN);

    Motor_PinInit(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN);
    Motor_PinInit(MOTOR3_DIR_PORT, MOTOR3_DIR_PIN);
    Motor_PinInit(MOTOR3_EN_PORT,  MOTOR3_EN_PIN);

    Motor_PinInit(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN);
    Motor_PinInit(MOTOR4_DIR_PORT, MOTOR4_DIR_PIN);
    Motor_PinInit(MOTOR4_EN_PORT,  MOTOR4_EN_PIN);

    /* ---------------- PA12(现在是 电机4 的脉冲脚)要多做一步 ----------------
     * test3 里 PA12 原来是 KEY2 按键(EXTI12 外部中断), 所以那里用 PA12 发脉冲前
     * 必须先把 EXTI12 关掉: HAL 把引脚重新配成"输出"时并不会顺手清掉 EXTI 的
     * 屏蔽位(IMR), 而 EXTI 是盯着引脚电平的(不管它是输入还是输出),
     * 不关的话每发一个脉冲都会触发一次按键中断, 把脉冲节奏整个拖慢。
     *
     * 本工程(改成按键版之后)没有用 EXTI 中断, 所以按理说不关也没事,
     * 这里连着一起搬过来当保险。 */
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
    EXTI->IMR &= ~(uint32_t)GPIO_PIN_12;    /* 关掉 EXTI12 的中断请求 */
    EXTI->EMR &= ~(uint32_t)GPIO_PIN_12;    /* 关掉 EXTI12 的事件请求 */
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_12);  /* 清掉可能已经挂起的标志 */

    /* ---------------- 默认状态: 使能电机 + 脉冲/方向先拉低 ---------------- */
    HAL_GPIO_WritePin(MOTOR1_EN_PORT,  MOTOR1_EN_PIN,  MOTOR1_EN_ACTIVE);   /* 电机1 使能 */
    HAL_GPIO_WritePin(MOTOR1_PUL_PORT, MOTOR1_PUL_PIN, GPIO_PIN_RESET);     /* 电机1 脉冲先拉低 */
    /* 方向脚先摆成"正转"那一档(方向反了的电机见 motor.h 里的 DIR_INVERT) */
    HAL_GPIO_WritePin(MOTOR1_DIR_PORT, MOTOR1_DIR_PIN, (GPIO_PinState)(0u ^ MOTOR1_DIR_INVERT));

    HAL_GPIO_WritePin(MOTOR2_EN_PORT,  MOTOR2_EN_PIN,  MOTOR2_EN_ACTIVE);   /* 电机2 使能 */
    HAL_GPIO_WritePin(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR2_DIR_PORT, MOTOR2_DIR_PIN, (GPIO_PinState)(0u ^ MOTOR2_DIR_INVERT));

    HAL_GPIO_WritePin(MOTOR3_EN_PORT,  MOTOR3_EN_PIN,  MOTOR3_EN_ACTIVE);   /* 电机3 使能 */
    HAL_GPIO_WritePin(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR3_DIR_PORT, MOTOR3_DIR_PIN, (GPIO_PinState)(0u ^ MOTOR3_DIR_INVERT));

    HAL_GPIO_WritePin(MOTOR4_EN_PORT,  MOTOR4_EN_PIN,  MOTOR4_EN_ACTIVE);   /* 电机4 使能 */
    HAL_GPIO_WritePin(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR4_DIR_PORT, MOTOR4_DIR_PIN, (GPIO_PinState)(0u ^ MOTOR4_DIR_INVERT));
}

/* ==========================================================================
 * 发脉冲的小工具 + Motor_Step() —— 引脚表用的还是上面那几个宏
 *
 * ★ 一句话: Motor_Run() 一次要把一整片脉冲发完(阻塞几十毫秒), 两个电机一起跟手时
 *   就会互相等、一顿一顿地抖; 所以另做一个 Motor_Step() —— 一次只发一个脉冲就返回,
 *   由主循环每转一圈给两个电机各发一个, 谁也不等谁。详见 motor.h 的"⑧"。
 * ========================================================================== */

/* 每个电机上一次写进方向脚的电平(0/1), 用来避免每次都重写方向脚 */
static uint8_t  s_dir_last[5];          /* 下标用 1~4(电机号), 0 不用 */
static uint8_t  s_dir_ok[5];            /* 1 = s_dir_last[] 里已经是有效值了 */

/* 把某个电机的方向脚设好(和 Motor_Run() 里那段一样, 含各自的 DIR_INVERT) */
static void Motor_DirSet(uint8_t motor, uint8_t dir)
{
    dir = (dir != 0u) ? 1u : 0u;

    if(motor == 1u)
        HAL_GPIO_WritePin(MOTOR1_DIR_PORT, MOTOR1_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR1_DIR_INVERT));
    else if(motor == 2u)
        HAL_GPIO_WritePin(MOTOR2_DIR_PORT, MOTOR2_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR2_DIR_INVERT));
    else if(motor == 3u)
        HAL_GPIO_WritePin(MOTOR3_DIR_PORT, MOTOR3_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR3_DIR_INVERT));
    else
        HAL_GPIO_WritePin(MOTOR4_DIR_PORT, MOTOR4_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR4_DIR_INVERT));
}

/* 把某个电机的脉冲脚拉高 / 拉低 */
static void Motor_PulseHi(uint8_t motor)
{
    if(motor == 1u)
        HAL_GPIO_WritePin(MOTOR1_PUL_PORT, MOTOR1_PUL_PIN, GPIO_PIN_SET);
    else if(motor == 2u)
        HAL_GPIO_WritePin(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN, GPIO_PIN_SET);
    else if(motor == 3u)
        HAL_GPIO_WritePin(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN, GPIO_PIN_SET);
}

static void Motor_PulseLo(uint8_t motor)
{
    if(motor == 1u)
        HAL_GPIO_WritePin(MOTOR1_PUL_PORT, MOTOR1_PUL_PIN, GPIO_PIN_RESET);
    else if(motor == 2u)
        HAL_GPIO_WritePin(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN, GPIO_PIN_RESET);
    else if(motor == 3u)
        HAL_GPIO_WritePin(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN, GPIO_PIN_RESET);
    else
        HAL_GPIO_WritePin(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN, GPIO_PIN_RESET);
}

/* 发一个完整的脉冲(高 → 低); 高电平宽度和 Motor_Run() 里一样, 驱动器才认得出 */
static void Motor_PulseOnce(uint8_t motor)
{
    Motor_PulseHi(motor);
    for(volatile int j=0; j<PULSE_HIGH_CNT; j++) __NOP();
    Motor_PulseLo(motor);
}

void Motor_Step(uint8_t motor, uint8_t dir)
{
    uint8_t level;

    if((motor != 1) && (motor != 2) && (motor != 3) && (motor != 4)) return;

    dir   = (dir != 0u) ? 1u : 0u;
    level = dir;
    if(motor == 1)      level ^= MOTOR1_DIR_INVERT;     /* 各电机自己的"方向反不反" */
    else if(motor == 2) level ^= MOTOR2_DIR_INVERT;
    else if(motor == 3) level ^= MOTOR3_DIR_INVERT;
    else                level ^= MOTOR4_DIR_INVERT;

    /* ★ 方向脚只在"真的要换向"的时候才重写, 并且多等一小会儿(十几微秒)再发脉冲 ——
     *   驱动器要求 DIR 在脉冲到来之前就稳定好。同一个方向连着发脉冲时这段等待一次都不走,
     *   所以连续发脉冲的节拍很干净(不会每发一个都白等一下)。 */
    if((s_dir_ok[motor] == 0u) || (s_dir_last[motor] != level))
    {
        Motor_DirSet(motor, dir);
        s_dir_last[motor] = level;
        s_dir_ok[motor]   = 1u;
        for(volatile int k=0; k<120; k++) __NOP();
    }

    Motor_PulseOnce(motor);             /* 发这一个脉冲(高 -> 低, 高电平宽度和 Motor_Run 里一样) */
}

/* ==========================================================================
 * 只让一个电机转 rev 圈
 *
 *   motor    : 电机号, 用 MOTOR_1 / MOTOR_2 / MOTOR_3 / MOTOR_4
 *   rev      : 转多少圈, 可以填小数(例如 0.5f = 半圈, 1.0f = 一圈)
 *   dir      : 方向, 0 = 正转, 1 = 反转(填非 0 都是反转)
 *   delay_us : 两个脉冲之间的间隔(微秒), 越大转得越慢, 常用 DEFAULT_SPEED
 *
 * 注意: 这是"阻塞"函数 —— 要把这一圈的脉冲全部发完才返回, 期间干不了别的事
 *       (按键扫描、舵机缓动、屏幕刷新都会停在这段时间里)。
 * ========================================================================== */
void Motor_Run(uint8_t motor, float rev, uint8_t dir, uint16_t delay_us)
{
    uint32_t pulses = (uint32_t)(rev * PULSE_PER_REV + 0.5f);

    /* 电机号不对(现在只有 1、2、3、4 四个电机)就直接返回 */
    if((motor != 1) && (motor != 2) && (motor != 3) && (motor != 4)) return;
    if(pulses == 0) return;

    /* 设置方向: 先把 dir 规整成 0/1(填非 0 都算反转),
     * 再按每个电机的 DIR_INVERT 决定方向脚实际是高还是低。
     * 电机4 装反了, 所以 motor.h 里 MOTOR4_DIR_INVERT = 1, 这里会自动翻过来。 */
    dir = (dir != 0) ? 1u : 0u;

    if(motor == 1)
        HAL_GPIO_WritePin(MOTOR1_DIR_PORT, MOTOR1_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR1_DIR_INVERT));
    else if(motor == 2)
        HAL_GPIO_WritePin(MOTOR2_DIR_PORT, MOTOR2_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR2_DIR_INVERT));
    else if(motor == 3)
        HAL_GPIO_WritePin(MOTOR3_DIR_PORT, MOTOR3_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR3_DIR_INVERT));   /* 电机3 方向脚 */
    else
        HAL_GPIO_WritePin(MOTOR4_DIR_PORT, MOTOR4_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR4_DIR_INVERT));   /* 电机4 方向脚 */

    /* 方向建立时间: 等一小会儿再发脉冲, 让驱动器先认好方向 */
    for(volatile int i=0; i<10; i++) __NOP();

    /* 循环发送脉冲 */
    for(uint32_t i=0; i<pulses; i++)
    {
        /* 输出高电平 */
        if(motor == 1)
            HAL_GPIO_WritePin(MOTOR1_PUL_PORT, MOTOR1_PUL_PIN, GPIO_PIN_SET);
        else if(motor == 2)
            HAL_GPIO_WritePin(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN, GPIO_PIN_SET);
        else if(motor == 3)
            HAL_GPIO_WritePin(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN, GPIO_PIN_SET);     /* 电机3 脉冲脚 */
        else
            HAL_GPIO_WritePin(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN, GPIO_PIN_SET);     /* 电机4 脉冲脚 */

        /* 高电平要保持够久, 驱动器(光耦)才认得出这一步 */
        for(volatile int j=0; j<PULSE_HIGH_CNT; j++) __NOP();

        /* 输出低电平(产生下降沿) */
        if(motor == 1)
            HAL_GPIO_WritePin(MOTOR1_PUL_PORT, MOTOR1_PUL_PIN, GPIO_PIN_RESET);
        else if(motor == 2)
            HAL_GPIO_WritePin(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN, GPIO_PIN_RESET);
        else if(motor == 3)
            HAL_GPIO_WritePin(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN, GPIO_PIN_RESET);   /* 电机3 脉冲脚 */
        else
            HAL_GPIO_WritePin(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN, GPIO_PIN_RESET);   /* 电机4 脉冲脚 */

        /* 脉冲间隔延时。
         * 下面这个空循环每转一圈大约 6 个时钟周期, 12 × 6 = 72 个周期 = 1us(72MHz),
         * 所以 delay_us 就是"脉冲间隔是多少微秒", 脉冲频率 ≈ 1 / delay_us。 */
        uint32_t delay = delay_us * 12;
        for(volatile int j=0; j<delay; j++) __NOP();
    }
}

/* ==========================================================================
 * 让选中的几个电机"同时"转 rev 圈
 *
 *   sel      : 用 MOTOR_SEL_1 ~ MOTOR_SEL_4 按位或起来
 *              四个一起转就写 MOTOR_SEL_ALL
 *   其它参数和 Motor_Run() 完全一样
 *
 * 为什么这样就"同步"了:
 *   四个电机是在同一个 for 循环里一起翻转脉冲脚的 ——
 *   一起拉高、一起拉低、一起等延时, 所以脉冲个数和节拍完全一致,
 *   是真正的同步启动、同步停止, 而不是"先转完一个再转下一个"。
 *
 * 注意:
 *   1) 几个电机同时转, 24V 电源要能供得起几倍的电流, 不然会没力气/丢步;
 *   2) 这个函数也是阻塞的, 转完才返回。
 * ========================================================================== */
void Motor_RunSync(uint8_t sel, float rev, uint8_t dir, uint16_t delay_us)
{
    uint32_t pulses = (uint32_t)(rev * PULSE_PER_REV + 0.5f);

    sel &= MOTOR_SEL_ALL;               /* 只保留 1/2/3/4 这四位, 防止乱填 */
    if(sel == 0) return;                /* 一个电机都没选 */
    if(pulses == 0) return;

    /* 四个电机的方向脚一起设好(同样按 DIR_INVERT 翻, 电机4 会跟着翻过来) */
    dir = (dir != 0) ? 1u : 0u;

    if(sel & MOTOR_SEL_1) HAL_GPIO_WritePin(MOTOR1_DIR_PORT, MOTOR1_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR1_DIR_INVERT));
    if(sel & MOTOR_SEL_2) HAL_GPIO_WritePin(MOTOR2_DIR_PORT, MOTOR2_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR2_DIR_INVERT));
    if(sel & MOTOR_SEL_3) HAL_GPIO_WritePin(MOTOR3_DIR_PORT, MOTOR3_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR3_DIR_INVERT));  /* 电机3 方向脚 */
    if(sel & MOTOR_SEL_4) HAL_GPIO_WritePin(MOTOR4_DIR_PORT, MOTOR4_DIR_PIN, (GPIO_PinState)(dir ^ MOTOR4_DIR_INVERT));  /* 电机4 方向脚 */

    /* 方向建立时间: 等一小会儿再发脉冲, 让驱动器先认好方向 */
    for(volatile int i=0; i<10; i++) __NOP();

    /* 同一个循环里给选中的电机一起发脉冲 -> 完全同步 */
    for(uint32_t i=0; i<pulses; i++)
    {
        /* 一起输出高电平 */
        if(sel & MOTOR_SEL_1) HAL_GPIO_WritePin(MOTOR1_PUL_PORT, MOTOR1_PUL_PIN, GPIO_PIN_SET);
        if(sel & MOTOR_SEL_2) HAL_GPIO_WritePin(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN, GPIO_PIN_SET);
        if(sel & MOTOR_SEL_3) HAL_GPIO_WritePin(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN, GPIO_PIN_SET);    /* 电机3 脉冲脚 */
        if(sel & MOTOR_SEL_4) HAL_GPIO_WritePin(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN, GPIO_PIN_SET);    /* 电机4 脉冲脚 */

        /* 高电平要保持够久, 驱动器(光耦)才认得出这一步 */
        for(volatile int j=0; j<PULSE_HIGH_CNT; j++) __NOP();

        /* 一起输出低电平(产生下降沿) */
        if(sel & MOTOR_SEL_1) HAL_GPIO_WritePin(MOTOR1_PUL_PORT, MOTOR1_PUL_PIN, GPIO_PIN_RESET);
        if(sel & MOTOR_SEL_2) HAL_GPIO_WritePin(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN, GPIO_PIN_RESET);
        if(sel & MOTOR_SEL_3) HAL_GPIO_WritePin(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN, GPIO_PIN_RESET);  /* 电机3 脉冲脚 */
        if(sel & MOTOR_SEL_4) HAL_GPIO_WritePin(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN, GPIO_PIN_RESET);  /* 电机4 脉冲脚 */

        /* 脉冲间隔延时: 和 Motor_Run() 用的是同一套节奏, 所以频率不变 */
        uint32_t delay = delay_us * 12;
        for(volatile int j=0; j<delay; j++) __NOP();
    }
}

/* ==========================================================================
 * 调试用: 12 个引脚的引脚自检
 *
 * 把 12 个脚依次慢慢闪烁: 每个脚闪 3 下(闪 0.25 秒、灭 0.25 秒),
 * 脚和脚之间停 0.7 秒, 一直循环。
 *
 * 因为正式程序里脉冲是 2kHz 的窄脉冲, 万用表量出来几乎是 0V, 灯珠也几乎不亮,
 * 光靠量电压分不清"是单片机没输出"还是"驱动器/接线的问题"。
 * 用这个函数就能一眼看出来: 哪个脚不闪, 就是那个脚的问题。
 *
 * 用法: 把 Core/Src/APP.c 里的 MOTOR_PIN_TEST 改成 1, 烧录后就会进这里
 *       (进去就不出来了, 正常现象; 测完改回 0 重新烧录)。
 * ========================================================================== */
static void Motor_BlinkPin(GPIO_TypeDef *port, uint16_t pin, uint8_t times)
{
    uint8_t k;

    for(k=0; k<times; k++)
    {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
        HAL_Delay(250);
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
        HAL_Delay(250);
    }

    HAL_Delay(700);                     /* 脚和脚之间停一下, 方便数 */
}

void Motor_PinTest(void)
{
    while(1)
    {
        Motor_BlinkPin(MOTOR1_PUL_PORT, MOTOR1_PUL_PIN, 3);   /* 电机1 脉冲 */
        Motor_BlinkPin(MOTOR1_DIR_PORT, MOTOR1_DIR_PIN, 3);   /* 电机1 方向 */
        Motor_BlinkPin(MOTOR1_EN_PORT,  MOTOR1_EN_PIN,  3);   /* 电机1 使能 */

        Motor_BlinkPin(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN, 3);   /* 电机2 脉冲 */
        Motor_BlinkPin(MOTOR2_DIR_PORT, MOTOR2_DIR_PIN, 3);   /* 电机2 方向 */
        Motor_BlinkPin(MOTOR2_EN_PORT,  MOTOR2_EN_PIN,  3);   /* 电机2 使能 */

        Motor_BlinkPin(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN, 3);   /* 电机3 脉冲 */
        Motor_BlinkPin(MOTOR3_DIR_PORT, MOTOR3_DIR_PIN, 3);   /* 电机3 方向 */
        Motor_BlinkPin(MOTOR3_EN_PORT,  MOTOR3_EN_PIN,  3);   /* 电机3 使能 */

        Motor_BlinkPin(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN, 3);   /* 电机4 脉冲 */
        Motor_BlinkPin(MOTOR4_DIR_PORT, MOTOR4_DIR_PIN, 3);   /* 电机4 方向 */
        Motor_BlinkPin(MOTOR4_EN_PORT,  MOTOR4_EN_PIN,  3);   /* 电机4 使能 */
    }
}
