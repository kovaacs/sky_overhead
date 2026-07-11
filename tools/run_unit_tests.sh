#!/usr/bin/env sh
set -eu

if [ -z "${ARDUINO_JSON_INC:-}" ]; then
  for candidate in \
    "$HOME/Documents/Arduino/libraries/ArduinoJson/src" \
    "$HOME/Documents/Arduino/libraries/ArduinoJson" \
    "$HOME/Arduino/libraries/ArduinoJson/src" \
    "$HOME/Arduino/libraries/ArduinoJson"
  do
    if [ -f "$candidate/ArduinoJson.h" ]; then
      ARDUINO_JSON_INC="$candidate"
      break
    fi
  done
fi

if [ -z "${ARDUINO_JSON_INC:-}" ]; then
  echo "ArduinoJson headers not found. Set ARDUINO_JSON_INC=/path/to/ArduinoJson/src" >&2
  exit 1
fi

run_test() {
  src="$1"
  name="$(basename "$src" .cpp)"
  out=".build/tests/$name"

  c++ -std=c++17 -Wall -Wextra -Werror -I. -I"$ARDUINO_JSON_INC" "$src" -o "$out"
  "$out"
}

mkdir -p .build/tests

for src in tools/test_*.cpp; do
  run_test "$src"
done
