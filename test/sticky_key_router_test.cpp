#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "sticky_key_router.h"

namespace {

constexpr int64_t kSecond = 1000000LL;
constexpr int64_t kMs = 1000LL;
// Well after the boot quiet period.
constexpr int64_t kT0 = 10 * kSecond;

KeyContext page(KeyPolicy policy, bool flipped = false)
{
    KeyContext context;
    context.policy = policy;
    context.side_keys_flipped = flipped;
    return context;
}

KeyContext launcher()
{
    KeyContext context;
    context.launcher_open = true;
    context.policy = KeyPolicy::Free;
    return context;
}

KeyDecision route(KeyRouter &router, KeyInput input, const KeyContext &context,
                  int64_t at_us = kT0)
{
    return router.route({input, at_us}, context);
}

void expect(const KeyDecision &decision, KeyAction action, int step = 0)
{
    assert(decision.action == action);
    assert(decision.step == step);
}

void top_single_walks_home_then_launcher_then_back()
{
    KeyRouter router;
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Free)),
           KeyAction::GoHome);
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Locked)),
           KeyAction::GoHome);
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Pager)),
           KeyAction::GoHome);
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Home)),
           KeyAction::OpenLauncher);
    expect(route(router, KeyInput::TopSingle, launcher()),
           KeyAction::CloseLauncher);
}

void top_double_runs_the_shortcut_except_on_capture_and_wizard_pages()
{
    KeyRouter router;
    for (KeyPolicy policy : {KeyPolicy::Free, KeyPolicy::Home, KeyPolicy::Pager,
                             KeyPolicy::Locked}) {
        expect(route(router, KeyInput::TopDouble, page(policy)),
               KeyAction::RunShortcut);
    }
    expect(route(router, KeyInput::TopDouble, page(KeyPolicy::Capture)),
           KeyAction::None);
    expect(route(router, KeyInput::TopDouble, page(KeyPolicy::Wizard)),
           KeyAction::None);
    expect(route(router, KeyInput::TopDouble, launcher()),
           KeyAction::RunShortcut);
}

void wizard_pages_ignore_every_key()
{
    KeyRouter router;
    const KeyContext wizard = page(KeyPolicy::Wizard);
    for (KeyInput input : {KeyInput::TopPress, KeyInput::TopSingle,
                           KeyInput::TopDouble, KeyInput::SideLeft,
                           KeyInput::SideRight}) {
        expect(route(router, input, wizard), KeyAction::None);
    }
}

void side_keys_switch_apps_on_free_and_home_pages()
{
    KeyRouter router;
    for (KeyPolicy policy : {KeyPolicy::Free, KeyPolicy::Home}) {
        expect(route(router, KeyInput::SideRight, page(policy)),
               KeyAction::SwitchApp, 1);
        expect(route(router, KeyInput::SideLeft, page(policy)),
               KeyAction::SwitchApp, -1);
    }
}

void side_keys_flip_the_list_on_pager_pages()
{
    KeyRouter router;
    expect(route(router, KeyInput::SideRight, page(KeyPolicy::Pager)),
           KeyAction::PageFlip, 1);
    expect(route(router, KeyInput::SideLeft, page(KeyPolicy::Pager)),
           KeyAction::PageFlip, -1);
}

void side_keys_are_ignored_where_they_could_lose_input()
{
    KeyRouter router;
    for (KeyPolicy policy : {KeyPolicy::Locked, KeyPolicy::Capture,
                             KeyPolicy::Wizard}) {
        expect(route(router, KeyInput::SideRight, page(policy)),
               KeyAction::None);
        expect(route(router, KeyInput::SideLeft, page(policy)),
               KeyAction::None);
    }
    expect(route(router, KeyInput::SideRight, launcher()), KeyAction::None);
    expect(route(router, KeyInput::SideLeft, launcher()), KeyAction::None);
}

void side_keys_swap_when_the_screen_is_turned()
{
    KeyRouter router;
    expect(route(router, KeyInput::SideRight, page(KeyPolicy::Free, true)),
           KeyAction::SwitchApp, -1);
    expect(route(router, KeyInput::SideLeft, page(KeyPolicy::Free, true)),
           KeyAction::SwitchApp, 1);
    assert(side_key_step(true, false) == 1);
    assert(side_key_step(false, false) == -1);
    assert(side_key_step(true, true) == -1);
    assert(side_key_step(false, true) == 1);
}

void top_press_is_only_a_command_on_capture_pages()
{
    KeyRouter router;
    expect(route(router, KeyInput::TopPress, page(KeyPolicy::Capture)),
           KeyAction::CapturePress);
    for (KeyPolicy policy : {KeyPolicy::Free, KeyPolicy::Home, KeyPolicy::Pager,
                             KeyPolicy::Locked, KeyPolicy::Wizard}) {
        expect(route(router, KeyInput::TopPress, page(policy)), KeyAction::None);
    }
    expect(route(router, KeyInput::TopPress, launcher()), KeyAction::None);
}

void capture_page_ignores_top_clicks()
{
    KeyRouter router;
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Capture)),
           KeyAction::None);
}

void click_echo_of_a_capture_press_does_not_leave_the_page_that_replaced_it()
{
    KeyRouter router;
    // The tenth press saves the session and the app shows the result page.
    expect(route(router, KeyInput::TopPress, page(KeyPolicy::Capture), kT0),
           KeyAction::CapturePress);
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Free),
                 kT0 + 250 * kMs),
           KeyAction::None);
    expect(route(router, KeyInput::TopDouble, page(KeyPolicy::Free),
                 kT0 + 700 * kMs),
           KeyAction::None);
    // A deliberate click later behaves normally again.
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Free),
                 kT0 + 701 * kMs),
           KeyAction::GoHome);
}

void echo_window_follows_the_latest_capture_press()
{
    KeyRouter router;
    route(router, KeyInput::TopPress, page(KeyPolicy::Capture), kT0);
    route(router, KeyInput::TopPress, page(KeyPolicy::Capture), kT0 + 600 * kMs);
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Free),
                 kT0 + 1200 * kMs),
           KeyAction::None);
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Free),
                 kT0 + 1400 * kMs),
           KeyAction::GoHome);
}

void side_keys_are_not_affected_by_the_capture_echo_window()
{
    KeyRouter router;
    route(router, KeyInput::TopPress, page(KeyPolicy::Capture), kT0);
    expect(route(router, KeyInput::SideRight, page(KeyPolicy::Free),
                 kT0 + 100 * kMs),
           KeyAction::SwitchApp, 1);
}

void keys_in_the_boot_quiet_period_are_dropped()
{
    KeyRouter router;
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Free),
                 KeyRouter::kBootQuietUs - 1),
           KeyAction::None);
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Free),
                 KeyRouter::kBootQuietUs),
           KeyAction::GoHome);
}

void events_pressed_before_a_transition_ended_are_dropped()
{
    KeyRouter router;
    router.ignore_events_before(kT0 + 2 * kSecond);
    expect(route(router, KeyInput::SideRight, page(KeyPolicy::Free),
                 kT0 + kSecond),
           KeyAction::None);
    expect(route(router, KeyInput::TopSingle, page(KeyPolicy::Free),
                 kT0 + kSecond),
           KeyAction::None);
    expect(route(router, KeyInput::SideRight, page(KeyPolicy::Free),
                 kT0 + 2 * kSecond),
           KeyAction::SwitchApp, 1);

    // The barrier only moves forward.
    router.ignore_events_before(kT0);
    expect(route(router, KeyInput::SideRight, page(KeyPolicy::Free),
                 kT0 + kSecond),
           KeyAction::None);
}

void launcher_swallows_everything_but_the_top_clicks()
{
    KeyRouter router;
    expect(route(router, KeyInput::TopPress, launcher()), KeyAction::None);
    expect(route(router, KeyInput::TopSingle, launcher()),
           KeyAction::CloseLauncher);
}

void merger_waits_for_the_window_and_sums_the_steps()
{
    SideStepMerger merger;
    int net = 99;
    assert(!merger.pending());
    assert(!merger.take(kT0, net));

    merger.add(1, kT0);
    merger.add(1, kT0 + 150 * kMs);
    assert(merger.pending());
    assert(!merger.take(kT0 + 549 * kMs, net));
    assert(merger.take(kT0 + 550 * kMs, net));
    assert(net == 2);
    assert(!merger.pending());
    assert(!merger.take(kT0 + kSecond, net));
}

void merger_cancels_opposite_presses_and_can_be_cleared()
{
    SideStepMerger merger;
    int net = 99;
    merger.add(1, kT0);
    merger.add(-1, kT0 + 100 * kMs);
    assert(merger.take(kT0 + 600 * kMs, net));
    assert(net == 0);

    merger.add(-1, kT0);
    merger.clear();
    assert(!merger.pending());
    assert(!merger.take(kT0 + kSecond, net));
}

}  // namespace

int main()
{
    top_single_walks_home_then_launcher_then_back();
    top_double_runs_the_shortcut_except_on_capture_and_wizard_pages();
    wizard_pages_ignore_every_key();
    side_keys_switch_apps_on_free_and_home_pages();
    side_keys_flip_the_list_on_pager_pages();
    side_keys_are_ignored_where_they_could_lose_input();
    side_keys_swap_when_the_screen_is_turned();
    top_press_is_only_a_command_on_capture_pages();
    capture_page_ignores_top_clicks();
    click_echo_of_a_capture_press_does_not_leave_the_page_that_replaced_it();
    echo_window_follows_the_latest_capture_press();
    side_keys_are_not_affected_by_the_capture_echo_window();
    keys_in_the_boot_quiet_period_are_dropped();
    events_pressed_before_a_transition_ended_are_dropped();
    launcher_swallows_everything_but_the_top_clicks();
    merger_waits_for_the_window_and_sums_the_steps();
    merger_cancels_opposite_presses_and_can_be_cleared();
    return 0;
}
