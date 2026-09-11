# Sticky Core Framework

这是 Seeed Studio reTerminal Sticky 的精简固件框架，保留电子纸、触摸、按键、RTC、电量、IMU、蜂鸣器、共享总线和深度睡眠能力。

当前应用：主页、设置、孕周。设置页支持中英文切换、PCF8563 校时和电子纸清洁全刷；孕周应用保留首次设置设备时间/预产期和孕周进度展示。

## 已验证基线

固件版本 `1.0.0`，基线提交 `fdc6be6`。2026-09-12 已在 Seeed Studio reTerminal Sticky 实机完成构建、烧录和功能验证：14/14 项主机测试通过，主页、设置、孕周三个应用及中英文切换、校时、清屏、睡眠唤醒均验证正常。串口启动日志确认 RTC、电池、SSD1677 电子纸、GT911 触摸、IMU 和按键初始化成功。

release 固件占用 427,576 B Flash、16,916 B RAM，生成的 `firmware.bin` 为 428,240 B。

构建与烧录命令、架构、迁移记录和硬件检查表见 [`README.md`](README.md) 与 [`docs/refactor`](docs/refactor)。
