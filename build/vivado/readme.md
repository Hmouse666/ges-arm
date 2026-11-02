# Vivado 构建说明

## 版本

- Vivado 2020.2
- Part：`xc7z020clg400-2`

## 重建工程

在 Vivado Tcl Console 执行：

```tcl
source build/vivado/create_project.tcl
```

脚本从 `src/fpga/rtl`、`src/fpga/constraints`、`src/fpga/ip_repo` 和 `src/fpga/bd/recreate_system_bd.tcl` 重建工程。

## 生成比特流与 XSA

```tcl
source build/vivado/build.tcl
```

输出：

- `build/vivado/work/project/zynq7020_gesture.xpr`
- `build/hardware/zynq7020_gesture.xsa`
- `build/reports/*_new.rpt`

## 已保存的实现快照

`build/reports/` 中保存了整理前最近一次成功实现的报告：

- `timing_summary.rpt`
- `utilization.rpt`
- `route_status.rpt`
- `drc.rpt`
- `methodology_drc.rpt`
- `power.rpt`

这些报告只用于追溯，不作为后续 RTL 修改后的最终验证结果。

## 自建 IP

两个自定义 IP 位于：

```text
src/fpga/ip_repo/dvi_tx
src/fpga/ip_repo/ov5640_capture_data
```

IP 的 VLNV 保持原值，避免破坏 Block Design：

- `openedv.com:user:DVI_Transmitter:1.0`
- `xilinx.com:user:ov5640_capture_data:1.0`

修改 `component.xml` 后，需要重新扫描 IP Catalog。

