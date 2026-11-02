# 基于 Zynq-7020 的手势识别物流搬运系统

本仓库整理了 Zynq-7020 + OV7725 + HDMI/LCD 的软硬件工程，包括 PL 图像与手势识别、PS 端 VDMA/LVGL 应用、Vitis 外设驱动、STM32 运动控制程序，以及 Vivado 实现报告。

## 1. 系统组成

- 主控：Xilinx Zynq-7020，器件型号 `xc7z020clg400-2`
- 视觉输入：OV7725 摄像头
- 实时监控：HDMI 输出
- 本地界面：4.3 英寸 800×480 RGB LCD，GT911 电容触摸
- 运动控制：STM32F103C8T6、步进电机、PCA9685 与夹爪舵机
- 开发环境：Vivado 2020.2、Vitis 2020.2、Keil MDK

系统数据通路如下：

```text
OV7725
  -> PL 采集与 AXI4-Stream 格式转换
  -> 帧差、肤色分割、手指/握拳识别、中心与深度计算
  -> AXI GPIO / VDMA
  -> PS 读取手势结果并运行 LVGL 状态机
  -> HDMI 实时画面 + LCD 交互界面
  -> UART
  -> STM32 任务状态机
  -> 步进电机、夹爪舵机
```

## 2. 代码目录

| 目录 | 内容说明 |
|---|---|
| `src/fpga/` | Zynq PL 侧设计，包括手势识别 RTL、板级约束、Block Design 和自建 IP。 |
| `src/ps/app/` | Vitis PS 应用，包括系统初始化、VDMA、OV7725、GT911 触摸、LVGL 界面和手势数据读取。 |
| `src/stm32/` | STM32F103C8T6 运动控制代码、HAL/CMSIS 驱动、电机与舵机驱动，以及 MDK/IAR 工程。 |
| `sim/` | PL 输出数据的手势识别仿真说明和仿真结果截图。 |
| `build/` | Vivado、Vitis 和 STM32 构建脚本、XSA、综合实现报告。 |
| `skill/` | 从项目调试过程整理出的 Codex 技能包，包含视频链路、触摸、手势算法和通信排查资料。 |
| `report/` | 设计报告、原始 Word 文档和大模型协作记录。 |

```text
/
├── readme.md
├── .gitignore
├── src/
│   ├── fpga/
│   │   ├── rtl/
│   │   │   ├── axi_vip_frame_difference.v
│   │   │   ├── rgb888_ycbcr444.v
│   │   │   ├── key.v
│   │   │   └── line_buffer.v
│   │   ├── constraints/
│   │   │   ├── board.xdc
│   │   │   └── debug_hub_clk.xdc
│   │   ├── bd/
│   │   │   ├── system.bd
│   │   │   └── recreate_system_bd.tcl
│   │   └── ip_repo/
│   │       ├── dvi_tx/
│   │       └── ov5640_capture_data/
│   ├── ps/
│   │   └── app/
│   │       ├── main.c
│   │       ├── gesture_ui.c
│   │       ├── lv_port_disp.c
│   │       ├── lv_port_indev.c
│   │       ├── display_ctrl/
│   │       ├── emio_sccb_cfg/
│   │       ├── ov7725/
│   │       ├── vdma_api/
│   │       ├── demos/
│   │       └── src/
│   └── stm32/
│       ├── core/
│       ├── drivers/
│       ├── tkkc_drivers/
│       ├── mdk_arm/
│       └── ewarm/
├── sim/
│   ├── readme.md
│   └── simulation.png
├── build/
│   ├── vivado/
│   ├── vitis/
│   ├── stm32/
│   ├── hardware/
│   ├── reports/
│   └── validate_names.ps1
├── skill/
│   ├── readme.md
│   └── zynq-video-gesture-debug/
│       ├── skill.md
│       ├── agents/
│       ├── references/
│       └── scripts/
└── report/
    ├── design_report.md
    ├── design_report_source.docx
    └── llm_collaboration_log.md
```

其中 `sim/` 当前用于保存 PL 两路 GPIO 调试数据、手指数量和最大连通域的仿真说明及结果截图。

## 3. 关键源码

- PL 手势识别顶层：`src/fpga/rtl/axi_vip_frame_difference.v`
- RGB 到 YCbCr 转换：`src/fpga/rtl/rgb888_ycbcr444.v`
- 按键消抖/处理：`src/fpga/rtl/key.v`
- 行缓存：`src/fpga/rtl/line_buffer.v`
- 板级约束：`src/fpga/constraints/board.xdc`
- Block Design 重建：`src/fpga/bd/recreate_system_bd.tcl`
- PS 主程序：`src/ps/app/main.c`
- LVGL 页面与手势 UI：`src/ps/app/gesture_ui.c`
- 触摸输入：`src/ps/app/lv_port_indev.c`
- 显示移植：`src/ps/app/lv_port_disp.c`
- STM32 主程序：`src/stm32/core/src/app.c`
- FPGA 串口协议：`src/stm32/tkkc_drivers/fpga_gesture.c`
- 任务状态机：`src/stm32/tkkc_drivers/task1.c`、`task2.c`、`task3.c`、`task4.c`
- 仿真说明与结果：`sim/readme.md`、`sim/simulation.png`

## 4. Vivado 复现

前置条件：

- Vivado 2020.2
- 已安装 Zynq-7000 器件支持

在 Vivado Tcl Console 中执行：

```tcl
cd <仓库根目录>
source build/vivado/create_project.tcl
```

脚本会：

1. 创建 `xc7z020clg400-2` 工程；
2. 注册 `src/fpga/ip_repo`；
3. 添加自定义 RTL 和 XDC；
4. 通过 `recreate_system_bd.tcl` 重建 `system` Block Design；
5. 生成 `system_wrapper` 顶层。

重建完成后可生成比特流与 XSA：

```tcl
source build/vivado/build.tcl
```

注意：自定义 IP 在 Vivado IP Catalog 中的 VLNV 保持不变，以免 Block Design 无法解析已有实例。

## 5. Vitis 复现

前置条件：

- Vitis 2020.2
- 已有 `build/hardware/system_wrapper.xsa`，或由 Vivado 重新导出 XSA

在 Vitis 2020.2 的 XSCT Console 中执行：

```tcl
cd <仓库根目录>
source build/vitis/build_app.tcl
```

脚本会基于 XSA 创建 `zynq7020_platform` 和 `gesture_app`，导入 `src/ps/app` 后编译。

烧录顺序：

1. 下载 PL 比特流；
2. 初始化 PS；
3. 下载并运行 `gesture_app.elf`；
4. 打开 115200 8N1 串口查看运行日志。

## 6. STM32 构建

使用 Keil MDK 打开：

```text
src/stm32/mdk_arm/22software_5.uvprojx
```

目标器件为 STM32F103C8T6，预定义宏：

```text
USE_FULL_LL_DRIVER
USE_HAL_DRIVER
STM32F103xB
```

也可使用 IAR 工程：

```text
src/stm32/ewarm/22software_5.ewp
```

详细说明见 `build/stm32/readme.md`。

## 7. 当前实现状态

已纳入仓库的最近一次实现报告显示：

- 时序约束全部满足；
- Slice LUT 使用约 17.69%；
- Slice Register 使用约 15.48%；
- Block RAM Tile 使用约 13.93%；
- DSP 使用 0%。

实现快照位于 `build/reports/`。这些报告对应仓库整理前的最近一次成功实现，重新修改 RTL 或 Block Design 后必须重新生成。

## 8. 已知注意事项

- 摄像头、LCD、HDMI 和触摸的引脚约束依赖当前底板，移植到其他板卡前必须重新核对 XDC。
- PL 与 PS 的 AXIS 时钟频率和时钟域必须保持一致；修改 VDMA 或视频路径后应重新执行设计规则检查。
- 触摸使用 EMIO GPIO 模拟 I2C，GT911 的地磁配置与坐标方向需要按实际屏幕安装方向校准。
- 图片数组较大，编译时间主要受 LVGL 图片资源和优化等级影响。
