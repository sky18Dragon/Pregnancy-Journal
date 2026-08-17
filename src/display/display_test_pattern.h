#pragma once

class Canvas;

// Draws an asymmetric monochrome image used to verify panel output and orientation.
// 绘制不对称黑白测试图，用于确认屏幕能够刷新，并判断画面方向是否正确。
void display_test_pattern_render(Canvas &canvas);
