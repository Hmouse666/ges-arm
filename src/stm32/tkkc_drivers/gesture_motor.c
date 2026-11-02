/**
  ******************************************************************************
  * @file    gesture_motor.c
  * @brief   电机随手势驱动: X/深度轴跟随、步进脉冲和 TIM3 中断。
  ******************************************************************************
  */

#include "main.h"
#include "fpga_gesture.h"
#include "motor.h"
#include "gesture_motor.h"
#include "task2.h"

/* ==================== X 轴跟随参数 ==================== */

#define M1_SPEED_US             50      /* 电机1 脉冲间隔(us) */
#define X_FOLLOW_UNIT           640.0f  /* X 满量程单位数 */
#define M1_TURNS_PER_UNIT       4.0f    /* X 满量程对应电机圈数 */
#define M1_X_INVERT             1       /* 电机1 方向取反 */
#define M4_Y_INVERT             0       /* 深度轴方向取反 */

#define X_LPF_A                 0.25f   /* X 低通滤波系数 */
#define X_DEADBAND              12       /* X 死区(单位) */
#define X_DEPTH_GUARD_UNITS     30      /* 深度运动时的 X 守卫死区(单位) */
#define X_DEPTH_GUARD_CONFIRM   10       /* X 抢占深度所需连续帧数 */
#define X_SLEW_PPS              13000   /* X 指令位置限速(脉冲/秒) */
#define X_REV_THRESH            10      /* X 反向判定阈值(单位) */
#define X_MAX_LAG_UNITS         60      /* X 最大欠账(单位) */

#define X_PULSES_PER_UNIT       (M1_TURNS_PER_UNIT * (float)PULSE_PER_REV / X_FOLLOW_UNIT)

/* ==================== 深度轴跟随参数 ==================== */

#define M4_SPEED_US             50      /* 电机4 脉冲间隔(us) */
#define Y_FOLLOW_UNIT           100.0f  /* 深度满量程单位数 */
#define Y_BASE_TURNS_PER_UNIT   3.5f    /* 深度满量程的基准圈数 */
#define Y_FORWARD_GAIN_PERCENT  80u    /* 电机2(向前): 与基准同速 */
#define Y_DOWN_GAIN_PERCENT     110u    /* 电机4(向下): 与基准同速 */
/* 保持改成双通道以前两路电机的相对方向不变。 */
#define Y_FORWARD_DIR_INVERT     1
#define Y_DOWN_DIR_INVERT        0

#define Y_LPF_A                 0.55f   /* 深度低通滤波系数 */
#define Y_DEADBAND              0.8f    /* 深度死区(单位) */
/* 两路同为 100%, 深度基准限速恢复为 12000 脉冲/秒。 */
#define Y_SLEW_PPS              13000
#define Y_REV_THRESH            1       /* 深度反向判定阈值(单位) */
#define Y_MAX_LAG_UNITS         8       /* 深度最大欠账(单位) */

#define Y_BASE_PULSES_PER_UNIT  (Y_BASE_TURNS_PER_UNIT * (float)PULSE_PER_REV / Y_FOLLOW_UNIT)

#define Y_STOP_WHEN_NO_Y        1       /* 1 = 深度数据不新鲜时停止 */
#define Y_ZERO_IS_NOHAND        1       /* 1 = 深度为 0 时视为无手 */

/* ==================== 轴优先级参数 ==================== */

#define AXIS_ACTIVE_MS          120     /* 最近发脉冲后仍算忙碌的时间(ms) */
#define AXIS_OWNER_NONE         0       /* 无轴占用 */
#define AXIS_OWNER_X            1       /* X 轴占用 */

/* ==================== 显示和调试参数 ==================== */

#define APP_LCD_IDLE_MS         300     /* 停止后允许刷新的时间(ms) */
#define MOTOR_PIN_TEST          0       /* 1 = 电机引脚自检 */
#define X_STOP_WHEN_NO_X        1       /* 1 = X 数据不新鲜时停止 */

/* ==================== 脉冲定时器参数 ==================== */

#define STEP_TIMER_HZ           13000u
#define STEP_TIMER_ARR          ((72000000u / STEP_TIMER_HZ) - 1u)
#define STEP_TARGET_DEADBAND    1.0f    /* 目标位置死区(脉冲) */

/* ==================== 跟随状态 ==================== */

typedef struct
{
    uint8_t  ready;         /* 1 = 已建立零点 */
    float    start;         /* 零点 */
    float    lpf;           /* 低通滤波值 */
    float    acc;           /* 死区后的输入值 */
    float    cmd;           /* 指令位置(脉冲) */
    float    pos;           /* 实际位置(脉冲) */
    int32_t  prev;          /* 上一帧输入值 */
    int32_t  delta;         /* 上一段有效变化量 */
    uint8_t  prev_ok;       /* 1 = prev/delta 有效 */
    uint32_t last_us;       /* 上次更新时间(us) */
} AxisFollow;

static AxisFollow s_x_follow;               /* X 轴跟随状态 */
static AxisFollow s_d_follow;               /* 深度轴跟随状态 */

static uint8_t  s_x_depth_guard;            /* 1 = X 处于深度守卫状态 */
static int32_t  s_x_depth_anchor;           /* 守卫基准 X */
static uint8_t  s_x_guard_out_cnt;          /* 连续超出守卫次数 */
static uint8_t  s_x_depth_override;         /* 1 = 本次深度运动 X 已抢占 */
static uint8_t  s_axis_owner = AXIS_OWNER_NONE;

static volatile int32_t s_x_steps_target;   /* X 目标脉冲数 */
static volatile int32_t s_x_steps_done;     /* X 已完成脉冲数 */
static volatile int32_t s_forward_steps_target; /* 电机2(向前)目标脉冲数 */
static volatile int32_t s_forward_steps_done;   /* 电机2(向前)已完成脉冲数 */
static volatile int32_t s_down_steps_target;    /* 电机4(向下)目标脉冲数 */
static volatile int32_t s_down_steps_done;      /* 电机4(向下)已完成脉冲数 */

static volatile uint32_t s_x_last_move_ms;  /* X 最近发脉冲时间(ms) */
static volatile uint32_t s_depth_last_move_ms; /* 深度轴最近发脉冲时间(ms) */

static uint8_t s_x_step_dir;                /* X 当前方向 */
static uint8_t s_x_step_dir_ok;             /* 1 = X 方向已建立 */
static uint8_t s_x_step_settle;             /* 1 = X 等待方向建立 */
static volatile uint8_t s_forward_step_dir; /* 电机2当前方向 */
static volatile uint8_t s_forward_step_dir_ok; /* 1 = 电机2方向已建立 */
static volatile uint8_t s_down_step_dir;    /* 电机4当前方向 */
static volatile uint8_t s_down_step_dir_ok; /* 1 = 电机4方向已建立 */
static volatile uint8_t s_depth_step_settle; /* 1 = 深度方向建立等待 */
static volatile uint16_t s_depth_gate_phase; /* 深度两路脉冲比例分配相位 */

/* ==================== 步进脉冲输出 ==================== */

/* 输出一个 X 脉冲。 */
static void step_x_pulse(void)
{
    MOTOR1_PUL_PORT->BSRR = MOTOR1_PUL_PIN;
    for (volatile int i = 0; i < PULSE_HIGH_CNT; i++)
    {
        __NOP();
    }
    MOTOR1_PUL_PORT->BRR = MOTOR1_PUL_PIN;
}

/* 按本次分配结果输出电机2/电机4 脉冲。 */
static void step_depth_pulse(uint8_t forward, uint8_t down)
{
    if (forward != 0u)
    {
        MOTOR2_PUL_PORT->BSRR = MOTOR2_PUL_PIN;
    }
    if (down != 0u)
    {
        MOTOR4_PUL_PORT->BSRR = MOTOR4_PUL_PIN;
    }
    for (volatile int i = 0; i < PULSE_HIGH_CNT; i++)
    {
        __NOP();
    }
    if (forward != 0u)
    {
        MOTOR2_PUL_PORT->BRR = MOTOR2_PUL_PIN;
    }
    if (down != 0u)
    {
        MOTOR4_PUL_PORT->BRR = MOTOR4_PUL_PIN;
    }
}

/* 设置 X 方向。 */
static void step_x_set_dir(uint8_t dir)
{
    uint8_t level = (uint8_t)((dir ^ M1_X_INVERT) ^ MOTOR1_DIR_INVERT);

    if (level != 0u)
    {
        MOTOR1_DIR_PORT->BSRR = MOTOR1_DIR_PIN;
    }
    else
    {
        MOTOR1_DIR_PORT->BRR = MOTOR1_DIR_PIN;
    }
}

/* 设置电机2方向。 */
static void step_forward_set_dir(uint8_t dir)
{
    uint8_t level = (uint8_t)((dir ^ M4_Y_INVERT) ^ MOTOR2_DIR_INVERT ^
                              Y_FORWARD_DIR_INVERT);

    if (level != 0u)
    {
        MOTOR2_DIR_PORT->BSRR = MOTOR2_DIR_PIN;
    }
    else
    {
        MOTOR2_DIR_PORT->BRR = MOTOR2_DIR_PIN;
    }
}

/* 设置电机4方向。 */
static void step_down_set_dir(uint8_t dir)
{
    uint8_t level = (uint8_t)((dir ^ M4_Y_INVERT) ^ MOTOR4_DIR_INVERT ^
                              Y_DOWN_DIR_INVERT);

    if (level != 0u)
    {
        MOTOR4_DIR_PORT->BSRR = MOTOR4_DIR_PIN;
    }
    else
    {
        MOTOR4_DIR_PORT->BRR = MOTOR4_DIR_PIN;
    }
}

/* X 轴定时器中断处理。 */
static void step_x_isr(void)
{
    int32_t err = s_x_steps_target - s_x_steps_done;
    uint8_t dir;

    if (err == 0)
    {
        s_x_step_settle = 0u;
        return;
    }

    dir = (err > 0) ? 0u : 1u;

    if ((s_x_step_dir_ok == 0u) || (s_x_step_dir != dir))
    {
        step_x_set_dir(dir);
        s_x_step_dir     = dir;
        s_x_step_dir_ok  = 1u;
        s_x_step_settle  = 1u;
        return;
    }

    if (s_x_step_settle != 0u)
    {
        s_x_step_settle = 0u;
        return;
    }

    step_x_pulse();
    if (dir == 0u)
    {
        s_x_steps_done++;
    }
    else
    {
        s_x_steps_done--;
    }
    s_x_last_move_ms = HAL_GetTick();
}

/* 深度轴定时器中断处理: 两路独立计数, 按增益比例分配脉冲。 */
static void step_depth_isr(void)
{
    int32_t forward_err = s_forward_steps_target - s_forward_steps_done;
    int32_t down_err = s_down_steps_target - s_down_steps_done;
    uint8_t forward_active = (forward_err != 0) ? 1u : 0u;
    uint8_t down_active = (down_err != 0) ? 1u : 0u;
    uint8_t forward_dir = 0u;
    uint8_t down_dir = 0u;
    uint8_t forward_pulse = 0u;
    uint8_t down_pulse = 0u;
    uint8_t changed = 0u;

    if ((forward_active == 0u) && (down_active == 0u))
    {
        s_depth_step_settle = 0u;
        s_depth_gate_phase = 0u;
        return;
    }

    if (forward_active != 0u)
    {
        forward_dir = (forward_err > 0) ? 0u : 1u;

        if ((s_forward_step_dir_ok == 0u) ||
            (s_forward_step_dir != forward_dir))
        {
            step_forward_set_dir(forward_dir);
            s_forward_step_dir = forward_dir;
            s_forward_step_dir_ok = 1u;
            changed = 1u;
        }
    }

    if (down_active != 0u)
    {
        down_dir = (down_err > 0) ? 0u : 1u;

        if ((s_down_step_dir_ok == 0u) ||
            (s_down_step_dir != down_dir))
        {
            step_down_set_dir(down_dir);
            s_down_step_dir = down_dir;
            s_down_step_dir_ok = 1u;
            changed = 1u;
        }
    }

    if (changed != 0u)
    {
        s_depth_step_settle = 1u;
        return;
    }

    if (s_depth_step_settle != 0u)
    {
        s_depth_step_settle = 0u;
        return;
    }

    if ((forward_active != 0u) && (down_active != 0u))
    {
        if (Y_FORWARD_GAIN_PERCENT >= Y_DOWN_GAIN_PERCENT)
        {
            forward_pulse = 1u;
            s_depth_gate_phase = (uint16_t)
                (s_depth_gate_phase + Y_DOWN_GAIN_PERCENT);
            if (s_depth_gate_phase >= Y_FORWARD_GAIN_PERCENT)
            {
                s_depth_gate_phase = (uint16_t)
                    (s_depth_gate_phase - Y_FORWARD_GAIN_PERCENT);
                down_pulse = 1u;
            }
        }
        else
        {
            down_pulse = 1u;
            s_depth_gate_phase = (uint16_t)
                (s_depth_gate_phase + Y_FORWARD_GAIN_PERCENT);
            if (s_depth_gate_phase >= Y_DOWN_GAIN_PERCENT)
            {
                s_depth_gate_phase = (uint16_t)
                    (s_depth_gate_phase - Y_DOWN_GAIN_PERCENT);
                forward_pulse = 1u;
            }
        }
    }
    else
    {
        forward_pulse = forward_active;
        down_pulse = down_active;
        s_depth_gate_phase = 0u;
    }

    if ((forward_pulse == 0u) && (down_pulse == 0u))
    {
        return;
    }

    step_depth_pulse(forward_pulse, down_pulse);

    if (forward_pulse != 0u)
    {
        s_forward_steps_done += (forward_dir == 0u) ? 1 : -1;
    }
    if (down_pulse != 0u)
    {
        s_down_steps_done += (down_dir == 0u) ? 1 : -1;
    }
    s_depth_last_move_ms = HAL_GetTick();
}

/* TIM3 更新中断。 */
void TIM3_IRQHandler(void)
{
    if (Task2_TimerPulseActive() != 0u)
    {
        Task2_Timer3PulseIsr();
        return;
    }

    if ((TIM3->SR & TIM_SR_UIF) != 0u)
    {
        TIM3->SR = (uint16_t)~TIM_SR_UIF;
        step_x_isr();
        step_depth_isr();
    }
}

/* 初始化 TIM3 脉冲定时器。 */
static void step_timer_init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();

    TIM3->CR1  = 0u;
    TIM3->PSC  = 0u;
    TIM3->ARR  = STEP_TIMER_ARR;
    TIM3->CNT  = 0u;
    TIM3->EGR  = TIM_EGR_UG;
    TIM3->SR   = 0u;
    TIM3->DIER = TIM_DIER_UIE;

    NVIC_SetPriority(TIM3_IRQn, 0u);
    NVIC_EnableIRQ(TIM3_IRQn);

    TIM3->CR1 = TIM_CR1_CEN;
}

void GestureMotor_RestartTimer(void)
{
    step_timer_init();
}

/* 根据当前完成位置计算四舍五入后的目标。调用者负责关闭中断。 */
static int32_t step_target_from_done(int32_t done, float cmd)
{
    float   pos = (float)done;
    int32_t t;

    if (cmd > (pos + STEP_TARGET_DEADBAND))
    {
        t = (int32_t)(cmd + 0.5f);
    }
    else if (cmd < (pos - STEP_TARGET_DEADBAND))
    {
        t = (int32_t)(cmd - 0.5f);
    }
    else
    {
        t = done;
    }
    return t;
}

/* 设置 X 轴目标脉冲数。 */
static void step_set_target(volatile int32_t *target, volatile int32_t *done, float cmd)
{
    __disable_irq();
    *target = step_target_from_done(*done, cmd);
    __enable_irq();
}

/* 将目标位置对齐到当前位置。 */
static void step_stop(volatile int32_t *target, volatile int32_t *done)
{
    __disable_irq();
    *target = *done;
    __enable_irq();
}

/* 将目标位置和已完成位置清零。 */
static void step_reset(volatile int32_t *target, volatile int32_t *done)
{
    __disable_irq();
    *target = 0;
    *done   = 0;
    __enable_irq();
}

/* 将深度两路目标同时对齐到各自当前位置。 */
static void step_depth_stop(void)
{
    __disable_irq();
    s_forward_steps_target = s_forward_steps_done;
    s_down_steps_target = s_down_steps_done;
    __enable_irq();
}

/* 将深度两路目标与完成位置同时清零。 */
static void step_depth_reset(void)
{
    __disable_irq();
    s_forward_steps_target = 0;
    s_forward_steps_done = 0;
    s_down_steps_target = 0;
    s_down_steps_done = 0;
    __enable_irq();
}

/* 从同一个深度基准指令计算电机2和电机4各自的目标。 */
static void step_set_depth_targets(float base_cmd)
{
    float forward_cmd = base_cmd *
        ((float)Y_FORWARD_GAIN_PERCENT / 100.0f);
    float down_cmd = base_cmd *
        ((float)Y_DOWN_GAIN_PERCENT / 100.0f);

    __disable_irq();
    s_forward_steps_target =
        step_target_from_done(s_forward_steps_done, forward_cmd);
    s_down_steps_target =
        step_target_from_done(s_down_steps_done, down_cmd);
    __enable_irq();
}

/* ==================== 微秒计时 ==================== */

static uint8_t s_us_use_dwt;        /* 1 = DWT 周期计数器可用 */

/* 初始化微秒计时。 */
static void app_us_init(void)
{
    uint32_t a;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    a = DWT->CYCCNT;
    for (volatile int i = 0; i < 2000; i++)
    {
        __NOP();
    }
    s_us_use_dwt = (DWT->CYCCNT != a) ? 1u : 0u;
}

/* 读取当前微秒时间。 */
static uint32_t app_us(void)
{
    if (s_us_use_dwt != 0u)
    {
        return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
    }

    return (HAL_GetTick() * 1000u) +
           ((SysTick->LOAD - SysTick->VAL) / (SystemCoreClock / 1000000u));
}

/* ==================== 跟随公共处理 ==================== */

/* 检测输入方向是否反转。 */
static uint8_t axis_reverse(int32_t value, int32_t *prev, uint8_t *prev_ok,
                            int32_t *delta, int32_t threshold)
{
    int32_t  dv = 0;
    uint8_t  rev = 0u;

    if (*prev_ok != 0u)
    {
        dv = value - *prev;

        if (dv != 0)
        {
            if (((dv >= threshold) && (*delta <= -threshold)) ||
                ((dv <= -threshold) && (*delta >= threshold)))
            {
                rev = 1u;
            }

            if ((dv >= threshold) || (dv <= -threshold))
            {
                *delta = dv;
            }
        }
    }

    *prev = value;
    *prev_ok = 1u;
    return rev;
}

/* 计算距上次更新的时间，限制在 1~200000us。 */
static uint32_t axis_elapsed_us(uint32_t now, uint32_t *last)
{
    uint32_t dt = now - *last;

    if (dt < 1u)
    {
        dt = 1u;
    }
    if (dt > 200000u)
    {
        dt = 200000u;
    }

    *last = now;
    return dt;
}

/* 低通滤波、方向反转处理和更新时间。 */
static uint32_t axis_filter(AxisFollow *a, int32_t value, float alpha,
                            int32_t rev_threshold, uint32_t us)
{
    uint8_t rev = axis_reverse(value, &a->prev, &a->prev_ok, &a->delta, rev_threshold);

    if (rev != 0u)
    {
        a->lpf = (float)value;
        a->acc = (float)value;
    }
    else
    {
        a->lpf += alpha * ((float)value - a->lpf);
    }

    return axis_elapsed_us(us, &a->last_us);
}

/* 死区处理。 */
static void axis_deadband(AxisFollow *a, float deadband)
{
    float d = a->lpf - a->acc;

    if (d >= deadband)
    {
        a->acc = a->lpf - deadband;
    }
    else if (d <= -deadband)
    {
        a->acc = a->lpf + deadband;
    }
}

/* 将跟随状态对齐到当前输入值。 */
static void axis_align(AxisFollow *a, int32_t value, uint32_t us,
                       volatile int32_t *target, volatile int32_t *done)
{
    a->lpf     = (float)value;
    a->acc     = (float)value;
    a->start   = (float)value;
    a->cmd     = 0.0f;
    a->pos     = 0.0f;
    step_reset(target, done);
    a->prev    = value;
    a->prev_ok = 1u;
    a->delta   = 0;
    a->last_us = us;
}

/* 建立新零点。 */
static void axis_set_zero(AxisFollow *a, int32_t value, uint32_t us,
                          volatile int32_t *target, volatile int32_t *done)
{
    a->start   = (float)value;
    a->lpf     = (float)value;
    a->acc     = (float)value;
    a->cmd     = 0.0f;
    a->pos     = 0.0f;
    step_reset(target, done);
    a->prev    = value;
    a->prev_ok = 1u;
    a->delta   = 0;
    a->last_us = us;
    a->ready   = 1u;
}

/* 保持当前位置并清除欠账。 */
static void axis_hold(AxisFollow *a, uint32_t us,
                      volatile int32_t *target, volatile int32_t *done)
{
    a->pos     = (float)(*done);
    a->cmd     = a->pos;
    a->prev_ok = 0u;
    a->delta   = 0;
    a->last_us = us;
    step_stop(target, done);
}

/* 丢弃坐标并停止输出。 */
static void axis_forget(AxisFollow *a, uint32_t us,
                        volatile int32_t *target, volatile int32_t *done)
{
    a->ready   = 0u;
    a->prev_ok = 0u;
    a->delta   = 0;
    a->last_us = us;
    step_stop(target, done);
}

/* 将深度跟随状态对齐到当前深度值, 同时清除两路脉冲目标。 */
static void depth_axis_align(AxisFollow *a, int32_t value, uint32_t us)
{
    a->lpf     = (float)value;
    a->acc     = (float)value;
    a->start   = (float)value;
    a->cmd     = 0.0f;
    a->pos     = 0.0f;
    step_depth_reset();
    a->prev    = value;
    a->prev_ok = 1u;
    a->delta   = 0;
    a->last_us = us;
}

/* 为两路深度输出建立共同的零点。 */
static void depth_axis_set_zero(AxisFollow *a, int32_t value, uint32_t us)
{
    a->start   = (float)value;
    a->lpf     = (float)value;
    a->acc     = (float)value;
    a->cmd     = 0.0f;
    a->pos     = 0.0f;
    step_depth_reset();
    a->prev    = value;
    a->prev_ok = 1u;
    a->delta   = 0;
    a->last_us = us;
    a->ready   = 1u;
}

/* 保持深度两路当前位置并清除欠账。 */
static void depth_axis_hold(AxisFollow *a, uint32_t us)
{
    float base_pos = (float)s_forward_steps_done * 100.0f /
                     (float)Y_FORWARD_GAIN_PERCENT;

    a->pos     = base_pos;
    a->cmd     = base_pos;
    a->prev_ok = 0u;
    a->delta   = 0;
    a->last_us = us;
    step_depth_stop();
}

/* 丢弃深度坐标并同时停止两路输出。 */
static void depth_axis_forget(AxisFollow *a, uint32_t us)
{
    a->ready   = 0u;
    a->prev_ok = 0u;
    a->delta   = 0;
    a->last_us = us;
    step_depth_stop();
}

/* 按限速计算基准指令位置。 */
static float axis_interpolate_cmd(AxisFollow *a, uint32_t dt_us,
                                  float pulses_per_unit, float max_lag_units,
                                  uint32_t slew_pps)
{
    float tgt;
    float diff;
    float lag;
    float step;

    tgt  = (a->acc - a->start) * pulses_per_unit;
    diff = tgt - a->cmd;
    lag  = max_lag_units * pulses_per_unit;

    if (diff > lag)
    {
        a->start += (diff - lag) / pulses_per_unit;
        tgt       = a->cmd + lag;
        diff      = lag;
    }
    else if (diff < -lag)
    {
        a->start += (diff + lag) / pulses_per_unit;
        tgt       = a->cmd - lag;
        diff      = -lag;
    }

    step = (float)slew_pps * (float)dt_us / 1000000.0f;

    if (diff > step)
    {
        a->cmd += step;
    }
    else if (diff < -step)
    {
        a->cmd -= step;
    }
    else
    {
        a->cmd += diff;
    }

    return a->cmd;
}

/* 按限速计算指令位置并更新 X 轴脉冲目标。 */
static void axis_interpolate(AxisFollow *a, uint32_t dt_us,
                             float pulses_per_unit, float max_lag_units,
                             uint32_t slew_pps,
                             volatile int32_t *target, volatile int32_t *done)
{
    float cmd = axis_interpolate_cmd(a, dt_us, pulses_per_unit,
                                     max_lag_units, slew_pps);
    step_set_target(target, done, cmd);
}

/* 判断深度轴是否仍在运动。 */
static uint8_t app_d_busy(void)
{
    if ((HAL_GetTick() - s_depth_last_move_ms) < (uint32_t)AXIS_ACTIVE_MS)
    {
        return 1u;
    }
    if ((s_forward_steps_target != s_forward_steps_done) ||
        (s_down_steps_target != s_down_steps_done))
    {
        return 1u;
    }
    return 0u;
}

/* ==================== X 轴跟随 ==================== */

/* 电机1 跟随 X 轴。 */
static void app_motor1_follow_x(void)
{
    int32_t  x;
    int32_t  gdx;
    uint8_t  d_busy;
    uint8_t  hold_x;
    uint8_t  guard_exit;
    uint32_t us;
    uint32_t dt_us;

    us = app_us();
    s_x_follow.pos = (float)s_x_steps_done;

    if ((Gesture_NoHand() != 0) || (Gesture_XLostLong() != 0) ||
        (Gesture_HasData() == 0) || (Gesture_XValid() == 0))
    {
        axis_forget(&s_x_follow, us, &s_x_steps_target, &s_x_steps_done);
        s_x_depth_guard    = 0u;
        s_x_guard_out_cnt  = 0u;
        s_x_depth_override = 0u;
        return;
    }

#if X_STOP_WHEN_NO_X
    if (Gesture_XFresh() == 0)
    {
        axis_hold(&s_x_follow, us, &s_x_steps_target, &s_x_steps_done);
        return;
    }
#endif

    x = Gesture_X();

    if (s_x_follow.ready == 0u)
    {
        axis_set_zero(&s_x_follow, x, us, &s_x_steps_target, &s_x_steps_done);
        s_x_depth_guard    = 0u;
        s_x_guard_out_cnt  = 0u;
        s_x_depth_override = 0u;
        return;
    }

    d_busy     = app_d_busy();
    hold_x     = 0u;
    guard_exit = 0u;

    if ((d_busy != 0u) && (s_x_depth_override != 0u))
    {
        /* X 已抢占本次深度运动，直接跟随。 */
    }
    else if (d_busy != 0u)
    {
        if (s_x_depth_guard == 0u)
        {
            s_x_depth_guard   = 1u;
            s_x_depth_anchor  = x;
            s_x_guard_out_cnt = 0u;
        }

        gdx = x - s_x_depth_anchor;

        if ((gdx <= X_DEPTH_GUARD_UNITS) && (gdx >= -X_DEPTH_GUARD_UNITS))
        {
            s_x_guard_out_cnt = 0u;
            hold_x = 1u;
        }
        else
        {
            if (s_x_guard_out_cnt < 255u)
            {
                s_x_guard_out_cnt++;
            }

            if (s_x_guard_out_cnt < X_DEPTH_GUARD_CONFIRM)
            {
                hold_x = 1u;
            }
            else
            {
                s_x_depth_guard    = 0u;
                s_x_guard_out_cnt  = 0u;
                s_x_depth_override = 1u;
                guard_exit         = 1u;
            }
        }
    }
    else
    {
        s_x_depth_override = 0u;

        if (s_x_depth_guard != 0u)
        {
            s_x_depth_guard   = 0u;
            s_x_guard_out_cnt = 0u;
            hold_x            = 1u;
        }
    }

    if (hold_x != 0u)
    {
        axis_align(&s_x_follow, x, us, &s_x_steps_target, &s_x_steps_done);
        return;
    }

    if (guard_exit != 0u)
    {
        float edge;
        float guard_span = (float)(X_DEPTH_GUARD_UNITS - X_DEADBAND);

        if (gdx >= 0)
        {
            edge = (float)s_x_depth_anchor + guard_span;
        }
        else
        {
            edge = (float)s_x_depth_anchor - guard_span;
        }

        s_x_follow.start   = edge;
        s_x_follow.lpf     = edge;
        s_x_follow.acc     = edge;
        s_x_follow.cmd     = 0.0f;
        s_x_follow.pos     = 0.0f;
        s_x_follow.prev    = x;
        s_x_follow.prev_ok = 1u;
        s_x_follow.delta   = 0;
        s_x_follow.last_us = us;
        step_reset(&s_x_steps_target, &s_x_steps_done);
        return;
    }

    dt_us = axis_filter(&s_x_follow, x, X_LPF_A, X_REV_THRESH, us);
    axis_deadband(&s_x_follow, (float)X_DEADBAND);
    axis_interpolate(&s_x_follow, dt_us, X_PULSES_PER_UNIT,
                     (float)X_MAX_LAG_UNITS, X_SLEW_PPS,
                     &s_x_steps_target, &s_x_steps_done);
}

/* 判断电机1 是否仍在运动。 */
static uint8_t app_motor1_moving(void)
{
    return ((HAL_GetTick() - s_x_last_move_ms) < (uint32_t)APP_LCD_IDLE_MS) ? 1u : 0u;
}

/* 判断 X 轴是否仍在运动。 */
static uint8_t app_x_busy(void)
{
    if ((HAL_GetTick() - s_x_last_move_ms) < (uint32_t)AXIS_ACTIVE_MS)
    {
        return 1u;
    }
    if (s_x_steps_target != s_x_steps_done)
    {
        return 1u;
    }
    return 0u;
}

/* ==================== 深度轴跟随 ==================== */

/* 电机2/电机4 跟随深度。 */
static void app_depth_follow(void)
{
    int32_t  y;
    uint32_t us;
    uint32_t dt_us;
    float    base_cmd;

    us = app_us();
    s_d_follow.pos = (float)s_forward_steps_done * 100.0f /
                     (float)Y_FORWARD_GAIN_PERCENT;
    y = Gesture_Depth();

    if ((Gesture_NoHand() != 0) || (Gesture_HasData() == 0) ||
        (Gesture_XValid() == 0) || ((y == 0) && (Y_ZERO_IS_NOHAND != 0)))
    {
        depth_axis_forget(&s_d_follow, us);
        return;
    }

#if Y_STOP_WHEN_NO_Y
    if (Gesture_XFresh() == 0)
    {
        depth_axis_hold(&s_d_follow, us);
        return;
    }
#endif

    if (s_d_follow.ready == 0u)
    {
        depth_axis_set_zero(&s_d_follow, y, us);
        return;
    }

    dt_us = axis_filter(&s_d_follow, y, Y_LPF_A, Y_REV_THRESH, us);
    axis_deadband(&s_d_follow, Y_DEADBAND);

    if (s_axis_owner == AXIS_OWNER_X)
    {
        depth_axis_align(&s_d_follow, y, us);
        return;
    }

    base_cmd = axis_interpolate_cmd(&s_d_follow, dt_us,
                                    Y_BASE_PULSES_PER_UNIT,
                                    (float)Y_MAX_LAG_UNITS, Y_SLEW_PPS);
    step_set_depth_targets(base_cmd);
}

/* 判断深度轴是否仍在运动。 */
static uint8_t app_depth_moving(void)
{
    return ((HAL_GetTick() - s_depth_last_move_ms) <
            (uint32_t)APP_LCD_IDLE_MS) ? 1u : 0u;
}

/* 选择本周期运动轴。 */
static void app_axis_pick(void)
{
    s_axis_owner = (app_x_busy() != 0u) ? AXIS_OWNER_X : AXIS_OWNER_NONE;
}

/* ==================== 模块接口 ==================== */

/* 初始化电机、微秒计时和 TIM3 脉冲定时器。 */
void GestureMotor_Init(void)
{
    Motor_Init();
    app_us_init();
    step_timer_init();

#if MOTOR_PIN_TEST
    Motor_PinTest();
#endif
}

/* 主循环调用: 选择运动轴并更新 X/深度轴跟随。 */
void GestureMotor_Task(void)
{
    app_axis_pick();
    app_motor1_follow_x();
    app_depth_follow();
}

/* 立即停住两个轴, 并清掉跟随基准。
 * 切换任务时必须调用: 只停止调用 GestureMotor_Task() 不够,
 * TIM3 中断还会把旧目标里剩下的脉冲发完。 */
void GestureMotor_Stop(void)
{
    uint32_t us = app_us();

    axis_forget(&s_x_follow, us, &s_x_steps_target, &s_x_steps_done);
    depth_axis_forget(&s_d_follow, us);

    s_axis_owner      = AXIS_OWNER_NONE;
    s_x_depth_guard   = 0u;
    s_x_depth_override = 0u;
    s_x_guard_out_cnt = 0u;
    s_x_step_settle   = 0u;
    s_depth_step_settle = 0u;
    s_depth_gate_phase = 0u;
    /* 电机2也被其他任务使用, 回到任务1时两路都重新写入方向脚。 */
    s_forward_step_dir_ok = 0u;
    s_down_step_dir_ok = 0u;
}

/* 判断两个电机是否都已停止。 */
uint8_t GestureMotor_IsIdle(void)
{
    if ((app_motor1_moving() == 0u) && (app_depth_moving() == 0u))
    {
        return 1u;
    }
    return 0u;
}
