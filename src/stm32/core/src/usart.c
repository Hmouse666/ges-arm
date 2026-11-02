/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

UART_HandleTypeDef huart1;

/* ★ FPGA 手势数据从这个串口进来(见文件最后那一段): PB10 = USART3_TX, PB11 = USART3_RX */
UART_HandleTypeDef huart3;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/* USART3 init function */

/**
 * @brief  FPGA 串口(USART3)初始化: PB10 = TX, PB11 = RX, 115200 8N1
 * @note   FPGA(Zynq) 那边把手势数据一行一行发过来, 我们只用 RX(PB11) 收;
 *         TX(PB10) 留着不用(想给 FPGA 回话的时候可以用)。
 *        ★ 必须放在 MX_GPIO_Init() 之后调用(main.c 里已经调了)
  */
void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspInit 0 */

  /* USER CODE END USART3_MspInit 0 */
    /* USART3 clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX   (FPGA 的 RX, 这里不用)
    PB11     ------> USART3_RX   (★ 接 FPGA 的 TX)
    */
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USART3 interrupt Init */
    HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspInit 1 */

  /* USER CODE END USART3_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

    /* USART1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspDeInit 0 */

  /* USER CODE END USART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_11);

    /* USART3 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* ==========================================================================
 *              ★ FPGA 手势数据: 收字节 -> 攒帧 -> 解析 -> 屏幕显示
 *
 *  这里就是 TKKC_Drivers\fpga_gesture.h 里那几个函数的"实现"(声明在那个头文件里)。
 *  写在 usart.c 而不是新建一个 .c, 是因为 Keil 只编译"工程里登记过的"文件:
 *  新加的 .c 不登记进去就会报 Undefined symbol, 并进本来就在工程里的 usart.c 最省事,
 *  你在 Keil 里一个字都不用设置。
 *
 *  串口: USART3 —— PB11 = USART3_RX(★ 接 FPGA 的 TX), PB10 = USART3_TX(留着不用)
 *        115200, 8 位数据, 无校验, 1 位停止(8N1); 用接收中断一个字节一个字节收。
 *        ★ USART3 的初始化在下面的 MX_USART3_UART_Init() 里, main.c 已经调用它了;
 *          中断向量 USART3_IRQHandler 在 stm32f1xx_it.c 里。
 *
 *  收到的一帧长这样(FPGA 那边整句打印的格式):
 *    【手势识别】 模式: [M1:1, M2:1] | 状态: 松开 | 手指: 0 | 坐标: (X: 612, Y: 362) | 深度: 53
 *  程序从里面解出: 状态(松开 / 抓取) + X + Y + 深度。
 *  另外也认 FPGA 的紧凑格式: "#G,mode1,mode2,is_fist,手指,X,Y,深度"(见 Gest_ParseGLine)。
 *
 *  屏幕上显示这 4 行(位置在 fpga_gesture.h 里改):
 *      X    : 手的 X 坐标        Y    : 手的 Y 坐标
 *      STATE: GRAB(抓取) / OPEN(松开) / ??(收到帧但没认出状态) / --(一帧都没收到)
 *      DEP  : 深度
 *  下面还有两行调试信息(RX 帧数 / 编码 / 状态字段的十六进制), 用 fpga_gesture.h 里的
 *  GEST_SHOW_DEBUG 开关: 填 1 打开(串口调不通时非常有用), 调好之后填 0 就只剩上面 4 行。
 * ========================================================================== */

#include "fpga_gesture.h"
#include "display.h"
#include "task_mode.h"
#include <string.h>

#define GEST_LINE_MAX       200     /* 一帧最多收多少字节(FPGA 一帧大约 90 字节) */
#define GEST_GAP_MS         20      /* 这么久收不到新字节, 就把手上这半行当成一整帧 */
#define GEST_HEX_BYTES      7       /* 屏幕上那行十六进制显示几个字节 */
#define GEST_TASK_LINE_MAX  GEST_LINE_MAX /* 任务帧单独留一个缓冲, 避免被手势帧覆盖 */

/* 认出来的关键字是哪种编码(屏幕上 EN: 那一行会显示出来) */
#define ENC_NONE            0
#define ENC_UTF8            1
#define ENC_GBK             2
#define ENC_ASCII           3

/* --------------------------------------------------------------------------
 * 关键字表 —— "抓取(夹紧)" 和 "松开(张开)" 各一大串
 *
 * 中文全部用十六进制写: 不管源文件是什么编码、Keil 怎么解释中文, 对上的都是同样的字节。
 * 每种说法都给了 UTF-8 和 GBK 两套(FPGA 那边发哪种都认); 除了整词(抓取/闭合/握拳),
 * 还准备了"单字"(抓/夹/闭/关/握/合...), 这样 FPGA 换个写法也能认出来。
 * 另外也认英文: GRAB / CLOSE / FIST 当抓取, OPEN / LOOSE / RELEASE 当松开。
 * ★ 判断顺序是"先抓取, 再松开" —— 免得"松开"里的"开"字把人带偏。
 * -------------------------------------------------------------------------- */

/* 抓取(闭合): UTF-8 */
static const char *const KW_CLOSE_UTF8[] = {
    "\xE6\x8A\x93\xE5\x8F\x96",         /* 抓取 */
    "\xE9\x97\xAD\xE5\x90\x88",         /* 闭合 */
    "\xE6\x8F\xA1\xE6\x8B\xB3",         /* 握拳 */
    "\xE5\xA4\xB9\xE5\x8F\x96",         /* 夹取 */
    "\xE5\x85\xB3\xE9\x97\xAD",         /* 关闭 */
    "\xE5\x90\x88\xE4\xB8\x8A",         /* 合上 */
    "\xE6\x94\xA5",                     /* 攥 */
    "\xE6\x8D\x8F",                     /* 捏 */
    "\xE6\x8B\xB3",                     /* 拳 */
    "\xE6\x8A\x93",                     /* 抓 */
    "\xE5\xA4\xB9",                     /* 夹 */
    "\xE9\x97\xAD",                     /* 闭 */
    "\xE5\x85\xB3",                     /* 关 */
    "\xE6\x8F\xA1",                     /* 握 */
    "\xE5\x90\x88",                     /* 合 */
};

/* 抓取(闭合): GBK */
static const char *const KW_CLOSE_GBK[] = {
    "\xD7\xA5\xC8\xA1",                 /* 抓取 */
    "\xB1\xD5\xBA\xCF",                 /* 闭合 */
    "\xCE\xD5\xC8\xAD",                 /* 握拳 */
    "\xBC\xD0\xC8\xA1",                 /* 夹取 */
    "\xB9\xD8\xB1\xD5",                 /* 关闭 */
    "\xBA\xCF\xC9\xCF",                 /* 合上 */
    "\xC4\xF3",                         /* 捏 */
    "\xC8\xAD",                         /* 拳 */
    "\xD7\xA5",                         /* 抓 */
    "\xBC\xD0",                         /* 夹 */
    "\xB1\xD5",                         /* 闭 */
    "\xB9\xD8",                         /* 关 */
    "\xCE\xD5",                         /* 握 */
    "\xBA\xCF",                         /* 合 */
};

/* 松开(张开): UTF-8 */
static const char *const KW_OPEN_UTF8[] = {
    "\xE6\x9D\xBE\xE5\xBC\x80",         /* 松开 */
    "\xE5\xBC\xA0\xE5\xBC\x80",         /* 张开 */
    "\xE5\xB1\x95\xE5\xBC\x80",         /* 展开 */
    "\xE6\x91\x8A\xE5\xBC\x80",         /* 摊开 */
    "\xE6\x94\xBE\xE6\x89\x8B",         /* 放手 */
    "\xE6\x9D\xBE",                     /* 松 */
    "\xE5\xBC\xA0",                     /* 张 */
    "\xE5\xBC\x80",                     /* 开 */
};

/* 松开(张开): GBK */
static const char *const KW_OPEN_GBK[] = {
    "\xCB\xC9\xBF\xAA",                 /* 松开 */
    "\xD5\xC5\xBF\xAA",                 /* 张开 */
    "\xD5\xB9\xBF\xAA",                 /* 展开 */
    "\xCC\xAF\xBF\xAA",                 /* 摊开 */
    "\xB7\xC5\xCA\xD6",                 /* 放手 */
    "\xCB\xC9",                         /* 松 */
    "\xD5\xC5",                         /* 张 */
    "\xBF\xAA",                         /* 开 */
};

/* 英文写法(FPGA 那边要是打印英文, 这几个词也认) */
static const char *const KW_CLOSE_EN[] = {"GRAB", "CLOSE", "FIST", "GRIP", "SHUT"};
static const char *const KW_OPEN_EN[]  = {"OPEN", "LOOSE", "RELEASE", "FREE"};

/* 坐标和深度的字段名(中文同样 UTF-8 / GBK 两套, 英文的 DEP/DEPTH 也认) */
#define KW_DEPTH_UTF8       "\xE6\xB7\xB1\xE5\xBA\xA6"      /* 深度 (UTF-8) */
#define KW_DEPTH_GBK        "\xC9\xEE\xB6\xC8"              /* 深度 (GBK)   */

/* "手指"个数的字段名(中文两套编码, 英文 FINGER/FINGERS 也认) */
#define KW_FINGER_UTF8      "\xE6\x89\x8B\xE6\x8C\x87"      /* 手指 (UTF-8) */
#define KW_FINGER_GBK       "\xCA\xD6\xD6\xB8"              /* 手指 (GBK)   */
#define KW_COLON_FULL_UTF8  "\xEF\xBC\x9A"                  /* ： (UTF-8)   */
#define KW_COLON_FULL_GBK   "\xA3\xBA"                      /* ： (GBK)     */

#define LIST_N(x)   ((uint8_t)(sizeof(x) / sizeof((x)[0])))

/* --------------------------------------------------------------------------
 * 变量
 * -------------------------------------------------------------------------- */

static uint8_t           s_rx_byte;                     /* HAL 单字节接收的落脚点 */
static char              s_line[GEST_LINE_MAX + 1];     /* 正在收的这一帧 */
static char              s_done[GEST_LINE_MAX + 1];     /* ★ 收好的"最新"一整帧(主循环没取走也会被下一帧覆盖) */
static uint16_t          s_line_len;                    /* 这一帧收了多少字节 */
static uint16_t          s_done_len;                    /* s_done 里这一帧有多少字节 */
static volatile uint8_t  s_line_ready;                  /* 1 = s_done 里有一整帧了 */
static char              s_task_done[GEST_TASK_LINE_MAX + 1]; /* 最新任务帧 */
static uint16_t          s_task_done_len;               /* s_task_done 里有多少字节 */
static volatile uint8_t  s_task_ready;                  /* 1 = s_task_done 里有一整帧任务数据 */
static volatile uint32_t s_last_rx_tick;                /* 最后一个字节什么时候收到的 */
static uint8_t           s_hdr_buf[3];                  /* 最近 3 个字节, 用来认帧头 "【" */

static uint8_t           s_state      = GEST_OPEN;      /* 这一帧里认出来的状态(原始值) */
static uint8_t           s_enc        = ENC_NONE;       /* 认出来的关键字是哪种编码 */
static uint32_t          s_frames     = 0;              /* 一共收到多少帧 */
static uint32_t          s_state_seq  = 0;              /* 每帧可识别状态 +1, 给任务切换等新手势 */

/* ---- 状态防抖(防止 FPGA 识别抖动时爪子在开/合之间来回抽) ----
 * 「同一个状态连续 GEST_CONFIRM_FRAMES 帧」或者「同一个状态已经持续 GEST_CONFIRM_MS 毫秒」
 * 才算数, 只抖一帧不认。确认后的状态放在 s_state_out 里, 屏幕和爪子都用它。 */
static uint8_t           s_cand       = GEST_OPEN;      /* 正在确认的候选状态 */
static uint8_t           s_cand_cnt   = 0;              /* 候选状态连续出现了几帧 */
#if (GEST_CONFIRM_MS > 0)
static uint32_t          s_cand_tick  = 0;              /* 候选状态第一次出现的时间 */
#endif
static uint8_t           s_state_out  = GEST_OPEN;      /* 防抖确认后的状态 */
static uint8_t           s_out_ok     = 0;              /* 1 = 已经有确认过的状态了 */
static uint8_t           s_fresh      = 0;              /* 1 = 串口最近还有数据 */
static uint8_t           s_drive_flags = 0;             /* 最近的 drive[2]/drive[3] 电平 */
static uint8_t           s_drive_seen  = 0;             /* 1 = 至少收到过一个 drive[] 字段 */
static uint32_t          s_drive_one_count[GEST_DRIVE_FIELD_COUNT]; /* drive[0..9]=1 的次数 */
static uint8_t           s_drive_value[GEST_DRIVE_FIELD_COUNT];
static uint8_t           s_drive_value_valid[GEST_DRIVE_FIELD_COUNT];
static uint32_t          s_drive_change_seq[GEST_DRIVE_FIELD_COUNT];
static uint8_t           s_slider_stream_active = 0;    /* 收到[SLIDER]后等待value */
static uint8_t           s_slider_value_valid = 0;
static int32_t           s_slider_value = 0;
static uint32_t          s_slider_value_seq = 0;

/* 屏幕上要显示的三个数(从数据里解出来的坐标和深度) */
static int32_t           s_x     = 0;                   /* X 坐标 */
static int32_t           s_y     = 0;                   /* Y 坐标 */
static int32_t           s_depth = 0;                   /* 深度 */
static uint8_t           s_x_ok  = 0;                   /* 1 = 已经解出过 X 坐标(APP.c 靠它判断有没有坐标) */
static uint32_t          s_last_x_tick = 0;             /* ★ 最近一次解出 X 是什么时候(电机"该不该停"看它) */
static uint32_t          s_x_gap_ms    = 0;             /* ★ 相邻两次解出 X 的间隔(用它推"多久没坐标算断") */
static uint8_t           s_x_zero      = 0;             /* ★ 1 = 最近一帧的 X 是 0(FPGA 说"没识别到手") */
static uint8_t           s_y_ok        = 0;             /* 1 = 已经解出过 Y 坐标(和 s_x_ok 一个意思) */
static uint32_t          s_last_y_tick = 0;             /* ★ 最近一次解出 Y 是什么时候(电机4 "该不该停"看它) */
static uint32_t          s_y_gap_ms    = 0;             /* ★ 相邻两次解出 Y 的间隔(和 s_x_gap_ms 一个意思) */
static uint8_t           s_y_zero      = 0;             /* ★ 1 = 最近一帧的 Y 是 0(FPGA 说"没识别到手") */
static int32_t           s_finger      = 0;             /* ★ "手指"个数(FPGA 报的) */
static uint8_t           s_finger_seen = 0;             /* ★ 1 = 收到过"手指"字段(没收到过就不按它判) */
static uint32_t          s_finger_seq  = 0;             /* ★ 每解析到一帧"手指"字段 +1 */

/* 屏幕上那行十六进制显示什么:
 *   s_val_ok = 1 -> 取自"状态"后面那几个字节(最有用: 一眼看出 FPGA 发的到底是什么字)
 *   s_val_ok = 0 -> 没找到"状态"两个字, 就显示这一帧开头的几个字节 */
static uint8_t           s_val[GEST_HEX_BYTES];
static uint8_t           s_val_len    = 0;
static uint8_t           s_val_ok     = 0;

/* ==========================================================================
 *                          串口接收(中断里跑)
 * ========================================================================== */

/**
  * @brief  把 s_line 里收到的 n 个字节当成"一整帧"交给主循环
  * @note   只能在中断里调用。
  *         ★ 只留最新的一帧: 不管上一帧主循环取没取走, 直接覆盖。
  *           原来"上一帧没取走就把这一帧丢掉"的写法, 扔掉的其实是最新帧、留着的是旧帧 ——
  *           主循环一旦被发脉冲卡住几百毫秒, 电机就跟上几百毫秒前的旧坐标,
  *           手反向时还会朝旧方向走。改成覆盖式之后, 主循环每次拿到的都是刚收到的这一帧。
  */
static uint8_t Gest_HasControlFrame(const char *line)
{
    return TaskMode_IsControlFrame(line);
}

static uint8_t Gest_HasGestureField(const char *line)
{
    return ((strstr(line, "X:") != 0) ||
            (strstr(line, "Y:") != 0) ||
            (strstr(line, "drive[0]") != 0) ||
            (strstr(line, "drive[1]") != 0) ||
            (strstr(line, "drive[2]") != 0) ||
            (strstr(line, "drive[3]") != 0) ||
            (strstr(line, "drive[4]") != 0) ||
            (strstr(line, "drive[5]") != 0) ||
            (strstr(line, "drive[6]") != 0) ||
            (strstr(line, "drive[7]") != 0) ||
            (strstr(line, "drive[8]") != 0) ||
            (strstr(line, "drive[9]") != 0) ||
            (strstr(line, "[SLIDER]") != 0) ||
            (strstr(line, "#G,") != 0) ||
            (strstr(line, "\xE7\x8A\xB6\xE6\x80\x81") != 0) ||   /* 状态 UTF-8 */
            (strstr(line, "\xD7\xB4\xCC\xAC") != 0)) ? 1u : 0u;   /* 状态 GBK */
}

static void Gest_LineDone(uint16_t n)
{
    uint8_t has_task;
    uint8_t has_gesture;

    if (n == 0)
    {
        return;                                 /* 空的, 没什么可交的 */
    }

    if (n > GEST_LINE_MAX)
    {
        n = GEST_LINE_MAX;
    }

    s_line[n] = '\0';
    has_task    = Gest_HasControlFrame(s_line);
    has_gesture = Gest_HasGestureField(s_line);

    /* 任务帧单独放一份, 否则手势帧来得快时会把任务帧覆盖掉。
     * 如果 FPGA 把任务字段和手势放同一行, 两边都存, 手势和任务都能解析。 */
    if ((has_task != 0u) && (n <= GEST_TASK_LINE_MAX))
    {
        memcpy(s_task_done, s_line, n);
        s_task_done[n] = '\0';
        s_task_done_len = n;
        s_task_ready    = 1;                    /* 覆盖式, 只留最新任务帧 */
    }

    if ((has_task == 0u) || (has_gesture != 0u))
    {
        memcpy(s_done, s_line, n);
        s_done[n]    = '\0';
        s_done_len   = n;
        s_line_ready = 1;                       /* 通知主循环: 有一整帧可以取了(覆盖式的) */
    }
}

/**
 * @brief  串口接收完成回调: USART3 每收到 1 个字节进一次
  * @note   中断里只搬字节, 不解析、不刷屏, 尽量快进快出
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint8_t ch;

    if (huart->Instance != USART3)
    {
        return;
    }

    ch = s_rx_byte;
    s_last_rx_tick = HAL_GetTick();

    /* ---- 帧头识别: "【" 的 UTF-8 是 E3 80 90 ----
     * FPGA 要是不发换行, 那"下一帧的帧头"一来就说明上一帧已经结束了。
     * ★ > 2 和 - 2: 认出帧头时, 它的前 2 个字节(E3 80)已经跟着上一帧收进来了,
     *   所以要扣掉这 2 个; 也只有长度大于 2 时, 前面才确实还有上一帧的内容。 */
    s_hdr_buf[0] = s_hdr_buf[1];
    s_hdr_buf[1] = s_hdr_buf[2];
    s_hdr_buf[2] = ch;

    if ((s_hdr_buf[0] == 0xE3) && (s_hdr_buf[1] == 0x80) && (s_hdr_buf[2] == 0x90) &&
        (s_line_len > 2))
    {
        Gest_LineDone((uint16_t)(s_line_len - 2));

        s_line[0]  = (char)0xE3;
        s_line[1]  = (char)0x80;
        s_line_len = 2;
    }

    if (ch == '\n')                             /* 有换行就按换行分帧 */
    {
        Gest_LineDone(s_line_len);
        s_line_len = 0;
    }
    else if (ch != '\r')                        /* '\r' 直接丢掉 */
    {
        if (s_line_len < GEST_LINE_MAX)
        {
            s_line[s_line_len] = (char)ch;
            s_line_len++;
        }
        else                                    /* 太长了, 丢掉重来, 防止越界 */
        {
            s_line_len = 0;
        }
    }

    HAL_UART_Receive_IT(&huart3, &s_rx_byte, 1);    /* ★ 重新挂上接收, 不然只收得到 1 个字节 */
}

/**
  * @brief  串口错误回调(溢出/帧错误/噪声...)
  * @note   清一下标志再重新挂接收, 免得被干扰一次之后串口就"死"了
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3)
    {
        return;
    }

    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);

    s_line_len = 0;

    HAL_UART_Receive_IT(&huart3, &s_rx_byte, 1);
}

/* ==========================================================================
 *                              解析
 * ========================================================================== */

/**
  * @brief  这一行里有没有表里的某个词
  */
static uint8_t Gest_MatchList(const char *line, const char *const *list, uint8_t n)
{
    uint8_t i;

    for (i = 0; i < n; i++)
    {
        if (strstr(line, list[i]) != NULL)
        {
            return 1;
        }
    }

    return 0;
}

/**
  * @brief  从一帧文本里认"状态"
  * @retval 1 = 认出来了; 0 = 这一帧里没有状态字段
  * @note   ★ 先判"抓取(闭合)", 再判"松开"。
  */
static uint8_t Gest_ParseFrame(const char *line)
{
    /* ---------------- 抓取(夹紧) ---------------- */
    if (Gest_MatchList(line, KW_CLOSE_UTF8, LIST_N(KW_CLOSE_UTF8)) != 0)
    {
        s_state = GEST_CLOSE;
        s_enc   = ENC_UTF8;
        return 1;
    }
    if (Gest_MatchList(line, KW_CLOSE_GBK, LIST_N(KW_CLOSE_GBK)) != 0)
    {
        s_state = GEST_CLOSE;
        s_enc   = ENC_GBK;
        return 1;
    }
    if (Gest_MatchList(line, KW_CLOSE_EN, LIST_N(KW_CLOSE_EN)) != 0)
    {
        s_state = GEST_CLOSE;
        s_enc   = ENC_ASCII;
        return 1;
    }

    /* ---------------- 松开(张开) ---------------- */
    if (Gest_MatchList(line, KW_OPEN_UTF8, LIST_N(KW_OPEN_UTF8)) != 0)
    {
        s_state = GEST_OPEN;
        s_enc   = ENC_UTF8;
        return 1;
    }
    if (Gest_MatchList(line, KW_OPEN_GBK, LIST_N(KW_OPEN_GBK)) != 0)
    {
        s_state = GEST_OPEN;
        s_enc   = ENC_GBK;
        return 1;
    }
    if (Gest_MatchList(line, KW_OPEN_EN, LIST_N(KW_OPEN_EN)) != 0)
    {
        s_state = GEST_OPEN;
        s_enc   = ENC_ASCII;
        return 1;
    }

    return 0;                                   /* 这一帧里没认出状态 */
}

/**
  * @brief  找到"状态"后面(也就是状态值)的第一个字节
  * @retval 非 0 = 找到了; 0 = 这一帧里没有"状态"这两个字
  * @note   只用来在屏幕上显示"状态值那几个字节的十六进制": 方便判断 FPGA 发的是
  *         什么写法、什么编码(比如发的是"抓取"还是"握拳", 是 UTF-8 还是 GBK)。
  */
static const char *Gest_FindStateValue(const char *line)
{
    static const char *const anchor[] = {
        "\xE7\x8A\xB6\xE6\x80\x81",             /* 状态 (UTF-8) */
        "\xD7\xB4\xCC\xAC",                     /* 状态 (GBK)   */
    };
    const char *p;
    uint8_t i;
    uint8_t skip;

    for (i = 0; i < LIST_N(anchor); i++)
    {
        p = strstr(line, anchor[i]);
        if (p != NULL)
        {
            p += strlen(anchor[i]);             /* 跳过"状态"两个字 */

            /* 再跳过中间的分隔符(冒号、空格、等号...) */
            skip = 0;
            while ((*p != '\0') && (skip < 6))
            {
                if ((*p == ':') || (*p == '=') || (*p == ' ') || (*p == '\t') ||
                    (*p == '-') || ((uint8_t)*p == 0xEF) || ((uint8_t)*p == 0xA3))
                {
                    p++;
                    skip++;
                }
                else
                {
                    break;
                }
            }

            return p;
        }
    }

    return 0;
}

/**
  * @brief  在一行文本里找 key, 然后把 key 后面的第一个整数取出来
  * @param  line : 一整帧文本
  * @param  key  : 关键字(例如 "X:" 或者 "深度:")
  * @param  out  : 取到的整数放这儿
  * @retval 1 = 找到了;  0 = 这一帧里没有这个字段
  * @note   中间允许夹着空格和括号(FPGA 打印的时候冒号后面一般带空格)。
  */
static uint8_t Gest_GetInt(const char *line, const char *key, int32_t *out)
{
    const char *p;
    int32_t     sign  = 1;
    int32_t     val   = 0;
    uint8_t     digit = 0;
    uint8_t     skip  = 0;

    p = strstr(line, key);                      /* 先找字段名 */
    if (p == NULL)
    {
        return 0;
    }
    p += strlen(key);                           /* 跳到字段名后面 */

    /* 跳过中间不是数字的字符(空格、冒号、括号...), 最多跳 12 个 */
    while ((*p != '\0') && (skip < 12))
    {
        if (((*p >= '0') && (*p <= '9')) || (*p == '-') || (*p == '+'))
        {
            break;
        }
        p++;
        skip++;
    }

    if (*p == '-')
    {
        sign = -1;
        p++;
    }
    else if (*p == '+')
    {
        p++;
    }

    while ((*p >= '0') && (*p <= '9'))          /* 数字 */
    {
        val = val * 10 + (*p - '0');
        digit++;
        p++;
    }

    if (digit == 0)                             /* 后面没数字 */
    {
        return 0;
    }

    *out = sign * val;
    return 1;
}

/**
  * @brief  解出一个 X 坐标之后统一走这里: 更新坐标、记下时间、顺便量一下帧间隔
  * @note   ★ 帧间隔是用来判断"多久没有新坐标就算识别不到"的, 见 Gesture_XFresh()。
  *         间隔太离谱(>2 秒)说明中间断过一次, 就不拿来算平均了。
  */
static void Gest_XGot(int32_t v)
{
    uint32_t now = HAL_GetTick();
    uint32_t gap = now - s_last_x_tick;

#if GEST_X_ZERO_IS_NOHAND
    /* ★★ X = 0 = FPGA 没识别到手 ★★
     *   这里直接"不接受"这个坐标: 不更新 s_x, 也不更新时间戳 ——
     *   于是 APP.c 那边 Gesture_NoHand() 变成 1 -> 电机立刻停, 并且丢掉之前所有坐标
     *   重新对零点, 不会再拿着旧的 X 当目标往那边追。
     *   ★ 千万不能把 0 当成正常坐标存下来: 存下来目标位置一下就变成"零点 - 600 个单位",
     *     电机就会一路跑个不停(之前那版"跳变保护"最后就是这个后果)。 */
    if (v == 0)
    {
        s_x_zero = 1;
        return;
    }
#endif
    s_x_zero = 0;

    if ((s_last_x_tick != 0u) && (gap > 0u) && (gap < 2000u))
    {
        s_x_gap_ms = gap;
    }
    s_last_x_tick = now;

    s_x    = v;
    s_x_ok = 1;                                         /* 解出一个 X 了 */
}

/**
  * @brief  解出一个 Y 坐标之后统一走这里 —— 和上面 Gest_XGot() 一模一样, 只是换成 Y
  * @note   ★ 为什么 Y 也要有这一套: 电机4 跟的是 Y, 它要像电机1 那样"手一停/坐标一丢就
  *         立刻停", 就得知道"这一帧的 Y 新不新鲜、是不是 0"。
  *         ★ 以前 Y 是直接 s_y = v 存下来的: FPGA 丢手时 Y 常常是 0, 0 被当成正常坐标 ->
  *         电机4 拿着"0"当目标猛跑 -> 手一停就剧烈抖。现在 0 会被拒绝掉(见 GEST_Y_ZERO_IS_NOHAND)。
  */
static void Gest_YGot(int32_t v)
{
    uint32_t now = HAL_GetTick();
    uint32_t gap = now - s_last_y_tick;

#if GEST_Y_ZERO_IS_NOHAND
    /* ★★ Y = 0 = FPGA 没识别到手 ★★
     *   和 X 那条一样: 不接受这个坐标, 也不更新时间戳 ——
     *   于是 APP.c 那边 Gesture_YNoHand() 变成 1 -> 电机4 立刻停, 并丢掉之前所有坐标
     *   重新对零点, 不会再拿着旧的 Y 当目标往那边追。 */
    if (v == 0)
    {
        s_y_zero = 1;
        return;
    }
#endif
    s_y_zero = 0;

    if ((s_last_y_tick != 0u) && (gap > 0u) && (gap < 2000u))
    {
        s_y_gap_ms = gap;
    }
    s_last_y_tick = now;

    s_y    = v;
    s_y_ok = 1;                                         /* 解出一个 Y 了 */
}

/**
  * @brief  解析 FPGA 的紧凑格式: "#G,mode1,mode2,is_fist,手指,X,Y,深度"
  * @retval 1 = 认出来了(状态和 X/Y/深度都填好了); 0 = 不是这种格式
  * @note   万一 FPGA 那边发的是这种格式, 屏幕上的 X/Y/深度/状态照样能显示。
  */
static uint8_t Gest_ParseGLine(const char *line)
{
    const char *p;
    int32_t     val[7] = {0};
    uint8_t     has[7] = {0};
    uint8_t     field  = 0;
    uint8_t     digits = 0;

    p = strstr(line, "#G,");
    if (p == NULL)
    {
        return 0;
    }
    p += 3;                                     /* 跳过 "#G," */

    while ((*p != '\0') && (field < 7))
    {
        if ((*p >= '0') && (*p <= '9'))
        {
            val[field] = val[field] * 10 + (*p - '0');
            digits++;
        }
        else if (*p == ',')
        {
            if (digits != 0)
            {
                has[field] = 1;
            }
            field++;
            digits = 0;
        }
        else
        {
            break;
        }
        p++;
    }
    if ((field < 7) && (digits != 0))
    {
        has[field] = 1;
    }

    if ((has[0] == 0) && (has[2] == 0) && (has[4] == 0))
    {
        return 0;                               /* 不像这种格式 */
    }

    s_state = (val[2] != 0) ? GEST_CLOSE : GEST_OPEN;   /* is_fist: 1 = 抓取(闭合) */
    s_enc   = ENC_ASCII;
    s_depth = val[6];

    if (has[4] != 0)
    {
        Gest_XGot(val[4]);                              /* ★ 这一帧确实带 X: 更新坐标 + 时间 + 间隔 */
    }

    if (has[5] != 0)
    {
        Gest_YGot(val[5]);                              /* ★ 这一帧确实带 Y: 和 X 一样的处理 */
    }

    if (has[3] != 0)
    {
        s_finger      = val[3];                         /* ★ 紧凑格式里第 4 个字段是"手指" */
        s_finger_seen = 1;
        s_finger_seq++;
    }

    return 1;
}

/**
  * @brief  从一帧里解出 X / Y / 深度(认出来才更新, 认不出就保持上一次的值)
  */
static void Gest_ParseNumbers(const char *line)
{
    int32_t v;

    if (Gest_GetInt(line, "X:", &v) != 0)
    {
        Gest_XGot(v);                                   /* ★ 解出一个 X: 更新坐标 + 时间 + 间隔 */
    }
    if (Gest_GetInt(line, "Y:", &v) != 0)
    {
        Gest_YGot(v);                                   /* ★ 解出一个 Y: 更新坐标 + 时间 + 间隔 */
    }

    /* 深度: 中文(UTF-8 / GBK)和英文写法都试一遍 */
    if ((Gest_GetInt(line, KW_DEPTH_UTF8 ":", &v) != 0) ||
        (Gest_GetInt(line, KW_DEPTH_GBK  ":", &v) != 0) ||
        (Gest_GetInt(line, "DEP:",   &v) != 0) ||
        (Gest_GetInt(line, "DEPTH:", &v) != 0))
    {
        s_depth = v;
    }

    /* ★ 手指个数: 中文(UTF-8 / GBK)和英文写法都试一遍。
     *   0 表示"没识别到手" -> Gesture_NoHand() 会让电机立即停(见 fpga_gesture.h)。 */
    if ((Gest_GetInt(line, KW_FINGER_UTF8 ":", &v) != 0) ||
        (Gest_GetInt(line, KW_FINGER_UTF8 KW_COLON_FULL_UTF8, &v) != 0) ||
        (Gest_GetInt(line, KW_FINGER_GBK  ":", &v) != 0) ||
        (Gest_GetInt(line, KW_FINGER_GBK  KW_COLON_FULL_GBK, &v) != 0) ||
        (Gest_GetInt(line, "FINGER:",  &v) != 0) ||
        (Gest_GetInt(line, "FINGER" KW_COLON_FULL_UTF8, &v) != 0) ||
        (Gest_GetInt(line, "FINGER" KW_COLON_FULL_GBK, &v) != 0) ||
        (Gest_GetInt(line, "FINGERS:", &v) != 0) ||
        (Gest_GetInt(line, "FINGERS" KW_COLON_FULL_UTF8, &v) != 0) ||
        (Gest_GetInt(line, "FINGERS" KW_COLON_FULL_GBK, &v) != 0))
    {
        s_finger      = v;
        s_finger_seen = 1;                              /* 这一帧确实带"手指"字段 */
        s_finger_seq++;
    }
}

/**
  * @brief  解析手动任务里的 10 个驱动位: drive[0] ~ drive[9]
  * @note   只接受 0/1；每收到一次1, 对应计数加1, 不管之前是否为1。
  */
static void Gest_ParseDrive(const char *line)
{
    static const char *const keys[GEST_DRIVE_FIELD_COUNT] = {
        "drive[0]", "drive[1]", "drive[2]", "drive[3]",
        "drive[4]", "drive[5]", "drive[6]", "drive[7]",
        "drive[8]", "drive[9]"
    };
    int32_t value;
    uint8_t flags = s_drive_flags;
    uint8_t found = 0u;
    uint8_t i;

    for (i = 0u; i < GEST_DRIVE_FIELD_COUNT; i++)
    {
        if ((Gest_GetInt(line, keys[i], &value) != 0) &&
            ((value == 0) || (value == 1)))
        {
            found = 1u;
            s_drive_value[i]       = (uint8_t)value;
            s_drive_value_valid[i] = 1u;
            s_drive_change_seq[i]++;

            if (value == 1)
            {
                s_drive_one_count[i]++;
            }

            if (i == 2u)
            {
                if (value == 1)
                {
                    flags |= GEST_DRIVE_FORWARD;
                }
                else if (value == 0)
                {
                    flags &= (uint8_t)~GEST_DRIVE_FORWARD;
                }
            }
            else if (i == 3u)
            {
                if (value == 1)
                {
                    flags |= GEST_DRIVE_REVERSE;
                }
                else if (value == 0)
                {
                    flags &= (uint8_t)~GEST_DRIVE_REVERSE;
                }
            }
        }
    }

    if (found != 0u)
    {
        s_drive_flags = flags;
        s_drive_seen  = 1u;
    }
}

/**
  * @brief  解析调速模式: [SLIDER] 后收到的 value = N
  */
static void Gest_ParseSlider(const char *line)
{
    int32_t value;

    if (strstr(line, "[SLIDER]") != 0)
    {
        s_slider_stream_active = 1u;
    }

    if ((s_slider_stream_active != 0u) &&
        (Gest_GetInt(line, "value", &value) != 0))
    {
        s_slider_value       = value;
        s_slider_value_valid = 1u;
        s_slider_value_seq++;
    }
}

/* ==========================================================================
 *                              对外接口
 * ========================================================================== */

/**
  * @brief  初始化: 打开 USART3 的接收中断(要在 MX_USART3_UART_Init() 之后调用)
  */
void Gesture_Init(void)
{
    s_rx_byte      = 0;
    s_line_len     = 0;
    s_done_len     = 0;
    s_line_ready   = 0;
    s_task_done_len = 0;
    s_task_ready    = 0;
    s_hdr_buf[0]   = 0;
    s_hdr_buf[1]   = 0;
    s_hdr_buf[2]   = 0;
    s_last_rx_tick = HAL_GetTick();

    s_state        = GEST_OPEN;
    s_enc          = ENC_NONE;
    s_frames       = 0;
    s_state_seq    = 0;
    s_cand         = GEST_OPEN;
    s_cand_cnt     = 0;
#if (GEST_CONFIRM_MS > 0)
    s_cand_tick    = HAL_GetTick();
#endif
    s_state_out    = GEST_OPEN;
    s_out_ok       = 0;
    s_fresh        = 0;
    s_drive_flags  = 0;
    s_drive_seen   = 0;
    memset(s_drive_one_count, 0, sizeof(s_drive_one_count));
    memset(s_drive_value, 0, sizeof(s_drive_value));
    memset(s_drive_value_valid, 0, sizeof(s_drive_value_valid));
    memset(s_drive_change_seq, 0, sizeof(s_drive_change_seq));
    s_finger_seq   = 0;
    s_slider_stream_active = 0u;
    s_slider_value_valid = 0u;
    s_slider_value = 0;
    s_slider_value_seq = 0u;
    s_val_len      = 0;
    s_val_ok       = 0;
    s_x_ok         = 0;
    s_last_x_tick  = HAL_GetTick();
    s_x_gap_ms     = 0;
    s_x_zero       = 0;

    HAL_UART_Receive_IT(&huart3, &s_rx_byte, 1);    /* ★ 从这一行开始串口才真正开始收数据 */
}

/**
  * @brief  取整帧 -> 解析 -> 判断超时(主循环里反复调用)
  */
void Gesture_Poll(void)
{
    char        work[GEST_LINE_MAX + 1];
    const char *val;
    uint8_t     have = 0;
    uint8_t     got_state = 0;
    uint8_t     i;
    uint16_t    n;

    /* ★ 任务帧先用单独缓冲处理, 不和手势帧抢同一个 s_done。
     *   即使主循环正在刷屏, 任务标志也不会被下一帧手势覆盖。 */
    if (s_task_ready != 0)
    {
        NVIC_DisableIRQ(USART3_IRQn);
        n = s_task_done_len;
        if (n > GEST_TASK_LINE_MAX)
        {
            n = GEST_TASK_LINE_MAX;
        }
        memcpy(work, s_task_done, n);
        work[n] = '\0';
        s_task_ready = 0;
        NVIC_EnableIRQ(USART3_IRQn);

        TaskMode_OnFrame(work);
    }

    if (s_line_ready != 0)                          /* 中断那边攒好一帧了 */
    {
        /* ★ 只取最新的一帧, 并且取完就把缓冲清空(清标志)。
         *   复制这一帧的时候先把 USART3 中断关一下, 免得正好复制到一半又被新的一帧盖掉;
         *   就关几微秒而已 —— 115200 下收一个字节要 87us, 不会丢字节。 */
        n = s_done_len;
        if (n > GEST_LINE_MAX)
        {
            n = GEST_LINE_MAX;
        }

        NVIC_DisableIRQ(USART3_IRQn);
        memcpy(work, s_done, n);
        work[n]      = '\0';
        s_line_ready = 0;                           /* ★ 取走 = 清空通信缓冲, 下次只认最新 */
        NVIC_EnableIRQ(USART3_IRQn);

        have = 1;
    }
    else if ((s_line_len > 0) &&
             ((HAL_GetTick() - s_last_rx_tick) >= GEST_GAP_MS))
    {
        /* FPGA 要是不发换行: 一段时间收不到新字节, 就把这半行当成一整帧 */
        n = s_line_len;
        if (n > GEST_LINE_MAX)
        {
            n = GEST_LINE_MAX;
        }
        memcpy(work, s_line, n);
        work[n] = '\0';
        s_line_len = 0;
        have = 1;

        /* 没有换行、靠 20ms 间隔结束的任务帧也要走任务解析。
         * 如果整帧只有 TASK 字段, 就不要把它当成手势帧。 */
        if (Gest_HasControlFrame(work) != 0u)
        {
            TaskMode_OnFrame(work);
            if (Gest_HasGestureField(work) == 0u)
            {
                have = 0;
            }
        }
    }

    if (have != 0)
    {
        s_frames++;                                 /* 收到一整帧, 帧数 +1 */
        Gest_ParseDrive(work);
        Gest_ParseSlider(work);

        /* ---- 留给屏幕上那行十六进制: 优先显示"状态"后面的字节 ---- */
        val = Gest_FindStateValue(work);
        if (val != 0)
        {
            for (i = 0; (i < GEST_HEX_BYTES) && (val[i] != '\0'); i++)
            {
                s_val[i] = (uint8_t)val[i];
            }
            s_val_len = i;
            s_val_ok  = 1;
        }
        else
        {
            /* 没找到"状态"两个字: 就显示这一帧开头几个字节(看帧头和编码) */
            n = (uint16_t)strlen(work);
            if (n > GEST_HEX_BYTES)
            {
                n = GEST_HEX_BYTES;
            }
            for (i = 0; i < (uint8_t)n; i++)
            {
                s_val[i] = (uint8_t)work[i];
            }
            s_val_len = (uint8_t)n;
            s_val_ok  = 0;
        }

        /* ---- 认状态 + 解 X/Y/深度 ----
         * 先试 FPGA 的紧凑格式 "#G,..."; 不是的话再按关键字(状态 + X:/Y:/深度:)来解。 */
        if (Gest_ParseGLine(work) != 0)
        {
            got_state = 1;
        }
        else
        {
            Gest_ParseNumbers(work);                /* X / Y / 深度 */

            if (Gest_ParseFrame(work) != 0)         /* 状态 */
            {
                got_state = 1;
            }
        }

        /* ---- 防抖第 1 步: 把这一帧认出来的状态记到"候选"里, 并数它连续出现几帧 ---- */
        if (got_state != 0)
        {
            s_state_seq++;                          /* 这一帧确实带来了可识别状态 */

            if (s_cand == s_state)
            {
                if (s_cand_cnt < 255)
                {
                    s_cand_cnt++;
                }
            }
            else
            {
                s_cand      = s_state;              /* 换了个状态: 重新数 */
                s_cand_cnt  = 1;
#if (GEST_CONFIRM_MS > 0)
                s_cand_tick = HAL_GetTick();
#endif
            }
        }
    }

    /* ================= 下面这些每次调用都要做(和有没有新帧无关) ================= */

    /* ---- 防抖第 2 步: 候选状态"连续够多帧"或者"持续够久" -> 确认 ----
     * ★ 具体帧数/毫秒数看 fpga_gesture.h 里的 GEST_CONFIRM_FRAMES / GEST_CONFIRM_MS;
     *   两者满足任意一个就确认。GEST_CONFIRM_MS 填 0 = 关掉这条计时判据。 */
    if ((s_out_ok == 0) || (s_cand != s_state_out))
    {
        if (s_cand_cnt >= GEST_CONFIRM_FRAMES)
        {
            s_state_out = s_cand;
            s_out_ok    = 1;
        }
#if (GEST_CONFIRM_MS > 0)
        else if ((HAL_GetTick() - s_cand_tick) >= GEST_CONFIRM_MS)
        {
            s_state_out = s_cand;
            s_out_ok    = 1;
        }
#endif
    }

    /* ---- 串口还活着吗(GEST_TIMEOUT_MS 内还收到过字节): 给 LED / 调试用 ---- */
    s_fresh = ((HAL_GetTick() - s_last_rx_tick) <= GEST_TIMEOUT_MS) ? 1u : 0u;
}

uint8_t Gesture_State(void)
{
    return s_state_out;                     /* 防抖确认后的状态 */
}

uint8_t Gesture_IsClosed(void)
{
    return (s_state_out == GEST_CLOSE) ? 1u : 0u;
}

uint8_t Gesture_IsValid(void)
{
    return s_out_ok;
}

uint32_t Gesture_StateSeq(void)
{
    return s_state_seq;
}

void Gesture_ResetValid(void)
{
    s_out_ok       = 0;
    s_cand         = s_state_out;
    s_cand_cnt     = 0;
#if (GEST_CONFIRM_MS > 0)
    s_cand_tick    = HAL_GetTick();
#endif
    s_x_ok         = 0;
    s_y_ok         = 0;
    s_x_zero       = 0;
    s_y_zero       = 0;
    s_x_gap_ms     = 0;
    s_y_gap_ms     = 0;
    s_last_x_tick  = HAL_GetTick();
    s_last_y_tick  = HAL_GetTick();
    s_finger       = 0;
    s_finger_seen  = 0;
    s_depth        = 0;
}

/**
  * @brief  串口最近还有没有数据(GEST_TIMEOUT_MS 内有字节进来)
  * @note   和 Gesture_IsValid() 的区别: 这个只看"串口活没活着"(给 LED / 调试用),
  *         IsValid 看的是"有没有确认过的状态"(给爪子和电机用)。
  */
uint8_t Gesture_HasData(void)
{
    return s_fresh;
}

uint32_t Gesture_Frames(void)
{
    return s_frames;
}

uint8_t Gesture_HasDriveField(void)
{
    return s_drive_seen;
}

uint8_t Gesture_DriveFlags(void)
{
    return s_drive_flags;
}

uint32_t Gesture_DriveForwardCount(void)
{
    return s_drive_one_count[2];
}

uint32_t Gesture_DriveReverseCount(void)
{
    return s_drive_one_count[3];
}

uint8_t Gesture_FingerValid(void)
{
    return s_finger_seen;
}

int32_t Gesture_Finger(void)
{
    return s_finger;
}

uint32_t Gesture_FingerSeq(void)
{
    return s_finger_seq;
}

uint32_t Gesture_DriveOneCount(uint8_t index)
{
    if (index >= GEST_DRIVE_FIELD_COUNT)
    {
        return 0u;
    }

    return s_drive_one_count[index];
}

uint8_t Gesture_DriveValue(uint8_t index)
{
    if (index >= GEST_DRIVE_FIELD_COUNT)
    {
        return 0u;
    }

    return s_drive_value[index];
}

uint8_t Gesture_DriveValueValid(uint8_t index)
{
    if (index >= GEST_DRIVE_FIELD_COUNT)
    {
        return 0u;
    }

    return s_drive_value_valid[index];
}

uint32_t Gesture_DriveChangeSeq(uint8_t index)
{
    if (index >= GEST_DRIVE_FIELD_COUNT)
    {
        return 0u;
    }

    return s_drive_change_seq[index];
}

int32_t Gesture_SliderValue(void)
{
    return s_slider_value;
}

uint8_t Gesture_SliderValueValid(void)
{
    return s_slider_value_valid;
}

uint32_t Gesture_SliderValueSeq(void)
{
    return s_slider_value_seq;
}

int32_t Gesture_X(void)
{
    return s_x;
}

/* 1 = 已经解出过坐标(APP.c 用它保证"有坐标才开始跟", 上电/刚重连时不会自己转) */
uint8_t Gesture_XValid(void)
{
    return s_x_ok;
}

/**
  * @brief  最近还有没有"新的坐标" —— 电机靠它决定该不该继续跟
  * @retval 1 = 还在正常收坐标; 0 = 太久没解出 X 了, 应当立即停
  * @note   ★ 看的是"坐标新不新鲜", 不是"串口有没有数据":
  *         串口还在发帧、但帧里没有坐标, 这里也是 0 -> 电机马上停。
  *         ★ 判据不写死: 用"最近一次帧间隔 × 3"当阈值(夹在 GEST_X_TIMEOUT_MIN_MS 和
  *         GEST_X_TIMEOUT_MAX_MS 之间) —— FPGA 帧快就停得快, 帧慢也不会误停。
  */
uint8_t Gesture_XFresh(void)
{
    uint32_t limit = s_x_gap_ms * 3u;

    if (limit < GEST_X_TIMEOUT_MIN_MS)
    {
        limit = GEST_X_TIMEOUT_MIN_MS;
    }
    if (limit > GEST_X_TIMEOUT_MAX_MS)
    {
        limit = GEST_X_TIMEOUT_MAX_MS;
    }

    return ((HAL_GetTick() - s_last_x_tick) <= limit) ? 1u : 0u;
}

/**
  * @brief  是不是已经"很久"没有有效坐标了(手真的走了/线掉了)
  * @retval 1 = 超过 GEST_X_RESYNC_MS 没拿到有效坐标 -> 该重新对零点(不要往旧位置追)
  */
uint8_t Gesture_XLostLong(void)
{
    return ((HAL_GetTick() - s_last_x_tick) >= GEST_X_RESYNC_MS) ? 1u : 0u;
}

/**
  * @brief  FPGA 最近一帧报的是不是"没识别到手"
  * @retval 1 = 手没了(应当立即停并丢掉旧坐标)
  * @note   两个信号, 谁先到听谁的:
  *           X = 0      -> FPGA 没识别到手 (GEST_X_ZERO_IS_NOHAND)
  *           手指 = 0   -> FPGA 报的"手指"是 0 (GEST_FINGER0_IS_NOHAND)
  *         ★ "手指"字段一次都没收到过时, 不按它判(免得换个格式就永远不停/永远停)。
  */
uint8_t Gesture_NoHand(void)
{
#if GEST_X_ZERO_IS_NOHAND
    if (s_x_zero != 0u)
    {
        return 1u;
    }
#endif
#if GEST_FINGER0_IS_NOHAND
    if ((s_finger_seen != 0u) && (s_finger == 0))
    {
        return 1u;
    }
#endif
    return 0u;                                          /* 没有"没手"的信号 */
}

int32_t Gesture_Y(void)
{
    return s_y;
}

/* 1 = 已经解出过 Y 坐标(和 Gesture_XValid() 一个意思, 给电机4 用) */
uint8_t Gesture_YValid(void)
{
    return s_y_ok;
}

/**
  * @brief  最近还有没有"新的 Y 坐标" —— 电机4 靠它决定该不该继续跟
  * @retval 1 = 还在正常收 Y; 0 = 太久没解出 Y 了, 应当立即停
  * @note   和 Gesture_XFresh() 完全一个道理: 看的是"Y 新不新鲜", 不是"串口有没有数据";
  *         阈值也是"最近一次帧间隔 × 3", 夹在 GEST_Y_TIMEOUT_MIN_MS 和 _MAX_MS 之间。
  */
uint8_t Gesture_YFresh(void)
{
    uint32_t limit = s_y_gap_ms * 3u;

    if (limit < GEST_Y_TIMEOUT_MIN_MS)
    {
        limit = GEST_Y_TIMEOUT_MIN_MS;
    }
    if (limit > GEST_Y_TIMEOUT_MAX_MS)
    {
        limit = GEST_Y_TIMEOUT_MAX_MS;
    }

    return ((HAL_GetTick() - s_last_y_tick) <= limit) ? 1u : 0u;
}

/**
  * @brief  是不是已经"很久"没有有效 Y 了(手真的走了 / 线掉了)
  * @retval 1 = 超过 GEST_Y_RESYNC_MS 没拿到有效 Y -> 该重新对零点(不要往旧位置追)
  */
uint8_t Gesture_YLostLong(void)
{
    return ((HAL_GetTick() - s_last_y_tick) >= GEST_Y_RESYNC_MS) ? 1u : 0u;
}

/**
  * @brief  FPGA 最近一帧报的 Y 是不是"没识别到手"(Y = 0)
  * @retval 1 = 手没了(电机4 应当立即停, 并丢掉旧坐标重新对零点)
  * @note   ★ 这一条就是"手一停, 电机4 不再乱抖"的关键: 丢手的那几帧 Y 常常是 0,
  *         以前会被当成正常坐标, 现在这里直接拦住(见 GEST_Y_ZERO_IS_NOHAND)。
  */
uint8_t Gesture_YNoHand(void)
{
    return (s_y_zero != 0u) ? 1u : 0u;
}

int32_t Gesture_Depth(void)
{
    return s_depth;
}

/* ==========================================================================
 *                              屏幕显示
 * ========================================================================== */

/**
  * @brief  把收到的几个字节拼成 "S:E68A93E58F96" 这样的十六进制串
  * @param  tag : 'S' = 这几个字节取自"状态"字段; 'H' = 取自这一帧的开头
  */
#if GEST_SHOW_DEBUG
static void Gest_MakeHexLine(char *buf, char tag)
{
    static const char hex_digit[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                       '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    uint8_t i;
    uint8_t o = 0;

    buf[o] = tag;
    o++;
    buf[o] = ':';
    o++;

    for (i = 0; i < s_val_len; i++)
    {
        buf[o] = hex_digit[(s_val[i] >> 4) & 0x0F];
        o++;
        buf[o] = hex_digit[s_val[i] & 0x0F];
        o++;
    }
    buf[o] = '\0';
}
#endif /* GEST_SHOW_DEBUG */

/**
  * @brief  刷屏幕(10Hz)
  * @note   屏幕上就是你要的这 4 行(8x16 字体, 一行 16 像素高):
  *           X     : 手的 X 坐标
  *           Y     : 手的 Y 坐标
  *           STATE : GRAB(抓取) / OPEN(松开) / ??(没认出来) / --(没数据)
  *           DEP   : 深度
  *         下面还有两行调试信息(RX 帧数 / 编码 / 状态字段的十六进制), 用
  *         fpga_gesture.h 里的 GEST_SHOW_DEBUG 开关, 调好之后改成 0 就只剩上面 4 行。
  *         舵机当前角度那一行(ANGLE)是 APP.c 画的, 位置同在 fpga_gesture.h 里。
  */
void Gesture_Show(void)
{
    static uint32_t last = 0;
    char            line[GEST_HEX_BYTES * 2 + 4];
    const char     *st;

    if ((HAL_GetTick() - last) < GEST_SHOW_MS)
    {
        return;
    }
    last = HAL_GetTick();

    /* ---- ① X 轴坐标 ---- */
    lcd_ShowString(GEST_X0, GEST_Y_X, "X:", 0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);
    lcd_ShowInt(GEST_X0 + 48, GEST_Y_X, (int)s_x, 5, 0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);

    /* ---- ② Y 轴坐标 ---- */
    lcd_ShowString(GEST_X0, GEST_Y_Y, "Y:", 0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);
    lcd_ShowInt(GEST_X0 + 48, GEST_Y_Y, (int)s_y, 5, 0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);

    /* ---- ③ 状态(state): 抓取 -> GRAB, 松开 -> OPEN ----
     * 屏幕自带的是 ASCII 字体, 画不了汉字, 所以这里用英文表示(意思一样)。 */
    if (s_out_ok == 0)
    {
        /* 一帧都没收到 -> --; 收到了但认不出状态 -> ??(这时看下面那行十六进制) */
        st = (s_frames == 0) ? "--   " : "??   ";
    }
    else
    {
        st = (s_state_out == GEST_CLOSE) ? "GRAB " : "OPEN ";
    }
    lcd_ShowString(GEST_X0, GEST_Y_STATE, "STATE:", 0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);
    lcd_ShowString(GEST_X0 + 48, GEST_Y_STATE, st, 0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);

    /* ---- ④ 深度 ---- */
    lcd_ShowString(GEST_X0, GEST_Y_DEP, "DEP:", 0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);
    lcd_ShowInt(GEST_X0 + 48, GEST_Y_DEP, (int)s_depth, 5, 0x0000, 0xFFFF, LCD_FONT_SIZE_8x16);

#if GEST_SHOW_DEBUG
    /* ---- 调试行 1: 收到多少帧 + 认出来的是哪种编码 ---- */
    display_String(GEST_X0, GEST_Y_RX, "RX:");
    lcd_ShowInt(GEST_X0 + 18, GEST_Y_RX, (int)s_frames, 5, 0x0000, 0xFFFF, LCD_FONT_SIZE_6x8);

    if (s_enc == ENC_UTF8)
    {
        display_String(GEST_X0 + 54, GEST_Y_RX, "EN:UTF8 ");
    }
    else if (s_enc == ENC_GBK)
    {
        display_String(GEST_X0 + 54, GEST_Y_RX, "EN:GBK  ");
    }
    else if (s_enc == ENC_ASCII)
    {
        display_String(GEST_X0 + 54, GEST_Y_RX, "EN:ASCII");
    }
    else
    {
        display_String(GEST_X0 + 54, GEST_Y_RX, "EN:--   ");
    }

    /* ---- 同一行最右边再加一个手指数 F:<n> ----
     *   ★ 拿手的时候看一眼这个数: 它是 0 就表示 FPGA 认为"没识别到手", 电机会立即停;
     *     如果手明明在、这里却是 0, 就把 fpga_gesture.h 的 GEST_FINGER0_IS_NOHAND 改成 0。 */
    display_String(GEST_X0 + 102, GEST_Y_RX, "F:");
    lcd_ShowInt(GEST_X0 + 114, GEST_Y_RX, (int)s_finger, 2, 0x0000, 0xFFFF, LCD_FONT_SIZE_6x8);

    /* ---- 调试行 2: 状态字段(或者帧头)那几个字节的十六进制 ---- */
    Gest_MakeHexLine(line, (s_val_ok != 0) ? 'S' : 'H');
    display_String(GEST_X0, GEST_Y_HEX, line);
#else
    /* 调试信息关掉的时候: 这几个变量/缓冲区没地方用, 加一句"假装用一下",
     * 免得编译器报"变量定义了没用到"的无害警告 */
    (void)line;
    (void)s_enc;
    (void)s_val;
    (void)s_val_ok;
    (void)s_val_len;
    (void)s_finger;
#endif /* GEST_SHOW_DEBUG */
}

/* USER CODE END 1 */
