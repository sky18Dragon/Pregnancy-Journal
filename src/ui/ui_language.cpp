#include "ui_language.h"

#include <atomic>
#include <cstring>

#include "font.h"

namespace {

struct Translation {
  const char *english;
  const char *chinese;
};
std::atomic<UiLanguage> s_language{UiLanguage::English};

constexpr Translation kTranslations[] = {
    {"CHOOSE AN APP", "选择应用"},
    {"YOUR PREGNANCY AT A GLANCE", "孕期状态一目了然"},
    {"TAP A CARD  /  SWIPE DOWN TO CLOSE", "点击卡片 / 下滑关闭"},
    {"Settings", "设置"},
    {"Checkups", "产检"},
    {"Baby Week", "孕周"},
    {"WEEK + DUE DATE", "孕周 + 预产期"},
    {"HOME DASHBOARD", "主页"},
    {"Weight", "体重"},
    {"Kicks", "胎动"},
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
    {"RETURN TO BABY WEEK", "返回孕周"},
    {"PREGNANCY SETTINGS", "孕期设置"},
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
    {"PREGNANCY JOURNAL", "孕期手帐"},
    {"COMPLETE PREGNANCY SETUP TO BEGIN", "完成孕期设置后开始使用"},
    {"OPEN APPS AND CHOOSE PREGNANCY", "打开应用并选择孕期"},
    {"PREGNANCY SETUP", "孕期设置"},
    {"CHOOSE THE DATE YOU KNOW", "选择你已知的日期"},
    {"ESTIMATED DUE DATE", "预产期"},
    {"LAST MENSTRUAL PERIOD", "末次月经"},
    {"A CLINICIAN-CONFIRMED DUE DATE IS PREFERRED",
     "优先使用医护人员确认的预产期"},
    {"SET LAST MENSTRUAL PERIOD", "设置末次月经日期"},
    {"USE THE FIRST DAY OF YOUR LAST PERIOD", "请输入末次月经第一天"},
    {"OVERVIEW", "概览"},
    {"BABY", "宝宝"},
    {"MOM", "妈妈"},
    {"THIS WEEK", "本周变化"},
    {"COMMON CHANGES", "常见变化"},
    {"GENTLE REMINDER", "温和提醒"},
    {"INFORMATION ONLY - CONTACT YOUR CARE TEAM IF UNWELL",
     "内容仅供参考，如有不适请咨询专业医护人员"},
    {"TODAY", "今天"},
    {"NEXT REMINDER", "下一个提醒"},
    {"TODAY'S NOTE", "今日提示"},
    {"NO REMINDERS TODAY", "今天暂无提醒"},
    {"Reminders", "提醒"},
    {"REMINDERS", "提醒"},
    {"UPCOMING", "即将到来"},
    {"NO REMINDERS", "暂无提醒"},
    {"TAP ADD TO CREATE ONE", "点击添加创建提醒"},
    {"ADD", "添加"},
    {"COMPLETE", "完成"},
    {"DELETE", "删除"},
    {"ADD REMINDER", "添加提醒"},
    {"ALL REMINDERS ON", "全部提醒已开启"},
    {"ALL REMINDERS OFF", "全部提醒已关闭"},
    {"GENERAL", "一般"},
    {"SUPPLEMENT", "补充剂"},
    {"CHECKUP", "产检"},
    {"WATER", "喝水"},
    {"ACTIVITY", "活动"},
    {"REST", "休息"},
    {"- HOUR", "- 小时"},
    {"+ HOUR", "+ 小时"},
    {"CHECKUPS", "产检"},
    {"YOUR APPOINTMENTS", "你的预约"},
    {"NO CHECKUPS ADDED", "暂无产检安排"},
    {"REFERENCE PLANS VARY BY CARE TEAM", "具体安排请遵循医护团队建议"},
    {"ADD CHECKUP", "添加产检"},
    {"CONFIRM THE DATE WITH YOUR CARE TEAM", "请与医护团队确认日期"},
    {"WEIGHT TRACKER", "体重记录"},
    {"PREGNANCY WEIGHT TREND", "孕期体重趋势"},
    {"NO WEIGHT RECORDS", "暂无体重记录"},
    {"CHECK WEIGHT TREND", "建议确认体重趋势"},
    {"ADD WEIGHT", "记录体重"},
    {"TODAY'S WEIGHT", "今日体重"},
    {"UNIT KG", "单位 KG"},
    {"UNIT LB", "单位 LB"},
    {"ADD TODAY", "记录今日体重"},
    {"-  HEIGHT  +", "-  身高  +"},
    {"REMOVE LATEST", "删除最近记录"},
    {"KICK COUNTER", "数胎动"},
    {"TODAY'S THREE CHECK-INS", "今日三次定时记录"},
    {"MORNING", "早上"},
    {"AFTERNOON", "下午"},
    {"EVENING", "晚上"},
    {"NO KICK SESSIONS TODAY", "今天还没有胎动记录"},
    {"CONTACT YOUR CARE TEAM", "请尽快联系专业医护人员"},
    {"PATTERN LOOKS STEADY", "今日胎动趋势平稳"},
    {"COMPLETE THREE CHECK-INS", "请完成早中晚三次记录"},
    {"TAP FOR EACH KICK", "每次胎动点击一下"},
    {"START SESSION", "开始计数"},
    {"RESET TODAY", "清除今日记录"},
};

} // namespace

UiLanguage ui_language_get() {
  return s_language.load(std::memory_order_acquire);
}

void ui_language_set(UiLanguage language) {
  s_language.store(language, std::memory_order_release);
}

bool ui_language_is_chinese() {
  return ui_language_get() == UiLanguage::ChineseSimplified;
}

const char *ui_text(const char *english) {
  if (english == nullptr || !ui_language_is_chinese())
    return english;
  for (const Translation &entry : kTranslations) {
    if (std::strcmp(entry.english, english) == 0)
      return entry.chinese;
  }
  return english;
}

int ui_text_width(const char *english, uint8_t scale) {
  return font_text_width(ui_text(english), scale);
}
