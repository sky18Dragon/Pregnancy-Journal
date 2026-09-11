#include <cassert>
#include <cstdint>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "home_pages.h"
#include "ui_language.h"

namespace {

constexpr uint16_t kWidth = 800U;
constexpr uint16_t kHeight = 480U;
constexpr size_t kStride = kWidth / 4U;

size_t count_level(const std::vector<uint8_t> &buffer, GrayLevel level)
{
    size_t count = 0U;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const size_t index = static_cast<size_t>(y) * kStride +
                                 static_cast<size_t>(x) / 4U;
            const uint8_t shift =
                static_cast<uint8_t>((3 - (x & 0x03)) * 2);
            if (((buffer[index] >> shift) & 0x03U) ==
                static_cast<uint8_t>(level)) {
                ++count;
            }
        }
    }
    return count;
}

void write_preview(const std::vector<uint8_t> &buffer, const char *path)
{
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << kWidth << " " << kHeight << "\n255\n";
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const size_t index = static_cast<size_t>(y) * kStride +
                                 static_cast<size_t>(x) / 4U;
            const uint8_t shift =
                static_cast<uint8_t>((3 - (x & 0x03)) * 2);
            const uint8_t value = static_cast<uint8_t>(
                ((buffer[index] >> shift) & 0x03U) * 85U);
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
        }
    }
}

}  // namespace

int main()
{
    std::vector<uint8_t> buffer(kStride * kHeight, 0xFFU);
    Canvas canvas(kWidth, kHeight, buffer.data(), buffer.size());
    const HomePageData data = {
        true, 2026U, 9U, 11U, 21U, 32U, true, 82,
    };

    home_page_render(canvas, data);
    assert(canvas.rotation() == CanvasRotation::Deg0);
    assert(count_level(buffer, GrayLevel::Black) > 6000U);
    write_preview(buffer, "/tmp/sticky_core_home.ppm");

    ui_language_set(UiLanguage::ChineseSimplified);
    home_page_render(canvas, data);
    assert(count_level(buffer, GrayLevel::Black) > 6000U);
    write_preview(buffer, "/tmp/sticky_core_home_zh.ppm");

    home_page_render(canvas, {});
    assert(count_level(buffer, GrayLevel::Black) > 4000U);
    return 0;
}

