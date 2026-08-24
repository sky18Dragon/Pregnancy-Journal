#include <cassert>
#include <cstdint>
#include <fstream>
#include <vector>

#include "app_pages.h"
#include "canvas.h"

namespace {

constexpr uint16_t kWidth = 800U;
constexpr uint16_t kHeight = 480U;
constexpr size_t kStride = kWidth / 4U;

uint8_t pixel_level(const std::vector<uint8_t> &buffer, int x, int y)
{
    const size_t index = static_cast<size_t>(y) * kStride +
                         static_cast<size_t>(x) / 4U;
    const uint8_t shift = static_cast<uint8_t>((3 - (x & 0x03)) * 2);
    return static_cast<uint8_t>((buffer[index] >> shift) & 0x03U);
}

uint8_t logical_pixel_level(const std::vector<uint8_t> &buffer,
                            CanvasRotation rotation,
                            int x,
                            int y)
{
    int physical_x = x;
    int physical_y = y;
    if (rotation == CanvasRotation::Deg90CounterClockwise) {
        physical_x = y;
        physical_y = static_cast<int>(kHeight) - 1 - x;
    }
    return pixel_level(buffer, physical_x, physical_y);
}

// Verifies that visible label pixels have balanced top and bottom margins.
// 验证标签文字的可见像素拥有均衡的上下留白。
void assert_label_text_centered(const std::vector<uint8_t> &buffer,
                                CanvasRotation rotation,
                                int x,
                                int y,
                                int width,
                                int height,
                                GrayLevel ink_level)
{
    int first_ink_y = -1;
    int last_ink_y = -1;
    for (int logical_y = y + 1; logical_y < y + height - 1;
         ++logical_y) {
        for (int logical_x = x + 7; logical_x < x + width - 7;
             ++logical_x) {
            if (logical_pixel_level(buffer, rotation,
                                    logical_x, logical_y) !=
                static_cast<uint8_t>(ink_level)) {
                continue;
            }
            if (first_ink_y < 0) {
                first_ink_y = logical_y;
            }
            last_ink_y = logical_y;
        }
    }
    assert(first_ink_y >= 0);
    const int top_margin = first_ink_y - y;
    const int bottom_margin = y + height - 1 - last_ink_y;
    const int difference = top_margin >= bottom_margin
                               ? top_margin - bottom_margin
                               : bottom_margin - top_margin;
    assert(difference <= 1);
}

// Measures the complete visible composition instead of isolated components.
// 测量整组可见内容，而不是只检查其中某个局部元素。
void assert_content_vertically_centered(
    const std::vector<uint8_t> &buffer,
    CanvasRotation rotation,
    int width,
    int height)
{
    int first_ink_y = -1;
    int last_ink_y = -1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (logical_pixel_level(buffer, rotation, x, y) ==
                static_cast<uint8_t>(GrayLevel::White)) {
                continue;
            }
            if (first_ink_y < 0) {
                first_ink_y = y;
            }
            last_ink_y = y;
        }
    }
    assert(first_ink_y >= 0);
    const int top_margin = first_ink_y;
    const int bottom_margin = height - 1 - last_ink_y;
    const int difference = top_margin >= bottom_margin
                               ? top_margin - bottom_margin
                               : bottom_margin - top_margin;
    assert(difference <= 8);
}

void write_preview(const std::vector<uint8_t> &buffer, const char *path)
{
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << kWidth << " " << kHeight << "\n255\n";
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const uint8_t value =
                static_cast<uint8_t>(pixel_level(buffer, x, y) * 85U);
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
        }
    }
}

size_t count_level(const std::vector<uint8_t> &buffer, GrayLevel level)
{
    size_t count = 0U;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            if (pixel_level(buffer, x, y) ==
                static_cast<uint8_t>(level)) {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace

int main()
{
    std::vector<uint8_t> buffer(kStride * kHeight, 0xFFU);
    Canvas canvas(kWidth, kHeight, buffer.data(), buffer.size());
    StickyAppId selected_app = StickyAppId::DesktopPet;

    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    app_page_render_launcher(canvas, StickyAppId::DesktopPet);
    assert(canvas.rotation() == CanvasRotation::Deg90CounterClockwise);
    assert(count_level(buffer, GrayLevel::Black) > 7000U);
    assert(count_level(buffer, GrayLevel::LightGray) > 2000U);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 132, 331, selected_app));
    assert(selected_app == StickyAppId::DesktopPet);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 348, 331, selected_app));
    assert(selected_app == StickyAppId::Pomodoro);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 132, 581, selected_app));
    assert(selected_app == StickyAppId::StatusBoard);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 348, 581, selected_app));
    assert(selected_app == StickyAppId::BookOfAnswers);
    assert(!app_page_launcher_app_at(
        canvas.width(), canvas.height(), 240, 400, selected_app));
    assert(!app_page_launcher_app_at(
        canvas.width(), canvas.height(), 132, 451, selected_app));
    assert_label_text_centered(
        buffer, CanvasRotation::Deg90CounterClockwise,
        31, 394, 190, 54, GrayLevel::White);
    assert_label_text_centered(
        buffer, CanvasRotation::Deg90CounterClockwise,
        259, 394, 190, 54, GrayLevel::Black);
    assert_content_vertically_centered(
        buffer, CanvasRotation::Deg90CounterClockwise,
        canvas.width(), canvas.height());
    write_preview(buffer, "/tmp/sticky_launcher_portrait.ppm");

    canvas.set_rotation(CanvasRotation::Deg0);
    app_page_render_launcher(canvas, StickyAppId::DesktopPet);
    assert(canvas.rotation() == CanvasRotation::Deg0);
    assert(count_level(buffer, GrayLevel::Black) > 7000U);
    assert(count_level(buffer, GrayLevel::LightGray) > 2000U);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 109, 286, selected_app));
    assert(selected_app == StickyAppId::DesktopPet);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 303, 286, selected_app));
    assert(selected_app == StickyAppId::Pomodoro);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 497, 286, selected_app));
    assert(selected_app == StickyAppId::StatusBoard);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 691, 286, selected_app));
    assert(selected_app == StickyAppId::BookOfAnswers);
    assert(!app_page_launcher_app_at(
        canvas.width(), canvas.height(), 206, 286, selected_app));
    assert_label_text_centered(
        buffer, CanvasRotation::Deg0,
        18, 366, 176, 54, GrayLevel::White);
    assert_label_text_centered(
        buffer, CanvasRotation::Deg0,
        214, 366, 176, 54, GrayLevel::Black);
    assert_content_vertically_centered(
        buffer, CanvasRotation::Deg0,
        canvas.width(), canvas.height());
    write_preview(buffer, "/tmp/sticky_launcher_landscape.ppm");
    return 0;
}
