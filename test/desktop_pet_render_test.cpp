#include <cassert>
#include <cstdint>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "desktop_pet_pages.h"

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
    DesktopPetState state = {};

    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle,
        "LET'S SPEND TODAY TOGETHER.");
    assert(canvas.rotation() == CanvasRotation::Deg90CounterClockwise);
    assert(black_pixel_count(buffer) > 18000U);
    assert(desktop_pet_page_action_at(false, 80, 680) ==
           DesktopPetAction::Feed);
    assert(desktop_pet_page_action_at(false, 240, 680) ==
           DesktopPetAction::Pet);
    assert(desktop_pet_page_action_at(false, 400, 680) ==
           DesktopPetAction::Play);
    assert(desktop_pet_page_action_at(false, 440, 35) ==
           DesktopPetAction::OpenTest);
    write_preview(buffer, "/tmp/desktop_pet_home.ppm");

    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Feed,
        "YUM! THAT WAS DELICIOUS!");
    write_preview(buffer, "/tmp/desktop_pet_feed.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Pet,
        "THAT FEELS SO NICE!");
    write_preview(buffer, "/tmp/desktop_pet_pet.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Play,
        "LET'S CHASE IT!");
    write_preview(buffer, "/tmp/desktop_pet_play.ppm");

    state.pet.needs.food = 20U;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle,
        "CARROT. NOW. PLEASE.");
    write_preview(buffer, "/tmp/desktop_pet_hungry.ppm");

    desktop_pet_page_render_test(canvas, state, false);
    assert(desktop_pet_page_action_at(true, 240, 290) ==
           DesktopPetAction::NextDay);
    assert(desktop_pet_page_action_at(true, 240, 380) ==
           DesktopPetAction::AddGrowth);
    assert(desktop_pet_page_action_at(true, 240, 470) ==
           DesktopPetAction::AddLove);
    assert(desktop_pet_page_action_at(true, 240, 590) ==
           DesktopPetAction::Reset);
    write_preview(buffer, "/tmp/desktop_pet_test.ppm");
    return 0;
}
