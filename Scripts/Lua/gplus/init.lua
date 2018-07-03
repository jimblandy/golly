-- This module is loaded if a script calls require "gplus".

local g = golly()

local floor = math.floor
local ceil = math.ceil
local abs = math.abs
local mmin = math.min
local mmax = math.max
local millisecs = g.millisecs

local m = {}

m.timing = {}       -- timing information
m.timingorder = {}  -- index to order timers
m.timingstack = 0   -- used for grouping output into dependent timers

--------------------------------------------------------------------------------

function m.timerstart(name)
    name = name or ""

    -- pause GC if running
    if collectgarbage("isrunning") then collectgarbage("stop") end

    local timing, timingorder = m.timing, m.timingorder
    if timing[name] == nil then
        timing[name] = { start=0, last=0, stack=m.timingstack }
        timingorder[#timingorder + 1] = name
    end
    m.timingstack = m.timingstack + 1

    -- start the timer
    timing[name].start = millisecs()
end

--------------------------------------------------------------------------------

function m.timersave(name)
    name = name or ""
    -- return time in milliseconds since last request or timer start
    local timenow = millisecs()
    local timing = m.timing
    if timing[name] then
        m.timingstack = m.timingstack - 1
        timing[name].last = timenow - timing[name].start
        timing[name].start = timenow
        timing[name].stack = m.timingstack
        return timing[name].last
    else
        -- if timer never started return 0
        return 0
    end
end

--------------------------------------------------------------------------------

function m.timervalue(name)
    name = name or ""
    -- return last measured value of the named timer in milliseconds
    local timing = m.timing
    if timing[name] then
        return timing[name].last
    else
        return 0
    end
end

--------------------------------------------------------------------------------

function m.timerresetall()
    -- reset all timers
    m.timing = {}
    m.timingorder = {}
    m.timingstack = 0

    -- restart GC if paused
    if not collectgarbage("isrunning") then
        collectgarbage("restart")
        collectgarbage()
    end
end

--------------------------------------------------------------------------------

function m.timervalueall(precision)
    precision = precision or 1
    if precision < 0 then precision = 0 end
    -- return a string containing all timer name value pairs in the order they were created
    local timing, timingorder = m.timing, m.timingorder
    local result = {}
    local laststack = 0
    local ntimers = #timingorder
    for i = 1, ntimers do
        local name = timingorder[i]
        local timer = timing[name]
        local output = ""
        if timer.stack > laststack then output = "{ "
        elseif timer.stack < laststack then output = "} " end
        laststack = timer.stack
        output = output..name.." "..string.format("%."..precision.."fms", timing[name].last)
        if i == ntimers then
            while laststack > 0 do
                output = output.." }"
                laststack = laststack - 1
            end
        end
        result[#result + 1] = output
    end
    m.timerresetall()
    return table.concat(result, " ")
end

--------------------------------------------------------------------------------

function m.int(x)
    -- return integer part of given floating point number
    return x < 0 and ceil(x) or floor(x)
end

--------------------------------------------------------------------------------

function m.round(x)
    -- return same result as Python's round function
    if x >= 0 then
        return floor(x+0.5)
    else
        return ceil(x-0.5)
    end
end

--------------------------------------------------------------------------------

function m.min(a)
    -- return minimum value in given array
    local alen = #a
    if alen == 0 then return nil end
    if alen < 100000 then
        -- use faster math.min call
        return mmin( table.unpack(a) )
    else
        -- slower code but no danger of exceeding stack limit
        local n = a[1]
        for i = 2, alen do
            if a[i] < n then n = a[i] end
        end
        return n
    end
end

--------------------------------------------------------------------------------

function m.max(a)
    -- return maximum value in given array
    local alen = #a
    if alen == 0 then return nil end
    if alen < 100000 then
        -- use faster math.max call
        return mmax( table.unpack(a) )
    else
        -- slower code but no danger of exceeding stack limit
        local n = a[1]
        for i = 2, alen do
            if a[i] > n then n = a[i] end
        end
        return n
    end
end

--------------------------------------------------------------------------------

function m.drawline(x1, y1, x2, y2, state)
    -- draw a line of cells from x1,y1 to x2,y2 using Bresenham's algorithm
    state = state or 1
    g.setcell(x1, y1, state)
    if x1 == x2 and y1 == y2 then return end

    local dx = x2 - x1
    local ax = abs(dx) * 2
    local sx = 1
    if dx < 0 then sx = -1 end

    local dy = y2 - y1
    local ay = abs(dy) * 2
    local sy = 1
    if dy < 0 then sy = -1 end

    if ax > ay then
        local d = ay - (ax / 2)
        while x1 ~= x2 do
            g.setcell(x1, y1, state)
            if d >= 0 then
                y1 = y1 + sy
                d = d - ax
            end
            x1 = x1 + sx
            d = d + ay
        end
    else
        local d = ax - (ay / 2)
        while y1 ~= y2 do
            g.setcell(x1, y1, state)
            if d >= 0 then
                x1 = x1 + sx
                d = d - ay
            end
            y1 = y1 + sy
            d = d + ax
        end
    end
    g.setcell(x2, y2, state)
end

--------------------------------------------------------------------------------

function m.getedges(r)
    -- return left, top, right, bottom edges of given rect array
    return r[1], r[2], r[1]+r[3]-1, r[2]+r[4]-1
end

--------------------------------------------------------------------------------

function m.getminbox(cells)
    -- return a rect with the minimal bounding box of given cell array or pattern
    if cells.array then
        -- arg is a pattern so get its cell array
        cells = cells.array
    end
    local len = #cells
    if len < 2 then return m.rect( {} ) end

    local minx = cells[1]
    local miny = cells[2]
    local maxx = minx
    local maxy = miny

    -- determine if cell array is one-state or multi-state
    local inc = 2
    if (len & 1) == 1 then inc = 3 end

    -- ignore padding int if present
    if (inc == 3) and (len % 3 == 1) then len = len - 1 end

    for x = 1, len, inc do
        if cells[x] < minx then minx = cells[x] end
        if cells[x] > maxx then maxx = cells[x] end
    end
    for y = 2, len, inc do
        if cells[y] < miny then miny = cells[y] end
        if cells[y] > maxy then maxy = cells[y] end
    end

    return m.rect( {minx, miny, maxx - minx + 1, maxy - miny + 1} )
end

--------------------------------------------------------------------------------

function m.validint(s)
    -- return true if given string represents a valid integer
    if #s == 0 then return false end
    s = s:gsub(",","")
    return s:match("^[+-]?%d+$") ~= nil
end

--------------------------------------------------------------------------------

function m.getposint()
    -- return current viewport position as integer coords
    local x, y = g.getpos()
    return tonumber(x), tonumber(y)
end

--------------------------------------------------------------------------------

function m.setposint(x,y)
    -- convert integer coords to strings and set viewport position
    g.setpos(tostring(x), tostring(y))
end

--------------------------------------------------------------------------------

function m.split(s, sep)
    -- split given string into substrings that are separated by given sep
    -- (emulates Python's split function)
    sep = sep or " "
    local t = {}
    local start = 1
    while true do
        local i = s:find(sep, start, true)  -- find string, not pattern
        if i == nil then
            if start <= #s then
                t[#t+1] = s:sub(start, -1)
            end
            break
        end
        if i > start then
            t[#t+1] = s:sub(start, i-1)
        elseif i == start then
            t[#t+1] = ""
        end
        start = i + #sep
    end
    if #t == 0 then
        -- sep does not exist in s so return s
        return s
    else
        return table.unpack(t)
    end
end

--------------------------------------------------------------------------------

function m.equal(a1, a2)
    -- return true if given arrays have the same values
    if #a1 ~= #a2 then
        -- arrays are not the same length
        return false
    else
        -- both arrays are the same length (possibly 0)
        for i = 1, #a1 do
            if a1[i] ~= a2[i] then return false end
        end
        return true
    end
end

--------------------------------------------------------------------------------

function m.rect(a)
    -- return a table that makes it easier to manipulate rectangles
    -- (emulates glife's rect class)
    local r = {}

    if #a == 0 then
        r.empty = true
    elseif #a == 4 then
        r.empty = false
        r.x  = a[1]
        r.y  = a[2]
        r.wd = a[3]
        r.ht = a[4]
        r.left   = r.x
        r.top    = r.y
        r.width  = r.wd
        r.height = r.ht
        if r.wd <= 0 then error("rect width must be > 0", 2) end
        if r.ht <= 0 then error("rect height must be > 0", 2) end
        r.right  = r.left + r.wd - 1
        r.bottom = r.top  + r.ht - 1
        r.visible = function ()
            return g.visrect( {r.x, r.y, r.wd, r.ht} )
        end
    else
        error("rect arg must be {} or {x,y,wd,ht}", 2)
    end

    return r
end

--------------------------------------------------------------------------------

-- create some useful synonyms:

-- for g.clear and g.advance
m.inside = 0
m.outside = 1

-- for g.flip
m.left_right = 0
m.top_bottom = 1
m.up_down = 1

-- for g.rotate
m.clockwise = 0
m.anticlockwise = 1

-- for g.setcursor (must match strings in Cursor Mode submenu)
m.draw    = "Draw"
m.pick    = "Pick"
m.select  = "Select"
m.move    = "Move"
m.zoomin  = "Zoom In"
m.zoomout = "Zoom Out"

-- transformation matrices
m.identity     = { 1,  0,  0,  1}
m.flip         = {-1,  0,  0, -1}
m.flip_x       = {-1,  0,  0,  1}
m.flip_y       = { 1,  0,  0, -1}
m.swap_xy      = { 0,  1,  1,  0}
m.swap_xy_flip = { 0, -1, -1,  0}
m.rcw          = { 0, -1,  1,  0}
m.rccw         = { 0,  1, -1,  0}

--------------------------------------------------------------------------------

function m.compose(S, T)
    -- Return the composition of two transformations S and T.
    -- A transformation is an array of the form {x, y, A}, which denotes
    -- multiplying by matrix A and then translating by vector (x, y).
    local x, y, A = table.unpack(S)
    local s, t, B = table.unpack(T)
    return { x * B[1] + y * B[2] + s,
             x * B[3] + y * B[4] + t,
             { A[1] * B[1] + A[3] * B[2],
               A[2] * B[1] + A[4] * B[2],
               A[1] * B[3] + A[3] * B[4],
               A[2] * B[3] + A[4] * B[4]
             }
           }
end

--------------------------------------------------------------------------------

-- create a metatable for all pattern tables
local mtp = {}

--------------------------------------------------------------------------------

function m.pattern(arg, x0, y0, A)
    -- return a table that makes it easier to manipulate patterns
    -- (emulates glife's pattern class)
    local p = {}

    setmetatable(p, mtp)

    arg = arg or {}
    x0 = x0 or 0
    y0 = y0 or 0
    A = A or m.identity

    if type(arg) == "table" then
        p.array = {}
        if getmetatable(arg) == mtp then
            -- arg is a pattern
            if #arg.array > 0 then p.array = g.join({},arg.array) end
        else
            -- assume arg is a cell array
            if #arg > 0 then p.array = g.join({},arg) end
        end
    elseif type(arg) == "string" then
        p.array = g.parse(arg, x0, y0, table.unpack(A))
    else
        error("1st arg of pattern must be a cell array, a pattern, or a string", 2)
    end

    p.t = function (x, y, A)
        -- allow scripts to call patt.t(x,y,A) or patt.t({x,y,A})
        if type(x) == "table" then
            x, y, A = table.unpack(x)
        end
        A = A or m.identity
        local newp = m.pattern()
        newp.array = g.transform(p.array, x, y, table.unpack(A))
        return newp
    end

    -- there's no need to implement p.translate and p.apply as they
    -- are just trivial variants of p.t

    p.put = function (x, y, A, mode)
        -- paste pattern into current universe
        x = x or 0
        y = y or 0
        A = A or m.identity
        mode = mode or "or"
        local axx, axy, ayx, ayy = table.unpack(A)
        g.putcells(p.array, x, y, axx, axy, ayx, ayy, mode)
    end

    p.display = function (title, x, y, A, mode)
        -- paste pattern into new universe and display it all
        title = title or "untitled"
        x = x or 0
        y = y or 0
        A = A or m.identity
        mode = mode or "or"
        g.new(title)
        local axx, axy, ayx, ayy = table.unpack(A)
        g.putcells(p.array, x, y, axx, axy, ayx, ayy, mode)
        g.fit()
        g.setcursor(m.zoomin)
    end

    p.save = function (filepath)
        -- save the pattern to given file in RLE format
        g.store(p.array, filepath)
    end

    p.add = function (p1, p2)
        local newp = m.pattern()
        newp.array = g.join(p1.array, p2.array)
        return newp
        -- note that above code is about 15% faster than doing
        -- return m.pattern( g.join(p1.array, p2.array) )
        -- because it avoids calling g.join again
    end

    p.evolve = function (p1, n)
        local newp = m.pattern()
        newp.array = g.evolve(p1.array, n)
        return newp
    end

    -- set metamethod so scripts can do pattern1 + pattern2
    mtp.__add = p.add

    -- set metamethod so scripts can do pattern[10] to get evolved pattern
    mtp.__index = p.evolve

    return p
end

--------------------------------------------------------------------------------

-- avoid calling any g.* command in m.trace otherwise we'll get a Lua error
-- message saying "error in error handling" if user aborts the script
local Windows = g.os() == "Windows"

function m.trace(msg)
    -- this function can be passed into xpcall so that a nice stack trace gets
    -- appended to a runtime error message
    if msg and #msg > 0 then
        -- append a stack trace starting with this function's caller (level 2)
        local result = msg.."\n\n"..debug.traceback(nil, 2)
        -- modify result to make it more readable
        result = result:gsub("\t", "")
        result = result:gsub("in upvalue ", "in function ") -- local function
        -- strip off paths that don't start with "." then remove ".\" or "./"
        if Windows then
            result = result:gsub("\n[^%.][^<\n]+\\", "\n")
            result = result:gsub("%.\\", "")
        else
            result = result:gsub("\n[^%.][^<\n]+/", "\n")
            result = result:gsub("%./", "")
        end
        -- following is nicer if a local function is passed into xpcall
        result = result:gsub("<.+:(%d+)>", "on line %1")
        return result
    else
        return msg
    end
end

--------------------------------------------------------------------------------

return m
