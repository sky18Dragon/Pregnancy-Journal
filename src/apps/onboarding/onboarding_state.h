#pragma once

#include <cstdint>

constexpr uint8_t kOnboardingPageCount = 6U;

enum class OnboardingAction : uint8_t {
    None,
    Previous,
    Next,
    Skip,
    Finish,
};

struct OnboardingState {
    uint8_t page_index = 0U;
    bool completed = false;
};

// Applies one footer action and reports whether the page content changed.
// 应用一次底部导航操作，并返回页面内容是否发生变化。
bool onboarding_state_apply(OnboardingState &state, OnboardingAction action);
