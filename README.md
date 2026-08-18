# Sticky Firmware

这是 reTerminal Sticky 的新固件工程。工程使用 PlatformIO 管理构建、烧录和串口监视，底层框架采用 ESP-IDF。`Sticky_dashboard_demo`是硬件驱动的参考来源。

当前`feature/ui-experience`分支将番茄钟作为一个完整、独立的APP运行。启动入口直接进入番茄钟，方便先验证页面、触摸、计时和蜂鸣器体验；产品主页和IMU入口将在后续整合阶段接入。

## 当前功能

### 番茄钟主页

- 默认选择15分钟。
- 提供10秒、30秒、1分钟、3分钟、5分钟和15分钟六个预设时间。
- 支持自定义小时、分钟和秒钟。
- 圆环是页面的主要视觉元素，操作区集中在页面下方。

### 自定义时间

- 使用固定数字键盘，适合电子纸的低刷新特性。
- 点击`HR`、`MIN`或`SEC`选择输入位置。
- `CLEAR`清空全部时间。
- `DELETE`删除当前输入位置的最后一位。
- `USE THIS TIME`保存有效时间并返回主页。

### 运行与结束

- 支持开始、暂停、继续和提前结束确认。
- 10秒、30秒、1分钟和所有计时的最后60秒会逐秒更新。
- 3分钟、5分钟和15分钟在前段按分钟更新，进入最后60秒后切换为逐秒更新。
- 提前结束后直接返回主页，并保留刚才选择的时长。
- 时间到后循环播放C6、E6、G6上升三音提示，按下`END`立即停止并返回主页，同时保留刚才选择的时长。

### 电子纸显示策略

- 页面切换使用黑白全屏刷新。
- 时间选择、数字输入、分钟节点和逐秒倒计时使用黑白局部快刷。
- 圆环的浅色部分使用黑白点阵模拟，使其在局部刷新后保持稳定。
- 计时使用真实截止时间计算，刷新耗时不会累加到剩余时间。

## 硬件接口

- SSD1677 800×480电子纸屏幕。
- GT911触摸控制器。
- GPIO48无源蜂鸣器，使用2400Hz、10位LEDC输出。
- 电子纸与MicroSD共享SPI2，启动时先将MicroSD控制脚设置为确定的空闲状态。
- GPIO45和GPIO46负责板级供电锁存。

番茄钟APP位于`src/apps/pomodoro/`，只通过屏幕、触摸和蜂鸣器接口使用硬件。原有姿态与方向页面源码继续保留，后续由产品入口负责选择并启动APP。

## 环境

- PlatformIO Core 6.1.19
- `espressif32` Platform 6.11.0
- ESP-IDF 5.4.1
- 目标芯片：ESP32-S3

工程固定使用`espressif32@6.11.0`，与硬件参考工程的ESP-IDF 5.4驱动接口保持一致。

## 编译

开发版保留详细诊断日志：

```bash
/Users/mengdu/.local/bin/pio run -e sticky-debug
```

发布版在编译时移除调试与追踪日志：

```bash
/Users/mengdu/.local/bin/pio run -e sticky-release
```

## 烧录和查看日志

连接Sticky后执行：

```bash
/Users/mengdu/.local/bin/pio run -e sticky-debug -t upload
/Users/mengdu/.local/bin/pio device monitor -b 115200
```

启动成功时会看到这些关键日志：

```text
phase=start
shared_spi=prepare_done result=ok
display=panel_ready
display=framebuffer_ready
touch=controller_ready
touch=polling_ready
buzzer=ready
phase=ready result=ok
pomodoro=ready page=setup default_duration_s=900 result=ok
```

## 日志设计

`src/core/app_log.h`提供五个编译级别：

- Error：必要组件无法继续运行。
- Warn：输入无效或功能结果不完整。
- Info：页面切换、触摸操作、计时和蜂鸣器状态。
- Debug：自定义时间字段和输入值。
- Trace：心跳和高频时序数据。

`platformio.ini`负责选择开发版和发布版的整体日志级别。高频类别继续由独立宏控制：

- `STICKY_LOG_BOOT_DETAILS_ENABLED`
- `STICKY_LOG_HEARTBEAT_ENABLED`
- `STICKY_LOG_DISPLAY_TIMING_ENABLED`
- `STICKY_LOG_TOUCH_SAMPLES_ENABLED`
- `STICKY_LOG_TOUCH_DRIVER_OUTPUT_ENABLED`
- `STICKY_LOG_MOTION_SAMPLES_ENABLED`
- `STICKY_LOG_TIMER_TICKS_ENABLED`

## 工程结构

```text
boards/                    Sticky的PlatformIO板卡定义
components/seeed_epaper    SSD1677/UC8179电子纸驱动
components/debug_logging   编译期详细日志开关
src/apps/pomodoro/         独立番茄钟页面、触摸映射和状态机
src/board/                 电源、引脚和共享SPI准备
src/core/                  日志基础设施
src/devices/               蜂鸣器等独立设备接口
src/display/               屏幕初始化和刷新
src/input/                 GT911触摸初始化、坐标转换和采样
src/sensors/               已保留的姿态检测源码
src/ui/                    画布、基础图形、字体和已保留页面源码
src/main.cpp               独立番茄钟启动入口
platformio.ini             开发版与发布版构建配置
```

## 真机验收

以下动作按顺序连续执行，方便将页面现象与串口日志一一对应。

### 主流程

1. 烧录`sticky-debug`并打开串口，等待番茄钟主页显示。
2. 确认默认时间为`15:00`，15分钟预设为选中状态。
3. 依次点击10秒、30秒、1分钟、3分钟、5分钟和15分钟，确认页面时间和选中项同步变化。
4. 点击`CUSTOM TIME`，依次选择小时、分钟和秒钟，使用数字键输入`00:01:30`。
5. 点击`USE THIS TIME`，确认主页显示`01:30`。
6. 点击`START FOCUS`，确认进入`FOCUSING`页面。
7. 点击`PAUSE`，等待数秒后点击`RESUME`，确认剩余时间从暂停位置继续。
8. 点击`END SESSION`，再点击`KEEP SESSION`，确认返回运行状态。
9. 再次点击`END SESSION`，然后点击`END NOW`，确认返回主页并保留本次选择的时长。

### 时间到与蜂鸣器

10. 在主页选择10秒并开始计时。
11. 确认时间数字从`00:10`开始逐秒快刷并递减。
12. 时间到后确认页面显示`TIME'S UP`、`00:00`和`ALARM SOUNDING`，蜂鸣器循环播放上升三音提示。
13. 保持10秒不操作，确认三音提示持续循环，并且每组声音之间存在安静间隔。
14. 点击`END`，确认蜂鸣器立即停止，随后页面返回主页并保留本次选择的时长。

### 边缘情况

15. 进入自定义时间，输入`00:00:00`并点击`USE THIS TIME`，确认页面保持在自定义时间页，日志出现`result=invalid`。
16. 分钟或秒钟输入60至99，再点击`USE THIS TIME`，确认页面保持不变并记录无效输入。
17. 在运行、暂停、提前结束确认和响铃页面点击空白区域，确认状态保持不变。
18. 连续完成三次10秒计时，确认每次`END`都能停止蜂鸣器，并继续保持10秒为选中时间。

主流程成功时，日志会按操作出现`pomodoro=touch`、`pomodoro=page`、`pomodoro=timer`和`buzzer=alarm`，并且触摸轮询保持安静。
