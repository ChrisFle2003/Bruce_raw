cmake_minimum_required(VERSION 3.20)
project(bruce_v4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bruce_core
  src/core_axes.cpp
  src/core_ops.cpp
  src/core_rules.cpp
  src/core_state.cpp
)
target_include_directories(bruce_core PUBLIC include)

add_library(bruce_io
  src/minijson.cpp
  src/util_sha256.cpp
  src/io_project_load.cpp
  src/io_fields.cpp
  src/io_export_json.cpp
)
target_include_directories(bruce_io PUBLIC include)

add_executable(brucev4
  src/cli_main.cpp
)
target_link_libraries(brucev4 PRIVATE bruce_core bruce_io)

add_executable(test_core
  tests/test_core.cpp
)
target_link_libraries(test_core PRIVATE bruce_core)

add_executable(test_v6
  tests/test_v6.cpp
)
target_link_libraries(test_v6 PRIVATE bruce_core)

add_executable(bench_mix_v4
  bench/bench_mix_v4.cpp
)
target_link_libraries(bench_mix_v4 PRIVATE bruce_core)
