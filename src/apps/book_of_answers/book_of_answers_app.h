#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

// Starts the standalone portrait Book of Answers application.
// 启动独立运行的竖屏答案书APP。
esp_err_t book_of_answers_app_start(Canvas &canvas);

// Prepares whether the next activation must wait for a fresh shake gesture.
// 设置下一次进入答案书时是否需要等待一次全新的摇晃动作。
void book_of_answers_app_prepare_entry(bool launcher_shake_consumed);

// Suspends input and page animation while preserving the current stable page.
// 暂停输入和页面动画，同时保留当前稳定页面。
esp_err_t book_of_answers_app_pause();

// Restores the preserved page and rearms the appropriate shake-input gate.
// 恢复已保留页面，并重新配置对应的摇晃输入门控。
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
