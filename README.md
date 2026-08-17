# Sticky Firmware

这是 reTerminal Sticky 的新固件工程。工程使用 PlatformIO 管理构建、烧录和串口监视，底层框架采用 ESP-IDF。`Sticky_dashboard_demo` 作为经过验证的硬件驱动参考，新功能将在本工程中按模块逐项实现和验收。

## 当前阶段

阶段 0 已完成真机验收，具备以下基础能力：

- ESP32-S3、32 MB Flash 和 Octal PSRAM 工程配置。
- 8 MB 固件分区与约 24 MB 资源分区。
- GPIO45 与 GPIO46 板级供电锁存。
- 启动原因、唤醒原因、芯片、Flash、堆内存与 PSRAM 诊断日志。
- 开发版与发布版两套日志配置。

阶段 1 已加入最小电子纸显示链路：

- SSD1677控制器和800×480电子纸驱动。
- GPIO47屏幕供电与SPI2总线初始化。
- 电子纸与MicroSD共享SPI2时的板级引脚准备。
- 在屏幕驱动启动前预装GPIO中断服务。
- 位于PSRAM中的2位灰度画布和旋转缓冲区。
- 5×7 ASCII点阵字体和基础绘图函数。
- 带方向标记的黑白全屏测试画面。
- 屏幕初始化、缓冲区和刷新耗时诊断日志。

阶段 2 正在验证GT911触摸链路：

- I2C0总线、触摸供电、复位和中断引脚。
- GT911双地址探测、分辨率读取和坐标采样。
- 五点触摸测试画面和物理坐标转换。
- 每次首次检测到手指时输出一条坐标日志。

IMU和正式产品页面将在触摸真机验收后逐项加入。

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
shared_spi=prepare_begin
shared_spi=sd_idle cs_level=1 en_level=1 detect_level=1
shared_spi=gpio_isr_ready state=installed
shared_spi=prepare_done result=ok
display=init_begin
display=power_on
display=spi_bus_ready
display=spi_device_ready
display=panel_ready
display=framebuffer_ready
display=refresh_begin mode=monochrome_full
display=refresh_done mode=monochrome_full
display=touch_test_pattern result=ok
touch=init_begin
touch=controller_ready
touch=polling_ready
system=idf
memory=flash
memory=heap
phase=ready result=ok
```

`detect_level=1`表示当前未检测到MicroSD卡，插卡后通常会显示`detect_level=0`。这一阶段只将MicroSD的控制脚设置为参考工程的启动状态，尚未挂载或读写存储卡。

屏幕会执行一次全屏刷新，然后显示白色背景、黑色外框、四种不同的角标和中央文字`STICKY DISPLAY OK`。左上角应是带白色`TL`文字的黑色方块，可用它确认画面方向。

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
- `STICKY_LOG_TOUCH_DRIVER_OUTPUT_ENABLED`
- `STICKY_LOG_MOTION_SAMPLES_ENABLED`

开发阶段按需要打开对应开关。GT911驱动执行路径保留在Debug固件中，轮询详细日志通过独立开关控制。发布环境统一关闭高频日志。

## 工程结构

```text
boards/                 Sticky 的 PlatformIO 板卡定义
components/seeed_epaper SSD1677/UC8179 电子纸公共驱动
components/debug_logging 电子纸和触摸的编译期详细日志开关
src/board/              电源、引脚和SD/屏幕共享SPI准备
src/core/               日志与后续应用核心
src/display/            屏幕初始化、刷新和测试图案
src/input/              GT911触摸初始化、坐标转换和采样
src/ui/                 画布、基础图形和5×7字体
src/main.cpp            固件入口、启动诊断和显示验证流程
platformio.ini          开发版与发布版构建配置
sdkconfig.defaults      ESP-IDF 硬件配置
partitions.csv          固件与资源空间分配
```

## 阶段 1 验收

1. 编译 `sticky-debug` 和 `sticky-release`。
2. 烧录 `sticky-debug`。
3. 确认串口依次出现`shared_spi=sd_idle cs_level=1 en_level=1`和`shared_spi=prepare_done result=ok`。
4. 等待电子纸完成一次全屏闪烁刷新。
5. 确认屏幕中央显示`STICKY DISPLAY OK`，四角图形完整可见。
6. 确认带`TL`文字的黑色方块位于左上角，文字没有镜像。
7. 确认串口出现`display=test_pattern result=ok`和`phase=ready result=ok`。
8. 连续运行一分钟，确认出现两次心跳且可用内存没有持续下降。
9. 按复位键，确认屏幕可以再次刷新出相同测试画面。
10. 分别在MicroSD卡插入和拔出状态下复位一次，确认两种状态都能完成屏幕刷新。

## 阶段 2 验收

1. 烧录`sticky-debug`并打开串口。
2. 确认屏幕显示`TOUCH 5 POINT TEST`和五个目标点。
3. 依次点击`TOP_LEFT`、`TOP_RIGHT`、`CENTER`、`BOTTOM_LEFT`和`BOTTOM_RIGHT`。
4. 每次手指按下时，确认串口只出现一条`touch=detected`日志。
5. 松手后再次按下，应再输出一条新日志。
