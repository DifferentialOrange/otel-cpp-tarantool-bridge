# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

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
