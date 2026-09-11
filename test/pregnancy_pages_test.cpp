#include <cassert>
#include <cstdint>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "pregnancy_pages.h"
#include "ui_language.h"

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

}  // namespace

int main()
{
    std::vector<uint8_t> buffer(kStride * kHeight, 0xFFU);
    Canvas canvas(kWidth, kHeight, buffer.data(), buffer.size());

    pregnancy_page_render_clock_setup(
        canvas, "202609111530", false, false);
    assert(count_level(buffer, GrayLevel::Black) > 5000U);
    assert(pregnancy_page_action_at(
               PregnancyPage::ClockSetup, false, 542, 100) ==
           PregnancyAction::Digit1);
    assert(pregnancy_page_action_at(
               PregnancyPage::ClockSetup, false, 730, 330) ==
           PregnancyAction::Continue);
    assert(pregnancy_page_action_at(
               PregnancyPage::ClockSetup, false, 100, 420) ==
           PregnancyAction::None);
    assert(pregnancy_page_action_at(
               PregnancyPage::ClockSetup, true, 100, 420) ==
           PregnancyAction::Back);
    write_preview(buffer, "/tmp/pregnancy_clock_setup.ppm");

    pregnancy_page_render_due_date_setup(
        canvas, "20270318", true, true);
    assert(count_level(buffer, GrayLevel::Black) > 5000U);
    write_preview(buffer, "/tmp/pregnancy_due_date_setup.ppm");

    PregnancyProgress progress = {};
    progress.weeks = 14U;
    progress.days = 3U;
    progress.percent = 36U;
    progress.gestational_days = 101U;
    progress.stage = PregnancyStage::SecondTrimester;
    pregnancy_page_render_dashboard(
        canvas, {2027U, 3U, 18U}, progress);
    assert(count_level(buffer, GrayLevel::Black) > 8000U);
    assert(count_level(buffer, GrayLevel::LightGray) > 5000U);
    assert(pregnancy_page_action_at(
               PregnancyPage::Dashboard, true, 710, 445) ==
           PregnancyAction::Edit);
    assert(pregnancy_page_action_at(
               PregnancyPage::Dashboard, true, 400, 200) ==
           PregnancyAction::None);
    write_preview(buffer, "/tmp/pregnancy_dashboard.ppm");

    ui_language_set(UiLanguage::ChineseSimplified);
    pregnancy_page_render_dashboard(
        canvas, {2027U, 3U, 18U}, progress);
    assert(count_level(buffer, GrayLevel::Black) > 8000U);
    write_preview(buffer, "/tmp/pregnancy_dashboard_zh.ppm");

    char digit = '\0';
    assert(pregnancy_action_digit(PregnancyAction::Digit0, digit));
    assert(digit == '0');
    assert(pregnancy_action_digit(PregnancyAction::Digit9, digit));
    assert(digit == '9');
    assert(!pregnancy_action_digit(PregnancyAction::Delete, digit));
    return 0;
}
