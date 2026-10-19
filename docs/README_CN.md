# 孕期手帐（Pregnancy Journal）

孕期手帐是运行在 Seeed Studio reTerminal Sticky（一台小型磁吸电子纸设备）上的离线
孕期记录固件。它在 Sticky Core 硬件框架上提供孕周首页、体重记录、数胎动和设置。
它只用于信息整理，不是诊断、治疗或紧急报警设备：所有与健康相关的数值都只是个人
记录和与医护沟通的提示，不构成医疗建议。

[English](../README.md) | **简体中文** | [日本語](README_JA.md)

## 当前状态

- 硬件：ESP32-S3 reTerminal Sticky，3.97 英寸 800×480 黑白电子纸；固件版本 `2.0.0`。
- 四个已注册应用：**孕周（Baby Week）**、**体重**、**胎动**、**设置**。没有提醒、产检、
  习惯、备忘、录音、配网和语音转文本。
- 界面语言：English / 简体中文。所有用户数据都存放在 MicroSD 卡上；没有账号、
  遥测、云上传、OTA 或 AI 服务。
- 这是尚未通过验收的固件候选。自动门槛是主机测试套件（`tools/run_host_tests.sh`，
  2026-10-19 为 77/77 步）加上 `sticky-release`、`sticky-debug`、`sticky-power-test`
  三个配置编译通过。实机验收另行记录在 [V1 验证记录](V1-VALIDATION.md) 和
  [硬件回归清单](pregnancy/hardware-regression-checklist.md)，本文不声称已通过实机验收。

完整的当前行为、交互和容量限制见[当前功能参考](pregnancy/current-functionality.md)。

## 应用功能

| 应用 | 当前实现 | 限制 |
| --- | --- | --- |
| 孕周 | 首页是“图版”：第 6–40 周每周一张雕版风格插画、周图网格和当前周数；宝宝页、妈妈页显示孕周/孕天、孕期阶段、距预产期天数和离线内容 | 内容范围 1–40 周；仅供参考 |
| 体重 | kg/lb，精度 0.1；基线与身高；7/30 日图表；历史编辑与删除 | 最多 366 条，每天一条；替换和删除都需确认 |
| 胎动 | 三种记录方式：1 小时计数、数到 10、感觉打卡；记住上次选的方式；每条记录与自己过去 7 天的中位数比较（只给出差值方向和大小） | 最多 400 条；可撤销；400 ms 防误触；单次最长 60 分钟 |
| 设置 | 语言、设备时间、孕期设置、双击快捷键、电子纸清屏、MicroSD 页（状态、创建数据集、安全弹出、旧数据导入、删除卡内数据） | 时间编辑时禁止休眠；创建数据集需两次明确操作 |

首页上点插画进入宝宝页，点周数进入妈妈页。首页不再显示进度条和倒计时，这些数字在
宝宝页和妈妈页里。

## 首次使用

首次进入“孕周”时，依次输入设备时间 `YYYYMMDDHHMM`，再选择并输入医护确认的预产期，
或末次月经第一天 `YYYYMMDD`。档案只保存在 MicroSD 卡上，之后可在 设置 → 孕期设置 中
修改。没有卡时应用会说明无法读取的原因，不会回退到 Flash。

## 按键与导航

| 操作 | 结果 |
| --- | --- |
| 顶键单击 | 任意页 → 孕周首页 → 功能列表 → 关闭（回到打开前的页面）；在胎动计数页是记一次胎动 |
| 顶键双击 | 执行“设置 → 快捷键”选定的目标：记录今日体重（默认）、孕周首页、开始数胎动、或关闭 |
| 侧键单按 | 切到上一个/下一个应用（沿功能列表顺序）；有未保存输入或正在计数的页面忽略侧键 |
| 两个侧键同按 2 秒 | 请求深度睡眠 |
| 从底边上滑 | 打开功能列表 |
| 在下方 30% 区域下滑 | 关闭功能列表 |
| 沿底边的长平滑左/右滑（功能列表关闭时） | 不打开列表，直接切到上一个/下一个应用，首尾循环 |
| 点功能列表条目 | 打开孕周、体重、胎动或设置 |

无操作 30 秒后功能列表自动关闭。画面方向跟随 IMU 报告的握持方向：功能列表横屏两列、
竖屏单列，每个应用都有专门的横屏和竖屏页面。页头按钮含义固定：**APPS**（离开应用回
功能列表）、**BACK**（返回上一级）、**CANCEL**（放弃，不保存）、**HOME**（回孕周首页），
详见[页面与导航结构](PAGE-STRUCTURE.md)。

## 睡眠与唤醒

无外接电源时，稳定页面在 60 秒无操作后进入深度睡眠；设置页为 3 分钟；编辑页和正在
进行的胎动计数不会超时休眠。顶键可唤醒。睡眠前协调器会安排每天 03:00 的维护唤醒和
当前应用请求的事件，定时器最多提前 15 秒启动。当前版本没有业务提醒定时唤醒和闹铃。

## 电子纸刷新

产品页面只用黑白，以 1-bit 帧送屏（全刷约 1.9 s，局刷约 0.9 s）。换页使用全刷，例外如下：

- 打开功能列表、回到应用、换应用时，第一帧用快速（局刷）波形，每连续 5 次后第 6 次全刷清屏。
- 孕周首页与宝宝页互相切换用局刷，空闲 5 秒后补一次全刷清残影。
- 显示层在连续 10 次局刷后强制一次全刷。
- 进入体重或胎动时，读卡在 1.5 秒内完成就直接画内容页，不出“加载中”页；读卡更慢才补画；
  孕周已持有档案时，进入后立即画首页并在后台静默校验。

方向变化和睡眠帧始终用全刷。规则见 [DESIGN.md](DESIGN.md) 和
[电子纸刷新策略](pregnancy/eink-refresh-strategy.md)。

## 存储与隐私

四个应用都使用绑定数据集、带日志的 MicroSD repository，不回退到 Flash。设置提供经校验的
旧数据导入（胎动有独立路径）、安全弹出和卡内应用数据删除。恢复索引最多容纳 1,024 个键（含已删除键）；日志和对象的垃圾回收尚未实现，
索引满时拒绝新键而不是丢弃记录。见[存储](pregnancy/storage.md)、
[数据模型](pregnancy/data-model.md)和[备份与恢复](S1-BACKUP-RECOVERY.md)。

## 仓库结构

| 路径 | 内容 |
| --- | --- |
| `src/app/` | 应用注册表、协调器（功能列表、应用切换、按键路由、睡眠） |
| `src/apps/` | 孕周（`pregnancy`）、`weight`、`kick`、`settings` |
| `src/pregnancy/` | 领域服务、模型和离线内容 |
| `src/ui/` | 画布、字体、设计令牌、公共页面、生成的资源 |
| `src/storage/` | MicroSD 服务、日志、索引和 repository |
| `src/display/`、`src/board/`、`src/devices/`、`src/input/`、`src/sensors/` | 显示所有者与硬件访问 |
| `components/` | GT911 触摸与 SSD1677 电子纸驱动 |
| `test/`、`tools/` | 主机测试与工具（见下） |
| `docs/` | 需求、设计、验证记录与证据 |

架构为 `应用 → 领域/内容服务 → Sticky Core 运行时 → 设备 HAL`。`app_registry.cpp` 是组合根，
`StickyAppManager` 负责应用生命周期以及显示/输入所有权。见[架构总览](ARCHITECTURE.md)和
[新增应用](refactor/adding-an-app.md)。

## 构建与测试

需要 PlatformIO 和 ESP-IDF 5.4.1。项目路径含空格，PlatformIO 会拒绝直接构建，请使用无空格
副本或精确镜像（见[开发流程](DEVELOPMENT.md)）：

```bash
pio run -e sticky-release
pio run -e sticky-debug
pio run -e sticky-power-test
tools/run_host_tests.sh
python3 tools/check_markdown_links.py
```

目标板为 `sticky_esp32s3`，使用自定义 `partitions.csv`（含 128 KiB `coredump` 分区，可用
`idf.py coredump-info` 读取崩溃信息）。`sticky-release` 另外套用 `sdkconfig.release.defaults`
（-O2、静默断言、INFO 日志上限）。`platformio.ini` 里的其他环境是独立的硬件测试（字形覆盖、
存储断电与故障注入），不是产品构建。

常用工具：`tools/ui_preview/render_all.sh` 把每个页面渲染成 1-bit PNG，并输出命中区叠加图和
检查结果；`tools/check_ui_tokens.py` 检查字号与边距令牌；`tools/validate_sd_backup.py` 校验复制出的
`PREGJNL` 数据集。

## 烧录

```bash
pio device list
pio run -e sticky-release -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor -p /dev/cu.usbmodemXXXX -b 115200 --filter time --rts 0 --dtr 0
```

把端口换成 `pio device list` 显示的设备。如果自动进入 bootloader 失败，按住 **BOOT**，点按
**RESET**，松开 **BOOT**，再执行一次烧录命令。烧录后请做
[硬件回归清单](pregnancy/hardware-regression-checklist.md)。

## 文档入口

**产品与状态**
- [当前功能参考](pregnancy/current-functionality.md)
- [首版四应用需求与验收](PROJECT-SPEC.md)
- [当前任务](TODO.md)和[变更记录](CHANGELOG.md)
- [V1 验证记录](V1-VALIDATION.md)、[实机结果](V1-HARDWARE-RESULTS.md)、
  [人工测试清单](V1-MANUAL-TEST-CHECKLIST.md)和
  [硬件回归清单](pregnancy/hardware-regression-checklist.md)

**架构与开发**
- [架构总览](ARCHITECTURE.md)和[孕周架构](pregnancy/architecture.md)
- [开发流程](DEVELOPMENT.md)、[组件开发规范](COMPONENT-GUIDELINES.md)和
  [贡献指南](CONTRIBUTING.md)
- [协作与代理指南](AGENTS.md)
- [reTerminal Sticky 硬件资料](HARDWARE-REFERENCE.md)

**界面与交互**
- [电子纸产品设计规范](DESIGN.md)
- [页面与导航结构](PAGE-STRUCTURE.md)
- [按键与快捷键设计](pregnancy/key-shortcuts-design.md)
- [UI 原型实现对照](pregnancy/ui-prototype-mapping.md)
- [电子纸刷新策略](pregnancy/eink-refresh-strategy.md)和[UI 审计](ui-audit.md)

**数据与存储**
- [存储](pregnancy/storage.md)和[数据模型](pregnancy/data-model.md)
- [孕周计算](pregnancy/pregnancy-calculation.md)
- [日志格式](S1-JOURNAL-FORMAT.md)、[索引快照](S1-INDEX-SNAPSHOT.md)和
  [备份与恢复](S1-BACKUP-RECOVERY.md)

许可与上游署名见 [../LICENSE](../LICENSE) 和 `third_party/fonts/NotoSansCJK` 下的说明。孕周插画的许可
记录在 [../assets/pregnancy/week-plates/README.md](../assets/pregnancy/week-plates/README.md)。

## 致谢

本项目最初基于 [limengdu/reTerminal_Sticky_Bunny](https://github.com/limengdu/reTerminal_Sticky_Bunny)
（MIT）搭建，并继续沿用它的硬件底层：SSD1677/UC8179 电子纸与 GT911 触摸组件、板级供电与共享 SPI、
显示所有者、IMU/RTC/电池/蜂鸣器驱动、按键与触摸输入、Canvas 绘图层和启动流程。应用框架、四个应用、
MicroSD 日志存储和测试是在此基础上为本项目编写的。
