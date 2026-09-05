#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${IDF_PATH:?Activate ESP-IDF or set IDF_PATH to its source directory}"
mkdir -p build-host/previews
json="$IDF_PATH/components/json/cJSON"
cc -c "$json/cJSON.c" -o build-host/cJSON.o
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I tests/stubs -I main -I "$json" -I components/zectrix_demo_ui/include -I components/zectrix_demo_ui/font \
  tests/test_dashboard.cc main/dashboard.cc main/render.cc components/zectrix_demo_ui/zectrix_canvas.cc \
  build-host/cJSON.o -o build-host/test-dashboard
./build-host/test-dashboard build-host/previews
