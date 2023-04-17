local fio = require('fio')

local METERS = 100
local BUCKETS = {1, 3, 5, 6, 7, 8, 9, 10, 15, 20}

local charset = {} -- [0-9a-zA-Z]
-- for c = 48, 57  do table.insert(charset, string.char(c)) end
for c = 65, 90  do table.insert(charset, string.char(c)) end
for c = 97, 122 do table.insert(charset, string.char(c)) end

local function random_string(length)
    if not length or length <= 0 then return '' end
    math.randomseed(os.clock()^5)
    return random_string(length - 1) .. charset[math.random(1, #charset)]
end

local meter_names = {}
local counter_names = {}

local scenario = [[
local metrics = require('metrics')

local provider = metrics.provider()
provider:init_otlp_http_exporter("http://localhost:4318/v1/metrics")

-- Just in case
local meter_registry = {}
local counter_registry = {}

local meter, counter
]]

for i = 1, METERS do
    local new_meter_name = random_string(10)
    assert(meter_names[new_meter_name] == nil)
    meter_names[new_meter_name] = true

    scenario = scenario .. ([[
meter = provider:meter(%q)
table.insert(meter_registry, meter)
]]):format(new_meter_name, i)

    for j = 1, BUCKETS[math.random(1, #BUCKETS)] do
        local new_counter_name = random_string(10)
        assert(counter_names[new_counter_name] == nil)
        counter_names[new_counter_name] = true

        scenario = scenario .. ([[
counter = meter:double_counter(%q)
table.insert(counter_registry, counter)
counter:add(%f)
]]):format(new_counter_name, math.random(1, 1000))
    end

    scenario = scenario .. "\n"
end

scenario = scenario .. [[
print('meters: ' .. tostring(#meter_registry))
print('counters: ' .. tostring(#counter_registry))

local clock = require('clock')
local fiber = require('fiber')

local time = 10 * 60

local start = clock.monotonic()

while (clock.monotonic() - start) < time do
    for _, c in pairs(counter_registry) do
        c:add(math.random(1, 1000))
    end

    fiber.sleep(5)
end
]]

local scenario_name = './perf_scenario_1.lua'
fio.unlink(scenario_name)
local f = fio.open(scenario_name, {'O_RDWR', 'O_CREAT', 'O_APPEND'}, tonumber('644',8))
f:write(scenario)
f:close()
