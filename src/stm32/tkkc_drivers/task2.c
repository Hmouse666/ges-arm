/**
  ******************************************************************************
  * @file    task2.c
  * @brief   任务2: 按两组电机串行执行抓取/放置循环
  *
  * 动作流程:
  *   1) 电机1/3正转, 两者同时开始、同时结束。
  *   2) 电机1/3全部停稳后, 电机2/4正转, 两者同时开始、同时结束。
  *   3) 电机2/4停稳后爪子抓取, 等爪子到位并稳定。
  *   4) 电机2/4反转, 停稳后电机1/3反转。
  *   5) 电机1/3停稳后电机2/4正转, 停稳后爪子松开。
  *   6) 爪子到位后电机2/4反转, 停稳后回到第1步。
  *
  * 电机1使用TIM2_CH1(PA0), 电机2使用TIM3_CH3(PB0), 电机3使用
  * TIM4_CH1(PB6), 电机4的PA12没有PWM复用功能, 由TIM1的更新/比较
  * 中断精确翻转。同一时间只运行一个电机组, 组间不会重叠。
  ******************************************************************************
  */

#include "task2.h"
#include "task1.h"
#include "motor.h"
#include "tim.h"
#include "fpga_gesture.h"
#include "gesture_motor.h"
#include "claw_servo.h"

/* ==================== 任务2参数 ==================== */

/* 电机1/3: 两组动作共用10秒运动时间, 通过各自周期保证同时结束。 */
#define TASK2_MOTOR1_REVERSE_TURNS          2.5f
#define TASK2_MOTOR1_FORWARD_TURNS          2.5f
#define TASK2_MOTOR3_FORWARD_TURNS         24.0f
#define TASK2_MOTOR3_REVERSE_TURNS         24.0f

/* 电机2/4: 正转、反转、再正转、再反转。圈数必须填正值。 */
#define TASK2_MOTOR2_FIRST_FORWARD_TURNS    1.3f
#define TASK2_MOTOR2_REVERSE_TURNS          1.3f
#define TASK2_MOTOR2_SECOND_FORWARD_TURNS   1.3f
#define TASK2_MOTOR4_FIRST_FORWARD_TURNS    1.3f
#define TASK2_MOTOR4_REVERSE_TURNS          1.3f
#define TASK2_MOTOR4_SECOND_FORWARD_TURNS   1.3f

/* 每一组电机的完整运动时间, 修改这里可整体调快/调慢对应组。
 * 电机1/3共用此时间, 从10000改为5000后两路速度均提高一倍, 仍同步启停。 */
#define TASK2_GROUP13_MOVE_MS                5000u
#define TASK2_GROUP24_MOVE_MS                2071u

/* PWM高电平宽度。 */
#define TASK2_MOTOR1_PULSE_HIGH_US            5u
#define TASK2_MOTOR2_PULSE_HIGH_US            5u
#define TASK2_MOTOR4_PULSE_HIGH_US            5u
#define TASK2_MOTOR3_PULSE_HIGH_US            2u

/* 方向建立时间和换向等待时间。 */
#define TASK2_DIR_SETUP_US                   20u
#define TASK2_GROUP_GAP_MS                   20u
#define TASK2_CLAW_DELAY_MS                 500u

/* 手势介入后跟随任务1; 爪子夹紧后连续收到5帧“抓取”才继续任务2。 */
#define TASK2_GESTURE_GRAB_FRAMES              5u

/* 逻辑方向: 0 = 正转, 1 = 反转。 */
#define TASK2_FORWARD_DIR                     0u
#define TASK2_REVERSE_DIR                     1u

/* 任务2内单独修正电机1/3接线方向, 不影响其它任务。 */
#define TASK2_MOTOR1_DIR_INVERT                1u
#define TASK2_MOTOR3_DIR_INVERT                1u

/* 任务阶段: 8个动作阶段, 另有停止、初始化松开和组间等待。 */
#define TASK2_PHASE_STOPPED                   0u
#define TASK2_PHASE_INIT_OPEN_WAIT            1u
#define TASK2_PHASE_GROUP13_FORWARD           2u
#define TASK2_PHASE_GROUP24_FORWARD_1         3u
#define TASK2_PHASE_CLAW_GRAB_WAIT             4u
#define TASK2_PHASE_GROUP24_REVERSE_1         5u
#define TASK2_PHASE_GROUP13_REVERSE           6u
#define TASK2_PHASE_GROUP24_FORWARD_2         7u
#define TASK2_PHASE_CLAW_OPEN_WAIT            8u
#define TASK2_PHASE_GROUP24_REVERSE_2         9u
#define TASK2_PHASE_SETTLE                   10u

typedef struct
{
    uint32_t period_us;
    uint32_t period_ticks;
    uint32_t high_us;
    uint32_t high_ticks;
} Task2TimerConfig;

static volatile uint8_t s_phase;
static uint8_t s_gesture_mode;
static uint8_t s_gesture_wait_release;
static uint8_t s_next_phase;
static uint32_t s_phase_change_ms;
static uint32_t s_settle_duration_ms;
static uint32_t s_claw_settled_ms;
static uint8_t s_claw_ready;
static uint8_t s_gesture_grab_count;
static uint32_t s_gesture_state_seq_seen;
static Task2TimerConfig s_motor1_timer;
static Task2TimerConfig s_motor2_timer;
static Task2TimerConfig s_motor3_timer;
static Task2TimerConfig s_motor4_timer;
static volatile uint8_t s_task2_tim3_active;
static volatile uint8_t s_active_mask;
static volatile uint32_t s_motor1_remaining;
static volatile uint32_t s_motor2_remaining;
static volatile uint32_t s_motor3_remaining;
static volatile uint32_t s_motor4_remaining;

#define TASK2_ACTIVE_M1          0x01u
#define TASK2_ACTIVE_M2          0x02u
#define TASK2_ACTIVE_M3          0x04u
#define TASK2_ACTIVE_M4          0x08u
#define TASK2_ACTIVE_GROUP13     (TASK2_ACTIVE_M1 | TASK2_ACTIVE_M3)
#define TASK2_ACTIVE_GROUP24     (TASK2_ACTIVE_M2 | TASK2_ACTIVE_M4)

static void Task2_StartGroup13(uint8_t direction);
static void Task2_StartGroup13Pulses(uint8_t direction,
                                     uint32_t motor1_pulses,
                                     uint32_t motor3_pulses);
static void Task2_StartGroup24(uint8_t direction,
                               uint32_t motor2_pulses,
                               uint32_t motor4_pulses);
static void Task2_BeginPhase(uint8_t phase);
static void Task2_QueuePhase(uint8_t phase, uint32_t delay_ms);

/* 与task4相同的DWT/SysTick微秒计时。 */
static uint32_t Task2_NowUs(void)
{
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0u)
    {
        return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
    }

    return (HAL_GetTick() * 1000u) +
           ((SysTick->LOAD - SysTick->VAL) / (SystemCoreClock / 1000000u));
}

static uint32_t Task2_TurnsToPulses(float turns)
{
    if (turns <= 0.0f)
    {
        return 0u;
    }

    return (uint32_t)(turns * (float)PULSE_PER_REV + 0.5f);
}

/* 先等舵机真正到位, 再从到位时刻起继续等待稳定时间。 */
static uint8_t Task2_ClawDelayDone(void)
{
    if (ClawServo_IsMoving() != 0u)
    {
        s_claw_ready = 0u;
        return 0u;
    }

    if (s_claw_ready == 0u)
    {
        s_claw_ready = 1u;
        s_claw_settled_ms = HAL_GetTick();
        return 0u;
    }

    return (uint8_t)((HAL_GetTick() - s_claw_settled_ms) >=
                     TASK2_CLAW_DELAY_MS);
}

/* 用同一个动作总时长, 按目标脉冲数计算各电机周期, 实现整组同步启停。 */
static uint32_t Task2_PeriodTicksForDuration(uint32_t duration_ticks,
                                             uint32_t pulses)
{
    uint32_t period_ticks;

    if (pulses == 0u)
    {
        return duration_ticks;
    }

    period_ticks = (duration_ticks + (pulses / 2u)) / pulses;
    if (period_ticks == 0u)
    {
        period_ticks = 1u;
    }

    return period_ticks;
}

/* 给同一组的两个电机计算周期, 让两路的总脉冲时间尽量同时结束。 */
static void Task2_CalculatePairPeriods(uint32_t duration_ticks,
                                       uint32_t pulses_a,
                                       uint32_t pulses_b,
                                       uint32_t *period_a,
                                       uint32_t *period_b)
{
    uint32_t target_ticks;
    uint32_t actual_ticks_a;
    uint32_t actual_ticks_b;
    uint8_t iteration;

    target_ticks = duration_ticks;
    *period_a = Task2_PeriodTicksForDuration(target_ticks, pulses_a);
    *period_b = Task2_PeriodTicksForDuration(target_ticks, pulses_b);

    for (iteration = 0u; iteration < 8u; iteration++)
    {
        actual_ticks_a = pulses_a * (*period_a);
        actual_ticks_b = pulses_b * (*period_b);

        if (actual_ticks_a == actual_ticks_b)
        {
            break;
        }

        target_ticks = (actual_ticks_a > actual_ticks_b) ?
                       actual_ticks_a : actual_ticks_b;
        *period_a = Task2_PeriodTicksForDuration(target_ticks, pulses_a);
        *period_b = Task2_PeriodTicksForDuration(target_ticks, pulses_b);
    }
}

static void Task2_SetDirection13(uint8_t dir13)
{
    uint8_t level;

    level = (uint8_t)(dir13 ^ MOTOR1_DIR_INVERT ^
                      TASK2_MOTOR1_DIR_INVERT);
    HAL_GPIO_WritePin(MOTOR1_DIR_PORT, MOTOR1_DIR_PIN,
                      (level != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    level = (uint8_t)(dir13 ^ MOTOR3_DIR_INVERT ^
                      TASK2_MOTOR3_DIR_INVERT);
    HAL_GPIO_WritePin(MOTOR3_DIR_PORT, MOTOR3_DIR_PIN,
                      (level != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void Task2_SetDirection24(uint8_t dir24)
{
    uint8_t level;

    level = (uint8_t)(dir24 ^ MOTOR2_DIR_INVERT);
    HAL_GPIO_WritePin(MOTOR2_DIR_PORT, MOTOR2_DIR_PIN,
                      (level != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    level = (uint8_t)(dir24 ^ MOTOR4_DIR_INVERT);
    HAL_GPIO_WritePin(MOTOR4_DIR_PORT, MOTOR4_DIR_PIN,
                      (level != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void Task2_PulsePinsAsGpioLow(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = MOTOR1_PUL_PIN;
    HAL_GPIO_Init(MOTOR1_PUL_PORT, &gpio);
    HAL_GPIO_WritePin(MOTOR1_PUL_PORT, MOTOR1_PUL_PIN, GPIO_PIN_RESET);

    gpio.Pin = MOTOR2_PUL_PIN;
    HAL_GPIO_Init(MOTOR2_PUL_PORT, &gpio);
    HAL_GPIO_WritePin(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN, GPIO_PIN_RESET);

    gpio.Pin = MOTOR3_PUL_PIN;
    HAL_GPIO_Init(MOTOR3_PUL_PORT, &gpio);
    HAL_GPIO_WritePin(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN, GPIO_PIN_RESET);

    gpio.Pin = MOTOR4_PUL_PIN;
    HAL_GPIO_Init(MOTOR4_PUL_PORT, &gpio);
    HAL_GPIO_WritePin(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN, GPIO_PIN_RESET);
}

#if 0
static void Task2_PulsePinsAsTimerOutput(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = MOTOR1_PUL_PIN;
    HAL_GPIO_Init(MOTOR1_PUL_PORT, &gpio);

    gpio.Pin = MOTOR2_PUL_PIN;
    HAL_GPIO_Init(MOTOR2_PUL_PORT, &gpio);

    gpio.Pin = MOTOR3_PUL_PIN;
    HAL_GPIO_Init(MOTOR3_PUL_PORT, &gpio);

    /* PA12没有定时器PWM复用输出, 继续作为普通GPIO, 由TIM3中断翻转。 */
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pin = MOTOR4_PUL_PIN;
    HAL_GPIO_Init(MOTOR4_PUL_PORT, &gpio);
}
#endif

/* 只把电机2/4的脉冲脚切到定时器输出, 不影响仍在运行的电机1/3。 */
static void Task2_Motor24PinsAsTimerOutput(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = MOTOR2_PUL_PIN;
    HAL_GPIO_Init(MOTOR2_PUL_PORT, &gpio);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pin = MOTOR4_PUL_PIN;
    HAL_GPIO_Init(MOTOR4_PUL_PORT, &gpio);
}

/* 只把电机1/3的脉冲脚切到定时器输出。 */
static void Task2_Motor13PinsAsTimerOutput(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = MOTOR1_PUL_PIN;
    HAL_GPIO_Init(MOTOR1_PUL_PORT, &gpio);

    gpio.Pin = MOTOR3_PUL_PIN;
    HAL_GPIO_Init(MOTOR3_PUL_PORT, &gpio);
}

/* 单个电机完成后立即恢复普通低电平输出, 不影响其他电机继续运行。 */
static void Task2_PulsePinAsGpioLow(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &gpio);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
}

/* 准备指定的PWM通道, 但先不打开输出和计数器。 */
static void Task2_PreparePwm(TIM_TypeDef *tim,
                             const Task2TimerConfig *cfg,
                             uint8_t channel)
{
    uint32_t period_ticks;
    uint32_t high_ticks;

    period_ticks = cfg->period_ticks;
    high_ticks = cfg->high_ticks;

    if (period_ticks < 2u)
    {
        period_ticks = 2u;
    }
    if (period_ticks > 65536u)
    {
        period_ticks = 65536u;
    }
    if (high_ticks >= period_ticks)
    {
        high_ticks = period_ticks / 2u;
    }
    if (high_ticks == 0u)
    {
        high_ticks = 1u;
    }

    tim->CR1 = 0u;
    tim->PSC = 0u;
    tim->ARR = period_ticks - 1u;
    tim->CNT = 0u;

    if (channel == 1u)
    {
        tim->CCR1 = high_ticks;
        tim->CCMR1 = (uint16_t)((tim->CCMR1 &
                       (uint16_t)~(TIM_CCMR1_OC1M | TIM_CCMR1_CC1S)) |
                      TIM_OCMODE_PWM1);
        tim->CCER &= (uint16_t)~(TIM_CCER_CC1P | TIM_CCER_CC1E);
    }
    else
    {
        tim->CCR3 = high_ticks;
        tim->CCMR2 = (uint16_t)((tim->CCMR2 &
                       (uint16_t)~(TIM_CCMR2_OC3M | TIM_CCMR2_CC3S)) |
                      TIM_OCMODE_PWM1);
        tim->CCER &= (uint16_t)~(TIM_CCER_CC3P | TIM_CCER_CC3E);
    }

    tim->EGR = TIM_EGR_UG;
    tim->SR = 0u;
}

/* TIM1只做内部定时, 不输出到引脚; 用于产生电机4的独立周期脉冲。 */
static void Task2_PrepareMotor4Timer(TIM_TypeDef *tim,
                                     const Task2TimerConfig *cfg)
{
    uint32_t period_ticks;
    uint32_t high_ticks;

    period_ticks = cfg->period_ticks;
    high_ticks = cfg->high_ticks;

    if (period_ticks < 2u)
    {
        period_ticks = 2u;
    }
    if (period_ticks > 65536u)
    {
        period_ticks = 65536u;
    }
    if (high_ticks >= period_ticks)
    {
        high_ticks = period_ticks / 2u;
    }
    if (high_ticks == 0u)
    {
        high_ticks = 1u;
    }

    tim->CR1 = 0u;
    tim->PSC = 0u;
    tim->ARR = period_ticks - 1u;
    tim->CCR1 = high_ticks;
    tim->CNT = 0u;
    tim->CCMR1 &= (uint16_t)~(TIM_CCMR1_OC1M | TIM_CCMR1_CC1S);
    tim->CCER &= (uint16_t)~(TIM_CCER_CC1P | TIM_CCER_CC1E);
    tim->EGR = TIM_EGR_UG;
    tim->SR = 0u;
}

static void Task2_SetMotor4Pulse(GPIO_PinState state)
{
    HAL_GPIO_WritePin(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN, state);
}

/* 关闭全部PWM/比较中断, 并把四个脉冲脚恢复成普通低电平输出。 */
static void Task2_StopPwm(void)
{
    __disable_irq();

    s_task2_tim3_active = 0u;
    s_active_mask = 0u;
    s_motor1_remaining = 0u;
    s_motor2_remaining = 0u;
    s_motor3_remaining = 0u;
    s_motor4_remaining = 0u;

    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM4->CR1 &= ~TIM_CR1_CEN;

    TIM1->DIER &= (uint16_t)~(TIM_DIER_UIE | TIM_DIER_CC1IE);
    TIM2->DIER &= (uint16_t)~TIM_DIER_CC1IE;
    TIM4->DIER &= (uint16_t)~TIM_DIER_CC1IE;
    TIM1->CCER &= (uint16_t)~TIM_CCER_CC1E;
    TIM2->CCER &= (uint16_t)~TIM_CCER_CC1E;
    TIM3->CCER &= (uint16_t)~TIM_CCER_CC3E;
    TIM4->CCER &= (uint16_t)~TIM_CCER_CC1E;

    TIM3->DIER &= (uint16_t)~(TIM_DIER_UIE | TIM_DIER_CC3IE);
    TIM1->SR = 0u;
    TIM2->SR = 0u;
    TIM3->SR = 0u;
    TIM4->SR = 0u;

    Task2_PulsePinsAsGpioLow();

    __enable_irq();
}

/* 第2段结束只停电机2/4, 电机1/3继续正转, 保证它们跨第2、3段不中断。 */
#if 0
static void Task2_StopMotor24(void)
{
    __disable_irq();

    s_task2_tim3_active = 0u;
    s_active_mask &= (uint8_t)~(TASK2_ACTIVE_M2 | TASK2_ACTIVE_M4);
    s_motor2_remaining = 0u;
    s_motor4_remaining = 0u;

    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM1->DIER &= (uint16_t)~(TIM_DIER_UIE | TIM_DIER_CC1IE);
    TIM3->DIER &= (uint16_t)~(TIM_DIER_UIE | TIM_DIER_CC3IE);
    TIM1->CCER &= (uint16_t)~TIM_CCER_CC1E;
    TIM3->CCER &= (uint16_t)~TIM_CCER_CC3E;
    TIM1->SR = 0u;
    TIM3->SR = 0u;

    Task2_PulsePinAsGpioLow(MOTOR2_PUL_PORT, MOTOR2_PUL_PIN);
    Task2_PulsePinAsGpioLow(MOTOR4_PUL_PORT, MOTOR4_PUL_PIN);

    __enable_irq();
}
#endif

/* 快速进入手势模式所需条件; 进入后由任务1自己处理丢手和坐标超时。 */
static uint8_t Task2_GestureCanEnter(void)
{
    if ((Gesture_IsValid() == 0u) ||
        (Gesture_XFresh() == 0u) ||
        (Gesture_NoHand() != 0u))
    {
        return 0u;
    }

    return 1u;
}

static void Task2_ResetGestureGrabCount(void)
{
    s_gesture_grab_count = 0u;
    s_gesture_state_seq_seen = Gesture_StateSeq();
}

static void Task2_BeginGestureMode(void)
{
    Task2_StopPwm();
    GestureMotor_RestartTimer();
    s_phase = TASK2_PHASE_STOPPED;
    s_gesture_mode = 1u;
    Task2_ResetGestureGrabCount();
    Task1_Enter();
}

/* 爪子夹紧后, 只统计连续识别为“抓取”的新状态帧。 */
static uint8_t Task2_GestureFiveGrabsDone(void)
{
    uint32_t state_seq;

    if ((Gesture_HasData() == 0u) || (Gesture_IsValid() == 0u))
    {
        Task2_ResetGestureGrabCount();
        return 0u;
    }

    state_seq = Gesture_StateSeq();
    if (state_seq == s_gesture_state_seq_seen)
    {
        return 0u;
    }
    s_gesture_state_seq_seen = state_seq;

    if ((Gesture_IsClosed() == 0u) ||
        (ClawServo_IsMoving() != 0u))
    {
        s_gesture_grab_count = 0u;
        return 0u;
    }

    if (s_gesture_grab_count < 255u)
    {
        s_gesture_grab_count++;
    }

    return (uint8_t)(s_gesture_grab_count >=
                     TASK2_GESTURE_GRAB_FRAMES);
}

/* 五次抓取后从反向阶段继续任务2: 保持爪子夹紧, 先让电机2/4反转。 */
static void Task2_EndGestureMode(void)
{
    GestureMotor_Stop();
    ClawServo_Hold();
    Task1_Exit();

    s_gesture_mode = 0u;
    s_gesture_wait_release = 1u;
    Task2_ResetGestureGrabCount();
    Task2_StopPwm();
    Gesture_ResetValid();
    Task2_BeginPhase(TASK2_PHASE_GROUP24_REVERSE_1);
}

static void Task2_SetTimerConfig(Task2TimerConfig *cfg,
                                 uint32_t period_ticks,
                                 uint32_t high_us,
                                 uint32_t ticks_per_us)
{
    cfg->period_ticks = period_ticks;
    cfg->period_us = (period_ticks + (ticks_per_us / 2u)) / ticks_per_us;
    cfg->high_us = high_us;
    cfg->high_ticks = high_us * ticks_per_us;
}

#if 0
/* 旧版两组并行阶段实现, 保留作对照, 不再参与编译。 */
/* 第1方向阶段: 电机1/3反转；第1周期时电机2/4正转, 后续周期反转。 */
static void Task2_StartStage1(void)
{
    uint32_t motor1_pulses;
    uint32_t motor2_pulses;
    uint32_t motor3_pulses;
    uint32_t motor4_pulses;
    uint32_t period1_ticks;
    uint32_t period2_ticks;
    uint32_t period3_ticks;
    uint32_t period4_ticks;
    uint32_t stage1_task_ticks;
    uint32_t stage1_task_runtime_ticks;
    uint32_t target_ticks;
    uint32_t actual_ticks;
    uint32_t ticks_per_us;
    uint8_t iteration;
    uint32_t wait_start_us;

    Task2_StopPwm();

    __disable_irq();

    Task2_SetDirection13(TASK2_REVERSE_DIR);
    Task2_SetDirection24((s_first_cycle != 0u) ?
                         TASK2_FORWARD_DIR : TASK2_REVERSE_DIR);

    motor1_pulses = (s_first_cycle != 0u) ?
        s_initial_motor1_pulses :
        Task2_TurnsToPulses(TASK2_MOTOR1_REVERSE_TURNS);
    motor2_pulses = Task2_TurnsToPulses(
        TASK2_MOTOR2_FIRST_FORWARD_TURNS);
    motor3_pulses = Task2_TurnsToPulses(
        TASK2_MOTOR3_REVERSE_TURNS);
    motor4_pulses = Task2_TurnsToPulses(
        TASK2_MOTOR4_FIRST_FORWARD_TURNS);

    ticks_per_us = SystemCoreClock / 1000000u;
    /* 反转使用与正转完全相同的脉冲周期；正转阶段自身保持原逻辑不变。 */
    period1_ticks = Task2_Motor1ForwardPeriodTicks(ticks_per_us);
    stage1_task_ticks = Task2_Stage24TargetTicks(
        TASK2_PHASE_STAGE1, ticks_per_us);
    target_ticks = stage1_task_ticks * 2u +
        (uint32_t)TASK2_REVERSE_GAP_MS * 1000u * ticks_per_us;

    if (s_first_cycle != 0u)
    {
        /* 第1周期: 电机2/4第1个动作独立占满整个第1方向阶段。 */
        actual_ticks = motor2_pulses *
            Task2_Motor1PeriodUs(TASK2_MOTOR2_VALUE) * ticks_per_us;
        if (actual_ticks > target_ticks)
        {
            target_ticks = actual_ticks;
        }
        actual_ticks = motor4_pulses *
            Task2_Motor1PeriodUs(TASK2_MOTOR4_VALUE) * ticks_per_us;
        if (actual_ticks > target_ticks)
        {
            target_ticks = actual_ticks;
        }

        for (iteration = 0u; iteration < 5u; iteration++)
        {
            period2_ticks =
                Task2_PeriodTicksForDuration(target_ticks, motor2_pulses);
            period3_ticks =
                Task2_PeriodTicksForDuration(target_ticks, motor3_pulses);
            period4_ticks =
                Task2_PeriodTicksForDuration(target_ticks, motor4_pulses);

            actual_ticks = motor1_pulses * period1_ticks;
            if ((motor2_pulses * period2_ticks) > actual_ticks)
            {
                actual_ticks = motor2_pulses * period2_ticks;
            }
            if ((motor3_pulses * period3_ticks) > actual_ticks)
            {
                actual_ticks = motor3_pulses * period3_ticks;
            }
            if ((motor4_pulses * period4_ticks) > actual_ticks)
            {
                actual_ticks = motor4_pulses * period4_ticks;
            }

            if (actual_ticks == target_ticks)
            {
                break;
            }
            target_ticks = actual_ticks;
        }
        s_stage1_task_duration_ms = 0u;
    }
    else
    {
        /* 第2周期起: 第1个方向阶段容纳电机2/4的反转、正转两个动作。 */
        period2_ticks =
            Task2_PeriodTicksForDuration(stage1_task_ticks, motor2_pulses);
        period4_ticks =
            Task2_PeriodTicksForDuration(stage1_task_ticks, motor4_pulses);
        period3_ticks =
            Task2_PeriodTicksForDuration(target_ticks, motor3_pulses);

        stage1_task_runtime_ticks = motor2_pulses * period2_ticks;
        if ((motor4_pulses * period4_ticks) >
            stage1_task_runtime_ticks)
        {
            stage1_task_runtime_ticks = motor4_pulses * period4_ticks;
        }
        s_stage1_task_duration_ms =
            Task2_TicksToMs(stage1_task_runtime_ticks);
    }

    period2_ticks = Task2_ApplySpeedPercent(
        period2_ticks, TASK2_MOTOR24_SPEED_PERCENT);
    period4_ticks = Task2_ApplySpeedPercent(
        period4_ticks, TASK2_MOTOR24_SPEED_PERCENT);

    Task2_SetTimerConfig(&s_motor1_timer, period1_ticks,
                         TASK2_MOTOR1_PULSE_HIGH_US, ticks_per_us);
    Task2_SetTimerConfig(&s_motor2_timer, period2_ticks,
                         TASK2_MOTOR2_PULSE_HIGH_US, ticks_per_us);
    Task2_SetTimerConfig(&s_motor3_timer, period3_ticks,
                         TASK2_MOTOR3_PULSE_HIGH_US, ticks_per_us);
    Task2_SetTimerConfig(&s_motor4_timer, period4_ticks,
                         TASK2_MOTOR4_PULSE_HIGH_US, ticks_per_us);

    s_motor1_remaining = motor1_pulses;
    s_motor2_remaining = motor2_pulses;
    s_motor3_remaining = motor3_pulses;
    s_motor4_remaining = motor4_pulses;

    s_active_mask = 0u;
    if (motor1_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M1;
    }
    if (motor2_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M2;
    }
    if (motor3_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M3;
    }
    if (motor4_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M4;
    }

    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    Task2_PreparePwm(TIM2, &s_motor1_timer, 1u);
    Task2_PreparePwm(TIM3, &s_motor2_timer, 3u);
    Task2_PreparePwm(TIM4, &s_motor3_timer, 1u);
    Task2_PrepareMotor4Timer(TIM1, &s_motor4_timer);

    NVIC_SetPriority(TIM1_UP_IRQn, 0u);
    NVIC_SetPriority(TIM1_CC_IRQn, 0u);
    NVIC_SetPriority(TIM2_IRQn, 0u);
    NVIC_SetPriority(TIM4_IRQn, 0u);
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM4_IRQn);

    __enable_irq();

    wait_start_us = Task2_NowUs();
    while ((uint32_t)(Task2_NowUs() - wait_start_us) <
           TASK2_DIR_SETUP_US)
    {
    }

    /* 同一段临界区启动四路定时器。 */
    __disable_irq();

    Task2_PulsePinsAsTimerOutput();
    TIM1->CNT = 0u;
    TIM2->CNT = 0u;
    TIM3->CNT = 0u;
    TIM4->CNT = 0u;

    s_task2_tim3_active = 1u;
    if ((s_active_mask & TASK2_ACTIVE_M4) != 0u)
    {
        Task2_SetMotor4Pulse(GPIO_PIN_SET);
    }

    if ((s_active_mask & TASK2_ACTIVE_M1) != 0u)
    {
        TIM2->CCER |= TIM_CCER_CC1E;
        TIM2->DIER |= TIM_DIER_CC1IE;
    }
    if ((s_active_mask & TASK2_ACTIVE_M2) != 0u)
    {
        TIM3->CCER |= TIM_CCER_CC3E;
        TIM3->DIER |= TIM_DIER_CC3IE;
    }
    if ((s_active_mask & TASK2_ACTIVE_M3) != 0u)
    {
        TIM4->CCER |= TIM_CCER_CC1E;
        TIM4->DIER |= TIM_DIER_CC1IE;
    }
    if ((s_active_mask & TASK2_ACTIVE_M4) != 0u)
    {
        TIM1->DIER |= (TIM_DIER_UIE | TIM_DIER_CC1IE);
    }

    TIM1->SR = 0u;
    TIM2->SR = 0u;
    TIM3->SR = 0u;
    TIM4->SR = 0u;

    TIM2->CR1 |= TIM_CR1_CEN;
    TIM3->CR1 |= TIM_CR1_CEN;
    TIM4->CR1 |= TIM_CR1_CEN;
    TIM1->CR1 |= TIM_CR1_CEN;

    __enable_irq();
    s_phase_change_ms = HAL_GetTick();
}

/* 第2方向阶段: 电机1/3正转, 电机2/4先执行反转。
 * 电机1/3的脉冲周期按“第2段 + 换向等待 + 第3段”计算。 */
static void Task2_StartStage2(void)
{
    uint32_t motor1_pulses;
    uint32_t motor2_pulses;
    uint32_t motor3_pulses;
    uint32_t motor4_pulses;
    uint32_t period1_ticks;
    uint32_t period2_ticks;
    uint32_t period3_ticks;
    uint32_t period4_ticks;
    uint32_t stage2_ticks;
    uint32_t stage2_runtime_ticks;
    uint32_t stage3_ticks;
    uint32_t gap_ticks;
    uint32_t forward_span_ticks;
    uint32_t target_ticks;
    uint32_t actual_ticks;
    uint32_t ticks_per_us;
    uint8_t iteration;
    uint32_t wait_start_us;

    Task2_StopPwm();

    ticks_per_us = SystemCoreClock / 1000000u;
    stage2_ticks = Task2_Stage24TargetTicks(
        TASK2_PHASE_STAGE2, ticks_per_us);
    stage3_ticks = Task2_Stage24TargetTicks(
        TASK2_PHASE_STAGE3, ticks_per_us);

    motor1_pulses = Task2_TurnsToPulses(TASK2_MOTOR1_FORWARD_TURNS);
    motor2_pulses = Task2_TurnsToPulses(TASK2_MOTOR2_REVERSE_TURNS);
    motor3_pulses = Task2_TurnsToPulses(TASK2_MOTOR3_FORWARD_TURNS);
    motor4_pulses = Task2_TurnsToPulses(TASK2_MOTOR4_REVERSE_TURNS);

    /* 每个周期的第2方向阶段都先反转, 然后由第3段执行正转。 */
    period2_ticks =
        Task2_PeriodTicksForDuration(stage2_ticks, motor2_pulses);
    period4_ticks =
        Task2_PeriodTicksForDuration(stage2_ticks, motor4_pulses);
    stage2_runtime_ticks = motor2_pulses * period2_ticks;
    if ((motor4_pulses * period4_ticks) > stage2_runtime_ticks)
    {
        stage2_runtime_ticks = motor4_pulses * period4_ticks;
    }
    gap_ticks = (uint32_t)TASK2_REVERSE_GAP_MS * 1000u * ticks_per_us;
    forward_span_ticks =
        stage2_runtime_ticks + gap_ticks + stage3_ticks;

    /* 电机1/3共用完整的正转时间跨度, 两边同时走完。 */
    target_ticks = forward_span_ticks;
    for (iteration = 0u; iteration < 5u; iteration++)
    {
        period1_ticks =
            Task2_PeriodTicksForDuration(target_ticks, motor1_pulses);
        period3_ticks =
            Task2_PeriodTicksForDuration(target_ticks, motor3_pulses);
        actual_ticks = motor1_pulses * period1_ticks;
        if ((motor3_pulses * period3_ticks) > actual_ticks)
        {
            actual_ticks = motor3_pulses * period3_ticks;
        }
        if (actual_ticks == target_ticks)
        {
            break;
        }
        target_ticks = actual_ticks;
    }

    s_stage2_duration_ms = Task2_TicksToMs(stage2_runtime_ticks);
    if (target_ticks > (stage2_runtime_ticks + gap_ticks))
    {
        s_stage3_duration_ticks =
            target_ticks - stage2_runtime_ticks - gap_ticks;
    }
    else
    {
        s_stage3_duration_ticks = stage3_ticks;
    }

    period2_ticks = Task2_ApplySpeedPercent(
        period2_ticks, TASK2_MOTOR24_SPEED_PERCENT);
    period4_ticks = Task2_ApplySpeedPercent(
        period4_ticks, TASK2_MOTOR24_SPEED_PERCENT);

    Task2_SetTimerConfig(&s_motor1_timer, period1_ticks,
                         TASK2_MOTOR1_PULSE_HIGH_US, ticks_per_us);
    Task2_SetTimerConfig(&s_motor2_timer, period2_ticks,
                         TASK2_MOTOR2_PULSE_HIGH_US, ticks_per_us);
    Task2_SetTimerConfig(&s_motor3_timer, period3_ticks,
                         TASK2_MOTOR3_PULSE_HIGH_US, ticks_per_us);
    Task2_SetTimerConfig(&s_motor4_timer, period4_ticks,
                         TASK2_MOTOR4_PULSE_HIGH_US, ticks_per_us);

    __disable_irq();
    Task2_SetDirection13(TASK2_FORWARD_DIR);
    Task2_SetDirection24(TASK2_REVERSE_DIR);

    s_motor1_remaining = motor1_pulses;
    s_motor2_remaining = motor2_pulses;
    s_motor3_remaining = motor3_pulses;
    s_motor4_remaining = motor4_pulses;

    s_active_mask = 0u;
    if (motor1_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M1;
    }
    if (motor2_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M2;
    }
    if (motor3_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M3;
    }
    if (motor4_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M4;
    }

    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    Task2_PreparePwm(TIM2, &s_motor1_timer, 1u);
    Task2_PreparePwm(TIM3, &s_motor2_timer, 3u);
    Task2_PreparePwm(TIM4, &s_motor3_timer, 1u);
    Task2_PrepareMotor4Timer(TIM1, &s_motor4_timer);

    NVIC_SetPriority(TIM1_UP_IRQn, 0u);
    NVIC_SetPriority(TIM1_CC_IRQn, 0u);
    NVIC_SetPriority(TIM2_IRQn, 0u);
    NVIC_SetPriority(TIM4_IRQn, 0u);
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM4_IRQn);

    __enable_irq();

    wait_start_us = Task2_NowUs();
    while ((uint32_t)(Task2_NowUs() - wait_start_us) <
           TASK2_DIR_SETUP_US)
    {
    }

    __disable_irq();

    Task2_PulsePinsAsTimerOutput();
    TIM1->CNT = 0u;
    TIM2->CNT = 0u;
    TIM3->CNT = 0u;
    TIM4->CNT = 0u;

    s_task2_tim3_active = 1u;
    if ((s_active_mask & TASK2_ACTIVE_M4) != 0u)
    {
        Task2_SetMotor4Pulse(GPIO_PIN_SET);
    }

    if ((s_active_mask & TASK2_ACTIVE_M1) != 0u)
    {
        TIM2->CCER |= TIM_CCER_CC1E;
        TIM2->DIER |= TIM_DIER_CC1IE;
    }
    if ((s_active_mask & TASK2_ACTIVE_M2) != 0u)
    {
        TIM3->CCER |= TIM_CCER_CC3E;
        TIM3->DIER |= TIM_DIER_CC3IE;
    }
    if ((s_active_mask & TASK2_ACTIVE_M3) != 0u)
    {
        TIM4->CCER |= TIM_CCER_CC1E;
        TIM4->DIER |= TIM_DIER_CC1IE;
    }
    if ((s_active_mask & TASK2_ACTIVE_M4) != 0u)
    {
        TIM1->DIER |= (TIM_DIER_UIE | TIM_DIER_CC1IE);
    }

    TIM1->SR = 0u;
    TIM2->SR = 0u;
    TIM3->SR = 0u;
    TIM4->SR = 0u;

    TIM2->CR1 |= TIM_CR1_CEN;
    TIM3->CR1 |= TIM_CR1_CEN;
    TIM4->CR1 |= TIM_CR1_CEN;
    TIM1->CR1 |= TIM_CR1_CEN;

    __enable_irq();
    s_phase_change_ms = HAL_GetTick();
}

/* 只重启电机2/4执行指定动作, 电机1/3的定时器继续运行。 */
static void Task2_StartMotor24Task(uint8_t dir24,
                                   uint32_t target_ticks,
                                   uint32_t motor2_pulses,
                                   uint32_t motor4_pulses)
{
    uint32_t period2_ticks;
    uint32_t period4_ticks;
    uint32_t ticks_per_us;

    ticks_per_us = SystemCoreClock / 1000000u;
    period2_ticks =
        Task2_PeriodTicksForDuration(target_ticks, motor2_pulses);
    period4_ticks =
        Task2_PeriodTicksForDuration(target_ticks, motor4_pulses);
    period2_ticks = Task2_ApplySpeedPercent(
        period2_ticks, TASK2_MOTOR24_SPEED_PERCENT);
    period4_ticks = Task2_ApplySpeedPercent(
        period4_ticks, TASK2_MOTOR24_SPEED_PERCENT);

    Task2_SetTimerConfig(&s_motor2_timer, period2_ticks,
                         TASK2_MOTOR2_PULSE_HIGH_US, ticks_per_us);
    Task2_SetTimerConfig(&s_motor4_timer, period4_ticks,
                         TASK2_MOTOR4_PULSE_HIGH_US, ticks_per_us);

    Task2_StopMotor24();
    __disable_irq();

    Task2_SetDirection24(dir24);
    s_motor2_remaining = motor2_pulses;
    s_motor4_remaining = motor4_pulses;
    if (motor2_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M2;
    }
    if (motor4_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M4;
    }

    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    Task2_PreparePwm(TIM3, &s_motor2_timer, 3u);
    Task2_PrepareMotor4Timer(TIM1, &s_motor4_timer);

    NVIC_SetPriority(TIM1_UP_IRQn, 0u);
    NVIC_SetPriority(TIM1_CC_IRQn, 0u);
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    NVIC_EnableIRQ(TIM1_CC_IRQn);

    Task2_Motor24PinsAsTimerOutput();
    TIM1->CNT = 0u;
    TIM3->CNT = 0u;
    s_task2_tim3_active = 1u;

    if ((s_active_mask & TASK2_ACTIVE_M4) != 0u)
    {
        Task2_SetMotor4Pulse(GPIO_PIN_SET);
        TIM1->DIER |= (TIM_DIER_UIE | TIM_DIER_CC1IE);
    }
    if ((s_active_mask & TASK2_ACTIVE_M2) != 0u)
    {
        TIM3->CCER |= TIM_CCER_CC3E;
        TIM3->DIER |= TIM_DIER_CC3IE;
    }

    TIM1->SR = 0u;
    TIM3->SR = 0u;
    TIM3->CR1 |= TIM_CR1_CEN;
    TIM1->CR1 |= TIM_CR1_CEN;

    __enable_irq();
    s_phase_change_ms = HAL_GetTick();
}

/* 后续周期的第1方向阶段: 在电机1/3继续运行时启动正转动作。 */
static void Task2_StartStage1Task2(void)
{
    uint32_t ticks_per_us;
    uint32_t target_ticks;
    uint32_t motor2_pulses;
    uint32_t motor4_pulses;

    ticks_per_us = SystemCoreClock / 1000000u;
    target_ticks = Task2_Stage24TargetTicks(
        TASK2_PHASE_STAGE2, ticks_per_us);
    motor2_pulses = Task2_TurnsToPulses(TASK2_MOTOR2_REVERSE_TURNS);
    motor4_pulses = Task2_TurnsToPulses(TASK2_MOTOR4_REVERSE_TURNS);

    Task2_StartMotor24Task(
        TASK2_FORWARD_DIR,
        target_ticks, motor2_pulses, motor4_pulses);
}

/* 第3段: 只重启电机2/4并执行正转。
 * 电机1/3的定时器始终保持运行。 */
static void Task2_StartStage3(void)
{
    uint32_t target_ticks;
    uint32_t motor2_pulses;
    uint32_t motor4_pulses;

    target_ticks = s_stage3_duration_ticks;
    if (target_ticks == 0u)
    {
        target_ticks = Task2_Stage24TargetTicks(
            TASK2_PHASE_STAGE3, SystemCoreClock / 1000000u);
    }
    motor2_pulses = Task2_TurnsToPulses(
        TASK2_MOTOR2_SECOND_FORWARD_TURNS);
    motor4_pulses = Task2_TurnsToPulses(
        TASK2_MOTOR4_SECOND_FORWARD_TURNS);

    Task2_StartMotor24Task(
        TASK2_FORWARD_DIR,
        target_ticks, motor2_pulses, motor4_pulses);
}
#endif

/* 启动电机1/3组。两路使用不同脉冲周期, 但总运动时间相同。 */
static void Task2_StartGroup13Pulses(uint8_t direction,
                                     uint32_t motor1_pulses,
                                     uint32_t motor3_pulses)
{
    uint32_t period1_ticks;
    uint32_t period3_ticks;
    uint32_t duration_ticks;
    uint32_t ticks_per_us;
    uint32_t wait_start_us;

    Task2_StopPwm();

    ticks_per_us = SystemCoreClock / 1000000u;
    duration_ticks =
        TASK2_GROUP13_MOVE_MS * 1000u * ticks_per_us;
    Task2_CalculatePairPeriods(duration_ticks,
                               motor1_pulses, motor3_pulses,
                               &period1_ticks, &period3_ticks);

    Task2_SetTimerConfig(&s_motor1_timer, period1_ticks,
                         TASK2_MOTOR1_PULSE_HIGH_US, ticks_per_us);
    Task2_SetTimerConfig(&s_motor3_timer, period3_ticks,
                         TASK2_MOTOR3_PULSE_HIGH_US, ticks_per_us);

    __disable_irq();
    Task2_SetDirection13(direction);

    s_motor1_remaining = motor1_pulses;
    s_motor3_remaining = motor3_pulses;
    s_active_mask = 0u;
    if (motor1_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M1;
    }
    if (motor3_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M3;
    }

    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    Task2_PreparePwm(TIM2, &s_motor1_timer, 1u);
    Task2_PreparePwm(TIM4, &s_motor3_timer, 1u);

    NVIC_SetPriority(TIM2_IRQn, 0u);
    NVIC_SetPriority(TIM4_IRQn, 0u);
    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM4_IRQn);
    __enable_irq();

    wait_start_us = Task2_NowUs();
    while ((uint32_t)(Task2_NowUs() - wait_start_us) <
           TASK2_DIR_SETUP_US)
    {
    }

    __disable_irq();
    Task2_Motor13PinsAsTimerOutput();
    TIM2->CNT = 0u;
    TIM4->CNT = 0u;
    s_task2_tim3_active = 1u;

    if ((s_active_mask & TASK2_ACTIVE_M1) != 0u)
    {
        TIM2->CCER |= TIM_CCER_CC1E;
        TIM2->DIER |= TIM_DIER_CC1IE;
    }
    if ((s_active_mask & TASK2_ACTIVE_M3) != 0u)
    {
        TIM4->CCER |= TIM_CCER_CC1E;
        TIM4->DIER |= TIM_DIER_CC1IE;
    }

    TIM2->SR = 0u;
    TIM4->SR = 0u;
    TIM2->CR1 |= TIM_CR1_CEN;
    TIM4->CR1 |= TIM_CR1_CEN;
    __enable_irq();
}

/* 任务2自身按固定圈数启动电机1/3。 */
static void Task2_StartGroup13(uint8_t direction)
{
    uint32_t motor1_pulses;
    uint32_t motor3_pulses;

    motor1_pulses = Task2_TurnsToPulses(
        (direction == TASK2_FORWARD_DIR) ?
        TASK2_MOTOR1_FORWARD_TURNS : TASK2_MOTOR1_REVERSE_TURNS);
    motor3_pulses = Task2_TurnsToPulses(
        (direction == TASK2_FORWARD_DIR) ?
        TASK2_MOTOR3_FORWARD_TURNS : TASK2_MOTOR3_REVERSE_TURNS);

    Task2_StartGroup13Pulses(direction, motor1_pulses, motor3_pulses);
}

/* 启动电机2/4组。正转的两次动作可以分别设置圈数。 */
static void Task2_StartGroup24(uint8_t direction,
                               uint32_t motor2_pulses,
                               uint32_t motor4_pulses)
{
    uint32_t period2_ticks;
    uint32_t period4_ticks;
    uint32_t duration_ticks;
    uint32_t ticks_per_us;
    uint32_t wait_start_us;

    Task2_StopPwm();

    ticks_per_us = SystemCoreClock / 1000000u;
    duration_ticks =
        TASK2_GROUP24_MOVE_MS * 1000u * ticks_per_us;
    Task2_CalculatePairPeriods(duration_ticks,
                               motor2_pulses, motor4_pulses,
                               &period2_ticks, &period4_ticks);

    Task2_SetTimerConfig(&s_motor2_timer, period2_ticks,
                         TASK2_MOTOR2_PULSE_HIGH_US, ticks_per_us);
    Task2_SetTimerConfig(&s_motor4_timer, period4_ticks,
                         TASK2_MOTOR4_PULSE_HIGH_US, ticks_per_us);

    __disable_irq();
    Task2_SetDirection24(direction);

    s_motor2_remaining = motor2_pulses;
    s_motor4_remaining = motor4_pulses;
    s_active_mask = 0u;
    if (motor2_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M2;
    }
    if (motor4_pulses != 0u)
    {
        s_active_mask |= TASK2_ACTIVE_M4;
    }

    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    Task2_PreparePwm(TIM3, &s_motor2_timer, 3u);
    Task2_PrepareMotor4Timer(TIM1, &s_motor4_timer);

    NVIC_SetPriority(TIM1_UP_IRQn, 0u);
    NVIC_SetPriority(TIM1_CC_IRQn, 0u);
    NVIC_SetPriority(TIM3_IRQn, 0u);
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
    NVIC_EnableIRQ(TIM3_IRQn);
    __enable_irq();

    wait_start_us = Task2_NowUs();
    while ((uint32_t)(Task2_NowUs() - wait_start_us) <
           TASK2_DIR_SETUP_US)
    {
    }

    __disable_irq();
    Task2_Motor24PinsAsTimerOutput();
    TIM1->CNT = 0u;
    TIM3->CNT = 0u;
    s_task2_tim3_active =
        (uint8_t)((s_active_mask & TASK2_ACTIVE_M2) != 0u);

    if ((s_active_mask & TASK2_ACTIVE_M4) != 0u)
    {
        Task2_SetMotor4Pulse(GPIO_PIN_SET);
        TIM1->DIER |= (TIM_DIER_UIE | TIM_DIER_CC1IE);
    }
    if ((s_active_mask & TASK2_ACTIVE_M2) != 0u)
    {
        TIM3->CCER |= TIM_CCER_CC3E;
        TIM3->DIER |= TIM_DIER_CC3IE;
    }

    TIM1->SR = 0u;
    TIM3->SR = 0u;
    TIM3->CR1 |= TIM_CR1_CEN;
    TIM1->CR1 |= TIM_CR1_CEN;
    __enable_irq();
}

/* 供任务3复用: 启动后由TIM3/TIM1在后台产生电机2/4脉冲。 */
void Task2_StartGroup24Motion(uint8_t direction,
                              uint32_t motor2_pulses,
                              uint32_t motor4_pulses)
{
    Task2_StartGroup24(direction, motor2_pulses, motor4_pulses);
}

uint8_t Task2_Group24MotionActive(void)
{
    return (uint8_t)((s_active_mask & TASK2_ACTIVE_GROUP24) != 0u);
}

/* 供任务3复用: 启动后由TIM2/TIM4在后台产生电机1/3脉冲。 */
void Task2_StartGroup13Motion(uint8_t direction,
                              uint32_t motor1_pulses,
                              uint32_t motor3_pulses)
{
    Task2_StartGroup13Pulses(direction,
                             motor1_pulses,
                             motor3_pulses);
}

uint8_t Task2_Group13MotionActive(void)
{
    return (uint8_t)((s_active_mask & TASK2_ACTIVE_GROUP13) != 0u);
}

/* 停止当前动作并清除TIM3所有权, 之后可重新启动手势跟随定时器。 */
void Task2_StopMotion(void)
{
    Task2_StopPwm();
}

void Task2_StopGroup24Motion(void)
{
    Task2_StopMotion();
}

static void Task2_QueuePhase(uint8_t phase, uint32_t delay_ms)
{
    s_next_phase = phase;
    s_settle_duration_ms = delay_ms;
    s_phase = TASK2_PHASE_SETTLE;
    s_phase_change_ms = HAL_GetTick();
}

static void Task2_BeginPhase(uint8_t phase)
{
    uint32_t motor2_pulses;
    uint32_t motor4_pulses;

    s_phase = phase;
    s_phase_change_ms = HAL_GetTick();

    switch (phase)
    {
        case TASK2_PHASE_INIT_OPEN_WAIT:
            Task2_StopPwm();
            ClawServo_Open();
            s_claw_ready = 0u;
            break;

        case TASK2_PHASE_GROUP13_FORWARD:
            Task2_StartGroup13(TASK2_FORWARD_DIR);
            break;

        case TASK2_PHASE_GROUP24_FORWARD_1:
            motor2_pulses = Task2_TurnsToPulses(
                TASK2_MOTOR2_FIRST_FORWARD_TURNS);
            motor4_pulses = Task2_TurnsToPulses(
                TASK2_MOTOR4_FIRST_FORWARD_TURNS);
            Task2_StartGroup24(TASK2_FORWARD_DIR,
                               motor2_pulses, motor4_pulses);
            break;

        case TASK2_PHASE_CLAW_GRAB_WAIT:
            Task2_StopPwm();
            ClawServo_Grab();
            s_claw_ready = 0u;
            break;

        case TASK2_PHASE_GROUP24_REVERSE_1:
        case TASK2_PHASE_GROUP24_REVERSE_2:
            motor2_pulses = Task2_TurnsToPulses(
                TASK2_MOTOR2_REVERSE_TURNS);
            motor4_pulses = Task2_TurnsToPulses(
                TASK2_MOTOR4_REVERSE_TURNS);
            Task2_StartGroup24(TASK2_REVERSE_DIR,
                               motor2_pulses, motor4_pulses);
            break;

        case TASK2_PHASE_GROUP13_REVERSE:
            Task2_StartGroup13(TASK2_REVERSE_DIR);
            break;

        case TASK2_PHASE_GROUP24_FORWARD_2:
            motor2_pulses = Task2_TurnsToPulses(
                TASK2_MOTOR2_SECOND_FORWARD_TURNS);
            motor4_pulses = Task2_TurnsToPulses(
                TASK2_MOTOR4_SECOND_FORWARD_TURNS);
            Task2_StartGroup24(TASK2_FORWARD_DIR,
                               motor2_pulses, motor4_pulses);
            break;

        case TASK2_PHASE_CLAW_OPEN_WAIT:
            Task2_StopPwm();
            ClawServo_Open();
            s_claw_ready = 0u;
            break;

        default:
            Task2_StopPwm();
            s_phase = TASK2_PHASE_STOPPED;
            break;
    }
}

void Task2_Enter(void)
{
    Gesture_ResetValid();
    s_gesture_mode = 0u;
    s_gesture_wait_release = 0u;
    Task2_ResetGestureGrabCount();
    Task2_BeginPhase(TASK2_PHASE_INIT_OPEN_WAIT);
}

#if 0
/* 旧版并行阶段调度, 保留作对照, 不再参与编译。 */
void Task2_Step(void)
{
    if (s_gesture_mode != 0u)
    {
        if (Task2_GesturePresent() == 0u)
        {
            Task2_EndGestureMode();
            return;
        }

        Task1_Step();
        return;
    }

    if (Task2_GesturePresent() != 0u)
    {
        Task2_BeginGestureMode();
        Task1_Step();
        return;
    }

    if (s_phase == TASK2_PHASE_STAGE1)
    {
        if (s_first_cycle != 0u)
        {
            if (s_active_mask == 0u)
            {
                Task2_StopPwm();
                ClawServo_Grab();
                s_claw_ready = 0u;
                s_phase = TASK2_PHASE_STAGE2_WAIT;
                s_phase_change_ms = HAL_GetTick();
            }
        }
        else if ((HAL_GetTick() - s_phase_change_ms) >=
                 s_stage1_task_duration_ms)
        {
            Task2_StopMotor24();
            s_phase = TASK2_PHASE_STAGE1_TASK2_WAIT;
            s_phase_change_ms = HAL_GetTick();
        }
    }
    else if (s_phase == TASK2_PHASE_STAGE2)
    {
        if ((HAL_GetTick() - s_phase_change_ms) >=
            s_stage2_duration_ms)
        {
            Task2_StopMotor24();
            s_phase = TASK2_PHASE_STAGE3_WAIT;
            s_phase_change_ms = HAL_GetTick();
        }
    }
    else if (s_phase == TASK2_PHASE_STAGE3)
    {
        if (s_active_mask == 0u)
        {
            Task2_StopPwm();
            ClawServo_Open();
            s_claw_ready = 0u;
            s_first_cycle = 0u;
            s_phase = TASK2_PHASE_RESTART_WAIT;
            s_phase_change_ms = HAL_GetTick();
        }
    }
    else if (s_phase == TASK2_PHASE_STAGE1_TASK2)
    {
        if (s_active_mask == 0u)
        {
            Task2_StopPwm();
            ClawServo_Grab();
            s_claw_ready = 0u;
            s_phase = TASK2_PHASE_STAGE2_WAIT;
            s_phase_change_ms = HAL_GetTick();
        }
    }
    else if (s_phase == TASK2_PHASE_STAGE2_WAIT)
    {
        if (Task2_ClawDelayDone() != 0u)
        {
            s_phase = TASK2_PHASE_STAGE2;
            Task2_StartStage2();
        }
    }
    else if (s_phase == TASK2_PHASE_STAGE3_WAIT)
    {
        if ((HAL_GetTick() - s_phase_change_ms) >= TASK2_REVERSE_GAP_MS)
        {
            s_phase = TASK2_PHASE_STAGE3;
            Task2_StartStage3();
        }
    }
    else if (s_phase == TASK2_PHASE_STAGE1_TASK2_WAIT)
    {
        if ((HAL_GetTick() - s_phase_change_ms) >= TASK2_REVERSE_GAP_MS)
        {
            s_phase = TASK2_PHASE_STAGE1_TASK2;
            Task2_StartStage1Task2();
        }
    }
    else if (s_phase == TASK2_PHASE_RESTART_WAIT)
    {
        if (Task2_ClawDelayDone() != 0u)
        {
            s_phase = TASK2_PHASE_STAGE1;
            Task2_StartStage1();
        }
    }
}
#endif

void Task2_Step(void)
{
    if (s_gesture_mode != 0u)
    {
        Task1_Step();

        if (Task2_GestureFiveGrabsDone() != 0u)
        {
            Task2_EndGestureMode();
        }
        return;
    }

    if (s_gesture_wait_release != 0u)
    {
        if ((Gesture_HasData() == 0u) ||
            ((Gesture_IsValid() != 0u) &&
             (Gesture_IsClosed() == 0u)))
        {
            s_gesture_wait_release = 0u;
        }
    }

    if ((s_gesture_wait_release == 0u) &&
        (Task2_GestureCanEnter() != 0u))
    {
        Task2_BeginGestureMode();
        Task1_Step();
        return;
    }

    switch (s_phase)
    {
        case TASK2_PHASE_SETTLE:
            if ((HAL_GetTick() - s_phase_change_ms) >=
                s_settle_duration_ms)
            {
                Task2_BeginPhase(s_next_phase);
            }
            break;

        case TASK2_PHASE_INIT_OPEN_WAIT:
        case TASK2_PHASE_CLAW_OPEN_WAIT:
            if (Task2_ClawDelayDone() != 0u)
            {
                if (s_phase == TASK2_PHASE_INIT_OPEN_WAIT)
                {
                    Task2_BeginPhase(TASK2_PHASE_GROUP13_FORWARD);
                }
                else
                {
                    Task2_BeginPhase(TASK2_PHASE_GROUP24_REVERSE_2);
                }
            }
            break;

        case TASK2_PHASE_CLAW_GRAB_WAIT:
            if (Task2_ClawDelayDone() != 0u)
            {
                Task2_BeginPhase(TASK2_PHASE_GROUP24_REVERSE_1);
            }
            break;

        case TASK2_PHASE_GROUP13_FORWARD:
            if ((s_active_mask & TASK2_ACTIVE_GROUP13) == 0u)
            {
                Task2_StopPwm();
                Task2_QueuePhase(TASK2_PHASE_GROUP24_FORWARD_1,
                                 TASK2_GROUP_GAP_MS);
            }
            break;

        case TASK2_PHASE_GROUP24_FORWARD_1:
            if ((s_active_mask & TASK2_ACTIVE_GROUP24) == 0u)
            {
                Task2_StopPwm();
                Task2_QueuePhase(TASK2_PHASE_CLAW_GRAB_WAIT,
                                 TASK2_GROUP_GAP_MS);
            }
            break;

        case TASK2_PHASE_GROUP24_REVERSE_1:
            if ((s_active_mask & TASK2_ACTIVE_GROUP24) == 0u)
            {
                Task2_StopPwm();
                Task2_QueuePhase(TASK2_PHASE_GROUP13_REVERSE,
                                 TASK2_GROUP_GAP_MS);
            }
            break;

        case TASK2_PHASE_GROUP13_REVERSE:
            if ((s_active_mask & TASK2_ACTIVE_GROUP13) == 0u)
            {
                Task2_StopPwm();
                Task2_QueuePhase(TASK2_PHASE_GROUP24_FORWARD_2,
                                 TASK2_GROUP_GAP_MS);
            }
            break;

        case TASK2_PHASE_GROUP24_FORWARD_2:
            if ((s_active_mask & TASK2_ACTIVE_GROUP24) == 0u)
            {
                Task2_StopPwm();
                Task2_QueuePhase(TASK2_PHASE_CLAW_OPEN_WAIT,
                                 TASK2_GROUP_GAP_MS);
            }
            break;

        case TASK2_PHASE_GROUP24_REVERSE_2:
            if ((s_active_mask & TASK2_ACTIVE_GROUP24) == 0u)
            {
                Task2_StopPwm();
                Task2_QueuePhase(TASK2_PHASE_GROUP13_FORWARD,
                                 TASK2_GROUP_GAP_MS);
            }
            break;

        case TASK2_PHASE_STOPPED:
        default:
            break;
    }
}

void Task2_Exit(void)
{
    if (s_gesture_mode != 0u)
    {
        GestureMotor_Stop();
        ClawServo_Hold();
        Task1_Exit();
        s_gesture_mode = 0u;
    }

    s_gesture_wait_release = 0u;
    Task2_ResetGestureGrabCount();
    s_phase = TASK2_PHASE_STOPPED;
    Task2_StopPwm();
    GestureMotor_RestartTimer();
}

/* 供GestureMotor的TIM3中断判断所有权, 避免任务2运行时执行手势步进。 */
uint8_t Task2_TimerPulseActive(void)
{
    return s_task2_tim3_active;
}

/* TIM1更新时电机4拉高。 */
void TIM1_UP_IRQHandler(void)
{
    if ((TIM1->SR & TIM_SR_UIF) != 0u)
    {
        TIM1->SR = (uint16_t)~TIM_SR_UIF;
        if ((s_active_mask & TASK2_ACTIVE_M4) != 0u)
        {
            MOTOR4_PUL_PORT->BSRR = MOTOR4_PUL_PIN;
        }
    }
}

/* TIM1比较时电机4拉低, 并按下降沿计数。 */
void TIM1_CC_IRQHandler(void)
{
    if ((TIM1->SR & TIM_SR_CC1IF) != 0u)
    {
        TIM1->SR = (uint16_t)~TIM_SR_CC1IF;
        MOTOR4_PUL_PORT->BRR = MOTOR4_PUL_PIN;

        if (s_motor4_remaining != 0u)
        {
            s_motor4_remaining--;
            if (s_motor4_remaining == 0u)
            {
                s_active_mask &= (uint8_t)~TASK2_ACTIVE_M4;
                TIM1->DIER &= (uint16_t)~(TIM_DIER_UIE |
                                           TIM_DIER_CC1IE);
                TIM1->CR1 &= ~TIM_CR1_CEN;
                Task2_PulsePinAsGpioLow(MOTOR4_PUL_PORT,
                                        MOTOR4_PUL_PIN);
            }
        }
    }
}

/* TIM2的CH1比较中断记录电机1完成的脉冲数。 */
void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & TIM_SR_CC1IF) == 0u)
    {
        return;
    }

    TIM2->SR = (uint16_t)~TIM_SR_CC1IF;
    if (s_motor1_remaining != 0u)
    {
        s_motor1_remaining--;
        if (s_motor1_remaining == 0u)
        {
            s_active_mask &= (uint8_t)~TASK2_ACTIVE_M1;
            TIM2->CCER &= (uint16_t)~TIM_CCER_CC1E;
            TIM2->DIER &= (uint16_t)~TIM_DIER_CC1IE;
            TIM2->CR1 &= ~TIM_CR1_CEN;
            Task2_PulsePinAsGpioLow(MOTOR1_PUL_PORT,
                                    MOTOR1_PUL_PIN);
        }
    }
}

/* TIM3的CH3比较中断记录电机2完成的脉冲数。 */
void Task2_Timer3PulseIsr(void)
{
    if ((TIM3->SR & TIM_SR_CC3IF) == 0u)
    {
        return;
    }

    TIM3->SR = (uint16_t)~TIM_SR_CC3IF;
    if (s_motor2_remaining != 0u)
    {
        s_motor2_remaining--;
        if (s_motor2_remaining == 0u)
        {
            s_active_mask &= (uint8_t)~TASK2_ACTIVE_M2;
            TIM3->CCER &= (uint16_t)~TIM_CCER_CC3E;
            TIM3->DIER &= (uint16_t)~TIM_DIER_CC3IE;
            TIM3->CR1 &= ~TIM_CR1_CEN;
            Task2_PulsePinAsGpioLow(MOTOR2_PUL_PORT,
                                    MOTOR2_PUL_PIN);
        }
    }
}

/* TIM4的CH1比较中断记录电机3完成的脉冲数。 */
void Task2_Timer4PulseIsr(void)
{
    if ((TIM4->SR & TIM_SR_CC1IF) == 0u)
    {
        return;
    }

    TIM4->SR = (uint16_t)~TIM_SR_CC1IF;
    if (s_motor3_remaining != 0u)
    {
        s_motor3_remaining--;
        if (s_motor3_remaining == 0u)
        {
            s_active_mask &= (uint8_t)~TASK2_ACTIVE_M3;
            TIM4->CCER &= (uint16_t)~TIM_CCER_CC1E;
            TIM4->DIER &= (uint16_t)~TIM_DIER_CC1IE;
            TIM4->CR1 &= ~TIM_CR1_CEN;
            Task2_PulsePinAsGpioLow(MOTOR3_PUL_PORT,
                                    MOTOR3_PUL_PIN);
        }
    }
}
