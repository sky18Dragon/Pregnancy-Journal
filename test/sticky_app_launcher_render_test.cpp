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
        canvas.width(), canvas.height(), 132, 291, selected_app));
    assert(selected_app == StickyAppId::DesktopPet);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 348, 291, selected_app));
    assert(selected_app == StickyAppId::Pomodoro);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 132, 541, selected_app));
    assert(selected_app == StickyAppId::StatusBoard);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 348, 541, selected_app));
    assert(selected_app == StickyAppId::BookOfAnswers);
    assert(!app_page_launcher_app_at(
        canvas.width(), canvas.height(), 240, 400, selected_app));
    assert(!app_page_launcher_app_at(
        canvas.width(), canvas.height(), 132, 406, selected_app));
    write_preview(buffer, "/tmp/sticky_launcher_portrait.ppm");

    canvas.set_rotation(CanvasRotation::Deg0);
    app_page_render_launcher(canvas, StickyAppId::DesktopPet);
    assert(canvas.rotation() == CanvasRotation::Deg0);
    assert(count_level(buffer, GrayLevel::Black) > 7000U);
    assert(count_level(buffer, GrayLevel::LightGray) > 2000U);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 109, 256, selected_app));
    assert(selected_app == StickyAppId::DesktopPet);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 303, 256, selected_app));
    assert(selected_app == StickyAppId::Pomodoro);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 497, 256, selected_app));
    assert(selected_app == StickyAppId::StatusBoard);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 691, 256, selected_app));
    assert(selected_app == StickyAppId::BookOfAnswers);
    assert(!app_page_launcher_app_at(
        canvas.width(), canvas.height(), 206, 256, selected_app));
    write_preview(buffer, "/tmp/sticky_launcher_landscape.ppm");
    return 0;
}
