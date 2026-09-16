#pragma once

class Canvas;

// Renders the final monochrome typography/geometry calibration page. Layout
// follows the current logical canvas orientation.
void ui_render_font_test_screen(Canvas &canvas);
