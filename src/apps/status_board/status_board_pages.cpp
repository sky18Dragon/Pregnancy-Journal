#include "status_board_pages.h"

#include <cstddef>
#include <cstring>

#include "canvas.h"

namespace {

constexpr int kScreenWidth = 800;
constexpr int kScreenHeight = 480;

struct Rect {
    int x;
    int y;
    int width;
    int height;

    bool contains(int point_x, int point_y) const
    {
        return point_x >= x && point_y >= y &&
               point_x < x + width && point_y < y + height;
    }
};

constexpr Rect kHeaderRect = {20, 20, 760, 220};
constexpr Rect kStatusRects[] = {
    {20, 285, 118, 160},
    {148, 285, 118, 160},
    {276, 285, 118, 160},
    {404, 285, 118, 160},
    {532, 285, 118, 160},
    {660, 285, 118, 160},
};

constexpr StatusBoardStatus kStatuses[] = {
    StatusBoardStatus::Focusing,
    StatusBoardStatus::InMeeting,
    StatusBoardStatus::Welcome,
    StatusBoardStatus::OutForLunch,
    StatusBoardStatus::OffDuty,
    StatusBoardStatus::Custom,
};

constexpr StatusBoardAction kActions[] = {
    StatusBoardAction::SelectFocusing,
    StatusBoardAction::SelectInMeeting,
    StatusBoardAction::SelectWelcome,
    StatusBoardAction::SelectOutForLunch,
    StatusBoardAction::SelectOffDuty,
    StatusBoardAction::SelectCustom,
};

int text_width(const char *text, int scale)
{
    const size_t length = std::strlen(text);
    if (length == 0U) {
        return 0;
    }
    return (static_cast<int>(length) * 6 - 1) * scale;
}

void draw_centered_text_in_rect(Canvas &canvas,
                                const Rect &rect,
                                int y,
                                const char *text,
                                int scale,
                                GrayLevel color)
{
    const int x = rect.x + (rect.width - text_width(text, scale)) / 2;
    canvas.draw_text(x, y, text, scale, color);
}

void draw_double_rect(Canvas &canvas, const Rect &rect, GrayLevel color)
{
    canvas.draw_rect(rect.x, rect.y, rect.width, rect.height, color);
    canvas.draw_rect(
        rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, color);
}

void draw_sun_icon(Canvas &canvas,
                   int center_x,
                   int center_y,
                   int radius,
                   GrayLevel color)
{
    canvas.draw_circle(center_x, center_y, radius, color);
    const int inner = radius + 5;
    const int outer = radius + 13;
    canvas.draw_line(center_x, center_y - inner,
                     center_x, center_y - outer, color);
    canvas.draw_line(center_x, center_y + inner,
                     center_x, center_y + outer, color);
    canvas.draw_line(center_x - inner, center_y,
                     center_x - outer, center_y, color);
    canvas.draw_line(center_x + inner, center_y,
                     center_x + outer, center_y, color);
    canvas.draw_line(center_x - inner + 2, center_y - inner + 2,
                     center_x - outer + 3, center_y - outer + 3, color);
    canvas.draw_line(center_x + inner - 2, center_y - inner + 2,
                     center_x + outer - 3, center_y - outer + 3, color);
    canvas.draw_line(center_x - inner + 2, center_y + inner - 2,
                     center_x - outer + 3, center_y + outer - 3, color);
    canvas.draw_line(center_x + inner - 2, center_y + inner - 2,
                     center_x + outer - 3, center_y + outer - 3, color);
}

void draw_meeting_icon(Canvas &canvas,
                       int center_x,
                       int center_y,
                       int size,
                       GrayLevel color)
{
    const int head_radius = size / 6;
    const int offset = size / 4;
    canvas.fill_circle(center_x - offset, center_y - size / 5,
                       head_radius, color);
    canvas.fill_circle(center_x + offset, center_y - size / 5,
                       head_radius, color);
    canvas.fill_rect(center_x - offset - head_radius,
                     center_y + 1,
                     head_radius * 2,
                     size / 3,
                     color);
    canvas.fill_rect(center_x + offset - head_radius,
                     center_y + 1,
                     head_radius * 2,
                     size / 3,
                     color);
}

void draw_message_icon(Canvas &canvas,
                       int center_x,
                       int center_y,
                       int size,
                       GrayLevel color)
{
    const int x = center_x - size / 2;
    const int y = center_y - size / 3;
    canvas.draw_rect(x, y, size, size * 2 / 3, color);
    canvas.draw_line(x + size / 4, y + size * 2 / 3,
                     x + size / 5, y + size * 5 / 6, color);
    canvas.draw_line(x + size / 5, y + size * 5 / 6,
                     x + size / 2, y + size * 2 / 3, color);
    canvas.fill_circle(center_x - size / 5, center_y, 2, color);
    canvas.fill_circle(center_x, center_y, 2, color);
    canvas.fill_circle(center_x + size / 5, center_y, 2, color);
}

void draw_lunch_icon(Canvas &canvas,
                     int center_x,
                     int center_y,
                     int size,
                     GrayLevel color)
{
    const int top = center_y - size / 2;
    const int bottom = center_y + size / 2;
    const int fork_x = center_x - size / 4;
    const int knife_x = center_x + size / 4;
    canvas.draw_line(fork_x, top, fork_x, bottom, color);
    canvas.draw_line(fork_x - 5, top, fork_x - 5, center_y - size / 8, color);
    canvas.draw_line(fork_x + 5, top, fork_x + 5, center_y - size / 8, color);
    canvas.draw_line(fork_x - 5, center_y - size / 8,
                     fork_x + 5, center_y - size / 8, color);
    canvas.fill_rect(knife_x - 3, top, 7, size, color);
}

void draw_house_icon(Canvas &canvas,
                     int center_x,
                     int center_y,
                     int size,
                     GrayLevel color)
{
    const int half = size / 2;
    canvas.draw_line(center_x - half, center_y - 4,
                     center_x, center_y - half, color);
    canvas.draw_line(center_x, center_y - half,
                     center_x + half, center_y - 4, color);
    canvas.draw_rect(center_x - half + 5,
                     center_y - 4,
                     size - 10,
                     half + 8,
                     color);
    canvas.draw_rect(center_x - 6, center_y + 8, 12, half - 5, color);
}

void draw_custom_icon(Canvas &canvas,
                      int center_x,
                      int center_y,
                      int size,
                      GrayLevel color)
{
    const int radius = size / 2;
    canvas.draw_circle(center_x, center_y, radius, color);
    canvas.draw_line(center_x - radius / 2, center_y,
                     center_x + radius / 2, center_y, color);
    canvas.draw_line(center_x, center_y - radius / 2,
                     center_x, center_y + radius / 2, color);
}

void draw_meeting_scene(Canvas &canvas)
{
    // Draws two pixel characters, a table, and a laptop as one scene.
    // 将两个像素人物、桌子和电脑绘制成一幅完整的会议场景。
    constexpr int kLeftX = 612;
    constexpr int kRightX = 720;
    constexpr int kHeadY = 105;
    constexpr int kCenters[] = {kLeftX, kRightX};

    for (const int center_x : kCenters) {
        canvas.fill_circle(center_x, kHeadY, 28, GrayLevel::White);
        canvas.fill_rect(center_x - 21, 55, 15, 36, GrayLevel::White);
        canvas.fill_rect(center_x + 6, 55, 15, 36, GrayLevel::White);
        canvas.fill_circle(center_x - 14, 55, 7, GrayLevel::White);
        canvas.fill_circle(center_x + 14, 55, 7, GrayLevel::White);
        canvas.fill_rect(center_x - 23, 132, 46, 37, GrayLevel::White);
        canvas.fill_circle(center_x - 10, 103, 3, GrayLevel::Black);
        canvas.fill_circle(center_x + 10, 103, 3, GrayLevel::Black);
        canvas.draw_line(center_x - 4, 116,
                         center_x, 120, GrayLevel::Black);
        canvas.draw_line(center_x, 120,
                         center_x + 4, 116, GrayLevel::Black);
    }

    canvas.fill_rect(575, 168, 185, 5, GrayLevel::White);
    canvas.fill_rect(583, 173, 5, 32, GrayLevel::White);
    canvas.fill_rect(747, 173, 5, 32, GrayLevel::White);
    canvas.draw_rect(661, 141, 42, 27, GrayLevel::White);
    canvas.draw_line(661, 168, 710, 168, GrayLevel::White);

    canvas.draw_rect(650, 57, 48, 29, GrayLevel::White);
    canvas.draw_line(664, 86, 657, 94, GrayLevel::White);
    canvas.fill_circle(662, 72, 2, GrayLevel::White);
    canvas.fill_circle(674, 72, 2, GrayLevel::White);
    canvas.fill_circle(686, 72, 2, GrayLevel::White);
}

void draw_status_icon(Canvas &canvas,
                      StatusBoardStatus status,
                      int center_x,
                      int center_y,
                      int size,
                      GrayLevel color)
{
    switch (status) {
    case StatusBoardStatus::Focusing:
        draw_sun_icon(canvas, center_x, center_y, size / 3, color);
        break;
    case StatusBoardStatus::InMeeting:
        draw_meeting_icon(canvas, center_x, center_y, size, color);
        break;
    case StatusBoardStatus::Welcome:
        draw_message_icon(canvas, center_x, center_y, size, color);
        break;
    case StatusBoardStatus::OutForLunch:
        draw_lunch_icon(canvas, center_x, center_y, size, color);
        break;
    case StatusBoardStatus::OffDuty:
        draw_house_icon(canvas, center_x, center_y, size, color);
        break;
    case StatusBoardStatus::Custom:
        draw_custom_icon(canvas, center_x, center_y, size, color);
        break;
    }
}

const char *status_title(StatusBoardStatus status)
{
    switch (status) {
    case StatusBoardStatus::Focusing:
        return "FOCUSING";
    case StatusBoardStatus::InMeeting:
        return "IN A MEETING";
    case StatusBoardStatus::Welcome:
        return "WELCOME";
    case StatusBoardStatus::OutForLunch:
        return "OUT FOR LUNCH";
    case StatusBoardStatus::OffDuty:
        return "OFF DUTY";
    case StatusBoardStatus::Custom:
        return "CUSTOM";
    }
    return "UNKNOWN";
}

const char *status_detail(StatusBoardStatus status)
{
    switch (status) {
    case StatusBoardStatus::Focusing:
        return "UNTIL 14:30";
    case StatusBoardStatus::InMeeting:
        return "BACK AT 15:00";
    case StatusBoardStatus::Welcome:
        return "COME IN";
    case StatusBoardStatus::OutForLunch:
        return "BACK AT 13:30";
    case StatusBoardStatus::OffDuty:
        return "BACK TOMORROW";
    case StatusBoardStatus::Custom:
        return "SET IN MOBILE APP";
    }
    return "";
}

int title_scale(StatusBoardStatus status)
{
    switch (status) {
    case StatusBoardStatus::OutForLunch:
        return 4;
    case StatusBoardStatus::InMeeting:
        return 6;
    case StatusBoardStatus::Custom:
        return 8;
    case StatusBoardStatus::Focusing:
    case StatusBoardStatus::Welcome:
    case StatusBoardStatus::OffDuty:
    default:
        return 7;
    }
}

void draw_status_label(Canvas &canvas,
                       const Rect &rect,
                       StatusBoardStatus status,
                       GrayLevel color)
{
    switch (status) {
    case StatusBoardStatus::InMeeting:
        draw_centered_text_in_rect(canvas, rect, 380, "IN A", 2, color);
        draw_centered_text_in_rect(canvas, rect, 402, "MEETING", 2, color);
        break;
    case StatusBoardStatus::OutForLunch:
        draw_centered_text_in_rect(canvas, rect, 380, "OUT FOR", 2, color);
        draw_centered_text_in_rect(canvas, rect, 402, "LUNCH", 2, color);
        break;
    default:
        draw_centered_text_in_rect(
            canvas, rect, 392, status_title(status), 2, color);
        break;
    }
}

void draw_header(Canvas &canvas, StatusBoardStatus selected_status)
{
    canvas.fill_rect(kHeaderRect.x,
                     kHeaderRect.y,
                     kHeaderRect.width,
                     kHeaderRect.height,
                     GrayLevel::Black);
    canvas.draw_text(50, 50, "CURRENT STATUS", 3, GrayLevel::White);
    canvas.draw_text(50,
                     90,
                     status_title(selected_status),
                     title_scale(selected_status),
                     GrayLevel::White);
    canvas.draw_text(50,
                     184,
                     status_detail(selected_status),
                     3,
                     GrayLevel::White);

    if (selected_status == StatusBoardStatus::InMeeting) {
        draw_meeting_scene(canvas);
    } else {
        draw_status_icon(canvas,
                         selected_status,
                         675,
                         128,
                         105,
                         GrayLevel::White);
    }
}

void draw_status_button(Canvas &canvas,
                        const Rect &rect,
                        StatusBoardStatus status,
                        bool selected)
{
    const GrayLevel foreground =
        selected ? GrayLevel::White : GrayLevel::Black;
    if (selected) {
        canvas.fill_rect(rect.x,
                         rect.y,
                         rect.width,
                         rect.height,
                         GrayLevel::Black);
    } else {
        draw_double_rect(canvas, rect, GrayLevel::Black);
    }

    draw_status_icon(canvas,
                     status,
                     rect.x + rect.width / 2,
                     338,
                     42,
                     foreground);
    draw_status_label(canvas, rect, status, foreground);
}

}  // namespace

void status_board_page_render(Canvas &canvas, StatusBoardStatus selected_status)
{
    // Status Board is a native landscape app on the 800x480 panel.
    // 状态牌直接使用电子纸原生800x480横屏坐标。
    canvas.set_rotation(CanvasRotation::Deg0);
    canvas.clear(GrayLevel::White);

    draw_header(canvas, selected_status);
    canvas.draw_text(35, 252, "CHOOSE STATUS", 3, GrayLevel::Black);

    for (size_t index = 0; index < 6U; ++index) {
        draw_status_button(canvas,
                           kStatusRects[index],
                           kStatuses[index],
                           kStatuses[index] == selected_status);
    }
}

StatusBoardAction status_board_page_action_at(int x, int y)
{
    if (x < 0 || y < 0 || x >= kScreenWidth || y >= kScreenHeight) {
        return StatusBoardAction::None;
    }

    for (size_t index = 0; index < 6U; ++index) {
        if (kStatusRects[index].contains(x, y)) {
            return kActions[index];
        }
    }
    return StatusBoardAction::None;
}

bool status_board_action_status(StatusBoardAction action,
                                StatusBoardStatus &status)
{
    switch (action) {
    case StatusBoardAction::SelectFocusing:
        status = StatusBoardStatus::Focusing;
        return true;
    case StatusBoardAction::SelectInMeeting:
        status = StatusBoardStatus::InMeeting;
        return true;
    case StatusBoardAction::SelectWelcome:
        status = StatusBoardStatus::Welcome;
        return true;
    case StatusBoardAction::SelectOutForLunch:
        status = StatusBoardStatus::OutForLunch;
        return true;
    case StatusBoardAction::SelectOffDuty:
        status = StatusBoardStatus::OffDuty;
        return true;
    case StatusBoardAction::SelectCustom:
        status = StatusBoardStatus::Custom;
        return true;
    case StatusBoardAction::None:
    default:
        return false;
    }
}

const char *status_board_status_name(StatusBoardStatus status)
{
    switch (status) {
    case StatusBoardStatus::Focusing:
        return "focusing";
    case StatusBoardStatus::InMeeting:
        return "in_meeting";
    case StatusBoardStatus::Welcome:
        return "welcome";
    case StatusBoardStatus::OutForLunch:
        return "out_for_lunch";
    case StatusBoardStatus::OffDuty:
        return "off_duty";
    case StatusBoardStatus::Custom:
        return "custom";
    }
    return "unknown";
}

const char *status_board_action_name(StatusBoardAction action)
{
    switch (action) {
    case StatusBoardAction::SelectFocusing:
        return "select_focusing";
    case StatusBoardAction::SelectInMeeting:
        return "select_in_meeting";
    case StatusBoardAction::SelectWelcome:
        return "select_welcome";
    case StatusBoardAction::SelectOutForLunch:
        return "select_out_for_lunch";
    case StatusBoardAction::SelectOffDuty:
        return "select_off_duty";
    case StatusBoardAction::SelectCustom:
        return "select_custom";
    case StatusBoardAction::None:
    default:
        return "none";
    }
}
