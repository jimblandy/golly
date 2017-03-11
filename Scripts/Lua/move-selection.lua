-- Allow user to move the current selection.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Apr 2011.

local g = golly()
local gp = require "gplus"
local split = gp.split

-- set edges of bounded grid for later use
local gridl, gridr, gridt, gridb
if g.getwidth() > 0 then
    gridl = -gp.int(g.getwidth() / 2)
    gridr = gridl + g.getwidth() - 1
end
if g.getheight() > 0 then
    gridt = -gp.int(g.getheight() / 2)
    gridb = gridt + g.getheight() - 1
end

local selrect = g.getselrect()
if #selrect == 0 then g.exit("There is no selection.") end
local selpatt = g.getcells(selrect)
local oldcells = {}

--------------------------------------------------------------------------------

local function showhelp()
    g.note([[
Alt-clicking in the selection allows you to
COPY it to another location (the original
selection is not deleted).

While moving the selection the following keys
can be used:

x  - flip selection left-right
y  - flip selection top-bottom
>  - rotate selection clockwise
<  - rotate selection anticlockwise
escape  - abort and restore the selection]])
end

--------------------------------------------------------------------------------

local function cellinrect(x, y, r)
    -- return true if given cell position is inside given rectangle
    if x < r[1] or x >= r[1] + r[3] then return false end
    if y < r[2] or y >= r[2] + r[4] then return false end
    return true
end

--------------------------------------------------------------------------------

local function rectingrid(r)
    -- return true if all of given rectangle is inside grid
    if g.getwidth() > 0 and (r[1] < gridl or r[1] + r[3] - 1 > gridr) then return false end
    if g.getheight() > 0 and (r[2] < gridt or r[2] + r[4] - 1 > gridb) then return false end
    return true
end

--------------------------------------------------------------------------------

local function lookforkeys(event, deltax, deltay)
    -- look for keys used to flip/rotate selection
    if event == "key x none" or event == "key y none" then
        -- flip floating selection left-right or top-bottom
        if #oldcells > 0 then
            g.clear(0)
            g.putcells(selpatt, deltax, deltay)
        end
        if event == "key x none" then
            g.flip(0)
        else
            g.flip(1)
        end
        selpatt = g.transform(g.getcells(selrect), -deltax, -deltay)
        if #oldcells > 0 then
            g.clear(0)
            g.putcells(oldcells)
            g.putcells(selpatt, deltax, deltay)
        end
        g.update()
        return
    end
    
    if event == "key > none" or event == "key < none" then
        -- rotate floating selection clockwise or anticlockwise;
        -- because we use g.rotate below we have to use the exact same
        -- calculation (see Selection::Rotate in wxselect.cpp) for rotrect
        local midx = selrect[1] + gp.int((selrect[3]-1) / 2)
        local midy = selrect[2] + gp.int((selrect[4]-1) / 2)
        local newleft = midx + selrect[2] - midy
        local newtop = midy + selrect[1] - midx
        local rotrect = { newleft, newtop, selrect[4], selrect[3] }
        if not rectingrid(rotrect) then
            g.show("Rotation is not allowed if selection would be outside grid.")
            return
        end
        g.clear(0)
        if #oldcells > 0 then g.putcells(oldcells) end
        oldcells = g.join(oldcells, g.getcells(rotrect))
        g.clear(0)
        g.select(rotrect)
        g.clear(0)
        g.select(selrect)
        g.putcells(selpatt, deltax, deltay)
        if event == "key > none" then
            g.rotate(0)
        else
            g.rotate(1)
        end
        selrect = g.getselrect()
        selpatt = g.transform(g.getcells(selrect), -deltax, -deltay)
        if #oldcells > 0 then
            g.clear(0)
            g.putcells(oldcells)
            g.putcells(selpatt, deltax, deltay)
        end
        g.update()
        return
    end
    
    if event == "key h none" then
        -- best not to show Help window while dragging selection!
        return
    end
    
    g.doevent(event)
end

--------------------------------------------------------------------------------

function moveselection()
    local x, y, firstx, firsty, xoffset, yoffset, oldmouse

    -- wait for click in selection
    while true do
        local event = g.getevent()
        if event:find("^click") then
            -- event is a string like "click 10 20 left none"
            local evt, xstr, ystr, butt, mods = split(event)
            x = tonumber(xstr)
            y = tonumber(ystr)
            if cellinrect(x, y, selrect) then
                oldmouse = xstr .. ' ' .. ystr
                firstx = x
                firsty = y
                xoffset = firstx - selrect[1]
                yoffset = firsty - selrect[2]
                if mods == "alt" then
                    -- don't delete pattern in selection
                    oldcells = g.getcells(selrect)
                end
                break
            end
        elseif event == "key h none" then
            showhelp()
        else
            g.doevent(event)
        end
    end
    
    -- wait for mouse-up while moving selection
    g.show("Move mouse and release button...")
    local mousedown = true
    local mousepos
    while mousedown do
        local event = g.getevent()
        if event:find("^mup") then
            mousedown = false
        elseif #event > 0 then
            lookforkeys(event, x - firstx, y - firsty)
            -- update xoffset,yoffset in case selection was rotated
            xoffset = x - selrect[1]
            yoffset = y - selrect[2]
        end
        mousepos = g.getxy()
        if #mousepos > 0 and mousepos ~= oldmouse then
            -- mouse has moved, so move selection rect and pattern
            g.clear(0)
            if #oldcells > 0 then g.putcells(oldcells) end
            local xstr, ystr = split(mousepos)
            x = tonumber(xstr)
            y = tonumber(ystr)
            selrect[1] = x - xoffset
            selrect[2] = y - yoffset

            if g.getwidth() > 0 then
                -- ensure selrect doesn't move beyond left/right edge of grid
                if selrect[1] < gridl then
                    selrect[1] = gridl
                    x = selrect[1] + xoffset
                elseif selrect[1] + selrect[3] - 1 > gridr then
                    selrect[1] = gridr + 1 - selrect[3]
                    x = selrect[1] + xoffset
                end
            end
            
            if g.getheight() > 0 then
                -- ensure selrect doesn't move beyond top/bottom edge of grid
                if selrect[2] < gridt then
                    selrect[2] = gridt
                    y = selrect[2] + yoffset
                elseif selrect[2] + selrect[4] - 1 > gridb then
                    selrect[2] = gridb + 1 - selrect[4]
                    y = selrect[2] + yoffset
                end
            end
            
            g.select(selrect)
            oldcells = g.getcells(selrect)
            g.putcells(selpatt, x - firstx, y - firsty)
            oldmouse = mousepos
            g.update()
        end
    end
end
--------------------------------------------------------------------------------

-- remember initial selection in case user aborts script
local firstrect = g.getselrect()
local firstpatt = g.getcells(selrect)

g.show("Click anywhere in selection, move mouse and release button... (hit 'h' for help)")
local oldcursor = g.getcursor()
g.setcursor("Move")

local aborted = true
local status, err = xpcall(function () moveselection() aborted = false end, gp.trace)
if err then g.continue(err) end

g.setcursor(oldcursor)
if aborted then
    g.clear(0)
    if #oldcells > 0 then g.putcells(oldcells) end
    g.putcells(firstpatt)
    g.select(firstrect)
else
    g.show(" ")
end
