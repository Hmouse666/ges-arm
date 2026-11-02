#include "servo.h"

/* ==========================================================================
 * 舵机驱动实现
 *
 * 这里做了两次换算:
 *   1) 角度 -> 脉宽:   脉宽(us) = 500 + (2500 - 500) × 角度 / 180
 *   2) 脉宽 -> 计数值: 计数值   = 脉宽(us) × 4096 / 20000
 *      (50Hz 一个周期是 20000us, 芯片把每个周期分成 4096 份)
 *
 * 缓动(慢慢转)是怎么实现的:
 *   每个通道记 4 样东西: 现在多少度、目标多少度、一步走几度、几步之间隔多少毫秒。
 *   主循环调用 Servo_MoveUpdate() 时, 谁"该动而且到时间了", 就让它的角度走一小步,
 *   所以舵机是一小步一小步挪过去的, 不会一下子甩到位。
 * ========================================================================== */

#define PWM_PERIOD_US   20000       /* 50Hz: 1000000us / 50 = 20000us */
#define PWM_MAX         4096        /* 芯片是 12 位, 一个周期分成 4096 份 */

static uint16_t g_min_us = 500;     /* 0 度对应的脉宽 */
static uint16_t g_max_us = 2500;    /* 180 度对应的脉宽 */
static uint8_t  g_ready  = 0;       /* 1 = 初始化成功 */

/* ---- 缓动用的数组, 下标就是通道号 0~15 ---- */
static uint8_t  s_now[SERVO_CH_NUM];        /* 现在(库认为)在多少度 */
static uint8_t  s_target[SERVO_CH_NUM];     /* 目标角度 */
static uint8_t  s_step[SERVO_CH_NUM];       /* 一步走几度 */
static uint16_t s_period[SERVO_CH_NUM];     /* 两步之间隔多少毫秒 */
static uint32_t s_tick[SERVO_CH_NUM];       /* 上一次动的时间 */
static uint8_t  s_slow_on[SERVO_CH_NUM];    /* 1 = 这一路正在慢慢转 */

/* ============================== 内部小工具 ============================== */

/**
  * @brief  把"填写用的角度(-90~+90)"换成"芯片用的绝对角度(0~180)"
  * @param  angle : 0 = 中位; 正数 = 往一边转; 负数 = 往另一边转
  * @retval 绝对角度 0~180(填得太大或太小会自动卡在两头)
  */
static uint8_t Servo_ToAbs(int16_t angle)
{
    int16_t abs_angle;

    if (angle > SERVO_ANGLE_MAX)
    {
        angle = SERVO_ANGLE_MAX;                /* 限幅, 防止舵机顶到机械限位 */
    }
    if (angle < SERVO_ANGLE_MIN)
    {
        angle = SERVO_ANGLE_MIN;
    }

    abs_angle = (int16_t)(SERVO_CENTER_ANGLE + angle);

    if (abs_angle < 0)
    {
        abs_angle = 0;
    }
    if (abs_angle > 180)
    {
        abs_angle = 180;
    }

    return (uint8_t)abs_angle;
}

/**
  * @brief  按角度发一次脉冲(内部用)
  */
static void Servo_Output(uint8_t ch, uint8_t angle)
{
    uint16_t us;

    if (angle > 180)
    {
        angle = 180;                            /* 限幅, 防止舵机顶到机械限位 */
    }

    us = g_min_us + (uint16_t)((uint32_t)(g_max_us - g_min_us) * angle / 180);

    Servo_SetPulse(ch, us);
}

/* ============================== 初始化 ============================== */

/**
  * @brief  舵机库初始化
  * @retval 0 = 成功, 1 = 失败(PCA9685 没应答)
  * @note   初始化后 16 路都不发脉冲(舵机没受力), 库默认认为每个舵机都在 90 度(中位);
  *         想让某个舵机先定在某个角度, 之后再用 Servo_SetStartAngle() 就行。
  */
uint8_t Servo_Init(void)
{
    uint8_t ch;

    g_ready = 0;

    if (PCA9685_Init() != 0)
    {
        return 1;
    }

    for (ch = 0; ch < SERVO_CH_NUM; ch++)
    {
        s_now[ch]     = 90;                     /* 默认当中位 */
        s_target[ch]  = 90;
        s_step[ch]    = 1;
        s_period[ch]  = 10;
        s_tick[ch]    = 0;
        s_slow_on[ch] = 0;                      /* 先不缓动 */
    }

    g_ready = 1;
    Servo_OffAll();                 /* 初始化完先不出脉冲, 等 APP 里给角度 */

    return 0;
}

/**
  * @brief  修改脉宽范围(0 度和 180 度对应的高电平时间)
  * @note   一般不用改, 舵机行程不够或者一直"吱吱"响的时候再标定
  */
void Servo_SetRange(uint16_t min_us, uint16_t max_us)
{
    if (max_us > min_us)
    {
        g_min_us = min_us;
        g_max_us = max_us;
    }
}

/* ============================== 立刻转 ============================== */

/**
  * @brief  让某个舵机立刻转到指定角度
  * @param  ch    : 通道 0~15
  * @param  angle : 角度 -90~+90(0 = 中位, 正数正着转, 负数反着转), 超过范围会自动卡住
  * @note   例如  0 度 -> 1500us(中位)
  *              30 度 -> 1800us
  *             -30 度 -> 1200us
  *         想让它慢慢转请用 Servo_SetAngleSlow()
  */
void Servo_SetAngle(uint8_t ch, int16_t angle)
{
    uint8_t abs_angle;

    if (ch >= SERVO_CH_NUM)
    {
        return;
    }

    abs_angle = Servo_ToAbs(angle);         /* 先换算成芯片要的 0~180 */

    s_slow_on[ch] = 0;              /* 立刻转: 关掉这一路的缓动 */
    s_now[ch]     = abs_angle;      /* 库记住"现在在这里" */
    s_target[ch]  = abs_angle;

    Servo_Output(ch, abs_angle);
}

/**
  * @brief  直接按脉宽控制(调试时用, 例如 1500us 是中位)
  */
void Servo_SetPulse(uint8_t ch, uint16_t us)
{
    uint32_t ticks;

    if (g_ready == 0)
    {
        return;                                 /* 初始化失败就不动作 */
    }

    if (us > PWM_PERIOD_US)
    {
        us = PWM_PERIOD_US;                     /* 不能超过一个周期 */
    }

    /* 脉宽换算成 4096 份里的第几份 */
    ticks = (uint32_t)us * PWM_MAX / PWM_PERIOD_US;

    if (ticks > 4095)
    {
        ticks = 4095;
    }

    PCA9685_SetPWM(ch, (uint16_t)ticks);
}

/* ============================== 慢慢转(缓动) ============================== */

/**
  * @brief  让舵机停在指定角度, 并把这个角度记成"缓动起点"
  * @param  angle : -90~+90(0 = 中位)
  * @note   和 Servo_SetAngle() 的区别:
  *           Servo_SetAngle()      转过去, 并且把缓动关掉;
  *           Servo_SetStartAngle() 也是转过去, 但意义是"从这里出发",
  *           一般用在 Servo_Init() 之后, 让机械臂先定在初始姿势。
  */
void Servo_SetStartAngle(uint8_t ch, int16_t angle)
{
    uint8_t abs_angle;

    if (ch >= SERVO_CH_NUM)
    {
        return;
    }

    abs_angle = Servo_ToAbs(angle);

    s_now[ch]     = abs_angle;
    s_target[ch]  = abs_angle;
    s_slow_on[ch] = 0;

    Servo_Output(ch, abs_angle);                /* 立刻按这个角度发脉冲, 舵机就停在这里 */
}

/**
  * @brief  设定缓动目标: 从当前位置出发, 一步一步转到 angle
  * @param  ch       : 通道 0~15
  * @param  angle    : 目标角度 -90~+90(0 = 中位, 正数正着转, 负数反着转)
  * @param  step_deg : 一步转几度, 越小越慢越柔
  * @param  step_ms  : 两步之间隔多少毫秒
  * @note   例如 step_deg=1, step_ms=10 就是"每 10ms 转 1 度"= 100 度/秒。
  *         这个函数只登记目标, 真正的转动在 Servo_MoveUpdate() 里发生。
  *         正在缓动时再叫一次, 就是改目标(用来跟着手走)。
  */
void Servo_SetAngleSlow(uint8_t ch, int16_t angle, uint8_t step_deg, uint16_t step_ms)
{
    uint8_t abs_angle;

    if (ch >= SERVO_CH_NUM)
    {
        return;
    }

    abs_angle = Servo_ToAbs(angle);

    if (step_deg == 0)
    {
        step_deg = 1;
    }

    if (step_ms == 0)
    {
        step_ms = 1;
    }

    s_target[ch]  = abs_angle;
    s_step[ch]    = step_deg;
    s_period[ch]  = step_ms;
    s_tick[ch]    = HAL_GetTick();
    s_slow_on[ch] = 1;
}

/**
  * @brief  缓动发动机: 主循环里反复调用, 每次给"该动"的通道挪一小步
  */
void Servo_MoveUpdate(void)
{
    uint8_t ch;
    uint8_t angle;

    for (ch = 0; ch < SERVO_CH_NUM; ch++)
    {
        if (s_slow_on[ch] == 0)
        {
            continue;                           /* 这一路不用缓动 */
        }

        if ((HAL_GetTick() - s_tick[ch]) < s_period[ch])
        {
            continue;                           /* 还没到该动的时间 */
        }

        s_tick[ch] = HAL_GetTick();
        angle      = s_now[ch];

        if (angle < s_target[ch])               /* 往"大"的方向走一小步 */
        {
            if (((uint16_t)angle + s_step[ch]) >= s_target[ch])
            {
                angle = s_target[ch];           /* 一步就能到, 直接到位 */
            }
            else
            {
                angle = (uint8_t)(angle + s_step[ch]);
            }
        }
        else if (angle > s_target[ch])          /* 往"小"的方向走一小步 */
        {
            if (((uint16_t)angle - s_step[ch]) <= s_target[ch])
            {
                angle = s_target[ch];
            }
            else
            {
                angle = (uint8_t)(angle - s_step[ch]);
            }
        }
        else
        {
            s_slow_on[ch] = 0;                  /* 已经到位, 这一路收工 */
            continue;
        }

        s_now[ch] = angle;
        Servo_Output(ch, angle);                /* 发一小步脉冲 */
    }
}

/**
  * @brief  现在(库认为)在多少度
  * @retval -90~+90(0 = 中位), 和你在 APP.c 里填的角度是同一套
  */
int16_t Servo_GetAngle(uint8_t ch)
{
    if (ch >= SERVO_CH_NUM)
    {
        return 0;
    }

    return (int16_t)((int16_t)s_now[ch] - SERVO_CENTER_ANGLE);
}

/**
  * @brief  这一路还在慢慢转吗
  * @retval 1 = 还在转, 0 = 已经到位(或这一路没在缓动)
  */
uint8_t Servo_IsMoving(uint8_t ch)
{
    if (ch >= SERVO_CH_NUM)
    {
        return 0;
    }

    return s_slow_on[ch];
}

/* ============================== 停止输出 ============================== */

/**
  * @brief  让某个舵机停止受力(不再发脉冲, 舵机会自由下垂)
  */
void Servo_Off(uint8_t ch)
{
    if (ch < SERVO_CH_NUM)
    {
        s_slow_on[ch] = 0;                      /* 松开就别再缓动了 */
    }

    PCA9685_SetPWM(ch, PWM_MAX);                /* 写 4096 就是不发脉冲 */
}

/**
  * @brief  16 路全部停止
  */
void Servo_OffAll(void)
{
    uint8_t ch;

    for (ch = 0; ch < SERVO_CH_NUM; ch++)
    {
        s_slow_on[ch] = 0;
    }

    PCA9685_AllOff();
}
