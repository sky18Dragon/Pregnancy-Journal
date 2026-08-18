#include <cassert>
#include <cstdint>
#include <vector>

#include "canvas.h"
#include "pixel_asset.h"
#include "status_bunny_assets.h"

namespace {

uint8_t pixel_level(const std::vector<uint8_t> &buffer,
                    uint16_t width,
                    int x,
                    int y)
{
    const size_t stride = (width + 3U) / 4U;
    const size_t index = static_cast<size_t>(y) * stride +
                         static_cast<size_t>(x) / 4U;
    const uint8_t shift = static_cast<uint8_t>((3 - (x & 0x03)) * 2);
    return static_cast<uint8_t>((buffer[index] >> shift) & 0x03U);
}

}  // namespace

int main()
{
    constexpr uint16_t kWidth = 16;
    constexpr uint16_t kHeight = 16;
    static constexpr uint8_t kData[] = {
        0x80U,
        0x40U,
    };
    static constexpr PixelAsset kAsset = {8, 2, kData};

    std::vector<uint8_t> buffer((kWidth / 4U) * kHeight, 0xFFU);
    Canvas canvas(kWidth, kHeight, buffer.data(), buffer.size());
    pixel_asset_draw(canvas, 2, 3, kAsset, 2, GrayLevel::Black);

    assert(pixel_level(buffer, kWidth, 2, 3) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(pixel_level(buffer, kWidth, 3, 4) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(pixel_level(buffer, kWidth, 4, 5) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(pixel_level(buffer, kWidth, 5, 6) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(pixel_level(buffer, kWidth, 4, 3) ==
           static_cast<uint8_t>(GrayLevel::White));

    canvas.clear(GrayLevel::Black);
    pixel_asset_draw_centered(
        canvas, 8, 8, kAsset, 1, GrayLevel::White);
    assert(pixel_level(buffer, kWidth, 4, 7) ==
           static_cast<uint8_t>(GrayLevel::White));
    assert(pixel_level(buffer, kWidth, 5, 8) ==
           static_cast<uint8_t>(GrayLevel::White));

    pixel_asset_draw(canvas, -1, -1, kAsset, 1, GrayLevel::White);

    const PixelAsset &focusing =
        status_bunny_asset(StatusBunnyAssetId::Focusing);
    const PixelAsset &meeting =
        status_bunny_asset(StatusBunnyAssetId::InMeeting);
    assert(focusing.width == 80U);
    assert(focusing.height == 80U);
    assert(focusing.data != nullptr);
    assert(meeting.data != focusing.data);

    constexpr StatusBunnyAssetId kStatusIds[] = {
        StatusBunnyAssetId::Focusing,
        StatusBunnyAssetId::InMeeting,
        StatusBunnyAssetId::Welcome,
        StatusBunnyAssetId::OutForLunch,
        StatusBunnyAssetId::OffDuty,
        StatusBunnyAssetId::Custom,
    };
    for (StatusBunnyAssetId id : kStatusIds) {
        const PixelAsset &primary = status_bunny_animation_asset(
            id, StatusBunnyAnimationFrame::Primary);
        const PixelAsset &secondary = status_bunny_animation_asset(
            id, StatusBunnyAnimationFrame::Secondary);
        assert(primary.width == 160U);
        assert(primary.height == 160U);
        assert(secondary.width == 160U);
        assert(secondary.height == 160U);
        assert(primary.data != secondary.data);
    }
    return 0;
}
