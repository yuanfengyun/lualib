local skynet = require "skynet"

local M = {}

local result = {}
local last_hpc = 0
local clock_tbl = {}
local time_tbl = {}
local name_tbl = {}

local function call_hook(t,line)
    local w = debug.getinfo(2, "f")

    if t == "call" then
        local t = clock_tbl[w.func]
        local now = skynet.hpc()
        if t then
            table.insert(t,now)
        else
            clock_tbl[w.func] = {now}
            w = debug.getinfo(2, "fn")
            name_tbl[w.func] = w.name
        end
    elseif t == "return" then
        local f = w.func
        local t = clock_tbl[f]
        if not t then
            return
        end
        local call_time = table.remove(t)
        local now = skynet.hpc()

        time_tbl[f] = (time_tbl[f] or 0) + now - call_time
    end
end

function M.start()
    result = {}
    debug.sethook(call_hook, "cr")
end

function M.stop()
     debug.sethook()
end

local function sortfunc(a,b)
    return a[2] > b[2]
end

function M.dump(filename)
    local list = {}
    for k,v in pairs(time_tbl) do
        table.insert(list,{k,v})
    end

    table.sort(list,sortfunc)

    local f = io.open(filename,"w")
    
    limit = limit or 0
    for _,v in ipairs(list) do
        local w = debug.getinfo(v[1], "nfS")
        print(v[1],tostr(w))
        f:write(w.source)
        f:write(":"..w.linedefined)
        f:write(" "..(name_tbl[v[1]] or ""))
        f:write(string.format(" %10f\n",v[2]/1000000))
    end
    f:close()
end

function M.test_func()
    local a = 1
    local b = 2
    return a+b
end

-- require("util.profile.line").test()
function M.test()
    M.start()
    M.test_func()
    M.stop()
    M.dump("test.txt")
end

return M