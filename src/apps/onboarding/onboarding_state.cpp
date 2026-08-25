#include "onboarding_state.h"

bool onboarding_state_apply(OnboardingState &state, OnboardingAction action)
{
    if (state.completed || action == OnboardingAction::None) {
        return false;
    }

    switch (action) {
    case OnboardingAction::Previous:
        if (state.page_index == 0U) {
            return false;
        }
        --state.page_index;
        return true;
    case OnboardingAction::Next:
        if (state.page_index + 1U >= kOnboardingPageCount) {
            return false;
        }
        ++state.page_index;
        return true;
    case OnboardingAction::Skip:
    case OnboardingAction::Finish:
        state.completed = true;
        return true;
    case OnboardingAction::None:
    default:
        return false;
    }
}

