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

function floodfill()
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
    
    g.show("Filling clicked region... (hit escape to stop)")
    
    -- do flood fill starting with clicked cell using a scanline algorithm
    local oldsecs = os.clock()
    local clist = {}
    clist[1] = {x, y}
    while #clist > 0 do
        -- remove cell from end of clist (much faster than removing from start)
        x, y = table.unpack( table.remove(clist) )
        local above = false
        local below = false
        while x >= minx and g.getcell(x, y) == oldstate do
            x = x-1
        end
        x = x+1
        while x <= maxx and g.getcell(x, y) == oldstate do
            g.setcell(x, y, newstate)
            if (not above) and y > miny and g.getcell(x, y-1) == oldstate then
                clist[#clist+1] = {x, y-1}
                above = true
            elseif above and y > miny and g.getcell(x, y-1) ~= oldstate then
                above = false
            end
            if (not below) and y < maxy and g.getcell(x, y+1) == oldstate then
                clist[#clist+1] = {x, y+1}
                below = true
            elseif below and y < maxy and g.getcell(x, y+1) ~= oldstate then
                below = false
            end
            x = x+1
        end
        local newsecs = os.clock()
        if newsecs - oldsecs >= 0.5 then    -- show changed pattern every half second
            oldsecs = newsecs
            g.update()
        end
    end
end
--------------------------------------------------------------------------------

oldcursor = g.getcursor()
g.setcursor("Draw")

local status, err = xpcall(floodfill, gp.trace)
if err then g.continue(err) end
-- the following code is executed even if error occurred or user aborted script

g.setcursor(oldcursor)
g.show(" ")
