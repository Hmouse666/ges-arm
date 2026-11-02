#ifndef __AXIS_FOLLOW_H
#define __AXIS_FOLLOW_H

#include "main.h"

/* ==========================================================================
 * 轴跟随引擎 —— 按"手移动的速度"跟一根轴(电机1 跟 X、电机4 跟 Y 都用它)
 *
 * 每个"新坐标"算一次(不是每个主循环都算):
 *   ① 轻滤波: 滤波值 += filter_a × (新坐标 - 滤波值), 目标位置用它算;
 *   ② 测速度: 速度 = (这次滤波值 - 上次滤波值) ÷ 隔了多久   [坐标单位/秒];
 *   ③ 起 / 停: 速度 >= go_speed   连续 go_cnt   次 -> 开始跟着手走;
 *              速度 <= stop_speed 连续 stop_cnt 次 -> 立刻停: 一个脉冲都不再发,
 *              零点对到手现在的位置、欠账(还没走完的那一段)直接丢掉 —— 不管有没有到位。
 *              夹在两个阈值中间(不快不慢)保持原状态, 免得在阈值附近来回跳。
 *
 * 跟的时候: 目标位置 = (滤波后的手 - 零点) × pulses_per_unit,
 *           指令位置按 slew_pps 一点点追, 每个主循环最多往外发一个脉冲(Motor_Step, 不阻塞)。
 *
 * 两个"单位"参数(单位 = 屏幕坐标单位, 换算成角度见 APP.c 的注释):
 *   max_lag_u : 欠账上限 —— 手太快、电机追不上时最多允许落后多少, 超了就把零点往前挪、
 *               这笔账不再记。停机后最多只会补完这一小块; 调太小会把跟手速度也压下来。
 *   err_dead_u: 死区 —— 目标差不到这么多就一个脉冲都不发(挡静止时的微小蠕动), 要比上限小。
 * ========================================================================== */

/* ---- 一根轴的参数(在 APP.c 里填好, 之后只读) ---- */
typedef struct
{
    uint8_t  motor;             /* MOTOR_1 / MOTOR_4 */
    uint8_t  dir_invert;        /* 方向反不反: M1_X_INVERT / M4_Y_INVERT */
    uint16_t pulse_us;          /* 同一个电机两个脉冲之间至少隔多少微秒 */
    uint16_t slew_pps;          /* 指令位置每秒最多走多少个脉冲 */
    uint16_t sample_ms;         /* 多久没有新坐标, 就按"手没动"记一帧 */
    uint16_t idle_ms;           /* 多久没发脉冲算"停下来了"(刷屏门控 / 让路判据) */
    float    pulses_per_unit;   /* 坐标 1 个单位 = 多少个脉冲 */
    float    filter_a;          /* 轻滤波系数 */
    float    go_speed;          /* 手速(单位/秒) >= 它 才算"在快速移动" */
    float    stop_speed;        /* 手速(单位/秒) <= 它 就算"手停了" */
    uint8_t  go_cnt;            /* 连续几个新坐标够快 -> 开始跟 */
    uint8_t  stop_cnt;          /* 连续几个新坐标够慢 -> 停 */
    float    max_lag_u;         /* 欠账上限(单位) */
    float    err_dead_u;        /* 死区(单位) */
} AxisCfg;

/* ---- 每个主循环喂进来的输入(从手势模块取) ---- */
typedef struct
{
    int32_t value;      /* 坐标 */
    uint8_t valid;      /* 1 = 已经解出过坐标 */
    uint8_t fresh;      /* 1 = 最近有解出坐标(超时了就不新鲜) */
    uint8_t lost_long;  /* 1 = 很久没有有效坐标(该重新对零点) */
    uint8_t no_hand;    /* 1 = FPGA 报"没识别到手"(坐标 = 0 / 手指 = 0) */
    uint8_t has_data;   /* 1 = 串口最近还在收数据 */
} AxisIn;

/* ---- 运行时状态(APP.c 里定义, 不用手动改) ---- */
typedef struct
{
    const AxisCfg *cfg;

    uint8_t  ready;             /* 1 = 已经对好零点 */
    uint8_t  move_on;           /* 1 = 正在跟着手走 */
    uint8_t  go_cnt, stop_cnt;

    int32_t  smpl_val;          /* 上一次采样到的坐标 */
    uint32_t smpl_ms;

    float    lpf, lpf_prev;     /* 滤波值 / 上一次的滤波值 */
    float    spd;               /* 最近算出来的手速(屏幕显示用) */

    float    start, cmd, pos;   /* 零点 / 指令位置 / 已经走到的位置(单位都是脉冲) */

    uint32_t last_us;           /* 上一次算限速的时刻(us) */
    uint32_t pulse_us;          /* 上一个脉冲的时刻(us) */
    uint32_t last_move_ms;      /* 上一次发脉冲的时刻(ms) */
} Axis;

void    Axis_Init(Axis *ax, const AxisCfg *cfg);
void    Axis_Step(Axis *ax, const AxisIn *in, uint8_t hold);    /* 每圈调一次; hold != 0 = 这一圈让路(按住不动) */

uint8_t Axis_Moving(const Axis *ax);        /* 1 = 最近还在动(刷屏门控 / 让路判据) */
uint8_t Axis_Following(const Axis *ax);     /* 1 = 正在跟(G), 0 = 停住(S) */
int32_t Axis_Speed(const Axis *ax);         /* 手速(单位/秒) */

/* ---- 一次只动一根轴(为了拾取精度) ---- */
#define AXIS_OWNER_NONE     0u  /* 两根都可以走 */
#define AXIS_OWNER_A        1u  /* a(第一个参数那根)在连续动, 另一根让路 */
#define AXIS_OWNER_B        2u  /* b 在连续动 */

/* 谁最近发过脉冲谁先走; 完全打平时让 a_first 那根先走 */
uint8_t Axis_Arbitrate(const Axis *a, const Axis *b, uint16_t active_ms, uint8_t a_first);

#endif /* __AXIS_FOLLOW_H */
