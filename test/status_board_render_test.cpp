#include <cassert>
#include <cstdint>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "status_board_pages.h"

namespace {

constexpr uint16_t kWidth = 800;
constexpr uint16_t kHeight = 480;
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
            const uint8_t level = pixel_level(buffer, x, y);
            const uint8_t value = static_cast<uint8_t>(level * 85U);
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

    status_board_page_render_menu(canvas, StatusBoardStatus::InMeeting);
    write_preview(buffer, "/tmp/status_board_menu.ppm");

    status_board_page_render_display(
        canvas, StatusBoardStatus::Focusing, "");
    write_preview(buffer, "/tmp/status_board_display.ppm");

    status_board_page_render_custom_input(canvas,
                                          "DEEP WORK MODE",
                                          StatusBoardKeyboardMode::Letters,
                                          false);

    assert(canvas.rotation() == CanvasRotation::Deg0);
    assert(pixel_level(buffer, 145, 20) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(pixel_level(buffer, 600, 420) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(pixel_level(buffer, 400, 170) ==
           static_cast<uint8_t>(GrayLevel::White));

    write_preview(buffer, "/tmp/status_board_preview.ppm");
    return 0;
}
