-- Use the current selection to create a toroidal universe.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Apr 2016.

local g = golly()

local selrect = g.getselrect()
if #selrect == 0 then g.exit("There is no selection.") end

local x, y, wd, ht = table.unpack(selrect)
local selcells = g.getcells(selrect)

if not g.empty() then
    g.clear(0)
    g.clear(1)
end

-- get current rule, remove any existing suffix, then add new suffix
local rule = g.getrule()
rule = rule:match("^(.+):") or rule
g.setrule(string.format("%s:T%d,%d", rule, wd, ht))

local newx = -math.floor(wd/2)
local newy = -math.floor(ht/2)
selrect[1] = newx
selrect[2] = newy
g.select(selrect)
if #selcells > 0 then g.putcells(selcells, newx - x, newy - y) end
g.fitsel()
