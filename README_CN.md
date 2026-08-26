# reTerminal Sticky Bunny

<p align="center">
  <strong>为 reTerminal Sticky 打造的成长型桌宠与交互式电子纸应用合集。</strong>
</p>

<p align="center">
  <a href="README.md">English</a> ·
  <a href="docs/wiki/Getting-Started.md">快速开始</a> ·
  <a href="docs/wiki/Home.md">完整文档</a> ·
  <a href="CHANGELOG.md">更新日志</a>
</p>

<p align="center">
  <img alt="ESP32-S3" src="https://img.shields.io/badge/MCU-ESP32--S3-000000?style=flat-square">
  <img alt="ESP-IDF 5.4.1" src="https://img.shields.io/badge/ESP--IDF-5.4.1-000000?style=flat-square">
  <img alt="PlatformIO" src="https://img.shields.io/badge/build-PlatformIO-000000?style=flat-square">
  <img alt="Firmware 0.1.0" src="https://img.shields.io/badge/firmware-0.1.0-000000?style=flat-square">
  <img alt="MIT License" src="https://img.shields.io/badge/license-MIT-000000?style=flat-square">
  <a href="https://github.com/limengdu/reTerminal_Sticky_Bunny/actions/workflows/build.yml"><img alt="Build and test" src="https://github.com/limengdu/reTerminal_Sticky_Bunny/actions/workflows/build.yml/badge.svg"></a>
</p>

<p align="center">
  <img src="docs/images/desktop-pet/home.png" alt="Sticky Bunny 桌宠主页" width="245">
  &nbsp;&nbsp;
  <img src="docs/images/launcher/launcher-portrait.png" alt="Sticky Bunny 竖屏应用选择器" width="245">
</p>

reTerminal Sticky Bunny 把 Seeed Studio **reTerminal Sticky** 变成一个持续存在的小世界：养育一只会记住照料过程的兔子，开始一次番茄专注，展示当前状态，或者向答案书提出心中的问题。触摸、实体按键、上下滑动、设备旋转、持续摇晃、RTC 定时和电子纸低功耗策略被整合成一套连贯的固件体验。

仓库包含完整 PlatformIO/ESP-IDF 源码、原创黑白像素素材、可在电脑运行的状态与渲染测试、发布文件生成方式，以及面向用户和开发者的说明文档。

## 项目亮点

- **桌宠不是静态吉祥物。** 它会破壳、经历五个成长阶段、形成三种性格之一，并持续记住用户的照料、对话、睡眠和外出。
- **四个统一设计的设备端应用。** 桌宠、番茄钟、状态板和答案书共享同一个启动器与视觉语言。
- **设备本身就是控制器。** 用户可以用按键、触摸、滑动、旋转和摇晃选择功能。
- **按照电子纸特性设计。** 稳定页面重视清晰度，倒计时和动画使用受控局刷，刷新过程中仍继续接收输入。
- **围绕真实硬件实现。** 固件管理显示屏与 SD 卡共享 SPI、GT911 触摸、LSM6DS3TR-C IMU、PCF8563 RTC、BQ27220 电量计、蜂鸣器、侧边按键、电池供电和深度睡眠。

## 四个应用

| 成长型桌宠 | Sticky Pomodoro Timer |
| --- | --- |
| <img src="docs/images/desktop-pet/home.png" alt="显示成长、亲密、饱腹和能量的兔子主页" width="330"> | <img src="docs/images/pomodoro/setup.png" alt="番茄钟时间选择页面" width="330"> |
| 孵化、命名、喂食、抚摸、聊天和陪玩都会影响兔子的成长、性格和对白。 | 支持 15、25、60 分钟与自定义时间，包含暂停、继续、结束确认和柔和循环提示音。 |

| Sticky Status Board | Book of Answers |
| --- | --- |
| <img src="docs/images/status-board/menu.png" alt="横屏状态板菜单" width="390"> | <img src="docs/images/book-of-answers/home.png" alt="答案书主页" width="245"> |
| 横屏展示 `BUSY`、`MEETING`、`ON CALL`、`OPEN TO TALK`、`REST` 或设备端输入的自定义状态。 | 心中默念问题并持续摇晃三秒，获得一句话答案，或 `YES`、`NO`、`UNCLEAR`。 |

## 桌宠成长系统

桌宠是固件的默认主页，也是整个项目的情感核心。真实时间由 RTC 推进，存档使用两个带校验值的 NVS 槽位轮换保存；最新存档损坏时，可以回退到另一个有效槽位。

<p align="center">
  <img src="docs/images/desktop-pet/growth-lineage.png" alt="从宠物蛋到三种成年性格的成长路线" width="640">
</p>

成长阶段包括：

1. `EGG`：连续三次明确点击完成孵化。
2. `HATCHLING`：建立最初的照料关系。
3. `CHILD`：喂食、抚摸和玩耍开始积累不同的性格倾向。
4. `YOUTH`：形成 `FOODIE`、`AFFECTIONATE` 或 `ACTIVE` 性格。
5. `ADULT`：三种路线拥有不同外形、互动动作、台词和外出纪念品。

成长值、亲密值、饱腹值、能量、心情、连续照料天数、最近动作、最近台词、性格证据和计划外出时间都会跨重启保存。完整规则见 [桌宠成长系统](docs/wiki/Pet-Growth-System.md)。

## 应用选择与设备交互

<p align="center">
  <img src="docs/images/launcher/launcher-landscape.png" alt="横屏应用选择器" width="700">
</p>

- 单击 AI 键或从屏幕底部上滑：打开应用选择器。
- 点击 APP 卡片：直接进入对应应用。
- 从屏幕上半部分下滑：关闭应用选择器。
- 在任意页面双击 AI 键：返回桌宠。
- 应用选择器打开时，从横屏旋转到竖屏：进入番茄钟。
- 应用选择器打开时，从竖屏旋转到横屏：进入状态板。
- 应用选择器打开时持续摇晃：进入答案书。
- 同时按住两个非 AI 侧键：进入深度睡眠。

旋转路由会等待设备停止移动并确认最终稳定方向；一旦检测到合格的持续摇晃，会优先锁定答案书。详细状态和阈值见 [应用选择器与手势](docs/wiki/App-Launcher-and-Gestures.md)。

## 电子纸、低功耗与 RTC

电子纸断电后仍会保留图像，因此固件不会把它当成普通液晶屏反复刷新。静态页面负责建立干净画面，倒计时数字和动作区域采用范围受控的局部刷新，并通过周期性高质量刷新恢复对比度。

桌宠在进入睡眠前会安排下一项有意义的自主事件。PCF8563 RTC 可以在外出或其他计划事件前唤醒设备，而不是每隔固定时间盲目唤醒。电池百分比来自 BQ27220 电量计，充电、电量和睡眠标志会根据页面留白自动选择位置。

进一步阅读：[低功耗与 RTC](docs/wiki/Power-and-RTC.md)、[硬件与驱动](docs/wiki/Hardware-and-Drivers.md)。

## 快速开始

### 准备

- reTerminal Sticky
- 支持数据传输的 USB-C 线
- PlatformIO Core 6.1 或 PlatformIO IDE
- Python 3
- macOS、Linux 或 Windows

工程固定使用 `espressif32@6.11.0` 和 ESP-IDF 5.4.1。

### 获取并编译

```bash
git clone https://github.com/limengdu/reTerminal_Sticky_Bunny.git
cd reTerminal_Sticky_Bunny
pio run
```

默认环境为 `sticky-release`，编译结果位于：

```text
.pio/build/sticky-release/firmware.bin
```

### 烧录正式版

```bash
pio run -e sticky-release -t upload
```

需要详细日志时使用：

```bash
pio run -e sticky-debug -t upload
pio device monitor -e sticky-debug
```

串口波特率为 `115200`。下载模式恢复、完整固件烧录、NVS 清空和首次启动现象见 [快速开始](docs/wiki/Getting-Started.md)。

## 构建环境

| 环境 | 用途 | 运行规则 | 日志 |
| --- | --- | --- | --- |
| `sticky-release` | 日常使用与发布 | 正式时间、成长上限和外出计划 | 必要警告与生命周期信息 |
| `sticky-debug` | 硬件和交互排查 | 保持正式玩法规则 | 详细应用、输入和存档日志 |
| `sticky-power-test` | 加速验证睡眠和唤醒 | 仅缩短功耗测试时间 | 功耗专项诊断 |

发布前同时构建三种环境：

```bash
pio run -e sticky-release -e sticky-debug -e sticky-power-test
```

## 工程结构

```text
src/
├── app/                 # 应用管理器、选择器、路由和生命周期
├── apps/                # 桌宠、番茄钟、状态板、答案书和教程
├── board/               # 供电、充电、共享总线与引脚配置
├── devices/             # 电池、RTC 与蜂鸣器驱动
├── display/             # 电子纸所有权与刷新操作
├── input/               # 实体按键与 GT911 触摸队列
├── sensors/             # IMU 姿态与持续摇晃会话
└── ui/                  # 画布、字体、电量叠层和像素素材

assets/                  # 原始素材、固件图片与视觉验收图
docs/wiki/               # 用户与开发者文档
test/                    # 状态、策略和页面渲染测试
tools/                   # 可重复运行的素材与数据库生成工具
third_party/             # 上游开源方案的许可证与使用说明
```

程序从 `app_main()` 到硬件初始化、首次教程、应用所有权、电子纸刷新和深度睡眠的完整顺序见 [固件架构](docs/wiki/Firmware-Architecture.md)。

## 测试

无需设备即可在电脑上验证状态机和页面渲染，包括：

- 桌宠成长、离线时间、对白、动作调度和双槽存档；
- 番茄钟输入、倒计时策略、页面布局和响铃状态；
- 状态选择、自定义文字和每种状态的兔子动画；
- 答案选择、三秒持续摇晃门槛和结果页面；
- 应用选择器触摸区、滑动手势、旋转路由和功耗策略；
- 首次教程导航和六张最终页面。

```bash
./tools/run_host_tests.sh
python3 tools/check_markdown_links.py
```

完整命令与输出文件见 [测试与调试](docs/wiki/Testing-and-Debugging.md)，真机发布检查见 [烧录与发布](docs/wiki/Flashing-and-Releases.md)。

## 文档入口

| 文档 | 内容 |
| --- | --- |
| [文档主页](docs/wiki/Home.md) | 用户和开发者文档总入口 |
| [快速开始](docs/wiki/Getting-Started.md) | 编译、烧录与首次启动 |
| [桌宠使用说明](docs/wiki/Desktop-Pet.md) | 日常互动与可见行为 |
| [桌宠成长系统](docs/wiki/Pet-Growth-System.md) | 阶段、性格、数值和存档 |
| [番茄钟](docs/wiki/Pomodoro-Timer.md) | 预设、自定义时间、倒计时和提示音 |
| [状态板](docs/wiki/Status-Board.md) | 预设状态与自定义横屏展示 |
| [答案书](docs/wiki/Book-of-Answers.md) | 一句话与水晶球答案模式 |
| [应用选择器与手势](docs/wiki/App-Launcher-and-Gestures.md) | 触摸、按键、旋转和摇晃路由 |
| [低功耗与 RTC](docs/wiki/Power-and-RTC.md) | 睡眠策略、计划事件和电池显示 |
| [固件架构](docs/wiki/Firmware-Architecture.md) | 模块、所有权和运行顺序 |
| [素材生成](docs/wiki/Asset-Pipeline.md) | 原图、固件位图和生成工具 |
| [测试与调试](docs/wiki/Testing-and-Debugging.md) | 原生测试、日志和视觉验收 |
| [常见问题](docs/wiki/Troubleshooting.md) | 构建、烧录、显示、触摸与 RTC 排查 |

## 设计演进

项目经历了多轮真机测试和电子纸 UI 调整。Wiki 将部分概念图与代码实际渲染页面并列保存，让后续开发者了解视觉体系，同时明确区分探索方案和正式效果。参见 [设计与素材画廊](docs/wiki/Design-and-Asset-Gallery.md)。

## 参与贡献

欢迎提交 Issue 和 Pull Request。请先阅读 [CONTRIBUTING.md](CONTRIBUTING.md)，硬件行为应能追溯到 Sticky 参考实现，行为改动应增加电脑端回归测试，需要真机才能验证的改动应附带明确测试步骤。

## 许可与致谢

Sticky Bunny 使用 [MIT License](LICENSE) 发布。

桌宠核心借鉴了 TamaPoke、esp32-artoria-tamagotchi、openclaw-tamagotchi 和 ESP32-TamaPetchi 中少量 MIT 许可思路。完整许可证与审查提交记录位于 [third_party/virtual_pet/NOTICE.md](third_party/virtual_pet/NOTICE.md)。仓库中的兔子美术为本项目原创。

答案书信息数据库来自 Apache License 2.0 授权的 `DBinK/The-Book-of-Answers-Interpreter`，完整许可文本与数据库源文件一起保存。

reTerminal Sticky 是 Seeed Studio 的产品。本仓库是社区固件项目，不代表设备出厂固件。
