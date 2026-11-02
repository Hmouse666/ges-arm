#ifndef __FPGA_GESTURE_H
#define __FPGA_GESTURE_H

#include "main.h"

/* ==========================================================================
 * FPGA 手势数据模块 —— 收 FPGA 发来的一行数据, 拆成 X / Y / 状态 / 深度
 *
 * ★ 函数体(实现)放在 Core\Src\usart.c 的最后(USER CODE BEGIN 1 / END 1 之间):
 *   这样 Keil 工程里不用新增文件 —— 工程里没登记的 .c, Keil 是不会编译的(会报
 *   Undefined symbol), 直接并进本来就在工程里的 usart.c 最省事, 什么都不用设置。
 *   这个头文件只负责"声明接口"。旧的那份独立文件存档在
 *   TKKC_Drivers\unused_sources_merged\fpga_gesture.c(只是留个备份, 工程编译不用它)。
 *
 *   串口: USART3, PB11 = USART3_RX(★ 接 FPGA 的 TX), PB10 = USART3_TX(留着不用),
 *         115200 8N1; 串口的初始化在 Core\Src\usart.c 的 MX_USART3_UART_Init() 里,
 *         main.c 已经调用它了。
 *
 *   FPGA 发来的一帧就是你给的那种:
 *      【手势识别】 模式: [M1:1, M2:1] | 状态: 松开 | 手指: 0 | 坐标: (X: 612, Y: 362) | 深度: 53
 *  模块干这些活:
 *    1) 收字节(接收中断) -> 攒成一整帧(认换行, 也认帧头"【");
 *    2) 从一帧里认"状态":
 *           抓取 / 夹取 / 闭合 / 握拳 / 捏 / 夹 / 抓 ...        -> GEST_CLOSE(爪子夹紧)
 *           松开 / 张开 / 展开 / 摊开 / 放手 / 松 / 张 / 开 ... -> GEST_OPEN (爪子张开)
 *       ★ 中文关键字同时按 UTF-8 和 GBK 两种编码来认, FPGA 那边用哪种都能对上;
 *         除了整词, 连"单字"(闭/关/合/开/松/张...)也认 —— 换个写法也能认出来;
 *         英文的 GRAB / CLOSE / FIST / OPEN / RELEASE 也认;
 *       ★ 另外也认 FPGA 的紧凑格式 "#G,mode1,mode2,is_fist,手指,X,Y,深度"。
 *    3) 解出 X / Y / 深度 三个数, 屏幕上显示这 4 行(8x16 字体, 一行 16 像素高):
 *           X     : 手的 X 坐标
 *           Y     : 手的 Y 坐标
 *           STATE : GRAB(抓取) / OPEN(松开) / ??(收到帧但没认出状态) / --(一帧都没收到)
 *           DEP   : 深度
 *       (屏幕自带的是 ASCII 字体, 画不了汉字, 所以状态用 GRAB / OPEN 表示, 意思一样)
 *       ANGLE(舵机当前角度)那一行是 APP.c 画的, 位置也用下面这几个宏。
 *       再下面还有两行调试信息(收到的帧数 / 编码 / 状态字段的十六进制), 用
 *       GEST_SHOW_DEBUG 开关: 填 1 打开。★ 屏幕上一直是 "--" / "??" 的时候,
 *       把它改成 1, 屏幕上会显示收到的帧数和"状态:"后面那几个字节的十六进制 ——
 *       把那串十六进制发出来就能判断 FPGA 到底发的什么字、什么编码。
 *    4) 超过 GEST_TIMEOUT_MS 收不到有效状态 -> 认为"没数据", 爪子保持不动。
 *
 *   上层(APP.c)只要用这几个函数:
 *       Gesture_Init();      初始化完 USART3 之后调用一次
 *       Gesture_Poll();      主循环里反复调用(取帧 + 解析 + 超时)
 *       Gesture_Show();      主循环里反复调用(刷 X/Y/STATE/DEP, 内部自己限速)
 *       Gesture_IsValid();   1 = 现在有确认过的状态
 *       Gesture_IsClosed();  1 = 抓取(闭合), 0 = 松开
 * ========================================================================== */

/* 屏幕上"调试信息"那两行的开关:
 *   ★ 现在为 0: 104/112 附近的行让给 TASK1/TASK2/TASK3 任务显示。
 *   1 = 显示: 收到的帧数 + 编码(UTF8/GBK/ASCII) + 状态字段那几个字节的十六进制
 *       —— 第一次联调 FPGA 的时候非常有用(一眼看出到底有没有收到、收到的是什么字);
 *   0 = 不显示, 屏幕上就只剩 X / Y / STATE / DEP / ANGLE 和任务行。
 * ★ 改成 0 之后编译会有 3 个"变量定义了没用到"的无害警告(s_enc / s_val_ok / 十六进制那行),
 *   不影响运行, 不用管它们。 */
#define GEST_SHOW_DEBUG     0

/* 状态值 */
#define GEST_OPEN           0       /* 松开 / 张开 */
#define GEST_CLOSE          1       /* 抓取 / 闭合 */

/* FPGA 手动模式驱动位(文本帧里的 drive[0] ~ drive[9])。 */
#define GEST_DRIVE_FIELD_COUNT  10u
#define GEST_DRIVE_FORWARD  0x01u   /* drive[2] = 1: 任务4 电机3 正转 */
#define GEST_DRIVE_REVERSE  0x02u   /* drive[3] = 1: 任务4 电机3 反转 */

/* 多久收不到有效的状态, 就认为"没数据"了(毫秒), 此时爪子保持不动 */
#define GEST_TIMEOUT_MS     800

/* ★ 多久收不到"带 X 的坐标", 就认为识别不到坐标了(毫秒) —— 电机立即停, 不再往前跟。
 *   这个阈值不写死: 程序自己量"相邻两次 X 之间隔多久", 用它的 3 倍来判, 夹在下面两个数之间 ——
 *   这样 FPGA 帧来得快时停得干脆, 帧来得慢时也不会把正常的帧间隔误判成"没坐标"。
 *   (以前写死 150ms: 帧间隔一旦超过它, 电机就会一帧对一次零点、永远不动。) */
#define GEST_X_TIMEOUT_MIN_MS   150     /* 下限: 再灵敏也不能比它短 */
#define GEST_X_TIMEOUT_MAX_MS   600     /* 上限: 帧再慢也不能等超过它 */

/* ★ 多久没拿到有效坐标, 就认为"手真的走了" —— 之后第一个像样的坐标当新基准(重新对零点),
 *   不再往旧位置追。要比 GEST_X_TIMEOUT_MAX_MS 大, 免得正常停顿就重新对零点。 */
#define GEST_X_RESYNC_MS        700

/* ★★ 上面这几个 X 轴的判据, Y 轴也各有一套(电机4 跟的是 Y, 它也要"手一停就立刻停") ★★
 *   数值和 X 轴那几个一样(坐标本来就是同一帧里一起来的), 分开写只是为了以后能各自单独调。 */
#define GEST_Y_TIMEOUT_MIN_MS   150     /* 同 GEST_X_TIMEOUT_MIN_MS, 给 Y 用 */
#define GEST_Y_TIMEOUT_MAX_MS   600     /* 同 GEST_X_TIMEOUT_MAX_MS, 给 Y 用 */
#define GEST_Y_RESYNC_MS        700     /* 同 GEST_X_RESYNC_MS, 给 Y 用 */

/* ★★ X = 0 就是"没识别到手" ★★
 *   1 = 打开(默认): FPGA 传来的 X 一变成 0, 就认为手没了 —— 立刻停, 并丢掉之前
 *       所有坐标(重新对零点), 不会再往旧的 X 追。
 *   0 = 关掉(把 X=0 当成一个正常坐标)。
 *   ★ 如果你的 FPGA"手在画面最左边"也会输出 0, 那就要把这个改成 0 并换别的判据。 */
#define GEST_X_ZERO_IS_NOHAND   1

/* ★★ Y = 0 也是"没识别到手" ★★
 *   和 X 那条一个道理, 而且它以前是"电机4 手一停就剧烈抖"的元凶:
 *   FPGA 丢手 / 手不动的那些帧, Y 常常会变成 0 —— 以前程序把 0 当成正常坐标存了下来,
 *   于是电机4 一会儿拿"坐标"当目标、一会儿拿"0"当目标, 来回猛跑(看上去就是抖)。
 *   打开这一条之后和 X 一样: 立刻停 + 丢掉旧坐标, 等 Y 回来重新对零点。
 *   ★ 如果你的 FPGA"手在画面最下面"也会输出 Y = 0, 那就要把这个改成 0 并换别的判据。 */
#define GEST_Y_ZERO_IS_NOHAND   1

/* ★★ 手指 = 0 就是"没识别到手" ★★
 *   1 = 打开(默认): FPGA 报的"手指"一变成 0, 就认为手没了 —— 立即停, 并丢掉之前
 *       所有坐标(重新对零点), 不会再往旧的 X 追。
 *   0 = 关掉(把 手指=0 当成正常数据)。
 *   ★ 这一条和上面 X=0 那条是"谁先到听谁的"(或的关系)。
 *   ★ 屏幕调试行上会显示 F:<手指数> —— 手拿着的时候看一眼: 如果显示的是 0, 说明这份
 *     数据里"手指=0"并不代表手没了, 那就把这里改成 0。 */
#define GEST_FINGER0_IS_NOHAND  1

/* ★ 状态防抖(抓取/松开): 防止 FPGA 识别抖动时爪子在开/合之间来回抽。
 *   「同一个状态连续出现 GEST_CONFIRM_FRAMES 帧」或者「同一个状态已经持续 GEST_CONFIRM_MS 毫秒」
 *   才算数, 只抖一帧不认。
 *   ★ 这里只管"爪子开合"这一个状态, 不影响电机1 跟 X 轴的速度 ——
 *     X 轴的消抖在 APP.c 里(X_FILT_N / X_DEADBAND / X_CONFIRM_FRAMES)。
 *   填 1 和 0 = 完全不防抖(收到就算, 最灵敏但会抽)。 */
#define GEST_CONFIRM_FRAMES 2       /* 同一个状态要连续多少帧才认 */
#define GEST_CONFIRM_MS     80      /* 或者: 同一个状态持续多少毫秒才认 */

/* 屏幕刷新间隔(毫秒), 100 = 10Hz */
#define GEST_SHOW_MS        100

/* 屏幕统一使用 8x16 字体, 每行占 16 像素(屏幕 128x160 刚好 10 行) */
#define GEST_X0             0       /* 左边距 */
#define GEST_Y_X            0       /* X 轴坐标 */
#define GEST_Y_Y            16      /* Y 轴坐标 */
#define GEST_Y_STATE        32      /* 状态 STATE: GRAB / OPEN */
#define GEST_Y_DEP          48      /* 深度 */
#define GEST_Y_ANGLE        64      /* 舵机当前角度: 这一行是 APP.c 画的 */
#define GEST_Y_RX           160     /* 调试行: 当前 GEST_SHOW_DEBUG=0, 不显示 */
#define GEST_Y_HEX          160     /* 调试行: 当前 GEST_SHOW_DEBUG=0, 不显示 */

/* ==================== 接口 ==================== */

/* 初始化: 打开 USART3 的接收中断(要在 MX_USART3_UART_Init() 之后调用) */
void Gesture_Init(void);

/* 主循环里反复调用: 取整帧 + 解析 + 判断超时 */
void Gesture_Poll(void);

/* 主循环里反复调用: 刷新屏幕上的 X / Y / STATE / DEP(内部自己限速) */
void Gesture_Show(void);

uint8_t  Gesture_State(void);       /* 最后一次确认的状态(GEST_OPEN / GEST_CLOSE) */
uint8_t  Gesture_IsClosed(void);    /* 1 = 抓取(闭合) / 0 = 松开 */
uint8_t  Gesture_IsValid(void);     /* 1 = 现在有确认过的状态 */
uint32_t Gesture_StateSeq(void);    /* 每收到一帧可识别状态 +1(任务切换等新手势用) */
void     Gesture_ResetValid(void);  /* 清掉旧任务的状态和坐标, 等新数据再开放控制 */
uint8_t  Gesture_HasData(void);     /* 1 = 串口最近还有数据(给 LED / 调试用) */
uint32_t Gesture_Frames(void);      /* 一共收到多少帧(调试用) */
uint8_t  Gesture_HasDriveField(void); /* 1 = 已收到过 drive[0] ~ drive[9] 字段 */
uint8_t  Gesture_DriveFlags(void);  /* 最近解析到的电机3两路电平 */
uint32_t Gesture_DriveOneCount(uint8_t index); /* 某个 drive[index]=1 的累计次数 */
uint8_t  Gesture_DriveValue(uint8_t index);      /* drive[index] 最新电平 */
uint8_t  Gesture_DriveValueValid(uint8_t index); /* 1 = 收到过该驱动位 */
uint32_t Gesture_DriveChangeSeq(uint8_t index);  /* 该驱动位每次变化 +1 */
uint32_t Gesture_DriveForwardCount(void); /* 累计收到 drive[2]=1 的次数 */
uint32_t Gesture_DriveReverseCount(void); /* 累计收到 drive[3]=1 的次数 */
uint8_t  Gesture_FingerValid(void);       /* 1 = 当前格式确实带"手指"字段 */
int32_t  Gesture_Finger(void);            /* 最新识别到的手指个数 */
uint32_t Gesture_FingerSeq(void);         /* 每解析到一帧手指字段 +1 */
int32_t  Gesture_SliderValue(void);       /* 调速模式最近一次 value */
uint8_t  Gesture_SliderValueValid(void);  /* 1 = 已收到 value */
uint32_t Gesture_SliderValueSeq(void);    /* value 每更新一次 +1 */
int32_t  Gesture_X(void);           /* 手的 X 坐标 */
uint8_t  Gesture_XValid(void);      /* 1 = 已经解出过 X 坐标(上电/刚重连时用它卡住电机) */
uint8_t  Gesture_XFresh(void);      /* 1 = 最近 GEST_X_TIMEOUT_MS 内解出过 X(★ 电机靠它决定该不该停) */
uint8_t  Gesture_XLostLong(void);   /* 1 = 已经很久(GEST_X_RESYNC_MS)没有有效坐标了, 该重新对零点 */
uint8_t  Gesture_NoHand(void);      /* 1 = 最近一帧的 X 是 0 -> FPGA 没识别到手(★ 电机立即停) */
int32_t  Gesture_Y(void);           /* 手的 Y 坐标 */
uint8_t  Gesture_YValid(void);      /* 1 = 已经解出过 Y 坐标(和 Gesture_XValid 一个意思) */
uint8_t  Gesture_YFresh(void);      /* 1 = 最近 GEST_Y_TIMEOUT_* 内解出过 Y(★ 电机4 靠它决定该不该停) */
uint8_t  Gesture_YLostLong(void);   /* 1 = 已经很久(GEST_Y_RESYNC_MS)没有有效 Y 了, 该重新对零点 */
uint8_t  Gesture_YNoHand(void);     /* 1 = 最近一帧的 Y 是 0 -> 没识别到手(★ 电机4 立即停) */
int32_t  Gesture_Depth(void);       /* 深度 */

#endif /* __FPGA_GESTURE_H */
