#pragma once

#include <cstdint>

#include "key_policy.h"

// What the hardware keys mean on the current page. Pure decision logic with no
// hardware access, so host tests can cover every key x page combination. The
// coordinator turns button events into KeyInput, asks the router for a
// KeyAction and carries it out. See docs/pregnancy/key-shortcuts-design.md.
// 硬件按键在当前页面上的含义：纯决策逻辑，不接触硬件；协调器把按键事件交给它，再执行返回的动作。

enum class KeyInput : uint8_t {
    TopPress,       // top key went down (immediate)
    TopSingle,      // top key single click (reported after the double-click window)
    TopDouble,      // top key double click
    SideLeft,       // side key single press
    SideRight,
};

enum class KeyAction : uint8_t {
    None,
    OpenLauncher,
    CloseLauncher,
    GoHome,          // Baby Week, on its dashboard page
    RunShortcut,     // the configured double-click target
    SwitchApp,       // `step` apps along the launcher order
    PageFlip,        // `step` pages inside the current page's own list
    CapturePress,    // hand the press to the app as its primary action
};

struct KeyContext {
    bool launcher_open = false;
    KeyPolicy policy = KeyPolicy::Locked;
    // Side keys are mirrored when the screen is turned so that "left" stays the
    // key on the viewer's left.
    bool side_keys_flipped = false;
};

struct KeyInputEvent {
    KeyInput input = KeyInput::TopPress;
    int64_t at_us = 0;  // when the press happened, not when it is processed
};

struct KeyDecision {
    KeyAction action = KeyAction::None;
    int step = 0;  // +1 next / -1 previous for SwitchApp and PageFlip
};

class KeyRouter {
public:
    // Clicks reported this soon after a Capture press are its echo, not a command.
    static constexpr int64_t kCaptureEchoUs = 700000LL;
    // Keys pressed in the first second after boot include the one that woke the
    // device from deep sleep.
    static constexpr int64_t kBootQuietUs = 1000000LL;

    KeyRouter() : ignore_before_us_(kBootQuietUs) {}

    KeyDecision route(const KeyInputEvent &event, const KeyContext &context);

    // Drops every event pressed before `at_us`. The coordinator calls this after
    // a transition that blocked it for a full refresh, so keys pressed while the
    // panel was busy do not fire afterwards.
    void ignore_events_before(int64_t at_us) {
        if (at_us > ignore_before_us_) ignore_before_us_ = at_us;
    }

private:
    int64_t ignore_before_us_;
    int64_t last_capture_press_us_ = 0;
    bool has_capture_press_ = false;
};

// Merges quick side-key presses into one net move, so two fast presses skip an
// app with a single refresh instead of two ~2 s refreshes in a row.
// 把连续快按的侧键合并成一次净位移，避免连跳多次全刷。
class SideStepMerger {
public:
    static constexpr int64_t kMergeWindowUs = 400000LL;

    void add(int step, int64_t at_us) {
        net_ += step;
        last_at_us_ = at_us;
        pending_ = true;
    }
    bool pending() const { return pending_; }
    // True once the window since the last press has passed; `net_steps` may be 0.
    bool take(int64_t now_us, int &net_steps) {
        if (!pending_ || now_us - last_at_us_ < kMergeWindowUs) return false;
        net_steps = net_;
        net_ = 0;
        pending_ = false;
        return true;
    }
    void clear() {
        net_ = 0;
        pending_ = false;
    }

private:
    int net_ = 0;
    int64_t last_at_us_ = 0;
    bool pending_ = false;
};

// +1 / -1 for a side key; the mirrored layout swaps them.
int side_key_step(bool right_key, bool flipped);
