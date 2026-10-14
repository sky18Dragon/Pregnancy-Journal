#include "voice_intent.h"

#include <cmath>

VoiceIntent voice_intent_from_command_id(int command_id)
{
    switch (command_id) {
    case 1:
        return VoiceIntent::Weight;
    case 2:
        return VoiceIntent::Checkup;
    case 3:
        return VoiceIntent::Habit;
    case 4:
        return VoiceIntent::Memo;
    default:
        return VoiceIntent::Unknown;
    }
}

const char *voice_intent_name(VoiceIntent intent)
{
    switch (intent) {
    case VoiceIntent::Weight:
        return "WEIGHT";
    case VoiceIntent::Checkup:
        return "CHECKUP";
    case VoiceIntent::Habit:
        return "HABIT";
    case VoiceIntent::Memo:
        return "MEMO";
    case VoiceIntent::Unknown:
    default:
        return "UNKNOWN";
    }
}

VoiceIntentDecision voice_intent_decide(const VoiceIntentCandidate *candidates,
                                        size_t candidate_count,
                                        float minimum_probability,
                                        float minimum_margin)
{
    VoiceIntent best_intent = VoiceIntent::Unknown;
    float best_probability = 0.0F;
    float runner_up_probability = 0.0F;
    if (candidates == nullptr || !std::isfinite(minimum_probability) ||
        !std::isfinite(minimum_margin) || minimum_probability < 0.0F ||
        minimum_probability > 1.0F || minimum_margin < 0.0F ||
        minimum_margin > 1.0F) {
        return {VoiceIntent::Unknown, 0.0F, 0.0F, true};
    }

    for (size_t index = 0; index < candidate_count; ++index) {
        const VoiceIntent intent =
            voice_intent_from_command_id(candidates[index].command_id);
        const float probability = candidates[index].probability;
        if (intent == VoiceIntent::Unknown || !std::isfinite(probability) ||
            probability < 0.0F || probability > 1.0F) {
            continue;
        }
        if (intent == best_intent) {
            if (probability > best_probability) {
                best_probability = probability;
            }
            continue;
        }
        if (probability > best_probability) {
            runner_up_probability = best_probability;
            best_probability = probability;
            best_intent = intent;
        } else if (probability > runner_up_probability) {
            runner_up_probability = probability;
        }
    }

    const bool accepted = best_intent != VoiceIntent::Unknown &&
                          best_probability >= minimum_probability &&
                          best_probability - runner_up_probability >= minimum_margin;
    return {accepted ? best_intent : VoiceIntent::Unknown,
            best_probability,
            runner_up_probability,
            !accepted};
}
