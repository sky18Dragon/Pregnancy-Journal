#pragma once

#include <cstdint>

// Turns raw edges of the two side buttons into one deliberate event each: a
// single press (judged on release) or the two-second sleep chord. Free of
// hardware so host tests can drive it with a fake clock; the button driver
// feeds it edges with the time they happened and polls it from one task.
// 把两个侧键的原始按下/松开边沿整理成明确的事件：单按（松开时判定）或两秒休眠组合键；
// 不接触硬件，宿主测试用假时钟驱动。

enum class SideKey : uint8_t { Left, Right };

enum class SideKeyEventType : uint8_t {
    None,
    Single,      // one key pressed and released on its own
    SleepChord,  // both keys held together for the chord time
};

struct SideKeyEvent {
    SideKeyEventType type = SideKeyEventType::None;
    SideKey side = SideKey::Left;  // meaningful for Single
    int64_t at_us = 0;             // release time for Single, detection time for chord
};

class SideKeyTracker {
public:
    // A press held longer than this is not a single press (reserved).
    static constexpr int64_t kSingleMaxHoldUs = 1000000LL;
    // Both keys must go down within this gap to count as a chord attempt.
    static constexpr int64_t kChordPressWindowUs = 500000LL;
    // Hold time, from the later press, before the chord fires.
    static constexpr int64_t kChordHoldUs = 2000000LL;

    // Records one edge. Returns true when it completes a Single press.
    // Repeated edges (press while down, release while up) are ignored.
    bool on_edge(SideKey key, bool pressed, int64_t at_us, SideKeyEvent &event);

    // Checks for the sleep chord. Returns true once per both-keys-held episode.
    bool poll(int64_t now_us, SideKeyEvent &event);

private:
    struct KeyState {
        bool down = false;
        int64_t pressed_at_us = 0;
        // The other key was down at some point while this one was down, so the
        // press may be part of a chord and must never become a single press.
        bool tainted = false;
    };

    static unsigned index_of(SideKey key) { return key == SideKey::Left ? 0U : 1U; }

    KeyState keys_[2];
    bool chord_latched_ = false;
};
