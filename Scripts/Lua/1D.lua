--[[
This script lets you use Golly to explore one-dimensional rules.
It supports all of Stephen Wolfram's 256 elementary rules, as well
as totalistic rules with up to 4 states and a maximum range of 4.
Author: Andrew Trevorrow (andrew@trevorrow.com), May 2019.
--]]

local g = golly()
local gp = require "gplus"
local split = gp.split

require "gplus.NewCA"
SCRIPT_NAME = "1D"
DEFAULT_RULE = "W110"
RULE_HELP = [[
This script lets you explore one-dimensional rules.
Stephen Wolfram's elementary rules are strings of the form Wn
where n is a number from 0 to 255.
<p>
Totalistic rules are strings of the form CcKkRr where c is a code
number from 0 to k^((2r+1)k-2r)-1, k is the number of states (2 to 4),
and r is the range (1 to 4).
<p>
More details can be found at these links:<br>
<a href="http://mathworld.wolfram.com/ElementaryCellularAutomaton.html"
        >http://mathworld.wolfram.com/ElementaryCellularAutomaton.html</a><br>
<a href="http://mathworld.wolfram.com/TotalisticCellularAutomaton.html"
        >http://mathworld.wolfram.com/TotalisticCellularAutomaton.html</a>
]]

-- the following are non-local so a startup script can change them
DEFWD, DEFHT = 500, 500         -- default grid size
aliases = {}                    -- none at the moment

--------------------------------------------------------------------------------

NextPattern = function() end    -- ParseRule sets this to NextElementary or NextTotalistic
local birth = {}                -- set by ParseRule (only if given rule is valid)
local numstates = 2             -- ditto
local range = 1                 -- ditto
local empty_row = false         -- NextPattern created an empty row?

function ParseRule(newrule)
    -- Parse the given rule string.
    -- If valid then return nil, the canonical rule string,
    -- the width and height of the grid, and the number of states.
    -- If not valid then just return an appropriate error message.
    
    if #newrule == 0 then
        newrule = DEFAULT_RULE  -- should be a valid rule!
    else
        -- check for a known alias
        local rule = aliases[newrule]
        if rule then
            newrule = rule
        elseif newrule:find(":") then
            -- try without the suffix
            local p, s = split(newrule,":")
            rule = aliases[p]
            if rule then newrule = rule..":"..s end
        end
    end
    
    local prefix, suffix = split(newrule:upper(),":")
    
    -- check for a valid prefix
    local elementary = true
    local n, c, k, r
    if prefix:find("^W") then
        n = tonumber( prefix:match("^W(%d+)$") )
        if n == nil or n > 255 then
            return "Rule syntax is Wn where n is from 0 to 255."
        end
    elseif prefix:find("^C") then
        c, k, r = prefix:match("^C(%d+)K([234])R([1234])$")
        c = tonumber(c)
        k = tonumber(k)
        r = tonumber(r)
        if not (c and k and r) then
            return "Rule syntax is CcKkRr where c is the code number,\n"..
                   "k is the number of states (2 to 4), and r is the\n"..
                   "range (1 to 4)."
        end
        local maxcode = math.floor(k^((2*r+1)*k-2*r)) - 1
        if c > maxcode then
            return "Maximum code for K"..k.." and R"..r.." is "..maxcode.."."
        end
        elementary = false
    else
        return "Rule must start with W or C."
    end    
    
    -- check for a valid suffix like T50 or T50,30
    local wd, ht = DEFWD, DEFHT
    if suffix then
        if suffix:find(",") then
            wd, ht = suffix:match("^T(%d+),(%d+)$")
        else
            wd = suffix:match("^T(%d+)$")
            ht = wd
        end
        wd = tonumber(wd)
        ht = tonumber(ht)
        if wd == nil or ht == nil then
            return "Rule suffix must be Twd,ht or Twd."
        end
    end
    if wd < 10 then wd = 10 elseif wd > 4000 then wd = 4000 end
    if ht < 10 then ht = 10 elseif ht > 4000 then ht = 4000 end
    
    -- given rule is valid
    
    -- set birth table for use in NextPattern
    birth = {}
    if elementary then
        local bit = 1
        for i = 0, 7 do
            if n & bit > 0 then birth[i] = 1 end
            bit = bit * 2
        end
    else
        -- totalistic
        local code = c
        for i = 0, (2*r+1)*k-2*r-1 do
            if code == 0 then break end
            local s = code % k
            if s > 0 then birth[i] = s end
            code = code // k
        end
    end
    if birth[0] == nil then empty_row = false end

    -- set NextPattern, numstates, range, and create the canonical rule
    local canonrule
    if elementary then
        NextPattern = NextElementary
        numstates = 2
        range = 1
        canonrule = "W"..n..":T"..wd..","..ht
    else
        -- totalistic
        NextPattern = NextTotalistic
        numstates = k
        range = r
        canonrule = "C"..c.."K"..k.."R"..r..":T"..wd..","..ht
    end
    
    return nil, canonrule, wd, ht, numstates
end

--------------------------------------------------------------------------------

function NextElementary(currcells, minx, miny, maxx, maxy)
    -- Create the next elementary pattern.
    
    local newrow = {}       -- cell array for the new row (one-state)
    local newlen = 0        -- length of newrow
    local get = g.getcell
    
    -- find the bottom row of the current pattern
    local bbox = g.getrect()
    local y = bbox[2] + bbox[4] - 1

    if birth[0] then
        -- for odd-numbered rules we need to ensure the grid doesn't become empty
        -- when we wrap to the top row
        if y == maxy then
            -- save bottom row, erase current pattern and put bottom row in top row
            local gridwd = maxx-minx+1
            local bottrow = g.getcells( {minx, maxy, gridwd, 1} )
            g.putcells(currcells, 0, 0, 1, 0, 0, 1, "xor")
            g.putcells(bottrow, 0, -(maxy-miny), 1, 0, 0, 1, "or")
            currcells = g.getcells( {minx, miny, gridwd, 1} )
            y = miny
        elseif empty_row then
            -- if bottom row of current pattern is full then advance y by 1
            local full = true
            for x = minx, maxx do
                if get(x, y) == 0 then full = false ; break end
            end
            if full then y = y + 1 end
        end
    end
    
    local newy = y + 1      -- y coordinates for newrow
    if newy > maxy then
        newy = miny         -- wrap to top row
    end
    
    for x = minx, maxx do
        local xm1 = x-1
        local xp1 = x+1
        -- wrap left and right edges
        if xm1 < minx then xm1 = maxx end
        if xp1 > maxx then xp1 = minx end
        local i = get(xp1, y)
        if get(x,   y) == 1 then i = i + 2 end
        if get(xm1, y) == 1 then i = i + 4 end
        if birth[i] then
            newlen = newlen+1 ; newrow[newlen] = x
            newlen = newlen+1 ; newrow[newlen] = newy
        end
    end
    
    empty_row = newlen == 0     -- for next call (only used if birth[0])
    
    if newy == miny then
        -- erase current pattern and put newrow in top row
        g.putcells(currcells, 0, 0, 1, 0, 0, 1, "xor")
        g.putcells(newrow)
        return newrow
    else
        -- append newrow to currcells (no need to erase current pattern)
        local currlen = #currcells
        for i = 1, newlen do
            currlen = currlen+1 ; currcells[currlen] = newrow[i]
        end
        g.putcells(newrow)
        return currcells
    end
end

--------------------------------------------------------------------------------

function NextTotalistic(currcells, minx, miny, maxx, maxy)
    -- Create the next totalistic pattern.
    
    local newrow = {}       -- cell array for the new row (one-state or multi-state)
    local newlen = 0        -- length of newrow
    local get = g.getcell
    local multistate = numstates > 2
    
    -- find the bottom row of the current pattern
    local bbox = g.getrect()
    local y = bbox[2] + bbox[4] - 1

    if birth[0] then
        -- for odd-numbered rules we need to ensure the grid doesn't become empty
        -- when we wrap to the top row
        if y == maxy then
            -- save bottom row, erase current pattern and put bottom row in top row
            local gridwd = maxx-minx+1
            local bottrow = g.getcells( {minx, maxy, gridwd, 1} )
            g.putcells(currcells, 0, 0, 1, 0, 0, 1, "xor")
            g.putcells(bottrow, 0, -(maxy-miny), 1, 0, 0, 1, "or")
            currcells = g.getcells( {minx, miny, gridwd, 1} )
            y = miny
        elseif empty_row then
            -- if bottom row of current pattern is full then advance y by 1
            local full = true
            for x = minx, maxx do
                if get(x, y) == 0 then full = false ; break end
            end
            if full then y = y + 1 end
        end
    end
    
    local newy = y + 1      -- y coordinates for newrow
    if newy > maxy then
        newy = miny         -- wrap to top row
    end
    
    for x = minx, maxx do
        local total = get(x, y)
        for i = 1, range do
            local xmi = x-i
            local xpi = x+i
            -- wrap left and right edges
            if xmi < minx then xmi = maxx - (minx - xmi - 1) end
            if xpi > maxx then xpi = minx + (xpi - maxx - 1) end
            total = total + get(xmi, y)
            total = total + get(xpi, y)
        end
        if birth[total] then
            newlen = newlen+1 ; newrow[newlen] = x
            newlen = newlen+1 ; newrow[newlen] = newy
            if multistate then
                newlen = newlen+1 ; newrow[newlen] = birth[total]
            end
        end
    end
    
    if newlen > 0 and multistate then
        -- ensure length of newrow is odd
        if newlen & 1 == 0 then newlen = newlen+1 ; newrow[newlen] = 0 end
    end
    
    empty_row = newlen == 0     -- for next call (only used if birth[0])
    
    if newy == miny then
        -- erase current pattern and put newrow in top row
        g.putcells(currcells, 0, 0, 1, 0, 0, 1, "xor")
        g.putcells(newrow)
        return newrow
    else
        -- append newrow to currcells (no need to erase current pattern)
        local currlen = #currcells
        if multistate then
            -- ignore any padding ints in currcells and newrow
            if currlen % 3 > 0 then currlen = currlen - 1 end
            if newlen % 3 > 0 then newlen = newlen - 1 end
        end
        for i = 1, newlen do
            currlen = currlen+1 ; currcells[currlen] = newrow[i]
        end
        if multistate then
            -- ensure length of currcells is odd
            if currlen & 1 == 0 then currcells[currlen+1] = 0 end
        end
        g.putcells(newrow)
        return currcells
    end
end

--------------------------------------------------------------------------------

-- override SetColors to use Wolfram's color scheme in ANKOS
function SetColors()
    g.setcolors{0,255,255,255}  -- state 0 is white
    if g.numstates() == 2 then
        -- state 1 is black
        g.setcolors{1,0,0,0}
    else
        -- live states vary from gray to black
        g.setcolors{128,128,128, 0,0,0}
    end
    g.setcolor("border", 190, 210, 230)     -- light blue border around grid
    g.setcolor(g.getalgo(), 190, 210, 230)  -- ditto for status bar background
end

--------------------------------------------------------------------------------

-- override RandomPattern to create a single row at the top of the grid
function RandomPattern()
    local rand = math.random
    -- avoid flash due to Refresh call in NewPattern
    local savedRefresh = Refresh
    Refresh = function() end
    NewPattern("random")
    Refresh = savedRefresh
    local minx = -(g.getwidth() // 2)
    local miny = -(g.getheight() // 2)
    local maxx = minx + g.getwidth() - 1
    local perc = GetDensity()
    for x = minx, maxx do
        if rand(0,99) < perc then
            g.setcell(x, miny, rand(1,g.numstates()-1))
        end
    end
    FitGrid()   -- calls Refresh
end

--------------------------------------------------------------------------------

-- user's startup script might want to override this
function RandomRule()
    local rand = math.random
    -- create a random totalistic rule
    local k = rand(2,4)
    local r = rand(1,4)
    local c = rand(0,math.floor(k^((2*r+1)*k-2*r)) - 1)
    return "C"..c.."K"..k.."R"..r
end

--------------------------------------------------------------------------------

-- allow alt-R to create a random pattern with a random totalistic rule
local saveHandleKey = HandleKey
function HandleKey(event)
    local _, key, mods = split(event)
    if key == "r" and mods == "alt" then
        SetRule(RandomRule())
        RandomPattern()
    else
        -- pass the event to the original HandleKey
        saveHandleKey(event)
    end
end

--------------------------------------------------------------------------------

-- and away we go...
StartNewCA()
