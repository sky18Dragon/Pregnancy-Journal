#include "desktop_pet_app.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "desktop_pet_pages.h"
#include "desktop_pet_sound_cues.h"
#include "desktop_pet_state.h"
#include "desktop_pet_storage.h"
#include "pet_animation_queue.h"
#include "pet_dialogue.h"
#include "pet_idle_scheduler.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sticky_buzzer.h"
#include "sticky_display.h"
#include "sticky_touch.h"

#ifndef STICKY_LOG_DESKTOP_PET_ENABLED
#define STICKY_LOG_DESKTOP_PET_ENABLED 0
#endif

namespace {

constexpr char kTag[] = "desktop_pet_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(30);
constexpr uint32_t kTaskStackSize = 6144;
constexpr UBaseType_t kTaskPriority = 3;
constexpr int64_t kActionPoseHoldUs = 1300000LL;
constexpr int64_t kTalkMessageHoldUs = 4000000LL;
constexpr int64_t kEvolutionStartingHoldUs = 1300000LL;
constexpr int64_t kEvolutionSilhouetteHoldUs = 1400000LL;
constexpr int64_t kEvolutionRevealHoldUs = 2600000LL;
constexpr int64_t kHatchWobbleHoldUs = 420000LL;
constexpr int64_t kHatchCrackHoldUs = 900000LL;
constexpr int64_t kHatchWelcomeHoldUs = 2400000LL;
#if STICKY_DESKTOP_PET_TEST_MODE
constexpr int64_t kSleepFrameHoldUs = 2400000LL;
constexpr uint32_t kSleepMinutesPerFrame = 2U;
#else
// TODO(rtc): Replace fixed sleep ticks with validated PCF8563 elapsed time.
// TODO(rtc): 使用校验后的PCF8563经过时间替换固定睡眠节拍。
constexpr int64_t kSleepFrameHoldUs = 60000000LL;
constexpr uint32_t kSleepMinutesPerFrame = 1U;
#endif
constexpr char kHomeMessage[] = "LET'S SPEND TODAY TOGETHER.";

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
DesktopPetState s_state = {};
DesktopPetPose s_pose = DesktopPetPose::Idle;
DesktopPetIdleFrame s_idle_frame = DesktopPetIdleFrame::Normal;
const char *s_message = kHomeMessage;
const char *s_home_message = kHomeMessage;
bool s_test_open = false;
bool s_personality_choice_open = false;
bool s_name_editor_open = false;
bool s_name_required = false;
bool s_name_input_error = false;
DesktopPetKeyboardMode s_name_keyboard_mode =
    DesktopPetKeyboardMode::Letters;
char s_name_text[kDesktopPetNameMaximumLength + 1U] = {};
size_t s_name_text_length = 0U;
bool s_reset_confirmation = false;
int64_t s_pose_deadline_us = 0;
int64_t s_idle_next_us = 0;
PetAnimationQueue s_idle_animation;
PetIdleAction s_previous_idle_action = PetIdleAction::None;
PetIdleAction s_second_previous_idle_action = PetIdleAction::None;
bool s_evolution_active = false;
DesktopPetEvolutionFrame s_evolution_frame =
    DesktopPetEvolutionFrame::Starting;
int64_t s_evolution_deadline_us = 0;
bool s_hatch_active = false;
bool s_hatch_final = false;
DesktopPetHatchFrame s_hatch_frame = DesktopPetHatchFrame::Resting;
int64_t s_hatch_deadline_us = 0;
bool s_sleep_secondary_frame = false;
int64_t s_sleep_deadline_us = 0;
#if STICKY_DESKTOP_PET_TEST_MODE
int64_t s_day_deadline_us = 0;
#endif

const PetCoreProfile &sleep_profile()
{
#if STICKY_DESKTOP_PET_TEST_MODE
    return pet_core_test_profile();
#else
    return pet_core_production_profile();
#endif
}

void play_visible_pet_sound(const DesktopPetSoundCue &cue,
                            const char *event)
{
    if (!cue.audible()) {
        return;
    }
    const esp_err_t result = sticky_buzzer_play_pattern(
        cue.pattern, cue.variant);
    if (result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "pet=sound event=%s state=failed result=%s",
                    event,
                    esp_err_to_name(result));
    }
}

const char *select_youth_home_message(uint32_t value)
{
    switch (s_state.pet.branch) {
    case PetPersonalityBranch::Foodie: {
        constexpr const char *kLines[] = {
            "I PACKED A SNACK FOR US.",
            "I CAN SMELL A CARROT NEARBY.",
            "SHALL WE TRY A NEW TREAT?",
        };
        return kLines[value % 3U];
    }
    case PetPersonalityBranch::Affectionate: {
        constexpr const char *kLines[] = {
            "I'M ALWAYS CLOSE BY.",
            "THIS FEELS LIKE HOME.",
            "I SAVED YOU A WARM HUG.",
        };
        return kLines[value % 3U];
    }
    case PetPersonalityBranch::Active: {
        constexpr const char *kLines[] = {
            "LET'S RACE TOGETHER!",
            "A NEW ADVENTURE AWAITS!",
            "I'M READY TO MOVE!",
        };
        return kLines[value % 3U];
    }
    case PetPersonalityBranch::Undecided:
    default:
        return nullptr;
    }
}

// Selects one persistent home line for the current Adult personality.
// 为当前成年性格选择一条主页常驻对白。
const char *select_adult_home_message(uint32_t value)
{
    switch (s_state.pet.branch) {
    case PetPersonalityBranch::Foodie: {
        constexpr const char *kLines[] = {
            "I PERFECTED A RECIPE FOR US.",
            "GOOD FOOD MAKES A WARM HOME.",
            "I SAVED THE CRUNCHIEST CARROT.",
        };
        return kLines[value % 3U];
    }
    case PetPersonalityBranch::Affectionate: {
        constexpr const char *kLines[] = {
            "WE GREW UP SIDE BY SIDE.",
            "MY HEART IS ALWAYS WITH YOU.",
            "THERE'S ALWAYS ROOM FOR A HUG.",
        };
        return kLines[value % 3U];
    }
    case PetPersonalityBranch::Active: {
        constexpr const char *kLines[] = {
            "I MAPPED A NEW TRAIL FOR US.",
            "BIG ADVENTURES START AT HOME.",
            "MY BACKPACK IS ALWAYS READY.",
        };
        return kLines[value % 3U];
    }
    case PetPersonalityBranch::Undecided:
    default:
        return nullptr;
    }
}

const char *select_home_message()
{
    if (desktop_pet_state_requires_sleep(s_state)) {
        return "TAP ME TO TUCK ME IN.";
    }
    if (desktop_pet_state_is_low_energy(s_state)) {
        return "I'M LOW ON ENERGY. MAY I REST?";
    }
    if (s_state.pet.stage == PetLifeStage::Hatchling &&
        s_state.pet.growth >= kDesktopPetHatchlingGrowthLimit &&
        !s_state.pet.evolution_ready) {
        return "CARE FOR ME TO HELP ME GROW.";
    }
    if (s_state.pet.stage == PetLifeStage::Child &&
        s_state.pet.growth >= kDesktopPetChildGrowthLimit &&
        !s_state.pet.evolution_ready) {
        return "CARE FOR ME TO FIND MY PATH.";
    }
    if (s_state.pet.stage == PetLifeStage::Youth &&
        s_state.pet.growth >= kDesktopPetYouthGrowthLimit &&
        !s_state.pet.evolution_ready) {
        return "CARE FOR ME TO HELP ME GROW UP.";
    }
    if (s_state.pet.stage == PetLifeStage::Adult) {
        const char *message = select_adult_home_message(
            static_cast<uint32_t>(esp_timer_get_time()));
        if (message != nullptr) {
            return message;
        }
    }
    if (s_state.pet.stage == PetLifeStage::Youth) {
        const char *message = select_youth_home_message(
            static_cast<uint32_t>(esp_timer_get_time()));
        if (message != nullptr) {
            return message;
        }
    }
    const PetDialogueContext context =
        pet_dialogue_context_for_state(s_state.pet);
    const PetDialogueEntry *entry = pet_dialogue_pick(
        s_state.pet,
        context,
        static_cast<uint32_t>(esp_timer_get_time()));
    return entry == nullptr ? kHomeMessage : entry->text;
}

esp_err_t refresh_display(bool partial_refresh, bool timing_log = true)
{
#if !STICKY_LOG_DISPLAY_TIMING_ENABLED
    (void)timing_log;
#endif
#if STICKY_LOG_DISPLAY_TIMING_ENABLED
    const int64_t started_us = timing_log ? esp_timer_get_time() : 0;
#endif
    const esp_err_t result = partial_refresh
                                 ? sticky_display_refresh_partial()
                                 : sticky_display_refresh_monochrome();
#if STICKY_LOG_DISPLAY_TIMING_ENABLED
    if (timing_log) {
        STICKY_LOGD(kTag,
                    "pet=refresh page=%s mode=%s elapsed_ms=%lld result=%s",
                    s_name_editor_open ? "name_editor"
                    : s_state.pet.activity == PetActivity::Sleeping ? "sleep"
                    : s_hatch_active ? "hatch"
                    : s_test_open ? "test"
                    : s_state.pet.stage == PetLifeStage::Egg ? "egg"
                                                              : "home",
                    partial_refresh ? "partial" : "full",
                    static_cast<long long>(
                        (esp_timer_get_time() - started_us) / 1000LL),
                    esp_err_to_name(result));
    }
#endif
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "pet=refresh page=%s mode=%s result=%s",
                    s_name_editor_open ? "name_editor"
                    : s_state.pet.activity == PetActivity::Sleeping ? "sleep"
                    : s_hatch_active ? "hatch"
                    : s_test_open ? "test"
                    : s_state.pet.stage == PetLifeStage::Egg ? "egg"
                                                              : "home",
                    partial_refresh ? "partial" : "full",
                    esp_err_to_name(result));
    }
    return result;
}

void render_current_page(bool partial_refresh, bool timing_log = true)
{
    if (s_name_editor_open) {
        desktop_pet_page_render_name_editor(
            *s_canvas,
            s_name_text,
            s_name_keyboard_mode,
            s_name_input_error,
            !s_name_required);
    } else if (s_hatch_active) {
        desktop_pet_page_render_egg(*s_canvas, s_state, s_hatch_frame);
    } else if (s_evolution_active) {
        desktop_pet_page_render_evolution(
            *s_canvas, s_state, s_evolution_frame);
    } else if (s_personality_choice_open) {
        desktop_pet_page_render_personality_choice(*s_canvas, s_state);
    } else if (s_test_open) {
        desktop_pet_page_render_test(
            *s_canvas, s_state, s_reset_confirmation);
    } else if (s_state.pet.activity == PetActivity::Sleeping) {
        desktop_pet_page_render_sleep(
            *s_canvas, s_state, s_sleep_secondary_frame);
    } else if (s_state.pet.stage == PetLifeStage::Egg) {
        desktop_pet_page_render_egg(
            *s_canvas, s_state, DesktopPetHatchFrame::Resting);
    } else {
        desktop_pet_page_render_home(
            *s_canvas, s_state, s_pose, s_idle_frame, s_message);
    }
    refresh_display(partial_refresh, timing_log);
}

DesktopPetIdleFrame idle_frame_for_action(PetIdleAction action)
{
    switch (action) {
    case PetIdleAction::Blink:
        return DesktopPetIdleFrame::Blink;
    case PetIdleAction::EarTwitch:
        return DesktopPetIdleFrame::EarTwitch;
    case PetIdleAction::LookAround:
        return DesktopPetIdleFrame::LookAround;
    case PetIdleAction::Stretch:
        return DesktopPetIdleFrame::Stretch;
    case PetIdleAction::Hungry:
        return DesktopPetIdleFrame::Hungry;
    case PetIdleAction::Tired:
        return DesktopPetIdleFrame::Tired;
    case PetIdleAction::None:
    default:
        return DesktopPetIdleFrame::Normal;
    }
}

void enqueue_idle_frame(DesktopPetIdleFrame frame,
                        uint16_t duration_ms,
                        uint32_t now_ms)
{
    PetAnimationNode node = {};
    node.type = PetAnimationNodeType::Pose;
    node.asset_id = static_cast<uint16_t>(frame);
    node.duration_ms = duration_ms;
    s_idle_animation.enqueue(node, now_ms);
}

void schedule_next_idle(int64_t now_us)
{
    if (s_state.pet.stage == PetLifeStage::Egg ||
        desktop_pet_state_requires_sleep(s_state) ||
        desktop_pet_state_is_low_energy(s_state)) {
        s_idle_next_us = 0;
        return;
    }
    const uint32_t random_value =
        static_cast<uint32_t>(now_us / 1000LL) ^
        static_cast<uint32_t>(s_state.pet.day * 2654435761U);
    const uint32_t delay_ms = pet_idle_next_delay_ms(
        random_value,
#if STICKY_DESKTOP_PET_TEST_MODE
        true
#else
        false
#endif
    );
    s_idle_next_us = now_us + static_cast<int64_t>(delay_ms) * 1000LL;
}

void cancel_idle_animation(bool schedule_next)
{
    s_idle_animation.reset();
    s_idle_frame = DesktopPetIdleFrame::Normal;
    s_idle_next_us = 0;
    if (schedule_next) {
        schedule_next_idle(esp_timer_get_time());
    }
}

// Opens the portrait editor with either a blank first name or current name.
// 打开竖屏命名编辑器，并载入首次空名字或当前已有名字。
void open_name_editor(bool required, bool render_page = true)
{
    cancel_idle_animation(false);
    s_test_open = false;
    s_personality_choice_open = false;
    s_reset_confirmation = false;
    s_pose = DesktopPetPose::Idle;
    s_pose_deadline_us = 0;
    s_name_editor_open = true;
    s_name_required = required;
    s_name_input_error = false;
    s_name_keyboard_mode = DesktopPetKeyboardMode::Letters;
    std::snprintf(s_name_text, sizeof(s_name_text), "%s",
                  required ? "" : s_state.name);
    s_name_text_length = std::strlen(s_name_text);
    sticky_touch_clear_press();
    if (render_page) {
        render_current_page(false);
    }
    STICKY_LOGI(kTag,
                "pet=name_editor state=open required=%d length=%u result=ok",
                required ? 1 : 0,
                static_cast<unsigned>(s_name_text_length));
}

// Starts one saved tap sequence without blocking touch polling.
// 启动一次已保存的轻触动画，同时保持触摸轮询不被阻塞。
void start_hatch_animation(const DesktopPetHatchResult &result)
{
    cancel_idle_animation(false);
    s_test_open = false;
    s_personality_choice_open = false;
    s_reset_confirmation = false;
    s_pose = DesktopPetPose::Idle;
    s_pose_deadline_us = 0;
    s_hatch_active = true;
    s_hatch_final = result.hatched;
    s_hatch_frame = DesktopPetHatchFrame::WobbleLeft;
    sticky_touch_clear_press();
    render_current_page(true, false);
    s_hatch_deadline_us = esp_timer_get_time() + kHatchWobbleHoldUs;
}

// Advances the wobble, crack and welcome frames for one accepted tap.
// 推进一次有效轻触对应的摇摆、裂开和欢迎画面。
void update_hatch_animation(int64_t now_us)
{
    if (!s_hatch_active || now_us < s_hatch_deadline_us) {
        return;
    }

    if (s_hatch_frame == DesktopPetHatchFrame::WobbleLeft) {
        s_hatch_frame = DesktopPetHatchFrame::WobbleRight;
        render_current_page(true, false);
        s_hatch_deadline_us = esp_timer_get_time() + kHatchWobbleHoldUs;
        return;
    }
    if (s_hatch_frame == DesktopPetHatchFrame::WobbleRight) {
        s_hatch_frame = DesktopPetHatchFrame::Cracked;
        render_current_page(true, false);
        s_hatch_deadline_us = esp_timer_get_time() + kHatchCrackHoldUs;
        return;
    }
    if (s_hatch_frame == DesktopPetHatchFrame::Cracked && s_hatch_final) {
        s_hatch_frame = DesktopPetHatchFrame::Opened;
        render_current_page(true, false);
        const esp_err_t buzzer_result = sticky_buzzer_play_hatch_chime();
        if (buzzer_result != ESP_OK) {
            STICKY_LOGW(kTag,
                        "pet=hatch sound=failed result=%s",
                        esp_err_to_name(buzzer_result));
        }
        s_hatch_deadline_us = esp_timer_get_time() + kHatchWelcomeHoldUs;
        return;
    }

    s_hatch_active = false;
    s_hatch_final = false;
    s_hatch_frame = DesktopPetHatchFrame::Resting;
    s_hatch_deadline_us = 0;
    sticky_touch_clear_press();
    if (s_state.pet.stage == PetLifeStage::Hatchling) {
        if (!desktop_pet_state_has_name(s_state)) {
            open_name_editor(true);
            STICKY_LOGI(kTag,
                        "pet=hatch state=complete stage=hatchling next=name_editor result=ok");
            return;
        }
        s_home_message = select_home_message();
        s_message = s_home_message;
        render_current_page(false);
        schedule_next_idle(esp_timer_get_time());
#if STICKY_DESKTOP_PET_TEST_MODE
        s_day_deadline_us = esp_timer_get_time() +
                            static_cast<int64_t>(
                                kDesktopPetTestDayLengthMs) * 1000LL;
#endif
        STICKY_LOGI(kTag,
                    "pet=hatch state=complete stage=hatchling result=ok");
        return;
    }
    render_current_page(true, false);
}

// Starts a non-blocking three-frame stage transition.
// 启动一个不阻塞触摸任务的三帧成长过场。
void start_evolution()
{
    cancel_idle_animation(false);
    s_test_open = false;
    s_personality_choice_open = false;
    s_reset_confirmation = false;
    s_pose = DesktopPetPose::Idle;
    s_idle_frame = DesktopPetIdleFrame::Normal;
    s_pose_deadline_us = 0;
    s_evolution_active = true;
    s_evolution_frame = DesktopPetEvolutionFrame::Starting;
    sticky_touch_clear_press();
    render_current_page(false);
    s_evolution_deadline_us =
        esp_timer_get_time() + kEvolutionStartingHoldUs;
}

void open_personality_choice()
{
    cancel_idle_animation(false);
    s_test_open = false;
    s_personality_choice_open = true;
    s_reset_confirmation = false;
    s_pose = DesktopPetPose::Idle;
    s_pose_deadline_us = 0;
    sticky_touch_clear_press();
    render_current_page(false);
}

void apply_evolution_outcome(DesktopPetEvolutionOutcome outcome)
{
    if (outcome == DesktopPetEvolutionOutcome::Evolved) {
        if (s_state.pet.stage == PetLifeStage::Youth) {
            STICKY_LOGI(kTag,
                        "pet=personality branch=%s source=automatic result=ok",
                        pet_core_personality_name(s_state.pet.branch));
        } else if (s_state.pet.stage == PetLifeStage::Adult) {
            STICKY_LOGI(kTag,
                        "pet=evolution stage=adult branch=%s result=ok",
                        pet_core_personality_name(s_state.pet.branch));
        }
        start_evolution();
    } else if (outcome == DesktopPetEvolutionOutcome::ChoiceRequired) {
        open_personality_choice();
    }
}

// Advances the evolution scene and returns to the new live stage home page.
// 推进成长过场，并在结束后回到新阶段的可交互主页。
void update_evolution(int64_t now_us)
{
    if (!s_evolution_active || now_us < s_evolution_deadline_us) {
        return;
    }

    if (s_evolution_frame == DesktopPetEvolutionFrame::Starting) {
        s_evolution_frame = DesktopPetEvolutionFrame::Silhouette;
        render_current_page(true, false);
        s_evolution_deadline_us =
            esp_timer_get_time() + kEvolutionSilhouetteHoldUs;
        return;
    }
    if (s_evolution_frame == DesktopPetEvolutionFrame::Silhouette) {
        s_evolution_frame = DesktopPetEvolutionFrame::Revealed;
        render_current_page(true, false);
        s_evolution_deadline_us =
            esp_timer_get_time() + kEvolutionRevealHoldUs;
        return;
    }

    s_evolution_active = false;
    s_evolution_deadline_us = 0;
    s_state.pet.activity = PetActivity::Idle;
    s_home_message = select_home_message();
    s_message = s_home_message;
    sticky_touch_clear_press();
    render_current_page(false);
    schedule_next_idle(esp_timer_get_time());
}

// Builds one non-blocking frame sequence for the selected idle action.
// 为选中的待机动作建立一段非阻塞帧序列。
void start_idle_animation(int64_t now_us)
{
    const uint32_t now_ms = static_cast<uint32_t>(now_us / 1000LL);
    const PetIdleAction action = pet_idle_select(
        s_state.pet,
        s_previous_idle_action,
        s_second_previous_idle_action,
        now_ms ^ static_cast<uint32_t>(s_state.pet.bond * 97U));
    s_second_previous_idle_action = s_previous_idle_action;
    s_previous_idle_action = action;
    s_idle_animation.reset();

    const DesktopPetIdleFrame action_frame = idle_frame_for_action(action);
    switch (action) {
    case PetIdleAction::EarTwitch:
        enqueue_idle_frame(action_frame, 900U, now_ms);
        enqueue_idle_frame(DesktopPetIdleFrame::Normal, 350U, now_ms);
        enqueue_idle_frame(action_frame, 900U, now_ms);
        enqueue_idle_frame(DesktopPetIdleFrame::Normal, 650U, now_ms);
        break;
    case PetIdleAction::Blink:
        enqueue_idle_frame(action_frame, 900U, now_ms);
        enqueue_idle_frame(DesktopPetIdleFrame::Normal, 650U, now_ms);
        break;
    case PetIdleAction::LookAround:
        enqueue_idle_frame(action_frame, 1600U, now_ms);
        enqueue_idle_frame(DesktopPetIdleFrame::Normal, 650U, now_ms);
        break;
    case PetIdleAction::Stretch:
        enqueue_idle_frame(action_frame, 1800U, now_ms);
        enqueue_idle_frame(DesktopPetIdleFrame::Normal, 650U, now_ms);
        break;
    case PetIdleAction::Hungry:
        enqueue_idle_frame(action_frame, 2000U, now_ms);
        enqueue_idle_frame(DesktopPetIdleFrame::Normal, 650U, now_ms);
        break;
    case PetIdleAction::Tired:
        enqueue_idle_frame(action_frame, 2300U, now_ms);
        enqueue_idle_frame(DesktopPetIdleFrame::Normal, 650U, now_ms);
        break;
    case PetIdleAction::None:
    default:
        schedule_next_idle(now_us);
        return;
    }

    s_idle_next_us = 0;
    s_idle_frame = action_frame;
    s_message = pet_idle_message(action, s_state.pet.stage);
#if STICKY_LOG_PET_ANIMATION_ENABLED
    STICKY_LOGD(kTag,
                "pet=idle action=%s queue_size=%u result=start",
                pet_idle_action_name(action),
                static_cast<unsigned>(s_idle_animation.size()));
#endif
    render_current_page(true, false);
    play_visible_pet_sound(
        desktop_pet_sound_for_idle(s_state, action_frame),
        "idle_frame");
}

// Advances autonomous frames without blocking touch processing.
// 推进自主动作帧，同时保持触摸处理不被阻塞。
void update_idle_animation(int64_t now_us)
{
    if (s_hatch_active || s_state.pet.stage == PetLifeStage::Egg ||
        s_evolution_active || s_test_open || s_personality_choice_open ||
        s_name_editor_open ||
        s_state.pet.activity == PetActivity::Sleeping ||
        desktop_pet_state_requires_sleep(s_state) ||
        desktop_pet_state_is_low_energy(s_state) ||
        s_pose != DesktopPetPose::Idle ||
        s_pose_deadline_us > 0) {
        return;
    }

    const uint32_t now_ms = static_cast<uint32_t>(now_us / 1000LL);
    if (s_idle_animation.playing()) {
        s_idle_animation.update(now_ms);
        const PetAnimationNode *node = s_idle_animation.current();
        const DesktopPetIdleFrame next_frame = node == nullptr
                                                   ? DesktopPetIdleFrame::Normal
                                                   : static_cast<DesktopPetIdleFrame>(
                                                         node->asset_id);
        if (next_frame != s_idle_frame) {
            s_idle_frame = next_frame;
            if (s_idle_frame == DesktopPetIdleFrame::Normal) {
                s_message = s_home_message;
            }
#if STICKY_LOG_PET_ANIMATION_ENABLED
            STICKY_LOGD(kTag,
                        "pet=idle frame=%u queue_size=%u",
                        static_cast<unsigned>(s_idle_frame),
                        static_cast<unsigned>(s_idle_animation.size()));
#endif
            render_current_page(true, false);
        }
        if (!s_idle_animation.playing()) {
            schedule_next_idle(now_us);
        }
        return;
    }

    if (s_idle_next_us == 0) {
        schedule_next_idle(now_us);
    } else if (now_us >= s_idle_next_us) {
        start_idle_animation(now_us);
    }
}

void save_state(const char *reason)
{
    const esp_err_t result = desktop_pet_storage_save(s_state);
    if (result == ESP_OK) {
#if STICKY_LOG_DESKTOP_PET_ENABLED
        STICKY_LOGD(kTag,
                    "pet=save reason=%s stage=%s day=%u growth=%u love=%u food=%u mood=%s result=ok",
                    reason,
                    pet_core_stage_name(s_state.pet.stage),
                    static_cast<unsigned>(s_state.pet.day),
                    static_cast<unsigned>(s_state.pet.growth),
                    static_cast<unsigned>(s_state.pet.bond),
                    static_cast<unsigned>(s_state.pet.needs.food),
                    desktop_pet_state_mood_label(s_state));
#endif
        return;
    }
    STICKY_LOGE(kTag,
                "pet=save reason=%s result=%s",
                reason,
                esp_err_to_name(result));
}

void start_sleep_page()
{
    cancel_idle_animation(false);
    s_test_open = false;
    s_personality_choice_open = false;
    s_reset_confirmation = false;
    s_pose = DesktopPetPose::Idle;
    s_pose_deadline_us = 0;
    s_sleep_secondary_frame = false;
    s_sleep_deadline_us = esp_timer_get_time() + kSleepFrameHoldUs;
    sticky_touch_clear_press();
    render_current_page(false);
    play_visible_pet_sound(
        desktop_pet_sound_for_performance(
            s_state,
            DesktopPetPerformance::FallingAsleep,
            s_message),
        "sleep_scene");
    STICKY_LOGI(kTag,
                "pet=sleep state=started energy=%u result=ok",
                static_cast<unsigned>(s_state.pet.needs.energy));
}

void finish_sleep(const char *message, const char *reason)
{
    s_sleep_secondary_frame = false;
    s_sleep_deadline_us = 0;
    s_pose = DesktopPetPose::Idle;
    s_idle_frame = DesktopPetIdleFrame::Normal;
    s_home_message = select_home_message();
    s_message = message;
    sticky_touch_clear_press();
    render_current_page(false);
    play_visible_pet_sound(
        desktop_pet_sound_for_performance(
            s_state,
            DesktopPetPerformance::Waking,
            s_message),
        "wake_scene");
    s_pose_deadline_us = esp_timer_get_time() + kTalkMessageHoldUs;
    schedule_next_idle(esp_timer_get_time());
    STICKY_LOGI(kTag,
                "pet=sleep state=finished reason=%s energy=%u result=ok",
                reason,
                static_cast<unsigned>(s_state.pet.needs.energy));
}

// Advances the e-paper breathing frame and accelerated test recovery together.
// 同时推进电子纸呼吸帧与测试版加速精力恢复。
void update_sleep(int64_t now_us)
{
    if (s_state.pet.activity != PetActivity::Sleeping ||
        s_sleep_deadline_us == 0 || now_us < s_sleep_deadline_us) {
        return;
    }

    pet_core_advance_minutes(s_state.pet,
                             kSleepMinutesPerFrame,
                             sleep_profile(),
                             false);
    if (s_state.pet.needs.energy >= 100U) {
        const DesktopPetActionResult wake_result =
            desktop_pet_state_apply(s_state, DesktopPetAction::Wake);
        save_state("sleep_complete");
        finish_sleep(wake_result.message, "rested");
        return;
    }

    s_sleep_secondary_frame = !s_sleep_secondary_frame;
    render_current_page(true, false);
    s_sleep_deadline_us = esp_timer_get_time() + kSleepFrameHoldUs;
#if STICKY_LOG_PET_ANIMATION_ENABLED
    STICKY_LOGD(kTag,
                "pet=sleep frame=%u energy=%u",
                s_sleep_secondary_frame ? 1U : 0U,
                static_cast<unsigned>(s_state.pet.needs.energy));
#endif
}

void handle_test_action(DesktopPetAction action)
{
    if (action == DesktopPetAction::CloseTest) {
        s_test_open = false;
        s_reset_confirmation = false;
        s_home_message = select_home_message();
        s_message = s_home_message;
        s_pose = DesktopPetPose::Idle;
        s_idle_frame = DesktopPetIdleFrame::Normal;
        s_pose_deadline_us = 0;
        if (s_state.pet.stage != PetLifeStage::Egg) {
            schedule_next_idle(esp_timer_get_time());
        }
        sticky_touch_clear_press();
        render_current_page(true);
        return;
    }

    if (action == DesktopPetAction::Reset && !s_reset_confirmation) {
        s_reset_confirmation = true;
        render_current_page(true);
        return;
    }

    if (s_state.pet.stage == PetLifeStage::Egg &&
        action != DesktopPetAction::Reset) {
        return;
    }

    if (action == DesktopPetAction::Reset) {
        const esp_err_t erase_result = desktop_pet_storage_reset();
        if (erase_result != ESP_OK) {
            STICKY_LOGE(kTag,
                        "pet=test action=reset erase_result=%s",
                        esp_err_to_name(erase_result));
            return;
        }
    }

#if STICKY_DESKTOP_PET_TEST_MODE
    if (action == DesktopPetAction::Sleep &&
        s_state.pet.needs.energy > 20U) {
        s_state.pet.needs.energy = 20U;
    }
#endif

    const DesktopPetActionResult result =
        desktop_pet_state_apply(s_state, action);
    if (!result.changed) {
        return;
    }
    s_reset_confirmation = false;
    s_message = result.message;
    s_home_message = select_home_message();
    const DesktopPetEvolutionOutcome evolution =
        desktop_pet_state_evolve_if_ready(s_state);
    save_state(desktop_pet_action_name(action));
    STICKY_LOGI(kTag,
                "pet=test action=%s stage=%s day=%u growth=%u love=%u food=%u mood=%s result=ok",
                desktop_pet_action_name(action),
                pet_core_stage_name(s_state.pet.stage),
                static_cast<unsigned>(s_state.pet.day),
                static_cast<unsigned>(s_state.pet.growth),
                static_cast<unsigned>(s_state.pet.bond),
                static_cast<unsigned>(s_state.pet.needs.food),
                desktop_pet_state_mood_label(s_state));
    if (evolution != DesktopPetEvolutionOutcome::None) {
        apply_evolution_outcome(evolution);
        return;
    }
    if (action == DesktopPetAction::Sleep) {
        start_sleep_page();
        return;
    }
    render_current_page(true);
}

struct NameEditorActionResult {
    bool changed = false;
    bool exited = false;
};

bool append_name_character(char character)
{
    if (s_name_text_length >= kDesktopPetNameMaximumLength) {
        return false;
    }
    s_name_text[s_name_text_length++] = character;
    s_name_text[s_name_text_length] = '\0';
    return true;
}

// Applies one editor command without refreshing, allowing queued keys to batch.
// 在不立即刷新的情况下应用一次编辑命令，让连续按键能够合并刷新。
NameEditorActionResult handle_name_editor_action(
    DesktopPetNameAction action)
{
    char character = '\0';
    if (desktop_pet_name_action_character(action, character)) {
        s_name_input_error = false;
        const bool appended = append_name_character(character);
        if (!appended) {
            STICKY_LOGW(kTag,
                        "pet=name_editor action=append result=full length=%u",
                        static_cast<unsigned>(s_name_text_length));
        }
        return {appended, false};
    }

    switch (action) {
    case DesktopPetNameAction::Space:
        s_name_input_error = false;
        if (s_name_text_length > 0U &&
            s_name_text[s_name_text_length - 1U] != ' ') {
            return {append_name_character(' '), false};
        }
        return {};
    case DesktopPetNameAction::Delete:
        s_name_input_error = false;
        if (s_name_text_length > 0U) {
            s_name_text[--s_name_text_length] = '\0';
            return {true, false};
        }
        return {};
    case DesktopPetNameAction::Clear:
        s_name_input_error = false;
        if (s_name_text_length > 0U) {
            s_name_text_length = 0U;
            s_name_text[0] = '\0';
            return {true, false};
        }
        return {};
    case DesktopPetNameAction::ToggleKeyboard:
        s_name_input_error = false;
        s_name_keyboard_mode =
            s_name_keyboard_mode == DesktopPetKeyboardMode::Letters
                ? DesktopPetKeyboardMode::Numbers
                : DesktopPetKeyboardMode::Letters;
        return {true, false};
    case DesktopPetNameAction::Apply:
        if (!desktop_pet_state_set_name(s_state, s_name_text)) {
            s_name_input_error = true;
            STICKY_LOGW(kTag,
                        "pet=name_editor action=apply result=empty");
            return {true, false};
        }
        save_state("name");
        s_name_editor_open = false;
        s_name_required = false;
        s_name_input_error = false;
        s_home_message = select_home_message();
        s_message = s_home_message;
        STICKY_LOGI(kTag,
                    "pet=name_editor action=apply length=%u result=ok",
                    static_cast<unsigned>(std::strlen(s_state.name)));
        return {true, true};
    case DesktopPetNameAction::Back:
        if (!s_name_required) {
            s_name_editor_open = false;
            s_name_input_error = false;
            s_home_message = select_home_message();
            s_message = s_home_message;
            STICKY_LOGI(kTag,
                        "pet=name_editor action=back result=cancelled");
            return {true, true};
        }
        return {};
    case DesktopPetNameAction::None:
    default:
        return {};
    }
}

void handle_action(DesktopPetAction action)
{
    if (action == DesktopPetAction::None) {
        return;
    }
    if (s_personality_choice_open) {
        PetPersonalityBranch branch = PetPersonalityBranch::Undecided;
        if (action == DesktopPetAction::ChooseFoodie) {
            branch = PetPersonalityBranch::Foodie;
        } else if (action == DesktopPetAction::ChooseAffectionate) {
            branch = PetPersonalityBranch::Affectionate;
        } else if (action == DesktopPetAction::ChooseActive) {
            branch = PetPersonalityBranch::Active;
        }
        if (!desktop_pet_state_choose_youth_branch(s_state, branch)) {
            return;
        }
        save_state(desktop_pet_action_name(action));
        STICKY_LOGI(kTag,
                    "pet=personality branch=%s source=choice result=ok",
                    pet_core_personality_name(s_state.pet.branch));
        start_evolution();
        return;
    }
    if (s_test_open) {
        handle_test_action(action);
        return;
    }
    cancel_idle_animation(false);
    if (action == DesktopPetAction::Pet &&
        desktop_pet_state_requires_sleep(s_state)) {
        action = DesktopPetAction::Sleep;
    }
    if (desktop_pet_state_requires_sleep(s_state) &&
        (action == DesktopPetAction::Feed ||
         action == DesktopPetAction::Talk ||
         action == DesktopPetAction::Play)) {
        s_pose = DesktopPetPose::Idle;
        s_idle_frame = DesktopPetIdleFrame::Tired;
        s_home_message = "TOO TIRED. TUCK ME IN FIRST.";
        s_message = s_home_message;
        render_current_page(true);
        return;
    }
    if (action == DesktopPetAction::OpenNameEditor) {
        open_name_editor(false);
        return;
    }
    if (action == DesktopPetAction::OpenTest) {
        s_test_open = true;
        s_reset_confirmation = false;
        sticky_touch_clear_press();
        render_current_page(true);
        return;
    }

    if (action == DesktopPetAction::TapEgg) {
        const DesktopPetHatchResult hatch_result =
            desktop_pet_state_tap_egg(s_state);
        if (!hatch_result.changed) {
            return;
        }
        save_state("tap_egg");
        STICKY_LOGI(kTag,
                    "pet=hatch tap=%u required=%u hatched=%d result=ok",
                    static_cast<unsigned>(hatch_result.tap_count),
                    static_cast<unsigned>(kDesktopPetRequiredHatchTaps),
                    hatch_result.hatched ? 1 : 0);
        start_hatch_animation(hatch_result);
        return;
    }

    const DesktopPetActionResult result =
        desktop_pet_state_apply(s_state, action);
    if (!result.changed) {
        return;
    }
    const bool requires_sleep = desktop_pet_state_requires_sleep(s_state);
    s_pose = requires_sleep ? DesktopPetPose::Idle : result.pose;
    s_idle_frame = requires_sleep ? DesktopPetIdleFrame::Tired
                                  : DesktopPetIdleFrame::Normal;
    s_home_message = select_home_message();
    s_message = requires_sleep ? s_home_message : result.message;
    const DesktopPetEvolutionOutcome evolution =
        desktop_pet_state_evolve_if_ready(s_state);
    save_state(desktop_pet_action_name(action));
    STICKY_LOGI(kTag,
                "pet=care action=%s stage=%s pose=%s rewarded=%d growth_delta=%u love_delta=%u growth=%u love=%u food=%u mood=%s result=ok",
                desktop_pet_action_name(action),
                pet_core_stage_name(s_state.pet.stage),
                desktop_pet_pose_name(result.pose),
                result.rewarded ? 1 : 0,
                static_cast<unsigned>(result.growth_delta),
                static_cast<unsigned>(result.love_delta),
                static_cast<unsigned>(s_state.pet.growth),
                static_cast<unsigned>(s_state.pet.bond),
                static_cast<unsigned>(s_state.pet.needs.food),
                desktop_pet_state_mood_label(s_state));
    if (evolution != DesktopPetEvolutionOutcome::None) {
        apply_evolution_outcome(evolution);
        return;
    }
    if (action == DesktopPetAction::Sleep) {
        start_sleep_page();
        return;
    }
    if (action == DesktopPetAction::Wake) {
        finish_sleep(result.message, "touch");
        return;
    }
    render_current_page(true);
    play_visible_pet_sound(
        desktop_pet_sound_for_performance(
            s_state, result.performance, s_message),
        "care_pose");
    if (requires_sleep) {
        s_pose_deadline_us = 0;
        return;
    }
    // Hold time begins after the e-paper refresh finishes so the complete
    // pose remains visible for the requested duration.
    // 电子纸刷新完成后再开始计时，确保完整动作真正显示足够时长。
    const int64_t hold_us = action == DesktopPetAction::Talk
                                ? kTalkMessageHoldUs
                                : kActionPoseHoldUs;
    s_pose_deadline_us = esp_timer_get_time() + hold_us;
}

DesktopPetAction action_for_press(const StickyTouchPress &press)
{
    if (s_hatch_active || s_evolution_active) {
        return DesktopPetAction::None;
    }
    int logical_x = 0;
    int logical_y = 0;
    s_canvas->physical_to_logical(
        press.x, press.y, logical_x, logical_y);
    DesktopPetAction action = DesktopPetAction::None;
    if (s_state.pet.activity == PetActivity::Sleeping) {
        action = desktop_pet_page_sleep_action_at(logical_x, logical_y);
    } else if (s_personality_choice_open) {
        action = desktop_pet_page_personality_action_at(logical_x,
                                                        logical_y);
    } else if (!s_test_open && s_state.pet.stage == PetLifeStage::Egg) {
        action = desktop_pet_page_egg_action_at(logical_x, logical_y);
    } else {
        action = desktop_pet_page_action_at(s_test_open,
                                            logical_x,
                                            logical_y);
    }
#if STICKY_LOG_DESKTOP_PET_ENABLED
    const uint32_t now_ms =
        static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
    STICKY_LOGD(kTag,
                "pet=touch page=%s action=%s physical_x=%u physical_y=%u logical_x=%d logical_y=%d queue_latency_ms=%u",
                s_state.pet.activity == PetActivity::Sleeping ? "sleep"
                : s_test_open ? "test"
                : s_state.pet.stage == PetLifeStage::Egg ? "egg" : "home",
                desktop_pet_action_name(action),
                static_cast<unsigned>(press.x),
                static_cast<unsigned>(press.y),
                logical_x,
                logical_y,
                static_cast<unsigned>(now_ms - press.captured_at_ms));
#endif
    return action;
}

DesktopPetNameAction name_action_for_press(const StickyTouchPress &press)
{
    int logical_x = 0;
    int logical_y = 0;
    s_canvas->physical_to_logical(
        press.x, press.y, logical_x, logical_y);
    const DesktopPetNameAction action = desktop_pet_page_name_action_at(
        s_name_keyboard_mode,
        !s_name_required,
        logical_x,
        logical_y);
#if STICKY_LOG_DESKTOP_PET_ENABLED
    if (action != DesktopPetNameAction::None) {
        const uint32_t now_ms =
            static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
        STICKY_LOGD(kTag,
                    "pet=touch page=name_editor action=%s physical_x=%u physical_y=%u logical_x=%d logical_y=%d queue_latency_ms=%u",
                    desktop_pet_name_action_name(action),
                    static_cast<unsigned>(press.x),
                    static_cast<unsigned>(press.y),
                    logical_x,
                    logical_y,
                    static_cast<unsigned>(now_ms - press.captured_at_ms));
    }
#endif
    return action;
}

void app_task(void *)
{
    bool found = false;
    const esp_err_t load_result = desktop_pet_storage_load(s_state, found);
    if (load_result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "pet=load result=%s fallback=default",
                    esp_err_to_name(load_result));
        s_state = {};
    }

    sticky_touch_clear_press();
    s_idle_animation.reset();
    s_idle_frame = DesktopPetIdleFrame::Normal;
    s_home_message = select_home_message();
    s_message = s_home_message;
    const DesktopPetEvolutionOutcome evolution_on_load =
        desktop_pet_state_evolve_if_ready(s_state);
    if (evolution_on_load == DesktopPetEvolutionOutcome::Evolved) {
        save_state("evolution_on_load");
        start_evolution();
    } else if (evolution_on_load ==
               DesktopPetEvolutionOutcome::ChoiceRequired) {
        open_personality_choice();
    } else {
        if (s_state.pet.activity == PetActivity::Sleeping) {
            s_sleep_secondary_frame = false;
            s_sleep_deadline_us =
                esp_timer_get_time() + kSleepFrameHoldUs;
        }
        render_current_page(false);
        if (s_state.pet.stage != PetLifeStage::Egg &&
            s_state.pet.activity != PetActivity::Sleeping) {
            schedule_next_idle(esp_timer_get_time());
        }
    }
#if STICKY_DESKTOP_PET_TEST_MODE
    s_day_deadline_us = esp_timer_get_time() +
                        static_cast<int64_t>(kDesktopPetTestDayLengthMs) *
                            1000LL;
#endif
    STICKY_LOGI(kTag,
                "pet=ready page=%s profile=%s save=%s stage=%s day=%u growth=%u love=%u food=%u mood=%s result=ok",
                s_name_editor_open
                    ? "name_editor"
                    : s_state.pet.activity == PetActivity::Sleeping
                           ? "sleep"
                    : (s_state.pet.stage == PetLifeStage::Egg
                           ? "egg"
                           : "home"),
#if STICKY_DESKTOP_PET_TEST_MODE
                "test",
#else
                "production",
#endif
                found ? "loaded" : "new",
                pet_core_stage_name(s_state.pet.stage),
                static_cast<unsigned>(s_state.pet.day),
                static_cast<unsigned>(s_state.pet.growth),
                static_cast<unsigned>(s_state.pet.bond),
                static_cast<unsigned>(s_state.pet.needs.food),
                desktop_pet_state_mood_label(s_state));

    while (true) {
        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            if (s_name_editor_open) {
                bool redraw_needed = false;
                bool exited = false;
                unsigned batched_actions = 0U;
                while (true) {
                    const DesktopPetNameAction action =
                        name_action_for_press(press);
                    if (action != DesktopPetNameAction::None) {
                        const NameEditorActionResult result =
                            handle_name_editor_action(action);
                        redraw_needed = result.changed || redraw_needed;
                        exited = result.exited || exited;
                        ++batched_actions;
                        if (result.exited ||
                            !desktop_pet_name_action_can_batch(action)) {
                            // The next screen must not inherit taps captured
                            // for the keyboard that has just disappeared.
                            // 下一个页面不接收刚刚消失的键盘所积累的触摸。
                            sticky_touch_clear_press();
                            break;
                        }
                    } else {
                        break;
                    }

                    if (!sticky_touch_take_press(press)) {
                        break;
                    }
                }

                if (redraw_needed) {
                    if (batched_actions > 1U) {
                        STICKY_LOGI(kTag,
                                    "pet=name_input_batch actions=%u refreshes=1",
                                    batched_actions);
                    }
                    render_current_page(!exited);
                    if (exited) {
                        schedule_next_idle(esp_timer_get_time());
#if STICKY_DESKTOP_PET_TEST_MODE
                        s_day_deadline_us = esp_timer_get_time() +
                            static_cast<int64_t>(
                                kDesktopPetTestDayLengthMs) * 1000LL;
#endif
                    }
                }
            } else {
                handle_action(action_for_press(press));
            }
        }

        const int64_t now_us = esp_timer_get_time();
        if (s_hatch_active) {
            update_hatch_animation(now_us);
            vTaskDelay(kPollInterval);
            continue;
        }
        if (s_evolution_active) {
            update_evolution(now_us);
            vTaskDelay(kPollInterval);
            continue;
        }
        if (s_state.pet.activity == PetActivity::Sleeping) {
            update_sleep(now_us);
            vTaskDelay(kPollInterval);
            continue;
        }
        if (!s_test_open && !s_personality_choice_open &&
            !s_name_editor_open &&
            s_pose_deadline_us > 0 &&
            now_us >= s_pose_deadline_us) {
            s_pose = DesktopPetPose::Idle;
            s_idle_frame = DesktopPetIdleFrame::Normal;
            s_message = s_home_message;
            s_pose_deadline_us = 0;
            render_current_page(true, false);
            schedule_next_idle(esp_timer_get_time());
        }

        update_idle_animation(now_us);

#if STICKY_DESKTOP_PET_TEST_MODE
        if (s_state.pet.stage != PetLifeStage::Egg &&
            !s_personality_choice_open && !s_name_editor_open &&
            s_day_deadline_us > 0 &&
            now_us >= s_day_deadline_us) {
            cancel_idle_animation(false);
            desktop_pet_state_advance_day(s_state);
            s_day_deadline_us +=
                static_cast<int64_t>(kDesktopPetTestDayLengthMs) * 1000LL;
            s_message = "A NEW DAY TOGETHER!";
            s_home_message = select_home_message();
            save_state("automatic_next_day");
            STICKY_LOGI(kTag,
                        "pet=day source=timer day=%u food=%u mood=%s result=ok",
                        static_cast<unsigned>(s_state.pet.day),
                        static_cast<unsigned>(s_state.pet.needs.food),
                        desktop_pet_state_mood_label(s_state));
            render_current_page(true);
            s_pose_deadline_us = esp_timer_get_time() + kActionPoseHoldUs;
        }
#endif
        vTaskDelay(kPollInterval);
    }
}

}  // namespace

esp_err_t desktop_pet_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_app_task != nullptr) {
        return ESP_OK;
    }
    s_canvas = &canvas;
    if (xTaskCreate(app_task,
                    "desktop_pet_app",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_app_task) != pdPASS) {
        s_canvas = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
