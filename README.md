# Sticky Firmware

这是 reTerminal Sticky 的新固件工程。工程使用 PlatformIO 管理构建、烧录和串口监视，底层框架采用 ESP-IDF。`Sticky_dashboard_demo`是硬件驱动的参考来源。

当前`feature/ui-experience`分支将答案书作为一个完整、独立的APP运行。启动入口直接进入竖屏答案书主页，使用IMU摇晃开始提问，并独立验证答案类型、全页动画、随机结果与触摸交互。番茄钟与状态牌APP源码继续保留，等待最终整合。

## 当前功能

### 答案书

- 主页面可以选择`MESSAGE`或`YES / NO`两种答案类型，返回主页时保留刚才的选择。
- 默认选中`MESSAGE`；用户可以直接摇晃设备开始提问，也可以先点击`YES / NO`切换答案类型。
- 第一次明显摇晃会启动答案动画；用户需要在摇晃、思考和揭晓动画播放期间持续摇晃，完整动画通过后才会抽取答案。
- 摇晃会话通过连续加速度峰值保持有效；动作停止超过允许间隔后立即判定为摇晃不足，普通缓慢拿起或转向不会进入提问流程。
- 摇晃页面交替播放左右两个大幅动作，兔子的身体、耳朵、手臂和水晶球位置都会明显变化。
- 主页、摇晃、思考、揭晓、摇晃不足、文字答案和水晶球答案七类页面都拥有独立动作帧。
- 摇晃期间依次显示摇晃、`THINKING...`和`REVEALING...`过渡动画；任一阶段提前停下都会进入`SHAKE A LITTLE LONGER`提示页，不会显示答案。
- `MESSAGE`模式使用来源CSV中的前350条英文答案，按原始顺序生成固件答案表；结果页会根据句子长度自动排成一至四行，并且不会连续重复同一条。
- `YES / NO`模式严格使用`YES`、`NO`和`UNCLEAR`三个结果；答案按水晶球圆心进行水平和垂直居中。
- 水晶球结果页使用完整双层玻璃轮廓、内部星光、装饰底座和互动兔子，两帧动画会改变高光、星尘、兔子眼睛与手部动作。
- 两类结果页都提供`ASK AGAIN`和`END`；再次提问返回保留答案类型的主页，重新持续摇晃后开始下一轮。
- 上电后先使用白色全屏波形清除电子纸旧画面，再完整刷新答案书主页；后续页面切换和动画使用黑白局部快刷。
- 触摸使用现有有序事件队列，动画期间产生的触摸会被当前动画页面消费，结果页不会收到遗留点击。
- 答案书插画拥有独立素材库，设计源图、固件预览与生成脚本集中存放在`assets/book_of_answers/`。

### 保留的横屏状态牌

- 原生使用800×480横屏坐标，一级菜单横向排列六张内容高度卡片：`FOCUSING`、`IN A MEETING`、`WELCOME`、`OUT FOR LUNCH`、`OFF DUTY`和`CUSTOM`；卡片只包住兔子与英文名称。
- 一级菜单底部是完整的宠物巡场区域：一条横跨底部的地面线标出行走路径，兔子在左侧挥手和蓄力，分段跳到右侧庆祝，再分段走回左侧循环。
- 宠物动画每帧只重绘底部区域并使用黑白局部快刷；应用每轮先处理触摸，再更新动画。
- 点击预设状态后进入二级展示页，状态文字和像素兔子场景共同铺满整个屏幕。
- 二级展示页只表达当前状态，不显示固定时间或补充信息。
- 六个二级展示页各自拥有符合状态的兔子动作：敲键盘、会议交流、挥手、抬勺吃饭、背包走路与倾斜状态牌。
- 子页动画使用160×160高精度素材以2倍绘制，保持兔子占屏尺寸的同时细化线条和动作。
- 子页动画仅替换右侧兔子区域，主帧停留450毫秒，次帧停留250毫秒；左侧状态文字和返回箭头保持不变。
- 六种状态使用完整轮廓、表情和动作的像素兔子素材；菜单与全屏页共用同一张1位位图，黑白背景自动切换绘制颜色。
- 二级展示页和自定义输入页左上角使用统一的返回箭头，整个左上角145×120区域均可触发返回。
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
- LSM6DS3TR-C加速度计，104Hz、正负2g量程。
- GPIO48无源蜂鸣器，使用2400Hz、10位LEDC输出。
- 电子纸与MicroSD共享SPI2，启动时先将MicroSD控制脚设置为确定的空闲状态。
- GPIO45和GPIO46负责板级供电锁存。

答案书APP位于`src/apps/book_of_answers/`，当前通过屏幕、触摸和IMU接口使用硬件。番茄钟、状态牌、姿态与方向页面源码继续保留，后续由产品入口负责选择并启动APP。

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
display=clear_begin color=white mode=full
display=clear_done color=white mode=full
touch=controller_ready
touch=polling_ready
buzzer=ready
sensor_bus=ready
imu=ready
imu=monitoring
phase=ready result=ok
book=ready page=home mode=message input=continuous_imu_shake message_answers=350 crystal_answers=3 shake_frames=4 animated_pages=7 result=ok
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
- `STICKY_LOG_PET_ANIMATION_ENABLED`
- `STICKY_LOG_STATUS_ANIMATION_ENABLED`
- `STICKY_LOG_BOOK_ANIMATION_ENABLED`
- `STICKY_LOG_TIMER_TICKS_ENABLED`

宠物动画默认只记录一次启动信息，不使用通用刷新耗时日志逐帧刷屏。需要查看每帧动作、位置和刷新耗时时，将`STICKY_LOG_PET_ANIMATION_ENABLED`设为`1`。
子页兔子动画采用同样的安静日志策略，需要逐帧调试时将`STICKY_LOG_STATUS_ANIMATION_ENABLED`设为`1`。
答案书默认记录页面切换、答案类型、随机结果、有效触摸和摇晃检测结果。开发版会记录三次有效摇晃峰值；需要查看所有页面逐帧序号时，将`STICKY_LOG_BOOK_ANIMATION_ENABLED`设为`1`。

## 工程结构

```text
boards/                    Sticky的PlatformIO板卡定义
components/seeed_epaper    SSD1677/UC8179电子纸驱动
components/debug_logging   编译期详细日志开关
src/apps/pomodoro/         已完成的独立番茄钟页面、触摸映射和状态机
src/apps/status_board/     横屏状态牌页面、状态机、自定义键盘和交互任务
src/apps/book_of_answers/  答案书页面、答案池、触摸映射和动画状态机
assets/book_of_answers/    答案书设计源图与固件黑白素材预览
assets/pixel_bunnies/      像素兔子设计源图、状态与宠物动画固件预览
src/board/                 电源、引脚和共享SPI准备
src/core/                  日志基础设施
src/devices/               蜂鸣器等独立设备接口
src/display/               屏幕初始化和刷新
src/input/                 GT911触摸初始化、坐标转换和采样
src/sensors/               姿态监测与防误触发摇晃检测
src/ui/                    画布、字体、公共1位像素素材接口和已保留页面源码
src/main.cpp               当前独立答案书启动入口
test/                      可在电脑上运行的回归测试
platformio.ini             开发版与发布版构建配置
```

## 答案书真机验收

以下操作连续执行，方便把画面变化与串口日志对应起来。

1. 烧录`sticky-debug`并打开串口，观察屏幕先执行一次完整白屏刷新，再显示答案书主页。
2. 确认旧页面残影已经清除，主页默认选中`MESSAGE`，并显示完整的兔子、水晶球和桌面。
3. 在主页停留2秒，确认兔子的耳朵、爪子和周围星光会持续变化，底部显示`SHAKE THE DEVICE`。
4. 点击`YES / NO`，确认右侧选项变成黑底；再点击`MESSAGE`，确认选择恢复到左侧。
5. 缓慢拿起设备并旋转90度，确认答案流程不会启动。
6. 用力摇动一次后立即停下，确认答案动画虽然启动，但随后进入`SHAKE A LITTLE LONGER`提示页，并且没有显示或记录任何答案。
7. 确认提示页显示`KEEP YOUR QUESTION IN YOUR HEART`和`THE CRYSTAL NEEDS MORE TIME`，兔子与水晶球左右摆动。
8. 等待提示页自动返回主页；再重复一次摇晃不足，并在提示页重新开始持续摇晃，确认可以直接重启答案动画。
9. 持续、明确地左右摇晃设备，并在摇晃、`THINKING...`和`REVEALING...`三个阶段全程保持动作。
10. 在思考页和揭晓页各观察至少一次动作变化，直到完整动画结束后才停止摇晃。
11. 确认动画通过后进入一句话答案页，兔子的眼睛、爪子和答案卡片会持续变化。
12. 点击`ASK AGAIN`，确认回到仍选中`MESSAGE`的主页；重新持续摇晃后获得新答案。
13. 确认相邻两次一句话答案不同，再点击`END`返回主页，并确认仍选中`MESSAGE`。
14. 选择`YES / NO`并持续摇晃完整动画，确认结果只能是`YES`、`NO`或`UNCLEAR`。
15. 确认分类结果页使用完整的双层玻璃球轮廓、星尘、带切面的底座和抱球兔子。
16. 分别获得`YES`、`NO`或`UNCLEAR`时，确认答案在玻璃球内部水平和垂直居中。
17. 在分类结果页停留2秒，确认兔子的眼睛、耳朵、爪子以及球内高光和星尘会持续变化。
18. 点击`ASK AGAIN`返回主页并完成下一轮持续摇晃，确认相邻两次结果不同。
19. 点击`END`返回主页，确认仍选中`YES / NO`。
20. 在动画期间点击屏幕，确认动画继续完成，结果页不会收到遗留点击。

摇晃不足时，日志会出现`imu=shake state=stopped`和`book=shake qualification=insufficient`；完整通过时会出现`book=shake qualification=passed`，随后才出现`book=answer selected`。开发版默认不会逐帧打印动画；需要逐帧观察时，把`STICKY_LOG_BOOK_ANIMATION_ENABLED`设为`1`重新编译。

## 状态牌真机验收（切换独立入口后使用）

以下动作按顺序连续执行，方便将页面现象与串口日志一一对应。

1. 烧录`sticky-debug`并打开串口，等待横屏状态牌显示。
2. 确认一级菜单横向显示六个状态，每张卡片只包住对应的像素兔子和英文名称；默认选中的`IN A MEETING`为黑底白兔线稿，其余状态为白底黑兔线稿。
3. 确认底部有一条左右各留20像素边距的细地面线；兔子在左边挥手后蓄力，分三段跳到右边，右边举手庆祝，再沿地面线分三段走回左边，然后自动重复。
4. 在兔子跳跃或走动时点击任意状态卡片，确认当前刷新结束后只需一次点击就能进入对应二级页。
5. 点击`FOCUSING`，确认进入黑底全屏展示页，左侧大字和右侧专注工作的兔子共同铺满画面，页面不显示固定时间。
6. 点击左上角返回箭头及箭头周围区域，确认都能返回一级菜单，并且`FOCUSING`保持为黑底选中状态。
7. 按同样顺序依次打开`IN A MEETING`、`WELCOME`、`OUT FOR LUNCH`和`OFF DUTY`，每次都使用左上角返回一级菜单。
8. 在六个状态子页分别停留3秒，确认兔子执行对应动作，左侧文字与返回箭头在动画期间保持不变。
9. 点击`CUSTOM`，确认进入设备端QWERTY全键盘，输入框显示`TYPE STATUS_`和`0 / 20`。
10. 连续输入`DEEP WORK MODE`，确认输入框与字符计数随每次按键更新。
11. 点击`123`，输入数字`2`，再点击`DELETE`删除该数字，确认数字键盘和删除操作都有效。
12. 点击`APPLY`，确认进入黑底全屏展示页并居中显示`DEEP WORK MODE`。
13. 点击左上角返回箭头返回一级菜单，再进入`CUSTOM`，确认刚才的自定义文字仍然保留。
14. 点击`CLEAR`后直接点击`APPLY`，确认页面保留在输入界面并显示至少输入一个字符的提示。
15. 连续输入20个字符后再点击任意字符，确认计数保持`20 / 20`，已输入内容不被覆盖。
16. 点击`CLEAR`，快速连续输入`STICKY`，确认六个字母按顺序完整出现；日志可出现`status_board=input_batch actions=... refreshes=1`。
17. 在自定义输入页点击一个字母，并在电子纸仍在刷新时点击一次左上角返回箭头，确认前一次刷新完成后自动返回一级菜单，无需重复点击。
18. 进入任一预设状态的二级展示页，点击一次左上角箭头或其周围区域，确认页面完成一次局刷后返回一级菜单。

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
