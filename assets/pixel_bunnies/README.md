# Pixel Bunny Asset Library

这套素材为Sticky的黑白电子纸界面提供统一的像素兔子视觉语言。每张图都包含完整角色轮廓、表情和对应场景，可由状态牌、番茄钟及后续APP共同使用。

## 目录

- `source/`：1254×1254的设计源图，保留完整细节，适合后续重新裁切和生成其他尺寸。
- `firmware/80x80/`：经过裁切、缩放和黑白化的固件预览图，与代码中的1位位图内容一致。
- `firmware/pet_animation/`：菜单底部宠物巡场动画的96×96固件预览图。
- `src/ui/assets/`：固件中的通用绘制接口与压缩位图数据。

## 当前场景

- `focusing`：兔子在电脑前专注工作。
- `in_meeting`：两只兔子围桌开会。
- `welcome`：兔子挥手欢迎。
- `out_for_lunch`：兔子坐在桌前用餐。
- `off_duty`：兔子背包挥手离开。
- `custom`：兔子举着可填写的状态牌。

## 宠物动画动作

- `pet_wave`：在左侧挥手。
- `pet_crouch`：起跳前蓄力或落地。
- `pet_jump`：从左侧分段跳到右侧。
- `pet_celebrate`：在右侧举手庆祝。
- `pet_walk_left`：从右侧分段走回左侧。

## 状态子页动作

每种状态都使用原始素材和`_alt`次帧素材循环表演：

- `focusing_alt`：兔子双手敲键盘，电脑旁出现小闪光。
- `in_meeting_alt`：左侧兔子举手发言，右侧兔子倾听。
- `welcome_alt`：挥手的手臂切换到另一个角度。
- `out_for_lunch_alt`：兔子将勺子从餐盘抬到嘴边。
- `off_duty_alt`：背包兔子交换前后脚继续走路。
- `custom_alt`：兔子将空白状态牌举高并倾斜。

## 重新生成

电脑已安装FFmpeg时，在工程根目录执行：

```bash
python3 tools/generate_pixel_bunnies.py
```

脚本会更新80×80预览图和`src/ui/assets/status_bunny_assets.cpp`。固件采用每像素1位的高位优先排列，每张80×80素材占800字节。

宠物动画素材使用独立生成脚本：

```bash
python3 tools/generate_pet_animation.py
```

该脚本更新`firmware/pet_animation/`与`src/ui/assets/pet_animation_assets.cpp`。

## 在页面中使用

页面先通过`status_bunny_asset()`取得素材，再调用`pixel_asset_draw()`或`pixel_asset_draw_centered()`绘制。`scale=1`适合菜单卡片，`scale=3`适合全屏状态页；颜色参数可以让同一张素材在白底和黑底界面中复用。
