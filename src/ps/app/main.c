#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xparameters.h"
#include "xaxivdma.h"
#include "xaxivdma_i.h"
#include "xgpio.h"
#include "xgpio_l.h"
#include "display_ctrl/display_ctrl.h"
#include "vdma_api/vdma_api.h"
#include "emio_sccb_cfg/emio_sccb_cfg.h"
#include "ov7725/ov7725_init.h"
#include "sleep.h"

// 手势界面: LVGL 引擎、界面构建与刷新都在 gesture_ui.c/h 里, main 只管硬件驱动
#include "gesture_ui.h"
#include "lv_port_indev.h"
// ==================== 硬件器件 ID 宏定义 ====================
#define VDMA_HDMI_ID       XPAR_AXIVDMA_0_DEVICE_ID   /* 摄像头 + HDMI 视频流通道 */
#define VDMA_LCD_ID        XPAR_AXIVDMA_1_DEVICE_ID   /* LCD 交互 UI 通道 */
#define VTC_HDMI_ID        XPAR_VTC_0_DEVICE_ID
#define VTC_LCD_ID         XPAR_VTC_1_DEVICE_ID

#ifndef XPAR_AXIVDMA_1_DEVICE_ID
#error "BSP has no XPAR_AXIVDMA_1_DEVICE_ID: update the platform with the new ges XSA and rebuild BSP"
#endif
#ifndef XPAR_VTC_1_DEVICE_ID
#error "BSP has no XPAR_VTC_1_DEVICE_ID: update the platform with the new ges XSA and rebuild BSP"
#endif

// 手势识别使用的 AXI GPIO：
//   axi_gpio_0 = 手势/深度数据输入（两路）
//   axi_gpio_1 = 背光使能输出（1 位，不要拿它读手势数据）
#define GESTURE_GPIO_ID    XPAR_AXI_GPIO_0_DEVICE_ID
#define GESTURE_GPIO_BASE  XPAR_AXI_GPIO_0_BASEADDR

// 显存地址分配：防踩踏硬件隔离
#define HDMI_FB_ADDR       (XPAR_PS7_DDR_0_S_AXI_BASEADDR + 0x01000000)  /* 0x01100000: 摄像头/HDMI 显存 */
#define LVGL_FB_ADDR       (XPAR_PS7_DDR_0_S_AXI_BASEADDR + 0x02000000)  /* 0x02100000: LVGL/LCD 显存 */
#define LCD_BYTES_PIXEL    4     /* LCD 通道为 32bit ARGB8888 */
#define VDMA0_BASE         XPAR_AXI_VDMA_0_BASEADDR
#define VDMA1_BASE         XPAR_AXI_VDMA_1_BASEADDR
#define MM2S_VDMACR_OFFSET 0x00u
#define MM2S_VDMASR_OFFSET 0x04u
#define MM2S_PARKPTR_OFFSET 0x28u
#define FRAME_BUFFER_CLEAR_BYTES (5u * 1024u * 1024u)

// ==================== 全局外设句柄 ====================
XAxiVdma     vdma_hdmi;       /* 摄像头 -> DDR -> HDMI */
XAxiVdma     vdma_lcd;        /* LVGL 显存 -> LCD */
DisplayCtrl  dispCtrl;        /* v_tc_0 : HDMI 时序 */
DisplayCtrl  dispCtrl_lcd;    /* v_tc_1 : LCD 时序  */
VideoMode    vd_mode_hdmi;    /* 640x480 */
VideoMode    vd_mode_lcd;     /* 800x480 */
XGpio        gesture_gpio;

// ==================== 显示通道诊断 ====================
static void dump_display_status(void)
{
    u32 v;

    v = XAxiVdma_ReadReg(VDMA0_BASE, MM2S_VDMACR_OFFSET);
    xil_printf("[DBG] VDMA0 CR=0x%08x", v);
    v = XAxiVdma_ReadReg(VDMA0_BASE, MM2S_VDMASR_OFFSET);
    xil_printf(" SR=0x%08x\r\n", v);

    v = XAxiVdma_ReadReg(VDMA1_BASE, MM2S_VDMACR_OFFSET);
    xil_printf("[DBG] VDMA1 CR=0x%08x", v);
    v = XAxiVdma_ReadReg(VDMA1_BASE, MM2S_VDMASR_OFFSET);
    xil_printf(" SR=0x%08x\r\n", v);

    v = XAxiVdma_ReadReg(VDMA1_BASE, MM2S_PARKPTR_OFFSET);
    xil_printf("[DBG] VDMA1 MaxNumFrames=%d PARK=0x%08x (readFrame=%d)\r\n",
               vdma_lcd.MaxNumFrames, v, (v >> 16) & 0x1Fu);

    v = Xil_In32(XPAR_V_TC_1_BASEADDR + 0x00u);
    xil_printf("[DBG] VTC1 CTL=0x%08x", v);
    v = Xil_In32(XPAR_V_TC_1_BASEADDR + 0x04u);
    xil_printf(" ISR=0x%08x\r\n", v);

    v = *(volatile u32 *)(UINTPTR)(LVGL_FB_ADDR + (240u * 800u + 400u) * 4u);
    xil_printf("[DBG] LVGL FB center=0x%08x\r\n", v);
}

// ==================== 手势采样回调 ====================
// 由 gesture_ui 每 30ms 回调一次: 读 AXI GPIO、解包手势数据、状态清零并转发给 STM32
static void gesture_sample(gesture_info_t *info)
{
    static u32 dbg_count = 0;
    u32 raw_data, raw_depth_data;

    // 1. 读取手势数据 (通道 1) 与深度数据 (通道 2)
    raw_data       = XGpio_DiscreteRead(&gesture_gpio, 1);
    raw_depth_data = XGpio_ReadReg(GESTURE_GPIO_BASE, XGPIO_DATA2_OFFSET);

    // 2. 解包手势信息
    int raw_center_x   = raw_data & 0x7FF;
    int raw_center_y   = (raw_data >> 11) & 0x7FF;
    int raw_finger_num = (raw_data >> 22) & 0x0F;
    int raw_is_fist    = (raw_data >> 26) & 0x01;
    info->mode1        = (raw_data >> 27) & 0x01;
    info->mode2        = (raw_data >> 28) & 0x01;
    int raw_depth      = raw_depth_data & 0x7FF;

    // 3. 判定是否检测到手（进入视野）
    // 关键修正：坚决不使用 (center_x > 15) 判断，手部存在必须依赖特征识别结果
    if (raw_is_fist == 1 || raw_finger_num > 0) {
        // === 检测到有效手势 ===
        info->hand_detected = 1;
        info->center_x      = raw_center_x;
        info->center_y      = raw_center_y;
        info->finger_num    = raw_finger_num;
        info->is_fist       = raw_is_fist;
        info->depth         = raw_depth;
    } else {
        // === 手离开视野：坐标与特征全部强制清零 ===
        info->hand_detected = 0;
        info->center_x      = 0;
        info->center_y      = 0;
        info->finger_num    = 0;
        info->is_fist       = 0;
        info->depth         = 0;
    }



}




// ==================== 主函数 ====================
int main(void)
{
    u32 status;
    u8  cam_ok;
    u32 cr;


    xil_printf("\r\n===== Gesture Recognition System with Dual-Display =====\r\n");

    // 1. 初始化 AXI GPIO (手势与深度数据采集)
    status = XGpio_Initialize(&gesture_gpio, GESTURE_GPIO_ID);
    if (status != XST_SUCCESS) {
        xil_printf("Gesture GPIO Init FAILED!\r\n");
    }
    XGpio_SetDataDirection(&gesture_gpio, 1, 0xFFFFFFFF);
    XGpio_WriteReg(GESTURE_GPIO_BASE, XGPIO_TRI2_OFFSET, 0xFFFFFFFF);

    // 2. UART 已在最前面初始化（UART0 同时用于调试和 STM32 协议）

    // 2.5 先把 LCD 通道跑起来并清成黑屏。
    // PL 侧背光在约 3.5 秒后自动点亮，如果那时 LCD 还没收到有效像素，
    // 面板默认就是白屏。所以这里尽早启动 VDMA1 + VTC1。
    vd_mode_lcd = VMODE_800x480;
    memset((void *)(UINTPTR)LVGL_FB_ADDR, 0, FRAME_BUFFER_CLEAR_BYTES);
    Xil_DCacheFlush();

    status = run_vdma_frame_buffer(&vdma_lcd, VDMA_LCD_ID,
                                   vd_mode_lcd.width, vd_mode_lcd.height,
                                   LVGL_FB_ADDR, 0, 0, ONLY_READ);
    xil_printf("VDMA1(LCD) early start status=%d, FB=0x%08x\r\n",
               status, (u32)LVGL_FB_ADDR);

    status = DisplayInitialize(&dispCtrl_lcd, VTC_LCD_ID);
    status = DisplaySetMode(&dispCtrl_lcd, &vd_mode_lcd);
    status = DisplayStart(&dispCtrl_lcd);
    xil_printf("LCD display (VTC1) early start, status=%d\r\n", status);

    // 3. 启动 OV7725 摄像头（提供视频时钟源与 Genlock 同步主信号）
    emio_init();
    cam_ok = ov7725_init();
    xil_printf("OV7725 init: %s\r\n", (cam_ok == 0) ? "ok" : "FAILED");

    // 4. 配置显示分辨率模式
    vd_mode_hdmi = VMODE_640x480;
    vd_mode_lcd  = VMODE_800x480;

    // 4.1 在启动 VDMA/VTC 之前先把显存清成全黑，避免上电首先显示未初始化 DDR
    memset((void *)(UINTPTR)HDMI_FB_ADDR, 0, FRAME_BUFFER_CLEAR_BYTES);
    memset((void *)(UINTPTR)LVGL_FB_ADDR, 0, FRAME_BUFFER_CLEAR_BYTES);
    Xil_DCacheFlush();

    // 5. 启动 HDMI 通道 (VDMA0 双通道模式, 摄像头写入 DDR, MM2S 读 DDR 送 HDMI)
    status = run_vdma_frame_buffer(&vdma_hdmi, VDMA_HDMI_ID,
                                   vd_mode_hdmi.width, vd_mode_hdmi.height,
                                   HDMI_FB_ADDR, 0, 0, BOTH);
    xil_printf("VDMA0(cam/HDMI) status=%d, FB=0x%08x\r\n", status, (u32)HDMI_FB_ADDR);

    // 6. LCD 通道已在 2.5 步提前启动

    // 6.1 打印两个 VDMA 的 Genlock 配置与运行状态
    cr = XAxiVdma_ReadReg(VDMA0_BASE, MM2S_VDMACR_OFFSET);
    xil_printf("VDMA0 MM2S VDMACR=0x%08x, GenlockEn=%d\r\n", cr, (cr >> 3) & 1u);
    cr = XAxiVdma_ReadReg(VDMA0_BASE, MM2S_VDMASR_OFFSET);
    xil_printf("VDMA0 MM2S VDMASR=0x%08x, halted=%d, SoFState=%d\r\n",
               cr, cr & 1u, (cr >> 12) & 0x0F);
    cr = XAxiVdma_ReadReg(VDMA1_BASE, MM2S_VDMACR_OFFSET);
    xil_printf("VDMA1 MM2S VDMACR=0x%08x, GenlockEn=%d\r\n", cr, (cr >> 3) & 1u);
    cr = XAxiVdma_ReadReg(VDMA1_BASE, MM2S_VDMASR_OFFSET);
    xil_printf("VDMA1 MM2S VDMASR=0x%08x, halted=%d, SoFState=%d\r\n",
               cr, cr & 1u, (cr >> 12) & 0x0F);

    // 7. 清空 LVGL 显存 (32bit ARGB8888)
    memset((void *)(UINTPTR)LVGL_FB_ADDR, 0,
           vd_mode_lcd.height * vd_mode_lcd.width * LCD_BYTES_PIXEL);
    Xil_DCacheFlush();

    // 8. 启动双路显示控制器 (VTC0 为 HDMI 时序, VTC1 为 LCD 时序)
    status = DisplayInitialize(&dispCtrl, VTC_HDMI_ID);
    status = DisplaySetMode(&dispCtrl, &vd_mode_hdmi);
    status = DisplayStart(&dispCtrl);
    xil_printf("HDMI display (VTC0) started, status=%d\r\n", status);

    // LCD 显示控制器已在 2.5 步提前启动

    // 9. 初始化 LVGL 引擎与手势界面 (绑定采样回调)
    gesture_ui_init(gesture_sample);
    xil_printf("LVGL Engine & UI Ready.\r\n");

    // 10. 进入 UI 调度主循环
    gesture_ui_run();
    return 0;

}
