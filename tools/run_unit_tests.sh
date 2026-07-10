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
  out="$2"
  c++ -std=c++17 -Wall -Wextra -Werror -I. -I"$ARDUINO_JSON_INC" "$src" -o "$out"
  "$out"
}

run_test tools/test_left_column_cascade.cpp /tmp/sky_overhead_left_column_tests

run_test tools/test_climate_format.cpp /tmp/sky_overhead_climate_format_tests

run_test tools/test_climate_sensor.cpp /tmp/sky_overhead_climate_sensor_tests

run_test tools/test_aircraft.cpp /tmp/sky_overhead_aircraft_tests

run_test tools/test_display_view.cpp /tmp/sky_overhead_display_view_tests

run_test tools/test_config.cpp /tmp/sky_overhead_config_tests

run_test tools/test_time_rules.cpp /tmp/sky_overhead_time_rule_tests

run_test tools/test_retained_state.cpp /tmp/sky_overhead_retained_state_tests

run_test tools/test_json_helpers.cpp /tmp/sky_overhead_json_helpers_tests

run_test tools/test_adsb_parser.cpp /tmp/sky_overhead_adsb_parser_tests

run_test tools/test_route_parser.cpp /tmp/sky_overhead_route_parser_tests
