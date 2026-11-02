---
name: zynq-video-gesture-debug
description: 诊断和复现 Zynq-7020 摄像头、HDMI/LCD、GT911/LVGL、PL 手势算法及 PS 与 STM32 通信问题；适用于已有工程的状态排查、接口修正和上板验证。
metadata:
  short-description: Zynq 视频、触摸与手势项目调试
---

# Zynq Video Gesture Debug

用于 Zynq-7020 视频手势项目的证据驱动排查。目标不是泛泛讲解 Zynq，而是尽快判断问题位于 PL 视频链路、PS 软件、触摸/UI、手势算法还是下位机通信，并用最少改动完成验证。

## 使用边界

- 适用于 OV7725/OV5640、AXI4-Stream、VDMA、VTC、HDMI/LCD、GT911、LVGL、AXI GPIO、ILA 和 UART 协议问题。
- 不替代 Vivado 综合/实现、ILA 实测、串口日志和板级电气检查。
- 不把本项目中的固定引脚、时钟、分辨率、阈值或协议字段当作所有板卡通用值。
- 不在没有备份或未确认版本控制状态时批量删除、重命名或重置工程。

## 工作方式

1. 先确认当前工程根目录、Git 分支、Vivado/Vitis 版本和目标板卡。
2. 根据现象选择最小证据集：Vivado 设计属性、实现/DRC、ILA、串口、LCD/HDMI 实际画面、STM32 接收状态。
3. 一次只改一个层级，并记录修改前后的可观察结果。
4. 优先修复时钟域、复位、地址、帧格式和协议顺序等结构性问题，再调算法阈值。
5. 上板结论必须来自新生成的比特流、XSA、ELF 和实际日志；旧报告不能证明新设计。

## 快速分诊

- 摄像头能识别但没有 HDMI 图像：先读 `references/video_pipeline.md`。
- HDMI 正常，只有 LCD、触摸或 LVGL 异常：读 `references/touch_lvgl.md`。
- 画面正常，但手指数量、握拳、坐标或深度错误：读 `references/gesture_algorithm.md`。
- PS 有数据，STM32 不动作或协议字段错位：读 `references/ps_mcu_protocol.md`。
- 工程无法重建、编码乱码或导出 XSA 失败：读 `references/build_repro.md`。
- 需要按症状快速定位：读 `references/known_issue_matrix.md`。

## 关键不变量

- AXIS 两端连接前，必须同时核对 `FREQ_HZ` 和 `CLK_DOMAIN`；只让频率数字相同并不一定属于同一时钟域。
- VDMA 的帧地址、帧长、stride、像素格式和读写方向必须形成完整闭环。双通道显示时使用独立帧缓存，避免显存互相覆盖。
- 摄像头初始化成功不等于图像通路的 `tvalid/tready/tlast/tuser` 正常。应使用 ILA 在采集、处理和 VDMA 入口分别观察。
- GT911 的 I2C ACK 只证明地址响应，不证明配置下载、坐标寄存器读取或状态位清零正确。
- LVGL 的 `LV_EVENT_CLICKED` 通常发生在完整点击结束后。需要按下即响应时，应使用 `LV_EVENT_PRESSED`/`LV_EVENT_RELEASED` 或明确的状态机。
- 手势阈值应随手部框宽或当前肤色块宽度缩放。固定像素阈值会随摄像头距离产生明显误判。
- 串口协议只保留一个字段顺序和一份解析真值。修改字段、字节序或发送周期时，必须同时验证 PS 输出和 STM32 解析。
- 中文注释可能使用 GBK 或其他本地编码。修复代码时优先做 ASCII 路径替换，避免把整文件解码后重新保存。

## 输出要求

诊断结果至少包含：

1. 现象和可复现条件；
2. 已有证据及缺失证据；
3. 最可能的根因，按可能性排序；
4. 最小修改方案和需要重新生成的产物；
5. 上板验证步骤与通过标准。

需要检查项目文件是否完整、命名是否合规时，运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/check_project.ps1 -Root <repo-root>
```

