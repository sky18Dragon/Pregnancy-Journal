#include "desktop_pet_app.h"

#include <cstdint>

#include "app_log.h"
#include "canvas.h"
#include "desktop_pet_pages.h"
#include "desktop_pet_state.h"
#include "desktop_pet_storage.h"
#include "pet_animation_queue.h"
#include "pet_dialogue.h"
#include "pet_idle_scheduler.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
#if STICKY_DESKTOP_PET_TEST_MODE
int64_t s_day_deadline_us = 0;
#endif

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

const char *select_home_message()
{
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
                    s_test_open ? "test" : "home",
                    partial_refresh ? "partial" : "full",
                    static_cast<long long>(
                        (esp_timer_get_time() - started_us) / 1000LL),
                    esp_err_to_name(result));
    }
#endif
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "pet=refresh page=%s mode=%s result=%s",
                    s_test_open ? "test" : "home",
                    partial_refresh ? "partial" : "full",
                    esp_err_to_name(result));
    }
    return result;
}

void render_current_page(bool partial_refresh, bool timing_log = true)
{
    if (s_evolution_active) {
        desktop_pet_page_render_evolution(
            *s_canvas, s_state, s_evolution_frame);
    } else if (s_personality_choice_open) {
        desktop_pet_page_render_personality_choice(*s_canvas, s_state);
    } else if (s_test_open) {
        desktop_pet_page_render_test(
            *s_canvas, s_state, s_reset_confirmation);
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
}

// Advances autonomous frames without blocking touch processing.
// 推进自主动作帧，同时保持触摸处理不被阻塞。
void update_idle_animation(int64_t now_us)
{
    if (s_evolution_active || s_test_open || s_personality_choice_open ||
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
        schedule_next_idle(esp_timer_get_time());
        sticky_touch_clear_press();
        render_current_page(true);
        return;
    }

    if (action == DesktopPetAction::Reset && !s_reset_confirmation) {
        s_reset_confirmation = true;
        render_current_page(true);
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
    render_current_page(true);
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
    if (action == DesktopPetAction::OpenTest) {
        s_test_open = true;
        s_reset_confirmation = false;
        sticky_touch_clear_press();
        render_current_page(true);
        return;
    }

    const DesktopPetActionResult result =
        desktop_pet_state_apply(s_state, action);
    if (!result.changed) {
        return;
    }
    s_pose = result.pose;
    s_idle_frame = DesktopPetIdleFrame::Normal;
    s_message = result.message;
    s_home_message = select_home_message();
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
    render_current_page(true);
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
    if (s_evolution_active) {
        return DesktopPetAction::None;
    }
    int logical_x = 0;
    int logical_y = 0;
    s_canvas->physical_to_logical(
        press.x, press.y, logical_x, logical_y);
    const DesktopPetAction action = s_personality_choice_open
                                        ? desktop_pet_page_personality_action_at(
                                              logical_x, logical_y)
                                        : desktop_pet_page_action_at(
                                              s_test_open,
                                              logical_x,
                                              logical_y);
#if STICKY_LOG_DESKTOP_PET_ENABLED
    const uint32_t now_ms =
        static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
    STICKY_LOGD(kTag,
                "pet=touch page=%s action=%s physical_x=%u physical_y=%u logical_x=%d logical_y=%d queue_latency_ms=%u",
                s_test_open ? "test" : "home",
                desktop_pet_action_name(action),
                static_cast<unsigned>(press.x),
                static_cast<unsigned>(press.y),
                logical_x,
                logical_y,
                static_cast<unsigned>(now_ms - press.captured_at_ms));
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
    const DesktopPetEvolutionOutcome evolution_on_load =
        desktop_pet_state_evolve_if_ready(s_state);
    s_home_message = select_home_message();
    s_message = s_home_message;
    if (evolution_on_load == DesktopPetEvolutionOutcome::Evolved) {
        save_state("evolution_on_load");
        start_evolution();
    } else if (evolution_on_load ==
               DesktopPetEvolutionOutcome::ChoiceRequired) {
        open_personality_choice();
    } else {
        render_current_page(false);
        schedule_next_idle(esp_timer_get_time());
    }
#if STICKY_DESKTOP_PET_TEST_MODE
    s_day_deadline_us = esp_timer_get_time() +
                        static_cast<int64_t>(kDesktopPetTestDayLengthMs) *
                            1000LL;
#endif
    STICKY_LOGI(kTag,
                "pet=ready page=home profile=%s save=%s stage=%s day=%u growth=%u love=%u food=%u mood=%s result=ok",
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
            handle_action(action_for_press(press));
        }

        const int64_t now_us = esp_timer_get_time();
        if (s_evolution_active) {
            update_evolution(now_us);
            vTaskDelay(kPollInterval);
            continue;
        }
        if (!s_test_open && !s_personality_choice_open &&
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
        if (!s_personality_choice_open && s_day_deadline_us > 0 &&
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
