-- Fill clicked region with current drawing state.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Apr 2016.

local g = golly()
local gp = require "gplus"

-- avoid an unbounded fill
local minx, miny, maxx, maxy
if g.empty() then
    if g.getwidth() == 0 or g.getheight() == 0 then
        g.exit("You cannot fill an empty universe that is unbounded!")
    end
else
    -- set fill limits to the pattern's bounding box
    -- (these will be extended below if the grid is bounded)
    minx, miny, maxx, maxy = gp.getedges(g.getrect())
end

-- allow filling to extend to the edges of bounded grid
if g.getwidth() > 0 then
    minx = -gp.int(g.getwidth() / 2)
    maxx = minx + g.getwidth() - 1
end
if g.getheight() > 0 then
    miny = -gp.int(g.getheight() / 2)
    maxy = miny + g.getheight() - 1
end

--------------------------------------------------------------------------------

local function checkneighbor(x, y, oldstate)
    -- first check if x,y is outside fill limits
    if x < minx or x > maxx then return false end
    if y < miny or y > maxy then return false end
    return g.getcell(x, y) == oldstate
end

--------------------------------------------------------------------------------

local function floodfill()
    local newstate = g.getoption("drawingstate")
    local oldstate = newstate
    local x, y

    -- wait for user to click a cell
    g.show("Click the region you wish to fill... (hit escape to abort)")
    while oldstate == newstate do
        local event = g.getevent()
        if event:find("click") == 1 then
            -- event is a string like "click 10 20 left none"
            local evt, xstr, ystr, butt, mods = gp.split(event)
            x = tonumber(xstr)
            y = tonumber(ystr)
            if x < minx or x > maxx or y < miny or y > maxy then
                -- click is outside pattern's bounding box in unbounded universe
                g.warn("Click within the pattern's bounding box\n"..
                       "otherwise the fill will be unbounded.")
            else
                -- note that user might have changed drawing state
                newstate = g.getoption("drawingstate")
                oldstate = g.getcell(x, y)
                if oldstate == newstate then
                    g.warn("The clicked cell must have a different state\n"..
                           "to the current drawing state.")
                end
            end
        else
            g.doevent(event)
        end
    end
    -- tell Golly to handle all further keyboard/mouse events
    g.getevent(false)

    -- do flood fill starting with clicked cell
    g.show("Filling clicked region... (hit escape to stop)")
    local clist = {}
    clist[1] = {x, y}
    g.setcell(x, y, newstate)
    local oldsecs = os.clock()
    while #clist > 0 do
        -- remove cell from end of clist (much faster than removing from start)
        x, y = table.unpack( table.remove(clist) )
        
        local newsecs = os.clock()
        if newsecs - oldsecs >= 0.5 then     -- show changed pattern every half second
            oldsecs = newsecs
            g.update()
        end
        
        -- check if any orthogonal neighboring cells are in oldstate
        if checkneighbor(      x, y-1, oldstate) then
            g.setcell(         x, y-1, newstate)
            clist[#clist+1] = {x, y-1}
        end
        if checkneighbor(      x, y+1, oldstate) then
            g.setcell(         x, y+1, newstate)
            clist[#clist+1] = {x, y+1}
        end
        if checkneighbor      (x+1, y, oldstate) then
            g.setcell(         x+1, y, newstate)
            clist[#clist+1] = {x+1, y}
        end
        if checkneighbor(      x-1, y, oldstate) then
            g.setcell(         x-1, y, newstate)
            clist[#clist+1] = {x-1, y}
        end
        
        -- check if any diagonal neighboring cells are in oldstate
        -- (note that we don't want to cross a diagonal line of live cells)
        if checkneighbor(      x+1, y+1, oldstate) and (g.getcell(x, y+1) == 0 or
                                                        g.getcell(x+1, y) == 0) then
            g.setcell(         x+1, y+1, newstate)
            clist[#clist+1] = {x+1, y+1}
        end
        if checkneighbor(      x+1, y-1, oldstate) and (g.getcell(x, y-1) == 0 or
                                                        g.getcell(x+1, y) == 0) then
            g.setcell(         x+1, y-1, newstate)
            clist[#clist+1] = {x+1, y-1}
        end
        if checkneighbor(      x-1, y+1, oldstate) and (g.getcell(x, y+1) == 0 or
                                                        g.getcell(x-1, y) == 0) then
            g.setcell(         x-1, y+1, newstate)
            clist[#clist+1] = {x-1, y+1}
        end
        if checkneighbor(      x-1, y-1, oldstate) and (g.getcell(x, y-1) == 0 or
                                                        g.getcell(x-1, y) == 0) then
            g.setcell(         x-1, y-1, newstate)
            clist[#clist+1] = {x-1, y-1}
        end
    end
end
--------------------------------------------------------------------------------

oldcursor = g.getcursor()
g.setcursor("Draw")

local status, err = pcall(function () floodfill() end)
if err then g.continue(err) end
-- the following code is executed even if error occurred or user aborted script

g.setcursor(oldcursor)
g.show(" ")
