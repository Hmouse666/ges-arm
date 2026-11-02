/*
 * lv_port_indev.c
 *
 * LVGL touch input for the Zynq-7020 board with a GT911 capacitive panel.
 *
 * Wiring (800x480 RGB LCD, 4.3 inch):
 *      touch_scl   R19 -> EMIO GPIO bit2 -> XGpioPs pin 56
 *      touch_sda   P20 -> EMIO GPIO bit3 -> XGpioPs pin 57
 *      touch_rst_n M19 -> EMIO GPIO bit4 -> XGpioPs pin 58
 *      touch_int   U19 -> EMIO GPIO bit5 -> XGpioPs pin 59
 *
 * The GT911 I2C address is selected by the INT level while RESET is released.
 * Driving INT low selects 0x5D (8-bit 0xBA/0xBB), which is what this panel
 * uses. No configuration download is needed, the chip uses its own config.
 */

#include "lv_port_indev.h"
#include "lvgl.h"

#include "xgpiops.h"
#include "xparameters.h"
#include "sleep.h"
#include "xil_printf.h"
#include "xil_types.h"

/*********************
 *      DEFINES
 *********************/

#define TP_GPIOPS_ID          XPAR_XGPIOPS_0_DEVICE_ID

#define TP_SCL_PIN            56
#define TP_SDA_PIN            57
#define TP_RST_PIN            58
#define TP_INT_PIN            59

#define TP_SCREEN_W           800
#define TP_SCREEN_H           480

/* panel -> screen mapping */
#define TP_MIRROR_X           0
#define TP_MIRROR_Y           0     /* 鏄剧ず閫氳矾娌℃湁缈昏浆锛岃Е鎽镐篃涓嶈闀滃儚 */

/* 瑙︽懜灞忓師濮嬪潗鏍囪寖鍥达紙鐢ㄤ袱涓鏍囧畾锛氬乏涓� / 鍙充笅锛� */
#define TP_RAW_X_MIN          0
#define TP_RAW_X_MAX          799
#define TP_RAW_Y_MIN          0
#define TP_RAW_Y_MAX          479

/* 1 = 鎸変笅鏃跺湪涓插彛鎵撳嵃鍘熷/鏄犲皠鍚庣殑鍧愭爣锛堟牎鍑嗙敤锛� */
#define TP_DEBUG              0

/* I2C address of the GT911 (8-bit write address) */
#define GT_ADDR_W             0xBA

/* GT911 registers */
#define GT_PID_REG            0x8140
#define GT_GSTID_REG          0x814E
#define GT_TP1_REG            0x8150

/**********************
 *  STATIC VARIABLES
 **********************/

static XGpioPs     tp_gpio;
static int         tp_ready    = 0;
static lv_coord_t  tp_x        = 0;
static lv_coord_t  tp_y        = 0;

/**********************
 *  LOW LEVEL I2C
 **********************/

static void tp_delay(void)
{
    usleep(2);
}

static void tp_scl(int v)
{
    XGpioPs_WritePin(&tp_gpio, TP_SCL_PIN, v ? 1 : 0);
}

static void tp_sda(int v)
{
    XGpioPs_WritePin(&tp_gpio, TP_SDA_PIN, v ? 1 : 0);
}

static void tp_sda_dir(int out)
{
    XGpioPs_SetDirectionPin(&tp_gpio, TP_SDA_PIN, out ? 1 : 0);
    XGpioPs_SetOutputEnablePin(&tp_gpio, TP_SDA_PIN, out ? 1 : 0);
}

static int tp_sda_read(void)
{
    return (int)XGpioPs_ReadPin(&tp_gpio, TP_SDA_PIN);
}

static void tp_rst(int v)
{
    XGpioPs_WritePin(&tp_gpio, TP_RST_PIN, v ? 1 : 0);
}

static void tp_int_dir(int out)
{
    XGpioPs_SetDirectionPin(&tp_gpio, TP_INT_PIN, out ? 1 : 0);
    XGpioPs_SetOutputEnablePin(&tp_gpio, TP_INT_PIN, out ? 1 : 0);
}

static void tp_int_out(int v)
{
    XGpioPs_WritePin(&tp_gpio, TP_INT_PIN, v ? 1 : 0);
}

static void tp_i2c_start(void)
{
    tp_sda_dir(1);
    tp_sda(1);
    tp_scl(1);
    tp_delay();
    tp_sda(0);
    tp_delay();
    tp_scl(0);
    tp_delay();
}

static void tp_i2c_stop(void)
{
    tp_sda_dir(1);
    tp_scl(0);
    tp_sda(0);
    tp_delay();
    tp_scl(1);
    tp_delay();
    tp_sda(1);
    tp_delay();
}

static u8 tp_i2c_wait_ack(void)
{
    u8 err = 0;

    tp_sda_dir(0);
    tp_sda(1);
    tp_delay();
    tp_scl(1);
    tp_delay();

    while (tp_sda_read()) {
        if (++err > 250) {
            tp_i2c_stop();
            return 1;
        }
    }

    tp_scl(0);
    tp_delay();
    return 0;
}

static void tp_i2c_send(u8 d)
{
    u8 i;

    tp_sda_dir(1);
    tp_scl(0);

    for (i = 0; i < 8; i++) {
        tp_sda((d & 0x80) >> 7);
        d <<= 1;
        tp_delay();
        tp_scl(1);
        tp_delay();
        tp_scl(0);
        tp_delay();
    }
}

static u8 tp_i2c_read(u8 ack)
{
    u8 i;
    u8 d = 0;

    tp_sda_dir(0);

    for (i = 0; i < 8; i++) {
        tp_scl(0);
        tp_delay();
        tp_scl(1);
        d <<= 1;
        if (tp_sda_read()) {
            d |= 0x01;
        }
        tp_delay();
    }

    tp_sda_dir(1);
    tp_scl(0);
    tp_sda(ack ? 0 : 1);
    tp_delay();
    tp_scl(1);
    tp_delay();
    tp_scl(0);
    tp_delay();

    return d;
}

/**********************
 *  GT911 ACCESS
 **********************/

static void gt_write(u16 reg, u8 *buf, u8 len)
{
    u8 i;

    tp_i2c_start();
    tp_i2c_send(GT_ADDR_W);
    tp_i2c_wait_ack();
    tp_i2c_send((u8)(reg >> 8));
    tp_i2c_wait_ack();
    tp_i2c_send((u8)(reg & 0xFF));
    tp_i2c_wait_ack();

    for (i = 0; i < len; i++) {
        tp_i2c_send(buf[i]);
        if (tp_i2c_wait_ack()) {
            break;
        }
    }
    tp_i2c_stop();
}

static void gt_read(u16 reg, u8 *buf, u8 len)
{
    u8 i;

    tp_i2c_start();
    tp_i2c_send(GT_ADDR_W);
    tp_i2c_wait_ack();
    tp_i2c_send((u8)(reg >> 8));
    tp_i2c_wait_ack();
    tp_i2c_send((u8)(reg & 0xFF));
    tp_i2c_wait_ack();
    tp_i2c_start();
    tp_i2c_send(GT_ADDR_W | 1);
    tp_i2c_wait_ack();

    for (i = 0; i < len; i++) {
        buf[i] = tp_i2c_read((i == (len - 1)) ? 0 : 1);
    }
    tp_i2c_stop();
}

/**********************
 *  TOUCH DRIVER
 **********************/

static void tp_gpio_init(void)
{
    XGpioPs_Config *cfg;

    cfg = XGpioPs_LookupConfig(TP_GPIOPS_ID);
    XGpioPs_CfgInitialize(&tp_gpio, cfg, cfg->BaseAddr);

    XGpioPs_SetDirectionPin(&tp_gpio, TP_SCL_PIN, 1);
    XGpioPs_SetOutputEnablePin(&tp_gpio, TP_SCL_PIN, 1);
    XGpioPs_SetDirectionPin(&tp_gpio, TP_RST_PIN, 1);
    XGpioPs_SetOutputEnablePin(&tp_gpio, TP_RST_PIN, 1);
    tp_int_dir(0);

    tp_sda_dir(1);
    tp_scl(1);
    tp_sda(1);
    tp_rst(1);
}

/* Reset the panel; INT low during reset selects I2C address 0x5D */
static int tp_probe(void)
{
    u8 buf[5];

    tp_int_dir(1);
    tp_int_out(0);
    tp_rst(0);
    usleep(200);
    tp_rst(1);
    usleep(55000);
    tp_int_dir(0);
    usleep(20000);

    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;
    gt_read(GT_PID_REG, buf, 4);
    buf[4] = 0;

    xil_printf("[TP] GT911 PID = %s\r\n", buf);

    if ((buf[0] == '9') && (buf[1] == '1') && (buf[2] == '1')) {
        return 0;
    }
    return 1;
}

static void tp_map(u16 raw_x, u16 raw_y, lv_coord_t *x, lv_coord_t *y)
{
    lv_coord_t sx = (lv_coord_t)raw_x;
    lv_coord_t sy = (lv_coord_t)raw_y;

#if TP_MIRROR_X
    sx = TP_RAW_X_MAX + TP_RAW_X_MIN - sx;
#endif
#if TP_MIRROR_Y
    sy = TP_RAW_Y_MAX + TP_RAW_Y_MIN - sy;
#endif

    /* 绾挎�ф槧灏勫埌灞忓箷鑼冨洿 */
    sx = (sx - TP_RAW_X_MIN) * (TP_SCREEN_W - 1) / (TP_RAW_X_MAX - TP_RAW_X_MIN);
    sy = (sy - TP_RAW_Y_MIN) * (TP_SCREEN_H - 1) / (TP_RAW_Y_MAX - TP_RAW_Y_MIN);

    if (sx < 0) {
        sx = 0;
    }
    if (sy < 0) {
        sy = 0;
    }
    if (sx > TP_SCREEN_W - 1) {
        sx = TP_SCREEN_W - 1;
    }
    if (sy > TP_SCREEN_H - 1) {
        sy = TP_SCREEN_H - 1;
    }

    *x = sx;
    *y = sy;
}

/* returns 1 while a finger is down */
static int tp_scan(void)
{
    u8  st = 0;
    u8  p[4];
    u8  zero = 0;

    if (!tp_ready) {
        return 0;
    }

    gt_read(GT_GSTID_REG, &st, 1);

    if ((st & 0x80) && (st & 0x0F)) {
        gt_read(GT_TP1_REG, p, 4);
        {
            u16 rx = (u16)(((u16)p[1] << 8) | p[0]);
            u16 ry = (u16)(((u16)p[3] << 8) | p[2]);
            tp_map(rx, ry, &tp_x, &tp_y);
#if TP_DEBUG
            {
                static u32 dbg = 0;
                if ((dbg++ % 10u) == 0u) {
                    xil_printf("[TP] raw(%u,%u) -> (%d,%d)\r\n",
                               (unsigned)rx, (unsigned)ry,
                               (int)tp_x, (int)tp_y);
                }
            }
#endif
        }
        gt_write(GT_GSTID_REG, &zero, 1);
        return 1;
    }

    if (st & 0x80) {
        gt_write(GT_GSTID_REG, &zero, 1);
    }
    return 0;
}

static void tp_init(void)
{
    tp_gpio_init();
    tp_ready = (tp_probe() == 0) ? 1 : 0;
    xil_printf("[TP] %s\r\n", tp_ready ? "GT911 ready" : "GT911 not found");
}

/**********************
 *   LVGL INTERFACE
 **********************/

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    int pressed;

    (void)indev_drv;

    pressed = tp_scan();

    data->point.x = tp_x;
    data->point.y = tp_y;
    data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

void lv_touch_get_last(lv_coord_t *x, lv_coord_t *y)
{
    if (x != NULL) {
        *x = tp_x;
    }
    if (y != NULL) {
        *y = tp_y;
    }
}

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    tp_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}
