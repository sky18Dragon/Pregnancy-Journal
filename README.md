# Sticky Firmware

这是 reTerminal Sticky 的新固件工程。工程使用 PlatformIO 管理构建、烧录和串口监视，底层框架采用 ESP-IDF。`Sticky_dashboard_demo`是硬件驱动的参考来源。

当前`feature/ui-experience`分支将状态牌作为一个完整、独立的APP运行。启动入口直接进入横屏状态选择页，方便先验证页面布局和触摸切换体验；产品主页和IMU入口将在后续整合阶段接入。番茄钟APP源码继续保留，等待最终整合。

## 当前功能

### 横屏状态牌

- 原生使用800×480横屏坐标，一级菜单横向排列六种状态：`FOCUSING`、`IN A MEETING`、`WELCOME`、`OUT FOR LUNCH`、`OFF DUTY`和`CUSTOM`。
- 点击预设状态后进入二级展示页，状态文字、图标和补充信息铺满整个屏幕。
- 二级展示页左上角保留低存在感的`< BACK`触摸区，点击后返回一级菜单。
- 点击`CUSTOM`进入设备端全键盘，可直接输入最多20个大写字母、数字或空格。
- 自定义键盘提供`123`/`ABC`切换、空格、删除、清空和应用操作；有效内容应用后进入全屏展示页。
- 首次显示使用黑白全屏刷新，菜单选择、页面跳转和键盘输入使用黑白局部快刷。
- 触摸使用8个事件的有序队列，电子纸刷新期间检测到的点击会在刷新结束后继续处理。
- 连续字母、数字、空格、删除和清空操作会合并为一次画面刷新，提高连续输入速度。

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
- 所有预设时间和自定义时间都会从开始到结束逐秒更新数字与进度圆环。
- 提前结束后直接返回主页，并保留刚才选择的时长。
- 时间到后循环播放C6、E6、G6上升三音提示，按下`END`立即停止并返回主页，同时保留刚才选择的时长。

### 电子纸显示策略

- 页面切换使用黑白全屏刷新。
- 时间选择、数字输入和逐秒倒计时使用黑白局部快刷。
- 圆环的浅色部分使用黑白点阵模拟，使其在局部刷新后保持稳定。
- 计时使用真实截止时间计算，刷新耗时不会累加到剩余时间。
- 倒计时刷新期间记录的触摸会在刷新结束后继续处理，暂停和结束操作不会丢失。

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
status_board=ready orientation=landscape page=menu status=in_meeting choices=6 result=ok
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
src/apps/pomodoro/         已完成的独立番茄钟页面、触摸映射和状态机
src/apps/status_board/     横屏状态牌页面、状态机、自定义键盘和交互任务
src/board/                 电源、引脚和共享SPI准备
src/core/                  日志基础设施
src/devices/               蜂鸣器等独立设备接口
src/display/               屏幕初始化和刷新
src/input/                 GT911触摸初始化、坐标转换和采样
src/sensors/               已保留的姿态检测源码
src/ui/                    画布、基础图形、字体和已保留页面源码
src/main.cpp               当前独立状态牌启动入口
test/                      可在电脑上运行的回归测试
platformio.ini             开发版与发布版构建配置
```

## 状态牌真机验收

以下动作按顺序连续执行，方便将页面现象与串口日志一一对应。

1. 烧录`sticky-debug`并打开串口，等待横屏状态牌显示。
2. 确认一级菜单横向显示六个状态，默认选中的`IN A MEETING`为黑底，其余状态为白底。
3. 点击`FOCUSING`，确认进入黑底全屏展示页，大字、图标和`UNTIL 14:30`完整显示。
4. 点击左上角`< BACK`，确认返回一级菜单，并且`FOCUSING`保持为黑底选中状态。
5. 按同样顺序依次打开`IN A MEETING`、`WELCOME`、`OUT FOR LUNCH`和`OFF DUTY`，每次都使用左上角返回一级菜单。
6. 点击`CUSTOM`，确认进入设备端QWERTY全键盘，输入框显示`TYPE STATUS_`和`0 / 20`。
7. 连续输入`DEEP WORK MODE`，确认输入框与字符计数随每次按键更新。
8. 点击`123`，输入数字`2`，再点击`DELETE`删除该数字，确认数字键盘和删除操作都有效。
9. 点击`APPLY`，确认进入黑底全屏展示页并居中显示`DEEP WORK MODE`。
10. 点击`< BACK`返回一级菜单，再进入`CUSTOM`，确认刚才的自定义文字仍然保留。
11. 点击`CLEAR`后直接点击`APPLY`，确认页面保留在输入界面并显示至少输入一个字符的提示。
12. 连续输入20个字符后再点击任意字符，确认计数保持`20 / 20`，已输入内容不被覆盖。
13. 点击`CLEAR`，快速连续输入`STICKY`，确认六个字母按顺序完整出现；日志可出现`status_board=input_batch actions=... refreshes=1`。
14. 在自定义输入页点击一个字母，并在电子纸仍在刷新时点击一次`< BACK`，确认前一次刷新完成后自动返回一级菜单，无需重复点击。
15. 进入任一预设状态的二级展示页，点击一次`< BACK`，确认页面完成一次局刷后返回一级菜单。

主流程成功时，日志会按操作出现`status_board=touch`、`status_board=custom_input`和`status_board=transition`，并且触摸轮询保持安静。
开发版日志中的`queue_latency_ms`表示点击从触摸队列到应用处理所等待的时间，`status_board=refresh state=done`中的`elapsed_ms`表示电子纸完成本次刷新的时间。

## 番茄钟回归验收

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
19. 分别启动3分钟、5分钟、15分钟和一个自定义时间，确认每一种时长都从开始后的第一秒持续逐秒变化。
20. 在数字逐秒刷新过程中连续点击`PAUSE`和`END SESSION`，确认每次点击都会在当前刷新完成后生效。

主流程成功时，日志会按操作出现`pomodoro=touch`、`pomodoro=page`、`pomodoro=timer`和`buzzer=alarm`，并且触摸轮询保持安静。
