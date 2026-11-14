#include "DailySchedule.h"
#include <cassert>
#include <cstdlib>
#include <initializer_list>

int firstCalls = 0;
int secondCalls = 0;
void firstAction() { ++firstCalls; }
void secondAction() { ++secondCalls; }

time_t localTime(int year, int month, int day, int hour, int minute, int second = 0, int isDst = -1) {
  struct tm local = {};
  local.tm_year = year - 1900;
  local.tm_mon = month - 1;
  local.tm_mday = day;
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_sec = second;
  local.tm_isdst = isDst;
  return mktime(&local);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
  const auto today = localTime(2026, 11, 12, 3, 0);
  DailySchedule schedule(3, 0, 15);
  assert(!schedule.isDue(today));
  assert(schedule.begin());
  assert(!schedule.isDue(0));
  assert(!schedule.isDue(today - 1));
  assert(schedule.isDue(today));
  assert(schedule.isDue(today + 899));
  assert(!schedule.isDue(today + 900));
  assert(!schedule.take(today - 1));
  assert(schedule.take(today + 300));
  assert(!schedule.take(today + 301));
  assert(!schedule.isDue(localTime(2026, 11, 11, 3, 0)));
  assert(schedule.take(localTime(2026, 11, 13, 3, 14)));
  assert(!schedule.isDue(localTime(2026, 11, 14, 4, 0)));
  assert(schedule.isDue(localTime(2026, 11, 15, 3, 5)));

  // A reboot forgets the last scheduled check because the date lives only in RAM.
  DailySchedule rebooted(3, 0, 15);
  assert(rebooted.begin() && rebooted.take(today + 400));
  assert(!rebooted.take(today + 401));

  // The same local hour can occur twice when daylight saving time ends.
  DailySchedule repeatedHour(2, 30, 15);
  assert(repeatedHour.begin());
  const auto first = localTime(2026, 10, 25, 2, 30, 0, 1);
  const auto second = localTime(2026, 10, 25, 2, 30, 0, 0);
  assert(second - first == 3600);
  assert(repeatedHour.take(first) && !repeatedHour.isDue(second));
  DailySchedule summer(3, 0, 15);
  assert(summer.begin() && summer.isDue(localTime(2026, 7, 12, 3, 0)));

  for (auto invalid : {DailySchedule(24, 0, 15), DailySchedule(3, 60, 15),
                       DailySchedule(3, 0, 0), DailySchedule(23, 50, 15)}) {
    assert(!invalid.begin() && !invalid.isDue(today));
  }

  // Actions sharing a time window keep independent execution dates.
  DailySchedule firstSchedule(8, 30, 15);
  DailySchedule secondSchedule(8, 30, 15);
  assert(firstSchedule.begin() && secondSchedule.begin());
  const auto morning = localTime(2026, 11, 12, 8, 30);
  assert(!firstSchedule.run(0, firstAction));
  assert(!firstSchedule.run(morning - 1, firstAction));
  assert(!firstSchedule.run(morning + 900, firstAction));
  assert(firstCalls == 0);
  assert(!firstSchedule.run(morning, nullptr));
  assert(firstSchedule.run(morning, firstAction) && firstCalls == 1);
  assert(secondSchedule.run(morning, secondAction) && secondCalls == 1);
  assert(!firstSchedule.run(morning + 1, firstAction) && firstCalls == 1);
  assert(!secondSchedule.run(morning + 1, secondAction) && secondCalls == 1);
  const auto nextMorning = localTime(2026, 11, 13, 8, 30);
  assert(firstSchedule.run(nextMorning, firstAction) && firstCalls == 2);
  assert(secondSchedule.isDue(nextMorning) && secondCalls == 1);
}
