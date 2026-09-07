#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${IDF_PATH:?Activate ESP-IDF or set IDF_PATH to its source directory}"
mkdir -p build-host/previews
json="$IDF_PATH/components/json/cJSON"
cc -c "$json/cJSON.c" -o build-host/cJSON.o
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I tests/stubs -I main -I "$json" -I components/zectrix_demo_ui/include -I components/zectrix_demo_ui/font \
  tests/test_dashboard.cc main/dashboard.cc main/tennis.cc main/formula_one.cc main/render.cc main/calendar.cc main/calendar_rules.cc components/zectrix_demo_ui/zectrix_canvas.cc \
  build-host/cJSON.o -o build-host/test-dashboard
./build-host/test-dashboard build-host/previews
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I tests/stubs -I main tests/test_battery_history.cc main/battery_history.cc \
  -o build-host/test-battery-history
./build-host/test-battery-history

c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I tests/stubs -I main -I "$json" -I components/zectrix_demo_ui/include -I components/zectrix_demo_ui/font \
  tests/test_formula_one.cc main/formula_one.cc main/dashboard.cc main/tennis.cc main/render.cc main/calendar.cc main/calendar_rules.cc components/zectrix_demo_ui/zectrix_canvas.cc \
  build-host/cJSON.o -o build-host/test-formula-one
./build-host/test-formula-one build-host/previews

c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I tests/stubs -I main -I "$json" -I components/zectrix_demo_ui/include -I components/zectrix_demo_ui/font \
  tests/test_calendar.cc main/calendar.cc main/calendar_rules.cc main/settings.cc main/dashboard.cc main/tennis.cc main/formula_one.cc main/render.cc components/zectrix_demo_ui/zectrix_canvas.cc \
  build-host/cJSON.o -o build-host/test-calendar
./build-host/test-calendar build-host/previews

c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I tests/stubs -I main -I "$json" \
  tests/test_power_policy.cc main/refresh_policy.cc main/dashboard.cc main/tennis.cc main/formula_one.cc \
  build-host/cJSON.o -o build-host/test-power-policy
./build-host/test-power-policy
