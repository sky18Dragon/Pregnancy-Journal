# First-boot tutorial assets

`redraws/`保存首次开机八页教程的逐页设计稿与素材，`previews/`保存转换后的480×800黑白固件预览。`concepts/selected-first-boot-tutorial.png`保留早期整体方向参考。

运行`python3 tools/generate_onboarding_assets.py`会将八张最终设计稿转换成1位黑白预览，并生成`src/ui/assets/onboarding_assets.h/.cpp`。固件直接显示整页位图，页面中的兔子、文字、箭头、边框和底栏保持与设计稿一致。教程运行与NVS完成标记由`onboarding_app.cpp`管理，纯页码状态位于`onboarding_state.cpp`，便于在电脑上独立测试。
