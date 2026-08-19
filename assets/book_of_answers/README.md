# Book of Answers Assets

这个目录保存答案书APP使用的像素插画、双帧动画素材和答案数据库。

## 目录

- `source/`：视觉设计源图、答案CSV和对应的Apache 2.0许可证。
- `firmware/`：经过裁切、缩放和黑白化后的固件预览图。
- `../../src/ui/assets/book_of_answers_assets.cpp`：生成后的1位位图数组，由固件直接读取。
- `../../src/apps/book_of_answers/book_of_answers_answers.cpp`：从CSV生成的350条固件答案。

主页、思考、揭晓、摇晃不足提示、文字结果和精细水晶球结果均提供两张动作帧。摇晃不足提示使用左右摆动的水晶球和鼓励继续摇晃的兔子；水晶球结果素材使用双层玻璃轮廓、星尘、装饰底座和互动兔子，球体中央留给固件动态绘制`YES`、`NO`或`UNCLEAR`。

## 答案来源

答案数据来自[The-Book-of-Answers-Interpreter的database.csv](https://github.com/DBinK/The-Book-of-Answers-Interpreter/blob/main/database.csv)，项目采用Apache 2.0许可证。仓库中的`source/database.csv`保留原始中英文三列数据，`source/DATABASE_LICENSE.txt`保留来源项目许可证。

固件当前显示CSV的英文答案列。生成时会把弯引号转换为5×7点阵字体支持的ASCII直引号，文字内容和答案顺序保持一致；CSV最后的`ACKNOWLEDGMENTS`是致谢边界，不进入随机答案池。

## 重新生成

调整源图或裁切参数后，在项目根目录执行：

```bash
python3 tools/generate_book_of_answers_assets.py
```

脚本会同时刷新`firmware/`预览图和C++位图数组。每张素材使用独立枚举名称，页面代码通过`book_of_answers_asset()`获取对应插画。

答案CSV更新后执行：

```bash
python3 tools/generate_book_of_answers_database.py
```

答案生成脚本会校验编号连续性、350条答案边界、ASCII字体支持和最多四行的显示范围，然后刷新固件答案表。
