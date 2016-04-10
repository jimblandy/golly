-- Invert all cell states in the current selection.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Mar 2016.

local g = gollylib()
local gp = require "gpackage"

local r = gp.rect(g.getselrect())
if r.empty then g.exit("There is no selection.") end

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
        g.setcell(col, row, maxstate - g.getcell(col, row))
    end
end

if not r.visible() then g.fitsel() end
