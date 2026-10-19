#include <cassert>
#include <initializer_list>

#include "sticky_shortcut.h"

namespace {

constexpr StickyAppId kApps[] = {StickyAppId::Pregnancy, StickyAppId::Weight,
                                 StickyAppId::Kicks, StickyAppId::Settings};
constexpr KeyPolicy kPolicies[] = {KeyPolicy::Locked, KeyPolicy::Free,
                                   KeyPolicy::Home,   KeyPolicy::Pager,
                                   KeyPolicy::Capture, KeyPolicy::Wizard};

void off_never_does_anything()
{
    for (StickyAppId app : kApps)
        for (KeyPolicy policy : kPolicies)
            assert(shortcut_action(StickyShortcutTarget::Off, app, policy) ==
                   ShortcutAction::None);
}

void baby_week_goes_home_unless_already_there()
{
    const StickyShortcutTarget t = StickyShortcutTarget::BabyWeek;
    assert(shortcut_action(t, StickyAppId::Weight, KeyPolicy::Free) == ShortcutAction::GoHome);
    assert(shortcut_action(t, StickyAppId::Weight, KeyPolicy::Locked) == ShortcutAction::GoHome);
    assert(shortcut_action(t, StickyAppId::Kicks, KeyPolicy::Capture) == ShortcutAction::GoHome);
    assert(shortcut_action(t, StickyAppId::Settings, KeyPolicy::Free) == ShortcutAction::GoHome);
    // On the Baby / Mom detail pages it returns to the dashboard.
    assert(shortcut_action(t, StickyAppId::Pregnancy, KeyPolicy::Free) == ShortcutAction::GoHome);
    // On the dashboard (or in the setup flow) there is nothing to do.
    assert(shortcut_action(t, StickyAppId::Pregnancy, KeyPolicy::Home) == ShortcutAction::None);
    assert(shortcut_action(t, StickyAppId::Pregnancy, KeyPolicy::Wizard) == ShortcutAction::None);
}

void weight_entry_never_replaces_a_weight_editor()
{
    const StickyShortcutTarget t = StickyShortcutTarget::WeightRecordToday;
    for (StickyAppId app : kApps) {
        for (KeyPolicy policy : kPolicies) {
            const bool protected_page =
                app == StickyAppId::Weight && policy == KeyPolicy::Locked;
            assert(shortcut_action(t, app, policy) ==
                   (protected_page ? ShortcutAction::None
                                   : ShortcutAction::OpenWeightToday));
        }
    }
}

void kick_start_never_replaces_a_running_counter()
{
    const StickyShortcutTarget t = StickyShortcutTarget::KickStart;
    for (StickyAppId app : kApps) {
        for (KeyPolicy policy : kPolicies) {
            const bool running = app == StickyAppId::Kicks && policy == KeyPolicy::Capture;
            assert(shortcut_action(t, app, policy) ==
                   (running ? ShortcutAction::None : ShortcutAction::OpenKickStart));
        }
    }
    // The same target from another app's Capture/Locked page still opens Kicks.
    assert(shortcut_action(t, StickyAppId::Weight, KeyPolicy::Locked) ==
           ShortcutAction::OpenKickStart);
}

void actions_name_the_app_they_open()
{
    assert(shortcut_app(ShortcutAction::OpenWeightToday) == StickyAppId::Weight);
    assert(shortcut_app(ShortcutAction::OpenKickStart) == StickyAppId::Kicks);
    assert(shortcut_app(ShortcutAction::GoHome) == StickyAppId::Pregnancy);
}

void default_target_is_weight_and_zero()
{
    assert(static_cast<uint8_t>(StickyShortcutTarget::WeightRecordToday) == 0U);
    assert(static_cast<uint8_t>(StickyShortcutTarget::Off) + 1U ==
           kStickyShortcutTargetCount);
}

}  // namespace

int main()
{
    off_never_does_anything();
    baby_week_goes_home_unless_already_there();
    weight_entry_never_replaces_a_weight_editor();
    kick_start_never_replaces_a_running_counter();
    actions_name_the_app_they_open();
    default_target_is_weight_and_zero();
    return 0;
}
