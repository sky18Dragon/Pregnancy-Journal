# S0 语音分类与资源预算进度记录

记录日期：2026-09-25  
状态：模型打包、刷写、四类冒烟、深睡唤醒和 30 秒超时通过；完整
S0-05～S0-07 验收未完成。

## 1. 当前结论

- ESP-SR 依赖固定为 `~2.4.7`，独立配置
  `sticky-voice-classifier-test` 使用 ESP32-S3 中文 MultiNet5 Q8 模型。
- 已实现四类稳定意图 ID：体重、产检、习惯、备忘；每类配置“XX记录”和
  “记录XX”两种拼音命令。模型候选须同时满足概率不低于 `0.60`、与次选差值
  不低于 `0.12`，否则返回“需要手动选择”。这仍是待样本标定的保守门槛。
- 按键层提供 `LongPressStart` / `LongPressEnd`。独立原型使用 600 ms 长按门槛，
  长按开始采音、松开结束；正式配置仍保留原 2000 ms 门槛。
- 原型直接把 16 kHz、PCM16、单声道 PDM 数据送入 MultiNet，不写 MicroSD、
  不联网、不保存音频。是否需要增加 AFE，由后续多说话人和噪声样本决定。
- 模型初始化、命令注册和真实麦克风识别链路已经跑通。四类工程样本均命中正确
  类别；只有“记录备忘”以 `0.6758` 超过产品门槛并自动接受，其余样本安全回退
  人工分类。静音和“今天天气不错”均未产生候选。不能据单人少量样本降低门槛
  或宣称准确率达标。

## 2. 模型打包与分区闭环

`tools/platformio_voice_model.py` 负责 PlatformIO 构建与上传闭环：

1. 从 `partitions.voice.csv` 解析应用与 `model` 分区，不复制硬编码地址。
2. 调用 ESP-SR 的 `movemodel.py`，使用当前环境解析后的 sdkconfig 生成
   `srmodels.bin`。
3. 将模型输入、ESP-SR manifest、模型脚本、语音 sdkconfig 和分区表纳入依赖。
4. 构建后检查模型尺寸、应用/模型边界和 32 MiB Flash 上界。
5. 把模型镜像按分区表地址追加到 PlatformIO 最终上传参数。

已验证的镜像布局：

| 镜像 | 地址/分区 | 实测 |
| --- | --- | --- |
| bootloader | `0x00000000` | 上传后哈希通过 |
| partition table | `0x00008000` | 启动日志解析出的表与 CSV 一致 |
| factory app | `0x00010000`，8 MiB | 4,300,636 字节，未越界 |
| `srmodels.bin` | `0x00810000`，`0x50C000` | 2,203,819 字节，上传后哈希通过 |

应用分区结束地址恰为模型起始地址 `0x810000`。自动校验会在应用越界、模型
越界、地址重叠、Flash 越界或 PlatformIO 应用偏移与 CSV 不一致时终止构建。
顶层 `CMakeLists.txt` 同时为原生 ESP-IDF 构建补充 ELF 对 `srmodels_bin` 的依赖。

## 3. 2026-09-25 真机证据

设备：Seeed Studio reTerminal Sticky，ESP32-S3 rev v0.2，32 MiB Flash、8 MiB
PSRAM；串口 `/dev/cu.usbmodem5C843369951`。刷写后无复位循环或看门狗错误。

### 启动与内存

- 分区表识别：factory `0x10000/0x800000`，model `0x810000/0x50C000`。
- `MODEL_LOADER` 报告模型分区 5,168 KiB，并成功加载 `srmodels`。
- 模型：`mn5q8_cn`，16 kHz，512 samples/chunk；8 条命令注册成功。
- 模型前：internal free 336,475 B，PSRAM free 8,145,100 B。
- 模型与命令后：internal free 298,723 B，internal minimum 287,103 B；PSRAM free
  与 minimum 均为 5,762,960 B。
- 屏幕在启动后完成全刷并显示 `READY`。

### 长按、采音和分类

| 次数 | 采音准备 | 录音时长 | 命中 | 概率 | 产品决策 |
| --- | ---: | ---: | --- | ---: | --- |
| 1 | 22 ms | 4,982 ms | WEIGHT | 0.5621 | 人工分类 |
| 2 | 22 ms | 2,400 ms | WEIGHT | 0.5253 | 人工分类 |
| 3 | 22 ms | 4,407 ms | WEIGHT | 0.5776 | 人工分类 |
| 4 | 22 ms | 2,656 ms | CHECKUP | 0.5369 | 人工分类 |
| 5 | 22 ms | 2,252 ms | HABIT | 0.5788 | 人工分类 |
| 6 | 22 ms | 2,252 ms | MEMO | 0.6758 | 自动接受 |
| 7（深睡唤醒后） | 22 ms | 2,802 ms | MEMO | 0.8368 | 自动接受 |

- 快速点按只产生短按结果，显示 `PRESS TOO SHORT`，没有开始采音。
- 每次长按均在 600 ms 门槛后产生开始事件，松开后 `released=true` 结束。
- 六次长按后均未观察到额外单击/双击事件。
- 麦克风与模型准备耗时从函数入口实测，不再使用固定估算值。
- 静音 3,582 ms 和无关语句“今天天气不错”3,692 ms 均为 `UNKNOWN`，候选概率
  `0.0000`，显示 `MANUAL CATEGORY REQUIRED`。
- 前五条命令词因低于门槛显示 `MANUAL CATEGORY REQUIRED`；备忘显示 `MEMO`。
- 双侧键持续 2 秒后显示 `SLEEPING`，等待侧键释放，再关闭电子纸并进入深睡；
  顶部键产生 `reset=deep_sleep wake=ext1`。唤醒后模型重新加载，无复位循环或
  看门狗，屏幕 `READY AFTER WAKE` 已由用户肉眼确认。
- 唤醒后的“记录备忘”正确命中 `MEMO`，概率 `0.8368`，证明模型与麦克风恢复
  后可继续使用。
- 长按静音达到上限时在 30,065 ms 自动结束，`timeout=true`、`released=false`、
  937 个采样块、零候选；随后松开只产生 `LongPressEnd`，未卡死或开始第二次录音。

以上只证明工程链路可运行，不满足 A03 的统计样本量，也未验证多人重复样本、
含混语句、背景噪声以及未接 USB 时的深睡电气状态。

## 4. 已完成的主机与资源工作

- 当前仓库主机测试通过：`34/34`。覆盖意图 ID、低置信度、含混候选、非法候选、
  录音配额边界及离线拼音索引。
- 存储合同：4 个打开文件、8 深度单写队列、64 字节物理路径、200 个 Unicode
  字符对应最多 800 UTF-8 字节；音频上限 100 条或 128 MiB，业务数据预留
  32 MiB，单条 30 秒 WAV 预留 960,044 字节。
- S1 继续使用 ASCII 8.3 物理文件名，UTF-8 用户标题只写入记录；单一 storage
  worker 访问 FatFs，不允许页面并发读写。
- 当前字形资产 623 个字形。GB2312 一级汉字 3,755 个加 95 个可打印 ASCII 的
  18/24 px Medium 两档字形，估算含 20% 余量约 687,001 字节。复算：

  ```bash
  python3 tools/report_chinese_input_budget.py
  ```

## 5. 下一步与完成门槛

1. 在未接 USB、由电池供电的条件下重复深睡/顶部键唤醒，并执行多个连续循环，
   记录外设供电、复位原因、看门狗及最低内存。
2. 在保持 `0.60/0.12` 门槛不变的条件下，补充四类重复样本、近似词和背景
   噪声；原始分数全部记录，不能只记录成功样本。
3. 再按 A03 执行至少 10 人 × 每类 10 句，以及不少于 100 条无关/含混语句；
   基于真阳性和错误归类率共同标定门槛。未达标时保留手选，不勾选自动分类。
4. S0-06 已固定可离线分发的 MIT 拼音数据和 OFL 字库，完成 200 字编辑、候选分页、
   横竖屏页面和真机测试入口；下一步是完成目标板人工触摸/字形/残影验收并测内存峰值。
   详见 [S0 离线拼音资源原型](S0-PINYIN-RESOURCE-PROTOTYPE.md)。
5. 补齐 S0-07 的慢卡、满卡、只读、拔卡、低电和恢复失败证据。

语音固件最后一次屏幕结果为 `READY AFTER WAKE`。当前设备运行
`sticky-chinese-input-test`；要恢复产品固件、语音或 SD 原型，必须重新刷对应 profile。

## 6. 相关文件

- `src/voice/voice_intent.*`：硬件无关意图判定。
- `src/voice/sticky_voice_classifier_test.*`：独立真机原型入口。
- `src/input/sticky_button.*`：长按开始/结束事件。
- `src/storage/storage_contract.h`：S1 初始限额与错误分类。
- `sdkconfig.voice.defaults`、`partitions.voice.csv`：语音模型独立配置。
- `tools/platformio_voice_model.py`：模型生成、边界校验和上传参数注入。
- `tools/report_chinese_input_budget.py`：输入字形 Flash 预算估算。
