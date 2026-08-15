#include <cstdlib>
#include <iostream>
#include <time.h>

#include "../TimeRules.h"

static void expectEqual(const char* name, uint32_t actual, uint32_t expected) {
  if (actual == expected) return;
  std::cerr << "FAIL " << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
  std::exit(1);
}

static void expectTrue(const char* name, bool ok) {
  if (ok) return;
  std::cerr << "FAIL " << name << "\n";
  std::exit(1);
}

int main() {
  // Same-day quiet hours include the start minute and exclude the end minute.
  expectTrue("same-day start is night", isNightMinute(true, 8 * 60, 17 * 60, 8 * 60));
  expectTrue("same-day before end is night", isNightMinute(true, 8 * 60, 17 * 60, 16 * 60 + 59));
  expectTrue("same-day end is day", !isNightMinute(true, 8 * 60, 17 * 60, 17 * 60));

  // Overnight quiet hours wrap across midnight.
  expectTrue("overnight evening is night", isNightMinute(true, 23 * 60, 7 * 60, 23 * 60 + 30));
  expectTrue("overnight morning is night", isNightMinute(true, 23 * 60, 7 * 60, 6 * 60 + 59));
  expectTrue("overnight midday is day", !isNightMinute(true, 23 * 60, 7 * 60, 12 * 60));

  // Disabled night mode and missing clock values should never suppress fetches.
  expectTrue("disabled night mode is day", !isNightMinute(false, 0, 0, 0));
  expectTrue("missing clock is day", !isNightMinute(true, 23 * 60, 7 * 60, -1));
  expectTrue("same start end means all day", isNightMinute(true, 0, 0, 12 * 60));

  // Sleep scheduling rolls forward to the next occurrence of the configured minute.
  expectEqual("future same day", secondsUntilMinuteOfDay(7 * 60 + 30, 7, 0, 0), 30 * 60);
  expectEqual("past rolls to tomorrow", secondsUntilMinuteOfDay(7 * 60, 8, 0, 0), 23 * 60 * 60);
  expectEqual("exact time rolls to tomorrow", secondsUntilMinuteOfDay(7 * 60, 7, 0, 0), 24 * 60 * 60);

  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
  tzset();
  struct tm spring = {};
  spring.tm_year = 2026 - 1900;
  spring.tm_mon = 2;
  spring.tm_mday = 7;
  spring.tm_hour = 23;
  spring.tm_isdst = -1;
  time_t springEpoch = mktime(&spring);
  expectEqual("spring DST uses elapsed time", secondsUntilLocalMinuteOfDay(7 * 60, springEpoch, spring), 7 * 60 * 60);

  struct tm autumn = {};
  autumn.tm_year = 2026 - 1900;
  autumn.tm_mon = 9;
  autumn.tm_mday = 31;
  autumn.tm_hour = 23;
  autumn.tm_isdst = -1;
  time_t autumnEpoch = mktime(&autumn);
  expectEqual("autumn DST uses elapsed time", secondsUntilLocalMinuteOfDay(7 * 60, autumnEpoch, autumn), 9 * 60 * 60);

  std::cout << "time rule tests passed\n";
  return 0;
}
