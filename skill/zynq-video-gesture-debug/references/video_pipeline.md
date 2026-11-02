# 视频链路排查

## 证据顺序

按以下顺序收集证据，避免直接改算法：

1. 摄像头初始化返回值；
2. 采集时钟、`vid_ce`、AXIS `tvalid/tready/tlast/tuser`；
3. `v_vid_in_axi4s` 输出格式和 VDMA S2MM 状态；
4. VDMA 帧地址、帧长、stride、读写通道；
5. VTC 分辨率和视频输出时钟；
6. DVI/TMDS 或 RGB LCD 引脚；
7. PS 中的中断、状态轮询和启动顺序。

摄像头检测成功只能证明 SCCB/I2C 初始化和寄存器写入成功，不能证明像素已经进入 PL。

## AXIS 时钟与格式

连线前核对两端：

- `FREQ_HZ`
- `CLK_DOMAIN`
- AXIS 位宽
- `tuser` 是否表示 SOF
- `tlast` 是否表示行尾

不能只把 `FREQ_HZ` 改成相同数字。不同时钟源即使频率相同，也仍是异步时钟域。

常见错误链路：

```text
摄像头像素时钟 -> v_vid_in_axi4s（异步）
v_vid_in_axi4s -> 帧差/处理 -> VDMA S2MM
VDMA MM2S -> v_axi4s_vid_out
```

在每个跨时钟域边界确认是否真的需要异步 FIFO 或 `v_vid_in_axi4s` 的异步时钟模式。

## VDMA 配置

逐项核对：

- `c_include_sg` 与软件配置是否一致；
- `c_s2mm_data_width` / `c_mm2s_data_width`；
- 每像素字节数与 AXI 位宽是否匹配；
- `hsize` 是像素数还是字节数；
- stride 是否等于一行字节数；
- 帧地址是否重叠；
- MM2S 是否处于运行态；
- S2MM 是否因 frame count、错误位或地址问题停止；
- Genlock 和 `vsync` 配置是否符合当前显示架构。

双显示建议使用两个独立帧区域。OV7725 路径与 LVGL 路径共用同一 DDR 区域时，任何 stride 或地址错误都可能表现为“HDMI 的内容跑到 LCD”。

## 典型画面与优先级

| 画面 | 优先检查 |
|---|---|
| 全黑 | 背光、复位、VTC、VDMA MM2S、显示时钟 |
| 全白 | 背光提前开启、帧缓存未初始化、RGB 数据恒高 |
| 雪花 | AXIS 数据无效、像素时钟/时序错误、TMDS/并口采样错误 |
| 花屏 | stride、像素格式、AXIS 位宽、帧地址或时钟域错误 |
| 固定测试图正常、摄像头无图 | 采集时钟、`vid_ce`、AXIS handshake、VDMA S2MM |
| HDMI 正常、LCD 异常 | 第二路 VDMA、LCD 色彩格式、VTC1、LCD 引脚 |

## ILA 使用

优先观察：

- `s_axis_tvalid`
- `s_axis_tready`
- `s_axis_tuser`
- `s_axis_tlast`
- `s_axis_tdata[23:0]`
- VDMA 输入 AXIS 的 handshake

如果 `tvalid` 从不拉高，问题在采集前端；如果 `tvalid=1` 但 `tready=0`，问题在 VDMA 或下游背压；如果只有首行有效，检查 `tlast` 和行长。

## 启动顺序

推荐顺序：

1. 确认 PL 已配置；
2. 初始化摄像头；
3. 初始化 VDMA 与帧缓存；
4. 启动 VTC；
5. 启动显示输出；
6. 在 PS 确认画面稳定后开启 LCD 背光。

背光若在 FPGA 和帧缓存准备好之前拉高，会出现白闪或亮度异常。PL 约束中可先给背光下拉，PS 再通过 GPIO 释放。

