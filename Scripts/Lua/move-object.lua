-- Allow user to move a connected group of live cells.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Jan 2011.

local g = golly()
local gp = require "gplus"
local int = gp.int
local split = gp.split
local rect = gp.rect
local getminbox = gp.getminbox

-- set edges of bounded grid for later use
local gridl, gridr, gridt, gridb
if g.getwidth() > 0 then
    gridl = -int(g.getwidth()/2)
    gridr = gridl + g.getwidth() - 1
end
if g.getheight() > 0 then
    gridt = -int(g.getheight()/2)
    gridb = gridt + g.getheight() - 1
end

local ncells = {}       -- list of neighboring live cells
local oldcells = {}     -- cells under moved object
local object = {}       -- cells in moving object
local object1 = {}      -- cells in initial object

--------------------------------------------------------------------------------

local function showhelp()
    g.note([[
Alt-clicking on an object allows you to COPY
it to another location (the original object
is not deleted).

While dragging the object the following keys
can be used:

x  - flip object left-right
y  - flip object top-bottom
>  - rotate object clockwise
<  - rotate object anticlockwise
escape  - abort and restore the object]])
end

--------------------------------------------------------------------------------

local function getstate(x, y)
    -- first check if x,y is outside bounded grid
    if g.getwidth() > 0 and (x < gridl or x > gridr) then return 0 end
    if g.getheight() > 0 and (y < gridt or y > gridb) then return 0 end
    return g.getcell(x, y)
end

--------------------------------------------------------------------------------

local function findlivecell(x, y)
    if g.getcell(x, y) > 0 then return {x, y} end

    -- spiral outwards from x,y looking for a nearby live cell;
    -- the smaller the scale the smaller the area searched
    local maxd = 10
    local mag = g.getmag()
    if mag > 0 then
        -- mag can be 1..5 (ie. scales 1:2 to 1:32)
        maxd = 2 * (6 - mag)    -- 10, 8, 6, 4, 2
    end
    local d = 1
    while d <= maxd do
        x = x - 1
        y = y - 1
        for i = 1, 2*d do
            x = x + 1   -- move east
            if getstate(x, y) > 0 then return {x, y} end
        end
        for i = 1, 2*d do
            y = y + 1   -- move south
            if getstate(x, y) > 0 then return {x, y} end
        end
        for i = 1, 2*d do
            x = x - 1   -- move west
            if getstate(x, y) > 0 then return {x, y} end
        end
        for i = 1, 2*d do
            y = y - 1   -- move north
            if getstate(x, y) > 0 then return {x, y} end
        end
        d = d + 1
    end
    return {}   -- failed to find a live cell
end

--------------------------------------------------------------------------------

local function checkneighbor(x, y)
    -- first check if x,y is outside bounded grid
    if g.getwidth() > 0 and (x < gridl or x > gridr) then return end
    if g.getheight() > 0 and (y < gridt or y > gridb) then return end
    if g.getcell(x, y) == 0 then return end
    ncells[#ncells + 1] = {x, y, g.getcell(x,y)}
    g.setcell(x, y, 0)
end

--------------------------------------------------------------------------------

local function getobject(x, y)
    local object = {}
    ncells[#ncells + 1] = {x, y, g.getcell(x,y)}
    g.setcell(x, y, 0)
    while #ncells > 0 do
        -- remove cell from end of ncells and append to object
        local x, y, s = table.unpack( table.remove(ncells) )
        object[#object + 1] = x
        object[#object + 1] = y
        object[#object + 1] = s
        -- add any live neighbors to ncells
        checkneighbor(x  , y+1)
        checkneighbor(x  , y-1)
        checkneighbor(x+1, y  )
        checkneighbor(x-1, y  )
        checkneighbor(x+1, y+1)
        checkneighbor(x+1, y-1)
        checkneighbor(x-1, y+1)
        checkneighbor(x-1, y-1)
    end
    -- append padding int if necessary
    if #object > 0 and (#object & 1) == 0 then object[#object + 1] = 0 end
    g.putcells(object)
    return object
end

--------------------------------------------------------------------------------

local function underneath(object)
    -- return array of live cells underneath given object (a multi-state array)
    local cells = {}
    local objlen = #object
    if objlen % 3 == 1 then objlen = objlen - 1 end    -- ignore padding int
    local i = 1
    while i <= objlen do
        local x = object[i]
        local y = object[i+1]
        local s = g.getcell(x, y)
        if s > 0 then
            cells[#cells + 1] = x
            cells[#cells + 1] = y
            cells[#cells + 1] = s
        end
        i = i + 3
    end
    -- append padding int if necessary
    if #cells > 0 and (#cells & 1) == 0 then cells[#cells + 1] = 0 end
    return cells
end

--------------------------------------------------------------------------------

local function rectingrid(r)
    -- return true if all of given rectangle is inside grid
    if g.getwidth() > 0 and (r[1] < gridl or r[1] + r[3] - 1 > gridr) then return false end
    if g.getheight() > 0 and (r[2] < gridt or r[2] + r[4] - 1 > gridb) then return false end
    return true
end

--------------------------------------------------------------------------------

local function lookforkeys(event)
    -- look for keys used to flip/rotate object
    if event == "key x none" or event == "key y none" then
        -- flip floating object left-right or top-bottom
        g.putcells(object, 0, 0, 1, 0, 0, 1, "xor")  -- erase object
        if #oldcells > 0 then g.putcells(oldcells) end
        local obox = getminbox(object)
        if event == "key x none" then
            -- translate object so that bounding box doesn't change
            local xshift = 2 * (obox.left + int(obox.wd/2))
            if obox.wd % 2 == 0 then xshift = xshift - 1 end
            object = g.transform(object, xshift, 0, -1, 0, 0, 1)
        else
            -- translate object so that bounding box doesn't change
            local yshift = 2 * (obox.top + int(obox.ht/2))
            if obox.ht % 2 == 0 then yshift = yshift - 1 end
            object = g.transform(object, 0, yshift, 1, 0, 0, -1)
        end
        oldcells = underneath(object)
        g.putcells(object)
        g.update()
        return
    end
    
    if event == "key > none" or event == "key < none" then
        -- rotate floating object clockwise or anticlockwise
        -- about the center of the object's bounding box
        local obox = getminbox(object)
        local midx = obox.left + int(obox.wd/2)
        local midy = obox.top + int(obox.ht/2)
        local newleft = midx + obox.top - midy
        local newtop = midy + obox.left - midx
        local rotrect = { newleft, newtop, obox.ht, obox.wd }
        if not rectingrid(rotrect) then
            g.show("Rotation is not allowed if object would be outside grid.")
            return
        end
        g.putcells(object, 0, 0, 1, 0, 0, 1, "xor")  -- erase object
        if #oldcells > 0 then g.putcells(oldcells) end
        if event == "key > none" then
            -- rotate clockwise
            object = g.transform(object, 0, 0, 0, -1, 1, 0)
        else
            -- rotate anticlockwise
            object = g.transform(object, 0, 0, 0, 1, -1, 0)
        end
        -- shift rotated object to same position as rotrect
        obox = getminbox(object)
        object = g.transform(object, newleft - obox.left, newtop - obox.top)
        oldcells = underneath(object)
        g.putcells(object)
        g.update()
        return
    end
    
    if event == "key h none" then
        -- best not to show Help window while dragging object!
        return
    end

    g.doevent(event)
end

--------------------------------------------------------------------------------

function moveobject()
    local oldmouse, mousepos, prevx, prevy
    
    -- wait for click in or near a live cell
    while true do
        local event = g.getevent()
        if event:find("^click") then
            -- event is a string like "click 10 20 left none"
            local evt, xstr, ystr, butt, mods = split(event)
            local result = findlivecell(tonumber(xstr), tonumber(ystr))
            if #result > 0 then
                prevx = tonumber(xstr)
                prevy = tonumber(ystr)
                oldmouse = xstr .. ' ' .. ystr
                g.show("Extracting object...")
                local x, y = table.unpack(result)
                object = getobject(x, y)
                object1 = g.join({},object)   -- save in case user aborts script
                if mods == "alt" then
                    -- don't delete object
                    oldcells = g.join({},object)
                end
                break
            else
                g.warn("Click on or near a live cell belonging to the desired object.")
            end
        elseif event == "key h none" then
            showhelp()
        else
            g.doevent(event)
        end
    end
    
    -- wait for mouse-up while moving object
    g.show("Move mouse and release button...")
    local mousedown = true
    while mousedown do
        local event = g.getevent()
        if event:find("^mup") then
            mousedown = false
        elseif #event > 0 then
            lookforkeys(event)
        end
        mousepos = g.getxy()
        if #mousepos > 0 and mousepos ~= oldmouse then
            -- mouse has moved, so move object
            g.putcells(object, 0, 0, 1, 0, 0, 1, "xor")  -- erase object
            if #oldcells > 0 then g.putcells(oldcells) end
            local xstr, ystr = split(mousepos)
            local x = tonumber(xstr)
            local y = tonumber(ystr)

            if g.getwidth() > 0 then
                -- ensure object doesn't move beyond left/right edge of grid
                local obox = getminbox( g.transform(object, x - prevx, y - prevy) )
                if obox.left < gridl then
                    x = x + gridl - obox.left
                elseif obox.right > gridr then
                    x = x - (obox.right - gridr)
                end
            end
            
            if g.getheight() > 0 then
                -- ensure object doesn't move beyond top/bottom edge of grid
                local obox = getminbox( g.transform(object, x - prevx, y - prevy) )
                if obox.top < gridt then
                    y = y + gridt - obox.top
                elseif obox.bottom > gridb then
                    y = y - (obox.bottom - gridb)
                end
            end
            
            object = g.transform(object, x - prevx, y - prevy)
            oldcells = underneath(object)
            g.putcells(object)
            prevx = x
            prevy = y
            oldmouse = mousepos
            g.update()
        end
    end
end

--------------------------------------------------------------------------------

if #g.getrect() == 0 then g.exit("There are no objects.") end

g.show("Click on or near a live cell, move mouse and release button... (hit 'h' for help)")
local oldcursor = g.getcursor()
g.setcursor("Move")

local aborted = true
local status, err = xpcall(function () moveobject() aborted = false end, gp.trace)
if err then g.continue(err) end

g.setcursor(oldcursor)
if aborted then
    -- erase object if it moved
    if #object > 0 then g.putcells(object, 0, 0, 1, 0, 0, 1, "xor") end
    if #oldcells > 0 then g.putcells(oldcells) end
    if #object1 > 0 then g.putcells(object1) end
else
    g.show(" ")
end
