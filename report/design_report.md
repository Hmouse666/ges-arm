# 基于 Zynq-7020 的无接触智能物流搬运系统

## 摘要

本系统面向重载物流机械臂的非接触控制与安全接管场景，采用 Xilinx Zynq-7020 的 ARM Cortex-A9 与 FPGA 可编程逻辑异构架构。FPGA 侧完成 OV7725 图像采集、色彩空间转换、手部区域分割、手指与握拳状态识别、手势中心和深度计算；ARM 侧完成 VDMA、DDR 帧缓存、LVGL 界面、触摸交互、任务状态机和 UART 通信；STM32F103C8T6 接收控制指令并驱动步进电机与夹爪舵机。

系统提供自动搬运、手动控制和手势介入三种工作方式。手势接入后，操作者可在不接触机械臂的情况下控制末端位置与夹爪，并在手部撤离后回到安全状态。HDMI 通道用于实时监控原始画面与识别结果，800×480 RGB LCD 用于显示 LVGL 任务界面。

## 1. 系统总体设计

系统由四层组成：

1. **视觉感知层**：OV7725 将 8 位并口图像送入 PL。
2. **异构计算层**：PL 执行像素级流水线，PS 执行设备管理、状态机和界面逻辑。
3. **显示交互层**：HDMI 显示实时视频；LCD 显示任务按钮、手势状态和运行提示；GT911 提供触摸输入。
4. **运动执行层**：PS 通过 UART 将任务、手势状态、坐标和深度发送给 STM32，STM32 控制电机和夹爪。

```text
OV7725 -> 采集 IP -> AXI4-Stream -> 手势识别 RTL -> VDMA/DDR -> HDMI
                                |                 |
                                +-> AXI GPIO/ILA   +-> ARM/PS -> LCD/LVGL
                                                     |
                                                     +-> UART -> STM32 -> 电机/夹爪
```

## 2. PL 设计

### 2.1 图像采集与格式转换

`ov5640_capture_data` 自建 IP 保留原 VLNV，实际用于 OV7725 并口时序采集。输出经 `v_vid_in_axi4s` 转换为 AXI4-Stream，并通过 RGB 到 YCbCr 转换模块进行肤色相关处理。

### 2.2 手势识别

`AXI_VIP_Frame_Difference` 是当前 PL 手势算法的核心模块，主要包含：

- 帧同步和有效像素计数；
- RGB 到 YCbCr 的并行计算；
- YCbCr 肤色阈值分割；
- 手部区域包围框；
- 扫描线上的连续肤色块统计；
- 手指数量与指缝判别；
- 握拳/张开状态判断；
- 手势中心坐标和深度信息输出；
- LCD 背光握手与 PS 调试数据输出。

算法参数必须与实际镜头距离、光照、肤色和白平衡配置联合标定。手部离镜头较远时，固定像素阈值会造成误判，因此缝隙阈值和连通域宽度采用相对手部框宽度的尺度。

### 2.3 Block Design

主 Block Design 包含：

- Zynq Processing System 7；
- 摄像头采集 IP；
- Video In to AXI4-Stream；
- 帧差与手势识别模块；
- AXI VDMA 0：摄像头到 HDMI；
- AXI VDMA 1：DDR 到 LCD；
- AXI Interconnect、AXI GPIO、VTC；
- AXI4-Stream to Video Out；
- DVI Transmitter；
- System ILA 调试核。

两路 VDMA 使用独立帧缓存地址。修改视频格式、帧长或时钟后，需要重新检查 AXIS 的 `CLK_DOMAIN` 和 `FREQ_HZ` 属性。

## 3. PS 软件设计

PS 应用运行于 Standalone 模式，主要模块如下：

- `main.c`：时钟、GPIO、VDMA、显示、LVGL、触摸和 UART 初始化；
- `display_ctrl/`：VTC 显示控制器；
- `vdma_api/`：VDMA 配置与状态检查；
- `ov7725/`：摄像头寄存器初始化；
- `emio_sccb_cfg/`：EMIO GPIO 模拟 SCCB/I2C；
- `lv_port_disp.c`：LCD 显示移植；
- `lv_port_indev.c`：GT911 触摸移植；
- `gesture_ui.c`：主页、任务页、启动动画、状态提示和按钮事件；
- `demos/`：LVGL 图片、字体和页面资源。

软件采用非阻塞主循环。手势数据约 20 ms 更新一次，LVGL 定时处理；按键状态使用任务编号锁存，切换任务时清除上一任务状态。

## 4. STM32 运动控制

STM32F103C8T6 通过 UART 接收 Zynq 指令，协议字段包括：

- 当前任务编号；
- 手势状态；
- 手指数量；
- 手势中心横坐标 `x`；
- 手势中心纵坐标 `y`；
- 深度值；
- 夹爪抓取/松开状态。

主要软件模块：

- `fpga_gesture.c`：协议接收、校验和状态缓存；
- `task1.c` 至 `task4.c`：任务流程；
- `motor.c`：四路步进电机脉冲与方向；
- `claw_servo.c`、`pca9685.c`：夹爪角度控制；
- `tkkc_st7735.c`：本地状态显示；
- `key.c`、`led.c`：按键与状态指示。

## 5. 实现结果

 Vivado 实现报告显示：

| 指标 | 使用量 | 总量 | 占比 |
|---|---:|---:|---:|
| Slice LUTs | 9411 | 53200 | 17.69% |
| Slice Registers | 16471 | 106400 | 15.48% |
| Block RAM Tile | 19.5 | 140 | 13.93% |
| DSPs | 0 | 220 | 0.00% |



## 6. 复现与验证

1. 使用 Vivado 2020.2 运行 `build/vivado/create_project.tcl`；
2. 检查 Block Design 的时钟域、AXIS 位宽和地址映射；
3. 运行综合、实现并导出 XSA；
4. 使用 Vitis 2020.2 运行 `build/vitis/build_app.tcl`；
5. 下载比特流和 ELF；
6. 验证 OV7725 初始化、HDMI 图像、LCD 页面、GT911 触摸和 UART；
7. 修改 STM32 任务参数后，重新进行机械臂行程与夹爪标定。

仿真目录暂未纳入本仓库。后续加入仿真图片时，可新建 `sim/` 并补充测试平台、输入激励和结果截图。

## 7. 已知问题与改进方向

- 手势识别对光照和肤色较敏感，可增加白平衡、曝光和自适应阈值；
- 深度值来自二维图像特征，不等同于真实距离，正式应用建议增加 TOF 或双目测距；
- GT911 坐标映射与屏幕镜像相关，更换屏幕方向后必须重新标定；
- 当前机械臂控制以任务状态机为主，闭环反馈仍可进一步增强；
- 大尺寸 LVGL 图片会显著增加 Flash 和编译压力，可转换为外部资源或压缩格式；
- 后续应补充自动化仿真、回归测试和实机测试记录。

