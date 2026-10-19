#include <cassert>
#include <cstdint>

#include "side_key_tracker.h"

namespace {

constexpr int64_t kMs = 1000LL;

bool press(SideKeyTracker &t, SideKey key, int64_t at_ms)
{
    SideKeyEvent event;
    return t.on_edge(key, true, at_ms * kMs, event);
}

bool release(SideKeyTracker &t, SideKey key, int64_t at_ms, SideKeyEvent &event)
{
    return t.on_edge(key, false, at_ms * kMs, event);
}

bool chord(SideKeyTracker &t, int64_t now_ms)
{
    SideKeyEvent event;
    return t.poll(now_ms * kMs, event);
}

void single_press_is_reported_on_release_for_each_side()
{
    SideKeyTracker t;
    SideKeyEvent event;
    assert(!press(t, SideKey::Left, 100));
    assert(release(t, SideKey::Left, 180, event));
    assert(event.type == SideKeyEventType::Single);
    assert(event.side == SideKey::Left);
    assert(event.at_us == 180 * kMs);

    assert(!press(t, SideKey::Right, 1000));
    assert(release(t, SideKey::Right, 1090, event));
    assert(event.side == SideKey::Right);
}

void hold_of_a_second_or_more_is_not_a_single_press()
{
    SideKeyTracker t;
    SideKeyEvent event;
    press(t, SideKey::Left, 0);
    assert(!release(t, SideKey::Left, 1000, event));

    press(t, SideKey::Left, 5000);
    assert(release(t, SideKey::Left, 5999, event));
}

void repeated_edges_are_ignored()
{
    SideKeyTracker t;
    SideKeyEvent event;
    assert(!release(t, SideKey::Left, 10, event));
    press(t, SideKey::Left, 100);
    press(t, SideKey::Left, 150);  // bounce: still the first press
    assert(release(t, SideKey::Left, 400, event));
    assert(event.at_us == 400 * kMs);
    assert(!release(t, SideKey::Left, 450, event));
}

void overlapping_presses_never_become_single_presses()
{
    SideKeyTracker t;
    SideKeyEvent event;
    // Right goes down while Left is held, then both are let go early.
    press(t, SideKey::Left, 0);
    press(t, SideKey::Right, 200);
    assert(!release(t, SideKey::Left, 400, event));
    assert(!release(t, SideKey::Right, 500, event));

    // The tracker is clean again afterwards.
    press(t, SideKey::Right, 2000);
    assert(release(t, SideKey::Right, 2100, event));
    assert(event.side == SideKey::Right);
}

void second_key_pressed_after_first_is_already_released_is_a_new_single()
{
    SideKeyTracker t;
    SideKeyEvent event;
    press(t, SideKey::Left, 0);
    assert(release(t, SideKey::Left, 100, event));
    press(t, SideKey::Right, 150);
    assert(release(t, SideKey::Right, 250, event));
    assert(event.side == SideKey::Right);
}

void chord_fires_once_after_two_seconds_from_the_later_press()
{
    SideKeyTracker t;
    SideKeyEvent event;
    press(t, SideKey::Left, 0);
    press(t, SideKey::Right, 300);
    assert(!chord(t, 2299));
    assert(t.poll(2300 * kMs, event));
    assert(event.type == SideKeyEventType::SleepChord);
    assert(event.at_us == 2300 * kMs);
    assert(!chord(t, 2400));  // latched until a key is released
}

void chord_needs_both_presses_within_the_window()
{
    SideKeyTracker t;
    press(t, SideKey::Left, 0);
    press(t, SideKey::Right, 501);
    assert(!chord(t, 10000));

    SideKeyTracker edge;
    press(edge, SideKey::Left, 0);
    press(edge, SideKey::Right, 500);
    assert(chord(edge, 2500));
}

void releasing_after_the_chord_gives_no_single_press_and_rearms_it()
{
    SideKeyTracker t;
    SideKeyEvent event;
    press(t, SideKey::Left, 0);
    press(t, SideKey::Right, 100);
    assert(chord(t, 2100));
    assert(!release(t, SideKey::Left, 2200, event));
    assert(!release(t, SideKey::Right, 2250, event));

    // A fresh chord can be made after both keys were let go.
    press(t, SideKey::Left, 5000);
    press(t, SideKey::Right, 5050);
    assert(chord(t, 7050));
}

void re_pressing_one_key_while_the_other_is_held_is_not_a_single_press()
{
    SideKeyTracker t;
    SideKeyEvent event;
    press(t, SideKey::Left, 0);
    press(t, SideKey::Right, 100);
    assert(!release(t, SideKey::Left, 300, event));
    press(t, SideKey::Left, 400);
    assert(!release(t, SideKey::Left, 500, event));
}

void chord_poll_without_both_keys_down_does_nothing()
{
    SideKeyTracker t;
    assert(!chord(t, 5000));
    press(t, SideKey::Left, 0);
    assert(!chord(t, 5000));
}

}  // namespace

int main()
{
    single_press_is_reported_on_release_for_each_side();
    hold_of_a_second_or_more_is_not_a_single_press();
    repeated_edges_are_ignored();
    overlapping_presses_never_become_single_presses();
    second_key_pressed_after_first_is_already_released_is_a_new_single();
    chord_fires_once_after_two_seconds_from_the_later_press();
    chord_needs_both_presses_within_the_window();
    releasing_after_the_chord_gives_no_single_press_and_rearms_it();
    re_pressing_one_key_while_the_other_is_held_is_not_a_single_press();
    chord_poll_without_both_keys_down_does_nothing();
    return 0;
}
