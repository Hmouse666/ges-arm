# 可复现构建与工程维护

## 目录原则

- 源码、约束、BD 脚本、IP 仓库和构建说明纳入版本控制；
- `.runs`、`.cache`、`.gen`、`.Xil`、`Debug` 等生成目录不提交；
- 不手工重命名 Vivado/Vitis 生成目录内部文件；
- 用户新增文件名使用小写英文、数字、下划线或连字符；
- 中文只放在文件正文中。

## Vivado

推荐流程：

```tcl
source build/vivado/create_project.tcl
source build/vivado/build.tcl
```

重建时确认：

- 器件为 `xc7z020clg400-2`；
- 自定义 IP 仓库已注册；
- BD Tcl 在 RTL 加入后执行；
- 自定义模块引用能找到；
- wrapper 被设为顶层；
- XDC 与当前底板一致。

自定义 IP 只改变目录名时，需要同步检查 `component.xml` 中的 `xgui` 和 RTL 文件路径；VLNV 不建议随意修改，否则 BD 会找不到实例。

## Vitis

使用新导出的 XSA 创建 platform。硬件地址、GPIO 双通道或 VDMA 配置改变后，应重新生成 BSP，而不是只重新编译应用。

应用源文件集合必须排除：

- `.bak` 文件；
- 空源文件；
- 重复的同名实现；
- 未启用的第三方平台源文件。

## STM32

核对 MDK 工程中的：

- 器件型号；
- 宏定义 `USE_HAL_DRIVER`、`USE_FULL_LL_DRIVER`、`STM32F103xB`；
- 启动文件；
- 链接脚本；
- HAL/LL source；
- 自定义驱动 include path。

## 编码与注释

项目历史中多次出现 GBK/UTF-8 混用。安全做法：

1. 先备份文件；
2. 判断原编码；
3. 只改必要的 ASCII 文件名或 include 路径；
4. 不把整个含中文注释的文件无依据地转码；
5. 修改后用原工具重新打开，确认注释、字符串和编译都正常。

## 产物一致性

每次重新生成后确认四者来自同一版本：

- 比特流；
- XSA；
- Vitis ELF；
- 板级下载配置。

只更新源文件但没有重新生成或下载其中一个产物，是“代码改了但现象没变”的常见原因。

## 命名检查

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build/validate_names.ps1 -Root .
```

脚本忽略 `.git` 元数据；工作树中的源码、文档和脚本均应通过。

