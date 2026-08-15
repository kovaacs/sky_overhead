#pragma once

#include <stdint.h>
#include <time.h>

static inline bool isNightMinute(bool nightEnabled, uint16_t startMinute, uint16_t endMinute, int nowMinute) {
  if (!nightEnabled) return false;
  if (nowMinute < 0) return false;
  if (startMinute == endMinute) return true;
  if (startMinute < endMinute) return nowMinute >= startMinute && nowMinute < endMinute;
  return nowMinute >= startMinute || nowMinute < endMinute;
}

static inline uint32_t secondsUntilMinuteOfDay(uint16_t minuteOfDay, int hour, int minute, int second) {
  int now = hour * 3600 + minute * 60 + second;
  int diff = (int)minuteOfDay * 60 - now;
  if (diff <= 0) diff += 86400;
  return (uint32_t)diff;
}

static inline uint32_t secondsUntilLocalMinuteOfDay(
  uint16_t minuteOfDay,
  time_t now,
  const struct tm& localNow
) {
  struct tm target = localNow;
  target.tm_hour = minuteOfDay / 60;
  target.tm_min = minuteOfDay % 60;
  target.tm_sec = 0;
  target.tm_isdst = -1;
  time_t targetEpoch = mktime(&target);
  if (targetEpoch <= now) {
    target = localNow;
    target.tm_mday++;
    target.tm_hour = minuteOfDay / 60;
    target.tm_min = minuteOfDay % 60;
    target.tm_sec = 0;
    target.tm_isdst = -1;
    targetEpoch = mktime(&target);
  }
  if (targetEpoch <= now) return 0;
  return (uint32_t)(targetEpoch - now);
}
