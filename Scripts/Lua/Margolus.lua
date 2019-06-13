--[[
This script lets you explore rules using the Margolus neighborhood.
Author: Andrew Trevorrow (andrew@trevorrow.com), May 2019.
--]]

local g = golly()
local gp = require "gplus"
local split = gp.split

require "gplus.NewCA"
SCRIPT_NAME = "Margolus"
DEFAULT_RULE = "M0,8,4,3,2,5,9,7,1,6,10,11,12,13,14,15"     -- BBM
RULE_HELP = [[
This script lets you explore rules using the Margolus neighborhood.
Rule strings are of the form Mn,n,n,n,n,n,n,n,n,n,n,n,n,n,n,n
where there must be 16 numbers with values from 0 to 15.
<p>
You can also enter rules using MCell's syntax (MS,Dn;n;n;n...).
Or you can enter one of the following aliases (case must match):
<p>
<center>
<table cellspacing=1 border=2 cols=2 width="90%%">
<tr><td align=right> Alias    &nbsp;</td><td>&nbsp; Rule </td></tr>
<tr><td align=right> bbm      &nbsp;</td><td>&nbsp; M0,8,4,3,2,5,9,7,1,6,10,11,12,13,14,15 </td></tr>
<tr><td align=right> critters &nbsp;</td><td>&nbsp; M15,14,13,3,11,5,6,1,7,9,10,2,12,4,8,0 </td></tr>
<tr><td align=right> tron     &nbsp;</td><td>&nbsp; M15,1,2,3,4,5,6,7,8,9,10,11,12,13,14,0 </td></tr>
</table>
</center>
<p>
For more details about the Margolus neighborhood see this link:<br>
<a href="http://www.mirekw.com/ca/rullex_marg.html"
        >http://www.mirekw.com/ca/rullex_marg.html</a>
]]

-- the following are non-local so a startup script can change them
DEFWD, DEFHT = 500, 500         -- default grid size
aliases = {
    bbm =      "M0,8,4,3,2,5,9,7,1,6,10,11,12,13,14,15",
    critters = "M15,14,13,3,11,5,6,1,7,9,10,2,12,4,8,0",
    tron =     "M15,1,2,3,4,5,6,7,8,9,10,11,12,13,14,0",
}

--------------------------------------------------------------------------------

NextPattern = function() end    -- ParseRule sets this to SlowPattern or FastPattern
local transition = {}           -- transition table set by ParseRule and used in NextPattern
local even_transition = {}      -- transition table for even generations
local odd_transition = {}       -- transition table for odd generations
local alternate_rules = false   -- FastPattern uses even_transition and odd_transition?

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
    if prefix:sub(1,1) ~= "M" then
        return "Rule must start with M."
    end
    local blocknum = {}
    local i = 0
    -- note that we append a comma to prefix to simplify the gmatch pattern
    for n in string.gmatch(prefix..",", "(%d+)[,;]") do
        if tonumber(n) > 15 then 
            return "Bad number: "..n.." (must be from 0 to 15)."
        end
        blocknum[i] = tonumber(n)
        i = i + 1
        if i > 16 then break end
    end
    if i ~= 16 then
        return "Rule must specify 16 comma-separated numbers from 0 to 15."
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
    if wd < 8 then wd = 8 elseif wd > 1000 then wd = 1000 end
    if ht < 8 then ht = 8 elseif ht > 1000 then ht = 1000 end
    
    -- ensure wd and ht are multiples of 4 to simplify NextPattern logic
    wd = wd - (wd % 4)
    ht = ht - (ht % 4)
    
    -- given rule is valid
    
    -- create the transition table for use in NextPattern
    transition = {}
    for i = 0, 15 do transition[i] = blocknum[i] end

    -- create the canonical form of the given rule
    local canonrule = "M"
    for i = 0, 15 do
        canonrule = canonrule..blocknum[i]
        if i < 15 then canonrule = canonrule.."," end
    end
    canonrule = canonrule..":T"..wd..","..ht
    
    if transition[0] == 0 then
        NextPattern = FastPattern
    else
        NextPattern = SlowPattern
    end

    alternate_rules = false
    if transition[0] == 15 and transition[15] == 0 then
        -- for rules like Critters and Tron we can avoid strobing and use
        -- FastPattern with alternating transition tables, eg:
        --         i: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
        --  Critters: 15,14,13,3, 11,5, 6, 1, 7, 9, 10, 2,  12, 4,  8,  0
        -- even gens: 0, 1, 2, 12,4, 10,9, 14,8, 6, 5,  13, 3,  11, 7,  15
        --  odd gens: 0, 8, 4, 12,2, 10,9, 7, 1, 6, 5,  11, 3,  13, 14, 15
        alternate_rules = true
        NextPattern = FastPattern
        even_transition = {}
        odd_transition = {}
        for i = 0, 15 do
            even_transition[i] = 15 - transition[i]     -- inverse
            if i == transition[i] then
                odd_transition[i] = even_transition[i]
            elseif even_transition[i] == 0 then odd_transition[i] = 0
            elseif even_transition[i] == 15 then odd_transition[i] = 15
            elseif even_transition[i] == 1 then odd_transition[i] = 8
            elseif even_transition[i] == 8 then odd_transition[i] = 1
            elseif even_transition[i] == 2 then odd_transition[i] = 4
            elseif even_transition[i] == 4 then odd_transition[i] = 2
            elseif even_transition[i] == 3 then odd_transition[i] = 5
            elseif even_transition[i] == 5 then odd_transition[i] = 3
            elseif even_transition[i] == 6 then odd_transition[i] = 9
            elseif even_transition[i] == 9 then odd_transition[i] = 6
            elseif even_transition[i] == 7 then odd_transition[i] = 14
            elseif even_transition[i] == 14 then odd_transition[i] = 7
            elseif even_transition[i] == 10 then odd_transition[i] = 12
            elseif even_transition[i] == 12 then odd_transition[i] = 10
            elseif even_transition[i] == 11 then odd_transition[i] = 13
            elseif even_transition[i] == 13 then odd_transition[i] = 11
            end
        end
    end
    
    return nil, canonrule, wd, ht, 2
end

--------------------------------------------------------------------------------

function FastPattern(currcells, minx, miny, maxx, maxy)
    -- Create the next pattern when transition[0] == 0 or when using
    -- alternating rules.

    local newcells = {}         -- cell array for the new pattern (one-state)
    local newlen = 0            -- length of newcells
    local blocks = {}           -- has all 2x2 blocks with at least 1 live cell
    local gridwd = maxx-minx+1
    local get = g.getcell
    local oddgen = GetGenCount() & 1 == 1
    
    if alternate_rules then
        if oddgen then
            transition = odd_transition
        else
            transition = even_transition
        end
    end

    for i = 1, #currcells, 2 do -- currcells is a one-state cell array
        local x = currcells[i]
        local y = currcells[i+1]
        -- find the top left cell in the 2x2 block to which this live cell belongs
        local n
        if oddgen then
            -- odd-numbered generation so top left cell in block must have odd coords
            local oddx = x & 1 == 1
            local oddy = y & 1 == 1
            if oddx and oddy then
                n = 1
            elseif not oddx and oddy then
                n = 2
                x = x-1
                if x < minx then x = maxx end
            elseif oddx and not oddy then
                n = 4
                y = y-1
                if y < miny then y = maxy end
            else -- x,y both even
                n = 8
                x = x-1
                y = y-1
                if x < minx then x = maxx end
                if y < miny then y = maxy end
            end
        else
            -- even-numbered generation so top left cell in block must have even coords
            -- (note that ParseRule ensures both grid dimensions are multiples of 4,
            -- so wrapping won't be needed and minx,miny are always even numbers)
            local evenx = x & 1 == 0
            local eveny = y & 1 == 0
            if evenx and eveny then
                n = 1
            elseif not evenx and eveny then
                n = 2
                x = x-1
            elseif evenx and not eveny then
                n = 4
                y = y-1
            else -- x,y both odd
                n = 8
                x = x-1
                y = y-1
            end
        end
        -- x,y is now the top left cell in a non-empty 2x2 block
        local k = (y-miny) * gridwd + (x-minx)
        blocks[k] = (blocks[k] or 0) + n
    end

    -- go thru all the 2x2 blocks created above and put the transitions in newcells
    for k,v in pairs(blocks) do
        local x = minx + (k % gridwd)
        local y = miny + (k // gridwd)
        -- x,y is the top left cell in this 2x2 block
        local xp1 = x+1
        local yp1 = y+1
        if oddgen then
            -- may need to wrap right and bottom edges
            if xp1 > maxx then xp1 = minx end
            if yp1 > maxy then yp1 = miny end
        end
        local n = transition[v]
        if n > 0 then
            if n & 1 == 1 then
                -- n is 1,3,5,7,9,11,13,15
                newlen = newlen+1 ; newcells[newlen] = x
                newlen = newlen+1 ; newcells[newlen] = y
            end
            if n == 2 or n == 3 or n == 6 or n == 7 or
               n == 10 or n == 11 or n == 14 or n == 15 then
                newlen = newlen+1 ; newcells[newlen] = xp1
                newlen = newlen+1 ; newcells[newlen] = y
            end
            if (n >= 4 and n <= 7) or (n >= 12 and n <= 15) then
                newlen = newlen+1 ; newcells[newlen] = x
                newlen = newlen+1 ; newcells[newlen] = yp1
            end
            if n >= 8 then
                newlen = newlen+1 ; newcells[newlen] = xp1
                newlen = newlen+1 ; newcells[newlen] = yp1
            end
        end
    end

    -- delete the old pattern and add the new pattern
    g.putcells(currcells, 0, 0, 1, 0, 0, 1, "xor")    
    g.putcells(newcells)

    return newcells     -- return the new pattern
end

--------------------------------------------------------------------------------

function SlowPattern(currcells, minx, miny, maxx, maxy)
    -- Create the next pattern when transition[0] > 0 and we have to examine
    -- every cell in the grid (so very slow).

    local newcells = {}         -- cell array for the new pattern (one-state)
    local newlen = 0            -- length of newcells
    local get = g.getcell
    local oddgen = GetGenCount() & 1 == 1

    local xfirst, yfirst, xlast, ylast
    if oddgen then
        -- odd-numbered generation
        xfirst = minx+1
        yfirst = miny+1
        xlast = maxx
        ylast = maxy
    else
        -- even-numbered generation (note that ParseRule ensures both grid
        -- dimensions are multiples of 4, so wrapping won't be needed and
        -- minx,miny are always even numbers)
        xfirst = minx
        yfirst = miny
        xlast = maxx-1
        ylast = maxy-1
    end

    for y = yfirst, ylast, 2 do
        local yp1 = y+1
        -- may need to wrap bottom edge
        if oddgen and yp1 > maxy then yp1 = miny end
        for x = xfirst, xlast, 2 do
            local xp1 = x+1
            -- may need to wrap right edge
            if oddgen and xp1 > maxx then xp1 = minx end
            local i = get(x, y)
            i = i + 2 * get(xp1, y)
            i = i + 4 * get(x,   yp1)
            i = i + 8 * get(xp1, yp1)
            local n = transition[i]
            if n > 0 then
                if n & 1 == 1 then
                    -- n is 1,3,5,7,9,11,13,15
                    newlen = newlen+1 ; newcells[newlen] = x
                    newlen = newlen+1 ; newcells[newlen] = y
                end
                if n == 2 or n == 3 or n == 6 or n == 7 or
                   n == 10 or n == 11 or n == 14 or n == 15 then
                    newlen = newlen+1 ; newcells[newlen] = xp1
                    newlen = newlen+1 ; newcells[newlen] = y
                end
                if (n >= 4 and n <= 7) or (n >= 12 and n <= 15) then
                    newlen = newlen+1 ; newcells[newlen] = x
                    newlen = newlen+1 ; newcells[newlen] = yp1
                end
                if n >= 8 then
                    newlen = newlen+1 ; newcells[newlen] = xp1
                    newlen = newlen+1 ; newcells[newlen] = yp1
                end
            end
        end
    end

    -- delete the old pattern and add the new pattern
    g.putcells(currcells, 0, 0, 1, 0, 0, 1, "xor")
    g.putcells(newcells)

    return newcells     -- return the new pattern
end

--------------------------------------------------------------------------------

-- override the SetColors function
function SetColors()
    g.setcolors( {0,0,0,0} )        -- state 0 is black
    g.setcolors( {1,255,255,0} )    -- state 1 is yellow
end

--------------------------------------------------------------------------------

-- user's startup script might want to override this
function RandomRule()
    local rand = math.random
    local rule = "M0,"  -- avoid strobing rules
    for i = 1, 14 do
        rule = rule..rand(0,15)..","
    end
    return rule..rand(0,15)
end

--------------------------------------------------------------------------------

-- allow alt-R to create a random pattern with a random rule
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
