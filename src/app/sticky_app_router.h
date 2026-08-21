#pragma once

#include <cstdint>

#include "sticky_app_id.h"
#include "sticky_orientation.h"

constexpr uint32_t kStickyLauncherShakeSelectMs = 800U;
constexpr uint8_t kStickyLauncherRotationStableSamples = 5U;

enum class StickyAppRouteAction {
    None,
    BaselineCaptured,
    AppSelected,
};

struct StickyAppRouterState {
    bool launcher_open = false;
    StickyImuOrientation baseline_orientation =
        StickyImuOrientation::Unknown;
};

struct StickyAppRouteResult {
    StickyAppRouteAction action = StickyAppRouteAction::None;
    StickyAppId selected_app = StickyAppId::DesktopPet;
    StickyImuOrientation from_orientation =
        StickyImuOrientation::Unknown;
    StickyImuOrientation to_orientation =
        StickyImuOrientation::Unknown;
};

// Opens a fresh routing session with the latest settled orientation.
// 使用最近一次稳定姿态打开新的应用路由会话。
void sticky_app_router_open(StickyAppRouterState &state,
                            StickyImuOrientation orientation);

// Closes the current routing session after cancellation or touch selection.
// 在取消或触摸选择完成后关闭当前应用路由会话。
void sticky_app_router_close(StickyAppRouterState &state);

// Captures the first upright raw sample immediately after the launcher opens.
// 在选择窗口打开后立即记录第一笔有效直立原始姿态。
StickyAppRouteResult sticky_app_router_observed(
    StickyAppRouterState &state,
    StickyImuOrientation orientation);

// Captures the baseline or maps one completed physical orientation change.
// 记录起始方向，或映射一次已经放稳的真实横竖方向变化。
StickyAppRouteResult sticky_app_router_settled(
    StickyAppRouterState &state,
    StickyImuOrientation orientation);

// Accepts a fast rotation only after a fresh quiet window and outside shaking.
// 仅在全新安静窗口达标且未摇晃时接受快速旋转。
StickyAppRouteResult sticky_app_router_rotation_candidate(
    StickyAppRouterState &state,
    StickyImuOrientation orientation,
    uint8_t stable_samples,
    bool shake_active);

// Selects Book of Answers after one continuous launcher shake session.
// 在选择窗口内持续摇晃达到门槛后选择答案书。
StickyAppRouteResult sticky_app_router_shaking(
    StickyAppRouterState &state,
    uint32_t shake_duration_ms);

const char *sticky_app_route_action_name(StickyAppRouteAction action);
