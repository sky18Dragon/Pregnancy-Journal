#include "status_board_pages.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "pixel_asset.h"
#include "status_bunny_assets.h"

namespace {

constexpr int kScreenWidth = 800;
constexpr int kScreenHeight = 480;
constexpr int kPetBandY = 340;
constexpr int kPetBandHeight = kScreenHeight - kPetBandY;
constexpr int kPetCenterY = 410;
constexpr int kPetGroundX = 20;
constexpr int kPetGroundY = 456;
constexpr int kPetGroundWidth = kScreenWidth - kPetGroundX * 2;
constexpr int kPetGroundHeight = 2;

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

constexpr Rect kStatusRects[] = {
    {20, 120, 118, 220},
    {148, 120, 118, 220},
    {276, 120, 118, 220},
    {404, 120, 118, 220},
    {532, 120, 118, 220},
    {660, 120, 118, 220},
};

constexpr Rect kBackRect = {0, 0, 145, 120};
constexpr Rect kInputRect = {145, 20, 635, 140};
constexpr Rect kLetterRow1[] = {
    {15, 180, 70, 58}, {92, 180, 70, 58}, {169, 180, 70, 58},
    {246, 180, 70, 58}, {323, 180, 70, 58}, {400, 180, 70, 58},
    {477, 180, 70, 58}, {554, 180, 70, 58}, {631, 180, 70, 58},
    {708, 180, 70, 58},
};
constexpr Rect kLetterRow2[] = {
    {53, 248, 70, 58}, {131, 248, 70, 58}, {209, 248, 70, 58},
    {287, 248, 70, 58}, {365, 248, 70, 58}, {443, 248, 70, 58},
    {521, 248, 70, 58}, {599, 248, 70, 58}, {677, 248, 70, 58},
};
constexpr Rect kLetterRow3[] = {
    {131, 316, 70, 58}, {209, 316, 70, 58}, {287, 316, 70, 58},
    {365, 316, 70, 58}, {443, 316, 70, 58}, {521, 316, 70, 58},
    {599, 316, 70, 58},
};
constexpr Rect kToggleRect = {20, 400, 90, 60};
constexpr Rect kSpaceRect = {120, 400, 170, 60};
constexpr Rect kDeleteRect = {300, 400, 155, 60};
constexpr Rect kClearRect = {465, 400, 120, 60};
constexpr Rect kApplyRect = {595, 400, 185, 60};

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

constexpr StatusBoardAction kLetterRow1Actions[] = {
    StatusBoardAction::KeyQ, StatusBoardAction::KeyW,
    StatusBoardAction::KeyE, StatusBoardAction::KeyR,
    StatusBoardAction::KeyT, StatusBoardAction::KeyY,
    StatusBoardAction::KeyU, StatusBoardAction::KeyI,
    StatusBoardAction::KeyO, StatusBoardAction::KeyP,
};
constexpr StatusBoardAction kLetterRow2Actions[] = {
    StatusBoardAction::KeyA, StatusBoardAction::KeyS,
    StatusBoardAction::KeyD, StatusBoardAction::KeyF,
    StatusBoardAction::KeyG, StatusBoardAction::KeyH,
    StatusBoardAction::KeyJ, StatusBoardAction::KeyK,
    StatusBoardAction::KeyL,
};
constexpr StatusBoardAction kLetterRow3Actions[] = {
    StatusBoardAction::KeyZ, StatusBoardAction::KeyX,
    StatusBoardAction::KeyC, StatusBoardAction::KeyV,
    StatusBoardAction::KeyB, StatusBoardAction::KeyN,
    StatusBoardAction::KeyM,
};
constexpr StatusBoardAction kDigitActions[] = {
    StatusBoardAction::Digit1, StatusBoardAction::Digit2,
    StatusBoardAction::Digit3, StatusBoardAction::Digit4,
    StatusBoardAction::Digit5, StatusBoardAction::Digit6,
    StatusBoardAction::Digit7, StatusBoardAction::Digit8,
    StatusBoardAction::Digit9, StatusBoardAction::Digit0,
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

void draw_centered_text(Canvas &canvas,
                        int y,
                        const char *text,
                        int scale,
                        GrayLevel color)
{
    canvas.draw_text((kScreenWidth - text_width(text, scale)) / 2,
                     y,
                     text,
                     scale,
                     color);
}

void draw_double_rect(Canvas &canvas, const Rect &rect, GrayLevel color)
{
    canvas.draw_rect(rect.x, rect.y, rect.width, rect.height, color);
    canvas.draw_rect(
        rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2, color);
}

void draw_button(Canvas &canvas,
                 const Rect &rect,
                 const char *label,
                 bool filled,
                 int scale)
{
    const GrayLevel foreground =
        filled ? GrayLevel::White : GrayLevel::Black;
    if (filled) {
        canvas.fill_rect(
            rect.x, rect.y, rect.width, rect.height, GrayLevel::Black);
    } else {
        draw_double_rect(canvas, rect, GrayLevel::Black);
    }
    const int y = rect.y + (rect.height - 7 * scale) / 2;
    draw_centered_text_in_rect(
        canvas, rect, y, label, scale, foreground);
}

void draw_back_arrow(Canvas &canvas, GrayLevel color)
{
    // Draws one compact left arrow while the surrounding corner remains the
    // larger invisible touch target.
    // 绘制小巧的左箭头，箭头周围的左上角区域作为更大的隐形触摸范围。
    constexpr int kPointX = 24;
    constexpr int kCenterY = 35;
    constexpr int kHeadEndX = 37;
    constexpr int kShaftEndX = 62;
    constexpr int kHeadHalfHeight = 13;
    constexpr int kThickness = 2;

    for (int offset = 0; offset < kThickness; ++offset) {
        canvas.draw_line(kPointX,
                         kCenterY + offset,
                         kHeadEndX,
                         kCenterY - kHeadHalfHeight + offset,
                         color);
        canvas.draw_line(kPointX,
                         kCenterY + offset,
                         kHeadEndX,
                         kCenterY + kHeadHalfHeight + offset,
                         color);
    }
    canvas.fill_rect(kPointX,
                     kCenterY,
                     kShaftEndX - kPointX,
                     kThickness,
                     color);
}

StatusBunnyAssetId status_asset_id(StatusBoardStatus status)
{
    switch (status) {
    case StatusBoardStatus::Focusing:
        return StatusBunnyAssetId::Focusing;
    case StatusBoardStatus::InMeeting:
        return StatusBunnyAssetId::InMeeting;
    case StatusBoardStatus::Welcome:
        return StatusBunnyAssetId::Welcome;
    case StatusBoardStatus::OutForLunch:
        return StatusBunnyAssetId::OutForLunch;
    case StatusBoardStatus::OffDuty:
        return StatusBunnyAssetId::OffDuty;
    case StatusBoardStatus::Custom:
        return StatusBunnyAssetId::Custom;
    }
    return StatusBunnyAssetId::Custom;
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

void draw_status_label(Canvas &canvas,
                       const Rect &rect,
                       StatusBoardStatus status,
                       GrayLevel color)
{
    const int first_line_y = rect.y + rect.height - 62;
    const int second_line_y = first_line_y + 22;
    const int single_line_y = rect.y + rect.height - 48;
    switch (status) {
    case StatusBoardStatus::InMeeting:
        draw_centered_text_in_rect(
            canvas, rect, first_line_y, "IN A", 2, color);
        draw_centered_text_in_rect(
            canvas, rect, second_line_y, "MEETING", 2, color);
        break;
    case StatusBoardStatus::OutForLunch:
        draw_centered_text_in_rect(
            canvas, rect, first_line_y, "OUT FOR", 2, color);
        draw_centered_text_in_rect(
            canvas, rect, second_line_y, "LUNCH", 2, color);
        break;
    default:
        draw_centered_text_in_rect(
            canvas, rect, single_line_y, status_title(status), 2, color);
        break;
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

    pixel_asset_draw_centered(canvas,
                              rect.x + rect.width / 2,
                              rect.y + rect.height / 2 - 20,
                              status_bunny_asset(status_asset_id(status)),
                              1,
                              foreground);
    draw_status_label(canvas, rect, status, foreground);
}

void begin_landscape_page(Canvas &canvas, GrayLevel background)
{
    canvas.set_rotation(CanvasRotation::Deg0);
    canvas.clear(background);
}

int display_text_scale(const char *text, int maximum_scale)
{
    constexpr int kTextRegionWidth = 420;
    const int unscaled_width = text_width(text, 1);
    if (unscaled_width <= 0) {
        return 1;
    }
    return std::max(1,
                    std::min(maximum_scale,
                             kTextRegionWidth / unscaled_width));
}

void draw_display_title(Canvas &canvas,
                        StatusBoardStatus status,
                        const char *title)
{
    constexpr Rect kTextRegion = {20, 76, 440, 328};

    // Preset phrases use deliberate line breaks so both words and artwork can
    // fill the landscape page. Custom text scales to the same left region.
    // 预设短语通过固定换行铺满横屏左侧，自定义文字缩放到同一区域。
    switch (status) {
    case StatusBoardStatus::InMeeting:
        draw_centered_text_in_rect(
            canvas, kTextRegion, 150, "IN A", 8, GrayLevel::White);
        draw_centered_text_in_rect(
            canvas, kTextRegion, 235, "MEETING", 8, GrayLevel::White);
        return;
    case StatusBoardStatus::OutForLunch:
        draw_centered_text_in_rect(
            canvas, kTextRegion, 150, "OUT FOR", 8, GrayLevel::White);
        draw_centered_text_in_rect(
            canvas, kTextRegion, 235, "LUNCH", 8, GrayLevel::White);
        return;
    case StatusBoardStatus::OffDuty:
        draw_centered_text_in_rect(
            canvas, kTextRegion, 150, "OFF", 9, GrayLevel::White);
        draw_centered_text_in_rect(
            canvas, kTextRegion, 240, "DUTY", 9, GrayLevel::White);
        return;
    case StatusBoardStatus::Focusing:
    case StatusBoardStatus::Welcome:
    case StatusBoardStatus::Custom:
        break;
    }

    const int scale = display_text_scale(title, 8);
    const int y = 240 - 7 * scale / 2;
    draw_centered_text_in_rect(
        canvas, kTextRegion, y, title, scale, GrayLevel::White);
}

void draw_key(Canvas &canvas,
              const Rect &rect,
              const char *label,
              int scale = 4)
{
    draw_double_rect(canvas, rect, GrayLevel::Black);
    const int y = rect.y + (rect.height - 7 * scale) / 2;
    draw_centered_text_in_rect(
        canvas, rect, y, label, scale, GrayLevel::Black);
}

void draw_letter_keyboard(Canvas &canvas)
{
    constexpr const char *kRow1Labels[] = {
        "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
    };
    constexpr const char *kRow2Labels[] = {
        "A", "S", "D", "F", "G", "H", "J", "K", "L",
    };
    constexpr const char *kRow3Labels[] = {
        "Z", "X", "C", "V", "B", "N", "M",
    };

    for (size_t index = 0; index < 10U; ++index) {
        draw_key(canvas, kLetterRow1[index], kRow1Labels[index]);
    }
    for (size_t index = 0; index < 9U; ++index) {
        draw_key(canvas, kLetterRow2[index], kRow2Labels[index]);
    }
    for (size_t index = 0; index < 7U; ++index) {
        draw_key(canvas, kLetterRow3[index], kRow3Labels[index]);
    }
}

void draw_number_keyboard(Canvas &canvas)
{
    constexpr const char *kDigitLabels[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    };
    for (size_t index = 0; index < 10U; ++index) {
        Rect rect = kLetterRow1[index];
        rect.y = 250;
        rect.height = 86;
        draw_key(canvas, rect, kDigitLabels[index], 5);
    }
    draw_centered_text(canvas,
                       355,
                       "NUMBER KEYS",
                       2,
                       GrayLevel::Black);
}

StatusBoardAction action_in_rects(const Rect *rects,
                                  const StatusBoardAction *actions,
                                  size_t count,
                                  int x,
                                  int y)
{
    for (size_t index = 0; index < count; ++index) {
        if (rects[index].contains(x, y)) {
            return actions[index];
        }
    }
    return StatusBoardAction::None;
}

}  // namespace

void status_board_page_render_menu(Canvas &canvas,
                                   StatusBoardStatus selected_status)
{
    // Status Board uses the panel's native 800x480 landscape coordinates.
    // 状态牌使用电子纸原生的800x480横屏坐标。
    begin_landscape_page(canvas, GrayLevel::White);
    draw_centered_text(canvas,
                       28,
                       "CHOOSE STATUS",
                       4,
                       GrayLevel::Black);
    draw_centered_text(canvas,
                       68,
                       "SELECT ONE TO DISPLAY",
                       2,
                       GrayLevel::Black);

    for (size_t index = 0; index < 6U; ++index) {
        draw_status_button(canvas,
                           kStatusRects[index],
                           kStatuses[index],
                           kStatuses[index] == selected_status);
    }
}

void status_board_page_render_menu_pet(Canvas &canvas,
                                       PetAnimationPose pose,
                                       int center_x)
{
    // The card row ends at y=340, so each frame can replace the free band.
    // 状态卡片在y=340结束，因此每帧只需替换底部留白区。
    canvas.fill_rect(0,
                     kPetBandY,
                     kScreenWidth,
                     kPetBandHeight,
                     GrayLevel::White);
    canvas.fill_rect(kPetGroundX,
                     kPetGroundY,
                     kPetGroundWidth,
                     kPetGroundHeight,
                     GrayLevel::Black);
    pixel_asset_draw_centered(canvas,
                              center_x,
                              kPetCenterY,
                              pet_animation_asset(pose),
                              1,
                              GrayLevel::Black);
}

void status_board_page_render_display(Canvas &canvas,
                                      StatusBoardStatus selected_status,
                                      const char *custom_text)
{
    begin_landscape_page(canvas, GrayLevel::Black);

    draw_back_arrow(canvas, GrayLevel::White);

    const char *title = selected_status == StatusBoardStatus::Custom &&
                                custom_text != nullptr &&
                                custom_text[0] != '\0'
                            ? custom_text
                            : status_title(selected_status);
    draw_display_title(canvas, selected_status, title);
    pixel_asset_draw_centered(
        canvas,
        630,
        240,
        status_bunny_asset(status_asset_id(selected_status)),
        4,
        GrayLevel::White);
}

void status_board_page_render_custom_input(
    Canvas &canvas,
    const char *text,
    StatusBoardKeyboardMode keyboard_mode,
    bool input_error)
{
    begin_landscape_page(canvas, GrayLevel::White);
    draw_back_arrow(canvas, GrayLevel::Black);
    draw_double_rect(canvas, kInputRect, GrayLevel::Black);

    const char *safe_text = text == nullptr ? "" : text;
    char preview[24] = {};
    if (safe_text[0] == '\0') {
        std::snprintf(preview, sizeof(preview), "TYPE STATUS_");
    } else {
        std::snprintf(preview, sizeof(preview), "%s_", safe_text);
    }
    const int preview_scale = std::strlen(preview) <= 15U ? 5 : 4;
    draw_centered_text_in_rect(canvas,
                               kInputRect,
                               64,
                               preview,
                               preview_scale,
                               GrayLevel::Black);

    char count_text[16] = {};
    std::snprintf(count_text,
                  sizeof(count_text),
                  "%u / 20",
                  static_cast<unsigned>(std::strlen(safe_text)));
    canvas.draw_text(kInputRect.x + kInputRect.width -
                         text_width(count_text, 2) - 14,
                     132,
                     count_text,
                     2,
                     GrayLevel::Black);
    if (input_error) {
        canvas.draw_text(kInputRect.x + 14,
                         132,
                         "TYPE AT LEAST 1 CHARACTER",
                         1,
                         GrayLevel::Black);
    }

    if (keyboard_mode == StatusBoardKeyboardMode::Letters) {
        draw_letter_keyboard(canvas);
    } else {
        draw_number_keyboard(canvas);
    }

    draw_button(canvas,
                kToggleRect,
                keyboard_mode == StatusBoardKeyboardMode::Letters
                    ? "123"
                    : "ABC",
                false,
                2);
    draw_button(canvas, kSpaceRect, "SPACE", false, 3);
    draw_button(canvas, kDeleteRect, "DELETE", false, 2);
    draw_button(canvas, kClearRect, "CLEAR", false, 2);
    draw_button(canvas, kApplyRect, "APPLY", true, 3);
}

StatusBoardAction status_board_page_action_at(
    StatusBoardPage page,
    StatusBoardKeyboardMode keyboard_mode,
    int x,
    int y)
{
    if (x < 0 || y < 0 || x >= kScreenWidth || y >= kScreenHeight) {
        return StatusBoardAction::None;
    }

    if (page == StatusBoardPage::Menu) {
        return action_in_rects(
            kStatusRects, kActions, 6U, x, y);
    }

    if (kBackRect.contains(x, y)) {
        return StatusBoardAction::Back;
    }
    if (page == StatusBoardPage::Display) {
        return StatusBoardAction::None;
    }

    if (keyboard_mode == StatusBoardKeyboardMode::Letters) {
        StatusBoardAction action = action_in_rects(
            kLetterRow1, kLetterRow1Actions, 10U, x, y);
        if (action != StatusBoardAction::None) {
            return action;
        }
        action = action_in_rects(
            kLetterRow2, kLetterRow2Actions, 9U, x, y);
        if (action != StatusBoardAction::None) {
            return action;
        }
        action = action_in_rects(
            kLetterRow3, kLetterRow3Actions, 7U, x, y);
        if (action != StatusBoardAction::None) {
            return action;
        }
    } else {
        Rect digit_rects[10] = {};
        for (size_t index = 0; index < 10U; ++index) {
            digit_rects[index] = kLetterRow1[index];
            digit_rects[index].y = 250;
            digit_rects[index].height = 86;
        }
        const StatusBoardAction action = action_in_rects(
            digit_rects, kDigitActions, 10U, x, y);
        if (action != StatusBoardAction::None) {
            return action;
        }
    }

    if (kToggleRect.contains(x, y)) {
        return StatusBoardAction::ToggleKeyboard;
    }
    if (kSpaceRect.contains(x, y)) {
        return StatusBoardAction::Space;
    }
    if (kDeleteRect.contains(x, y)) {
        return StatusBoardAction::Delete;
    }
    if (kClearRect.contains(x, y)) {
        return StatusBoardAction::Clear;
    }
    if (kApplyRect.contains(x, y)) {
        return StatusBoardAction::Apply;
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

bool status_board_action_character(StatusBoardAction action, char &character)
{
    if (action >= StatusBoardAction::KeyA &&
        action <= StatusBoardAction::KeyZ) {
        character = static_cast<char>(
            'A' + static_cast<int>(action) -
            static_cast<int>(StatusBoardAction::KeyA));
        return true;
    }
    if (action >= StatusBoardAction::Digit0 &&
        action <= StatusBoardAction::Digit9) {
        character = static_cast<char>(
            '0' + static_cast<int>(action) -
            static_cast<int>(StatusBoardAction::Digit0));
        return true;
    }
    return false;
}

bool status_board_action_can_batch(StatusBoardAction action)
{
    if ((action >= StatusBoardAction::KeyA &&
         action <= StatusBoardAction::KeyZ) ||
        (action >= StatusBoardAction::Digit0 &&
         action <= StatusBoardAction::Digit9)) {
        return true;
    }
    return action == StatusBoardAction::Space ||
           action == StatusBoardAction::Delete ||
           action == StatusBoardAction::Clear;
}

const char *status_board_page_name(StatusBoardPage page)
{
    switch (page) {
    case StatusBoardPage::Menu:
        return "menu";
    case StatusBoardPage::Display:
        return "display";
    case StatusBoardPage::CustomInput:
        return "custom_input";
    }
    return "unknown";
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
    if (action >= StatusBoardAction::KeyA &&
        action <= StatusBoardAction::KeyZ) {
        return "key_letter";
    }
    if (action >= StatusBoardAction::Digit0 &&
        action <= StatusBoardAction::Digit9) {
        return "key_digit";
    }

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
    case StatusBoardAction::Back:
        return "back";
    case StatusBoardAction::ToggleKeyboard:
        return "toggle_keyboard";
    case StatusBoardAction::Space:
        return "space";
    case StatusBoardAction::Delete:
        return "delete";
    case StatusBoardAction::Clear:
        return "clear";
    case StatusBoardAction::Apply:
        return "apply";
    case StatusBoardAction::None:
    default:
        return "none";
    }
}
