# V1 四应用实机验收记录

日期：2026-10-10（北京时间）。本文件记录最终四应用候选的实际证据；
历史 S1 单卡测试和主机替身测试不计作本轮实机通过。

## 当前结论

当前设备为 SPI 轮询修复候选 `56a9b49be81fa6a135f8cac139b938e49fc7bd8541420d051301badc66909444`（确认保存首轮通过，连续操作待复测），原卡重新插入后页面自动恢复和三项数据保留已获实机确认。
孕周保存/返回、体重草稿保留/保存/历史、胎动计数保留/保存/统计、深睡唤醒后恢复均有通过证据。
R01–R07 软件修复和 V1-REG-04 胎动栈修复已加入；原 Settings 刷新停滞的独立根因尚未确认，不能用短时正常操作关闭长时间稳定性验收。
未完成三卡、真实满卡/只读、迁移中断、写入中断电、四向双语触摸与功耗等门槛，当前不可正式发布。

以下早期步骤保留失败过程与修复依据；当前判定以验收映射及文末修复接续记录为准。

## 测试条件

- 固件：`2.0.0` / `sticky-release`。
- 首轮 Firmware SHA-256：`b7aebe0dd6a08f1b434a1874abd7803e11dd9fc23ff0f55f78957dc52c641762`。
- 两项修复已烧录 SHA-256：`1f2ad14202a51a01dd0137d2126b880082ce72a893095e3dcad8c941e26ed2d4`；
  两项修复已通过主机 73/73 和三个配置构建，已重烧录并正常启动，操作复测进行中。
- 三项修复构建 SHA-256：`6b0a879158b26df4c5d98662385858bb1fa07e3afd4d0b7c13d2011a94a29eb7`；主机 73/73、三个配置构建通过，已烧录并正常启动，初始化操作复测进行中。
- 设备：esptool 识别为 ESP32-S3 revision v0.2。
- 串口：`/dev/cu.usbmodem5C843369951`，115200；USB Single Serial。
- 连接：用户已连接 USB；电池状态及独立电池供电尚未确认。
- MicroSD：用户确认可直接用于测试，没有需要保留的记录；总容量 32,199,671,808 B，空闲 32,193,282,048 B；卡型号待现场确认。
- 本轮未执行全 Flash 擦除、卡格式化或旧 NVS 清除。

## 早期候选步骤（历史证据）

| 步骤 | 结果 | 证据与限制 |
| --- | --- | --- |
| USB 串口检测 | PASS | 系统枚举及 `pio device list` 均检测到上述端口 |
| 候选包与当前编译输入比对 | PASS | 305 文件摘要匹配；包内 SHA-256 和 ZIP 完整性通过 |
| release 烧录 | PASS | `pio run -e sticky-release -t upload --upload-port /dev/cu.usbmodem5C843369951`；SUCCESS 78.955 s；引导、分区、应用写入后均 hash verified |
| 烧录文件身份 | PASS | 镜像中 `firmware.bin` 摘要与候选包一致 |
| release 启动 | PASS（串口） | `phase=ready result=ok`；`runtime=ready apps=4 default=pregnancy current=pregnancy` |
| 板级服务初始化 | PASS（串口） | RTC、battery、SSD1677、GT911、IMU、按键和蜂鸣器均 ready；尚未完成交互/屏幕观察 |
| 恢复工作区 | PASS（当前卡） | `psram_bytes=209056 slots=1024`；存储工作任务 free stack 2,332 B，internal free 224,359 B，PSRAM free 7,904,688 B；不是全量数据验收 |
| 当前卡挂载与恢复 | 已观察，需清理测试数据 | generation 1，manifest slot A / commit 1；TailTruncated，有效前缀 journal 7 / last_tx 7 / live 28 / objects 28；未尝试业务写入 |
| 两项修复版烧录/启动 | PASS（串口） | SUCCESS 76.665 s，写入校验通过，phase=ready、apps=4；初始读取后栈余量 10,524 B，internal free 216,167 B |
| 三项修复版烧录/启动 | PASS（串口） | SUCCESS 74.342 s，写入 hash verified；phase=ready、apps=4；新空数据集 Empty / ready=1，初始读取栈余量 10,700 B |
| Settings 进入路径 | 首轮 FAIL，修复后本次进入 PASS（串口） | 首轮存储任务栈溢出；最终版本 346360 ms 成功切换 settings，随后存储请求栈余量 7,180 B，没有记录 panic；卡管理页及删除/创建均已执行 |
| 修复后孕周保存 | FAIL，需继续定位 | request 4 / Commit 返回 ObjectError（16），未提交；栈余量 8,876 B，没有再次记录 panic；该卡仍为旧截断测试数据集 |
| 错误页返回应用列表 | PASS（屏幕和串口） | 用户点击右上角 BACK 后确认四项列表出现；320193 ms 记录 launcher=open |
| 卡上应用数据删除与初始化 | PASS（用户和串口） | 删除 4 文件 / 4 目录，缓存失效；request 7 初始化 result=0 changed=1；用户确认成功 |
| 初始化后直接进入孕周 | FAIL（V1-REG-03） | 507916 ms 切回 pregnancy 后仍为存储错误，无后续 repository 请求；恢复状态仍为删除后的 NotAttempted / ready=0 |
| RTC 时间 | 待设置 | `rtc=read validity=low_voltage result=invalid` |
| 面板刷新 | PASS（驱动返回） | 两次 full monochrome refresh 返回 ESP_OK，约 2 s；实际画面与残影待现场确认 |

原始运行证据保存于候选目录的 `hardware/`；仅有串口结果的项目不能代表屏幕、触摸或电源验收通过。

## V1-A01–V1-A15 状态

| 编号 | 场景 | 本轮状态 |
| --- | --- | --- |
| V1-A01 | 四应用入口、旧 ID 回退 | 部分：四应用启动与普通切换、Launcher 开关通过；四向入口、旧 ID 实机回退待验证 |
| V1-A02 | 旧数据不触发业务提醒 | 未执行 |
| V1-A03 | 孕周设置、修改、跨日及恢复 | 部分：日期保存/返回及深睡恢复通过；LMP/修改/周边界/跨日实机待验证 |
| V1-A04 | 体重创建、同日确认、历史编辑/删除、kg/lb | 部分：草稿保留、创建、历史读取、深睡恢复通过；替换/编辑/删除/单位实机待验证 |
| V1-A05 | 7/30 日趋势、366 条、满额与分页 | 未执行 |
| V1-A06 | 胎动计数、撤销、保存/取消、防误触与超时 | 部分：3 次计数、Launcher 保留、保存、今日统计及深睡恢复通过；撤销/取消/超时/连续操作待验证 |
| V1-A07 | 胎动 SD 提交、42 槽旧 NVS 迁移、中断/冲突 | 部分：新胎动 SD 提交 tx=6 与恢复通过；真实旧 NVS 来源、迁移/中断/冲突待验证 |
| V1-A08 | 双语、时间、清屏、卡管理、独立删除/重置边界 | 部分：早期删除/创建通过，软件恢复缺陷已修复；最终候选不重启删除/初始化、双语、清屏及删除边界待完整复测 |
| V1-A09 | 无卡、真实只读、满卡 | 未执行 |
| V1-A10 | 插拔、换卡与迟到请求隔离 | 未执行 |
| V1-A11 | 断电、损坏和未知 schema | 部分：本轮 release 识别既有截断尾部并发布有效前缀；其他故障与写入边界待执行 |
| V1-A12 | 安全取出、USB/电池睡眠与唤醒 | 部分：deep_sleep/ext1 后重挂、恢复通过；电源条件、安全取出及卸载失败待验证 |
| V1-A13 | 中英文、四向触摸和连续局刷残影 | 未执行 |
| V1-A14 | 三卡、低电和实际离线功耗 | 部分：当前一张约 32 GB 卡挂载成功；多卡、满额资源、低电和功耗待执行 |
| V1-A15 | 备份恢复与交付包 | 部分：自动验证、构建和候选包通过；真实备份/恢复及发布待完成 |

## 本轮缺陷 V1-REG-01

- 触发：顶部按键打开 Launcher，触摸 Settings；协调器保存 last-app 设备偏好。
- 证据：串口 `A stack overflow in task sd_storage`，随后 `reset=panic`；用户照片
  显示孕周 `MICROSD DATA NEEDS REVIEW` 错误页，不是设置的卡管理页。
- 定位：原存储任务栈 8,192 B；release 的 worker 1,216 B、请求分发 1,856 B、
  repository 执行 1,840 B、journal store append 560 B、journal append 1,408 B、
  journal replay 1,344 B 的嵌套路径已累计 8,224 B，尚未含 FatFs/驱动/libc。
- 修复：任务栈改为 16 KiB；repository 完成日志增加运行栈高水位余量。
- 状态：16 KiB 栈版本已再次烧录并正常启动，当前卡初始 read 请求栈余量 10,332 B；
  最终候选进入 Settings 未再记录 panic，协调器偏好保存请求返回 JournalError（17），该路径最小栈余量 7,180 B。仍需完成新数据集上的成功提交和持续操作复测。两项修复的最终三配置构建已通过。此前候选
  不得作为正式发布固件。首轮日志和照片保存在 `dist/v1-hardware-20261010/`。
- 仍待处理：旧测试数据集截断尾部、RTC 设置，以及完整 V1-A01–V1-A15。

## 代码检查缺陷 V1-REG-02

- 定位：自动事务请求保留零占位值，实际提交由 worker 执行层分配非零 ID；
  完成后的磁盘恢复校验却与请求中的零比较，导致有效提交被误报 RecoveryError。
- 修复：以 `result.batch.transaction_id` 验证恢复结果，完成日志也显示实际 ID；
  规则抽为纯 C++，覆盖自动 ID 正常回读、错号、无效视图、零 ID 和原失败保留。
- 状态：修复后主机回归 73/73 和最终三配置构建通过，卡上真实新提交待复测。
  这项由代码检查发现，不冒充已经观察到的实机失败。

## 本轮缺陷 V1-REG-03

- 触发：Settings 删除应用数据后创建新数据集，不重启，直接返回孕周。
- 证据：删除时恢复状态清为 NotAttempted / ready=0；初始化 request 7 成功，随后切回 pregnancy 仍显示错误，未提交 repository 请求。
- 根因：manifest worker 只更新所选 manifest，未刷新恢复视图；adapter preflight 因 view_ready=false 拒绝读取。
- 修复：成功 manifest 请求重新探测并加载恢复视图，再发布 manifest 和完成请求；缺少 journal 的新数据集应为 Empty / ready=1。
- 验证：新增真实临时目录上的初始化→空恢复→读取 NotFound→首次提交→重新恢复场景；主机回归 73/73、三配置构建和 Markdown/diff 检查通过；修复版已烧录正常启动，新空数据集 Empty / ready=1；不重启重复删除/创建及首次业务保存待执行。

## 复审修复版（R01–R07）

- release SHA-256：`f086d3523d001619f8af0f6352a1cb6f98b6ddd926bac0e1cc1ee3428c04ab2c`。
- 主机 76/76 C++、8 个 Python 用例和三配置构建通过；写入设备耗时 57.482 秒，哈希校验通过。
- 新包 `dist/v1-reviewfix-20261010/`；本轮源输入与编译镜像的 290 个固件相关文件逐字节一致。
- 复位启动正常：4 应用 runtime ready；SD 挂载 generation=1，新数据集 Empty / ready=1；两次全屏刷新分别约 2,011 ms 和 2,000 ms，均返回 ESP_OK。尚未观察到本轮 panic 或 SPI 完成等待超时。
- 用户确认 Launcher → Baby Week 已进入日期设置页或孕周首页；串口 36,117 ms Launcher 打开、37,123 ms 选择成功，随后刷新均成功。用户进一步确认日期保存成功，切到 Settings 再返回后孕周正确、无 MicroSD 错误。串口独立确认 290,755 ms 首次业务提交 tx=1 / result=0 / changed=1，storage worker 栈余量 4,948 B；当前日志片段尚未包含该次 Settings 往返，往返结果按用户确认记录。体重复测随后通过，详见下项。软件修复见 [复审记录](V1-CODE-REVIEW.md)；显示停滞根因与完整实体操作验收仍未关闭。

- 体重：用户确认编辑数字在打开/关闭 Launcher 后保留，保存成功、历史可见。串口 375,669 ms pregnancy → weight 成功，399,724/402,249 ms Launcher 打开/关闭成功；tx=2、3、4 连续提交均 result=0 / changed=1，storage worker 最小栈余量 4,884 B。这同时提供多个不同业务对象连续保存的实机证据；尚不等同于断电恢复验证。
- 胎动复测失败：用户报告完成失败/统计缺失/页面卡住。串口确认 468,797 ms weight → kicks；偏好 tx=5 保存成功，随后 `A stack overflow in task kicks has been detected`，重启后因 last-app=kicks 再次溢出。未看到胎动记录提交，不能将偏好 tx=5 当作胎动保存。

## V1-REG-04：胎动任务栈溢出

- release 反汇编确认胎动 `poll_store` 的直接帧为 `0x900`（2,304 B），包含重试分支两个 1,024 B 编码 payload，即使正常加载也分配整个帧。原任务栈 5,120 B，叠加 repository 完成收集等调用后发生实机溢出。
- 修复：两个比较缓冲区移至胎动任务独占的静态空间，任务栈改为 8,192 B；完成日志记录栈高水位。没有删除或重建 SD 数据。
- 验证：76/76 C++、8 个 Python 用例及三配置构建通过；release 反汇编 `poll_store` 帧从 2,304 B 降为 256 B。
- 新 release SHA-256：`7bc9b30ba742f28f5e45880347aef7cfaf89f4fc4d38e20e070c24f14f3daba1`；包为 `dist/v1-kickfix-20261010/`，已烧录并通过哈希校验（77.296 秒）。重启自动进入 kicks，加载成功、任务栈余量 4,096 B，两次全屏刷新均 ESP_OK；短时观察未再溢出，已请求重做计数/Launcher/Finish 测试。原 `v1-reviewfix-20261010` 标为 BLOCKED，保留失败固件与日志。

- V1-REG-04 复测：用户确认胎动计数在打开/关闭 Launcher 后保留，Finish 保存成功、今日统计可见。串口 546,095/546,901 ms Launcher 打开/关闭成功，551,335 ms 胎动 tx=6 / result=0 / changed=1；恢复视图 ready=1、live=5、objects=5。胎动任务完成时栈余量仍为 4,096 B，storage worker 最小余量 4,972 B；本次启动约 561 秒日志未再见栈溢出。
- 当前孕周保存/返回、体重草稿/保存/历史、胎动计数/保存/统计均有用户通过确认。用户回复三项数据均保留、页面正常。串口对应启动原因为 `reset=deep_sleep wake=ext1`，恢复 journal=6 / last_tx=6 / live=5 / objects=5 / ready=1，后续读取成功。因此记录为深睡唤醒后数据恢复通过；未将其计作 RESET 冷启动或写入中断电测试。后续切换偏好 tx=7–9 均成功，storage worker 最小栈余量 4,784 B。

## 当前固件硬件复位验证

- 通过串口 RTS 执行正常硬件复位；没有在写入中断电，也未移除电池供电。
- 启动日志 `reset=power_on wake=undefined`，4,778 ms 恢复 journal=12 / last_tx=12 / live=5 / objects=5 / ready=1；四应用 runtime ready，当前 pregnancy。
- 四次 repository 读取 result=0；两次全屏刷新分别 2,010 / 2,000 ms 且 ESP_OK，初始 24 秒未见 panic/栈溢出。证明当前数据集经过硬件复位可恢复，不代替真实切断供电或写入中故障测试。
- 新证据保存为候选包 `RESET-DEVICE.log`。核对包内 22 项原文件摘要及源码清单均匹配；本轮仅整理文档，无生产代码变更。
- 原卡热插拔复测失败：用户报告插回后显示内存卡不存在，详见 V1-REG-05。

## 接续规则

执行 [需求验收表](PROJECT-SPEC.md) 和 [硬件清单](pregnancy/hardware-regression-checklist.md)。
每项记录卡型/容量/数据集、供电方式、操作、串口日志、屏幕证据和 PASS/FAIL。
未测试、未观察或只有自动替身证据的项目保留未执行；发现失败时记录问题并修复后重测。

## V1-REG-05：热插拔后设置页状态未刷新

- 用户在安全取出、拔卡并插回原卡后，页面仍显示卡不存在。串口实际在 172,925 ms / 175,890 ms 挂载成功（generation 2 / 3），并恢复 tx=13 / live=5 / objects=5 / ready=1。
- 根因：StorageSummary 仅在操作事件中重绘，未订阅异步卡状态变化；触摸解释重新读取当前状态，可能与显示的初始化/安全取出按钮不一致。日志后续再次卸载与此风险相符，不能据此断言用户实际点了哪一项。
- 修复：比较已绘制状态与实时快照，卡状态或挂载代次变化时重绘；新代次清除旧操作消息，刷新后丢弃旧触摸，按钮动作使用已绘制快照。
- 主机 76/76 C++ 和 8 个 Python 用例通过；新增缺卡→插入、同卡新代次、操作消息及动作可用性变化的比较回归。三配置构建通过（debug 27.707 秒、power-test 25.185 秒、release 26.488 秒），实机复测待完成。
- 候选包 `dist/v1-hotplugfix-20261010/`；release SHA-256 `eab17adcc80cc4d813d737341b9fa96ab4abb7b15e52e4af64874a7298528371`；386 个固件输入与编译镜像逐字节匹配。

- V1-REG-05 固件已烧录，耗时 81.608 秒，设备写入哈希校验通过。实机插拔复测待完成。
- 新固件启动：挂载 generation=1 成功，恢复 tx=13 / live=5 / objects=5 / ready=1；四应用 runtime ready，当前 Settings，两次全刷 2,002 / 2,001 ms 均 ESP_OK。已请求原卡安全取出和重插复测，串口继续采集。

- V1-REG-05 实机复测通过：用户确认插回后页面自动恢复，孕周、体重历史、胎动统计均保留。串口安全卸载 80,504 ms、拔卡 103,361 ms，重插挂载 generation=2/3 均成功，最终 113,238 ms 恢复 tx=13 / live=5 / objects=5 / ready=1；自动全刷均 ESP_OK。后续依次切换 pregnancy、weight、kicks 成功。本项仅关闭当前原卡热插拔，不代表三卡/跨数据集验证通过。

## 中文显示与触摸（四个方向）

用户先确认当前方向下孕周、体重历史、胎动统计及设置文字完整、点击正常，随后确认其余三个方向显示和点击均正常。中文四向在上述页面范围内通过，依据为用户实机确认；串口仅作为部分方向切换的补充证据。英文四向及其他编辑/确认子页尚未完整验证，不据此关闭全部显示交互验收。

## 英文四向验收范围调整

2026-10-10 用户明确决定放弃英文测试。本轮英文四向记为 SKIPPED（用户决定），不记 PASS；中文四向已有通过记录继续有效。本次跳过不改变英文功能代码，也不覆盖其他实机验收缺口。下一项转为体重编辑、同日替换及单位切换。

## V1-REG-06：体重确认页 SPI 完成等待阻塞

- 用户报告编辑已有体重后，确认页 CONFIRM 无法点击。串口触摸仍有事件，但 114,305 ms 全屏刷新开始后，驱动持续报告 `spi phase=completion_wait dc=1 bytes=1024`，等待超过 38 秒，未完成刷新；页面任务同步等待，无法继续消费确认动作。
- CONFIRM 绘制与命中共用 footer，现有横竖屏命中测试覆盖该位置。没有证据指向按钮坐标错误。
- 修复候选：电子纸单调用者串行传输改为 IDF `spi_device_polling_transmit`，不再依赖中断事务结果队列；每块 1,024 B 结束后由 IDF 释放总线，保持 DMA 内存有效。IDF SDSPI 同样使用 polling 路径。混合设备事务本身是 IDF 支持的配置，因此不将混用直接断言为根因；丢失完成通知的底层原因仍未证实。
- API 依据：[ESP-IDF 5.4.1 SPI master](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/peripherals/spi_master.html) 及当前安装的 IDF 5.4.1 `spi_master.c`。轮询传输占用调用任务 CPU，功耗验收仍需实测。
- 自动回归、三配置构建、烧录与原操作复测进行中；本项不能因代码改动就标为关闭。

- V1-REG-06 自动验证：76/76 C++、8 个 Python 用例通过；debug/power-test/release 构建分别 32.889 / 28.265 / 29.437 秒，全部成功。386 个源输入与镜像匹配。候选 release SHA-256 `56a9b49be81fa6a135f8cac139b938e49fc7bd8541420d051301badc66909444`，包 `dist/v1-spifix-20261010/`；烧录及实机确认保存待完成。
- V1-REG-06 候选烧录完成，77.684 秒，写入哈希校验通过；等待启动日志和人工体重确认保存复测。
- 新候选启动恢复 tx=36 / live=5 / objects=5 / ready=1，四应用 ready、当前 weight，已观察全刷 2,017 ms / ESP_OK；人工复测请求已发送。该启动证据不代替连续刷新和保存验证。

- V1-REG-06 首轮原操作复测通过：用户确认修改成功、日期不变且仍只有一条。串口 37,757 ms 提交 tx=37 / result=0 / changed=1，storage worker 栈余量 4,868 B，后续全刷 2,084 / 2,003 ms 均 ESP_OK。仅证明本次确认保存成功；连续刷新、共享总线并发和功耗仍待验证，底层阻塞根因不作已证实结论。

- V1-REG-06 连续操作复测：用户确认三轮同一体重记录编辑、确认保存及 Settings/Weight 往返全部成功，无重复记录或页面卡住。日志显示后续事务提交至 tx=46 均成功，连续全刷/局刷返回 ESP_OK，storage worker 最小栈余量 4,756 B。本次短程复测通过；长期运行、功耗及其他介质下稳定性仍待验收。

## 体重单位往返验收

用户确认 KG→LB（历史单位同步）→KG 切换正常，原公斤数值恢复，日期和记录条数不变。串口对应偏好保存 tx=47、48 均 result=0 / changed=1，storage worker 栈余量 4,756 B。此项不包含在 LB 下编辑并保存的实机验证。

## 统一人工验收方式（2026-10-10）

按用户要求，后续改用[统一人工测试清单](V1-MANUAL-TEST-CHECKLIST.md)批量执行并汇总问题，停止逐项聊天提问。上一条同日替换询问纳入 W02，尚未收到结果，不记通过。人工完成可执行项目后统一排查失败项；需要专业环境的项目记阻塞，英文测试按用户决定跳过。
