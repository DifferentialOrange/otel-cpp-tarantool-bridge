# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

project(lua-bridge-otel)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(TARANTOOL_FIND_REQUIRED ON)
find_package(Tarantool)
find_package(Protobuf REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/api/include)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/sdk/include)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/ext/include)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/exporters/otlp/include)
include_directories(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/third_party/opentelemetry-proto/gen/cpp)
include_directories(${TARANTOOL_INCLUDE_DIRS})

add_subdirectory(${CMAKE_SOURCE_DIR}/opentelemetry-cpp)
add_subdirectory(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/exporters/otlp)
include(${CMAKE_SOURCE_DIR}/opentelemetry-cpp/cmake/opentelemetry-proto.cmake)

add_library(metrics SHARED metrics.cc)
target_link_libraries(
  metrics ${CMAKE_THREAD_LIBS_INIT} opentelemetry_metrics
  opentelemetry_exporter_otlp_http_metric opentelemetry_resources
  common_metrics_foo_library ${Protobuf_PROTOC_LIBRARY} ${Protobuf_LIBRARIES})

set_target_properties(metrics PROPERTIES PREFIX "" OUTPUT_NAME "metrics")
install(TARGETS metrics LIBRARY DESTINATION ${TARANTOOL_INSTALL_LIBDIR}/otel-metrics)
