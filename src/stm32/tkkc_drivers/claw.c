#include "claw.h"
#include "servo.h"
#include "fpga_gesture.h"

#define CLAW_ANGLE_MAX      90      /* 限幅, 防止撞机械限位 */
#define CLAW_ANGLE_MIN      (-90)

static int16_t s_cmd;               /* 当前的目标角度 */

uint8_t Claw_Init(void)
{
    if (Servo_Init() != 0u)
    {
        return 1u;                  /* PCA9685 没应答(屏幕上会提示查 PA6/PA7) */
    }

    s_cmd = CLAW_ANGLE_OPEN;
    Servo_OffAll();                                 /* 其它 15 路关掉 */
    Servo_SetStartAngle(CLAW_SERVO_CH, CLAW_ANGLE_OPEN);
    return 0u;
}

/* 让爪子转到指定角度(角度没变就不发命令; 转动由 Claw_Update() 一步步走) */
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
    if (angle == s_cmd)
    {
        return;
    }

    s_cmd = angle;
    Servo_SetAngleSlow(CLAW_SERVO_CH, angle, CLAW_STEP_DEG, CLAW_STEP_MS);
}

void Claw_Task(void)
{
    if (Gesture_IsValid() == 0)
    {
        return;                     /* 没数据: 保持不动 */
    }

    claw_goto((Gesture_IsClosed() != 0) ? CLAW_ANGLE_GRAB : CLAW_ANGLE_OPEN);
}

void Claw_Update(void)
{
    Servo_MoveUpdate();
}

int16_t Claw_Angle(void)
{
    return Servo_GetAngle(CLAW_SERVO_CH);
}
