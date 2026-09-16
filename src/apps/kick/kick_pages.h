#pragma once

#include <cstdint>

#include "kick.h"

class Canvas;

enum class KickPage : uint8_t {
  Today,
  Insights,
  Counter,
};

enum class KickAction : uint8_t {
  None,
  Back,
  Start,
  ShowInsights,
  Count,
  Undo,
  Finish,
};

void kick_page_render_today(Canvas &canvas, const KickDaySummary &today,
                            bool save_failed = false);
void kick_page_render_insights(Canvas &canvas, const KickDaySummary &today,
                               const KickTrendSummary &trend);
void kick_page_render_counter(Canvas &canvas, KickPeriod period,
                              uint16_t count, uint16_t elapsed_minutes);

KickAction kick_page_action_at(KickPage page, int width, int height, int x,
                               int y);
