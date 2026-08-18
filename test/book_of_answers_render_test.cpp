#include <cassert>
#include <cstdint>
#include <fstream>
#include <vector>

#include "book_of_answers_pages.h"
#include "canvas.h"

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
    std::vector<uint8_t> buffer(kStride * kPhysicalHeight, 0xFFU);
    Canvas canvas(kPhysicalWidth,
                  kPhysicalHeight,
                  buffer.data(),
                  buffer.size());

    book_of_answers_page_render_home(
        canvas, BookOfAnswersMode::Message);
    assert(canvas.rotation() == CanvasRotation::Deg90CounterClockwise);
    assert(black_pixel_count(buffer) > 10000U);
    assert(book_of_answers_page_action_at(
               BookOfAnswersPage::Home, 100, 640) ==
           BookOfAnswersAction::SelectMessage);
    assert(book_of_answers_page_action_at(
               BookOfAnswersPage::Home, 350, 640) ==
           BookOfAnswersAction::SelectCrystal);
    assert(book_of_answers_page_action_at(
               BookOfAnswersPage::Home, 240, 730) ==
           BookOfAnswersAction::Start);
    write_preview(buffer, "/tmp/book_home.ppm");

    book_of_answers_page_render_shaking(
        canvas, BookOfAnswersShakeFrame::Left);
    write_preview(buffer, "/tmp/book_shake_left.ppm");
    book_of_answers_page_render_shaking(
        canvas, BookOfAnswersShakeFrame::Right);
    write_preview(buffer, "/tmp/book_shake_right.ppm");
    book_of_answers_page_render_thinking(canvas);
    write_preview(buffer, "/tmp/book_thinking.ppm");
    book_of_answers_page_render_revealing(canvas);
    write_preview(buffer, "/tmp/book_revealing.ppm");

    book_of_answers_page_render_message_result(
        canvas, "TRUST YOUR", "INSTINCTS");
    assert(book_of_answers_page_action_at(
               BookOfAnswersPage::MessageResult, 240, 710) ==
           BookOfAnswersAction::AskAgain);
    assert(book_of_answers_page_action_at(
               BookOfAnswersPage::MessageResult, 240, 770) ==
           BookOfAnswersAction::End);
    write_preview(buffer, "/tmp/book_message_result.ppm");

    book_of_answers_page_render_crystal_result(canvas, "UNCLEAR");
    assert(black_pixel_count(buffer) > 10000U);
    write_preview(buffer, "/tmp/book_crystal_result.ppm");
    return 0;
}
