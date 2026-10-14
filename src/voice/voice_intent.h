#pragma once

#include <cstddef>
#include <cstdint>

enum class VoiceIntent : uint8_t {
    Unknown = 0,
    Weight,
    Checkup,
    Habit,
    Memo,
};

struct VoiceIntentCandidate {
    int command_id;
    float probability;
};

struct VoiceIntentDecision {
    VoiceIntent intent;
    float probability;
    float runner_up_probability;
    bool needs_manual_selection;
};

constexpr float kVoiceIntentMinimumProbability = 0.60F;
constexpr float kVoiceIntentMinimumMargin = 0.12F;

VoiceIntent voice_intent_from_command_id(int command_id);
const char *voice_intent_name(VoiceIntent intent);
VoiceIntentDecision voice_intent_decide(
    const VoiceIntentCandidate *candidates,
    size_t candidate_count,
    float minimum_probability = kVoiceIntentMinimumProbability,
    float minimum_margin = kVoiceIntentMinimumMargin);
