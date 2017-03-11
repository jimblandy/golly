-- Allow user to draw one or more straight lines by clicking end points.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Apr 2016.

local g = golly()
local gp = require "gplus"

local oldline = {}
local firstcell = {}    -- pos and state of the 1st cell clicked
local drawstate = g.getoption("drawingstate")

--------------------------------------------------------------------------------

local function drawline(x1, y1, x2, y2)
    -- draw a line of cells from x1,y1 to x2,y2 using Bresenham's algorithm;
    -- we also return the old cells in the line so we can erase line later
    local oldcells = {}
    -- note that x1,y1 has already been drawn
    if x1 == x2 and y1 == y2 then
        g.update()
        return oldcells
    end
    
    local dx = x2 - x1
    local ax = math.abs(dx) * 2
    local sx = 1
    if dx < 0 then sx = -1 end
    
    local dy = y2 - y1
    local ay = math.abs(dy) * 2
    local sy = 1
    if dy < 0 then sy = -1 end

    if ax > ay then
        local d = ay - (ax / 2)
        while x1 ~= x2 do
            oldcells[#oldcells+1] = {x1, y1, g.getcell(x1, y1)}
            g.setcell(x1, y1, drawstate)
            if d >= 0 then
                y1 = y1 + sy
                d = d - ax
            end
            x1 = x1 + sx
            d = d + ay
        end
    else
        local d = ax - (ay / 2)
        while y1 ~= y2 do
            oldcells[#oldcells+1] = {x1, y1, g.getcell(x1, y1)}
            g.setcell(x1, y1, drawstate)
            if d >= 0 then
                x1 = x1 + sx
                d = d - ay
            end
            y1 = y1 + sy
            d = d + ax
        end
    end
    
    oldcells[#oldcells+1] = {x2, y2, g.getcell(x2, y2)}
    g.setcell(x2, y2, drawstate)
    g.update()
    return oldcells
end

--------------------------------------------------------------------------------

local function eraseline(oldcells)
    for _, t in ipairs(oldcells) do
        g.setcell( table.unpack(t) )
    end
end

--------------------------------------------------------------------------------

function drawlines()
    local started = false
    local oldmouse = ""
    local startx, starty, endx, endy
    while true do
        local event = g.getevent()
        if event:find("click") == 1 then
            -- event is a string like "click 10 20 left altctrlshift"
            local evt, x, y, butt, mods = gp.split(event)
            oldmouse = x .. ' ' .. y
            if started then
                -- draw permanent line from start pos to end pos
                endx = tonumber(x)
                endy = tonumber(y)
                drawline(startx, starty, endx, endy)
                -- this is also the start of another line
                startx = endx
                starty = endy
                oldline = {}
                firstcell = {}
            else
                -- start first line
                startx = tonumber(x)
                starty = tonumber(y)
                firstcell = { startx, starty, g.getcell(startx, starty) }
                g.setcell(startx, starty, drawstate)
                g.update()
                started = true
                g.show("Click where to end this line (and start another line)...")
            end
        else
            -- event might be "" or "key m none"
            if #event > 0 then g.doevent(event) end
            local mousepos = g.getxy()
            if started and #mousepos == 0 then
                -- erase old line if mouse is not over grid
                if #oldline > 0 then
                    eraseline(oldline)
                    oldline = {}
                    g.update()
                end
            elseif started and #mousepos > 0 and mousepos ~= oldmouse then
                -- mouse has moved, so erase old line (if any) and draw new line
                if #oldline > 0 then eraseline(oldline) end
                local x, y = gp.split(mousepos)
                oldline = drawline(startx, starty, tonumber(x), tonumber(y))
                oldmouse = mousepos
            end
        end
    end
end

--------------------------------------------------------------------------------

g.show("Click where to start line...")
local oldcursor = g.getcursor()
g.setcursor("Draw")

local status, err = xpcall(drawlines, gp.trace)
if err then g.continue(err) end
-- the following code is executed even if error occurred or user aborted script

g.setcursor(oldcursor)
if #oldline > 0 then eraseline(oldline) end
if #firstcell > 0 then
    local x, y, s = table.unpack(firstcell)
    g.setcell(x, y, s)
end