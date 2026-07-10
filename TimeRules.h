#pragma once

#include <stdint.h>

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
