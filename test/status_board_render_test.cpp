#include <cassert>
#include <cstdint>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "status_board_pages.h"
#include "status_pet_animation.h"

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

uint8_t logical_pixel_level(const std::vector<uint8_t> &buffer,
                            int x,
                            int y)
{
    return pixel_level(buffer,
                       static_cast<int>(kWidth) - 1 - x,
                       static_cast<int>(kHeight) - 1 - y);
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

void assert_bottom_band_is_clear(const std::vector<uint8_t> &buffer)
{
    for (int y = 415; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            assert(logical_pixel_level(buffer, x, y) ==
                   static_cast<uint8_t>(GrayLevel::Black));
        }
    }
}

size_t black_pixel_count(const std::vector<uint8_t> &buffer,
                         int top,
                         int bottom)
{
    size_t count = 0U;
    for (int y = top; y < bottom; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            if (logical_pixel_level(buffer, x, y) ==
                static_cast<uint8_t>(GrayLevel::Black)) {
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
    canvas.set_rotation(CanvasRotation::Deg180);

    status_board_page_render_menu(canvas, StatusBoardStatus::InMeeting);
    const StatusPetFrame &left_frame = status_pet_frame(0U);
    status_board_page_render_menu_pet(
        canvas, left_frame.pose, left_frame.center_x);
    assert(black_pixel_count(buffer, 340, 480) > 100U);
    assert(logical_pixel_level(buffer, 20, 456) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(logical_pixel_level(buffer, 779, 457) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(logical_pixel_level(buffer, 19, 456) ==
           static_cast<uint8_t>(GrayLevel::White));
    assert(logical_pixel_level(buffer, 780, 456) ==
           static_cast<uint8_t>(GrayLevel::White));
    write_preview(buffer, "/tmp/status_board_menu.ppm");

    constexpr size_t kPreviewFrames[] = {2U, 4U, 5U, 6U, 8U};
    constexpr const char *kPreviewPaths[] = {
        "/tmp/status_pet_jump_left.ppm",
        "/tmp/status_pet_jump_right.ppm",
        "/tmp/status_pet_celebrate.ppm",
        "/tmp/status_pet_walk_right.ppm",
        "/tmp/status_pet_walk_left.ppm",
    };
    for (size_t index = 0U; index < 5U; ++index) {
        const StatusPetFrame &frame = status_pet_frame(kPreviewFrames[index]);
        status_board_page_render_menu_pet(
            canvas, frame.pose, frame.center_x);
        assert(black_pixel_count(buffer, 340, 480) > 100U);
        write_preview(buffer, kPreviewPaths[index]);
    }

    status_board_page_render_display(
        canvas, StatusBoardStatus::Focusing, "");
    assert_bottom_band_is_clear(buffer);
    write_preview(buffer, "/tmp/status_board_display.ppm");

    constexpr StatusBoardStatus kStatuses[] = {
        StatusBoardStatus::Focusing,
        StatusBoardStatus::InMeeting,
        StatusBoardStatus::Welcome,
        StatusBoardStatus::OutForLunch,
        StatusBoardStatus::OffDuty,
        StatusBoardStatus::Custom,
    };
    constexpr const char *kDisplayPaths[] = {
        "/tmp/status_display_focusing.ppm",
        "/tmp/status_display_in_meeting.ppm",
        "/tmp/status_display_welcome.ppm",
        "/tmp/status_display_out_for_lunch.ppm",
        "/tmp/status_display_off_duty.ppm",
        "/tmp/status_display_custom.ppm",
    };
    constexpr const char *kDisplaySecondaryPaths[] = {
        "/tmp/status_display_focusing_secondary.ppm",
        "/tmp/status_display_in_meeting_secondary.ppm",
        "/tmp/status_display_welcome_secondary.ppm",
        "/tmp/status_display_out_for_lunch_secondary.ppm",
        "/tmp/status_display_off_duty_secondary.ppm",
        "/tmp/status_display_custom_secondary.ppm",
    };
    for (size_t index = 0; index < 6U; ++index) {
        status_board_page_render_display(
            canvas, kStatuses[index], "DEEP WORK MODE");
        assert_bottom_band_is_clear(buffer);
        write_preview(buffer, kDisplayPaths[index]);

        const std::vector<uint8_t> primary_buffer = buffer;
        status_board_page_render_display_pet(
            canvas, kStatuses[index], true);
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < 460; ++x) {
                assert(logical_pixel_level(buffer, x, y) ==
                       logical_pixel_level(primary_buffer, x, y));
            }
        }
        assert_bottom_band_is_clear(buffer);
        write_preview(buffer, kDisplaySecondaryPaths[index]);
    }

    status_board_page_render_custom_input(canvas,
                                          "DEEP WORK MODE",
                                          StatusBoardKeyboardMode::Letters,
                                          false);

    assert(canvas.rotation() == CanvasRotation::Deg180);
    assert(logical_pixel_level(buffer, 145, 20) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(logical_pixel_level(buffer, 600, 420) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(logical_pixel_level(buffer, 400, 170) ==
           static_cast<uint8_t>(GrayLevel::White));

    write_preview(buffer, "/tmp/status_board_preview.ppm");
    return 0;
}
