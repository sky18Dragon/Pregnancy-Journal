#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "chinese_input_page.h"

using sticky_ui::ChineseEditorResult;
using sticky_ui::ChineseInputAction;
using sticky_ui::ChineseTextEditor;

namespace {

void write_preview(Canvas &canvas, const char *path)
{
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << canvas.width() << " " << canvas.height() << "\n255\n";
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            const uint8_t value =
                canvas.pixel_at(x, y) == GrayLevel::Black ? 0U : 255U;
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
            output.write(reinterpret_cast<const char *>(&value), 1);
        }
    }
}

void verify_action_centers(int width, int height)
{
    const auto first = static_cast<uint8_t>(ChineseInputAction::Cancel);
    const auto last = static_cast<uint8_t>(ChineseInputAction::DeleteText);
    for (uint8_t value = first; value <= last; ++value) {
        const auto action = static_cast<ChineseInputAction>(value);
        const prototype_ui::Rect rect =
            sticky_ui::chinese_input_page_action_rect(action, width, height);
        assert(rect.width >= 48 && rect.height >= 48);
        assert(rect.x >= 0 && rect.y >= 0);
        assert(rect.x + rect.width <= width);
        assert(rect.y + rect.height <= height);
        assert(sticky_ui::chinese_input_page_action_at(
                   width, height, rect.x + rect.width / 2,
                   rect.y + rect.height / 2) == action);
        for (uint8_t other_value = static_cast<uint8_t>(value + 1U);
             other_value <= last; ++other_value) {
            const prototype_ui::Rect other =
                sticky_ui::chinese_input_page_action_rect(
                    static_cast<ChineseInputAction>(other_value), width,
                    height);
            const bool overlaps =
                rect.x < other.x + other.width &&
                rect.x + rect.width > other.x &&
                rect.y < other.y + other.height &&
                rect.y + rect.height > other.y;
            assert(!overlaps);
        }
    }
}

void verify_missing_glyph_fallback()
{
    std::vector<uint8_t> unsupported_buffer(800U * 480U / 2U);
    std::vector<uint8_t> marker_buffer(800U * 480U / 2U);
    Canvas unsupported_canvas(800U, 480U, unsupported_buffer.data(),
                              unsupported_buffer.size());
    Canvas marker_canvas(800U, 480U, marker_buffer.data(),
                         marker_buffer.size());
    unsupported_canvas.clear(GrayLevel::White);
    marker_canvas.clear(GrayLevel::White);
    sticky_ui::chinese_input_draw_text(unsupported_canvas, 8, 8, "A😀B", 24U,
                                       GrayLevel::Black);
    sticky_ui::chinese_input_draw_text(marker_canvas, 8, 8, "A□B", 24U,
                                       GrayLevel::Black);
    assert(unsupported_buffer == marker_buffer);
    assert(sticky_ui::chinese_input_text_width("A😀B", 24U) ==
           sticky_ui::chinese_input_text_width("A□B", 24U));
}

}  // namespace

int main()
{
    verify_missing_glyph_fallback();
    std::vector<uint8_t> framebuffer(800U * 480U / 2U);
    Canvas canvas(800U, 480U, framebuffer.data(), framebuffer.size());
    ChineseTextEditor editor;
    assert(editor.set_text("今天感觉很好，记得补充水分。缺字：😀") ==
           ChineseEditorResult::Ok);
    for (char letter : std::string("shi")) {
        assert(sticky_ui::chinese_input_page_apply_action(
                   editor, static_cast<ChineseInputAction>(
                               static_cast<uint8_t>(ChineseInputAction::LetterA) +
                               static_cast<uint8_t>(letter - 'a'))) ==
               ChineseEditorResult::Ok);
    }
    assert(editor.candidate_count() > 8U);
    sticky_ui::chinese_input_page_render(canvas, editor);
    write_preview(canvas, "/tmp/chinese_input_landscape.ppm");
    verify_action_centers(800, 480);

    canvas.set_rotation(CanvasRotation::Deg90Clockwise);
    sticky_ui::chinese_input_page_render(canvas, editor);
    write_preview(canvas, "/tmp/chinese_input_portrait.ppm");
    verify_action_centers(480, 800);

    const auto candidate = ChineseInputAction::Candidate0;
    assert(sticky_ui::chinese_input_page_apply_action(editor, candidate) ==
           ChineseEditorResult::Ok);
    assert(editor.pinyin_bytes() == 0U);
    assert(sticky_ui::chinese_input_text_width("孕期", 24U) > 24);
    return 0;
}
