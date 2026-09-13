# 孕期手帐（Pregnancy Journal）

这是运行在 Seeed Studio reTerminal Sticky 上的离线孕期记录固件。当前版本
在保留 Sticky Core 电子纸、触摸、按键、RTC、电量、IMU、蜂鸣器、共享总线和
深度睡眠能力的基础上，提供孕周、提醒、产检、体重和胎动记录。它用于信息
整理，不是诊断、治疗或紧急报警设备。

## 当前版本

- 固件版本：`2.0.0`
- 硬件：ESP32-S3，800×480 黑白电子纸
- 六个应用：孕周（Baby Week）、提醒、产检、体重、数胎动、设置
- 界面语言：English / 简体中文，语言选择保存在设备 NVS 中
- 数据边界：无账号、网络、遥测、云日志、OTA 或 AI 服务，记录仅保存在设备

完整的当前行为、交互和容量限制见[当前功能参考](docs/pregnancy/current-functionality.md)。

## 使用流程

首次进入“孕周”时，依次输入设备时间 `YYYYMMDDHHMM`，再选择并输入确认后的
预产期或末次月经第一天 `YYYYMMDD`。孕周首页显示孕周/孕天、孕期阶段、距离
（或超过）预产期的天数和 40 周进度；“宝宝”和“妈妈”页显示内置的离线周内容。

启动器固定为横屏布局：上方大卡片是孕周，下方卡片是产检、提醒、体重和胎动，
设置位于标题栏。顶部按键单击切换启动器；底部边缘上滑打开、下方 30% 区域下滑
关闭；顶部按键双击返回孕周。启动器右上角可切换中英文。

## 应用功能

| 应用 | 当前实现 | 限制 |
| --- | --- | --- |
| 孕周 | 预产期/LMP 设置、孕周/孕天、阶段、预产期倒计时、40 周进度、宝宝/妈妈内容 | 内容范围 1–40 周；仅供参考 |
| 提醒 | 今天/即将到来列表，六种类型，完成/删除，全局开关 | 最多 12 条，同时显示 4 条；服务层支持每日/每周重复 |
| 产检 | 添加、完成、删除预约 | 最多 8 条，同时显示 4 条；新增默认当前日期 +7 天、09:00 |
| 体重 | kg/lb 记录、BMI、基线增重、最近 7 点趋势 | 最多 32 条；每天 1 条；身高 100–220 cm |
| 数胎动 | 早/中/晚三时段，采样总数、12 小时估算、跨日提示 | 最多 42 个会话；5 秒防重复；会话最长 60 分钟 |
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

当前主机测试为 20/20，release 构建已通过；六应用固件仍需在实机执行
[硬件回归清单](docs/pregnancy/hardware-regression-checklist.md)。

## 文档入口

- [当前功能参考](docs/pregnancy/current-functionality.md)
- [架构](docs/pregnancy/architecture.md)
- [数据模型](docs/pregnancy/data-model.md)
- [孕周计算](docs/pregnancy/pregnancy-calculation.md)
- [提醒与调度](docs/pregnancy/reminder-scheduler.md)
- [存储与隐私](docs/pregnancy/storage.md)
- [电子纸刷新策略](docs/pregnancy/eink-refresh-strategy.md)
- [硬件回归清单](docs/pregnancy/hardware-regression-checklist.md)
- [未来路线图](docs/pregnancy/future-roadmap.md)
