-- Shift current selection by given x y amounts using optional mode.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Mar 2016.

local g = golly()
local gp = require "gplus"

local selrect = g.getselrect()
if #selrect == 0 then g.exit("There is no selection.") end

-- use same file name as in shift.py
local inifilename = g.getdir("data").."shift.ini"
local oldparams = "0 0 or"
local f = io.open(inifilename, "r")
if f then
    -- get the parameters given last time
    oldparams = f:read("*l") or ""
    f:close()
end

local s = g.getstring("Enter x y shift amounts and an optional mode\n"..
                      "(valid modes are copy/or/xor, default is or):",
                      oldparams, "Shift selection")
local x, y, mode = gp.split(s)

-- check x and y
if y == nil then g.exit("Enter x and y amounts separated by a space.") end
if not gp.validint(x) then g.exit("Bad x value: "..x) end
if not gp.validint(y) then g.exit("Bad y value: "..y) end
x = tonumber(x)
y = tonumber(y)

-- check optional mode
if mode == nil then
    mode = "or"
else
    mode = string.lower(mode)
    if mode == "c" then mode = "copy" end
    if mode == "o" then mode = "or" end
    if mode == "x" then mode = "xor" end
    if not (mode == "copy" or mode == "or" or mode == "xor") then
        g.exit("Unknown mode: "..mode.." (must be copy/or/xor)")
    end
end

-- given parameters are valid so save them for next run
f = io.open(inifilename, "w")
if f then
    f:write(s)
    f:close()
end

-- abort shift if the new selection would be outside a bounded grid
if g.getwidth() > 0 then
    local gridl = -g.getwidth()/2
    local gridr = gridl + g.getwidth() - 1
    local newl = selrect[1] + x
    local newr = newl + selrect[3] - 1
    if newl < gridl or newr > gridr then
        g.exit("New selection would be outside grid.")
    end
end
if g.getheight() > 0 then
    local gridt = -g.getheight()/2
    local gridb = gridt + g.getheight() - 1
    local newt = selrect[2] + y
    local newb = newt + selrect[4] - 1
    if newt < gridt or newb > gridb then
        g.exit("New selection would be outside grid.")
    end
end

-- do the shift by cutting the current selection and pasting it into
-- the new position without changing the current clipboard pattern
local selcells = g.getcells(selrect)
g.clear(0)
selrect[1] = selrect[1] + x
selrect[2] = selrect[2] + y
g.select(selrect)
if mode == "copy" then g.clear(0) end
g.putcells(selcells, x, y, 1, 0, 0, 1, mode)

if not g.visrect(selrect) then g.fitsel() end
