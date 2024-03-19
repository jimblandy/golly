-- Go to a requested generation.  The given generation can be an
-- absolute number like 1,000,000 (commas are optional) or a number
-- relative to the current generation like +9 or -6.  If the target
-- generation is less than the current generation then we go back
-- to the starting generation (normally 0) and advance to the target.
-- Authors: Andrew Trevorrow and Dave Greene, Mar 2024.

local g = golly()
local gp = require "gplus"
local validint = gp.validint

--------------------------------------------------------------------------------

local function intbase(n, b)
    -- convert integer n >= 0 to a base b digit array (thanks to PM 2Ring)
    digits = {}
    while n > 0 do
        digits[#digits + 1] = n % b
        n = math.floor(n / b)
    end
    if #digits == 0 then digits = {0} end
    return digits
end

--------------------------------------------------------------------------------

-- math.maxinteger is a 19-digit number on a 64-bit system, but we have to
-- limit gen counts to 16-digit numbers to avoid overflow problems
local maxdigits = #tostring(math.maxinteger) - 3

local function check_number(numstr)
    if #numstr > maxdigits then
        g.exit("Number is too large for Lua -- use goto.py instead.")
    end
    return tonumber(numstr)
end

--------------------------------------------------------------------------------

local function go_to(gen)
    local currgen = check_number(g.getgen())
    local newgen
    if gen:sub(1,1) == '+' then
        newgen = currgen + check_number(gen:sub(2,-1))
    elseif gen:sub(1,1) == '-' then
        local n = check_number(gen:sub(2,-1))
        if currgen > n then
            newgen = currgen - n
        else
            newgen = 0
        end
    else
        newgen = check_number(gen)
    end
    
    if newgen < currgen then
        -- try to go back to starting gen (not necessarily 0) and
        -- then forwards to newgen; note that reset() also restores
        -- algorithm and/or rule, so too bad if user changed those
        -- after the starting info was saved;
        -- first save current location and scale
        local midx, midy = g.getpos()
        local mag = g.getmag()
        g.reset()
        -- restore location and scale
        g.setpos(midx, midy)
        g.setmag(mag)
        -- current gen might be > 0 if user loaded a pattern file
        -- that set the gen count
        currgen = check_number(g.getgen())
        if newgen < currgen then
            g.error("Can't go back any further; pattern was saved "..
                    "at generation "..currgen..".")
            return
        end
    end
    if newgen == currgen then return end
    
    g.show("Hit escape to abort...")
    local oldsecs = os.clock()
    
    -- before stepping we advance by 1 generation, for two reasons:
    -- 1. if we're at the starting gen then the *current* step size
    --    will be saved (and restored upon Reset/Undo) rather than a
    --    possibly very large step size
    -- 2. it increases the chances the user will see updates and so
    --    get some idea of how long the script will take to finish
    --    (otherwise if the base is 10 and a gen like 1,000,000,000
    --    is given then only a single step() of 10^9 would be done)
    g.run(1)
    currgen = currgen + 1

    -- use fast stepping (thanks to PM 2Ring)
    for i, d in ipairs(intbase(newgen - currgen, g.getbase())) do
        if d > 0 then
            g.setstep(i-1)
            for j = 1, d do
                if g.empty() then
                    g.show("Pattern is empty.")
                    return
                end
                g.step()
                local newsecs = os.clock()
                if newsecs - oldsecs >= 1.0 then
                    -- do an update every sec
                    oldsecs = newsecs
                    g.update()
                end
            end
        end
    end
    g.show("")
end

--------------------------------------------------------------------------------

local function savegen(filename, gen)
    local f = io.open(filename, "w")
    if f then
        f:write(gen)
        f:close()
    else
        g.warn("Can't save gen in filename:\n"..filename)
    end
end

--------------------------------------------------------------------------------

-- use same file name as in goto.py
local GotoINIFileName = g.getdir("data").."goto.ini"
local previousgen = ""
local f = io.open(GotoINIFileName, "r")
if f then
    previousgen = f:read("*l") or ""
    f:close()
end

local gen = g.getstring("Enter the desired generation number,\n"..
                        "or -n/+n to go back/forwards by n:",
                        previousgen, "Go to generation")
if gen == "" then
    g.exit()
elseif gen == '+' or gen == '-' then
    -- clear the default
    savegen(GotoINIFileName, "")
elseif not validint(gen) then
    g.exit("Sorry, but \""..gen.."\" is not a valid integer.")
else
    -- best to save given gen now in case user aborts script
    savegen(GotoINIFileName, gen)
    local oldstep = g.getstep()
    go_to(gen:gsub(",",""))
    g.setstep(oldstep)
end
