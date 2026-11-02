# Zynq 视频手势项目技能包

本目录包含从该项目对话和实机调试中提炼的 Codex Skill：

```text
skill/
├── readme.md
└── zynq-video-gesture-debug/
    ├── skill.md
    ├── agents/
    │   └── openai.yaml
    ├── references/
    │   ├── video_pipeline.md
    │   ├── touch_lvgl.md
    │   ├── gesture_algorithm.md
    │   ├── ps_mcu_protocol.md
    │   ├── build_repro.md
    │   └── known_issue_matrix.md
    └── scripts/
        ├── check_project.ps1
        └── install.ps1
```

## 技能用途

`zynq-video-gesture-debug` 用于诊断和复现以下工作流：

- OV7725 采集、AXIS、VDMA、VTC、HDMI/LCD 视频链路；
- Zynq PL 手势识别与 ILA 调试；
- GT911/EMIO I2C 触摸与 LVGL 输入；
- PS 与 STM32 的手势数据协议和任务状态机；
- Vivado/Vitis/STM32 工程重建与命名规范。

## 安装到 Codex

项目为满足文件名全小写规范，技能入口文件使用 `skill.md`。安装脚本会将它复制为 Codex 标准的 `SKILL.md`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\skill\zynq-video-gesture-debug\scripts\install.ps1
```

安装后通过显式方式调用：

```text
$zynq-video-gesture-debug
```

## 项目检查

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\skill\zynq-video-gesture-debug\scripts\check_project.ps1 -Root .
```

检查内容包括关键源码是否存在、文件名是否合规，以及实现报告是否已放置。

