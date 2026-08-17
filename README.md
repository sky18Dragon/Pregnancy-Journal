# Sticky Firmware

这是 reTerminal Sticky 的新固件工程。工程使用 PlatformIO 管理构建、烧录和串口监视，底层框架采用 ESP-IDF。`Sticky_dashboard_demo` 作为经过验证的硬件驱动参考，新功能将在本工程中按模块逐项实现和验收。

## 当前阶段

阶段 0 已建立以下基础能力：

- ESP32-S3、32 MB Flash 和 Octal PSRAM 工程配置。
- 8 MB 固件分区与约 24 MB 资源分区。
- GPIO45 与 GPIO46 板级供电锁存。
- 启动原因、唤醒原因、芯片、Flash、堆内存与 PSRAM 诊断日志。
- 开发版与发布版两套日志配置。

电子纸、触摸、IMU 和正式应用将在后续阶段逐项加入。

## 环境

- PlatformIO Core 6.1.19
- `espressif32` Platform 6.11.0
- ESP-IDF 5.4.1
- 目标芯片：ESP32-S3

本工程固定使用 `espressif32@6.11.0`，以保持与硬件参考工程 ESP-IDF 5.4 的驱动接口一致。

## 编译

开发版默认保留详细诊断日志：

```bash
/Users/mengdu/.local/bin/pio run -e sticky-debug
```

发布版在编译时移除调试与追踪日志：

```bash
/Users/mengdu/.local/bin/pio run -e sticky-release
```

## 烧录和查看日志

连接 Sticky 后执行：

```bash
/Users/mengdu/.local/bin/pio run -e sticky-debug -t upload
/Users/mengdu/.local/bin/pio device monitor -b 115200
```

开发版启动成功时会依次看到：

```text
phase=start
boot=reason
step=begin
step=ready
system=idf
memory=flash
memory=heap
phase=ready result=ok
```

## 日志设计

`src/core/app_log.h` 提供五个编译级别：

- Error：无法继续运行的错误。
- Warn：可以继续运行，但结果可能不完整。
- Info：启动、状态切换和关键操作结果。
- Debug：硬件参数、内存和动作判定过程。
- Trace：传感器采样、心跳和高频时序数据。

`platformio.ini` 负责选择整体级别。高频类别还有独立开关：

- `STICKY_LOG_BOOT_DETAILS_ENABLED`
- `STICKY_LOG_HEARTBEAT_ENABLED`
- `STICKY_LOG_DISPLAY_TIMING_ENABLED`
- `STICKY_LOG_TOUCH_SAMPLES_ENABLED`
- `STICKY_LOG_MOTION_SAMPLES_ENABLED`

开发阶段按需要打开对应开关，发布环境统一关闭高频日志。

## 工程结构

```text
boards/                 Sticky 的 PlatformIO 板卡定义
src/board/              电源、引脚和后续共享板级资源
src/core/               日志与后续应用核心
src/main.cpp            固件入口和启动诊断
platformio.ini          开发版与发布版构建配置
sdkconfig.defaults      ESP-IDF 硬件配置
partitions.csv          固件与资源空间分配
```

## 阶段 0 验收

1. 编译 `sticky-debug` 和 `sticky-release`。
2. 烧录 `sticky-debug`。
3. 确认 Sticky 上电后保持运行。
4. 确认串口出现 `phase=ready result=ok`。
5. 连续运行一分钟，确认出现两次心跳且可用内存没有持续下降。
6. 按复位键，确认新的启动日志再次完整出现。
