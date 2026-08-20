#include "desktop_pet_state.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "pet_dialogue.h"

namespace {

const PetCoreProfile &active_profile()
{
#if STICKY_DESKTOP_PET_TEST_MODE
    return pet_core_test_profile();
#else
    return pet_core_production_profile();
#endif
}

PetCoreAction core_action(DesktopPetAction action)
{
    switch (action) {
    case DesktopPetAction::Feed:
        return PetCoreAction::Feed;
    case DesktopPetAction::Pet:
        return PetCoreAction::Pet;
    case DesktopPetAction::Play:
        return PetCoreAction::Play;
    default:
        return PetCoreAction::Pet;
    }
}

DesktopPetPose action_pose(DesktopPetAction action)
{
    switch (action) {
    case DesktopPetAction::Feed:
        return DesktopPetPose::Feed;
    case DesktopPetAction::Pet:
        return DesktopPetPose::Pet;
    case DesktopPetAction::Play:
        return DesktopPetPose::Play;
    default:
        return DesktopPetPose::Idle;
    }
}

const char *rewarded_message(DesktopPetAction action)
{
    switch (action) {
    case DesktopPetAction::Feed:
        return "YUM! THAT WAS DELICIOUS!";
    case DesktopPetAction::Pet:
        return "THAT FEELS SO NICE!";
    case DesktopPetAction::Play:
        return "LET'S CHASE IT!";
    default:
        return "LET'S SPEND TODAY TOGETHER.";
    }
}

const char *companion_message(DesktopPetAction action)
{
    switch (action) {
    case DesktopPetAction::Feed:
        return "I'M FULL, BUT THANK YOU!";
    case DesktopPetAction::Pet:
        return "I LOVE YOUR GENTLE PATS.";
    case DesktopPetAction::Play:
        return "ONE MORE LITTLE HOP!";
    default:
        return "LET'S SPEND TODAY TOGETHER.";
    }
}

const char *personality_care_message(const DesktopPetState &state,
                                     DesktopPetAction action)
{
    if (state.pet.stage != PetLifeStage::Youth &&
        state.pet.stage != PetLifeStage::Adult) {
        return nullptr;
    }
    const bool adult = state.pet.stage == PetLifeStage::Adult;
    switch (state.pet.branch) {
    case PetPersonalityBranch::Foodie:
        if (action == DesktopPetAction::Feed) {
            return adult ? "I KNOW ALL THE BEST FLAVORS!"
                         : "I SAVED THE BEST BITE!";
        }
        if (action == DesktopPetAction::Pet) {
            return adult ? "A PAT AFTER DINNER IS PERFECT."
                         : "PATS MAKE SNACKS BETTER!";
        }
        return adult ? "LET'S ROLL OFF THAT SNACK!"
                     : "RACE YOU TO THE KITCHEN!";
    case PetPersonalityBranch::Affectionate:
        if (action == DesktopPetAction::Feed) {
            return adult ? "EVERY MEAL IS WARMER WITH YOU."
                         : "SHARING MAKES IT TASTIER!";
        }
        if (action == DesktopPetAction::Pet) {
            return adult ? "I'LL ALWAYS SAVE YOU A HUG."
                         : "STAY RIGHT HERE WITH ME.";
        }
        return adult ? "OUR PLAYTIME IS MY FAVORITE."
                     : "I LOVE PLAYING TOGETHER!";
    case PetPersonalityBranch::Active:
        if (action == DesktopPetAction::Feed) {
            return adult ? "TRAIL FUEL, THEN WE EXPLORE!"
                         : "FUEL FOR OUR NEXT RACE!";
        }
        if (action == DesktopPetAction::Pet) {
            return adult ? "EVEN EXPLORERS NEED A SOFT PAT."
                         : "A QUICK PAT, THEN WE RUN!";
        }
        return adult ? "THE NEXT TRAIL IS OURS!"
                     : "TRY TO KEEP UP!";
    case PetPersonalityBranch::Undecided:
    default:
        return nullptr;
    }
}

const char *stage_care_message(DesktopPetState &state,
                               DesktopPetAction action)
{
    const char *personality_message = personality_care_message(state, action);
    if (personality_message != nullptr) {
        return personality_message;
    }
    if (state.pet.stage != PetLifeStage::Child) {
        return nullptr;
    }
    PetDialogueContext context = PetDialogueContext::Pet;
    switch (action) {
    case DesktopPetAction::Feed:
        context = PetDialogueContext::Feed;
        break;
    case DesktopPetAction::Play:
        context = PetDialogueContext::Play;
        break;
    case DesktopPetAction::Pet:
    default:
        break;
    }
    const uint32_t random_value =
        static_cast<uint32_t>(state.pet.day) * 2654435761U +
        static_cast<uint32_t>(state.pet.growth) * 97U +
        static_cast<uint32_t>(action);
    const PetDialogueEntry *entry =
        pet_dialogue_pick(state.pet, context, random_value);
    return entry == nullptr ? nullptr : entry->text;
}

DesktopPetActionResult apply_care(DesktopPetState &state,
                                  DesktopPetAction action)
{
    const PetCoreActionResult core_result = pet_core_apply_action(
        state.pet, core_action(action), active_profile());
    state.pet.activity = PetActivity::Idle;

    DesktopPetActionResult result = {};
    result.changed = core_result.changed;
    result.rewarded = core_result.growth_delta > 0 ||
                      core_result.bond_delta > 0;
    result.growth_delta = static_cast<uint16_t>(
        std::max<int16_t>(core_result.growth_delta, 0));
    result.love_delta = static_cast<uint8_t>(
        std::max<int8_t>(core_result.bond_delta, 0));
    result.pose = action_pose(action);
    const char *stage_message = stage_care_message(state, action);
    result.message = stage_message != nullptr
                         ? stage_message
                         : (result.rewarded ? rewarded_message(action)
                                            : companion_message(action));
    if (core_result.evolution_became_ready) {
        if (state.pet.stage == PetLifeStage::Hatchling) {
            result.message = "I'M READY TO GROW!";
        } else if (state.pet.stage == PetLifeStage::Youth) {
            result.message = "I'M READY TO GROW UP!";
        } else {
            result.message = "NEXT STAGE IS READY!";
        }
    }
    return result;
}

// Selects urgent-state dialogue or a bond-matched conversation line.
// 选择紧急状态对白，或匹配当前亲密度的聊天台词。
DesktopPetActionResult apply_talk(DesktopPetState &state)
{
    if (state.pet.stage == PetLifeStage::Youth ||
        state.pet.stage == PetLifeStage::Adult) {
        const bool adult = state.pet.stage == PetLifeStage::Adult;
        const bool close_bond = state.pet.bond >= 70U;
        switch (state.pet.branch) {
        case PetPersonalityBranch::Foodie:
            return {true, false, 0U, 0U, DesktopPetPose::Idle,
                    adult ? (close_bond
                                 ? "YOU MADE EVERY SHARED MEAL SPECIAL."
                                 : "I LEARNED A NEW RECIPE FOR US.")
                          : (close_bond
                                 ? "I SAVED MY FAVORITE SNACK FOR YOU."
                                 : "WANT TO SHARE A SNACK?")};
        case PetPersonalityBranch::Affectionate:
            return {true, false, 0U, 0U, DesktopPetPose::Idle,
                    adult ? (close_bond
                                 ? "WE GREW UP SIDE BY SIDE."
                                 : "THIS HOME FEELS WARM WITH YOU.")
                          : (close_bond ? "YOU'RE MY SAFEST PLACE."
                                        : "CAN I STAY CLOSE?")};
        case PetPersonalityBranch::Active:
            return {true, false, 0U, 0U, DesktopPetPose::Idle,
                    adult ? (close_bond
                                 ? "YOU'RE MY FAVORITE ADVENTURE PARTNER."
                                 : "I FOUND A NEW TRAIL FOR US.")
                          : (close_bond
                                 ? "EVERY ADVENTURE IS BETTER WITH YOU."
                                 : "READY FOR OUR NEXT ADVENTURE?")};
        case PetPersonalityBranch::Undecided:
        default:
            break;
        }
    }
    PetDialogueContext context =
        pet_dialogue_context_for_state(state.pet);
    if (context == PetDialogueContext::Idle) {
        context = PetDialogueContext::Talk;
    }
    const uint32_t random_value =
        static_cast<uint32_t>(state.pet.day) * 2654435761U +
        static_cast<uint32_t>(state.pet.bond) * 97U +
        state.pet.recent_dialogue_ids[0];
    const PetDialogueEntry *entry = pet_dialogue_pick(
        state.pet, context, random_value);
    return {true,
            false,
            0U,
            0U,
            DesktopPetPose::Idle,
            entry == nullptr ? "I'M LISTENING." : entry->text};
}

}  // namespace

bool desktop_pet_state_set_name(DesktopPetState &state, const char *name)
{
    if (name == nullptr) {
        return false;
    }

    char normalized[kDesktopPetNameMaximumLength + 1U] = {};
    size_t output_length = 0U;
    bool pending_space = false;
    for (size_t index = 0U;
         name[index] != '\0' &&
         output_length < kDesktopPetNameMaximumLength;
         ++index) {
        const unsigned char raw = static_cast<unsigned char>(name[index]);
        if (raw == ' ') {
            pending_space = output_length > 0U;
            continue;
        }
        if (!std::isalnum(raw)) {
            continue;
        }
        if (pending_space &&
            output_length < kDesktopPetNameMaximumLength - 1U) {
            normalized[output_length++] = ' ';
        }
        pending_space = false;
        normalized[output_length++] = static_cast<char>(std::toupper(raw));
    }

    if (output_length == 0U) {
        return false;
    }
    normalized[output_length] = '\0';
    std::memcpy(state.name, normalized, sizeof(state.name));
    return true;
}

bool desktop_pet_state_has_name(const DesktopPetState &state)
{
    return state.name[0] != '\0';
}

DesktopPetActionResult desktop_pet_state_apply(DesktopPetState &state,
                                               DesktopPetAction action)
{
    if (state.pet.stage == PetLifeStage::Egg &&
        action != DesktopPetAction::Reset) {
        return {};
    }
    switch (action) {
    case DesktopPetAction::Feed:
    case DesktopPetAction::Pet:
    case DesktopPetAction::Play:
        return apply_care(state, action);
    case DesktopPetAction::Talk:
        return apply_talk(state);
    case DesktopPetAction::NextDay:
        desktop_pet_state_advance_day(state);
        return {true, false, 0U, 0U, DesktopPetPose::Idle,
                "A NEW DAY TOGETHER!"};
    case DesktopPetAction::AddGrowth: {
        DesktopPetActionResult result = {};
        result.changed = true;
        const uint16_t growth_before = state.pet.growth;
        const uint16_t growth_limit = desktop_pet_state_growth_limit(state);
        const uint16_t growth_remaining =
            state.pet.growth >= growth_limit
                ? 0U
                : static_cast<uint16_t>(growth_limit - state.pet.growth);
        result.growth_delta = static_cast<uint16_t>(std::min<uint16_t>(
            30U, growth_remaining));
        state.pet.growth = static_cast<uint16_t>(
            state.pet.growth + result.growth_delta);
        pet_core_sanitize(state.pet, active_profile());
        if (growth_before < growth_limit &&
            state.pet.growth >= growth_limit) {
            if (state.pet.stage == PetLifeStage::Hatchling) {
                result.message = "I'M READY TO GROW!";
            } else if (state.pet.stage == PetLifeStage::Youth) {
                result.message = "I'M READY TO GROW UP!";
            } else {
                result.message = "NEXT STAGE IS READY!";
            }
        } else if (result.growth_delta > 0U) {
            result.message = "I FEEL A LITTLE BIGGER!";
        } else {
            result.message = "GROWTH LIMIT REACHED.";
        }
        return result;
    }
    case DesktopPetAction::AddLove: {
        DesktopPetActionResult result = {};
        result.changed = true;
        result.love_delta = static_cast<uint8_t>(std::min<uint16_t>(
            20U, 100U - state.pet.bond));
        state.pet.bond = static_cast<uint8_t>(
            state.pet.bond + result.love_delta);
        result.message = "I FEEL SO LOVED!";
        return result;
    }
    case DesktopPetAction::Reset:
        state = {};
        return {true, false, 0U, 0U, DesktopPetPose::Idle,
                "LET'S SPEND TODAY TOGETHER."};
    case DesktopPetAction::OpenTest:
    case DesktopPetAction::OpenNameEditor:
    case DesktopPetAction::CloseTest:
    case DesktopPetAction::ChooseFoodie:
    case DesktopPetAction::ChooseAffectionate:
    case DesktopPetAction::ChooseActive:
    case DesktopPetAction::TapEgg:
    case DesktopPetAction::None:
        return {};
    }
    return {};
}

DesktopPetHatchResult desktop_pet_state_tap_egg(DesktopPetState &state)
{
    if (state.pet.stage != PetLifeStage::Egg) {
        return {};
    }

    state.hatch_taps = static_cast<uint8_t>(std::min<unsigned>(
        static_cast<unsigned>(state.hatch_taps) + 1U,
        kDesktopPetRequiredHatchTaps));
    if (state.hatch_taps < kDesktopPetRequiredHatchTaps) {
        return {true, false, state.hatch_taps};
    }

    // The newborn always starts from the documented Hatchling baseline.
    // 新生宠物始终从已定义的幼兔初始数值开始。
    state.pet = {};
    state.pet.stage = PetLifeStage::Hatchling;
    return {true, true, state.hatch_taps};
}

void desktop_pet_state_advance_day(DesktopPetState &state)
{
    if (state.pet.stage == PetLifeStage::Egg) {
        return;
    }
#if STICKY_DESKTOP_PET_TEST_MODE
    pet_core_advance_minutes(state.pet,
                             kDesktopPetTestNeedMinutesPerDay,
                             active_profile(),
                             false);
#endif
    pet_core_advance_day(state.pet);
}

DesktopPetEvolutionOutcome desktop_pet_state_evolve_if_ready(
    DesktopPetState &state)
{
    if (state.pet.stage != PetLifeStage::Hatchling &&
        state.pet.stage != PetLifeStage::Child &&
        state.pet.stage != PetLifeStage::Youth) {
        return DesktopPetEvolutionOutcome::None;
    }
    if (!pet_core_can_evolve(state.pet, active_profile())) {
        return DesktopPetEvolutionOutcome::None;
    }

    if (state.pet.stage == PetLifeStage::Child &&
        state.pet.branch == PetPersonalityBranch::Undecided) {
        const PetPersonalityDecision decision =
            pet_core_personality_decision(state.pet);
        if (decision.choice_required) {
            return DesktopPetEvolutionOutcome::ChoiceRequired;
        }
        if (!pet_core_choose_personality(
                state.pet, decision.automatic_branch)) {
            return DesktopPetEvolutionOutcome::None;
        }
    }

    if (!pet_core_evolve(state.pet, active_profile())) {
        return DesktopPetEvolutionOutcome::None;
    }
    state.pet.activity = PetActivity::Idle;
    return DesktopPetEvolutionOutcome::Evolved;
}

bool desktop_pet_state_choose_youth_branch(
    DesktopPetState &state,
    PetPersonalityBranch branch)
{
    if (state.pet.stage != PetLifeStage::Child ||
        !pet_core_can_evolve(state.pet, active_profile()) ||
        !pet_core_choose_personality(state.pet, branch) ||
        !pet_core_evolve(state.pet, active_profile())) {
        return false;
    }
    state.pet.activity = PetActivity::Idle;
    return true;
}

uint16_t desktop_pet_state_growth_limit(const DesktopPetState &state)
{
    switch (state.pet.stage) {
    case PetLifeStage::Egg:
        return kDesktopPetHatchlingGrowthLimit;
    case PetLifeStage::Adult:
    case PetLifeStage::Youth:
        return kDesktopPetYouthGrowthLimit;
    case PetLifeStage::Child:
        return kDesktopPetChildGrowthLimit;
    case PetLifeStage::Hatchling:
    default:
        return kDesktopPetHatchlingGrowthLimit;
    }
}

const char *desktop_pet_state_stage_label(const DesktopPetState &state)
{
    switch (state.pet.stage) {
    case PetLifeStage::Egg:
        return "EGG";
    case PetLifeStage::Adult:
        switch (state.pet.branch) {
        case PetPersonalityBranch::Foodie:
            return "FOODIE ADULT";
        case PetPersonalityBranch::Affectionate:
            return "HEART ADULT";
        case PetPersonalityBranch::Active:
            return "ACTIVE ADULT";
        case PetPersonalityBranch::Undecided:
        default:
            return "ADULT";
        }
    case PetLifeStage::Youth:
        switch (state.pet.branch) {
        case PetPersonalityBranch::Foodie:
            return "FOODIE YOUTH";
        case PetPersonalityBranch::Affectionate:
            return "HEART YOUTH";
        case PetPersonalityBranch::Active:
            return "ACTIVE YOUTH";
        case PetPersonalityBranch::Undecided:
        default:
            return "YOUTH";
        }
    case PetLifeStage::Child:
        return "CHILD";
    case PetLifeStage::Hatchling:
    default:
        return "HATCHLING";
    }
}

const char *desktop_pet_state_mood_label(const DesktopPetState &state)
{
    switch (pet_core_mood(state.pet)) {
    case PetMood::Ecstatic:
        return "ECSTATIC";
    case PetMood::Happy:
        return "HAPPY";
    case PetMood::Content:
        return "CONTENT";
    case PetMood::Neutral:
        return "NEUTRAL";
    case PetMood::Sad:
        return "SAD";
    case PetMood::Upset:
        return "UPSET";
    case PetMood::Miserable:
        return "MISERABLE";
    case PetMood::Hungry:
        return "HUNGRY";
    case PetMood::Tired:
        return "TIRED";
    case PetMood::Dirty:
        return "DIRTY";
    }
    return "NEUTRAL";
}

const char *desktop_pet_action_name(DesktopPetAction action)
{
    switch (action) {
    case DesktopPetAction::TapEgg:
        return "tap_egg";
    case DesktopPetAction::Feed:
        return "feed";
    case DesktopPetAction::Pet:
        return "pet";
    case DesktopPetAction::Talk:
        return "talk";
    case DesktopPetAction::Play:
        return "play";
    case DesktopPetAction::OpenNameEditor:
        return "open_name_editor";
    case DesktopPetAction::OpenTest:
        return "open_test";
    case DesktopPetAction::CloseTest:
        return "close_test";
    case DesktopPetAction::NextDay:
        return "next_day";
    case DesktopPetAction::AddGrowth:
        return "add_growth";
    case DesktopPetAction::AddLove:
        return "add_love";
    case DesktopPetAction::Reset:
        return "reset";
    case DesktopPetAction::ChooseFoodie:
        return "choose_foodie";
    case DesktopPetAction::ChooseAffectionate:
        return "choose_affectionate";
    case DesktopPetAction::ChooseActive:
        return "choose_active";
    case DesktopPetAction::None:
    default:
        return "none";
    }
}

const char *desktop_pet_pose_name(DesktopPetPose pose)
{
    switch (pose) {
    case DesktopPetPose::Feed:
        return "feed";
    case DesktopPetPose::Pet:
        return "pet";
    case DesktopPetPose::Play:
        return "play";
    case DesktopPetPose::Idle:
    default:
        return "idle";
    }
}
