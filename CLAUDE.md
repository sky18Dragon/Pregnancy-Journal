# CLAUDE.md

本仓库的总体工程规则见 [AGENTS.md](AGENTS.md)（分层边界、存储、测试清单）。
视觉与交互契约见 [DESIGN.md](DESIGN.md)。UI 现状与问题见 [docs/ui-audit.md](docs/ui-audit.md)。
下面的章节是**所有 UI 工作必须遵守的约束**。2026-10-15 起，负责人已按草案确认其中的数值与策略；仍标注 **【待确认】** 的只剩真机才能得出的参数，不得当作既定事实，也不得猜测。

## 墨水屏 UI 约束

### 1. 屏幕参数

| 项 | 值 |
| --- | --- |
| 设备 | Seeed Studio reTerminal Sticky（ESP32-S3） |
| 面板 | 3.97 英寸，物理 800×480，约 235 ppi；面板料号 **【待确认】**（Seeed 官方文档、Seeed_GFX2 均只给出控制器，未给面板料号） |
| 控制器 / 驱动 | SSD1677；`components/seeed_epaper`（SPI2，与 MicroSD 共线） |
| 逻辑画布 | 横屏 800×480 / 竖屏 480×800，左上角原点；方向由 IMU 稳定后决定，两种都必须支持 |
| 帧缓冲 | Canvas 为 2 bit 灰度（Black/DarkGray/LightGray/White） |
| **送屏格式** | **1-bit 黑白**：灰度 0、1 → 黑，2、3 → 白。产品页面不走 4 级灰阶刷新 |
| 输入 | GT911 电容触摸；顶键（单击/双击）；双侧键长按 2 s 睡眠；无旋钮。物理触摸坐标必须经 `Canvas::physical_to_logical()` |
| UI 框架 | 自研立即模式绘图层（`Canvas` + 页面 `render` + `*_action_at` 命中函数），不使用 LVGL |
| 全刷耗时 | 实测约 2000 ms；局刷耗时 **【待确认】**（仓库与官方资料均无数据；真机日志 `display=refresh_done mode=monochrome_partial elapsed_ms=` 可直接取得） |
| 预览 | `tools/ui_preview/render_all.sh`（见下文「工作方式」），输出 1-bit PNG + 命中区叠加图 |

### 2. 设计规则（草案）

字号与线宽
- 最小字号分两档（`ui_tokens.h`）：**正文与操作文字 ≥ 18 px**（`kBody`：按钮、列表行、数值、警示，凡用户必须读懂才能操作的文字）；**说明类文字 ≥ 16 px**（`kSecondaryBody`：副标题、页脚、图表刻度）。**低于 16 px 一律禁止**（`kMicro`、`kCompactLabel` 已删除）。旧 `scale` 映射改为 1→16 px、2→18 px、≥3 仍为 7×scale，因此 `draw_text(…, 2)` 与按钮标签默认就是 18 px。
- 页面标题 28 px，区块标题 24 px，大数字 52–56 px；一律使用 `ui_tokens.h` 令牌，不在页面里写字号字面量。
- 页面代码里的字号和左边距必须用 `ui_tokens::` 名称（`kBody`、`kSectionHeading`、`kContentMargin` …），不写数字字面量；`tools/check_ui_tokens.py` 在 `run_host_tests.sh` 中强制检查（`--fix` 可自动改写已有对应令牌的字面量）。新增字号先加令牌再用。
- 最小线宽 **2 px**（已确认）用于需要一眼看清的边框/分隔/图标笔画；1 px 仅允许用于次要分隔线且必须是纯黑。选中/强调描边 3 px（`kEmphasisStroke`）。

颜色
- 产品页面**只使用 Black 和 White**。
- **禁止** LightGray/DarkGray 参与画线、描边、填充或承载信息（1-bit 阈值会让它们消失或破碎）。
- **禁止**大面积灰色填充、渐变、抖动（dither）图案、斜线阴影。
- 仅在经过评审并改用 4 级灰阶全刷的专用页面中才允许灰阶，当前无此类页面。

状态表达
- 选中/激活：**反色（黑底白字）或 3 px 粗边框**，二选一并全 App 统一；不得只靠灰色或颜色表示。
- 禁用：虚线或删除线加文字说明，不靠变灰。
- 按下反馈：会触发换页（全刷）的点按，必须先用 `src/ui/press_feedback.h` 画按下态并局刷一次（小目标反色、高度 > 80 px 的目标用 3 px 框）；页内点按不加按下态，其局刷本身就是反馈。新页面无需额外代码，只要命中函数 `*_action_at` 与绘制一致。
- 二选一状态用分段开关（两个选项都可见，当前项反色），不要用“A / B”单按钮文案。
- **不使用任何动画**（过渡、渐变、进度动画、闪烁光标均禁止），没有悬停态。

布局与点击
- 内容边距与安全区 24 px；最小触控高度 48 px，主按钮 56 px。
- **同一个 `Rect` 同时用于绘制和命中测试**，不得分别维护两套坐标。孕周页是范例：每个方向一份 `PageLayout`，渲染画它，`pregnancy_page_targets()` 列出可点目标，`pregnancy_page_action_at()` 只是“第一个包含该点的目标”，并有宿主测试（目标在屏内、≥48 px、互不重叠）。新页面和其他 App 逐步按此改造。
- 横竖屏分别验证；不得裁剪或把横屏截图旋转当作竖屏。
- 文案走 `ui_text()`；新增中文字必须通过字体生成器，不允许出现缺字方框。

### 3. 刷新策略（草案）

| 场景 | 刷新方式 |
| --- | --- |
| 进入新 App、整页内容替换、方向变化、启动器打开/关闭、日期翻页、睡眠帧 | **全刷**（`sticky_display_refresh_monochrome()`） |
| 同一页内小范围变化：计数、数值加减、选中态切换、文字小幅更新 | **局刷**（`sticky_display_refresh_partial()`），变化区域 ≤ 屏幕面积的 25%（已确认）；超过则用全刷 |
| 连续局刷累计 | 显示层现有硬上限 **8 次**后自动提升为全刷（`kPartialRefreshesBeforeCleanup`），已确认沿用 8 次；只有真机残影观察证明不够时才考虑收紧。页面层**不得再维护第二份计数** |
| 页面之间的刷新选择 | 所有 App 统一使用同一条规则：**页面（`page`）变化 → 全刷；页内变化 → 局刷**（已确认，接受每次页面跳转约 2 s 全刷；与 Seeed 官方 demo 的做法一致）。当前设置、孕周、启动器与此不一致，属于待修复项（`ui-audit.md` E1） |
| 睡眠 | 绘制睡眠帧 → 全刷 → 断电（保持现状） |
| 用户手动清屏 | 设置里的“刷新屏幕”动作保留 |

补充：
- 刷新只能由所属 App 任务或框架协调器发起；**禁止**在 ISR、触摸回调、页面渲染函数、传感器后台任务里触发刷新（沿用 AGENTS.md）。
- 不得为了“看起来更新”而定时重绘；仅在入口、恢复、用户动作、有意义的时间翻转时渲染。
- 后续若新增集中的刷新策略模块，应放在 UI/应用层，不得改动驱动。

### 4. 代码边界

**允许修改（UI 层）**
- `src/ui/`（绘图层、令牌、通用组件、布局辅助、字体与资源使用）
- `src/apps/*/*_pages.*`（页面渲染与命中测试）
- 对应的 `test/*_pages_test.cpp`、`assets/`、`tools/generate_*.py`、`src/ui/assets/`（生成物，用生成器重新生成）
- `docs/`、`DESIGN.md`、`CHANGELOG.md`

**需要先说明理由并获得确认才能改**
- `src/apps/*/*_app.cpp` 中与刷新调用相关的部分（只能改“选择刷新方式”的逻辑，不得改存储/生命周期/状态机语义）
- `src/app/`（应用协调器、输入分发、手势）

**不允许修改**
- `components/seeed_epaper/`、`components/gt911/`（面板与触摸驱动）
- `src/display/`（刷新实现与局刷计数），除非任务明确以显示层为目标
- `src/board/`、`src/devices/`、`src/sensors/`、`src/input/`（底层驱动与硬件）
- `src/storage/`、`src/pregnancy/`（存储与领域逻辑）、`partitions*.csv`、`sdkconfig*`、`platformio.ini`
- `StickyAppId` 数值（持久化数据，不得重编号）

> 注：工作区当前有大量未提交改动（含显示层与驱动组件）。开始任何 UI 改动前，先与负责人确认基线提交。

### 5. 工作方式

1. **一次只改一个页面**（横竖屏算同一页面）。不要顺手改邻近页面、不要跨 App 批量替换。
2. 动手前先对照 `docs/ui-audit.md` 的问题编号，说明本次要解决哪几条。
3. **改动 UI 后必须渲染预览并自查**：运行 `tools/ui_preview/render_all.sh [输出目录] [页面名过滤]`（默认输出到 `build/ui_preview/`，打开其中的 `index.html`）。它直接编译真实页面渲染函数，输出每页横/竖屏 × 中/英文的 1-bit PNG（与送屏数据一致）和 `.hits.png` 命中区叠加图，并对灰阶像素、小于 48 px 的命中区、命中区重叠给出警告。新增页面时在 `tools/ui_preview/ui_preview.cpp` 注册。自查要点：
   - 最小字号 / 线宽 / 无灰色 / 无渐变（预览会输出 `fontlint.txt`：低于 16 px、文字越界/互相重叠、被行数上限截断的文字、溢出按钮的标签；前三类必须为 0，16–17 px 的说明类文字需人工确认不承载操作信息）
   - 选中态与按下反馈是否可见
   - 绘制 `Rect` 与命中 `Rect` 是否一致
   - 文字是否溢出或粘连
   预览不反映刷新过程和残影，汇报时必须写明“仅宿主预览，未经真机验证”。
4. 修改渲染后运行 `./tools/run_host_tests.sh` 与 `python3 tools/check_markdown_links.py`；涉及刷新、触摸、方向的改动还须按 `docs/pregnancy/hardware-regression-checklist.md` 做真机验证，**不要声称真机通过，除非有记录**。
5. 项目路径含空格，PlatformIO 构建按 `docs/DEVELOPMENT.md` 的无空格镜像流程处理，不要重命名工作区。
6. 不确定的屏幕/刷新参数标注 **【待确认】** 并询问，不要猜。
