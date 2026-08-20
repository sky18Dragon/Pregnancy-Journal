#pragma once

#include <cstddef>
#include <cstdint>

#include "core/pet_core.h"
#include "desktop_pet_outing.h"

#ifndef STICKY_DESKTOP_PET_TEST_MODE
#define STICKY_DESKTOP_PET_TEST_MODE 1
#endif

enum class DesktopPetAction : uint8_t {
    None,
    TapEgg,
    Feed,
    Pet,
    Talk,
    Play,
    Sleep,
    Wake,
    OpenNameEditor,
    OpenTest,
    CloseTest,
    NextDay,
    AddGrowth,
    AddLove,
    ReduceEnergy,
    StartOuting,
    CallHome,
    Reset,
    ChooseFoodie,
    ChooseAffectionate,
    ChooseActive,
};

enum class DesktopPetPose : uint8_t {
    Idle,
    Feed,
    Pet,
    Play,
};

// Describes the character performance that accompanies an accepted action.
// 描述一次有效互动正在呈现的角色表演。
enum class DesktopPetPerformance : uint8_t {
    None,
    Eating,
    ReceivingPet,
    Playing,
    Speaking,
    FallingAsleep,
    Waking,
};

enum class DesktopPetIdleFrame : uint8_t {
    Normal,
    Blink,
    EarTwitch,
    LookAround,
    Stretch,
    Hungry,
    Tired,
};

enum class DesktopPetEvolutionFrame : uint8_t {
    Starting,
    Silhouette,
    Revealed,
};

enum class DesktopPetEvolutionOutcome : uint8_t {
    None,
    Evolved,
    ChoiceRequired,
};

enum class DesktopPetHatchFrame : uint8_t {
    Resting,
    WobbleLeft,
    WobbleRight,
    Cracked,
    Opened,
};

constexpr uint32_t kDesktopPetStateVersion = 7U;
constexpr uint8_t kDesktopPetRequiredHatchTaps = 3U;
constexpr size_t kDesktopPetNameMaximumLength = 10U;

struct DesktopPetState {
    uint32_t version = kDesktopPetStateVersion;
    PetCoreState pet = {};
    uint8_t hatch_taps = 0U;
    char name[kDesktopPetNameMaximumLength + 1U] = {};
    DesktopPetOutingPlan outing_plan = {};

    DesktopPetState()
    {
        pet.stage = PetLifeStage::Egg;
    }
};

struct DesktopPetHatchResult {
    bool changed = false;
    bool hatched = false;
    uint8_t tap_count = 0U;
};

struct DesktopPetActionResult {
    bool changed = false;
    bool rewarded = false;
    uint16_t growth_delta = 0U;
    uint8_t love_delta = 0U;
    DesktopPetPose pose = DesktopPetPose::Idle;
    const char *message = "LET'S SPEND TODAY TOGETHER.";
    DesktopPetPerformance performance = DesktopPetPerformance::None;
};

constexpr uint16_t kDesktopPetHatchlingGrowthLimit = 30U;
constexpr uint16_t kDesktopPetChildGrowthLimit = 120U;
constexpr uint16_t kDesktopPetYouthGrowthLimit = 280U;
constexpr uint32_t kDesktopPetTestDayLengthMs = 120000U;
constexpr uint32_t kDesktopPetTestNeedMinutesPerDay = 10U;
constexpr uint8_t kDesktopPetLowEnergyThreshold = 10U;

// Applies one care or test action to the persistent pet state.
// 将一次照料或测试操作应用到可持久化的桌宠状态。
DesktopPetActionResult desktop_pet_state_apply(DesktopPetState &state,
                                               DesktopPetAction action);

// Records one accepted egg tap and creates the initial Hatchling on tap three.
// 记录一次有效的蛋触摸，并在第三次触摸时创建初始幼兔。
DesktopPetHatchResult desktop_pet_state_tap_egg(DesktopPetState &state);

// Starts a new simulated day and resets only the daily reward counters.
// 开始新的模拟日期，并只重置当天奖励计数。
void desktop_pet_state_advance_day(DesktopPetState &state);

// Resolves a ready stage transition or requests the Youth branch choice page.
// 处理已满足条件的阶段成长，或返回需要显示青年分支选择页。
DesktopPetEvolutionOutcome desktop_pet_state_evolve_if_ready(
    DesktopPetState &state);

// Saves one final Youth personality choice and immediately evolves the pet.
// 保存用户最终选择的青年性格路线，并立即完成进化。
bool desktop_pet_state_choose_youth_branch(
    DesktopPetState &state,
    PetPersonalityBranch branch);

// Stores a trimmed uppercase pet name containing letters, digits or spaces.
// 保存由字母、数字或空格组成并已整理格式的大写宠物名字。
bool desktop_pet_state_set_name(DesktopPetState &state, const char *name);

// Returns whether the pet already has a user-confirmed name.
// 返回宠物是否已经拥有用户确认过的名字。
bool desktop_pet_state_has_name(const DesktopPetState &state);

// Returns whether an awake pet has no energy and must rest before interacting.
// 返回清醒宠物是否已耗尽能量，并需要先休息才能继续互动。
bool desktop_pet_state_requires_sleep(const DesktopPetState &state);

// Returns whether an awake pet has 1-9 energy and should rest between actions.
// 返回清醒宠物是否只剩1到9点能量，并应在互动间隙保持休息。
bool desktop_pet_state_is_low_energy(const DesktopPetState &state);

uint16_t desktop_pet_state_growth_limit(const DesktopPetState &state);
const char *desktop_pet_state_stage_label(const DesktopPetState &state);

const char *desktop_pet_state_mood_label(const DesktopPetState &state);
const char *desktop_pet_action_name(DesktopPetAction action);
const char *desktop_pet_pose_name(DesktopPetPose pose);
