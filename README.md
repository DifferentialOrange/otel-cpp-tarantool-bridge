# OpenTelemetry Metrics in Lua Prototype

Only counter collector and OTLP HTTP exporter are supported now.
Prototype may segfault or memory leak on resources cleanup

To build the shared library for Lua in-source, run
```bash
git submodule update --init --recursive
cd ./opentelemetry-cpp/third_party/opentelemetry-proto
make PROTOC=protoc gen-cpp
cd ../../..
cmake -DBUILD_TESTING=OFF -DWITH_OTLP_HTTP=ON -DBUILD_SHARED_LIBS=ON  . && cmake --build .
```

You'll need `protobuf` installed.

`metrics.so` should appear in the project folder.

Then setup the application
```bash
./tarantool
```

```lua
metrics = require('metrics')

provider = metrics.provider()

provider:init_otlp_http_exporter("http://localhost:4318/v1/metrics")

meter = provider:meter("my_state")

c = meter:double_counter("my_counter")

c:add(4)
```

Start [an OpenTelemetry collector](https://opentelemetry.io/docs/collector/):

```bash
docker run --network=host -v `pwd`/otel-collector-config.yaml:/etc/otel-collector-config.yaml otel/opentelemetry-collector:latest --config=/etc/otel-collector-config.yaml
```

Check application metrics exposed in Prometheus format
```bash
curl localhost:8889/metrics
```

(Beware that `localhost:8888/metrics` contains collector metrics and doesn't contain application metrics.)
