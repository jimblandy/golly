-- Invert all cell states in the current selection.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Mar 2016.

local g = gollylib()

local rect = g.getselrect()
if #rect == 0 then g.exit("There is no selection.") end

local left = rect[1]
local top = rect[2]
local right = left + rect[3] - 1
local bottom = top + rect[4] - 1

local oldsecs = os.clock()
local maxstate = g.numstates() - 1

for row = top, bottom do
    -- if large selection then give some indication of progress
    local newsecs = os.clock()
    if newsecs - oldsecs >= 1.0 then
        oldsecs = newsecs
        g.update()
    end
    for col = left, right do
        g.setcell(col, row, maxstate - g.getcell(col, row))
    end
end

if not g.visrect(rect) then g.fitsel() end
