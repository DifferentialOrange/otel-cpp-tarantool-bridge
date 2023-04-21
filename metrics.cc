#include <memory>
#include <thread>
#include <chrono>
#include <random>
#include <algorithm>
#include <vector>
#include <variant>
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/aggregation/default_aggregation.h"
#include "opentelemetry/sdk/metrics/aggregation/histogram_aggregation.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/sync_instruments.h"

#include "examples/common/metrics_foo_library/foo_library.h"

namespace metric_sdk      = opentelemetry::sdk::metrics;
namespace common          = opentelemetry::common;
namespace exportermetrics = opentelemetry::exporter::otlp;
namespace metrics_api     = opentelemetry::metrics;
namespace nostd       = opentelemetry::nostd;

#include <iostream>
#include <iterator>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
#include <lua.h>
#include <luaconf.h>

#include <tarantool/module.h>
}  // extern "C"

extern "C" {
static const char otlp_provider_mt_name[] = "__tnt_otlp_provider";
static const char otlp_meter_mt_name[] = "__tnt_otlp_meter";
static const char otlp_double_counter_mt_name[] = "__tnt_otlp_double_counter";
static const char otlp_reader_mt_name[] = "__tnt_otlp_reader";
}

static int lua_create_provider(struct lua_State *L)
{
	metric_sdk::MeterProvider** provider_ptr = (metric_sdk::MeterProvider **) lua_newuserdata(L, sizeof(metric_sdk::MeterProvider*));
	
	auto shared_p = std::shared_ptr<metrics_api::MeterProvider>(new metric_sdk::MeterProvider());
    metrics_api::Provider::SetMeterProvider(shared_p);

    *provider_ptr = std::static_pointer_cast<metric_sdk::MeterProvider>(shared_p).get();

	luaL_getmetatable(L, otlp_provider_mt_name);
	lua_setmetatable(L, -2);
	return 1;
}

static int lua_clean_provider(struct lua_State *L)
{
	metric_sdk::MeterProvider** provider_ptr = (metric_sdk::MeterProvider **) luaL_checkudata(L, 1, otlp_provider_mt_name);

	if (*provider_ptr == NULL) {
		return 0;
	}

	std::shared_ptr<metrics_api::MeterProvider> none;
	metrics_api::Provider::SetMeterProvider(none);

	delete *provider_ptr;

	*provider_ptr = NULL;

	return 0;
}

static int lua_provider_init_otlp_http_exporter(struct lua_State *L)
{
	metric_sdk::MeterProvider** provider_ptr = (metric_sdk::MeterProvider **) luaL_checkudata(L, 1, otlp_provider_mt_name);
	std::string url = luaL_checkstring(L, 2);

	exportermetrics::OtlpHttpMetricExporterOptions exporter_options;
	exporter_options.url = url;
	auto exporter = exportermetrics::OtlpHttpMetricExporterFactory::Create(exporter_options);

	metric_sdk::PeriodicExportingMetricReaderOptions options;
	options.export_interval_millis = std::chrono::milliseconds(1000);
	options.export_timeout_millis  = std::chrono::milliseconds(500);
	std::unique_ptr<metric_sdk::MetricReader> reader{
	    new metric_sdk::PeriodicExportingMetricReader(std::move(exporter), options)};

	(*provider_ptr)->AddMetricReader(std::move(reader));

	return 0;
}

class MyExporter : public metric_sdk::MetricReader
{
public:
  /**
   * Constructor - binds an exposer and collector to the exporter
   * @param options: options for an exposer that exposes
   *  an HTTP endpoint for the exporter to connect to
   */
  MyExporter();

  metric_sdk::AggregationTemporality GetAggregationTemporality(
      metric_sdk::InstrumentType instrument_type) const noexcept override;

private:
  bool OnForceFlush(std::chrono::microseconds timeout) noexcept override;

  bool OnShutDown(std::chrono::microseconds timeout) noexcept override;

  void OnInitialized() noexcept override;
};

MyExporter::MyExporter()
{

}

metric_sdk::AggregationTemporality MyExporter::GetAggregationTemporality(
    metric_sdk::InstrumentType /* instrument_type */) const noexcept
{
  // Prometheus exporter only support Cumulative
  return metric_sdk::AggregationTemporality::kCumulative;
}

bool MyExporter::OnForceFlush(std::chrono::microseconds /* timeout */) noexcept
{
  return true;
}

bool MyExporter::OnShutDown(std::chrono::microseconds /* timeout */) noexcept
{
  return true;
}

void MyExporter::OnInitialized() noexcept {}


static int lua_provider_new_reader(struct lua_State *L)
{
	metric_sdk::MeterProvider** provider_ptr = (metric_sdk::MeterProvider **) luaL_checkudata(L, 1, otlp_provider_mt_name);

	auto reader = std::shared_ptr<metric_sdk::MetricReader>(new MyExporter);
	
	(*provider_ptr)->AddMetricReader(reader);

	metric_sdk::MetricReader** reader_ptr = (metric_sdk::MetricReader **) lua_newuserdata(L, sizeof(metric_sdk::MetricReader*));
	*reader_ptr = reader.get();

	luaL_getmetatable(L, otlp_reader_mt_name);
	lua_setmetatable(L, -2);

	return 1;
}

static int lua_reader_collect(struct lua_State *L)
{
	metric_sdk::MetricReader** reader_ptr = (metric_sdk::MetricReader **) luaL_checkudata(L, 1, otlp_reader_mt_name);

	(*reader_ptr)->Collect([L](metric_sdk::ResourceMetrics &metric_data) {
		lua_newtable(L); // return value

		lua_newtable(L); // "resourceMetrics" array

		lua_pushnumber(L, 1); // "resourceMetrics" array key
		lua_newtable(L); // "resourceMetrics" array element

		lua_newtable(L); // "resource" field of "resourceMetrics" array element
		auto attributes = metric_data.resource_->GetAttributes();

		lua_newtable(L); // "attributes" field of "resource" map
		int i = 0;
		for (auto const& attr : attributes) {
			i++;
			lua_pushnumber(L, i); // "attributes" array key

			lua_newtable(L); // "attributes" array element

			lua_pushstring(L, attr.first.c_str()); // "key" field value of "attributes" array element
			lua_setfield(L, -2, "key"); // push "key" field to "attributes" array element

			lua_newtable(L);  // "value" field value of "attributes" array element

			if (nostd::holds_alternative<std::string>(attr.second)) {
				auto strval = nostd::get<std::string>(attr.second);
				lua_pushstring(L, strval.c_str()); // "stringValue" field value of "value" map
				lua_setfield(L, -2, "stringValue"); // push "stringValue" field to "value" map
			}

			lua_setfield(L, -2, "value"); // push "value" field to "attributes" array element

			lua_settable(L, -3); // push "attributes" array element to "attributes" array
		}
		lua_setfield(L, -2, "attributes"); // push "attributes" field to "resource" map

		lua_setfield(L, -2, "resource"); // push "resource" field to "resourceMetrics" array element

		lua_newtable(L); // "scopeMetrics" field of "resourceMetrics" array element

		int j = 0;
		for (auto const& scope : metric_data.scope_metric_data_) {
			j++;
			lua_pushnumber(L, j); // "scopeMetrics" array key

			lua_newtable(L); // "scopeMetrics" array element

			lua_newtable(L); // "scope" field of "scopeMetrics" array element

			lua_pushstring(L, scope.scope_->GetName().c_str()); // "name" field of "scope" map
			lua_setfield(L, -2, "name"); // "name" field of "scope" map

			lua_pushstring(L, scope.scope_->GetVersion().c_str()); // "version" field of "scope" map
			lua_setfield(L, -2, "version"); // "version" field of "scope" map

			// schemaURL is also should be here

			lua_setfield(L, -2, "scope"); // push "scope" field to "scopeMetrics" array element

			lua_newtable(L); // "metrics" field of "scopeMetrics" array element

			int k = 0;
			for (auto const& metric : scope.metric_data_) {
				k++;
				lua_pushnumber(L, k); // "metrics" array key

				lua_newtable(L); // "metrics" array element

				lua_pushstring(L, metric.instrument_descriptor.name_.c_str()); // "name" field of "metrics" array element
				lua_setfield(L, -2, "name"); // push "name" field of "metrics" array element

				lua_newtable(L); // "sum" field of "metrics" array element

				lua_pushnumber(L, int(metric.aggregation_temporality)); // "aggregationTemporality" field of "sum" map
				lua_setfield(L, -2, "aggregationTemporality"); // push "aggregationTemporality" field of "sum" map

				lua_pushboolean(L, true); // "isMonotonic" field of "sum" map
				lua_setfield(L, -2, "isMonotonic"); // push "isMonotonic" field of "sum" map

				lua_newtable(L); // "dataPoints" field of "sum" map

				int l = 0;
				for (auto const& point : metric.point_data_attr_) {
					l++;
					lua_pushnumber(L, l); // "dataPoints" array key

					lua_newtable(L); // "dataPoints" array element

					if (!point.attributes.empty()) {
						lua_newtable(L); // "attributes" field of "dataPoints" array element

						int m = 0;
						for (auto const& attr : point.attributes) {
							m++;
							lua_pushnumber(L, m); // "attributes" array key

							lua_newtable(L); // "attributes" array element

							lua_pushstring(L, attr.first.c_str()); // "key" field value of "attributes" array element
							lua_setfield(L, -2, "key"); // push "key" field to "attributes" array element

							lua_newtable(L);  // "value" field value of "attributes" array element

							if (nostd::holds_alternative<std::string>(attr.second)) {
								auto strval = nostd::get<std::string>(attr.second);
								lua_pushstring(L, strval.c_str()); // "stringValue" field value of "value" map
								lua_setfield(L, -2, "stringValue"); // push "stringValue" field to "value" map
							}

							lua_setfield(L, -2, "value"); // push "value" field to "attributes" array element

							lua_settable(L, -3); // push "attributes" array element to "attributes" array
						}
						lua_setfield(L, -2, "attributes"); // push "attributes" field to "resource" map
					}

					lua_pushstring(L, std::to_string(metric.start_ts.time_since_epoch().count()).c_str()); // "startTimeUnixNano" field value of "dataPoints" array element
					lua_setfield(L, -2, "startTimeUnixNano"); // push "startTimeUnixNano" field to "dataPoints" array element

					lua_pushstring(L, std::to_string(metric.end_ts.time_since_epoch().count()).c_str()); // "startTimeUnixNano" field value of "dataPoints" array element
					lua_setfield(L, -2, "timeUnixNano"); // push "timeUnixNano" field to "dataPoints" array element

					if (nostd::holds_alternative<metric_sdk::SumPointData>(point.point_data)) {
						auto pt = nostd::get<metric_sdk::SumPointData>(point.point_data);
						// no int expected here
						auto val = nostd::get<double>(pt.value_);

						lua_pushnumber(L, val); // "asDouble" field value of "dataPoints" array element
						lua_setfield(L, -2, "asDouble"); // push "asDouble" field to "dataPoints" array element
					}

					lua_settable(L, -3); // push "dataPoints" array element to "dataPoints" array
				}
				lua_setfield(L, -2, "dataPoints"); // push "dataPoints" field to "scopeMetrics" array element

				lua_setfield(L, -2, "sum"); // push "sum" field of "metrics" array element

				lua_settable(L, -3); // push "metrics" array element to "scopeMetrics" array
			}
			lua_setfield(L, -2, "metrics"); // push "metrics" field to "scopeMetrics" array element

			lua_settable(L, -3); // push "scopeMetrics" array element to "scopeMetrics" array
		}

		lua_setfield(L, -2, "scopeMetrics"); // push "scopeMetrics" field to "resourceMetrics" array element

		lua_settable(L, -3); // push "resourceMetrics" array element to "resourceMetrics" array

		lua_setfield(L, -2, "resourceMetrics"); // push "resourceMetrics" field to return value table

	    return true;
	});

	return 1;
}

static int lua_provider_new_meter(struct lua_State *L)
{
	metric_sdk::MeterProvider** provider_ptr = (metric_sdk::MeterProvider **) luaL_checkudata(L, 1, otlp_provider_mt_name);
	std::string name = luaL_checkstring(L, 2);

	std::string version{"1.2.0"};

	nostd::shared_ptr<metrics_api::Meter> meter = (*provider_ptr)->GetMeter(name, version);

	metrics_api::Meter** meter_ptr = (metrics_api::Meter **) lua_newuserdata(L, sizeof(metrics_api::Meter*));
	*meter_ptr = meter.get();

	luaL_getmetatable(L, otlp_meter_mt_name);
	lua_setmetatable(L, -2);

	return 1;
}

static int lua_meter_new_double_counter(struct lua_State *L)
{
	metrics_api::Meter** meter_ptr = (metrics_api::Meter **) luaL_checkudata(L, 1, otlp_meter_mt_name);
	std::string counter_name = luaL_checkstring(L, 2);

	nostd::unique_ptr<metrics_api::Counter<double>> double_counter = (*meter_ptr)->CreateDoubleCounter(counter_name);

	metrics_api::Counter<double>** counter_ptr = (metrics_api::Counter<double> **) lua_newuserdata(L, sizeof(metrics_api::Counter<double>*));
	*counter_ptr = double_counter.release();

	luaL_getmetatable(L, otlp_double_counter_mt_name);
	lua_setmetatable(L, -2);

	return 1;
}

static std::map<std::string, std::string> lua_getstringtable(lua_State *L, int index)
{
	std::map<std::string, std::string> res;
    // Push another reference to the table on top of the stack (so we know
    // where it is, and this function can work for negative, positive and
    // pseudo indices
    lua_pushvalue(L, index);
    // stack now contains: -1 => table
    lua_pushnil(L);
    // stack now contains: -1 => nil; -2 => table
    while (lua_next(L, -2))
    {
        // stack now contains: -1 => value; -2 => key; -3 => table
        // copy the key so that lua_tostring does not modify the original
        lua_pushvalue(L, -2);
        // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
        const char *key = lua_tostring(L, -1);
        const char *value = lua_tostring(L, -2);
        res[key] = value;
        // pop value + copy of key, leaving original key
        lua_pop(L, 2);
        // stack now contains: -1 => key; -2 => table
    }
    // stack now contains: -1 => table (when lua_next returns 0 it pops the key
    // but does not push anything.)
    // Pop table
    lua_pop(L, 1);
    // Stack is now the same as it was on entry to this function
    return std::move(res);
}

static int lua_counter_add(struct lua_State *L)
{
	metrics_api::Counter<double>** counter_ptr = (metrics_api::Counter<double> **) luaL_checkudata(L, 1, otlp_double_counter_mt_name);

	luaL_checktype(L, 2, LUA_TNUMBER);
	double val = lua_tonumber(L, 2);

	if (lua_gettop(L) > 2) {
		auto labels = lua_getstringtable(L, 3);
		(*counter_ptr)->Add(val, labels);
	} else {
		(*counter_ptr)->Add(val);
	}

	return 0;
}

static int lua_counter_add_perf_test(struct lua_State *L)
{
	metrics_api::Counter<double>** counter_ptr = (metrics_api::Counter<double> **) luaL_checkudata(L, 1, otlp_double_counter_mt_name);


	// First create an instance of an engine.
    std::random_device rnd_device;
    // Specify the engine and distribution.
    std::mt19937 mersenne_engine {rnd_device()};  // Generates random integers
    std::uniform_real_distribution<double> dist {1, 100};
    
    auto gen = [&dist, &mersenne_engine](){
        return dist(mersenne_engine);
    };

    size_t samples = 100000;
    std::vector<double> vec(samples);
    generate(std::begin(vec), std::end(vec), gen);

	auto start = std::chrono::high_resolution_clock::now();

    for (auto v : vec) {
        (*counter_ptr)->Add(v);
    }

	auto end = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	std::cout << "No labels time for " << samples << " samples: " << duration.count() << "mks" << std::endl;

	std::map<std::string, std::string> labels{{"mylabel1", "val1"}, {"mylabel2", "val2"}};

	start = std::chrono::high_resolution_clock::now();

    for (auto v : vec) {
        (*counter_ptr)->Add(v, labels);
    }

	end = std::chrono::high_resolution_clock::now();

	duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	std::cout << "With 2 labels time for " << samples << " samples: " << duration.count() << "mks" << std::endl;

	return 0;
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

extern "C" int luaopen_metrics(struct lua_State *L)
{
	lua_newtable(L);
	static const struct luaL_Reg otlp_provider_methods [] = {
		{"init_otlp_http_exporter", lua_provider_init_otlp_http_exporter},
		{"meter", lua_provider_new_meter},
		{"reader", lua_provider_new_reader},
		{"__gc", lua_clean_provider},
		{NULL, NULL}
	};
	luaL_newmetatable(L, otlp_provider_mt_name);
	lua_pushvalue(L, -1);
	setfuncs(L, otlp_provider_methods, 0);
	lua_setfield(L, -2, "__index");
	lua_pushstring(L, otlp_provider_mt_name);
	lua_setfield(L, -2, "__metatable");
	lua_pop(L, 1);

	lua_newtable(L);
	static const struct luaL_Reg otlp_meter_methods [] = {
		{"double_counter", lua_meter_new_double_counter},
		{NULL, NULL}
	};
	luaL_newmetatable(L, otlp_meter_mt_name);
	lua_pushvalue(L, -1);
	setfuncs(L, otlp_meter_methods, 0);
	lua_setfield(L, -2, "__index");
	lua_pushstring(L, otlp_meter_mt_name);
	lua_setfield(L, -2, "__metatable");
	lua_pop(L, 1);

	lua_newtable(L);
	static const struct luaL_Reg otlp_double_counter_methods [] = {
		{"add", lua_counter_add},
		{"add_perf_test", lua_counter_add_perf_test},
		{NULL, NULL}
	};
	luaL_newmetatable(L, otlp_double_counter_mt_name);
	lua_pushvalue(L, -1);
	setfuncs(L, otlp_double_counter_methods, 0);
	lua_setfield(L, -2, "__index");
	lua_pushstring(L, otlp_double_counter_mt_name);
	lua_setfield(L, -2, "__metatable");
	lua_pop(L, 1);

	lua_newtable(L);
	static const struct luaL_Reg otlp_reader_methods [] = {
		{"collect", lua_reader_collect},
		{NULL, NULL}
	};
	luaL_newmetatable(L, otlp_reader_mt_name);
	lua_pushvalue(L, -1);
	setfuncs(L, otlp_reader_methods, 0);
	lua_setfield(L, -2, "__index");
	lua_pushstring(L, otlp_reader_mt_name);
	lua_setfield(L, -2, "__metatable");
	lua_pop(L, 1);

	lua_newtable(L);
	static const struct luaL_Reg meta [] = {
		{"provider", lua_create_provider},
		{NULL, NULL}
	};
	setfuncs(L, meta, 0);

	return 1;
}
