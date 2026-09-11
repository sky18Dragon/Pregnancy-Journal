#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <utility>
#include <vector>

#include "app_pages.h"
#include "app_launcher_assets.h"
#include "canvas.h"
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

bool asset_pixel_set(const PixelAsset &asset, int x, int y)
{
    const size_t stride = (asset.width + 7U) / 8U;
    const size_t index = static_cast<size_t>(y) * stride +
                         static_cast<size_t>(x) / 8U;
    const uint8_t mask = static_cast<uint8_t>(
        1U << (7U - static_cast<uint8_t>(x & 0x07)));
    return (asset.data[index] & mask) != 0U;
}

AppLauncherAssetId launcher_asset_id(StickyAppId app)
{
    switch (app) {
    case StickyAppId::Home:
        return AppLauncherAssetId::Pet;
    case StickyAppId::DesktopPet:
        return AppLauncherAssetId::Pet;
    case StickyAppId::Pomodoro:
        return AppLauncherAssetId::Focus;
    case StickyAppId::StatusBoard:
        return AppLauncherAssetId::Status;
    case StickyAppId::BookOfAnswers:
        return AppLauncherAssetId::Answers;
    case StickyAppId::Pregnancy:
        return AppLauncherAssetId::Pregnancy;
    }
    return AppLauncherAssetId::Pet;
}

std::pair<int, int> selection_probe(const AppLauncherStickerAsset &asset)
{
    // Uses the right-side outer ring, away from the separate corner marker.
    // 选取贴纸右侧的外圈像素，避开独立绘制的左上角选择标记。
    for (int x = static_cast<int>(asset.selection.width) - 1;
         x >= static_cast<int>(asset.selection.width) / 2;
         --x) {
        for (int y = 0; y < static_cast<int>(asset.selection.height); ++y) {
            if (asset_pixel_set(asset.selection, x, y) &&
                !asset_pixel_set(asset.gray, x, y) &&
                !asset_pixel_set(asset.black, x, y)) {
                return {x, y};
            }
        }
    }
    assert(false);
    return {0, 0};
}

void assert_selection_follows_current_app(
    Canvas &canvas,
    const std::vector<uint8_t> &buffer,
    CanvasRotation rotation,
    const std::array<std::pair<int, int>, 5> &asset_origins)
{
    constexpr std::array<StickyAppId, 5> kApps = {
        StickyAppId::DesktopPet,
        StickyAppId::Pomodoro,
        StickyAppId::StatusBoard,
        StickyAppId::BookOfAnswers,
        StickyAppId::Pregnancy,
    };

    for (size_t selected_index = 0; selected_index < kApps.size();
         ++selected_index) {
        app_page_render_launcher(canvas, kApps[selected_index]);
        for (size_t card_index = 0; card_index < kApps.size(); ++card_index) {
            const AppLauncherStickerAsset &asset =
                app_launcher_sticker_asset(
                    launcher_asset_id(kApps[card_index]));
            const auto probe = selection_probe(asset);
            const int x = asset_origins[card_index].first + probe.first;
            const int y = asset_origins[card_index].second + probe.second;
            const GrayLevel expected = card_index == selected_index
                                           ? GrayLevel::Black
                                           : GrayLevel::White;
            assert(logical_pixel_level(buffer, rotation, x, y) ==
                   static_cast<uint8_t>(expected));
        }
    }
}

}  // namespace

int main()
{
    std::vector<uint8_t> buffer(kStride * kHeight, 0xFFU);
    Canvas canvas(kWidth, kHeight, buffer.data(), buffer.size());

#if STICKY_ONBOARDING_TEST_MODE
    assert(app_page_launcher_tutorial_at(480, 800, 455, 112));
    assert(app_page_launcher_tutorial_at(800, 480, 775, 74));
    assert(!app_page_launcher_tutorial_at(480, 800, 240, 112));
#endif
    StickyAppId selected_app = StickyAppId::Home;
    assert(app_page_launcher_language_at(480, 800, 430, 48));
    assert(app_page_launcher_language_at(800, 480, 752, 76));
    assert(!app_page_launcher_language_at(800, 480, 700, 76));

    canvas.set_rotation(CanvasRotation::Deg90CounterClockwise);
    app_page_render_launcher(canvas, StickyAppId::Home);
    assert(canvas.rotation() == CanvasRotation::Deg90CounterClockwise);
    assert(count_level(buffer, GrayLevel::Black) > 7000U);
    assert(count_level(buffer, GrayLevel::LightGray) > 2000U);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 126, 223, selected_app));
    assert(selected_app == StickyAppId::Home);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 354, 223, selected_app));
    assert(selected_app == StickyAppId::DesktopPet);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 126, 451, selected_app));
    assert(selected_app == StickyAppId::Pomodoro);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 354, 451, selected_app));
    assert(selected_app == StickyAppId::StatusBoard);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 126, 679, selected_app));
    assert(selected_app == StickyAppId::BookOfAnswers);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 354, 679, selected_app));
    assert(selected_app == StickyAppId::Pregnancy);
    assert(!app_page_launcher_app_at(
        canvas.width(), canvas.height(), 240, 223, selected_app));
    assert(!app_page_launcher_app_at(
        canvas.width(), canvas.height(), 126, 324, selected_app));
    assert_label_text_centered(
        buffer, CanvasRotation::Deg90CounterClockwise,
        31, 266, 190, 54, GrayLevel::White);
    assert_label_text_centered(
        buffer, CanvasRotation::Deg90CounterClockwise,
        259, 266, 190, 54, GrayLevel::Black);
    assert_content_vertically_centered(
        buffer, CanvasRotation::Deg90CounterClockwise,
        canvas.width(), canvas.height());
    assert_selection_follows_current_app(
        canvas,
        buffer,
        CanvasRotation::Deg90CounterClockwise,
        {{{286, 128}, {58, 356}, {286, 356}, {58, 584},
          {286, 584}}});
    app_page_render_launcher(canvas, StickyAppId::Home);
    write_preview(buffer, "/tmp/sticky_launcher_portrait.ppm");

    canvas.set_rotation(CanvasRotation::Deg0);
    app_page_render_launcher(canvas, StickyAppId::Home);
    assert(canvas.rotation() == CanvasRotation::Deg0);
    assert(count_level(buffer, GrayLevel::Black) > 7000U);
    assert(count_level(buffer, GrayLevel::LightGray) > 2000U);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 72, 286, selected_app));
    assert(selected_app == StickyAppId::Home);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 203, 286, selected_app));
    assert(selected_app == StickyAppId::DesktopPet);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 334, 286, selected_app));
    assert(selected_app == StickyAppId::Pomodoro);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 465, 286, selected_app));
    assert(selected_app == StickyAppId::StatusBoard);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 596, 286, selected_app));
    assert(selected_app == StickyAppId::BookOfAnswers);
    assert(app_page_launcher_app_at(
        canvas.width(), canvas.height(), 727, 286, selected_app));
    assert(selected_app == StickyAppId::Pregnancy);
    assert(!app_page_launcher_app_at(
        canvas.width(), canvas.height(), 138, 286, selected_app));
    assert_label_text_centered(
        buffer, CanvasRotation::Deg0,
        12, 366, 120, 54, GrayLevel::White);
    assert_label_text_centered(
        buffer, CanvasRotation::Deg0,
        143, 366, 120, 54, GrayLevel::Black);
    assert_content_vertically_centered(
        buffer, CanvasRotation::Deg0,
        canvas.width(), canvas.height());
    assert_selection_follows_current_app(
        canvas,
        buffer,
        CanvasRotation::Deg0,
        {{{135, 190}, {266, 190}, {397, 190}, {528, 190},
          {659, 190}}});
    app_page_render_launcher(canvas, StickyAppId::Home);
    write_preview(buffer, "/tmp/sticky_launcher_landscape.ppm");
    ui_language_set(UiLanguage::ChineseSimplified);
    app_page_render_launcher(canvas, StickyAppId::Pregnancy);
    write_preview(buffer, "/tmp/sticky_launcher_landscape_zh.ppm");
    return 0;
}
