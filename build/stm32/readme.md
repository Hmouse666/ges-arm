# STM32 构建说明

## 工程

主工程：

```text
src/stm32/mdk_arm/22software_5.uvprojx
```

IAR 工程：

```text
src/stm32/ewarm/22software_5.ewp
```

目标器件：STM32F103C8T6。

## 宏定义

```text
USE_FULL_LL_DRIVER
USE_HAL_DRIVER
STM32F103xB
```

## 头文件目录

```text
src/stm32/core/inc
src/stm32/drivers/stm32f1xx_hal_driver/inc
src/stm32/drivers/stm32f1xx_hal_driver/inc/legacy
src/stm32/drivers/cmsis/device/st/stm32f1xx/include
src/stm32/drivers/cmsis/include
src/stm32/tkkc_drivers
```

## 关键功能

- `fpga_gesture.c`：解析 Zynq 通过 UART 发送的任务、手势、坐标和深度；
- `task1.c`：任务一控制；
- `task2.c`：任务二控制；
- `task3.c`：任务三控制；
- `task4.c`：扩展任务控制；
- `motor.c`：步进电机脉冲与方向控制；
- `claw_servo.c`、`pca9685.c`：夹爪舵机控制；
- `tkkc_st7735.c`、`TKKC_Drivers/display.c`：本地状态屏。

## 编译后检查

1. 确认 UART 引脚与波特率同 Zynq 端一致；
2. 确认目标器件为 STM32F103C8；
3. 下载后先检查串口协议是否持续刷新；
4. 手部离开摄像头后，确认协议中的坐标和任务状态能按预期清零或保持。

