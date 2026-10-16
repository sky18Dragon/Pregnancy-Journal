#include "sticky_shortcut.h"

ShortcutAction shortcut_action(StickyShortcutTarget target, StickyAppId current,
                               KeyPolicy policy)
{
    switch (target) {
    case StickyShortcutTarget::Off:
        return ShortcutAction::None;
    case StickyShortcutTarget::BabyWeek:
        // Already on the dashboard (or in the setup flow): nothing to redraw.
        if (current == StickyAppId::Pregnancy &&
            (policy == KeyPolicy::Home || policy == KeyPolicy::Wizard)) {
            return ShortcutAction::None;
        }
        return ShortcutAction::GoHome;
    case StickyShortcutTarget::WeightRecordToday:
        // Weight's editor, profile and confirmation pages are its Locked pages.
        if (current == StickyAppId::Weight && policy == KeyPolicy::Locked) {
            return ShortcutAction::None;
        }
        return ShortcutAction::OpenWeightToday;
    case StickyShortcutTarget::KickStart:
        if (current == StickyAppId::Kicks && policy == KeyPolicy::Capture) {
            return ShortcutAction::None;
        }
        return ShortcutAction::OpenKickStart;
    }
    return ShortcutAction::None;
}

StickyAppId shortcut_app(ShortcutAction action)
{
    switch (action) {
    case ShortcutAction::OpenWeightToday:
        return StickyAppId::Weight;
    case ShortcutAction::OpenKickStart:
        return StickyAppId::Kicks;
    case ShortcutAction::GoHome:
    case ShortcutAction::None:
        break;
    }
    return StickyAppId::Pregnancy;
}
