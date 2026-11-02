/**
  ******************************************************************************
  * @file    task4.c
  * @brief   任务 4: 串口识别到 manul ready! / manual ready! 后进入
  *
  * 电机1: drive[7] 正转, drive[6] 反转
  * 电机2: drive[5] 正转, drive[4] 反转
  * 电机3: drive[2] 正转, drive[3] 反转
  * 电机4: drive[1] 正转, drive[0] 反转
  * 每收到一次对应位为1就转动宏定义圈数, 不需要先收到0。
  ******************************************************************************
  */

#include "task4.h"
#include "fpga_gesture.h"
#include "motor.h"
#include "tim.h"
#include "servo.h"

/* ==================== 每个电机的圈数(支持小数, 0.5f = 半圈) ==================== */
#define TASK4_MOTOR1_FORWARD_TURNS   0.1f
#define TASK4_MOTOR1_REVERSE_TURNS   0.1f

#define TASK4_MOTOR2_FORWARD_TURNS   0.1f
#define TASK4_MOTOR2_REVERSE_TURNS   0.1f

#define TASK4_MOTOR3_FORWARD_TURNS   1.0f
#define TASK4_MOTOR3_REVERSE_TURNS   1.0f

#define TASK4_MOTOR4_FORWARD_TURNS   0.1f
#define TASK4_MOTOR4_REVERSE_TURNS   0.1f

/* ==================== 每个电机的速度 ====================
 * 单位是脉冲间隔(us), 数字越小转得越快。
 * 83us 约 12kHz, 与任务1手势跟随电机的速度相同;
 * 想更快可改 50(约20kHz), 想更慢可改 100、200、500。 */
#define TASK4_MOTOR1_SPEED_US        300u
#define TASK4_MOTOR2_SPEED_US        300u
#define TASK4_MOTOR3_SPEED_US        50u
#define TASK4_MOTOR4_SPEED_US        300u

/* value 每增加1, 对应电机的脉冲间隔减少多少us(间隔越短, 电机越快)。 */
#define TASK4_MOTOR1_VALUE_STEP_US   3u
#define TASK4_MOTOR2_VALUE_STEP_US   4u
#define TASK4_MOTOR3_VALUE_STEP_US   0u   /* 电机3使用下面的两段速度映射 */
#define TASK4_MOTOR4_VALUE_STEP_US   4u

/* 电机3: value=60 达到原 value=100 的速度(3us), value=100 再加速到2us。 */
#define TASK4_MOTOR3_KNEE_VALUE       60u
#define TASK4_MOTOR3_KNEE_SPEED_US     3u
#define TASK4_MOTOR3_MAX_VALUE       100u
#define TASK4_MOTOR3_MAX_SPEED_US      1u
/* 新value=100对应旧value=79, 整条电机3曲线按79/100缩放。 */
#define TASK4_MOTOR3_VALUE_SCALE      79u

/* 各通道独立的倍频系数: 实际脉冲间隔 = 调速计算出的间隔 / 倍率。 */
#define TASK4_MOTOR1_SPEED_MULTIPLIER  1u
#define TASK4_MOTOR2_SPEED_MULTIPLIER  1u
#define TASK4_MOTOR3_SPEED_MULTIPLIER  1u
#define TASK4_MOTOR4_SPEED_MULTIPLIER  1u

/* 调速后的安全范围, 防止脉冲过快导致丢步。 */
#define TASK4_SPEED_MIN_US           50u
#define TASK4_MOTOR2_MIN_SPEED_US    30u
#define TASK4_MOTOR3_MIN_SPEED_US     1u
#define TASK4_MOTOR4_MIN_SPEED_US    30u
#define TASK4_SPEED_MAX_US           10000u

/* 电机3 使用 TIM4_CH1(PB6) 硬件产生脉冲。
 * PSC=0时TIM4按72MHz计数, 可以生成小于1us的脉宽。 */
#define TASK4_MOTOR3_TIMER_MIN_US    1u
#define TASK4_MOTOR3_TIMER_PSC       0u
#define TASK4_MOTOR3_PULSE_HIGH_US   2u

/* 任务4舵机: drive[8]正方向, drive[9]反方向。
 * 每收到一次1, 目标角度增加/减少对应度数; 0立即停住当前角度。 */
#define TASK4_SERVO_CHANNEL          2u
#define TASK4_SERVO_FORWARD_INDEX    8u
#define TASK4_SERVO_REVERSE_INDEX    9u
#define TASK4_SERVO_FORWARD_STEP_DEG 5
#define TASK4_SERVO_REVERSE_STEP_DEG 5
#define TASK4_SERVO_MOVE_STEP_DEG    1u
#define TASK4_SERVO_MOVE_STEP_MS     10u

#define TASK4_MOTOR_COUNT            4u

typedef struct
{
    uint32_t forward_seen;
    uint32_t reverse_seen;
    uint32_t forward_pending;
    uint32_t reverse_pending;
    uint32_t pulses_left;
    uint32_t last_pulse_us;
    uint8_t  moving;
    uint8_t  moving_dir;
    uint8_t  active_index;      /* 当前实际启动该电机的 drive[x] */
} Task4MotorState;

#define TASK4_NO_DRIVE_INDEX         0xFFu

/* 下标顺序固定为 电机1、电机2、电机3、电机4。 */
static const uint8_t s_motors[TASK4_MOTOR_COUNT] = {
    MOTOR_1, MOTOR_2, MOTOR_3, MOTOR_4
};
static const uint8_t s_forward_index[TASK4_MOTOR_COUNT] = {7u, 5u, 2u, 1u};
static const uint8_t s_reverse_index[TASK4_MOTOR_COUNT] = {6u, 4u, 3u, 0u};
static const float s_forward_turns[TASK4_MOTOR_COUNT] = {
    TASK4_MOTOR1_FORWARD_TURNS,
    TASK4_MOTOR2_FORWARD_TURNS,
    TASK4_MOTOR3_FORWARD_TURNS,
    TASK4_MOTOR4_FORWARD_TURNS
};
static const float s_reverse_turns[TASK4_MOTOR_COUNT] = {
    TASK4_MOTOR1_REVERSE_TURNS,
    TASK4_MOTOR2_REVERSE_TURNS,
    TASK4_MOTOR3_REVERSE_TURNS,
    TASK4_MOTOR4_REVERSE_TURNS
};
static uint16_t s_speed_us[TASK4_MOTOR_COUNT] = {
    TASK4_MOTOR1_SPEED_US,
    TASK4_MOTOR2_SPEED_US,
    TASK4_MOTOR3_SPEED_US,
    TASK4_MOTOR4_SPEED_US
};
static const uint16_t s_value_step_us[TASK4_MOTOR_COUNT] = {
    TASK4_MOTOR1_VALUE_STEP_US,
    TASK4_MOTOR2_VALUE_STEP_US,
    TASK4_MOTOR3_VALUE_STEP_US,
    TASK4_MOTOR4_VALUE_STEP_US
};
static const uint16_t s_speed_multiplier[TASK4_MOTOR_COUNT] = {
    TASK4_MOTOR1_SPEED_MULTIPLIER,
    TASK4_MOTOR2_SPEED_MULTIPLIER,
    TASK4_MOTOR3_SPEED_MULTIPLIER,
    TASK4_MOTOR4_SPEED_MULTIPLIER
};
static const uint16_t s_min_speed_us[TASK4_MOTOR_COUNT] = {
    TASK4_SPEED_MIN_US,
    TASK4_MOTOR2_MIN_SPEED_US,
    TASK4_MOTOR3_MIN_SPEED_US,
    TASK4_MOTOR4_MIN_SPEED_US
};
static Task4MotorState s_motor_state[TASK4_MOTOR_COUNT];
static uint32_t s_drive_seq_seen[GEST_DRIVE_FIELD_COUNT];
static uint32_t s_slider_seq_seen;
static volatile uint8_t  s_m3_timer_active;
static DMA_HandleTypeDef s_m3_dma;
static volatile uint32_t s_m3_dma_dummy;
static volatile uint32_t s_m3_dma_remaining;
static uint32_t s_servo_forward_seen;
static uint32_t s_servo_reverse_seen;
static int16_t  s_servo_target_angle;
static uint8_t  s_servo_active_index;

static uint32_t Task4_NowUs(void)
{
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0u)
    {
        return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
    }

    return (HAL_GetTick() * 1000u) +
           ((SysTick->LOAD - SysTick->VAL) / (SystemCoreClock / 1000000u));
}

static uint32_t Task4_TurnsToPulses(float turns)
{
    if (turns <= 0.0f)
    {
        return 0u;
    }

    return (uint32_t)(turns * (float)PULSE_PER_REV + 0.5f);
}

static int16_t Task4_ServoLimitAngle(int32_t angle)
{
    if (angle > SERVO_ANGLE_MAX)
    {
        return SERVO_ANGLE_MAX;
    }
    if (angle < SERVO_ANGLE_MIN)
    {
        return SERVO_ANGLE_MIN;
    }

    return (int16_t)angle;
}

/* drive[8]/drive[9]的0: 放弃剩余缓动并停在当前角度。 */
static void Task4_ServoStop(void)
{
    int16_t angle = Servo_GetAngle(TASK4_SERVO_CHANNEL);

    s_servo_target_angle = angle;
    s_servo_active_index = TASK4_NO_DRIVE_INDEX;
    Servo_SetAngle(TASK4_SERVO_CHANNEL, angle);
}

/* 每次正/反向命令累加一个角度步长; 反向命令立即从当前位置重新出发。 */
static void Task4_ServoStep(uint8_t index, uint32_t count)
{
    int32_t target;
    int32_t delta;
    int16_t current;

    if (count == 0u)
    {
        return;
    }

    current = Servo_GetAngle(TASK4_SERVO_CHANNEL);

    if (s_servo_active_index != index)
    {
        target = current;
    }
    else
    {
        target = s_servo_target_angle;
    }

    if (index == TASK4_SERVO_FORWARD_INDEX)
    {
        delta = (int32_t)(count * (uint32_t)TASK4_SERVO_FORWARD_STEP_DEG);
    }
    else
    {
        delta = -(int32_t)(count * (uint32_t)TASK4_SERVO_REVERSE_STEP_DEG);
    }

    s_servo_target_angle = Task4_ServoLimitAngle(target + delta);
    s_servo_active_index = index;
    Servo_SetAngleSlow(TASK4_SERVO_CHANNEL, s_servo_target_angle,
                       TASK4_SERVO_MOVE_STEP_DEG,
                       TASK4_SERVO_MOVE_STEP_MS);
}

/* value=0 使用基础速度; value每增加1, 各电机按自己的宏缩短脉冲间隔。 */
static void Task4_ApplySliderSpeed(void)
{
    uint32_t value = 0u;
    uint32_t period_us;
    uint32_t step_us;
    uint32_t min_us;
    uint32_t motor3_value;
    uint8_t i;

    if (Gesture_SliderValueValid() != 0u)
    {
        if (Gesture_SliderValue() > 0)
        {
            value = (uint32_t)Gesture_SliderValue();
        }
    }

    if (value > 100u)
    {
        value = 100u;
    }
    motor3_value = value * TASK4_MOTOR3_VALUE_SCALE / 100u;

    s_speed_us[0] = TASK4_MOTOR1_SPEED_US;
    s_speed_us[1] = TASK4_MOTOR2_SPEED_US;
    s_speed_us[2] = TASK4_MOTOR3_SPEED_US;
    s_speed_us[3] = TASK4_MOTOR4_SPEED_US;

    for (i = 0u; i < TASK4_MOTOR_COUNT; i++)
    {
        period_us = (uint32_t)s_speed_us[i];
        min_us = s_min_speed_us[i];

        if (i == 2u)
        {
            if (motor3_value <= TASK4_MOTOR3_KNEE_VALUE)
            {
                period_us = (uint32_t)TASK4_MOTOR3_SPEED_US -
                    (((uint32_t)TASK4_MOTOR3_SPEED_US -
                      TASK4_MOTOR3_KNEE_SPEED_US) * motor3_value /
                     TASK4_MOTOR3_KNEE_VALUE);
            }
            else
            {
                period_us = (uint32_t)TASK4_MOTOR3_KNEE_SPEED_US -
                    (((uint32_t)TASK4_MOTOR3_KNEE_SPEED_US -
                      TASK4_MOTOR3_MAX_SPEED_US) *
                     (motor3_value - TASK4_MOTOR3_KNEE_VALUE) /
                     (TASK4_MOTOR3_MAX_VALUE -
                      TASK4_MOTOR3_KNEE_VALUE));
            }
        }
        else
        {
            step_us = value * (uint32_t)s_value_step_us[i];
            if (period_us > step_us)
            {
                period_us -= step_us;
            }
            else
            {
                period_us = 0u;
            }
        }

        if (period_us < min_us)
        {
            period_us = min_us;
        }
        else if (period_us > TASK4_SPEED_MAX_US)
        {
            period_us = TASK4_SPEED_MAX_US;
        }

        if (s_speed_multiplier[i] > 1u)
        {
            period_us /= (uint32_t)s_speed_multiplier[i];

            if (period_us == 0u)
            {
                period_us = 1u;
            }
        }

        s_speed_us[i] = (uint16_t)period_us;
    }
}

/* 停表后把 PB6 恢复成普通输出并拉低, 保证停止时没有残余脉冲。 */
static void Task4_Motor3PinLow(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin   = MOTOR3_PUL_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MOTOR3_PUL_PORT, &gpio);
    HAL_GPIO_WritePin(MOTOR3_PUL_PORT, MOTOR3_PUL_PIN, GPIO_PIN_RESET);
}

static void Task4_Motor3TimerStop(void)
{
    s_m3_timer_active = 0u;
    s_m3_dma_remaining = 0u;

    __HAL_TIM_DISABLE_DMA(&htim4, TIM_DMA_UPDATE);
    TIM4->DIER &= ~TIM_DIER_UIE;
    TIM4->CCER &= ~TIM_CCER_CC1E;
    TIM4->CR1  &= ~TIM_CR1_CEN;
    TIM4->SR    = (uint16_t)~TIM_SR_UIF;

    if (s_m3_dma.State == HAL_DMA_STATE_BUSY)
    {
        HAL_DMA_Abort(&s_m3_dma);
    }
    DMA1->IFCR = DMA_IFCR_CGIF7;

    Task4_Motor3PinLow();
}

static void Task4_Motor3TimerInit(void)
{
    TIM_OC_InitTypeDef oc = {0};
    GPIO_InitTypeDef gpio = {0};

    Task4_Motor3TimerStop();

    __HAL_RCC_DMA1_CLK_ENABLE();

    s_m3_dma.Instance                 = DMA1_Channel7; /* TIM4_UP */
    s_m3_dma.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    s_m3_dma.Init.PeriphInc           = DMA_PINC_DISABLE;
    s_m3_dma.Init.MemInc              = DMA_MINC_DISABLE;
    s_m3_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    s_m3_dma.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
    s_m3_dma.Init.Mode                = DMA_NORMAL;
    s_m3_dma.Init.Priority            = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&s_m3_dma);
    __HAL_LINKDMA(&htim4, hdma[TIM_DMA_ID_UPDATE], s_m3_dma);

    NVIC_SetPriority(DMA1_Channel7_IRQn, 0u);
    NVIC_EnableIRQ(DMA1_Channel7_IRQn);

    /* 停止后需要把 PB6 从TIM4复用输出切回普通输出。 */
    gpio.Pin   = MOTOR3_PUL_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MOTOR3_PUL_PORT, &gpio);

    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = TASK4_MOTOR3_PULSE_HIGH_US;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim4, &oc, TIM_CHANNEL_1);
}

static void Task4_Motor3TimerSetPeriod(uint32_t period_us)
{
    uint32_t ticks_per_us;
    uint32_t period_ticks;
    uint32_t high_ticks;

    if (period_us < TASK4_MOTOR3_TIMER_MIN_US)
    {
        period_us = TASK4_MOTOR3_TIMER_MIN_US;
    }

    ticks_per_us = SystemCoreClock / 1000000u;
    period_ticks = period_us * ticks_per_us;
    high_ticks = (uint32_t)TASK4_MOTOR3_PULSE_HIGH_US * ticks_per_us;

    if (high_ticks >= period_ticks)
    {
        high_ticks = period_ticks / 2u;
        if (high_ticks == 0u)
        {
            high_ticks = 1u;
        }
    }

    TIM4->ARR = period_ticks - 1u;
    TIM4->CCR1 = high_ticks;
}

static void Task4_Motor3DmaStartChunk(uint32_t pulses)
{
    uint32_t chunk = pulses;

    if (chunk > 0xFFFFu)
    {
        chunk = 0xFFFFu;
    }
    if (chunk == 0u)
    {
        return;
    }

    s_m3_dma_remaining = pulses - chunk;

    if (HAL_DMA_Start_IT(&s_m3_dma, (uint32_t)&TIM4->CNT,
                         (uint32_t)&s_m3_dma_dummy, chunk) != HAL_OK)
    {
        s_m3_dma_remaining = 0u;
    }
}

static void Task4_Motor3TimerStart(uint32_t period_us, uint32_t pulses,
                                   uint8_t dir)
{
    GPIO_InitTypeDef gpio = {0};

    Task4_Motor3TimerStop();

    HAL_GPIO_WritePin(MOTOR3_DIR_PORT, MOTOR3_DIR_PIN,
                      (GPIO_PinState)(dir ^ MOTOR3_DIR_INVERT));

    gpio.Pin   = MOTOR3_PUL_PIN;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MOTOR3_PUL_PORT, &gpio);

    TIM4->PSC = TASK4_MOTOR3_TIMER_PSC;
    Task4_Motor3TimerSetPeriod(period_us);
    TIM4->CNT = 0u;
    TIM4->EGR = TIM_EGR_UG;
    TIM4->SR  = (uint16_t)~TIM_SR_UIF;

    Task4_Motor3DmaStartChunk(pulses);
    if (s_m3_dma.State != HAL_DMA_STATE_BUSY)
    {
        Task4_Motor3TimerStop();
        s_motor_state[2].pulses_left = 0u;
        s_motor_state[2].moving = 0u;
        return;
    }

    s_m3_timer_active = 1u;

    __HAL_TIM_ENABLE_DMA(&htim4, TIM_DMA_UPDATE);
    TIM4->CCER |= TIM_CCER_CC1E;
    TIM4->CR1  |= TIM_CR1_CEN;
}

/* TIM4_UP 每产生一个脉冲, DMA 完成一次计数, 不占用每拍CPU中断。 */
void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma)
{
    if (hdma != &s_m3_dma)
    {
        return;
    }

    if (s_m3_dma_remaining > 0u)
    {
        uint32_t remaining = s_m3_dma_remaining;

        Task4_Motor3DmaStartChunk(remaining);
        if (s_m3_dma.State == HAL_DMA_STATE_BUSY)
        {
            return;
        }
    }

    Task4_Motor3TimerStop();
    s_motor_state[2].pulses_left = 0u;
    s_motor_state[2].moving = 0u;
}

void HAL_DMA_XferErrorCallback(DMA_HandleTypeDef *hdma)
{
    if (hdma != &s_m3_dma)
    {
        return;
    }

    Task4_Motor3TimerStop();
    s_motor_state[2].pulses_left = 0u;
    s_motor_state[2].moving = 0u;
}

void DMA1_Channel7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_m3_dma);
}

/* drive[x]=0: 只停止由 drive[x]=1 启动的那台电机, 并丢弃它的剩余命令。 */
static void Task4_StopDriveIndex(uint8_t index)
{
    Task4MotorState *state;
    uint8_t i;

    for (i = 0u; i < TASK4_MOTOR_COUNT; i++)
    {
        state = &s_motor_state[i];
        if (state->active_index == index)
        {
            if (i == 2u)
            {
                Task4_Motor3TimerStop();
            }

            state->forward_pending = 0u;
            state->reverse_pending = 0u;
            state->pulses_left = 0u;
            state->moving = 0u;
            state->active_index = TASK4_NO_DRIVE_INDEX;
        }
    }
}

/* 切换到任务 4 时调用一次。 */
void Task4_Enter(void)
{
    uint32_t now_us = Task4_NowUs();
    uint8_t i;

    /* 只处理进入任务4之后收到的 drive[]=1, 不执行进入前的旧命令。 */
    for (i = 0u; i < TASK4_MOTOR_COUNT; i++)
    {
        s_motor_state[i].forward_seen =
            Gesture_DriveOneCount(s_forward_index[i]);
        s_motor_state[i].reverse_seen =
            Gesture_DriveOneCount(s_reverse_index[i]);
        s_motor_state[i].forward_pending = 0u;
        s_motor_state[i].reverse_pending = 0u;
        s_motor_state[i].pulses_left = 0u;
        s_motor_state[i].last_pulse_us = now_us;
        s_motor_state[i].moving = 0u;
        s_motor_state[i].moving_dir = 0u;
        s_motor_state[i].active_index = TASK4_NO_DRIVE_INDEX;
    }

    for (i = 0u; i < GEST_DRIVE_FIELD_COUNT; i++)
    {
        s_drive_seq_seen[i] = Gesture_DriveChangeSeq(i);
    }

    s_servo_forward_seen = Gesture_DriveOneCount(TASK4_SERVO_FORWARD_INDEX);
    s_servo_reverse_seen = Gesture_DriveOneCount(TASK4_SERVO_REVERSE_INDEX);
    s_servo_target_angle = Servo_GetAngle(TASK4_SERVO_CHANNEL);
    s_servo_active_index = TASK4_NO_DRIVE_INDEX;
    Servo_SetAngle(TASK4_SERVO_CHANNEL, s_servo_target_angle);

    s_slider_seq_seen = Gesture_SliderValueSeq();
    Task4_ApplySliderSpeed();
    Task4_Motor3TimerInit();
}

/* 任务 4 运行期间反复调用。 */
void Task4_Step(void)
{
    Task4MotorState *state;
    uint32_t forward_count;
    uint32_t reverse_count;
    uint32_t forward_new;
    uint32_t reverse_new;
    uint32_t servo_forward_count;
    uint32_t servo_reverse_count;
    uint32_t servo_forward_new;
    uint32_t servo_reverse_new;
    uint32_t change_seq;
    uint32_t now_us;
    uint8_t i;

    now_us = Task4_NowUs();

    /* 如果任务4运行期间收到新的value, 立即更新四路速度, 不改动动作状态。 */
    if (Gesture_SliderValueSeq() != s_slider_seq_seen)
    {
        s_slider_seq_seen = Gesture_SliderValueSeq();
        Task4_ApplySliderSpeed();

        if (s_motor_state[2].moving != 0u)
        {
            Task4_Motor3TimerSetPeriod(s_speed_us[2]);
        }
    }

    /* 先处理 drive[x]=0: 不用等当前圈数转完, 当轮立即停止对应电机。 */
    for (i = 0u; i < GEST_DRIVE_FIELD_COUNT; i++)
    {
        change_seq = Gesture_DriveChangeSeq(i);
        if (change_seq != s_drive_seq_seen[i])
        {
            s_drive_seq_seen[i] = change_seq;

            if ((Gesture_DriveValueValid(i) != 0u) &&
                (Gesture_DriveValue(i) == 0u))
            {
                Task4_StopDriveIndex(i);

                if (((i == TASK4_SERVO_FORWARD_INDEX) ||
                     (i == TASK4_SERVO_REVERSE_INDEX)) &&
                    (s_servo_active_index == i))
                {
                    Task4_ServoStop();
                }
            }
        }
    }

    servo_forward_count =
        Gesture_DriveOneCount(TASK4_SERVO_FORWARD_INDEX);
    servo_reverse_count =
        Gesture_DriveOneCount(TASK4_SERVO_REVERSE_INDEX);
    servo_forward_new = servo_forward_count - s_servo_forward_seen;
    servo_reverse_new = servo_reverse_count - s_servo_reverse_seen;
    s_servo_forward_seen = servo_forward_count;
    s_servo_reverse_seen = servo_reverse_count;

    if ((servo_forward_new != 0u) && (servo_reverse_new == 0u))
    {
        Task4_ServoStep(TASK4_SERVO_FORWARD_INDEX, servo_forward_new);
    }
    else if ((servo_reverse_new != 0u) && (servo_forward_new == 0u))
    {
        Task4_ServoStep(TASK4_SERVO_REVERSE_INDEX, servo_reverse_new);
    }

    for (i = 0u; i < TASK4_MOTOR_COUNT; i++)
    {
        state = &s_motor_state[i];
        forward_count = Gesture_DriveOneCount(s_forward_index[i]);
        reverse_count = Gesture_DriveOneCount(s_reverse_index[i]);
        forward_new = forward_count - state->forward_seen;
        reverse_new = reverse_count - state->reverse_seen;

        state->forward_seen = forward_count;
        state->reverse_seen = reverse_count;

        /* 同一个电机同一帧同时收到正反转命令时不动作, 避免方向冲突。 */
        if ((forward_new != 0u) && (reverse_new != 0u))
        {
            forward_new = 0u;
            reverse_new = 0u;
        }

        state->forward_pending += forward_new;
        state->reverse_pending += reverse_new;

        /* 反向抢占: 当前圈数没转完也立即舍弃, 并清掉旧方向的排队命令。
         * 新方向命令保留, 在本轮下面直接启动。 */
        if ((state->moving != 0u) &&
            (((state->moving_dir == 0u) && (reverse_new != 0u)) ||
             ((state->moving_dir != 0u) && (forward_new != 0u))))
        {
            if (state->moving_dir == 0u)
            {
                state->forward_pending = 0u;
            }
            else
            {
                state->reverse_pending = 0u;
            }

            state->pulses_left = 0u;
            state->moving = 0u;

            if (i == 2u)
            {
                Task4_Motor3TimerStop();
            }
        }

        /* 每个电机独立、非阻塞地发脉冲, 可以同时运行。 */
        if (state->moving == 0u)
        {
            if (state->forward_pending > 0u)
            {
                state->forward_pending--;
                state->moving_dir = 0u;
                state->pulses_left = Task4_TurnsToPulses(s_forward_turns[i]);
                state->active_index = s_forward_index[i];
            }
            else if (state->reverse_pending > 0u)
            {
                state->reverse_pending--;
                state->moving_dir = 1u;
                state->pulses_left = Task4_TurnsToPulses(s_reverse_turns[i]);
                state->active_index = s_reverse_index[i];
            }
            else
            {
                continue;
            }

            if (state->pulses_left == 0u)
            {
                continue;
            }

            if (i == 2u)
            {
                state->moving = 1u;
                Task4_Motor3TimerStart(s_speed_us[2], state->pulses_left,
                                       state->moving_dir);
                continue;
            }

            state->last_pulse_us = now_us;
            state->moving = 1u;
        }

        if (i == 2u)
        {
            continue;                   /* 电机3由TIM4硬件输出脉冲 */
        }

        if ((uint32_t)(now_us - state->last_pulse_us) >= s_speed_us[i])
        {
            state->last_pulse_us = now_us;
            Motor_Step(s_motors[i], state->moving_dir);
            state->pulses_left--;

            if (state->pulses_left == 0u)
            {
                state->moving = 0u;
            }
        }
    }
}

/* 离开任务 4 时调用一次。
 * 公共电机/舵机仍由 APP.c 的 app_task_switch() 统一停。 */
void Task4_Exit(void)
{
    uint8_t i;

    Task4_ServoStop();
    s_servo_forward_seen = Gesture_DriveOneCount(TASK4_SERVO_FORWARD_INDEX);
    s_servo_reverse_seen = Gesture_DriveOneCount(TASK4_SERVO_REVERSE_INDEX);

    Task4_Motor3TimerStop();

    for (i = 0u; i < TASK4_MOTOR_COUNT; i++)
    {
        s_motor_state[i].forward_seen =
            Gesture_DriveOneCount(s_forward_index[i]);
        s_motor_state[i].reverse_seen =
            Gesture_DriveOneCount(s_reverse_index[i]);
        s_motor_state[i].forward_pending = 0u;
        s_motor_state[i].reverse_pending = 0u;
        s_motor_state[i].pulses_left = 0u;
        s_motor_state[i].moving = 0u;
        s_motor_state[i].active_index = TASK4_NO_DRIVE_INDEX;
    }
}
