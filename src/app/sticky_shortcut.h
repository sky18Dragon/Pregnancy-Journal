#pragma once

#include <cstdint>

#include "key_policy.h"
#include "shortcut_target.h"
#include "sticky_app_id.h"

// What a double-click on the top button has to do, given the configured target
// and where the user is. Pure, so host tests can cover every combination.
// 双击顶键具体要做什么：由所选目标和当前位置决定；纯函数，宿主测试覆盖所有组合。
enum class ShortcutAction : uint8_t {
    None,             // nothing to do: off, or already there / not to be disturbed
    GoHome,           // Baby Week on its dashboard
    OpenWeightToday,  // weight entry for today
    OpenKickStart,    // start recording kicks with the remembered method
};

// `policy` is the key policy of the page the current app is showing.
// A page that holds unsaved input or a running session (weight editors, the
// kick counter) is never replaced: the shortcut does nothing there.
// 带未保存输入或进行中会话的页面（体重编辑页、胎动计数页）不会被替换，快捷键在那里不动作。
ShortcutAction shortcut_action(StickyShortcutTarget target, StickyAppId current,
                               KeyPolicy policy);

// The app a non-None action opens.
StickyAppId shortcut_app(ShortcutAction action);
