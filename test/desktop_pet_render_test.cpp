#include <cassert>
#include <cstdint>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "desktop_pet_pages.h"

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
    assert(desktop_pet_page_action_at(false, 440, 35) ==
           DesktopPetAction::OpenTest);
    write_preview(buffer, "/tmp/desktop_pet_home.ppm");

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
    desktop_pet_page_render_evolution(
        canvas, state, DesktopPetEvolutionFrame::Revealed);
    write_preview(buffer, "/tmp/desktop_pet_adult_evolution.ppm");

    desktop_pet_page_render_test(canvas, state, false);
    assert(desktop_pet_page_action_at(true, 240, 290) ==
           DesktopPetAction::NextDay);
    assert(desktop_pet_page_action_at(true, 240, 380) ==
           DesktopPetAction::AddGrowth);
    assert(desktop_pet_page_action_at(true, 240, 470) ==
           DesktopPetAction::AddLove);
    assert(desktop_pet_page_action_at(true, 240, 590) ==
           DesktopPetAction::Reset);
    write_preview(buffer, "/tmp/desktop_pet_test.ppm");
    return 0;
}
