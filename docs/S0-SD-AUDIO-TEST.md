# S0：共享 SPI2 与录音写卡原型

实现日期：2026-09-25。状态：代码原型；实机验收未完成。此原型不表示 S0 整阶段通过。

## 本次范围

- SPI2 初始化归属 `board_shared_spi_init()`，显示与 SDSPI 使用同一总线；仅启动阶段调用，设备不得自行释放总线。
- 独立 `sticky-sd-audio-test` 配置，在正常业务/NVS 初始化之前进入诊断入口。普通 release/debug/power-test 不进入录音原型。
- 先挂载 SD，再初始化显示；禁止自动格式化。挂载失败关闭 SD 电源并显示错误，不写 Flash。
- 顶部键单击后录一次 30 秒 PCM16/16 kHz/单声道 WAV；每次需重启重新录制，无后台录音或联网发送。
- GPIO19=PDM CLK、20=DATA、38=MIC_EN。采音任务优先级 8、写卡任务 5；32×4096 字节 PSRAM 块池（128 KiB），两条队列传递块所有权。溢出直接失败，不丢帧后仍报告成功。
- 写入前检查可用空间至少 960,000+65,536 字节。文件数据逐块写入，检查短写与错误；完成后回填 WAV 头、fsync、close、读回头/长度/CRC32，再重命名。
- SD 路径 `/S0TEST/XXXXXXXX.TMP`（8.3 文件名）；新建使用 `O_EXCL`，已有 WAV 不覆盖。校验成功才改为 `.WAV`；最终文件恰为 **960,044 字节**。失败的 TMP 保留用于排查，不自动删除。
- 主线程约每五秒交替全黑白刷新和局刷；刷新耗时会延长实际刷新间隔。串口报告结果、PCM 字节数、最慢单块写入耗时和文件名，不输出音频内容。
- 工作线程退出、文件关闭后才卸载 SD；不释放共享 SPI2。卸载成功后关闭 SD 电源。

## 明确不包含

这不是 S1 的生产存储服务：没有热插拔重挂载、卡身份/generation 隔离、加密、配额回收、断电事务恢复和业务仓库迁移。卡检测使用轮询，不能保证捕获瞬时拔插；写入中拔卡/断电仍可能损坏 FAT，TMP/rename 不能作为掉电原子性保证。必须使用已备份的测试卡。

不初始化电量计、低电保护或深睡流程；低电试验仅属后续受控硬件验收。无离线命令词、静音检测、语音分类或 Coze 调用。普通业务仍保持原有 NVS 存储，MicroSD 迁移在 S2 实施。

## 构建与操作

工作区含空格，请先按 [开发流程](DEVELOPMENT.md) 镜像到无空格目录。该镜像中执行：

```bash
pio run -e sticky-sd-audio-test
pio device list
pio run -e sticky-sd-audio-test -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor -p /dev/cu.usbmodemXXXX -b 115200 --rts 0 --dtr 0
```

`XXXX` 必须替换成实际串口。首次代码验证未执行烧录；后续实机进度见下方追加记录。

1. 备份测试卡内容；断电后插入已格式化 FAT 测试卡，接 USB。不要在原型中格式化。
2. 出现 `Press TOP once to record` 后才单击顶部键。随后说明页刷新完成才启动采音；不要在刷新期间提前讲话。
3. 讲话约 30 秒，观察全刷/局刷和串口。等待读回验证、卸载后显示 PASS；FAIL 查看日志，保留 TMP。
4. 断电取卡，在电脑验证 WAV 格式、时长、可听性、无明显断音；仅 CRC 相同不能证明麦克风声音正确。
5. 重启后可重复测试，新录音不得覆盖旧文件。结束后烧回正常固件。原型不擦除 NVS。

## 基线与验收记录

当前工具链：espressif32 6.11.0、ESP-IDF 5.4.1、C++17；固件版本仍为 2.0.0，不在原型阶段冒充正式发布。

验证基于提交 `c23dd09` 加本次工作区改动；2026-09-25 最终构建四项全部通过：`sticky-debug`、`sticky-power-test`、`sticky-release`、`sticky-sd-audio-test`。原型静态 RAM 24,232 字节、Flash 4,177,472 字节；这些不包含运行时分配的块池、DMA、任务栈和显示缓冲，不能替代峰值内存实测。

中间构建发现目标工具链不保证 `std::atomic` 无锁，已改用 FreeRTOS ISR 临界区记录 DMA 溢出标志，任务侧消费该标志；修正后重新构建四项通过。Markdown 检查通过（47 文件、114 本地链接），`git diff --check` 通过。

已有 NVS：`sticky_core` 设备设置记录 v1；`pregnancy` 配置记录 v2（内含 profile schema 字段）；`reminders` v1；`checkups`、`weight`、`kicks` v1。本次没有修改记录布局或擦除 NVS。后续迁移以各 storage 源文件校验规则为准。

主机基线 26/26 通过；新增 PCM/WAV 主机测试后 27/27 通过，覆盖头格式、小端编码、空音频头、CRC 标准向量与分块一致性。这些测试不模拟 SD/I2S 时序。

| 实机项目 | 验收证据 | 本次状态 |
| --- | --- | --- |
| 板卡版本/卡型号 | PCB 标识、容量、格式、速度等级 | 待提供 |
| 无卡/挂载失败 | 可显示说明页，无格式化或 Flash 回退 | 待测 |
| 至少三张卡 | 正常卡、慢卡、接近满卡；读写/同步/卸载记录 | 待测 |
| 30 秒正常采音 | 960,044 字节 WAV；16kHz/mono/16bit；试听无断音 | 单卡设备端保存/读回通过；电脑读卡与试听暂缓 |
| 全刷/局刷并发 | 完整录音、无溢出，串口 max_write_ms | 单次录音通过；结束画面根因已修复并实机确认 |
| 空间不足 | 开始前拒绝；原有文件不受影响 | 待测 |
| 插拔/低电/断电 | 使用可丢弃测试卡，失败不报告 PASS；检查 FAT | 待测 |
| 正式固件回归 | 三种配置构建；显示/休眠等硬件清单 | 三种构建通过；硬件待测 |

首次代码验证时未发现 Sticky USB 串口；后续用户连接设备后的进度如下。

### 首次连接与烧录

2026-09-25：检测到 `/dev/cu.usbmodem5C843369951`，烧录器识别 ESP32-S3 revision v0.2。专用 `sticky-sd-audio-test` 烧录成功且各区块哈希校验通过；未擦除 NVS 区域、未格式化 SD。PCB 修订与卡型号仍未确认。

串口已确认 `mount=ESP_OK`、`display=init_done`、`button=ready`，首个全黑白刷新 `elapsed_ms=2030 result=ESP_OK`。

### 首次录音结果与暂缓项

用户按键录音后，串口于 08:24:28（日志原始时间）报告 `result=ESP_OK pcm_bytes=960000 max_write_ms=36 file=/sdcard/S0TEST/F96E7D56.WAV`；随后 `unmount=ESP_OK`，入口返回。按原型成功路径，WAV 头、长度和 PCM CRC 已在设备端读回校验，文件关闭且 SD 卸载成功。此结果仅代表本张卡本次测试，不代表三卡兼容性或声音质量通过。

用户反馈超过 30 秒未看到 PASS/FAIL。末次全刷日志虽返回 ESP_OK（369 ms），不能据此确认物理显示成功；保留“结束画面未更新”缺陷，原因待排查。

用户因暂无读卡器，明确暂缓本次电脑读卡及试听验证，不记为通过。录音保留在卡内 `/S0TEST/F96E7D56.WAV`，本次未删除、导出或上传。后续有读卡器再核对文件并试听；其他硬件验收项仍保持待测。

### 结束画面排查：CS 状态恢复

检查本机 ESP-IDF 5.4.1 `components/esp_driver_sdspi/src/sdspi_host.c` 的 `deinit_slot()`，确认卸载会将 SD CS 配置为输入。原型原先只调用 `gpio_set_level(CS, 1)`，没有恢复输出，因此无法保证卸载后的 SD 片选空闲状态。修复为先预置高电平锁存，再恢复输出，成功后才关闭 SD 电源；任一步失败会返回错误，不再误报完整清理成功。

新增 `board_sd_card_test` 以驱动替身模拟 IDF 卸载行为，检查执行顺序、无卡、重复挂载、卸载失败和方向恢复失败。串口状态日志现在输出实际页面文字与刷新结果，但仍需人工确认物理屏幕。

新增 `sticky-sd-display-test` 专用配置：挂载卡后显示 `BEFORE UNMOUNT`，随后卸载并显示 `AFTER UNMOUNT - CHECK SCREEN`。不启动麦克风、不调用录音文件写入；用于复现结束画面路径，不替代音频测试或完整硬件验收。

2026-09-25 13:18 实机烧录此配置，烧录数据哈希校验通过。串口依次记录 `mount=ESP_OK`、`status="BEFORE UNMOUNT" refresh=ESP_OK`、`display_only unmount=ESP_OK`、`status="AFTER UNMOUNT - CHECK SCREEN" refresh=ESP_OK`；卸载后全刷耗时 1999 ms，入口正常返回。测试没有启动麦克风或创建/修改录音文件。五个配置最终均构建通过；其中 release 首轮因 PlatformIO 包元数据 JSON 临时截断失败，单独重跑后通过，不是代码编译故障。

用户随后肉眼确认电子纸已切换为 `AFTER UNMOUNT - CHECK SCREEN`。因此“SD 卸载后结束画面不更新”缺陷关闭：根因是 ESP-IDF 卸载将 SD CS 改为输入，而原型未恢复输出。本次显示诊断通过不改变电脑读卡试听、多卡、低电和插拔项目的待测状态。

硬件依据见 [本地硬件资料](HARDWARE-REFERENCE.md)。
