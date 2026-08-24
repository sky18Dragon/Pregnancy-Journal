#include <cassert>
#include <cstdint>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "pomodoro_pages.h"

namespace {

constexpr uint16_t kPhysicalWidth = 800;
constexpr uint16_t kPhysicalHeight = 480;
constexpr size_t kStride = kPhysicalWidth / 4U;

uint8_t pixel_level(const std::vector<uint8_t> &buffer, int x, int y)
{
    const size_t index = static_cast<size_t>(y) * kStride +
                         static_cast<size_t>(x) / 4U;
    const uint8_t shift = static_cast<uint8_t>((3 - (x & 0x03)) * 2);
    return static_cast<uint8_t>((buffer[index] >> shift) & 0x03U);
}

size_t black_pixel_count(const std::vector<uint8_t> &buffer)
{
    size_t count = 0U;
    for (int y = 0; y < kPhysicalHeight; ++y) {
        for (int x = 0; x < kPhysicalWidth; ++x) {
            if (pixel_level(buffer, x, y) ==
                static_cast<uint8_t>(GrayLevel::Black)) {
                ++count;
            }
        }
    }
    return count;
}

void write_preview(const std::vector<uint8_t> &buffer, const char *path)
{
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << kPhysicalWidth << " " << kPhysicalHeight
           << "\n255\n";
    for (int y = 0; y < kPhysicalHeight; ++y) {
        for (int x = 0; x < kPhysicalWidth; ++x) {
            const uint8_t value =
                static_cast<uint8_t>(pixel_level(buffer, x, y) * 85U);
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
        }
    }
}

}  // namespace

int main()
{
    std::vector<uint8_t> buffer(kStride * kPhysicalHeight, 0xFFU);
    Canvas canvas(kPhysicalWidth, kPhysicalHeight,
                  buffer.data(), buffer.size());
    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);

    pomodoro_page_render_setup(canvas, 900U);
    assert(canvas.width() == 480U);
    assert(canvas.height() == 800U);
    assert(black_pixel_count(buffer) > 15000U);
    assert(pomodoro_page_action_at(PomodoroPage::Setup, 92, 568) ==
           PomodoroAction::Preset15Minutes);
    assert(pomodoro_page_action_at(PomodoroPage::Setup, 239, 568) ==
           PomodoroAction::Preset25Minutes);
    assert(pomodoro_page_action_at(PomodoroPage::Setup, 387, 568) ==
           PomodoroAction::Preset60Minutes);
    assert(pomodoro_page_action_at(PomodoroPage::Setup, 240, 650) ==
           PomodoroAction::StartFocus);
    assert(pomodoro_page_action_at(PomodoroPage::Setup, 240, 730) ==
           PomodoroAction::OpenCustomTime);
    assert(pomodoro_page_action_at(PomodoroPage::Setup, 240, 598) ==
           PomodoroAction::None);
    write_preview(buffer, "/tmp/pomodoro_setup_15_minutes.ppm");

    pomodoro_page_render_setup(canvas, 1500U);
    write_preview(buffer, "/tmp/pomodoro_setup_25_minutes.ppm");
    pomodoro_page_render_setup(canvas, 3600U);
    write_preview(buffer, "/tmp/pomodoro_setup_60_minutes.ppm");

    pomodoro_page_render_custom_time(
        canvas, 0U, 25U, 0U, PomodoroTimeField::Minutes);
    assert(black_pixel_count(buffer) > 10000U);
    assert(pomodoro_page_action_at(PomodoroPage::CustomTime, 45, 780) ==
           PomodoroAction::Back);
    assert(pomodoro_page_action_at(PomodoroPage::CustomTime, 435, 780) ==
           PomodoroAction::Back);
    assert(pomodoro_page_action_at(PomodoroPage::CustomTime, 240, 729) ==
           PomodoroAction::None);
    assert(pomodoro_custom_action_can_batch(PomodoroAction::Digit2));
    assert(pomodoro_custom_action_can_batch(PomodoroAction::Clear));
    assert(pomodoro_custom_action_can_batch(PomodoroAction::Delete));
    assert(!pomodoro_custom_action_can_batch(PomodoroAction::UseCustomTime));
    assert(!pomodoro_custom_action_can_batch(PomodoroAction::Back));
    write_preview(buffer, "/tmp/pomodoro_custom_time.ppm");

    pomodoro_page_render_custom_time(
        canvas, 0U, 0U, 0U, PomodoroTimeField::Minutes);
    write_preview(buffer, "/tmp/pomodoro_custom_time_cleared.ppm");

    pomodoro_page_render_timer(
        canvas, PomodoroPage::Running, 1499U, 1500U);
    assert(black_pixel_count(buffer) > 12000U);
    write_preview(buffer, "/tmp/pomodoro_running.ppm");

    pomodoro_page_render_alarm(canvas, 1500U);
    assert(black_pixel_count(buffer) > 9000U);
    write_preview(buffer, "/tmp/pomodoro_alarm.ppm");
    return 0;
}
