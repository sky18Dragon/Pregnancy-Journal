#pragma once

#include "esp_err.h"

class Canvas;

// Starts the standalone portrait Book of Answers application.
// 启动独立运行的竖屏答案书APP。
esp_err_t book_of_answers_app_start(Canvas &canvas);

esp_err_t book_of_answers_app_pause();
esp_err_t book_of_answers_app_resume();
