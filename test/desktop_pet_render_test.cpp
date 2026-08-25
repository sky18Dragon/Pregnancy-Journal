#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "desktop_pet_pages.h"
#include "pet_rtc_time.h"

namespace {

constexpr uint16_t kPhysicalWidth = 800;
constexpr uint16_t kPhysicalHeight = 480;
constexpr size_t kStride = kPhysicalWidth / 4U;

uint8_t pixel_level(const std::vector<uint8_t> &buffer, int x, int y)
{
    const size_t index = static_cast<size_t>(y) * kStride +
                         static_cast<size_t>(x) / 4U;
    const uint8_t shift = static_cast<uint8_t>((3 - (x & 0x03)) * 2);
    return static_cast<uint8_t>((buffer[index] >> shift) & 0x03U);
}

uint8_t logical_pixel_level(const std::vector<uint8_t> &buffer, int x, int y)
{
    return pixel_level(buffer, y, kPhysicalHeight - 1 - x);
}

size_t logical_black_pixel_count(const std::vector<uint8_t> &buffer,
                                 int left,
                                 int top,
                                 int right,
                                 int bottom)
{
    size_t count = 0U;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            if (logical_pixel_level(buffer, x, y) ==
                static_cast<uint8_t>(GrayLevel::Black)) {
                ++count;
            }
        }
    }
    return count;
}

void assert_white_ink_is_centered(const std::vector<uint8_t> &buffer,
                                  int left,
                                  int top,
                                  int right,
                                  int bottom)
{
    int first_white_x = right + 1;
    int last_white_x = left - 1;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            if (logical_pixel_level(buffer, x, y) !=
                static_cast<uint8_t>(GrayLevel::White)) {
                continue;
            }
            first_white_x = std::min(first_white_x, x);
            last_white_x = std::max(last_white_x, x);
        }
    }

    assert(last_white_x >= first_white_x);
    assert(std::abs(first_white_x + last_white_x - 479) <= 1);
}

size_t black_pixel_count(const std::vector<uint8_t> &buffer)
{
    size_t count = 0U;
    for (int y = 0; y < kPhysicalHeight; ++y) {
        for (int x = 0; x < kPhysicalWidth; ++x) {
            if (pixel_level(buffer, x, y) ==
                static_cast<uint8_t>(GrayLevel::Black)) {
                ++count;
            }
        }
    }
    return count;
}

void write_preview(const std::vector<uint8_t> &buffer, const char *path)
{
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << kPhysicalWidth << " " << kPhysicalHeight
           << "\n255\n";
    for (int y = 0; y < kPhysicalHeight; ++y) {
        for (int x = 0; x < kPhysicalWidth; ++x) {
            const uint8_t value =
                static_cast<uint8_t>(pixel_level(buffer, x, y) * 85U);
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
        }
    }
}

}  // namespace

int main()
{
    std::vector<uint8_t> buffer(kStride * kPhysicalHeight, 0xFFU);
    Canvas canvas(kPhysicalWidth, kPhysicalHeight,
                  buffer.data(), buffer.size());
    DesktopPetState state = {};

    desktop_pet_page_render_egg(
        canvas, state, DesktopPetHatchFrame::Resting);
    assert(canvas.rotation() == CanvasRotation::Deg90CounterClockwise);
    assert(black_pixel_count(buffer) > 7000U);
    assert(desktop_pet_page_egg_action_at(240, 350) ==
           DesktopPetAction::TapEgg);
    assert(desktop_pet_page_egg_action_at(435, 180) ==
           DesktopPetAction::OpenTutorial);
    assert(desktop_pet_page_egg_action_at(440, 35) ==
           DesktopPetAction::OpenTest);
    assert(desktop_pet_page_egg_action_at(20, 760) ==
           DesktopPetAction::None);
    assert(logical_black_pixel_count(buffer, 400, 140, 470, 225) >
           100U);
    write_preview(buffer, "/tmp/desktop_pet_egg_intact.ppm");

    state.hatch_taps = 1U;
    desktop_pet_page_render_egg(
        canvas, state, DesktopPetHatchFrame::Cracked);
    write_preview(buffer, "/tmp/desktop_pet_egg_crack_one.ppm");
    state.hatch_taps = 2U;
    desktop_pet_page_render_egg(
        canvas, state, DesktopPetHatchFrame::Cracked);
    write_preview(buffer, "/tmp/desktop_pet_egg_crack_two.ppm");
    state.hatch_taps = kDesktopPetRequiredHatchTaps;
    state.pet.stage = PetLifeStage::Hatchling;
    desktop_pet_page_render_egg(
        canvas, state, DesktopPetHatchFrame::Opened);
    write_preview(buffer, "/tmp/desktop_pet_egg_open.ppm");

    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Normal,
        "LET'S SPEND TODAY TOGETHER.");
    assert(!desktop_pet_state_has_name(state));
    assert(desktop_pet_page_action_at(false, 120, 35) ==
           DesktopPetAction::OpenNameEditor);
    write_preview(buffer, "/tmp/desktop_pet_unnamed_home.ppm");

    desktop_pet_page_render_name_editor(
        canvas, "BUNNY", DesktopPetKeyboardMode::Letters, false, false);
    assert(canvas.rotation() == CanvasRotation::Deg90CounterClockwise);
    assert(black_pixel_count(buffer) > 12000U);
    assert(desktop_pet_page_name_action_at(
               DesktopPetKeyboardMode::Letters, false, 31, 300) ==
           DesktopPetNameAction::KeyQ);
    assert(desktop_pet_page_name_action_at(
               DesktopPetKeyboardMode::Letters, false, 240, 540) ==
           DesktopPetNameAction::Space);
    assert(desktop_pet_page_name_action_at(
               DesktopPetKeyboardMode::Letters, false, 300, 630) ==
           DesktopPetNameAction::Apply);
    assert(desktop_pet_page_name_action_at(
               DesktopPetKeyboardMode::Letters, false, 40, 50) ==
           DesktopPetNameAction::None);
    assert(desktop_pet_page_name_action_at(
               DesktopPetKeyboardMode::Letters, true, 40, 50) ==
           DesktopPetNameAction::Back);
    char name_character = '\0';
    assert(desktop_pet_name_action_character(
        DesktopPetNameAction::KeyZ, name_character));
    assert(name_character == 'Z');
    assert(desktop_pet_name_action_can_batch(
        DesktopPetNameAction::Delete));
    assert(!desktop_pet_name_action_can_batch(
        DesktopPetNameAction::Apply));
    write_preview(buffer, "/tmp/desktop_pet_name_letters.ppm");

    desktop_pet_page_render_name_editor(
        canvas, "BUNNY 2", DesktopPetKeyboardMode::Numbers, false, true);
    assert(desktop_pet_page_name_action_at(
               DesktopPetKeyboardMode::Numbers, true, 63, 328) ==
           DesktopPetNameAction::Digit1);
    assert(desktop_pet_page_name_action_at(
               DesktopPetKeyboardMode::Numbers, true, 415, 418) ==
           DesktopPetNameAction::Digit0);
    write_preview(buffer, "/tmp/desktop_pet_name_numbers.ppm");

    assert(desktop_pet_state_set_name(state, "BUNNY"));
    state.pet.day = 4U;
    const PetRtcDateTime preview_date = {2026U, 8U, 24U, 12U, 0U, 0U};
    assert(pet_rtc_time_to_epoch(preview_date,
                                 state.pet.last_rtc_epoch_seconds));

    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Normal,
        "LET'S SPEND TODAY TOGETHER.");
    assert(canvas.rotation() == CanvasRotation::Deg90CounterClockwise);
    assert(black_pixel_count(buffer) > 18000U);
    assert(desktop_pet_page_action_at(false, 80, 680) ==
           DesktopPetAction::Feed);
    assert(desktop_pet_page_action_at(false, 240, 680) ==
           DesktopPetAction::Talk);
    assert(desktop_pet_page_action_at(false, 400, 680) ==
           DesktopPetAction::Play);
    assert(desktop_pet_page_action_at(false, 240, 500) ==
           DesktopPetAction::Pet);
    assert(desktop_pet_page_action_at(false, 380, 95) ==
           DesktopPetAction::Sleep);
    assert(desktop_pet_page_action_at(false, 380, 145) ==
           DesktopPetAction::None);
    assert(desktop_pet_page_action_at(false, 435, 180) ==
           DesktopPetAction::OpenTutorial);
    assert(desktop_pet_page_action_at(false, 120, 35) ==
           DesktopPetAction::OpenNameEditor);
    assert(desktop_pet_page_action_at(false, 440, 35) ==
           DesktopPetAction::OpenTest);
    assert(logical_black_pixel_count(buffer, 400, 140, 470, 225) >
           100U);
    write_preview(buffer, "/tmp/desktop_pet_home.ppm");

    state.pet.needs.energy = 9U;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Normal,
        "I'M LOW ON ENERGY. MAY I REST?");
    const std::vector<uint8_t> low_energy_idle_frame = buffer;
    write_preview(buffer, "/tmp/desktop_pet_hatchling_energy_low.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Pet, DesktopPetIdleFrame::Normal,
        "THAT FEELS SO NICE!");
    assert(buffer != low_energy_idle_frame);

    state.pet.needs.energy = 0U;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Normal,
        "TAP ME TO TUCK ME IN.");
    write_preview(buffer, "/tmp/desktop_pet_hatchling_energy_zero.ppm");
    const std::vector<uint8_t> depleted_frame = buffer;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Play, DesktopPetIdleFrame::Stretch,
        "TAP ME TO TUCK ME IN.");
    assert(buffer == depleted_frame);
    state.pet.needs.energy = 80U;

    state.pet.activity = PetActivity::Sleeping;
    state.pet.needs.energy = 44U;
    desktop_pet_page_render_sleep(canvas, state, false);
    assert(black_pixel_count(buffer) > 24000U);
    assert(desktop_pet_page_sleep_action_at(240, 630) ==
           DesktopPetAction::Wake);
    assert(desktop_pet_page_sleep_action_at(240, 300) ==
           DesktopPetAction::None);
    write_preview(buffer, "/tmp/desktop_pet_sleep_a.ppm");
    state.pet.needs.energy = 56U;
    desktop_pet_page_render_sleep(canvas, state, true);
    write_preview(buffer, "/tmp/desktop_pet_sleep_b.ppm");
    state.pet.activity = PetActivity::Idle;

    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Feed, DesktopPetIdleFrame::Normal,
        "YUM! THAT WAS DELICIOUS!");
    write_preview(buffer, "/tmp/desktop_pet_feed.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Pet, DesktopPetIdleFrame::Normal,
        "THAT FEELS SO NICE!");
    write_preview(buffer, "/tmp/desktop_pet_pet.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Play, DesktopPetIdleFrame::Normal,
        "LET'S CHASE IT!");
    write_preview(buffer, "/tmp/desktop_pet_play.ppm");

    state.pet.needs.food = 20U;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Hungry,
        "CARROT. NOW. PLEASE.");
    write_preview(buffer, "/tmp/desktop_pet_hungry.ppm");

    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Blink,
        "JUST A HAPPY LITTLE BLINK.");
    write_preview(buffer, "/tmp/desktop_pet_idle_blink.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::EarTwitch,
        "DID YOU HEAR THAT?");
    write_preview(buffer, "/tmp/desktop_pet_idle_ear.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::LookAround,
        "WHAT'S OVER THERE?");
    write_preview(buffer, "/tmp/desktop_pet_idle_look.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Stretch,
        "BIG STRETCH!");
    write_preview(buffer, "/tmp/desktop_pet_idle_stretch.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Tired,
        "RESTING MY EYES A MOMENT.");
    write_preview(buffer, "/tmp/desktop_pet_idle_tired.ppm");

    state.pet.stage = PetLifeStage::Child;
    state.pet.growth = kDesktopPetHatchlingGrowthLimit;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Normal,
        "I'M READY FOR AN ADVENTURE.");
    assert(black_pixel_count(buffer) > 18000U);
    write_preview(buffer, "/tmp/desktop_pet_child_home.ppm");
    state.pet.needs.energy = 0U;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Normal,
        "TAP ME TO TUCK ME IN.");
    write_preview(buffer, "/tmp/desktop_pet_child_energy_zero.ppm");
    state.pet.needs.energy = 80U;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Feed, DesktopPetIdleFrame::Normal,
        "ENERGY FOR ADVENTURES!");
    write_preview(buffer, "/tmp/desktop_pet_child_feed.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Pet, DesktopPetIdleFrame::Normal,
        "MY FUR GREW EXTRA SOFT!");
    write_preview(buffer, "/tmp/desktop_pet_child_pet.ppm");
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Play, DesktopPetIdleFrame::Normal,
        "CATCH ME IF YOU CAN!");
    write_preview(buffer, "/tmp/desktop_pet_child_play.ppm");

    desktop_pet_page_render_evolution(
        canvas, state, DesktopPetEvolutionFrame::Starting);
    write_preview(buffer, "/tmp/desktop_pet_evolution_start.ppm");
    desktop_pet_page_render_evolution(
        canvas, state, DesktopPetEvolutionFrame::Silhouette);
    write_preview(buffer, "/tmp/desktop_pet_evolution_silhouette.ppm");
    desktop_pet_page_render_evolution(
        canvas, state, DesktopPetEvolutionFrame::Revealed);
    write_preview(buffer, "/tmp/desktop_pet_evolution_reveal.ppm");

    desktop_pet_page_render_personality_choice(canvas, state);
    assert(desktop_pet_page_personality_action_at(240, 280) ==
           DesktopPetAction::ChooseFoodie);
    assert(desktop_pet_page_personality_action_at(240, 425) ==
           DesktopPetAction::ChooseAffectionate);
    assert(desktop_pet_page_personality_action_at(240, 570) ==
           DesktopPetAction::ChooseActive);
    assert(desktop_pet_page_personality_action_at(20, 700) ==
           DesktopPetAction::None);
    write_preview(buffer, "/tmp/desktop_pet_personality_choice.ppm");

    state.pet.stage = PetLifeStage::Youth;
    state.pet.growth = kDesktopPetChildGrowthLimit;
    state.pet.branch = PetPersonalityBranch::Foodie;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Normal,
        "I PACKED A SNACK FOR US.");
    assert(black_pixel_count(buffer) > 18000U);
    write_preview(buffer, "/tmp/desktop_pet_youth_foodie.ppm");
    state.pet.branch = PetPersonalityBranch::Affectionate;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Pet, DesktopPetIdleFrame::Normal,
        "I'M ALWAYS CLOSE BY.");
    write_preview(buffer, "/tmp/desktop_pet_youth_affectionate.ppm");
    state.pet.branch = PetPersonalityBranch::Active;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Play, DesktopPetIdleFrame::Normal,
        "LET'S RACE TOGETHER!");
    write_preview(buffer, "/tmp/desktop_pet_youth_active.ppm");
    desktop_pet_page_render_evolution(
        canvas, state, DesktopPetEvolutionFrame::Revealed);
    write_preview(buffer, "/tmp/desktop_pet_youth_evolution.ppm");

    state.pet.stage = PetLifeStage::Adult;
    state.pet.growth = kDesktopPetYouthGrowthLimit;
    state.pet.branch = PetPersonalityBranch::Foodie;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Normal,
        "I PERFECTED A RECIPE FOR US.");
    assert(black_pixel_count(buffer) > 18000U);
    write_preview(buffer, "/tmp/desktop_pet_adult_foodie.ppm");
    state.pet.branch = PetPersonalityBranch::Affectionate;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Pet, DesktopPetIdleFrame::Normal,
        "WE GREW UP SIDE BY SIDE.");
    write_preview(buffer, "/tmp/desktop_pet_adult_affectionate.ppm");
    state.pet.branch = PetPersonalityBranch::Active;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Play, DesktopPetIdleFrame::Normal,
        "I MAPPED A NEW TRAIL FOR US.");
    write_preview(buffer, "/tmp/desktop_pet_adult_active.ppm");
    state.pet.needs.energy = 0U;
    desktop_pet_page_render_home(
        canvas, state, DesktopPetPose::Idle, DesktopPetIdleFrame::Normal,
        "TAP ME TO TUCK ME IN.");
    write_preview(buffer, "/tmp/desktop_pet_adult_energy_zero.ppm");
    state.pet.needs.energy = 80U;
    desktop_pet_page_render_evolution(
        canvas, state, DesktopPetEvolutionFrame::Revealed);
    write_preview(buffer, "/tmp/desktop_pet_adult_evolution.ppm");

    desktop_pet_page_render_care_celebration(
        canvas, state, 3U, DesktopPetCelebrationFrame::Proud);
    assert(black_pixel_count(buffer) > 12000U);
    assert(logical_black_pixel_count(buffer, 0, 655, 479, 673) == 0U);
    assert(logical_pixel_level(buffer, 30, 674) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert(logical_pixel_level(buffer, 449, 741) ==
           static_cast<uint8_t>(GrayLevel::Black));
    assert_white_ink_is_centered(buffer, 30, 674, 449, 741);
    const std::vector<uint8_t> proud_celebration = buffer;
    write_preview(buffer, "/tmp/desktop_pet_streak_3_proud.ppm");
    desktop_pet_page_render_care_celebration(
        canvas, state, 3U, DesktopPetCelebrationFrame::Jump);
    assert(black_pixel_count(buffer) > 12000U);
    assert(buffer != proud_celebration);
    assert(logical_black_pixel_count(buffer, 0, 655, 479, 673) == 0U);
    assert_white_ink_is_centered(buffer, 30, 674, 449, 741);
    write_preview(buffer, "/tmp/desktop_pet_streak_3_jump.ppm");

    desktop_pet_page_render_test(canvas, state, false);
    assert(desktop_pet_page_action_at(true, 240, 250) ==
           DesktopPetAction::NextDay);
    assert(desktop_pet_page_action_at(true, 240, 300) ==
           DesktopPetAction::AddGrowth);
    assert(desktop_pet_page_action_at(true, 240, 360) ==
           DesktopPetAction::AddLove);
    assert(desktop_pet_page_action_at(true, 240, 420) ==
           DesktopPetAction::ReduceEnergy);
    assert(desktop_pet_page_action_at(true, 240, 480) ==
           DesktopPetAction::Sleep);
    assert(desktop_pet_page_action_at(true, 240, 545) ==
           DesktopPetAction::StartOuting);
    assert(desktop_pet_page_action_at(true, 240, 625) ==
           DesktopPetAction::Reset);
    write_preview(buffer, "/tmp/desktop_pet_test.ppm");

    DesktopPetOutingSession outing = {};
    outing.away_duration_ms = 27000U;
    outing.phase = DesktopPetOutingPhase::Packing;
    desktop_pet_page_render_outing(canvas, state, outing, false);
    assert(black_pixel_count(buffer) > 18000U);
    assert(desktop_pet_page_outing_action_at(
               outing.phase, 240, 640) == DesktopPetAction::None);
    write_preview(buffer, "/tmp/desktop_pet_outing_pack.ppm");

    outing.phase = DesktopPetOutingPhase::Leaving;
    desktop_pet_page_render_outing(canvas, state, outing, false);
    write_preview(buffer, "/tmp/desktop_pet_outing_leave.ppm");

    outing.phase = DesktopPetOutingPhase::Away;
    desktop_pet_page_render_outing(canvas, state, outing, true);
    assert(desktop_pet_page_outing_action_at(
               outing.phase, 240, 700) == DesktopPetAction::CallHome);
    assert(desktop_pet_page_outing_action_at(
               outing.phase, 20, 580) == DesktopPetAction::None);
    write_preview(buffer, "/tmp/desktop_pet_outing_away.ppm");

    outing.phase = DesktopPetOutingPhase::Returning;
    desktop_pet_page_render_outing(canvas, state, outing, false);
    write_preview(buffer, "/tmp/desktop_pet_outing_return.ppm");

    outing.phase = DesktopPetOutingPhase::Reunion;
    desktop_pet_page_render_outing(canvas, state, outing, false);
    write_preview(buffer, "/tmp/desktop_pet_outing_reunion.ppm");
    return 0;
}
