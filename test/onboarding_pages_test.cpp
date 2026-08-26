#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "canvas.h"
#include "onboarding_assets.h"
#include "onboarding_pages.h"

namespace {

constexpr uint16_t kPhysicalWidth = 800U;
constexpr uint16_t kPhysicalHeight = 480U;
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

size_t logical_black_pixel_count(const Canvas &canvas,
                                 int left,
                                 int top,
                                 int right,
                                 int bottom)
{
    size_t count = 0U;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            if (canvas.pixel_at(x, y) == GrayLevel::Black) {
                ++count;
            }
        }
    }
    return count;
}

void write_preview(const Canvas &canvas, const std::string &path)
{
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << canvas.width() << " " << canvas.height()
           << "\n255\n";
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            const uint8_t value =
                static_cast<uint8_t>(canvas.pixel_at(x, y)) * 85U;
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
    Canvas canvas(kPhysicalWidth,
                  kPhysicalHeight,
                  buffer.data(),
                  buffer.size());
    static_assert(kOnboardingPageCount == kOnboardingAssetCount);

    std::array<size_t, kOnboardingPageCount> ink_counts = {};
    for (uint8_t page = 0U; page < kOnboardingPageCount; ++page) {
        onboarding_page_render(canvas, page);
        assert(canvas.rotation() == CanvasRotation::Deg90CounterClockwise);
        assert(canvas.width() == 480U);
        assert(canvas.height() == 800U);
        ink_counts[page] = black_pixel_count(buffer);
        assert(ink_counts[page] > 10000U);
        assert(logical_black_pixel_count(canvas, 8, 18, 18, 713) >
               1500U);
        assert(logical_black_pixel_count(canvas, 461, 18, 471, 713) >
               1500U);
        assert(logical_black_pixel_count(canvas, 8, 714, 18, 798) >
               120U);
        assert(logical_black_pixel_count(canvas, 461, 714, 471, 798) >
               120U);
        if (page == 2U) {
            assert(logical_black_pixel_count(canvas, 78, 703, 402, 713) <
                   50U);
        }
        if (page == 1U) {
            assert(logical_black_pixel_count(canvas, 138, 373, 184, 394) >
                   35U);
            assert(logical_black_pixel_count(canvas, 173, 382, 184, 394) >
                   7U);
        }
        if (page == 3U) {
            assert(logical_black_pixel_count(canvas, 24, 30, 80, 86) >
                   500U);
            assert(logical_black_pixel_count(canvas, 20, 100, 460, 680) >
                   5000U);
        }
        write_preview(canvas,
                      "/tmp/onboarding_" + std::to_string(page + 1U) +
                          ".ppm");
    }

    assert(onboarding_page_action_at(0U, 60, 750) ==
           OnboardingAction::None);
    assert(onboarding_page_action_at(1U, 60, 750) ==
           OnboardingAction::Previous);
    assert(onboarding_page_action_at(0U, 240, 780) ==
           OnboardingAction::Skip);
    assert(onboarding_page_action_at(1U, 240, 780) ==
           OnboardingAction::Skip);
    assert(onboarding_page_action_at(0U, 60, 780) ==
           OnboardingAction::None);
    assert(onboarding_page_action_at(0U, 420, 750) ==
           OnboardingAction::Next);
    assert(onboarding_page_action_at(5U, 420, 750) ==
           OnboardingAction::Finish);
    assert(onboarding_page_action_at(0U, 240, 750) ==
           OnboardingAction::None);
    assert(onboarding_page_action_at(0U, 420, 700) ==
           OnboardingAction::None);
    assert(onboarding_page_action_at(0U, -1, 760) ==
           OnboardingAction::None);
    assert(onboarding_page_action_at(0U, 480, 760) ==
           OnboardingAction::None);

    int logical_x = 0;
    int logical_y = 0;
    canvas.physical_to_logical(780, 239, logical_x, logical_y);
    assert(logical_x == 240);
    assert(logical_y == 780);
    assert(onboarding_page_action_at(0U, logical_x, logical_y) ==
           OnboardingAction::Skip);
    assert(onboarding_page_action_at(1U, logical_x, logical_y) ==
           OnboardingAction::Skip);
    canvas.physical_to_logical(750, 419, logical_x, logical_y);
    assert(logical_x == 60);
    assert(logical_y == 750);
    assert(onboarding_page_action_at(0U, logical_x, logical_y) ==
           OnboardingAction::None);
    assert(onboarding_page_action_at(1U, logical_x, logical_y) ==
           OnboardingAction::Previous);
    canvas.physical_to_logical(750, 59, logical_x, logical_y);
    assert(logical_x == 420);
    assert(logical_y == 750);
    assert(onboarding_page_action_at(5U, logical_x, logical_y) ==
           OnboardingAction::Finish);
    return 0;
}
