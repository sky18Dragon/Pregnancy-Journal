#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

// Starts the standalone portrait Book of Answers application.
// 启动独立运行的竖屏答案书APP。
esp_err_t book_of_answers_app_start(Canvas &canvas);

esp_err_t book_of_answers_app_pause();
esp_err_t book_of_answers_app_resume();

// Reports whether no shake or reveal animation is currently in progress.
// 返回当前是否没有正在进行的摇晃或揭晓动画。
bool book_of_answers_app_power_sleep_allowed();

// Pauses the app and saves its stable answer page in RTC memory.
// 暂停APP，并把稳定的答案页面保存到RTC内存。
esp_err_t book_of_answers_app_prepare_power_sleep();

// Returns the battery idle timeout for stable pages; zero blocks it.
// 返回稳定页面的电池空闲休眠时间；零表示暂缓自动休眠。
uint32_t book_of_answers_app_power_sleep_timeout_ms();
