#include <cassert>

#include "onboarding_state.h"

int main()
{
    OnboardingState state = {};
    assert(state.page_index == 0U);
    assert(!state.completed);
    assert(!onboarding_state_apply(state, OnboardingAction::Previous));

    for (uint8_t page = 1U; page < kOnboardingPageCount; ++page) {
        assert(onboarding_state_apply(state, OnboardingAction::Next));
        assert(state.page_index == page);
    }
    assert(!onboarding_state_apply(state, OnboardingAction::Next));
    assert(onboarding_state_apply(state, OnboardingAction::Previous));
    assert(state.page_index == kOnboardingPageCount - 2U);
    assert(onboarding_state_apply(state, OnboardingAction::Skip));
    assert(state.completed);
    assert(!onboarding_state_apply(state, OnboardingAction::Previous));

    OnboardingState finish_state = {};
    finish_state.page_index = kOnboardingPageCount - 1U;
    assert(onboarding_state_apply(finish_state, OnboardingAction::Finish));
    assert(finish_state.completed);
    return 0;
}
