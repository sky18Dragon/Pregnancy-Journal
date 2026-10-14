#include <cassert>
#include <cmath>

#include "voice_intent.h"

int main()
{
    assert(voice_intent_from_command_id(1) == VoiceIntent::Weight);
    assert(voice_intent_from_command_id(4) == VoiceIntent::Memo);
    assert(voice_intent_from_command_id(99) == VoiceIntent::Unknown);

    const VoiceIntentCandidate accepted[] = {{2, 0.82F}, {1, 0.31F}};
    auto decision = voice_intent_decide(accepted, 2U);
    assert(decision.intent == VoiceIntent::Checkup);
    assert(!decision.needs_manual_selection);

    const VoiceIntentCandidate low[] = {{1, 0.59F}};
    decision = voice_intent_decide(low, 1U);
    assert(decision.intent == VoiceIntent::Unknown);
    assert(decision.needs_manual_selection);

    const VoiceIntentCandidate ambiguous[] = {{3, 0.80F}, {4, 0.72F}};
    decision = voice_intent_decide(ambiguous, 2U);
    assert(decision.intent == VoiceIntent::Unknown);

    const VoiceIntentCandidate duplicate_category[] = {
        {1, 0.79F}, {1, 0.72F}, {4, 0.50F}};
    decision = voice_intent_decide(duplicate_category, 3U);
    assert(decision.intent == VoiceIntent::Weight);
    assert(decision.runner_up_probability == 0.50F);

    const VoiceIntentCandidate invalid[] = {
        {99, 0.99F}, {1, NAN}, {2, 1.1F}};
    decision = voice_intent_decide(invalid, 3U);
    assert(decision.intent == VoiceIntent::Unknown);
    assert(voice_intent_decide(nullptr, 1U).needs_manual_selection);
}
