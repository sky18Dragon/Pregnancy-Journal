#include "pet_dialogue.h"

#include <array>
#include <cstring>

namespace {

constexpr PetLifeStage kFirstStage = PetLifeStage::Hatchling;
constexpr PetLifeStage kLastStage = PetLifeStage::Elder;

constexpr PetDialogueEntry kEntries[] = {
    {1000U, PetDialogueContext::Idle, kFirstStage, kLastStage, 0U, 100U,
     "TWITCHING MY NOSE."},
    {1001U, PetDialogueContext::Idle, kFirstStage, kLastStage, 0U, 100U,
     "JUST VIBING."},
    {1002U, PetDialogueContext::Idle, kFirstStage, kLastStage, 0U, 100U,
     "DID SOMEBODY SAY CARROT?"},
    {1003U, PetDialogueContext::Idle, kFirstStage, kLastStage, 0U, 100U,
     "I'M 90% EARS."},
    {1004U, PetDialogueContext::Idle, kFirstStage, kLastStage, 0U, 100U,
     "PLOTTING SOMETHING. PROBABLY."},
    {1005U, PetDialogueContext::Idle, kFirstStage, kLastStage, 0U, 100U,
     "THE GARDEN WAS A MISTAKE."},
    {1006U, PetDialogueContext::Idle, kFirstStage, kLastStage, 0U, 100U,
     "HOPPING IS CONTROLLED FALLING."},
    {1007U, PetDialogueContext::Idle, kFirstStage, kLastStage, 0U, 100U,
     "LOOKING CUTE ON PURPOSE."},
    {1008U, PetDialogueContext::Idle, kFirstStage, kLastStage, 0U, 100U,
     "I'M A REGISTERED HOPSTER."},
    {1010U, PetDialogueContext::Walk, kFirstStage, kLastStage, 0U, 100U,
     "HOP HOP HOP."},
    {1011U, PetDialogueContext::Walk, kFirstStage, kLastStage, 0U, 100U,
     "GOING SOMEWHERE IMPORTANT."},
    {1012U, PetDialogueContext::Walk, kFirstStage, kLastStage, 0U, 100U,
     "PRETENDING I HAVE A DESTINATION."},
    {1013U, PetDialogueContext::Walk, kFirstStage, kLastStage, 0U, 100U,
     "WABBIT ON THE MOVE!"},
    {1014U, PetDialogueContext::Walk, kFirstStage, kLastStage, 0U, 100U,
     "RUN FAST. THINK LATER."},
    {1015U, PetDialogueContext::Walk, kFirstStage, kLastStage, 0U, 100U,
     "RECONNAISSANCE MISSION."},
    {1020U, PetDialogueContext::Sleep, kFirstStage, kLastStage, 0U, 100U,
     "ZZZ... CARROTS... ZZZ..."},
    {1021U, PetDialogueContext::Sleep, kFirstStage, kLastStage, 0U, 100U,
     "*TWITCHES EAR IN DREAM*"},
    {1022U, PetDialogueContext::Sleep, kFirstStage, kLastStage, 0U, 100U,
     "DREAMING OF AN OPEN FRIDGE."},
    {1023U, PetDialogueContext::Sleep, kFirstStage, kLastStage, 0U, 100U,
     "I SLEEP WITH ONE EYE OPEN."},
    {1030U, PetDialogueContext::Hungry, kFirstStage, kLastStage, 0U, 100U,
     "CARROT. NOW. PLEASE."},
    {1031U, PetDialogueContext::Hungry, kFirstStage, kLastStage, 0U, 100U,
     "I REQUIRE PRODUCE."},
    {1032U, PetDialogueContext::Hungry, kFirstStage, kLastStage, 0U, 100U,
     "MY STOMACH HAS OPINIONS."},
    {1033U, PetDialogueContext::Hungry, kFirstStage, kLastStage, 0U, 100U,
     "EMPTY RABBIT, SAD RABBIT."},
    {1040U, PetDialogueContext::Sad, kFirstStage, kLastStage, 0U, 100U,
     "I MISS THE GARDEN."},
    {1041U, PetDialogueContext::Sad, kFirstStage, kLastStage, 0U, 100U,
     "EARS ARE HEAVY TODAY."},
    {1042U, PetDialogueContext::Sad, kFirstStage, kLastStage, 0U, 100U,
     "COULD USE A HEAD PAT."},
    {1050U, PetDialogueContext::Tired, kFirstStage, kLastStage, 0U, 100U,
     "NEED... BURROW..."},
    {1051U, PetDialogueContext::Tired, kFirstStage, kLastStage, 0U, 100U,
     "HOPPING IS EXHAUSTING."},
    {1052U, PetDialogueContext::Tired, kFirstStage, kLastStage, 0U, 100U,
     "JUST FIVE MORE MINUTES."},
    {1060U, PetDialogueContext::Feed, kFirstStage, kLastStage, 0U, 100U,
     "YUM YUM!"},
    {1061U, PetDialogueContext::Pet, kFirstStage, kLastStage, 0U, 100U,
     "THAT FEELS SO NICE!"},
    {1080U, PetDialogueContext::Talk, kFirstStage, kLastStage, 0U, 34U,
     "I'M STILL GETTING TO KNOW YOU."},
    {1081U, PetDialogueContext::Talk, kFirstStage, kLastStage, 0U, 34U,
     "YOU CAN SIT A LITTLE CLOSER."},
    {1082U, PetDialogueContext::Talk, kFirstStage, kLastStage, 0U, 34U,
     "MY EARS HEAR EVERYTHING."},
    {1083U, PetDialogueContext::Talk, kFirstStage, kLastStage, 0U, 34U,
     "DID YOU BRING A CARROT?"},
    {1084U, PetDialogueContext::Talk, kFirstStage, kLastStage, 0U, 34U,
     "LET'S TAKE IT SLOW."},
    {1085U, PetDialogueContext::Talk, kFirstStage, kLastStage, 0U, 34U,
     "I'M LISTENING."},
    {1090U, PetDialogueContext::Talk, kFirstStage, kLastStage, 35U, 69U,
     "I'M GLAD YOU CAME BACK."},
    {1091U, PetDialogueContext::Talk, kFirstStage, kLastStage, 35U, 69U,
     "TELL ME ABOUT YOUR DAY."},
    {1092U, PetDialogueContext::Talk, kFirstStage, kLastStage, 35U, 69U,
     "SHALL WE HOP SOMEWHERE?"},
    {1093U, PetDialogueContext::Talk, kFirstStage, kLastStage, 35U, 69U,
     "YOU MAKE THIS ROOM FEEL WARM."},
    {1094U, PetDialogueContext::Talk, kFirstStage, kLastStage, 35U, 69U,
     "I SAVED THIS SPOT FOR YOU."},
    {1095U, PetDialogueContext::Talk, kFirstStage, kLastStage, 35U, 69U,
     "LET'S SPEND MORE TIME TOGETHER."},
    {1100U, PetDialogueContext::Talk, kFirstStage, kLastStage, 70U, 100U,
     "YOU'RE MY FAVORITE PERSON."},
    {1101U, PetDialogueContext::Talk, kFirstStage, kLastStage, 70U, 100U,
     "I TRUST YOU WITH MY CARROTS."},
    {1102U, PetDialogueContext::Talk, kFirstStage, kLastStage, 70U, 100U,
     "EVERY DAY IS BETTER WITH YOU."},
    {1103U, PetDialogueContext::Talk, kFirstStage, kLastStage, 70U, 100U,
     "I KNEW YOU WOULD COME BACK."},
    {1104U, PetDialogueContext::Talk, kFirstStage, kLastStage, 70U, 100U,
     "LET'S STAY TOGETHER A WHILE."},
    {1105U, PetDialogueContext::Talk, kFirstStage, kLastStage, 70U, 100U,
     "HOME IS WHERE YOU ARE."},
    {1062U, PetDialogueContext::Play, kFirstStage, kLastStage, 0U, 100U,
     "ONE MORE LITTLE HOP!"},
    {1063U, PetDialogueContext::Clean, kFirstStage, kLastStage, 0U, 100U,
     "FRESH AND FLUFFY!"},
    {1070U, PetDialogueContext::EvolutionReady, kFirstStage,
     PetLifeStage::Youth, 0U, 100U, "I'M READY TO GROW!"},
    {1071U, PetDialogueContext::EvolutionComplete, PetLifeStage::Child,
     PetLifeStage::Adult, 0U, 100U, "LOOK! I GREW!"},
    {1200U, PetDialogueContext::Idle, PetLifeStage::Child,
     PetLifeStage::Child, 0U, 100U, "I'M READY FOR AN ADVENTURE."},
    {1201U, PetDialogueContext::Idle, PetLifeStage::Child,
     PetLifeStage::Child, 0U, 100U, "MY LEGS FEEL SO SPRINGY!"},
    {1202U, PetDialogueContext::Idle, PetLifeStage::Child,
     PetLifeStage::Child, 0U, 100U, "I CAN REACH HIGHER NOW."},
    {1203U, PetDialogueContext::Idle, PetLifeStage::Child,
     PetLifeStage::Child, 0U, 100U, "WHERE SHOULD WE EXPLORE?"},
    {1210U, PetDialogueContext::Feed, PetLifeStage::Child,
     PetLifeStage::Child, 0U, 100U, "ENERGY FOR ADVENTURES!"},
    {1211U, PetDialogueContext::Pet, PetLifeStage::Child,
     PetLifeStage::Child, 0U, 100U, "MY FUR GREW EXTRA SOFT!"},
    {1212U, PetDialogueContext::Play, PetLifeStage::Child,
     PetLifeStage::Child, 0U, 100U, "CATCH ME IF YOU CAN!"},
    {1220U, PetDialogueContext::Talk, PetLifeStage::Child,
     PetLifeStage::Child, 0U, 34U, "I'M LEARNING NEW THINGS."},
    {1221U, PetDialogueContext::Talk, PetLifeStage::Child,
     PetLifeStage::Child, 0U, 34U, "WILL YOU SHOW ME AROUND?"},
    {1222U, PetDialogueContext::Talk, PetLifeStage::Child,
     PetLifeStage::Child, 0U, 34U, "MY LEGS ARE STILL WOBBLY."},
    {1230U, PetDialogueContext::Talk, PetLifeStage::Child,
     PetLifeStage::Child, 35U, 69U, "LET'S GO SOMEWHERE TOGETHER."},
    {1231U, PetDialogueContext::Talk, PetLifeStage::Child,
     PetLifeStage::Child, 35U, 69U, "I SAVED MY BEST HOP FOR YOU."},
    {1232U, PetDialogueContext::Talk, PetLifeStage::Child,
     PetLifeStage::Child, 35U, 69U, "YOU MAKE ME FEEL BRAVE."},
    {1240U, PetDialogueContext::Talk, PetLifeStage::Child,
     PetLifeStage::Child, 70U, 100U, "WE'RE THE BEST ADVENTURE TEAM."},
    {1241U, PetDialogueContext::Talk, PetLifeStage::Child,
     PetLifeStage::Child, 70U, 100U, "I'LL ALWAYS HOP BACK TO YOU."},
    {1242U, PetDialogueContext::Talk, PetLifeStage::Child,
     PetLifeStage::Child, 70U, 100U, "YOU HELPED ME GROW THIS STRONG."},
};

bool stage_in_range(PetLifeStage value,
                    PetLifeStage minimum,
                    PetLifeStage maximum)
{
    return static_cast<uint8_t>(value) >= static_cast<uint8_t>(minimum) &&
           static_cast<uint8_t>(value) <= static_cast<uint8_t>(maximum);
}

bool was_recent(const PetCoreState &state, uint16_t id)
{
    for (uint16_t recent : state.recent_dialogue_ids) {
        if (recent == id) {
            return true;
        }
    }
    return false;
}

void remember(PetCoreState &state, uint16_t id)
{
    for (size_t index = kPetRecentDialogueCount - 1U; index > 0U; --index) {
        state.recent_dialogue_ids[index] =
            state.recent_dialogue_ids[index - 1U];
    }
    state.recent_dialogue_ids[0] = id;
}

bool eligible(const PetDialogueEntry &entry,
              const PetCoreState &state,
              PetDialogueContext context,
              bool allow_recent)
{
    return entry.context == context &&
           stage_in_range(state.stage,
                          entry.minimum_stage,
                          entry.maximum_stage) &&
           state.bond >= entry.minimum_bond &&
           state.bond <= entry.maximum_bond &&
           (allow_recent || !was_recent(state, entry.id));
}

}  // namespace

const PetDialogueEntry *pet_dialogue_pick(PetCoreState &state,
                                          PetDialogueContext context,
                                          uint32_t random_value)
{
    constexpr size_t count = sizeof(kEntries) / sizeof(kEntries[0]);
    for (bool allow_recent : {false, true}) {
        for (bool stage_specific : {true, false}) {
            std::array<size_t, count> matches = {};
            size_t match_count = 0U;
            for (size_t index = 0U; index < count; ++index) {
                const bool exact_stage =
                    kEntries[index].minimum_stage == state.stage &&
                    kEntries[index].maximum_stage == state.stage;
                if (exact_stage != stage_specific) {
                    continue;
                }
                if (eligible(kEntries[index], state, context, allow_recent)) {
                    matches[match_count++] = index;
                }
            }
            if (match_count > 0U) {
                const size_t selected = matches[random_value % match_count];
                remember(state, kEntries[selected].id);
                return &kEntries[selected];
            }
        }
    }
    if (context != PetDialogueContext::Idle) {
        return pet_dialogue_pick(state, PetDialogueContext::Idle, random_value);
    }
    return nullptr;
}

PetDialogueContext pet_dialogue_context_for_state(const PetCoreState &state)
{
    if (state.activity == PetActivity::Sleeping) {
        return PetDialogueContext::Sleep;
    }
    switch (pet_core_mood(state)) {
    case PetMood::Hungry:
        return PetDialogueContext::Hungry;
    case PetMood::Tired:
        return PetDialogueContext::Tired;
    case PetMood::Sad:
    case PetMood::Upset:
    case PetMood::Miserable:
        return PetDialogueContext::Sad;
    case PetMood::Ecstatic:
    case PetMood::Happy:
    case PetMood::Content:
    case PetMood::Neutral:
    case PetMood::Dirty:
    default:
        return PetDialogueContext::Idle;
    }
}

const PetDialogueEntry *pet_dialogue_entries(size_t &count)
{
    count = sizeof(kEntries) / sizeof(kEntries[0]);
    return kEntries;
}

size_t pet_dialogue_text_bytes()
{
    size_t bytes = 0U;
    for (const PetDialogueEntry &entry : kEntries) {
        bytes += std::strlen(entry.text) + 1U;
    }
    return bytes;
}
