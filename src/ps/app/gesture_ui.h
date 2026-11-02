/**
 * @file gesture_ui.h
 * @brief 手势识别 LVGL 界面:界面构建、刷新与引擎循环
 */

#ifndef GESTURE_UI_H
#define GESTURE_UI_H

/*********************
 *      数据结构
 *********************/

/* 一次手势采样的结果,由 main.c 的采样回调填充 */
typedef struct {
    int center_x;       /* 手心 X(摄像头坐标系) */
    int center_y;       /* 手心 Y */
    int finger_num;     /* 伸出的手指数 */
    int is_fist;        /* 1 = 握拳 */
    int mode1;          /* 下位机模式位 */
    int mode2;
    int depth;          /* 深度值 */
    int hand_detected;  /* 是否在画面中检测到手 */
} gesture_info_t;
typedef enum {
    PAGE_HOME = 0,
    PAGE_MANUL = 1,
	PAGE_AUTO = 2,

} page_state_t;

static page_state_t g_current_page = PAGE_HOME;
/* 采样回调:界面每 30ms 调一次,由 main.c 实现(读 AXI GPIO、转发串口协议包) */
typedef void (*gesture_sample_fn)(gesture_info_t *info);

/*********************
 *      接口函数
 *********************/

/**
 * 初始化 LVGL 引擎、显示驱动和手势界面,并启动 30ms 刷新定时器。
 * @param sample_cb 采样回调,不可为 NULL
 */
void gesture_ui_init(gesture_sample_fn sample_cb);

/**
 * LVGL 引擎主循环(不返回):喂 tick、跑定时器和界面重绘。
 */
void gesture_ui_run(void);

#endif /* GESTURE_UI_H */
