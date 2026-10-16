#include "sticky_key_router.h"

int side_key_step(bool right_key, bool flipped)
{
    return right_key != flipped ? 1 : -1;
}

namespace {

bool is_side(KeyInput input)
{
    return input == KeyInput::SideLeft || input == KeyInput::SideRight;
}

}  // namespace

KeyDecision KeyRouter::route(const KeyInputEvent &event, const KeyContext &context)
{
    KeyDecision none;
    if (event.at_us < ignore_before_us_) {
        return none;
    }

    if (context.launcher_open) {
        KeyDecision decision;
        if (event.input == KeyInput::TopSingle) {
            decision.action = KeyAction::CloseLauncher;
        } else if (event.input == KeyInput::TopDouble) {
            decision.action = KeyAction::RunShortcut;
        }
        return decision;
    }

    if (event.input == KeyInput::TopPress) {
        if (context.policy != KeyPolicy::Capture) {
            return none;
        }
        last_capture_press_us_ = event.at_us;
        has_capture_press_ = true;
        return {KeyAction::CapturePress, 0};
    }

    if (is_side(event.input)) {
        const int step = side_key_step(event.input == KeyInput::SideRight,
                                       context.side_keys_flipped);
        switch (context.policy) {
        case KeyPolicy::Free:
        case KeyPolicy::Home:
            return {KeyAction::SwitchApp, step};
        case KeyPolicy::Pager:
            return {KeyAction::PageFlip, step};
        default:
            return none;
        }
    }

    // TopSingle / TopDouble. A click reported right after a Capture press is
    // the echo of that press, even when the page has changed since.
    if (has_capture_press_ &&
        event.at_us - last_capture_press_us_ <= kCaptureEchoUs) {
        return none;
    }
    if (context.policy == KeyPolicy::Capture ||
        context.policy == KeyPolicy::Wizard) {
        return none;
    }
    if (event.input == KeyInput::TopDouble) {
        return {KeyAction::RunShortcut, 0};
    }
    return {context.policy == KeyPolicy::Home ? KeyAction::OpenLauncher
                                              : KeyAction::GoHome,
            0};
}
