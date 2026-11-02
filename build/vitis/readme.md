# Vitis 构建说明

## 版本

- Vitis 2020.2
- CPU：`ps7_cortexa9_0`
- OS：Standalone

## 重建

在 XSCT Console 中执行：

```tcl
source build/vitis/build_app.tcl
```

脚本使用 `build/hardware/system_wrapper.xsa` 创建：

- 平台：`zynq7020_platform`
- 应用：`gesture_app`
- 工作区：`build/vitis/workspace`

PS 源码来自 `src/ps/app`。其中：

- `main.c`：系统初始化、VDMA、显示与主循环；
- `gesture_ui.c`：LVGL 页面与任务按钮；
- `lv_port_indev.c`：GT911 触摸输入；
- `lv_port_disp.c`：LCD/显示刷新；
- `demos/`：界面图片和 LVGL 示例资源；
- `src/`：LVGL 8.3.11 源码。

## 运行顺序

1. 在 Vivado 中下载比特流；
2. 在 Vitis 中下载 ELF；
3. 串口参数设置为 `115200 8N1`；
4. 确认 LCD 背光、HDMI、OV7725 和 GT911 均完成初始化。

