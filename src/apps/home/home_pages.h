#pragma once

#include <cstdint>

class Canvas;

struct HomePageData {
    bool rtc_valid = false;
    uint16_t year = 0U;
    uint8_t month = 0U;
    uint8_t day = 0U;
    uint8_t hour = 0U;
    uint8_t minute = 0U;
    bool battery_valid = false;
    int battery_percent = 0;
};

// Renders the generic framework Home page without accessing hardware.
void home_page_render(Canvas &canvas, const HomePageData &data);

