#pragma once

#include <cstddef>
#include <cstdint>

enum class GrayLevel : uint8_t {
    Black = 0,
    DarkGray = 1,
    LightGray = 2,
    White = 3,
};

class Canvas {
public:
    // Wraps a caller-owned 2-bit grayscale buffer as an 800x480 drawing surface.
    // 将调用方提供的2位灰度缓冲区包装成可绘图画布，画布本身不负责释放内存。
    Canvas(uint16_t width, uint16_t height, uint8_t *buffer, size_t buffer_size);

    uint16_t width() const { return width_; }
    uint16_t height() const { return height_; }
    size_t stride() const { return stride_; }
    uint8_t *data() { return buffer_; }
    const uint8_t *data() const { return buffer_; }

    void clear(GrayLevel color = GrayLevel::White);

    // Drawing primitives clip coordinates at the canvas boundary.
    // 以下绘图函数会自动裁剪超出画布边界的坐标。
    void draw_pixel(int x, int y, GrayLevel color = GrayLevel::Black);
    void draw_line(int x0, int y0, int x1, int y1,
                   GrayLevel color = GrayLevel::Black);
    void draw_rect(int x, int y, int width, int height,
                   GrayLevel color = GrayLevel::Black);
    void fill_rect(int x, int y, int width, int height, GrayLevel color);
    void draw_text(int x, int y, const char *text, uint8_t scale = 1,
                   GrayLevel color = GrayLevel::Black);

private:
    uint16_t width_;
    uint16_t height_;
    size_t stride_;
    uint8_t *buffer_;
    size_t buffer_size_;
};
