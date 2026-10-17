#pragma once

#include <cstdint>

constexpr uint16_t kStickyTouchSensorWidth = 480U;
constexpr uint16_t kStickyTouchSensorHeight = 800U;
constexpr uint32_t kStickyTouchReadFailureThreshold = 5U;
constexpr uint32_t kStickyTouchMaximumContactMs = 10000U;
// A controller that comes up reporting a wrong geometry (seen as 2048x2048 after some boots and
// deep-sleep wakes) delivers no touches at all, and pulsing its reset pin does not free it. Start-up
// therefore repeats the whole start after power-cycling the touch rail, and a background retry keeps
// doing so while no contact has ever been read.
// 控制器启动后若报告错误的分辨率（部分开机/深睡眠唤醒后出现 2048x2048），将收不到任何触摸，且只拉复位脚
// 救不回来；因此启动时掉电重启触摸电源后整体重来，并在从未读到触摸前由后台重试继续这样做。
constexpr uint32_t kStickyTouchInitAttempts = 3U;
constexpr uint32_t kStickyTouchGeometryRetryFirstMs = 2000U;
constexpr uint32_t kStickyTouchGeometryRetryMaxMs = 30000U;

struct StickyTouchSensorResolution {
    uint16_t width;
    uint16_t height;
    bool used_fallback;
};

// Selects the verified Sticky sensor geometry when a register read is unusable.
// 当寄存器读取结果不可用时，选用Sticky已验证的触摸分辨率。
StickyTouchSensorResolution sticky_touch_select_sensor_resolution(
    bool read_succeeded,
    uint16_t reported_width,
    uint16_t reported_height);

// True when the controller read succeeded and reported the verified Sticky sensor geometry.
// 读取成功且报告的是 Sticky 已验证的触摸分辨率。
bool sticky_touch_geometry_valid(bool read_succeeded, uint16_t reported_width,
                                 uint16_t reported_height);

// Delay before the next background retry after `retries_so_far` retries: 2 s, 4 s, 8 s, 16 s, then
// 30 s. Backs off so a controller that stays broken is not reset over and over.
// 距下一次后台重试的等待：2 秒、4 秒、8 秒、16 秒，之后每次 30 秒。
uint32_t sticky_touch_geometry_retry_delay_ms(uint32_t retries_so_far);

// True when a background re-initialisation is due: the geometry is invalid, no contact has been
// read since start-up (so touch is genuinely dead, not just unusual hardware), nothing is being
// touched right now and the retry time has come.
// 是否应当后台重新初始化：分辨率无效、启动后从未读到触摸、当前无触摸、且已到重试时间。
bool sticky_touch_geometry_retry_due(bool geometry_valid, bool touch_seen, bool touching,
                                     uint32_t now_ms, uint32_t due_ms);

// Reports when repeated polling failures require the reference reset flow.
// 判断连续轮询失败是否需要重新执行示例复位流程。
bool sticky_touch_recovery_required(uint32_t consecutive_failures);

// Reports when one contact has remained active beyond normal UI interaction.
// 判断一次触摸是否已持续超过正常界面交互时间。
bool sticky_touch_contact_stuck(uint32_t started_at_ms, uint32_t now_ms);
