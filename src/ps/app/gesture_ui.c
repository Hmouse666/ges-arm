/**
 * @file gesture_ui.c
 * @brief 手势机械臂主页面: Welcome 扇出启动页 -> 主页面(task1/task2/task3)
 */

#include "gesture_ui.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "sleep.h"
#include "xil_printf.h"
#include "xtime_l.h"
#include "demos/lv_demos.h"
#include "lv_port_indev.h"
#include "xgpio.h"
/* 0: 启动 Welcome 页 -> 主页面(当前); 1: 直接运行 LVGL widgets Demo */
#define GESTURE_UI_USE_WIDGETS_DEMO 0

/* 1 = 在串口上同时打印人类可读的手势信息；0 = 只发 #G,... 协议包 */
#define GESTURE_UART_DEBUG   1

#define SPLASH_HOLD_MS       2800
#define TITLE_Y              42
#define HOME_BG_TOP          0x0B0E14
#define HOME_BG_BOTTOM       0x1B2430
#define TITLE_RED            0xE53935
#define TASK_BG              0x1565C0
#define TASK_BG_PRESSED      0x0D47A1
#define TASK_BORDER          0x64B5F6
#define TASK_TEXT            0xFFFFFF
#define AUTO_TASK_COUNT      3
#define AUTO_TASK_BG         0x000000
#define AUTO_TASK_SELECTED   TASK_BG_PRESSED

/* 红点中心映射范围：LCD 右下角跟踪区 */
#define AUTO_DOT_X0          380
#define AUTO_DOT_Y0          260
#define AUTO_DOT_X1          780
#define AUTO_DOT_Y1          455

/* 启动页图片资源尺寸: 725 x 139 (ARGB8888 真彩) */
#define WELCOME_W            725
#define WELCOME_H            139
#define WELCOME_MASK_H       150
#define WELCOME_X            ((MY_DISP_HOR_RES - WELCOME_W) / 2)
#define WELCOME_Y            ((MY_DISP_VER_RES - WELCOME_MASK_H) / 2)

LV_IMG_DECLARE(img_lvgl_logo);
LV_IMG_DECLARE(img_demo_widgets_avatar);
LV_IMG_DECLARE(img_clothes);
LV_IMG_DECLARE(img_welcome);
LV_IMG_DECLARE(img_arm);
LV_IMG_DECLARE(img_manul);
LV_IMG_DECLARE(img_auto);

static lv_obj_t *g_home_page = NULL;
static lv_obj_t *g_manul_page = NULL;
static lv_obj_t *g_auto_page = NULL;
static lv_obj_t *g_task_status = NULL;
static gesture_sample_fn s_sample_cb = NULL;
static XGpio bl_gpio;
static lv_obj_t *g_track_dot = NULL; //auto页面追踪点
static lv_obj_t *g_coord_label = NULL; //auto页面左上角坐标
static lv_obj_t *g_task2_running_label = NULL; //TASK2无手势提示
static lv_obj_t *g_auto_task_btns[AUTO_TASK_COUNT] = {NULL};
static int32_t g_auto_selected_task = -1; // -1: 未选择; 0/1/2: TASK1/2/3
static int32_t g_slider_value = 0;
/**********************
 *   Welcome 扇出动画
 **********************/
static void backlight_on(void)
{
    XGpio_Initialize(&bl_gpio, XPAR_AXI_GPIO_1_DEVICE_ID);
    XGpio_SetDataDirection(&bl_gpio, 1, 0x0);   /* 输出 */
    XGpio_DiscreteWrite(&bl_gpio, 1, 1);        /* 拉高 lcd_bl_en */
}

static void reveal_width_anim_cb(void *var, int32_t value)
{
    lv_obj_set_width((lv_obj_t *)var, value);
}

static void create_reveal_layer(lv_obj_t *parent, lv_color_t color,
                                lv_opa_t opacity, lv_coord_t y_offset,
                                uint32_t delay_ms, bool recolor)
{
    lv_obj_t *mask = lv_obj_create(parent);
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, 0, WELCOME_MASK_H);
    lv_obj_set_pos(mask, WELCOME_X, WELCOME_Y + y_offset);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *image = lv_img_create(mask);
    lv_img_set_src(image, &img_welcome);
    if(recolor) {
        /* 单色蒙版图片才需要染色；彩色图片必须关闭，否则会变成纯色块 */
        lv_obj_set_style_img_recolor(image, color, 0);
        lv_obj_set_style_img_recolor_opa(image, LV_OPA_COVER, 0);
    }
    lv_obj_set_style_img_opa(image, opacity, 0);
    lv_obj_align(image, LV_ALIGN_LEFT_MID, 0, 0);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, mask);
    lv_anim_set_values(&anim, 0, WELCOME_W);
    lv_anim_set_time(&anim, 1100);
    lv_anim_set_delay(&anim, delay_ms);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, reveal_width_anim_cb);
    lv_anim_start(&anim);
}

static void create_splash_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 彩色图片：单层显示，保留从左到右的展开动画，不做染色 */
    create_reveal_layer(scr, lv_color_hex(0xFFFFFF), LV_OPA_COVER, 0, 0, false);

    lv_scr_load(scr);
}

/**********************
 *   主页面控件
 **********************/
static void create_faux_bold_title(lv_obj_t *parent, const char *text)
{
    static const lv_point_t offsets[] = {
        {0, 0}, {1, 0}, {0, 1}, {1, 1},
    };

    for(uint32_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(TITLE_RED), 0);
        lv_obj_align(label, LV_ALIGN_TOP_MID, offsets[i].x, TITLE_Y + offsets[i].y);
    }
}

static void task_event_cb(lv_event_t *event)
{
    const char *task_name = (const char *)lv_event_get_user_data(event);

    if(g_task_status != NULL) {
        lv_label_set_text_fmt(g_task_status, "%s selected", task_name);
    }

    LV_LOG_USER("task clicked: %s", task_name);
}

static void create_task_button(lv_obj_t *parent, const char *task_name)
{
    lv_obj_t *button = lv_btn_create(parent);

    lv_obj_set_size(button, 300, 74);
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(TASK_BG), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_50, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(TASK_BORDER), 0);
    lv_obj_set_style_border_opa(button, LV_OPA_60, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(TASK_BG_PRESSED), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_70, LV_STATE_PRESSED);


    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, task_name);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(TASK_TEXT), 0);
    lv_obj_center(label);
}



static int g_splash_done = 0;              /* 启动页动画结束标志 */

static void splash_done_cb(lv_timer_t *t)
{
    (void)t;
    g_splash_done = 1;
}

static void splash_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if(g_home_page != NULL) {
        lv_scr_load_anim(g_home_page, LV_SCR_LOAD_ANIM_FADE_IN, 450, 0, true);
    }

    /* 等淡入动画（450ms）结束后，才允许手势/按钮数据走串口 */
    lv_timer_t *t2 = lv_timer_create(splash_done_cb, 500, NULL);
    lv_timer_set_repeat_count(t2, 1);
}

static uint32_t g_drive[10] = {0};
static uint32_t g_back[3] = {0};

/* auto 页面左侧任务栏：选择后保持深蓝色，切换任务时只保留一个选中项 */
static void auto_task_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    uint32_t selected_idx =
            (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if(selected_idx >= AUTO_TASK_COUNT) {
        return;
    }

    int32_t previous_task = g_auto_selected_task;
    g_auto_selected_task = (int32_t)selected_idx;

    lv_obj_t *selected_btn = lv_event_get_target(e);
    for(uint32_t i = 0; i < AUTO_TASK_COUNT; i++) {
        if(g_auto_task_btns[i] == NULL) {
            continue;
        }
        if(i == selected_idx) {
            lv_obj_add_state(g_auto_task_btns[i], LV_STATE_CHECKED);
        }
        else {
            lv_obj_clear_state(g_auto_task_btns[i], LV_STATE_CHECKED);
        }
    }

    lv_obj_add_state(selected_btn, LV_STATE_CHECKED);

    if(g_task_status != NULL) {
        lv_label_set_text_fmt(g_task_status, "TASK %u selected",
                              (unsigned)(selected_idx + 1));
    }
    if(previous_task != g_auto_selected_task) {
        xil_printf("[TASK] TASK %u selected\r\n",
                   (unsigned)(selected_idx + 1));
    }
}

static void create_auto_task_menu(lv_obj_t *parent)
{
    static const char *const task_names[AUTO_TASK_COUNT] = {
        "TASK 1", "TASK 2", "TASK 3"
    };

    lv_obj_t *menu = lv_obj_create(parent);
    lv_obj_set_size(menu, 320, 330);
    lv_obj_align(menu, LV_ALIGN_LEFT_MID, 20, 20);
    lv_obj_set_style_bg_opa(menu, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(menu, 0, 0);
    lv_obj_set_style_pad_all(menu, 16, 0);
    lv_obj_set_style_pad_row(menu, 10, 0);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);

    for(uint32_t i = 0; i < AUTO_TASK_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(menu);
        lv_obj_set_size(btn, lv_pct(100), 64);
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(AUTO_TASK_BG), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(TASK_BORDER), 0);
        lv_obj_set_style_border_opa(btn, LV_OPA_60, 0);

        lv_obj_set_style_bg_color(btn, lv_color_hex(AUTO_TASK_SELECTED),
                                  LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(AUTO_TASK_SELECTED),
                                  LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_color_hex(TASK_BORDER),
                                      LV_STATE_CHECKED);

        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(btn, auto_task_event_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, task_names[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(TASK_TEXT), 0);
        lv_obj_center(label);

        g_auto_task_btns[i] = btn;
    }
}

//manul按键方向驱动回调
static void drive_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    if (idx >= 10) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        g_drive[idx] = 1;                 /* 按下：立即开始持续发送 */
        xil_printf("[MANUL] drive[%d] = 1\r\n", idx);
    }
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_drive[idx] = 0;                 /* 松开 / 滑出：立即停止发送 */
        xil_printf("[MANUL] drive[%d] = 0\r\n", idx);
    }
}
//进入manul页面事件
static void manul_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    	xil_printf("manul ready!\n");
        if (g_manul_page != NULL) {
        	g_current_page = PAGE_MANUL;
            lv_scr_load_anim(g_manul_page, LV_SCR_LOAD_ANIM_FADE_IN, 60, 0, false);
        }
    }
}
//进入auto页面事件
static void auto_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    	xil_printf("auto ready!\n");
        if (g_auto_page != NULL) {
        	g_current_page = PAGE_AUTO;
            lv_scr_load_anim(g_auto_page, LV_SCR_LOAD_ANIM_FADE_IN, 60, 0, false);
        }
    }
}

//返回主页面回调
static void manul_btn_back_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
	    	uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
	    	if (g_manul_page != NULL) {
	    		g_current_page = PAGE_HOME;
	    	    lv_scr_load_anim(g_home_page, LV_SCR_LOAD_ANIM_FADE_OUT, 60, 0, false);
	    	        }
	}
}


//manul页面下的方向键
static void create_manul_btn(lv_obj_t *parent, lv_coord_t x_ofs, lv_coord_t y_ofs, const char *title,lv_event_cb_t event_cb,void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 90, 90);
    lv_obj_align(btn, LV_ALIGN_CENTER, x_ofs, y_ofs);
    lv_obj_set_style_radius(btn, 16, 0);

    /*背景不透明度设为 0 */
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 5);

    /* 按下状态也可以保持透明，或者给一点半透明反馈 */
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);

    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL,user_data);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_center(label);
}
//manul页面下的控制键
static void create_manul_dri_btn(lv_obj_t *parent, lv_coord_t x_ofs, lv_coord_t y_ofs, const char *title,lv_event_cb_t event_cb,void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 90, 90);
    lv_obj_align(btn, LV_ALIGN_CENTER, x_ofs, y_ofs);
    lv_obj_set_style_radius(btn, 16, 0);

    /*背景不透明度设为 0 */
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 5);

    /* 按下状态也可以保持透明，或者给一点半透明反馈 */
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);

    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL,user_data);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_center(label);
}
//创建返回键，一键返回home page
static void create_back_btn(lv_obj_t *parent, lv_coord_t x_ofs, lv_coord_t y_ofs, const char *title,lv_event_cb_t event_cb,void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 80, 20);
    lv_obj_align(btn, LV_ALIGN_CENTER, x_ofs, y_ofs);
    lv_obj_set_style_radius(btn, 16, 0);

    /*背景不透明度设为 0 */
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 5);

    /* 按下状态也可以保持透明，或者给一点半透明反馈 */
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);

    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL,user_data);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_center(label);
}

//创建按键(主页面)
static void create_count_btn(lv_obj_t *parent, lv_coord_t x_ofs, lv_coord_t y_ofs, const char *title,lv_event_cb_t event_cb)
{

	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_set_size(btn, 300, 90);
	lv_obj_align(btn, LV_ALIGN_CENTER, x_ofs, y_ofs);
	lv_obj_set_style_radius(btn, 16, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(TASK_BG), 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(TASK_BG_PRESSED), LV_STATE_PRESSED);

	lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_center(label);

    /* 红色阴影：直接设置到控件上（lv_style_set_* 需要 lv_style_t，不能传控件指针） */
    lv_obj_set_style_shadow_width(btn, 55, 0);
    lv_obj_set_style_shadow_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_shadow_ofs_x(btn, 0, 0);
    lv_obj_set_style_shadow_ofs_y(btn, 20, 0);
    lv_obj_set_style_shadow_spread(btn, 0, 0);
}
//滑块读值回调
static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    g_slider_value = lv_slider_get_value(slider);
}
// 滑块绘制
static void create_slider(lv_obj_t *parent, lv_event_cb_t event_cb)
{
    /* 1. 创建滑块 */
	    lv_obj_t *slider = lv_slider_create(parent);
	    lv_obj_set_size(slider, 330, 50);   // 👈 尺寸在这里改：宽180，高30

	    /* 2. 样式 */
	    static lv_style_t style_indic;
	    lv_style_init(&style_indic);
	    lv_style_set_bg_color(&style_indic, lv_palette_lighten(LV_PALETTE_RED, 3));
	    lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
	    lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_HOR);

	    static lv_style_t style_indic_pr;
	    lv_style_init(&style_indic_pr);
	    lv_style_set_shadow_color(&style_indic_pr, lv_palette_main(LV_PALETTE_RED));
	    lv_style_set_shadow_width(&style_indic_pr, 10);
	    lv_style_set_shadow_spread(&style_indic_pr, 3);

	    lv_obj_add_style(slider, &style_indic, LV_PART_INDICATOR);
	    lv_obj_add_style(slider, &style_indic_pr, LV_PART_INDICATOR | LV_STATE_PRESSED);

	    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
	    lv_obj_add_event_cb(slider, event_cb, LV_EVENT_VALUE_CHANGED, NULL);
	    /* 3. 定位：放到右下角 */
	    lv_obj_align(slider, LV_ALIGN_BOTTOM_RIGHT, -20, -80);   /* 距右/下边像素 */
}
static void set_temp(void *bar, int32_t temp)
{
    lv_bar_set_value(bar, temp, LV_ANIM_ON);
}

static void create_temp_bar(lv_obj_t *parent)
{
    /* 1. 样式：横向渐变 */
    static lv_style_t style_indic;
    lv_style_init(&style_indic);
    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_HOR);   //  水平

    /* 2. 创建进度条：宽 > 高 → 横向 */
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_add_style(bar, &style_indic, LV_PART_INDICATOR);
    lv_obj_set_size(bar, 200, 20);                              // 宽200、高20
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 200, -350);             // 坐标
    lv_bar_set_range(bar, -20, 40);

    /* 3. 自动来回动画（原样保留） */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_temp);
    lv_anim_set_time(&a, 3000);
    lv_anim_set_playback_time(&a, 3000);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, -20, 40);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}
static void create_home_page(void)
{
    g_home_page = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(g_home_page, lv_color_hex(HOME_BG_TOP), 0);
    lv_obj_set_style_bg_opa(g_home_page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(g_home_page, LV_OBJ_FLAG_SCROLLABLE);

    /* 左侧机械臂图片 */
    lv_obj_t *arm_img = lv_img_create(g_home_page);
    lv_img_set_src(arm_img, &img_arm);
    lv_obj_align(arm_img, LV_ALIGN_LEFT_MID, 30, 0);

    lv_obj_clear_flag(arm_img, LV_OBJ_FLAG_SCROLLABLE);
    //初始按键
    lv_obj_t *title = lv_label_create(g_home_page);
    lv_label_set_text(title, "Home Page");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(TITLE_RED), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    create_count_btn(g_home_page, 215, -140,"Auto",auto_btn_event_cb);   // 按钮1
    create_count_btn(g_home_page, 215,  0 ,"Manul",manul_btn_event_cb);    // 按钮2

    create_slider(g_home_page,slider_event_cb);//滑块3


//    g_count_label = lv_label_create(g_home_page);
//    lv_label_set_text(g_count_label, "0");
//    lv_obj_set_style_text_font(g_count_label, &lv_font_montserrat_48, 0);
//    lv_obj_set_style_text_color(g_count_label, lv_color_hex(0xFFFFFF), 0);
//    lv_obj_align(g_count_label, LV_ALIGN_CENTER, 0, -70);



}
static void create_manul_page(void)
{
	g_manul_page = lv_obj_create(NULL);

	lv_obj_set_style_bg_color(g_manul_page, lv_color_hex(HOME_BG_TOP), 0);
	lv_obj_set_style_bg_opa(g_manul_page, LV_OPA_COVER, 0);
	lv_obj_clear_flag(g_manul_page, LV_OBJ_FLAG_SCROLLABLE);

	    /* manul图片 */
	lv_obj_t *manul_img = lv_img_create(g_manul_page);
	lv_img_set_src(manul_img, &img_manul);
	lv_obj_set_pos(manul_img, 0, 0);

	create_manul_btn(g_manul_page, -200, -60, "UP", drive_btn_event_cb,(void*)(uintptr_t)0);
	create_manul_btn(g_manul_page, -200, 120, "DOWN", drive_btn_event_cb,(void*)(uintptr_t)1);
	create_manul_btn(g_manul_page, -290, 30, "LEFT", drive_btn_event_cb,(void*)(uintptr_t)2);
	create_manul_btn(g_manul_page, -110, 30, "RIGHT", drive_btn_event_cb,(void*)(uintptr_t)3);

	create_manul_btn(g_manul_page, 200, -60, "UP", drive_btn_event_cb,(void*)(uintptr_t)4);
	create_manul_btn(g_manul_page, 200, 120, "DOWN", drive_btn_event_cb,(void*)(uintptr_t)5);
	create_manul_btn(g_manul_page, 290, 30, "LEFT", drive_btn_event_cb,(void*)(uintptr_t)6);
	create_manul_btn(g_manul_page, 110, 30, "RIGHT", drive_btn_event_cb,(void*)(uintptr_t)7);

	create_manul_dri_btn(g_manul_page, -100, -200, "GRAP", drive_btn_event_cb,(void*)(uintptr_t)8);
	create_manul_dri_btn(g_manul_page, 100, -200, "LOOS", drive_btn_event_cb,(void*)(uintptr_t)9);

	create_back_btn(g_manul_page, -350, -220, "BACK", manul_btn_back_cb,(void*)(uintptr_t)0);
}
static void create_auto_page(void)
{
	g_auto_page = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(g_auto_page, lv_color_hex(HOME_BG_TOP), 0);
    lv_obj_set_style_bg_grad_color(g_auto_page, lv_color_hex(HOME_BG_BOTTOM), 0);
    lv_obj_set_style_bg_grad_dir(g_auto_page, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(g_auto_page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(g_auto_page, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *auto_bg = lv_img_create(g_auto_page);
    lv_img_set_src(auto_bg, &img_auto);
    lv_obj_set_pos(auto_bg, 0, 0);

    lv_obj_t *panel = lv_obj_create(g_auto_page);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 420, 300);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 45);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 跟踪点：一个红色圆点 */
    g_track_dot = lv_obj_create(g_auto_page);
    lv_obj_set_size(g_track_dot, 30, 30);                      // 圆点大小
    lv_obj_set_style_radius(g_track_dot, LV_RADIUS_CIRCLE, 0); // 圆形
    lv_obj_set_style_bg_color(g_track_dot, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(g_track_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_track_dot, 0, 0);
    lv_obj_clear_flag(g_track_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(g_track_dot, LV_ALIGN_TOP_LEFT, 0, 0);        /* 用左上角作基准，set_pos 才是绝对坐标 */

    g_task2_running_label = lv_label_create(g_auto_page);
    lv_label_set_text(g_task2_running_label, "RUNNING...");
    lv_obj_set_style_text_font(g_task2_running_label,
                               &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_task2_running_label,
                                lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(g_task2_running_label,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(g_task2_running_label,
                              lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(g_task2_running_label, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_task2_running_label, 8, 0);
    lv_obj_set_style_pad_all(g_task2_running_label, 12, 0);
    lv_obj_align(g_task2_running_label, LV_ALIGN_CENTER, 180, 117);
    lv_obj_add_flag(g_task2_running_label, LV_OBJ_FLAG_HIDDEN);

    g_task_status = lv_label_create(g_auto_page);
    lv_label_set_text(g_task_status, "auto processing...");
    lv_obj_set_style_text_font(g_task_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_task_status, lv_color_hex(0x90A4AE), 0);
    lv_obj_align(g_task_status, LV_ALIGN_BOTTOM_MID, 0, -18);

    create_back_btn(g_auto_page, -350, -220, "BACK", manul_btn_back_cb,(void*)(uintptr_t)0);
    create_temp_bar (g_auto_page);

    create_auto_task_menu(g_auto_page);

    /* 左上角坐标标签。向下偏移，避让 BACK 按钮。 */
    g_coord_label = lv_label_create(g_auto_page);
    lv_label_set_text(g_coord_label, "X: ---  Y: ---");
    lv_obj_set_style_text_font(g_coord_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_coord_label, lv_color_hex(0x00FFFF), 0);
    lv_obj_align(g_coord_label, LV_ALIGN_TOP_LEFT, 10, 56);

    /* 任务栏创建得比红点晚，必须把红点和坐标重新提到最上层。 */
    lv_obj_move_foreground(g_track_dot);
    lv_obj_move_foreground(g_task2_running_label);
    lv_obj_move_foreground(g_coord_label);
}

/**********************
 *     外部接口
 **********************/
/* 每 30ms 调用一次采样回调（读 AXI GPIO / 串口转发/打印都在里面） */
static void sample_timer_cb(lv_timer_t *timer)
{
    gesture_info_t info;
    (void)timer;

    if (!g_splash_done) {
        return;                     /* 启动页动画期间不占用串口，动画不受影响 */
    }

    if (s_sample_cb != NULL) {
        s_sample_cb(&info);         /* 读 AXI GPIO + UART3 协议转发 */
    }
    if (g_current_page == PAGE_HOME) {
    	xil_printf("[SLIDER] value = %d\r\n", (int)g_slider_value);
    }


    else if (g_current_page == PAGE_MANUL) {

        for (int i = 0; i < 10; i++) {
            if (g_drive[i] != 0) {
                xil_printf("[MANUL] drive[%d] = 1\r\n", i);
            }
        }
    }
    else if (g_current_page == PAGE_AUTO) {
        int task1 = (g_auto_selected_task == 0);
        int task2 = (g_auto_selected_task == 1);
        int task3 = (g_auto_selected_task == 2);
    	xil_printf("【手势识别】 任务: [TASK1:%d, TASK2:%d, TASK3:%d] | 状态: %s | 手指: %d | 坐标: (X: %d, Y: %d) | 深度: %d\r\n",
    	                   task1, task2, task3,
    	                   info.is_fist ? "抓取" : "松开",
    	                   info.finger_num, info.center_x, info.center_y, info.depth);

        if (g_task2_running_label != NULL) {
            if (g_auto_selected_task == 1 && !info.hand_detected) {
                lv_obj_clear_flag(g_task2_running_label, LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(g_task2_running_label, LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (g_track_dot != NULL) {
        	if (info.finger_num != 0)
        	{
            lv_obj_clear_flag(g_track_dot, LV_OBJ_FLAG_HIDDEN);   /* 检测到手 → 重新显示 */
            /* 摄像头 640x480 → LCD 右下角跟踪区 */
            int32_t cam_x = info.center_x;
            int32_t cam_y = info.center_y;

            /* 边界保护：摄像头坐标限制在 0~639 / 0~479 */
            if (cam_x < 0) cam_x = 0;
            if (cam_x > 639) cam_x = 639;
            if (cam_y < 0) cam_y = 0;
            if (cam_y > 479) cam_y = 479;

            /* 只压缩映射范围，不改变手部数据判定 */
            lv_coord_t screen_x = (lv_coord_t)(
                    AUTO_DOT_X0 +
                    cam_x * (AUTO_DOT_X1 - AUTO_DOT_X0) / 639);
            lv_coord_t screen_y = (lv_coord_t)(
                    AUTO_DOT_Y0 +
                    cam_y * (AUTO_DOT_Y1 - AUTO_DOT_Y0) / 479);

            /* 圆点中心对准手势坐标（减去半径） */
            lv_obj_set_pos(g_track_dot, screen_x - 15, screen_y - 15);
            if (g_coord_label != NULL) {
                lv_label_set_text_fmt(g_coord_label, "X:%3d  Y:%3d",
                                      (int)screen_x, (int)screen_y);
            }
        	}
        	else{
        		 lv_obj_add_flag(g_track_dot, LV_OBJ_FLAG_HIDDEN);
        		 if (g_coord_label != NULL) {
        		     lv_label_set_text(g_coord_label, "X: ---  Y: ---");
        		 }
        	}
        }
    }
}

void gesture_ui_init(gesture_sample_fn sample_cb)
{
    s_sample_cb = sample_cb;

    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

#if GESTURE_UI_USE_WIDGETS_DEMO
    lv_demo_widgets();
#else
    create_home_page();
    create_manul_page();
    create_auto_page();
    create_splash_screen();

    lv_timer_t *splash_timer = lv_timer_create(splash_timer_cb, SPLASH_HOLD_MS, NULL);
    lv_timer_set_repeat_count(splash_timer, 1);
    lv_timer_create(sample_timer_cb, 20, NULL);
    /* 先把第一帧真正画进帧缓存，再点亮背光 */
    lv_refr_now(NULL);
    usleep(200000);
    backlight_on();
#endif
}
void gesture_ui_run(void)
{
    while (1) {
        lv_tick_inc(5);
        lv_timer_handler();
        usleep(5000);
    }
}
