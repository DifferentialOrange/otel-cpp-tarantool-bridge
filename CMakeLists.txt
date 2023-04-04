# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

include(FetchContent)
FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)
FetchContent_MakeAvailable(googletest)

add_subdirectory(${CMAKE_SOURCE_DIR}/opentelemetry-cpp)

include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/api/include)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/sdk/include)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/exporters/ostream/include)
add_executable(metrics_ostream_example metrics_ostream.cc)
target_link_libraries(
  metrics_ostream_example ${CMAKE_THREAD_LIBS_INIT} opentelemetry_metrics
  opentelemetry_exporter_ostream_metrics opentelemetry_resources
  common_metrics_foo_library)
