# Book of Answers Assets

这个目录保存答案书APP使用的像素插画素材。

## 目录

- `source/`：视觉设计源图，保留完整构图，方便后续调整裁切范围。
- `firmware/`：经过裁切、缩放和黑白化后的固件预览图。
- `../../src/ui/assets/book_of_answers_assets.cpp`：生成后的1位位图数组，由固件直接读取。

## 重新生成

调整源图或裁切参数后，在项目根目录执行：

```bash
python3 tools/generate_book_of_answers_assets.py
```

脚本会同时刷新`firmware/`预览图和C++位图数组。每张素材使用独立枚举名称，页面代码通过`book_of_answers_asset()`获取对应插画。
