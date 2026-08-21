#include "sticky_app_router.h"

namespace {

bool is_upright(StickyImuOrientation orientation)
{
    return orientation == StickyImuOrientation::Landscape0 ||
           orientation == StickyImuOrientation::Landscape180 ||
           orientation == StickyImuOrientation::Portrait0 ||
           orientation == StickyImuOrientation::Portrait180;
}

bool is_physical_portrait(StickyImuOrientation orientation)
{
    // The calibrated sensor landscape axis corresponds to the device standing
    // vertically in the enclosure.
    // 传感器标定后的landscape轴对应设备外壳实际竖置。
    return orientation == StickyImuOrientation::Landscape0 ||
           orientation == StickyImuOrientation::Landscape180;
}

bool is_physical_landscape(StickyImuOrientation orientation)
{
    // The calibrated sensor portrait axis corresponds to the device lying
    // horizontally in the enclosure.
    // 传感器标定后的portrait轴对应设备外壳实际横置。
    return orientation == StickyImuOrientation::Portrait0 ||
           orientation == StickyImuOrientation::Portrait180;
}

StickyAppRouteResult selected_result(
    StickyAppRouterState &state,
    StickyAppId app,
    StickyImuOrientation from,
    StickyImuOrientation to)
{
    StickyAppRouteResult result = {};
    result.action = StickyAppRouteAction::AppSelected;
    result.selected_app = app;
    result.from_orientation = from;
    result.to_orientation = to;
    sticky_app_router_close(state);
    return result;
}

}  // namespace

void sticky_app_router_open(StickyAppRouterState &state,
                            StickyImuOrientation orientation)
{
    state.launcher_open = true;
    state.baseline_orientation =
        is_upright(orientation) ? orientation
                                : StickyImuOrientation::Unknown;
}

void sticky_app_router_close(StickyAppRouterState &state)
{
    state = {};
}

StickyAppRouteResult sticky_app_router_observed(
    StickyAppRouterState &state,
    StickyImuOrientation orientation)
{
    if (!state.launcher_open ||
        is_upright(state.baseline_orientation) ||
        !is_upright(orientation)) {
        return {};
    }
    state.baseline_orientation = orientation;
    StickyAppRouteResult result = {};
    result.action = StickyAppRouteAction::BaselineCaptured;
    result.from_orientation = orientation;
    result.to_orientation = orientation;
    return result;
}

StickyAppRouteResult sticky_app_router_settled(
    StickyAppRouterState &state,
    StickyImuOrientation orientation)
{
    if (!state.launcher_open || !is_upright(orientation)) {
        return {};
    }

    if (!is_upright(state.baseline_orientation)) {
        return sticky_app_router_observed(state, orientation);
    }

    const StickyImuOrientation from = state.baseline_orientation;
    if (is_physical_landscape(from) &&
        is_physical_portrait(orientation)) {
        return selected_result(state,
                               StickyAppId::Pomodoro,
                               from,
                               orientation);
    }
    if (is_physical_portrait(from) &&
        is_physical_landscape(orientation)) {
        return selected_result(state,
                               StickyAppId::StatusBoard,
                               from,
                               orientation);
    }
    return {};
}

StickyAppRouteResult sticky_app_router_shaking(
    StickyAppRouterState &state,
    uint32_t shake_duration_ms)
{
    if (!state.launcher_open ||
        shake_duration_ms < kStickyLauncherShakeSelectMs) {
        return {};
    }
    return selected_result(state,
                           StickyAppId::BookOfAnswers,
                           state.baseline_orientation,
                           state.baseline_orientation);
}

const char *sticky_app_route_action_name(StickyAppRouteAction action)
{
    switch (action) {
    case StickyAppRouteAction::None:
        return "none";
    case StickyAppRouteAction::BaselineCaptured:
        return "baseline_captured";
    case StickyAppRouteAction::AppSelected:
        return "app_selected";
    }
    return "unknown";
}
