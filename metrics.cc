// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <memory>
#include <thread>
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/aggregation/default_aggregation.h"
#include "opentelemetry/sdk/metrics/aggregation/histogram_aggregation.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"

#include "examples/common/metrics_foo_library/foo_library.h"

namespace metric_sdk      = opentelemetry::sdk::metrics;
namespace common          = opentelemetry::common;
namespace exportermetrics = opentelemetry::exporter::otlp;
namespace metrics_api     = opentelemetry::metrics;

#include <iostream>
#include <iterator>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}  // extern "C"

namespace
{

void InitMetrics(const std::string &name)
{
  exportermetrics::OtlpHttpMetricExporterOptions exporter_options;
  auto exporter = exportermetrics::OtlpHttpMetricExporterFactory::Create(exporter_options);

  std::string version{"1.2.0"};
  std::string schema{"https://opentelemetry.io/schemas/1.2.0"};

  metric_sdk::PeriodicExportingMetricReaderOptions options;
  options.export_interval_millis = std::chrono::milliseconds(1000);
  options.export_timeout_millis  = std::chrono::milliseconds(500);
  std::unique_ptr<metric_sdk::MetricReader> reader{
      new metric_sdk::PeriodicExportingMetricReader(std::move(exporter), options)};
  auto provider = std::shared_ptr<metrics_api::MeterProvider>(new metric_sdk::MeterProvider());
  auto p        = std::static_pointer_cast<metric_sdk::MeterProvider>(provider);
  p->AddMetricReader(std::move(reader));

  // counter view
  std::string counter_name = name + "_counter";
  std::unique_ptr<metric_sdk::InstrumentSelector> instrument_selector{
      new metric_sdk::InstrumentSelector(metric_sdk::InstrumentType::kCounter, counter_name)};
  std::unique_ptr<metric_sdk::MeterSelector> meter_selector{
      new metric_sdk::MeterSelector(name, version, schema)};
  std::unique_ptr<metric_sdk::View> sum_view{
      new metric_sdk::View{name, "description", metric_sdk::AggregationType::kSum}};
  p->AddView(std::move(instrument_selector), std::move(meter_selector), std::move(sum_view));

  // observable counter view
  std::string observable_counter_name = name + "_observable_counter";
  std::unique_ptr<metric_sdk::InstrumentSelector> observable_instrument_selector{
      new metric_sdk::InstrumentSelector(metric_sdk::InstrumentType::kObservableCounter,
                                         observable_counter_name)};
  std::unique_ptr<metric_sdk::MeterSelector> observable_meter_selector{
      new metric_sdk::MeterSelector(name, version, schema)};
  std::unique_ptr<metric_sdk::View> observable_sum_view{
      new metric_sdk::View{name, "test_description", metric_sdk::AggregationType::kSum}};
  p->AddView(std::move(observable_instrument_selector), std::move(observable_meter_selector),
             std::move(observable_sum_view));

  // histogram view
  std::string histogram_name = name + "_histogram";
  std::unique_ptr<metric_sdk::InstrumentSelector> histogram_instrument_selector{
      new metric_sdk::InstrumentSelector(metric_sdk::InstrumentType::kHistogram, histogram_name)};
  std::unique_ptr<metric_sdk::MeterSelector> histogram_meter_selector{
      new metric_sdk::MeterSelector(name, version, schema)};
  std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig> aggregation_config{
      new opentelemetry::sdk::metrics::HistogramAggregationConfig};
  static_cast<opentelemetry::sdk::metrics::HistogramAggregationConfig *>(aggregation_config.get())
      ->boundaries_ = std::vector<double>{0.0,    50.0,   100.0,  250.0,   500.0,  750.0,
                                          1000.0, 2500.0, 5000.0, 10000.0, 20000.0};
  std::unique_ptr<metric_sdk::View> histogram_view{new metric_sdk::View{
      name, "description", metric_sdk::AggregationType::kHistogram, aggregation_config}};
  p->AddView(std::move(histogram_instrument_selector), std::move(histogram_meter_selector),
             std::move(histogram_view));
  metrics_api::Provider::SetMeterProvider(provider);
}

void CleanupMetrics()
{
  std::shared_ptr<metrics_api::MeterProvider> none;
  metrics_api::Provider::SetMeterProvider(none);
}
}  // namespace

static int run(lua_State* L) noexcept
{
  std::string name{"metric_example"};
  InitMetrics(name);

  std::thread counter_example{&foo_library::counter_example, name};
  std::thread observable_counter_example{&foo_library::observable_counter_example, name};
  std::thread histogram_example{&foo_library::histogram_example, name};

  counter_example.join();
  observable_counter_example.join();
  histogram_example.join();

  CleanupMetrics();

  return 1;
}

// Copied from Lua 5.3 so that we can use it with Lua 5.1.
static void setfuncs(lua_State* L, const luaL_Reg* l, int nup) {
  luaL_checkstack(L, nup + 1, "too many upvalues");
  for (; l->name != NULL; l++) { /* fill the table with given functions */
    int i;
    lua_pushstring(L, l->name);
    for (i = 0; i < nup; i++) /* copy upvalues to the top */
      lua_pushvalue(L, -(nup + 1));
    lua_pushcclosure(L, l->func, nup); /* closure with those upvalues */
    lua_settable(L, -(nup + 3));
  }
  lua_pop(L, nup); /* remove upvalues */
}

extern "C" int luaopen_metrics(lua_State* L) {
  lua_newtable(L);
  const struct luaL_Reg functions[] = {
      {"run", run},
      {nullptr, nullptr}};
  setfuncs(L, functions, 0);

  return 1;
}
