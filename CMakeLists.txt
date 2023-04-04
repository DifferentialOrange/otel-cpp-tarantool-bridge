# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

project(lua-bridge-otel)

set(TARANTOOL_FIND_REQUIRED ON)
find_package(Tarantool)

add_subdirectory(${CMAKE_SOURCE_DIR}/opentelemetry-cpp)

include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/api/include)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/sdk/include)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/exporters/prometheus/include)
include_directories(${TARANTOOL_INCLUDE_DIRS})

add_library(metrics SHARED metrics.cc)
target_link_libraries(
  metrics ${CMAKE_THREAD_LIBS_INIT} opentelemetry_metrics
  opentelemetry_exporter_prometheus opentelemetry_resources
  common_metrics_foo_library)

set_target_properties(metrics PROPERTIES PREFIX "" OUTPUT_NAME "metrics")
install(TARGETS metrics LIBRARY DESTINATION ${TARANTOOL_INSTALL_LIBDIR}/otel-metrics)
