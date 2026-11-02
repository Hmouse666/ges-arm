#include "axis_follow.h"
#include "motor.h"
#include "ustimer.h"

/* 立刻停: 一个脉冲都不发, 指令位置拉回电机当前位置(欠账清掉)。零点保住,
 * 下次手再快起来就从现在这里接着走, 不会突然补一大段。 */
static void axis_hold(Axis *ax)
{
    ax->cmd      = ax->pos;
    ax->move_on  = 0u;
    ax->go_cnt   = 0u;
    ax->stop_cnt = 0u;
    ax->last_us  = Us_Now();            /* 免得下次起步算出一个巨大的 dt */
}

/* 连零点一起丢掉(手离开 / 很久没有坐标): 下次收到的第一个坐标当新零点 */
static void axis_forget(Axis *ax)
{
    axis_hold(ax);
    ax->ready = 0u;
}

void Axis_Init(Axis *ax, const AxisCfg *cfg)
{
    ax->cfg          = cfg;
    ax->ready        = 0u;
    ax->move_on      = 0u;
    ax->go_cnt       = 0u;
    ax->stop_cnt     = 0u;
    ax->smpl_val     = 0;
    ax->smpl_ms      = 0u;
    ax->lpf          = 0.0f;
    ax->lpf_prev     = 0.0f;
    ax->spd          = 0.0f;
    ax->start        = 0.0f;
    ax->cmd          = 0.0f;
    ax->pos          = 0.0f;
    ax->last_us      = Us_Now();
    ax->pulse_us     = Us_Now();
    ax->last_move_ms = HAL_GetTick() - (uint32_t)cfg->idle_ms - 1u;   /* 上电算"已经停了" */
}

void Axis_Step(Axis *ax, const AxisIn *in, uint8_t hold)
{
    const AxisCfg *cfg = ax->cfg;
    uint32_t now, us, dt_us;
    int32_t  v;
    float    spd, tgt, diff, step, need, lag, dead;
    uint8_t  dir = 0u;

    now = HAL_GetTick();
    us  = Us_Now();

    /* ---- 手没了 / 很久没坐标 / 串口没数据 / 一个坐标都没解出来: 立刻停 + 丢掉旧零点 ---- */
    if ((in->no_hand != 0u) || (in->lost_long != 0u) ||
        (in->has_data == 0u) || (in->valid == 0u))
    {
        axis_forget(ax);
        return;
    }

    /* ---- 暂时没有新坐标(超过自适应阈值没解出坐标): 立刻停, 零点保住 ---- */
    if (in->fresh == 0u)
    {
        axis_hold(ax);
        return;
    }

    v = in->value;

    /* ---- 上电后收到的第一个坐标就是零点, 这一帧不动 ---- */
    if (ax->ready == 0u)
    {
        ax->start    = (float)v;
        ax->lpf      = (float)v;
        ax->lpf_prev = (float)v;
        ax->spd      = 0.0f;
        ax->smpl_val = v;
        ax->smpl_ms  = now;
        ax->cmd      = 0.0f;
        ax->pos      = 0.0f;
        ax->move_on  = 0u;                  /* 先按"停"起手: 手真的动快了才开始跟 */
        ax->go_cnt   = 0u;
        ax->stop_cnt = 0u;
        ax->last_us  = us;
        ax->pulse_us = us;
        ax->ready    = 1u;
        return;
    }

    /* ---- ★ 每个"新坐标"算一次: 轻滤波 + 测速度 + 起/停判定 ----
     * "新坐标" = 坐标和上次不一样, 或者过了 sample_ms 还是同一个值
     * (FPGA 在手不动时会一直发同一个值, 那也算"手没动"的一帧)。 */
    if ((v != ax->smpl_val) || ((uint32_t)(now - ax->smpl_ms) >= (uint32_t)cfg->sample_ms))
    {
        float dt_ms = (float)(now - ax->smpl_ms);

        if (dt_ms < 1.0f)
        {
            dt_ms = 1.0f;
        }

        ax->lpf += cfg->filter_a * ((float)v - ax->lpf);

        spd          = (ax->lpf - ax->lpf_prev) * 1000.0f / dt_ms;
        ax->lpf_prev = ax->lpf;
        ax->spd      = spd;
        ax->smpl_val = v;
        ax->smpl_ms  = now;

        if (spd >= cfg->go_speed)
        {
            if (ax->go_cnt < 250u)
            {
                ax->go_cnt++;
            }
            ax->stop_cnt = 0u;

            if (ax->go_cnt >= cfg->go_cnt)
            {
                ax->move_on = 1u;
            }
        }
        else if (spd <= cfg->stop_speed)
        {
            if (ax->stop_cnt < 250u)
            {
                ax->stop_cnt++;
            }
            ax->go_cnt = 0u;

            if (ax->stop_cnt >= cfg->stop_cnt)
            {
                ax->move_on = 0u;
            }
        }
        /* 夹在两个阈值中间: 保持原状态不变 */
    }

    /* ---- ★ 手慢了/停了: 立刻停, 欠账丢掉, 不管有没有走到手的位置 ---- */
    if (ax->move_on == 0u)
    {
        ax->start   = ax->lpf;              /* 零点 = 手现在的位置 */
        ax->cmd     = ax->pos;              /* 指令位置 = 电机当前位置: 欠账清零 */
        ax->last_us = us;
        return;
    }

    /* ---- ★ 让路(另一根轴在连续动): 这一圈不动, 零点跟手走, 松开时接着走 ---- */
    if (hold != 0u)
    {
        ax->start   = ax->lpf;
        ax->cmd     = ax->pos;
        ax->last_us = us;
        return;
    }

    /* ---- 这一圈过了多久(算限速用) ---- */
    dt_us = us - ax->last_us;
    if (dt_us < 1u)
    {
        dt_us = 1u;
    }
    if (dt_us > 200000u)
    {
        dt_us = 200000u;                    /* 只按 200ms 算, 免得卡顿时一次走太大步 */
    }
    ax->last_us = us;

    /* ---- 目标位置 = (滤波后的手 - 零点) × 每单位脉冲数 ---- */
    tgt  = (ax->lpf - ax->start) * cfg->pulses_per_unit;
    diff = tgt - ax->cmd;

    /* ---- 欠账上限: 手太快、电机追不上的那一截就地放弃(把零点往前挪, 这笔账不再记) ---- */
    lag = cfg->max_lag_u * cfg->pulses_per_unit;
    if (diff > lag)
    {
        ax->start += (diff - lag) / cfg->pulses_per_unit;
        diff       = lag;
    }
    else if (diff < -lag)
    {
        ax->start += (diff + lag) / cfg->pulses_per_unit;
        diff       = -lag;
    }

    /* ---- 死区: 目标差得不多就一个脉冲都不发(挡掉静止时的微小蠕动) ---- */
    dead = cfg->err_dead_u * cfg->pulses_per_unit;
    if ((diff > -dead) && (diff < dead))
    {
        return;
    }

    /* ---- 限速: 指令位置按 slew_pps 一点点朝目标走 ---- */
    step = (float)cfg->slew_pps * (float)dt_us / 1000000.0f;
    if (diff > step)
    {
        ax->cmd += step;
    }
    else if (diff < -step)
    {
        ax->cmd -= step;
    }
    else
    {
        ax->cmd += diff;
    }

    /* ---- 输出: 这一圈最多往外发一个脉冲 ---- */
    need = ax->cmd - ax->pos;
    if (need >= 1.0f)
    {
        dir = 0u;                           /* 正转 */
    }
    else if (need <= -1.0f)
    {
        dir = 1u;                           /* 反转 */
    }
    else
    {
        return;                             /* 还不到一个脉冲(或已经到位): 这一圈不动 */
    }

    /* 同一个电机的两个脉冲之间要隔够 pulse_us, 挤太近这一圈就先不发 */
    if ((uint32_t)(us - ax->pulse_us) < (uint32_t)cfg->pulse_us)
    {
        return;
    }

    ax->pulse_us     = us;
    ax->pos         += (dir != 0u) ? -1.0f : 1.0f;
    ax->last_move_ms = now;

    Motor_Step(cfg->motor, (uint8_t)(dir ^ cfg->dir_invert));
}

uint8_t Axis_Moving(const Axis *ax)
{
    return ((HAL_GetTick() - ax->last_move_ms) < (uint32_t)ax->cfg->idle_ms) ? 1u : 0u;
}

uint8_t Axis_Following(const Axis *ax)
{
    return (ax->move_on != 0u) ? 1u : 0u;
}

int32_t Axis_Speed(const Axis *ax)
{
    return (int32_t)ax->spd;
}

uint8_t Axis_Arbitrate(const Axis *a, const Axis *b, uint16_t active_ms, uint8_t a_first)
{
    uint32_t ea = HAL_GetTick() - a->last_move_ms;      /* a 上次发脉冲距今多久(ms) */
    uint32_t eb = HAL_GetTick() - b->last_move_ms;
    uint8_t  ab = (ea < (uint32_t)active_ms) ? 1u : 0u; /* 1 = a 还在连续动 */
    uint8_t  bb = (eb < (uint32_t)active_ms) ? 1u : 0u;

    if ((ab != 0u) && (bb != 0u))
    {
        /* 交接的那一下(两根都还在动): 让"最近发过脉冲"的那根接着走 */
        if (a_first != 0u)
        {
            return (ea <= eb) ? AXIS_OWNER_A : AXIS_OWNER_B;
        }
        return (eb <= ea) ? AXIS_OWNER_B : AXIS_OWNER_A;
    }
    if (ab != 0u)
    {
        return AXIS_OWNER_A;
    }
    if (bb != 0u)
    {
        return AXIS_OWNER_B;
    }
    return AXIS_OWNER_NONE;                             /* 两根都停着: 谁想动谁动 */
}
