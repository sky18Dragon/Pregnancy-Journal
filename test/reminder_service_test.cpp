#include <cassert>
#include <cstring>

#include "reminder_service.h"

Reminder make_reminder(uint32_t id, PregnancyDate date, uint8_t hour,
                       const char *title) {
  Reminder result = {};
  result.id = id;
  result.date = date;
  result.hour = hour;
  std::strncpy(result.title, title, sizeof(result.title) - 1U);
  return result;
}

int main() {
  ReminderService service;
  assert(service.create(make_reminder(1U, {2026U, 9U, 12U}, 15U, "CHECKUP")));
  assert(service.create(make_reminder(2U, {2026U, 9U, 12U}, 9U, "SUPPLEMENT")));
  assert(service.count() == 2U);
  Reminder *today[4] = {};
  assert(service.get_today({2026U, 9U, 12U}, today, 4U) == 2U);
  uint32_t now = 0U;
  assert(reminder_epoch(make_reminder(9U, {2026U, 9U, 12U}, 8U, "NOW"), now));
  uint32_t next_epoch = 0U;
  assert(service.next(now, &next_epoch)->id == 2U);
  assert(service.any_enabled());
  assert(service.toggle(2U));
  assert(service.next(now, &next_epoch)->id == 1U);
  assert(service.toggle(2U));
  assert(service.complete(2U));
  assert(service.next(now)->id == 1U);
  assert(service.remove(1U));
  assert(service.count() == 1U);

  Reminder daily = make_reminder(3U, {2026U, 12U, 31U}, 8U, "WATER");
  daily.repeat_rule = RepeatRule::Daily;
  assert(service.create(daily));
  assert(service.complete(3U));
  assert(service.find(3U)->date.year == 2027U);
  assert(service.find(3U)->date.month == 1U);
  assert(service.find(3U)->date.day == 1U);

  Reminder weekly = make_reminder(4U, {2028U, 2U, 25U}, 10U, "REST");
  weekly.repeat_rule = RepeatRule::Weekly;
  assert(service.create(weekly));
  assert(service.complete(4U));
  assert(service.find(4U)->date.year == 2028U);
  assert(service.find(4U)->date.month == 3U);
  assert(service.find(4U)->date.day == 3U);

  ReminderService full;
  for (uint32_t id = 1U; id <= ReminderService::kCapacity; ++id)
    assert(full.create(make_reminder(id, {2027U, 1U, 1U}, 8U, "ITEM")));
  assert(!full.create(make_reminder(99U, {2027U, 1U, 1U}, 8U, "FULL")));
  return 0;
}
