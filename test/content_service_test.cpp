#include <cassert>

#include "content_service.h"

int main()
{
    const int weeks[] = {1, 12, 24, 40, -7, 99};
    for (const int week : weeks) {
        const WeekContent content = pregnancy_content_for_week(week);
        assert(content.week >= 1U && content.week <= 40U);
        assert(content.baby_size_text != nullptr);
        assert(content.baby_weight_text != nullptr);
        assert(content.baby_development != nullptr);
        assert(content.mother_changes != nullptr);
        assert(content.daily_advice != nullptr);
        assert(content.baby_development_zh != nullptr);
        assert(content.mother_changes_zh != nullptr);
        assert(content.daily_advice_zh != nullptr);
    }
    assert(pregnancy_content_for_week(-7).week == 1U);
    assert(pregnancy_content_for_week(99).week == 40U);
    return 0;
}
