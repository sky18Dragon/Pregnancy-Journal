#include <cassert>
#include <cstring>

#include "pregnancy_week_assets.h"
#include "pregnancy_week_visual.h"

int main()
{
    for (uint8_t week = 6U; week <= 40U; ++week) {
        const PregnancyWeekVisual &visual = pregnancy_week_visual(week);
        const PixelAsset &asset = pregnancy_week_size_asset(week);
        assert(visual.week == week);
        assert(visual.object_en != nullptr && visual.object_en[0] != '\0');
        assert(visual.object_zh != nullptr && visual.object_zh[0] != '\0');
        assert(visual.sentence_en != nullptr && visual.sentence_en[0] != '\0');
        assert(visual.sentence_zh != nullptr && visual.sentence_zh[0] != '\0');
        assert(asset.width == 148U && asset.height == 148U);
        assert(asset.data != nullptr);
        for (uint8_t other = 6U; other < week; ++other) {
            const PregnancyWeekVisual &previous = pregnancy_week_visual(other);
            assert(std::strcmp(visual.object_en, previous.object_en) != 0);
            assert(std::strcmp(visual.sentence_en, previous.sentence_en) != 0);
            assert(std::strcmp(visual.sentence_zh, previous.sentence_zh) != 0);
            assert(asset.data != pregnancy_week_size_asset(other).data);
        }
    }
    assert(pregnancy_week_visual(0U).week == 6U);
    assert(pregnancy_week_visual(41U).week == 40U);
    assert(pregnancy_week_size_asset(0U).data ==
           pregnancy_week_size_asset(6U).data);
    assert(pregnancy_week_size_asset(41U).data ==
           pregnancy_week_size_asset(40U).data);
    return 0;
}
