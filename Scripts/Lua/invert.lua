-- Invert all cell states in the current selection.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Mar 2016.

local g = golly()
local gp = require "gplus"

-- re-assigning inner loop functions results in a 10% speed up
local setcell = g.setcell
local getcell = g.getcell

local r = gp.rect(g.getselrect())
if r.empty then g.exit("There is no selection.") end

-- local t1 = os.clock()

local oldsecs = os.clock()
local maxstate = g.numstates() - 1

for row = r.top, r.bottom do
    -- if large selection then give some indication of progress
    local newsecs = os.clock()
    if newsecs - oldsecs >= 1.0 then
        oldsecs = newsecs
        g.update()
    end
    for col = r.left, r.right do
        setcell(col, row, maxstate - getcell(col, row))
    end
end

if not r.visible() then g.fitsel() end

-- g.show(""..(os.clock()-t1))
