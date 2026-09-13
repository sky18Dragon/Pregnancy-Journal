#include "content_service.h"

#include <algorithm>

namespace {

struct ContentBand {
    uint8_t first_week;
    uint8_t last_week;
    const char *size;
    const char *weight;
    const char *baby;
    const char *mother;
    const char *advice;
    const char *baby_zh;
    const char *mother_zh;
    const char *advice_zh;
};

// Development summaries are deliberately broad and non-diagnostic. They are
// based on ACOG's public fetal-development bands rather than generated at run
// time: https://www.acog.org/womens-health/faqs/how-your-fetus-grows-during-pregnancy
constexpr ContentBand kBands[] = {
    {1U, 3U, "EARLY STAGE", "TOO EARLY TO ESTIMATE",
     "GESTATIONAL AGE IS COUNTED FROM THE LAST MENSTRUAL PERIOD.",
     "YOU MAY NOT NOTICE PREGNANCY-RELATED CHANGES YET.",
     "KEEP YOUR CONFIRMED DATES AND CARE CONTACTS HANDY.",
     "孕周从末次月经第一天开始计算。", "此时可能还没有明显的孕期变化。",
     "记录确认日期，并保存医护联系方式。"},
    {4U, 8U, "VERY SMALL", "DEVELOPING",
     "THE BRAIN, SPINE AND EARLY ORGAN STRUCTURES BEGIN TO FORM.",
     "SOME PEOPLE NOTICE FATIGUE OR OTHER COMMON EARLY CHANGES.",
     "REST REGULARLY AND FOLLOW YOUR CLINICIAN'S PLAN.",
     "脑、脊柱和早期器官结构开始形成。", "有些人可能出现疲劳或其他早孕期常见变化。",
     "规律休息，并遵循医护人员给出的计划。"},
    {9U, 12U, "EARLY FETAL STAGE", "GROWING",
     "LIMBS, EYELIDS AND NAILS CONTINUE DEVELOPING.",
     "COMMON EARLY-PREGNANCY CHANGES MAY CONTINUE.",
     "WRITE DOWN QUESTIONS FOR YOUR NEXT PRENATAL VISIT.",
     "四肢、眼睑和指甲等结构继续发育。", "常见的孕早期变化可能仍会持续。",
     "把想咨询的问题记在下次产检清单中。"},
    {13U, 16U, "RAPID GROWTH", "INCREASING",
     "BONES HARDEN AND HEARING BEGINS TO DEVELOP.",
     "ENERGY AND COMFORT CAN CHANGE FROM DAY TO DAY.",
     "BALANCE ACTIVITY WITH REST THAT FEELS COMFORTABLE.",
     "骨骼逐渐变硬，听觉开始发育。", "精力和舒适感可能每天不同。",
     "在舒适范围内安排活动与休息。"},
    {17U, 20U, "MORE DEFINED", "INCREASING",
     "MOVEMENT CONTROL AND THE DIGESTIVE SYSTEM CONTINUE DEVELOPING.",
     "SOME PEOPLE BEGIN TO NOTICE MOVEMENT; TIMING VARIES.",
     "KEEP ROUTINE APPOINTMENTS AND NOTE NEW QUESTIONS.",
     "运动控制和消化系统继续发育。", "有些人会逐渐感受到胎动，时间存在个体差异。",
     "按计划产检，并记录新问题。"},
    {21U, 24U, "ACTIVE GROWTH", "ADDING FAT",
     "KICKS MAY GROW STRONGER AND THE SUCKING REFLEX DEVELOPS.",
     "BODY SHAPE AND DAILY COMFORT MAY CONTINUE TO CHANGE.",
     "NOTICE YOUR OWN ROUTINE AND DISCUSS CONCERNS WITH YOUR CARE TEAM.",
     "胎动可能更有力，吸吮反射继续发育。", "体形和日常舒适度可能继续变化。",
     "留意自己的日常规律，有疑问时咨询医护人员。"},
    {25U, 28U, "ACTIVE GROWTH", "ADDING FAT",
     "EYELIDS, LUNGS AND THE NERVOUS SYSTEM CONTINUE MATURING.",
     "REST NEEDS AND COMFORT MAY CHANGE AS PREGNANCY PROGRESSES.",
     "REVIEW UPCOMING APPOINTMENTS AND PREPARATION NOTES.",
     "眼睑、肺和神经系统继续成熟。", "随着孕期推进，休息需求和舒适度可能变化。",
     "查看下一次产检和准备事项。"},
    {29U, 32U, "STEADY GROWTH", "ADDING FAT",
     "STRETCHING, KICKING AND RESPONSES TO LIGHT CONTINUE DEVELOPING.",
     "SOME PEOPLE NOTICE MORE TIREDNESS OR PHYSICAL PRESSURE.",
     "PACE DAILY TASKS AND KEEP TIME FOR REST.",
     "伸展、踢动和对光线的反应继续发育。", "有些人可能更容易疲劳或感到身体负担。",
     "放慢日常节奏，并留出休息时间。"},
    {33U, 36U, "LATE-PREGNANCY GROWTH", "INCREASING",
     "BONES HARDEN WHILE THE SKULL REMAINS FLEXIBLE.",
     "SLEEP AND MOVEMENT MAY FEEL DIFFERENT AS SPACE CHANGES.",
     "KEEP BIRTH-PREPARATION NOTES IN ONE EASY-TO-FIND PLACE.",
     "骨骼继续变硬，头骨仍保持一定柔韧性。", "随着空间变化，睡眠和活动感受可能不同。",
     "把分娩准备事项集中记录在容易找到的位置。"},
    {37U, 40U, "FULL-TERM STAGE", "CONTINUING TO GROW",
     "THE LUNGS, BRAIN AND NERVOUS SYSTEM CONTINUE FINAL MATURATION.",
     "LATE-PREGNANCY CHANGES VARY; FOLLOW YOUR PERSONAL CARE PLAN.",
     "KEEP YOUR CARE TEAM'S CONTACT AND ARRIVAL PLAN READY.",
     "肺、脑和神经系统继续完成最后阶段的成熟。", "孕晚期变化存在个体差异，请遵循个人产检计划。",
     "准备好医护联系方式和就诊安排。"},
};

}  // namespace

WeekContent pregnancy_content_for_week(int week)
{
    const int safe_week = std::max(1, std::min(40, week));
    const ContentBand *selected = &kBands[0];
    for (const ContentBand &band : kBands) {
        if (safe_week >= band.first_week && safe_week <= band.last_week) {
            selected = &band;
            break;
        }
    }
    return {
        static_cast<uint8_t>(safe_week), selected->size, selected->weight,
        selected->baby, selected->mother, selected->advice,
        selected->baby_zh, selected->mother_zh, selected->advice_zh,
    };
}
