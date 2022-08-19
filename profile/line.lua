local M = {}

local result = {}
local last_hpc = 0

local function hook(t,line)
    local bt = debug.traceback(nil,2)
    local count = result[bt] or 0
    result[bt] = count + 1
end

function M.start()
    result = {}
    debug.sethook(hook, "line",10)
end

function M.stop()
     debug.sethook()
end

local function sortfunc(a,b)
    return a[2] > b[2]
end

function M.dump(filename)
    local list = {}
    for k,v in pairs(result) do
        table.insert(list,{k,v})
    end

    table.sort(list,sortfunc)

    local f = io.open(filename,"w")
    
    for _,v in ipairs(list) do
        local s = v[1]
        s = string.gsub(s,"\t","")
        s = string.gsub(s,"\n",";")
        local slist = split(s, ";")
        table.remove(slist,1)

        local snew = {}
        for _,item in ipairs(slist) do
            table.insert(snew,1,item)
        end
        s = table.concat(snew,";")
        f:write(s)
        f:write(string.format(" %8d\n",v[2]))
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