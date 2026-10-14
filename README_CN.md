# 孕期手帐（Pregnancy Journal）

这是运行在 Seeed Studio reTerminal Sticky 上的离线孕期记录固件。当前版本
在保留 Sticky Core 电子纸、触摸、按键、RTC、电量、IMU、蜂鸣器、共享总线和
深度睡眠能力的基础上，提供孕周、体重、胎动记录和设置。它用于信息
整理，不是诊断、治疗或紧急报警设备。

## 当前交付计划

2026-10-10 已确认：首版先完成**体重、胎动、设置、孕周**四个应用。提醒、产检与
到期提醒、习惯、备忘、录音/分类、配网和语音转文本移到下一版本。用户数据使用
MicroSD 的决策继续生效，胎动 SD 存储和独立旧记录迁移已接入候选固件。

范围和验收见[首版需求](docs/PROJECT-SPEC.md)、[当前任务](TODO.md)；下一版本设计
见 [V2 扩展需求](docs/V2-PROJECT-SPEC.md)。四应用候选代码已完成，尚未通过实机验收；
自动验证、固件校验值及人工交接见 [V1 验证记录](docs/V1-VALIDATION.md)。

## 当前版本

- 固件版本：`2.0.0`
- 硬件：ESP32-S3，800×480 黑白电子纸
- 四个应用：孕周（Baby Week）、体重、胎动、设置
- 界面语言：English / 简体中文，语言选择保存在当前 MicroSD 数据集中
- 数据边界：孕期档案、体重、胎动和偏好使用 MicroSD repository，不回退到 Flash。
  胎动有独立迁移与清除确认；提醒/产检原副本保留。当前没有云上传或 AI 转写路径

完整的当前行为、交互和容量限制见[当前功能参考](docs/pregnancy/current-functionality.md)。

## 使用流程

首次进入“孕周”时，依次输入设备时间 `YYYYMMDDHHMM`，再选择并输入确认后的
预产期或末次月经第一天 `YYYYMMDD`。孕周首页显示孕周/孕天、孕期阶段、距离
（或超过）预产期的天数和 40 周进度；“宝宝”和“妈妈”页显示内置的离线周内容。

启动器横屏两列、竖屏单列，仅展示四应用。顶部按键单击切换启动器；底部边缘
上滑打开、下方 30% 区域下滑关闭；顶部按键双击返回孕周。语言在设置中修改。
旧 last-app 为提醒/产检时回到孕周，持久 ID 不变，业务定时唤醒与闹铃已停用。

## 应用功能

| 应用 | 当前实现 | 限制 |
| --- | --- | --- |
| 孕周 | 预产期/LMP 设置、孕周/孕天、阶段、预产期倒计时、40 周进度、宝宝/妈妈内容 | 内容范围 1–40 周；仅供参考 |
| 体重 | kg/lb、基线变化、7/30 日历趋势、历史编辑/删除、身高/基线 | 最多 366 条；每天一条；同日替换和删除均需确认 |
| 数胎动 | 早/中/晚三时段、今日汇总和最近 7 天记录趋势 | 最多 42 个会话；400 ms 防误触；会话最长 60 分钟 |
| 设置 | 语言、RTC 时间、孕期设置、电子纸清屏、返回孕周 | 时间编辑时禁止休眠 |

体重和胎动提示是记录辅助，不替代专业医护判断。

## 构建、烧录与验证

需要 PlatformIO、ESP-IDF 5.4.1。由于项目路径含空格，PlatformIO 可能拒绝直接
构建，请使用无空格副本或临时镜像：

```bash
pio run -e sticky-release
pio run -e sticky-debug
pio run -e sticky-power-test
tools/run_host_tests.sh
```

烧录前用 `pio device list` 找到串口，再执行：

```bash
pio run -e sticky-release -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor -p /dev/cu.usbmodemXXXX -b 115200 --filter time --rts 0 --dtr 0
```

自动结果见 [V1 验证记录](docs/V1-VALIDATION.md)。release、debug、power-test 构建
作为发布门槛；四应用候选仍需在实机执行
[硬件回归清单](docs/pregnancy/hardware-regression-checklist.md)。

## 文档入口

- [首版四应用需求与验收（内存卡存储）](docs/PROJECT-SPEC.md)
- [V2 提醒、语音转文本及联网扩展需求](docs/V2-PROJECT-SPEC.md)
- [开发方案与任务计划](TODO.md)
- [协作与代理指南](AGENTS.md)
- [电子纸产品设计规范](DESIGN.md)
- [架构总览](docs/ARCHITECTURE.md)
- [组件开发规范](docs/COMPONENT-GUIDELINES.md)
- [页面与导航结构](docs/PAGE-STRUCTURE.md)
- [开发流程](docs/DEVELOPMENT.md)
- [reTerminal Sticky 硬件资料](docs/HARDWARE-REFERENCE.md)
- [当前功能参考](docs/pregnancy/current-functionality.md)
- [架构](docs/pregnancy/architecture.md)
- [数据模型](docs/pregnancy/data-model.md)
- [孕周计算](docs/pregnancy/pregnancy-calculation.md)
- [提醒与调度](docs/pregnancy/reminder-scheduler.md)
- [存储与隐私](docs/pregnancy/storage.md)
- [电子纸刷新策略](docs/pregnancy/eink-refresh-strategy.md)
- [硬件回归清单](docs/pregnancy/hardware-regression-checklist.md)
- [未来路线图](docs/pregnancy/future-roadmap.md)
