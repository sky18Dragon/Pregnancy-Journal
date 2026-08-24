# Pomodoro Assets

番茄钟使用64×64黑白桌宠标志。图形沿用桌宠童年阶段的长耳、大眼睛、脸颊绒毛与三角围巾，并让角色在半身构图中双手抱着番茄。

- `source/tomato_bunny_half.png`：确认后的高清半身角色母版。
- `firmware/tomato_bunny.png`：与固件尺寸一致的64×64纯黑白预览。
- `tools/generate_pomodoro_assets.py`：母版裁切、缩放、二值化和一位位图打包工具，是固件素材的生成入口。
- `src/ui/assets/pomodoro_assets.cpp`：固件实际编译的一位位图数据。

重新生成预览和固件素材：

```bash
python3 tools/generate_pomodoro_assets.py
```

生成脚本使用与其他桌宠素材工具相同的Pillow环境。生成完成后，预览图和C++位图会同步更新。
