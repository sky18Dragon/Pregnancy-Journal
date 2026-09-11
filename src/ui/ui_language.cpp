#include "ui_language.h"

#include <atomic>
#include <cstring>

#include "font.h"

namespace {

struct Translation { const char *english; const char *chinese; };
std::atomic<UiLanguage> s_language{UiLanguage::English};

constexpr Translation kTranslations[] = {
    {"CHOOSE AN APP", "选择应用"},
    {"TAP A CARD  /  SWIPE DOWN TO CLOSE", "点击卡片 / 下滑关闭"},
    {"Home", "主页"},
    {"Settings", "设置"},
    {"Baby Week", "孕周"},
    {"STICKY CORE", "STICKY 核心"},
    {"FRAMEWORK", "通用框架"},
    {"RTC  READY", "时钟  正常"},
    {"RTC  UNAVAILABLE", "时钟  不可用"},
    {"RTC READY", "时钟 正常"},
    {"RTC UNAVAILABLE", "时钟 不可用"},
    {"SWIPE UP FOR APPS", "向上滑动打开应用"},
    {"SETTINGS", "设置"},
    {"DEVICE AND FRAMEWORK CONTROLS", "设备与框架控制"},
    {"LANGUAGE: ENGLISH", "语言：英文"},
    {"SET DEVICE TIME", "设置设备时间"},
    {"DISPLAY CLEAN REFRESH", "屏幕清洁全刷"},
    {"RETURN HOME", "返回主页"},
    {"YYYY / MM / DD / HH / MM", "年 / 月 / 日 / 时 / 分"},
    {"ENTER 12 DIGITS, THEN SAVE", "输入12位数字后保存"},
    {"INVALID DATE OR RTC WRITE FAILED", "日期无效或时钟写入失败"},
    {"BACK", "返回"},
    {"SAVE", "保存"},
    {"DEL", "删除"},
    {"CANCEL", "取消"},
    {"NEXT", "下一步"},
    {"EDIT", "修改"},
    {"SET DUE DATE", "设置预产期"},
    {"YEAR / MONTH / DAY / HOUR / MINUTE", "年 / 月 / 日 / 时 / 分"},
    {"USE YOUR CONFIRMED ESTIMATED DUE DATE", "请输入确认后的预产期"},
    {"CHECK THE DATE AND TRY AGAIN", "日期有误，请重新输入"},
    {"TAP A NUMBER TO REPLACE THE VALUE", "点击数字即可替换当前值"},
    {"SAVED ONLY ON THIS DEVICE", "数据仅保存在本设备"},
    {"CURRENT DEVELOPMENT STAGE", "当前发育阶段"},
    {"40-WEEK DEVELOPMENT PROGRESS", "40 周孕期进度"},
    {"FIRST TRIMESTER", "孕早期"},
    {"SECOND TRIMESTER", "孕中期"},
    {"THIRD TRIMESTER", "孕晚期"},
    {"UNKNOWN", "未知"},
};

}  // namespace

UiLanguage ui_language_get()
{
    return s_language.load(std::memory_order_acquire);
}

void ui_language_set(UiLanguage language)
{
    s_language.store(language, std::memory_order_release);
}

bool ui_language_is_chinese()
{
    return ui_language_get() == UiLanguage::ChineseSimplified;
}

const char *ui_text(const char *english)
{
    if (english == nullptr || !ui_language_is_chinese()) return english;
    for (const Translation &entry : kTranslations) {
        if (std::strcmp(entry.english, english) == 0) return entry.chinese;
    }
    return english;
}

int ui_text_width(const char *english, uint8_t scale)
{
    return font_text_width(ui_text(english), scale);
}
