#include "pet_idle_scheduler.h"

#include <array>

namespace {

bool was_recent(PetIdleAction action,
                PetIdleAction previous,
                PetIdleAction second_previous)
{
    return action == previous || action == second_previous;
}

}  // namespace

PetIdleAction pet_idle_select(const PetCoreState &state,
                              PetIdleAction previous,
                              PetIdleAction second_previous,
                              uint32_t random_value)
{
    const PetMood mood = pet_core_mood(state);
    const PetIdleAction urgent = mood == PetMood::Hungry
                                     ? PetIdleAction::Hungry
                                 : mood == PetMood::Tired
                                     ? PetIdleAction::Tired
                                     : PetIdleAction::None;
    if (urgent != PetIdleAction::None &&
        !was_recent(urgent, previous, second_previous) &&
        random_value % 3U != 0U) {
        return urgent;
    }

    constexpr std::array<PetIdleAction, 4> common = {
        PetIdleAction::Blink,
        PetIdleAction::EarTwitch,
        PetIdleAction::LookAround,
        PetIdleAction::Stretch,
    };
    const size_t start = random_value % common.size();
    for (size_t offset = 0U; offset < common.size(); ++offset) {
        const PetIdleAction candidate =
            common[(start + offset) % common.size()];
        if (!was_recent(candidate, previous, second_previous)) {
            return candidate;
        }
    }
    return common[start];
}

const char *pet_idle_message(PetIdleAction action,
                             PetLifeStage stage)
{
    if (stage == PetLifeStage::Adult) {
        switch (action) {
        case PetIdleAction::Blink:
            return "I'VE GROWN, BUT I'M STILL ME.";
        case PetIdleAction::EarTwitch:
            return "THESE EARS KNOW YOUR FOOTSTEPS.";
        case PetIdleAction::LookAround:
            return "OUR HOME HOLDS MANY MEMORIES.";
        case PetIdleAction::Stretch:
            return "ALL GROWN AND READY FOR TODAY!";
        case PetIdleAction::Hungry:
            return "GROWN RABBITS NEED SNACKS TOO.";
        case PetIdleAction::Tired:
            return "A QUIET REST BESIDE YOU SOUNDS NICE.";
        case PetIdleAction::None:
        default:
            return "WE GREW UP TOGETHER.";
        }
    }
    if (stage == PetLifeStage::Child) {
        switch (action) {
        case PetIdleAction::Blink:
            return "STILL QUICK ON MY FEET.";
        case PetIdleAction::EarTwitch:
            return "MY EARS ARE GETTING LONGER!";
        case PetIdleAction::LookAround:
            return "I CAN SEE MUCH FARTHER NOW.";
        case PetIdleAction::Stretch:
            return "LOOK HOW TALL I AM!";
        case PetIdleAction::Hungry:
            return "A GROWING BUN NEEDS SNACKS.";
        case PetIdleAction::Tired:
            return "BIG ADVENTURES NEED NAPS.";
        case PetIdleAction::None:
        default:
            return "I'M READY FOR AN ADVENTURE.";
        }
    }
    switch (action) {
    case PetIdleAction::Blink:
        return "JUST A HAPPY LITTLE BLINK.";
    case PetIdleAction::EarTwitch:
        return "DID YOU HEAR THAT?";
    case PetIdleAction::LookAround:
        return "WHAT'S OVER THERE?";
    case PetIdleAction::Stretch:
        return "BIG STRETCH!";
    case PetIdleAction::Hungry:
        return "MY TUMMY IS RUMBLING.";
    case PetIdleAction::Tired:
        return "RESTING MY EYES A MOMENT.";
    case PetIdleAction::None:
    default:
        return "LET'S SPEND TODAY TOGETHER.";
    }
}

uint32_t pet_idle_next_delay_ms(uint32_t random_value,
                                bool accelerated)
{
    const uint32_t minimum = accelerated ? 4000U : 12000U;
    const uint32_t spread = accelerated ? 4001U : 16001U;
    return minimum + random_value % spread;
}

const char *pet_idle_action_name(PetIdleAction action)
{
    switch (action) {
    case PetIdleAction::Blink:
        return "blink";
    case PetIdleAction::EarTwitch:
        return "ear_twitch";
    case PetIdleAction::LookAround:
        return "look_around";
    case PetIdleAction::Stretch:
        return "stretch";
    case PetIdleAction::Hungry:
        return "hungry";
    case PetIdleAction::Tired:
        return "tired";
    case PetIdleAction::None:
    default:
        return "none";
    }
}
