-- Tile current selection with clipboard pattern.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Mar 2016.

local g = golly()
local gp = require "gplus"

-- assume one-state cell array (may change below)
local multistate = false

--------------------------------------------------------------------------------

local function clip_rb(cells, right, bottom)
    -- set given cells except those outside given right and bottom edges
    local len = #cells
    local x = 1
    local y = 2
    if multistate then
        -- ignore padding int if present
        if len % 3 == 1 then len = len - 1 end
        while x <= len do
            if (cells[x] <= right) and (cells[y] <= bottom) then
                g.setcell(cells[x], cells[y], cells[x+2])
            end
            x = x + 3
            y = y + 3
        end
    else   
        while x <= len do
            if (cells[x] <= right) and (cells[y] <= bottom) then
                g.setcell(cells[x], cells[y], 1)
            end
            x = x + 2
            y = y + 2
        end
    end
end

--------------------------------------------------------------------------------

local selrect = g.getselrect()
if #selrect == 0 then g.exit("There is no selection.") end

-- get selection edges
local selleft, seltop, selright, selbottom = gp.getedges(selrect)

local pwidth, pheight, p = g.getclip()

if #p & 1 == 1 then multistate = true end

g.clear(0)
if #p > 0 then
    -- tile selrect with p, clipping right & bottom edges if necessary
    local y = seltop
    while y <= selbottom do
        local bottom = y + pheight - 1
        local x = selleft
        while x <= selright do
            local right = x + pwidth - 1
            if (right <= selright) and (bottom <= selbottom) then
                g.putcells(p, x, y)
            else
                local tempcells = g.transform(p, x, y)
                clip_rb(tempcells, selright, selbottom)
            end
            x = x + pwidth
        end
        y = y + pheight
    end
end
if not g.visrect(selrect) then g.fitsel() end
