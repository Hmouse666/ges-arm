/**
  ******************************************************************************
  * @file    task3.c
  * @brief   任务3: 任务1式手势跟随 -> 连续10帧抓取后锁定舵机
  *         -> 非2跳变到2后连续10帧2指 -> 小任务1
  *         -> 非3跳变到3后连续10帧3指 -> 小任务2
  *
  * 十次抓取确认前严格复用任务1控制, 夹爪随手势开合。
  * 十次抓取确认后夹爪锁定在抓取状态, 电机继续随手势运动。
  * 锁爪后、进入小任务前连续十帧松开可解除锁爪并恢复手势控制。
  * 检测到手指从0变为非0后静默100ms, 再用新帧识别小任务。
  *
  * 两个小任务都先执行: 电机2/4同时反转1圈。
  *
  * 小任务1: 电机1/3正转 -> 电机2/4正转 -> 夹爪松开
  *          -> 电机2/4反转 -> 电机1/3反转 -> 电机2/4正转。
  * 小任务2: 电机1/3反转 -> 电机2/4正转 -> 夹爪松开
  *          -> 电机2/4反转 -> 电机1/3正转 -> 电机2/4正转。
  *
  * 两个小任务完成后均恢复手势跟随, 并重新识别下一次触发指令。
  ******************************************************************************
  */

#include "task3.h"
#include "main.h"
#include "claw_servo.h"
#include "fpga_gesture.h"
#include "gesture_motor.h"
#include "motor.h"
#include "task1.h"
#include "task2.h"

/* ==================== 任务3参数 ==================== */

#define TASK3_GRAB_CONFIRM_FRAMES         10u
#define TASK3_UNLOCK_OPEN_FRAMES          10u
#define TASK3_FINGER_CONFIRM_FRAMES       10u
#define TASK3_NONZERO_DELAY_MS           100u

/* 小任务1和小任务2分别配置电机1/3圈数, 此处数值可按实机继续标定。 */
/* 小任务1按原比例缩小10%, 正反转保持一致。 */
#define TASK3_SUB1_MOTOR1_FORWARD_TURNS    2.5f
#define TASK3_SUB1_MOTOR3_FORWARD_TURNS   24.0f
#define TASK3_SUB1_MOTOR1_REVERSE_TURNS    2.5f
#define TASK3_SUB1_MOTOR3_REVERSE_TURNS   24.0f
/* 小任务2按原比例缩小10%, 正反转保持一致。 */
#define TASK3_SUB2_MOTOR1_FORWARD_TURNS    1.4f
#define TASK3_SUB2_MOTOR3_FORWARD_TURNS    1.4f
#define TASK3_SUB2_MOTOR1_REVERSE_TURNS    1.4f
#define TASK3_SUB2_MOTOR3_REVERSE_TURNS    1.4f

#define TASK3_MOTOR2_FORWARD_TURNS        1.0f
#define TASK3_MOTOR4_FORWARD_TURNS        1.0f
#define TASK3_MOTOR2_REVERSE_TURNS        1.0f
#define TASK3_MOTOR4_REVERSE_TURNS        1.0f

/* 进入任一小任务后、电机1/3启动前的同步预动作。 */
#define TASK3_PRE_MOTOR2_REVERSE_TURNS     1.0f
#define TASK3_PRE_MOTOR4_REVERSE_TURNS     1.0f

/* 任务3内部的小任务编号。 */
#define TASK3_SUBTASK_NONE                 0u
#define TASK3_SUBTASK_1                    1u
#define TASK3_SUBTASK_2                    2u

/* 逻辑方向: 0 = 正转, 1 = 反转, 与任务2一致。 */
#define TASK3_DIR_FORWARD                  0u
#define TASK3_DIR_REVERSE                  1u
#define TASK3_MOTOR24_FORWARD_DIR          TASK3_DIR_FORWARD
#define TASK3_MOTOR24_REVERSE_DIR          TASK3_DIR_REVERSE

/* 第一段电机1/3动作和第二段电机1/3动作的方向。 */
#define TASK3_SUB1_M13_FIRST_DIR           TASK3_DIR_FORWARD
#define TASK3_SUB1_M13_SECOND_DIR          TASK3_DIR_REVERSE
#define TASK3_SUB2_M13_FIRST_DIR           TASK3_DIR_REVERSE
#define TASK3_SUB2_M13_SECOND_DIR          TASK3_DIR_FORWARD

#define TASK3_GROUP_GAP_MS                 20u
#define TASK3_CLAW_SETTLE_MS              500u

/* 任务3阶段。 */
#define TASK3_PHASE_GESTURE                 0u
#define TASK3_PHASE_WAIT                    1u
#define TASK3_PHASE_M13_FIRST               2u
#define TASK3_PHASE_M24_FORWARD1            3u
#define TASK3_PHASE_CLAW_OPEN_WAIT          4u
#define TASK3_PHASE_M24_REVERSE1            5u
#define TASK3_PHASE_M13_SECOND              6u
#define TASK3_PHASE_M24_FORWARD2            7u
#define TASK3_PHASE_RESUME_GESTURE          8u
#define TASK3_PHASE_M24_PRE_REVERSE         9u

static uint8_t  s_phase;
static uint8_t  s_next_phase;
static uint32_t s_wait_ms;
static uint32_t s_wait_duration_ms;

/* 第一个有效状态只作为基准, 之后只认新的松开 -> 抓取边沿。 */
static uint32_t s_state_seq_seen;
static uint8_t s_prev_closed;
static uint8_t s_prev_closed_valid;
static uint8_t s_grab_count;
static uint8_t s_claw_locked;
static uint32_t s_unlock_state_seq_seen;
static uint8_t s_unlock_open_count;

/* 抓取指令完成后, 分别统计2指和3指的跳变及连续帧。 */
static uint32_t s_finger_seq_seen;
static uint8_t  s_finger2_count;
static uint8_t  s_finger2_armed;
static uint8_t  s_finger3_count;
static uint8_t  s_finger3_armed;
static uint8_t  s_subtask;
static uint8_t  s_nonzero_delay_active;
static uint8_t  s_nonzero_delay_done;
static uint32_t s_nonzero_delay_ms;

/* 等待舵机真正到位后, 再从到位时刻开始计稳定时间。 */
static uint8_t  s_claw_ready;
static uint32_t s_claw_settled_ms;

static uint32_t Task3_TurnsToPulses(float turns)
{
    if (turns <= 0.0f)
    {
        return 0u;
    }

    return (uint32_t)(turns * (float)PULSE_PER_REV + 0.5f);
}

static void Task3_ResetGestureTracking(void)
{
    s_state_seq_seen    = Gesture_StateSeq();
    s_prev_closed       = 0u;
    s_prev_closed_valid = 0u;
    s_grab_count        = 0u;
    s_claw_locked       = 0u;
    s_unlock_state_seq_seen = Gesture_StateSeq();
    s_unlock_open_count     = 0u;
    s_finger_seq_seen   = Gesture_FingerSeq();
    s_finger2_count     = 0u;
    s_finger2_armed     = 0u;
    s_finger3_count     = 0u;
    s_finger3_armed     = 0u;
    s_subtask           = TASK3_SUBTASK_NONE;
    s_nonzero_delay_active = 0u;
    s_nonzero_delay_done   = 0u;
    s_nonzero_delay_ms     = HAL_GetTick();
}

static void Task3_StartWait(uint8_t next_phase, uint32_t delay_ms)
{
    s_next_phase       = next_phase;
    s_wait_duration_ms = delay_ms;
    s_wait_ms          = HAL_GetTick();
    s_phase            = TASK3_PHASE_WAIT;
}

static uint8_t Task3_IsSubtask2(void)
{
    return (uint8_t)(s_subtask == TASK3_SUBTASK_2);
}

static void Task3_BeginMotor13First(void)
{
    uint8_t is_subtask2 = Task3_IsSubtask2();
    uint8_t direction = (is_subtask2 != 0u) ?
                        TASK3_SUB2_M13_FIRST_DIR : TASK3_SUB1_M13_FIRST_DIR;
    float motor1_turns = (is_subtask2 != 0u) ?
                         TASK3_SUB2_MOTOR1_REVERSE_TURNS :
                         TASK3_SUB1_MOTOR1_FORWARD_TURNS;
    float motor3_turns = (is_subtask2 != 0u) ?
                         TASK3_SUB2_MOTOR3_REVERSE_TURNS :
                         TASK3_SUB1_MOTOR3_FORWARD_TURNS;

    Task2_StartGroup13Motion(
        direction,
        Task3_TurnsToPulses(motor1_turns),
        Task3_TurnsToPulses(motor3_turns));
    s_phase = TASK3_PHASE_M13_FIRST;
}

static void Task3_BeginMotor13Second(void)
{
    uint8_t is_subtask2 = Task3_IsSubtask2();
    uint8_t direction = (is_subtask2 != 0u) ?
                        TASK3_SUB2_M13_SECOND_DIR : TASK3_SUB1_M13_SECOND_DIR;
    float motor1_turns = (is_subtask2 != 0u) ?
                         TASK3_SUB2_MOTOR1_FORWARD_TURNS :
                         TASK3_SUB1_MOTOR1_REVERSE_TURNS;
    float motor3_turns = (is_subtask2 != 0u) ?
                         TASK3_SUB2_MOTOR3_FORWARD_TURNS :
                         TASK3_SUB1_MOTOR3_REVERSE_TURNS;

    Task2_StartGroup13Motion(
        direction,
        Task3_TurnsToPulses(motor1_turns),
        Task3_TurnsToPulses(motor3_turns));
    s_phase = TASK3_PHASE_M13_SECOND;
}

static void Task3_BeginMotor24Forward(uint8_t phase)
{
    Task2_StartGroup24Motion(
        TASK3_MOTOR24_FORWARD_DIR,
        Task3_TurnsToPulses(TASK3_MOTOR2_FORWARD_TURNS),
        Task3_TurnsToPulses(TASK3_MOTOR4_FORWARD_TURNS));
    s_phase = phase;
}

static void Task3_BeginPreMotor24Reverse(void)
{
    Task2_StartGroup24Motion(
        TASK3_MOTOR24_REVERSE_DIR,
        Task3_TurnsToPulses(TASK3_PRE_MOTOR2_REVERSE_TURNS),
        Task3_TurnsToPulses(TASK3_PRE_MOTOR4_REVERSE_TURNS));
    s_phase = TASK3_PHASE_M24_PRE_REVERSE;
}

static void Task3_BeginClawOpen(void)
{
    Task2_StopMotion();
    ClawServo_Open();
    s_claw_ready      = 0u;
    s_claw_settled_ms = HAL_GetTick();
    s_phase           = TASK3_PHASE_CLAW_OPEN_WAIT;
}

static void Task3_BeginMotor24Reverse(uint8_t phase)
{
    Task2_StartGroup24Motion(
        TASK3_MOTOR24_REVERSE_DIR,
        Task3_TurnsToPulses(TASK3_MOTOR2_REVERSE_TURNS),
        Task3_TurnsToPulses(TASK3_MOTOR4_REVERSE_TURNS));
    s_phase = phase;
}

static uint8_t Task3_ClawDelayDone(void)
{
    uint32_t now;

    if (ClawServo_IsMoving() != 0u)
    {
        s_claw_ready = 0u;
        return 0u;
    }

    now = HAL_GetTick();
    if (s_claw_ready == 0u)
    {
        s_claw_ready      = 1u;
        s_claw_settled_ms = now;
        return 0u;
    }

    return (uint8_t)((now - s_claw_settled_ms) >=
                     TASK3_CLAW_SETTLE_MS);
}

static void Task3_BeginSequence(uint8_t subtask)
{
    s_subtask = subtask;
    Task2_StopMotion();
    ClawServo_Hold();
    Task3_BeginPreMotor24Reverse();
}

static void Task3_ResumeGesture(void)
{
    GestureMotor_Stop();
    GestureMotor_RestartTimer();
    Task3_ResetGestureTracking();
    s_phase = TASK3_PHASE_GESTURE;
}

/* 连续10帧抓取确认后锁爪, 但继续允许电机随手势运动。 */
static void Task3_ConfirmGrab(void)
{
    ClawServo_Grab();
    s_finger2_count   = 0u;
    s_finger2_armed   = 0u;
    s_finger3_count   = 0u;
    s_finger3_armed   = 0u;
    s_finger_seq_seen = Gesture_FingerSeq();
    s_nonzero_delay_active = 0u;
    s_nonzero_delay_done   = 0u;
    s_claw_locked          = 1u;
    s_unlock_state_seq_seen = Gesture_StateSeq();
    s_unlock_open_count     = 0u;
}

/* 第一个有效状态只建立基准; 之后从新的松开 -> 抓取边沿开始,
 * 连续收到10帧抓取后允许接收2指或3指指令。 */
static void Task3_PollGrabEdge(void)
{
    uint32_t seq;
    uint8_t closed;

    if (Gesture_IsValid() == 0u)
    {
        s_state_seq_seen    = Gesture_StateSeq();
        s_prev_closed       = 0u;
        s_prev_closed_valid = 0u;
        s_grab_count        = 0u;
        return;
    }

    /* 主循环调用频率远高于串口帧率; 同一个状态帧只能参与一次计数。 */
    seq = Gesture_StateSeq();
    if (seq == s_state_seq_seen)
    {
        return;
    }
    s_state_seq_seen = seq;

    closed = Gesture_IsClosed();

    if (s_prev_closed_valid == 0u)
    {
        s_prev_closed       = closed;
        s_prev_closed_valid = 1u;
        return;
    }

    if ((s_prev_closed == 0u) && (closed != 0u))
    {
        s_grab_count = 1u;
    }
    else if ((closed == 0u) || (s_grab_count == 0u))
    {
        s_grab_count = 0u;
    }
    else if (s_grab_count < TASK3_GRAB_CONFIRM_FRAMES)
    {
        s_grab_count++;
    }

    s_prev_closed = closed;

    if (s_grab_count >= TASK3_GRAB_CONFIRM_FRAMES)
    {
        Task3_ConfirmGrab();
    }
}

/* 锁爪后尚未进入小任务时, 连续十帧松开可解除锁定。 */
static uint8_t Task3_PollUnlockByOpen(void)
{
    uint32_t seq;

    if (Gesture_IsValid() == 0u)
    {
        s_unlock_state_seq_seen = Gesture_StateSeq();
        s_unlock_open_count     = 0u;
        return 0u;
    }

    seq = Gesture_StateSeq();
    if (seq == s_unlock_state_seq_seen)
    {
        return 0u;
    }
    s_unlock_state_seq_seen = seq;

    if (Gesture_IsClosed() != 0u)
    {
        s_unlock_open_count = 0u;
        return 0u;
    }

    if (s_unlock_open_count < 255u)
    {
        s_unlock_open_count++;
    }

    if (s_unlock_open_count >= TASK3_UNLOCK_OPEN_FRAMES)
    {
        /* Reset also clears finger candidates and re-arms a fresh grab edge. */
        Task3_ResetGestureTracking();
        return 1u;
    }

    return 0u;
}

static void Task3_UpdateFingerCandidate(int32_t finger,
                                        int32_t target,
                                        uint8_t *armed,
                                        uint8_t *count)
{
    if (finger != target)
    {
        *armed = 1u;
        *count = 0u;
        return;
    }

    if (*armed == 0u)
    {
        *count = 0u;
        return;
    }

    if (*count < 255u)
    {
        (*count)++;
    }
}

/* 抓取确认后每个新的手指帧都有效, 不再判断当前是抓取还是松开。
 * 首次识别到非0手指时先静默100ms, 并丢弃延迟期间及到期当帧,
 * 延迟结束后只使用后续新帧识别。
 * 2指连续10帧启动小任务1; 3指连续10帧启动小任务2。
 * 两种手指数量都必须先经历一次非目标值跳变。 */
static void Task3_PollFingerFrames(void)
{
    uint32_t seq;
    uint32_t now;
    int32_t finger;

    if ((Gesture_HasData() == 0u) ||
        (Gesture_IsValid() == 0u) ||
        (Gesture_FingerValid() == 0u) ||
        (Gesture_NoHand() != 0u))
    {
        s_finger2_count   = 0u;
        s_finger2_armed   = 0u;
        s_finger3_count   = 0u;
        s_finger3_armed   = 0u;
        s_finger_seq_seen = Gesture_FingerSeq();
        s_nonzero_delay_active = 0u;
        s_nonzero_delay_done   = 0u;
        return;
    }

    seq = Gesture_FingerSeq();

    if (s_nonzero_delay_active != 0u)
    {
        now = HAL_GetTick();
        if ((now - s_nonzero_delay_ms) < TASK3_NONZERO_DELAY_MS)
        {
            return;
        }

        s_nonzero_delay_active = 0u;
        s_nonzero_delay_done   = 1u;
        s_finger_seq_seen      = seq;
        s_finger2_count        = 0u;
        s_finger2_armed        = 0u;
        s_finger3_count        = 0u;
        s_finger3_armed        = 0u;
        return;
    }

    if (seq == s_finger_seq_seen)
    {
        return;
    }
    s_finger_seq_seen = seq;

    finger = Gesture_Finger();
    if (finger == 0)
    {
        s_nonzero_delay_active = 0u;
        s_nonzero_delay_done   = 0u;
        s_finger2_count        = 0u;
        s_finger2_armed        = 0u;
        s_finger3_count        = 0u;
        s_finger3_armed        = 0u;
        return;
    }

    if (s_nonzero_delay_done == 0u)
    {
        s_nonzero_delay_active = 1u;
        s_nonzero_delay_ms     = HAL_GetTick();
        s_finger2_count        = 0u;
        s_finger2_armed        = 0u;
        s_finger3_count        = 0u;
        s_finger3_armed        = 0u;
        return;
    }

    Task3_UpdateFingerCandidate(finger, 2,
                                &s_finger2_armed, &s_finger2_count);
    Task3_UpdateFingerCandidate(finger, 3,
                                &s_finger3_armed, &s_finger3_count);

    if (s_finger2_count >= TASK3_FINGER_CONFIRM_FRAMES)
    {
        Task3_BeginSequence(TASK3_SUBTASK_1);
    }
    else if (s_finger3_count >= TASK3_FINGER_CONFIRM_FRAMES)
    {
        Task3_BeginSequence(TASK3_SUBTASK_2);
    }
}

/* 切换到任务3时调用一次。 */
void Task3_Enter(void)
{
    s_phase            = TASK3_PHASE_GESTURE;
    s_next_phase       = TASK3_PHASE_GESTURE;
    s_wait_ms          = HAL_GetTick();
    s_wait_duration_ms = 0u;
    s_claw_ready       = 0u;
    s_claw_settled_ms  = HAL_GetTick();
    Task3_ResetGestureTracking();
}

/* 任务3运行期间反复调用。 */
void Task3_Step(void)
{
    uint32_t now;

    switch (s_phase)
    {
        case TASK3_PHASE_GESTURE:
            if (s_claw_locked == 0u)
            {
                /* 锁爪前严格复用任务1: 电机和夹爪都随手势控制。 */
                Task1_Step();
                Task3_PollGrabEdge();
            }
            else
            {
                if (Task3_PollUnlockByOpen() != 0u)
                {
                    /* 解除锁爪后立即恢复完整的任务1式手势控制。 */
                    Task1_Step();
                }
                else
                {
                    /* 锁爪后保持抓取, 电机继续跟随手势并识别小任务。 */
                    ClawServo_Grab();
                    GestureMotor_Task();
                    Task3_PollFingerFrames();
                }
            }
            break;

        case TASK3_PHASE_WAIT:
            now = HAL_GetTick();
            if ((now - s_wait_ms) < s_wait_duration_ms)
            {
                break;
            }

            if (s_next_phase == TASK3_PHASE_M24_FORWARD1)
            {
                Task3_BeginMotor24Forward(TASK3_PHASE_M24_FORWARD1);
            }
            else if (s_next_phase == TASK3_PHASE_CLAW_OPEN_WAIT)
            {
                Task3_BeginClawOpen();
            }
            else if (s_next_phase == TASK3_PHASE_M24_REVERSE1)
            {
                Task3_BeginMotor24Reverse(TASK3_PHASE_M24_REVERSE1);
            }
            else if (s_next_phase == TASK3_PHASE_M13_SECOND)
            {
                Task3_BeginMotor13Second();
            }
            else if (s_next_phase == TASK3_PHASE_M24_FORWARD2)
            {
                Task3_BeginMotor24Forward(TASK3_PHASE_M24_FORWARD2);
            }
            else if (s_next_phase == TASK3_PHASE_RESUME_GESTURE)
            {
                Task3_ResumeGesture();
            }
            else if (s_next_phase == TASK3_PHASE_M13_FIRST)
            {
                Task3_BeginMotor13First();
            }
            else
            {
                Task3_BeginMotor13First();
            }
            break;

        case TASK3_PHASE_M24_PRE_REVERSE:
            if (Task2_Group24MotionActive() == 0u)
            {
                Task2_StopMotion();
                Task3_StartWait(TASK3_PHASE_M13_FIRST,
                                TASK3_GROUP_GAP_MS);
            }
            break;

        case TASK3_PHASE_M13_FIRST:
            if (Task2_Group13MotionActive() == 0u)
            {
                Task2_StopMotion();
                Task3_StartWait(TASK3_PHASE_M24_FORWARD1,
                                TASK3_GROUP_GAP_MS);
            }
            break;

        case TASK3_PHASE_M24_FORWARD1:
            if (Task2_Group24MotionActive() == 0u)
            {
                Task2_StopMotion();
                Task3_StartWait(TASK3_PHASE_CLAW_OPEN_WAIT,
                                TASK3_GROUP_GAP_MS);
            }
            break;

        case TASK3_PHASE_CLAW_OPEN_WAIT:
            if (Task3_ClawDelayDone() != 0u)
            {
                Task3_BeginMotor24Reverse(TASK3_PHASE_M24_REVERSE1);
            }
            break;

        case TASK3_PHASE_M24_REVERSE1:
            if (Task2_Group24MotionActive() == 0u)
            {
                Task2_StopMotion();
                Task3_StartWait(TASK3_PHASE_M13_SECOND,
                                TASK3_GROUP_GAP_MS);
            }
            break;

        case TASK3_PHASE_M13_SECOND:
            if (Task2_Group13MotionActive() == 0u)
            {
                Task2_StopMotion();
                Task3_StartWait(TASK3_PHASE_M24_FORWARD2,
                                TASK3_GROUP_GAP_MS);
            }
            break;

        case TASK3_PHASE_M24_FORWARD2:
            if (Task2_Group24MotionActive() == 0u)
            {
                Task2_StopMotion();
                GestureMotor_Stop();
                ClawServo_Hold();
                Task3_StartWait(TASK3_PHASE_RESUME_GESTURE,
                                TASK3_GROUP_GAP_MS);
            }
            break;

        default:
            Task3_ResumeGesture();
            break;
    }
}

/* 离开任务3时调用一次。 */
void Task3_Exit(void)
{
    /* 任务2的硬件定时器不归GestureMotor_Stop()管理, 必须在这里停掉。 */
    Task2_StopMotion();
    GestureMotor_Stop();

    s_phase            = TASK3_PHASE_GESTURE;
    s_next_phase       = TASK3_PHASE_GESTURE;
    s_claw_ready       = 0u;
    Task3_ResetGestureTracking();
}
