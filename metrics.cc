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
	static const struct luaL_Reg meta [] = {
		{"provider", lua_create_provider},
		{NULL, NULL}
	};
	setfuncs(L, meta, 0);

	return 1;
}
