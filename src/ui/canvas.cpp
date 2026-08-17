#include "canvas.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "font.h"

Canvas::Canvas(uint16_t width, uint16_t height, uint8_t *buffer, size_t buffer_size)
    : width_(width),
      height_(height),
      stride_((width + 3U) / 4U),
      buffer_(buffer),
      buffer_size_(buffer_size)
{
}

void Canvas::clear(GrayLevel color)
{
    // Repeat one 2-bit value four times so memset can clear four pixels at once.
    // 将同一个2位灰度值复制四次，让memset一次写入四个像素。
    const uint8_t value = static_cast<uint8_t>(color) & 0x03U;
    const uint8_t packed = static_cast<uint8_t>(value * 0x55U);
    if (buffer_ != nullptr) {
        std::memset(buffer_, packed, buffer_size_);
    }
}

void Canvas::draw_pixel(int x, int y, GrayLevel color)
{
    if (buffer_ == nullptr || x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }

    // Pixels are packed from the most-significant pair to the least-significant pair.
    // 每个字节从高位到低位依次保存四个像素，每个像素占两位。
    const size_t index = static_cast<size_t>(y) * stride_ + static_cast<size_t>(x) / 4U;
    const uint8_t shift = static_cast<uint8_t>((3 - (x & 0x03)) * 2);
    const uint8_t mask = static_cast<uint8_t>(0x03U << shift);
    const uint8_t value = static_cast<uint8_t>(color) << shift;
    buffer_[index] = static_cast<uint8_t>((buffer_[index] & ~mask) | value);
}

void Canvas::draw_line(int x0, int y0, int x1, int y1, GrayLevel color)
{
    // Bresenham's integer algorithm draws every slope without floating-point math.
    // Bresenham整数算法无需浮点运算，就能绘制任意斜率的连续直线。
    const int dx = std::abs(x1 - x0);
    const int step_x = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int step_y = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    while (true) {
        draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int twice_error = 2 * error;
        if (twice_error >= dy) {
            error += dy;
            x0 += step_x;
        }
        if (twice_error <= dx) {
            error += dx;
            y0 += step_y;
        }
    }
}

void Canvas::draw_rect(int x, int y, int width, int height, GrayLevel color)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    draw_line(x, y, x + width - 1, y, color);
    draw_line(x, y + height - 1, x + width - 1, y + height - 1, color);
    draw_line(x, y, x, y + height - 1, color);
    draw_line(x + width - 1, y, x + width - 1, y + height - 1, color);
}

void Canvas::fill_rect(int x, int y, int width, int height, GrayLevel color)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    // Clip once before looping so out-of-bounds rectangles remain safe and predictable.
    // 进入循环前统一裁剪边界，使超出屏幕的矩形也能安全绘制。
    const int start_x = std::max(0, x);
    const int start_y = std::max(0, y);
    const int end_x = std::min<int>(width_, x + width);
    const int end_y = std::min<int>(height_, y + height);
    for (int row = start_y; row < end_y; ++row) {
        for (int column = start_x; column < end_x; ++column) {
            draw_pixel(column, row, color);
        }
    }
}

void Canvas::draw_text(int x, int y, const char *text, uint8_t scale, GrayLevel color)
{
    if (text == nullptr || scale == 0) {
        return;
    }

    // A set font bit becomes one scale-by-scale square on the canvas.
    // 点阵字体中的每个有效点会放大成一个scale乘scale的方块。
    int cursor_x = x;
    for (const char *character = text; *character != '\0'; ++character) {
        const FontGlyph &glyph = font_get_glyph(*character);
        for (uint8_t column = 0; column < kFontWidth; ++column) {
            for (uint8_t row = 0; row < kFontHeight; ++row) {
                if ((glyph.columns[column] & (1U << row)) != 0U) {
                    fill_rect(cursor_x + column * scale,
                              y + row * scale,
                              scale,
                              scale,
                              color);
                }
            }
        }
        cursor_x += (kFontWidth + kFontSpacing) * scale;
    }
}
