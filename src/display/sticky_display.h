#pragma once

#include "esp_err.h"

class Canvas;

constexpr uint16_t kStickyDisplayWidth = 800;
constexpr uint16_t kStickyDisplayHeight = 480;

// Initializes SPI2, powers the SSD1677 panel, and creates the framebuffer.
// 初始化SPI2、打开SSD1677屏幕供电，并创建用于绘图的帧缓冲区。
esp_err_t sticky_display_init();

// Returns the drawing surface after sticky_display_init() succeeds.
// sticky_display_init()成功后，返回供页面绘图使用的画布；失败时返回空指针。
Canvas *sticky_display_canvas();

// Rotates the logical framebuffer 180 degrees and sends it to the panel.
// Canvas coordinates remain a normal 800 x 480, top-left-origin coordinate system.
// 将逻辑画面旋转180度后发送给屏幕；画布仍使用左上角为原点的800x480坐标系。
esp_err_t sticky_display_refresh();

// Refreshes black-and-white changes made to Canvas since the previous refresh.
// SSD1677 requires a full comparison frame to preserve unchanged pixels; that
// transfer and the physical 180-degree rotation are handled internally.
// 执行黑白局部刷新；驱动会自动准备SSD1677需要的完整对比帧并处理物理旋转。
esp_err_t sticky_display_refresh_partial();

// Refreshes the full screen with the panel's black-and-white waveform.
// Intended for pages that contain only Black and White canvas pixels.
// 使用黑白波形刷新整个屏幕，适合只包含黑色和白色的页面。
esp_err_t sticky_display_refresh_monochrome();

// Arms a fast first frame, with a periodic full refresh for panel cleanup.
// 为APP首帧安排快刷，并定期保留一次全刷清理屏幕。
bool sticky_display_prepare_app_transition_refresh();

// Cancels an armed transition refresh when app activation cannot continue.
// 当APP无法继续启动时，取消已安排的过渡刷新。
void sticky_display_cancel_app_transition_refresh();

// Clears the physical panel to white and clears the framebuffer.
// 将实体屏幕和内存中的画布同时清成白色。
esp_err_t sticky_display_clear();

// Puts the panel controller to sleep and removes display power. The last
// e-paper image remains visible without power.
// 让屏幕控制器休眠并关闭屏幕供电；电子纸会继续保留最后一帧画面。
esp_err_t sticky_display_sleep();
