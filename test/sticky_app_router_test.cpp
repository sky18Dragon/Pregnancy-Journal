#include <cassert>

#include "sticky_app_router.h"

int main()
{
    StickyAppRouterState state = {};

    // Sensor portrait means the enclosure is physically horizontal.
    // 传感器portrait表示设备外壳实际横置。
    sticky_app_router_open(state, StickyImuOrientation::Portrait0);
    StickyAppRouteResult route = sticky_app_router_settled(
        state, StickyImuOrientation::Landscape0);
    assert(route.action == StickyAppRouteAction::AppSelected);
    assert(route.selected_app == StickyAppId::Pomodoro);
    assert(!state.launcher_open);

    // Sensor landscape means the enclosure is physically vertical.
    // 传感器landscape表示设备外壳实际竖置。
    sticky_app_router_open(state, StickyImuOrientation::Landscape0);
    route = sticky_app_router_settled(
        state, StickyImuOrientation::Portrait0);
    assert(route.action == StickyAppRouteAction::AppSelected);
    assert(route.selected_app == StickyAppId::StatusBoard);

    sticky_app_router_open(state, StickyImuOrientation::Unknown);
    route = sticky_app_router_observed(
        state, StickyImuOrientation::Landscape180);
    assert(route.action == StickyAppRouteAction::BaselineCaptured);
    assert(state.launcher_open);
    route = sticky_app_router_settled(
        state, StickyImuOrientation::Portrait180);
    assert(route.selected_app == StickyAppId::StatusBoard);

    sticky_app_router_open(state, StickyImuOrientation::Landscape0);
    route = sticky_app_router_shaking(
        state, kStickyLauncherShakeSelectMs - 1U);
    assert(route.action == StickyAppRouteAction::None);
    assert(state.launcher_open);
    route = sticky_app_router_shaking(
        state, kStickyLauncherShakeSelectMs);
    assert(route.action == StickyAppRouteAction::AppSelected);
    assert(route.selected_app == StickyAppId::BookOfAnswers);
    assert(!state.launcher_open);

    sticky_app_router_open(state, StickyImuOrientation::FaceUp);
    sticky_app_router_close(state);
    route = sticky_app_router_shaking(state, 3000U);
    assert(route.action == StickyAppRouteAction::None);
    return 0;
}
