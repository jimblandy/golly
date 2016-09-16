-- Tile current selection with pattern inside selection.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Mar 2016.

local g = golly()
local gp = require "gplus"

local selrect = g.getselrect()
if #selrect == 0 then g.exit("There is no selection.") end

local selpatt = g.getcells(selrect)
if #selpatt == 0 then g.exit("No pattern in selection.") end

-- determine if selpatt is one-state or multi-state
local inc = 2
if (#selpatt & 1) == 1 then inc = 3 end

--------------------------------------------------------------------------------

local function clip_left(cells, left)
    local len = #cells
    local x = 1
    if inc == 3 then
        -- ignore padding int if present
        if len % 3 == 1 then len = len - 1 end
        while x <= len do
            if cells[x] >= left then
                g.setcell(cells[x], cells[x+1], cells[x+2])
            end
            x = x + 3
        end
    else
        while x <= len do
            if cells[x] >= left then
                g.setcell(cells[x], cells[x+1], 1)
            end
            x = x + 2
        end
    end
end

--------------------------------------------------------------------------------

local function clip_right(cells, right)
    local len = #cells
    local x = 1
    if inc == 3 then
        -- ignore padding int if present
        if len % 3 == 1 then len = len - 1 end
        while x <= len do
            if cells[x] <= right then
                g.setcell(cells[x], cells[x+1], cells[x+2])
            end
            x = x + 3
        end
    else   
        while x <= len do
            if cells[x] <= right then
                g.setcell(cells[x], cells[x+1], 1)
            end
            x = x + 2
        end
    end
end

--------------------------------------------------------------------------------

local function clip_top(cells, top)
    local len = #cells
    local y = 2
    if inc == 3 then
        -- ignore padding int if present
        if len % 3 == 1 then len = len - 1 end
        while y <= len do
            if cells[y] >= top then
                g.setcell(cells[y-1], cells[y], cells[y+1])
            end
            y = y + 3
        end
    else   
        while y <= len do
            if cells[y] >= top then
                g.setcell(cells[y-1], cells[y], 1)
            end
            y = y + 2
        end
    end
end

--------------------------------------------------------------------------------

local function clip_bottom(cells, bottom)
    local len = #cells
    local y = 2
    if inc == 3 then
        -- ignore padding int if present
        if len % 3 == 1 then len = len - 1 end
        while y <= len do
            if cells[y] <= bottom then
                g.setcell(cells[y-1], cells[y], cells[y+1])
            end
            y = y + 3
        end
    else   
        while y <= len do
            if cells[y] <= bottom then
                g.setcell(cells[y-1], cells[y], 1)
            end
            y = y + 2
        end
    end
end

--------------------------------------------------------------------------------

-- get selection edges
local selleft, seltop, selright, selbottom = gp.getedges(selrect)

-- find selpatt's minimal bounding box
local bbox = gp.getminbox(selpatt)
local i

-- first tile selpatt horizontally, clipping where necessary
local left = bbox.x
local right = left + bbox.wd - 1
i = 0
while left > selleft do
    left = left - bbox.wd
    i = i + 1
    if left >= selleft then
        g.putcells(selpatt, -bbox.wd * i, 0)
    else
        local tempcells = g.transform(selpatt, -bbox.wd * i, 0)
        clip_left(tempcells, selleft)
    end
end
i = 0
while right < selright do
    right = right + bbox.wd
    i = i + 1
    if right <= selright then
        g.putcells(selpatt, bbox.wd * i, 0)
    else
        local tempcells = g.transform(selpatt, bbox.wd * i, 0)
        clip_right(tempcells, selright)
    end
end

-- get new selection pattern and tile vertically, clipping where necessary
selpatt = g.getcells(selrect)
bbox = gp.getminbox(selpatt)
local top = bbox.y
local bottom = top + bbox.ht - 1
i = 0
while top > seltop do
    top = top - bbox.ht
    i = i + 1
    if top >= seltop then
        g.putcells(selpatt, 0, -bbox.ht * i)
    else
        local tempcells = g.transform(selpatt, 0, -bbox.ht * i)
        clip_top(tempcells, seltop)
    end
end
i = 0
while bottom < selbottom do
    bottom = bottom + bbox.ht
    i = i + 1
    if bottom <= selbottom then
        g.putcells(selpatt, 0, bbox.ht * i)
    else
        local tempcells = g.transform(selpatt, 0, bbox.ht * i)
        clip_bottom(tempcells, selbottom)
    end
end

if not g.visrect(selrect) then g.fitsel() end
