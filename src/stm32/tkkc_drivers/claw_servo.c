/**
  ******************************************************************************
  * @file    claw_servo.c
  * @brief   爪子舵机控制: 手势开合、缓动和角度反馈。
  ******************************************************************************
  */

#include "main.h"
#include "fpga_gesture.h"
#include "servo.h"
#include "claw_servo.h"

/* ==================== 爪子舵机参数 ==================== */

#define SINGLE_SERVO_CH         2       /* 爪子舵机通道 */
#define CLAW_ANGLE_INIT         -60     /* 松开角度 */
#define CLAW_ANGLE_GRAB         40      /* 抓取角度 */
#define CLAW_STEP_DEG           5       /* 每次转动角度 */
#define CLAW_STEP_MS            10      /* 转动间隔(ms) */
#define CLAW_ANGLE_MAX          90      /* 最大角度 */
#define CLAW_ANGLE_MIN          (-90)   /* 最小角度 */

#define CLAW_STATE_OPEN_ANGLE   CLAW_ANGLE_INIT
#define CLAW_STATE_GRAB_ANGLE   CLAW_ANGLE_GRAB

static int16_t  s_claw_cmd;                 /* 爪子目标角度 */
static uint8_t  s_servo_err;                /* 1 = PCA9685 初始化失败 */

/* ==================== 爪子舵机 ==================== */

/* 初始化爪子位置。 */
static void app_servo_init(void)
{
    s_claw_cmd = CLAW_ANGLE_INIT;

    Servo_OffAll();
    Servo_SetStartAngle(SINGLE_SERVO_CH, CLAW_ANGLE_INIT);
}

/* 让爪子转到指定角度。 */
static void claw_goto(int16_t angle)
{
    if (angle > CLAW_ANGLE_MAX)
    {
        angle = CLAW_ANGLE_MAX;
    }
    if (angle < CLAW_ANGLE_MIN)
    {
        angle = CLAW_ANGLE_MIN;
    }

    if (angle == s_claw_cmd)
    {
        return;
    }

    s_claw_cmd = angle;
    Servo_SetAngleSlow(SINGLE_SERVO_CH, angle, CLAW_STEP_DEG, CLAW_STEP_MS);
}

/* 根据手势状态控制爪子。 */
static void app_claw_task(void)
{
    if (Gesture_IsValid() == 0)
    {
        return;
    }

    claw_goto((Gesture_IsClosed() != 0) ? CLAW_STATE_GRAB_ANGLE : CLAW_STATE_OPEN_ANGLE);
}

/* ==================== 模块接口 ==================== */

/* 初始化 PCA9685 和爪子初始角度。 */
void ClawServo_Init(void)
{
    if (Servo_Init() != 0)
    {
        s_servo_err = 1;
    }
    else
    {
        s_servo_err = 0;
        app_servo_init();
    }
}

/* 主循环调用: 根据手势决定爪子的开/合目标。 */
void ClawServo_Task(void)
{
    app_claw_task();
}

/* 停在当前角度, 同时关掉这一路的缓慢转动。
 * 任务切换时调用, 免得旧任务的爪子目标在切换完成后继续走。 */
void ClawServo_Hold(void)
{
    int16_t angle;

    if (s_servo_err != 0u)
    {
        return;
    }

    angle = Servo_GetAngle(SINGLE_SERVO_CH);
    s_claw_cmd = angle;
    Servo_SetAngle(SINGLE_SERVO_CH, angle);
}

/* 任务流程调用: 明确抓取或松开, 后续由 ClawServo_Update() 缓动到位。 */
void ClawServo_Grab(void)
{
    if (s_servo_err == 0u)
    {
        claw_goto(CLAW_STATE_GRAB_ANGLE);
    }
}

void ClawServo_Open(void)
{
    if (s_servo_err == 0u)
    {
        claw_goto(CLAW_STATE_OPEN_ANGLE);
    }
}

uint8_t ClawServo_IsMoving(void)
{
    if (s_servo_err != 0u)
    {
        return 0u;
    }

    return Servo_IsMoving(SINGLE_SERVO_CH);
}

/* 主循环调用: 推进舵机缓慢转动。 */
void ClawServo_Update(void)
{
    Servo_MoveUpdate();
}

/* 读取爪子当前角度。 */
int16_t ClawServo_GetAngle(void)
{
    return Servo_GetAngle(SINGLE_SERVO_CH);
}

/* 查询 PCA9685 初始化是否失败。 */
uint8_t ClawServo_HasError(void)
{
    return s_servo_err;
}
