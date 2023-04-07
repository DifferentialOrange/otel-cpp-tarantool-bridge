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
#include <lualib.h>
#include <lua.h>
#include <luaconf.h>

#include <tarantool/module.h>
}  // extern "C"

extern "C" {
static const char otlp_provider_mt_name[] = "__tnt_otlp_provider";
}

static int lua_create_provider(struct lua_State *L)
{
	metric_sdk::MeterProvider** provider_ptr = (metric_sdk::MeterProvider **) lua_newuserdata(L, sizeof(metric_sdk::MeterProvider*));
	*provider_ptr = new metric_sdk::MeterProvider();

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

	delete *provider_ptr;

	*provider_ptr = NULL;

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

	static const struct luaL_Reg meta [] = {
		{"new", lua_create_provider},
		{NULL, NULL}
	};
	setfuncs(L, meta, 0);

	return 1;
}
