# First-boot tutorial assets

`redraws/`保存首次开机六页教程的逐页设计稿与素材，`previews/`保存转换后的480×800黑白固件预览。`archive/legacy-eight-page-previews/`保存合并前的独立APP说明页预览，`concepts/selected-first-boot-tutorial.png`保留早期整体方向参考。

运行`python3 tools/generate_onboarding_assets.py`会将六张最终设计稿转换成1位黑白预览，并生成`src/ui/assets/onboarding_assets.h/.cpp`。固件显示整页位图后，由`onboarding_pages.cpp`统一三像素页面外框并重绘双层底栏：第一行以粗像素字体显示`BACK`、页码和`NEXT / START`，第二行居中显示`SKIP TUTORIAL`。教程运行、强制重新打开与NVS完成标记由`onboarding_app.cpp`管理，纯页码状态位于`onboarding_state.cpp`，便于在电脑上独立测试。
