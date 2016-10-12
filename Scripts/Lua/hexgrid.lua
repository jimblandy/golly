-- If the current rule uses a hexagonal neighborhood then create
-- an overlay that shows the current pattern in a hexagonal grid.
-- Much thanks to http://www.redblobgames.com/grids/hexagons/.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), August 2016.

local g = golly()
--!!! require "gplus.strict"
local gp = require "gplus"
local op = require "oplus"
local int = gp.int

local ov = g.overlay

local viewwd, viewht = g.getview(g.getlayer())

-- the following are set in createoverlay()
local gridr, gridg, gridb   -- RGB colors for grid lines
local rgba = {}             -- table of "r g b 255" strings for each state
local gridwd, gridht        -- width and height of grid (bounded if either > 0)

local minedge = 1   -- minimum length of a hexagon edge
local maxedge = 32  -- maximum length of a hexagon edge
local edgelen = 16  -- initial length
local mingrid = 4   -- minimum edgelen at which to draw hexagonal grid

-- the following depend on edgelen and are set in initvalues()
local hexwd         -- width of 1 hexagon
local hexht         -- height of 1 hexagon
local hdist         -- horizontal distance between adjacent hexagons
local vdist         -- vertical distance between adjacent hexagons
local xrad          -- x distances to each vertex
local yrad          -- y distances to each vertex
local hextox        -- used to find x coord of hexagon's center
local hextoy        -- used to find y coord of hexagon's center

-- remember which hexagons were filled
local livehexagons = {}

-- initial position of viewport
-- fix!!! (use gp.getposint())
local xpos = 0
local ypos = 0

--------------------------------------------------------------------------------

local function checkrule()
    -- check if the current rule uses a hexagonal neighborhood
    if not op.hexrule() then
        local algo = g.getalgo()
        if algo == "RuleLoader" then
            g.warn(
[[If the current rule does not use
a hexagonal neighborhood then the
results won't be correct!]])
            -- let user continue
        else
            g.exit("The current rule does not use a hexagonal neighborhood.")
        end
    end
end

--------------------------------------------------------------------------------

local function createoverlay()
    -- for checking if grid is bounded
    gridwd = g.getwidth()
    gridht = g.getheight()

    ov("create "..viewwd.." "..viewht)      -- overlay covers entire viewport
    
    local currcolors = g.getcolors()
    for i = 0, g.numstates()-1 do
        rgba[i] = ""..currcolors[2+i*4].." "..currcolors[3+i*4].." "..currcolors[4+i*4].." 255"
    end
    
    -- set grid colors based on current background color
    gridr, gridg, gridb = currcolors[2], currcolors[3], currcolors[4]
    local diff = 48
    if (gridr + gridg + gridb) / 3 > 127 then
       -- light background so make grid lines slightly darker
       if gridr > diff then gridr = gridr - diff else gridr = 0 end
       if gridg > diff then gridg = gridg - diff else gridg = 0 end
       if gridb > diff then gridb = gridb - diff else gridb = 0 end
    else
       -- dark background so make grid lines slightly lighter
       if gridr + diff < 256 then gridr = gridr + diff else gridr = 255 end
       if gridg + diff < 256 then gridg = gridg + diff else gridg = 255 end
       if gridb + diff < 256 then gridb = gridb + diff else gridb = 255 end
    end
end

--------------------------------------------------------------------------------

local function initvalues()
    -- initialize values that depend on edgelen
    -- (note that we use flat-topped hexagons)
    hexwd = edgelen * 2
    hexht = math.sqrt(3) / 2 * hexwd
    hdist = hexwd * 3/4
    vdist = hexht
    
    -- these arrays are used in getvertex
    xrad = {}
    yrad = {}
    for i = 0, 5 do
        local angle_rad = math.pi / 180 * 60 * i
        xrad[i] = edgelen * math.cos(angle_rad)
        yrad[i] = edgelen * math.sin(angle_rad)
    end
    
    -- these values are used in drawlivecells
    hextox = edgelen * 3/2
    hextoy = edgelen * math.sqrt(3)
end

--------------------------------------------------------------------------------

local function cube_to_hex(x, y, z)
    local q = x
    local r = z
    return q, r
end

--------------------------------------------------------------------------------

local function hex_to_cube(q, r)
    local x = q
    local z = r
    local y = -x-z
    return x, y, z
end

--------------------------------------------------------------------------------

local function round(x)
    return math.floor(x + 0.5)
end

--------------------------------------------------------------------------------

local function cube_round(x, y, z)
    local rx = round(x)
    local ry = round(y)
    local rz = round(z)

    local x_diff = math.abs(rx - x)
    local y_diff = math.abs(ry - y)
    local z_diff = math.abs(rz - z)

    if x_diff > y_diff and x_diff > z_diff then
        rx = -ry-rz
    elseif y_diff > z_diff then
        ry = -rx-rz
    else
        rz = -rx-ry
    end

    return rx, ry, rz
end

--------------------------------------------------------------------------------

local function hex_round(q, r)
    return cube_to_hex(cube_round(hex_to_cube(q, r)))
end

--------------------------------------------------------------------------------

local function pixel_to_hex(x, y)
    local q = x * 2/3 / edgelen
    local r = (-x/3 + math.sqrt(3)/3 * y) / edgelen
    
    q, r = hex_round(q, r)
    
    return int(q), int(r)
end

--------------------------------------------------------------------------------

local function getboundary()
    local maxx = int((viewwd+1) / 2)
    local minx = -maxx
    local maxy = int((viewht+1) / 2)
    local miny = -maxy
    
    minx = minx + xpos
    maxx = maxx + xpos
    miny = miny + ypos
    maxy = maxy + ypos

    return minx, maxx, miny, maxy
end

--------------------------------------------------------------------------------

local function getvisiblerect()
    local minx, maxx, miny, maxy = getboundary()

    -- get axial coords of corner hexagons
    local qmin, _ = pixel_to_hex(minx, miny)
    local qmax, _ = pixel_to_hex(maxx, maxy)
    local _, rmin = pixel_to_hex(maxx, miny)
    local _, rmax = pixel_to_hex(minx, maxy)

    -- convert axial coords to square cell coords
    minx = qmin + rmin
    maxx = qmax + rmax
    miny = rmin
    maxy = rmax
    
    -- adjust limits if current layer's grid is bounded
    if gridwd > 0 then
        local bminx = -gp.int(gridwd / 2)
        local bmaxx = bminx + gridwd - 1
        if minx < bminx then minx = bminx end
        if maxx > bmaxx then maxx = bmaxx end
    end
    if gridht > 0 then
        local bminy = -gp.int(gridht / 2)
        local bmaxy = bminy + gridht - 1
        if miny < bminy then miny = bminy end
        if maxy > bmaxy then maxy = bmaxy end
    end

    return { minx, miny, maxx-minx+1, maxy-miny+1 }
end

--------------------------------------------------------------------------------

local function fillhexagon(xc, yc, oldstate, newstate)
    xc = xc + int(viewwd/2)
    yc = yc + int(viewht/2)

    ov("rgba "..rgba[newstate])

    if edgelen < mingrid then
        --!!!???
        ov("set "..xc.." "..yc)
    else
        ov("flood "..xc.." "..yc)
    end
end

--------------------------------------------------------------------------------

local function drawlivecells()
    livehexagons = {}
    
    local cells = {}
    if not g.empty() then
        -- get cells that will be visible in current hex grid
        cells = g.getcells( getvisiblerect() )
    end
    if #cells == 0 then return end
    
    local len = #cells
    local inc = 2
    if len & 1 == 1 then
        -- multi-state cell array so set inc to 3 and ignore any padding int
        inc = 3
        if len % 3 == 1 then len = len - 1 end
    end
    
    for i = 1, len, inc do
        local x = cells[i]
        local y = cells[i+1]
        local state = 1
        if inc == 3 then state = cells[i+2] end
        
        --[[
        convert square coords x,y to axial coords q,r so that the pattern
        is rotated clockwise by 45 degrees; ie. the square cell at x,y = -1,-1
        will be in the hexagonal cell at q,r = 0,-1
         _________________                  _____
        |     |     |     |                /     \
        |-1,-1| 0,-1|+1,-1|          -----{  0,-1 }-----
        |_____|_____|_____|         /-1,0  \_____/ +1,-1\
        |     |     |     |         \      /     \      /
        |-1,0 | 0,0 |+1,0 |  --->    -----{  0,0  }----{
        |_____|_____|_____|         /-1,+1 \_____/ +1,0 \
        |     |     |     |         \      /     \      /
        |-1,+1| 0,+1|+1,+1|          -----{  0,+1 }-----
        |_____|_____|_____|                \_____/
        --]]
        local q = x - y
        local r = y
        
        -- calculate pixel at center of hexagon and fill it with current state
        local xc = int(hextox * q)
        local yc = int(hextoy * (r + q/2))
        fillhexagon(xc, yc, 0, state)
        
        livehexagons[#livehexagons + 1] = {xc, yc, state}
    end
end

--------------------------------------------------------------------------------

local function killlivecells()
    for _, hexcell in ipairs(livehexagons) do
        local xc, yc, state = table.unpack(hexcell)
        fillhexagon(xc, yc, state, 0)
    end
    livehexagons = nil
end

--------------------------------------------------------------------------------

local function getvertex(centerx, centery, i)
    centerx = centerx + viewwd/2
    centery = centery + viewht/2
    return int(centerx + xrad[i]), int(centery + yrad[i])
end 

--------------------------------------------------------------------------------

local function drawline(x1, y1, x2, y2)
    -- faster to use string.format???!!!
    ov("line "..x1.." "..y1.." "..x2.." "..y2)
end

--------------------------------------------------------------------------------

local function drawhexagon(cx, cy)
    local x0, y0 = getvertex(cx, cy, 0)
    local x1, y1 = getvertex(cx, cy, 1)
    local x2, y2 = getvertex(cx, cy, 2)
    local x3, y3 = getvertex(cx, cy, 3)
    local x4, y4 = getvertex(cx, cy, 4)
    local x5, y5 = getvertex(cx, cy, 5)
    drawline(x5, y5, x0, y0)
    drawline(x0, y0, x1, y1)
    drawline(x1, y1, x2, y2)
    drawline(x2, y2, x3, y3)
    drawline(x3, y3, x4, y4)
    -- no need to draw top edge
    -- drawline(x4, y4, x5, y5)
    
    -- draw horizontal line from right-most vertex
    drawline(x0, y0, x0 + edgelen, y0)
end

--------------------------------------------------------------------------------

local function drawhexgrid()    
    local minx, maxx, miny, maxy = getboundary()
    local x, y = 0, 0
    
    ov("rgba "..gridr.." "..gridg.." "..gridb.." 255")
    
    repeat
        drawhexagon(x, y)
        while y <= maxy do
            y = y + vdist
            drawhexagon(x, y)
        end
        y = 0
        while y >= miny do
            y = y - vdist
            drawhexagon(x, y)
        end
        x = x + hdist * 2
        y = 0
    until x - hdist > maxx
    
    x = -hdist * 2
    y = 0
    repeat
        drawhexagon(x, y)
        while y <= maxy do
            y = y + vdist
            drawhexagon(x, y)
        end
        y = 0
        while y >= miny do
            y = y - vdist
            drawhexagon(x, y)
        end
        x = x - hdist * 2
        y = 0
    until x + hdist < minx
    
    -- if necessary, draw horizontal lines from left-most vertices
    x = x + hexwd / 2 + edgelen
    if x > minx then
        y = 0
        while y <= maxy do
            drawline(int(x), int(y), int(x - edgelen), int(y))
            y = y + vdist
        end
        y = -vdist
        while y >= miny do
            drawline(int(x), int(y), int(x - edgelen), int(y))
            y = y - vdist
        end
    end
end

--------------------------------------------------------------------------------

local function refresh()
    ov("rgba "..rgba[0]) -- background color
    ov("fill")
    initvalues()
    if edgelen >= mingrid then
        drawhexgrid()
    end
    drawlivecells()
    --!!! g.update()
    ov("update") --???!!!
end

--------------------------------------------------------------------------------

local function advance(by1)
    if g.empty() then
        g.note("There are no live cells.")
        return
    end
    
    if by1 then
        g.run(1)
    else
        g.step()
    end
    
    -- erase live hexagons and draw new ones
    killlivecells()
    drawlivecells()
    g.update()
end

--------------------------------------------------------------------------------

local function reset()
    g.reset()
    
    -- erase filled hexagons and draw new ones
    killlivecells()
    drawlivecells()
    g.update()
end

--------------------------------------------------------------------------------

local function zoomin()
    if edgelen < maxedge then
        -- draw bigger hexagons
        edgelen = edgelen + 1
        refresh()
    end
end

--------------------------------------------------------------------------------

local function zoomout()
    if edgelen > minedge then
        -- draw smaller hexagons
        edgelen = edgelen - 1
        refresh()
    end
end

--------------------------------------------------------------------------------

local function showhelp()
    --!!!
end

--------------------------------------------------------------------------------

local outsidemsg = "Cell is outside grid boundary."

local function outsidegrid(q, r)
    if gridwd == 0 and gridht == 0 then
        -- grid is unbounded
        return false
    end

    -- convert axial coords to square grid coords
    local x = q + r
    local y = r

    if gridwd > 0 then
        local bminx = -gp.int(gridwd / 2)
        local bmaxx = bminx + gridwd - 1
        if x < bminx or x > bmaxx then
            g.note(outsidemsg)
            return true
        end
    end
    if gridht > 0 then
        local bminy = -gp.int(gridht / 2)
        local bmaxy = bminy + gridht - 1
        if y < bminy or y > bmaxy then
            g.note(outsidemsg)
            return true
        end
    end
    
    return false
end

--------------------------------------------------------------------------------

local function doclick(event)
    -- process a mouse event like "click 100 20 left none"
    local _, x, y, button, mods = gp.split(event)
    
    local state = g.getcell(x, y)
    --[[ !!!???
    if state == gridstate then
        -- ignore click in grid line
        return
    end
    --]]
    
    if g.getcursor() == "Pick" then
        g.setoption("drawingstate", state)
        g.update()  -- updates edit bar
    
    elseif g.getcursor() == "Draw" then
        local drawstate = g.getoption("drawingstate")
        if state == 0 and drawstate == 0 then
            -- no change
            return
        end
        
        -- get axial coords of hexagon containing clicked pixel
        local q, r = pixel_to_hex(x, y)
        
        -- check if cell is outside bounded grid
        if outsidegrid(q, r) then return end
        
        -- find central pixel of hexagon containing x,y
        local xc = int(hextox * q)
        local yc = int(hextoy * (r + q/2))
        
        -- update hexagon
        if state == drawstate then
            -- erase hexagon
            fillhexagon(xc, yc, state, 0)
        else
            fillhexagon(xc, yc, state, drawstate)
            if drawstate > 0 then
                livehexagons[#livehexagons + 1] = {xc, yc, drawstate}
            end
        end
        
        -- update corresponding cell in current layer
        local xcell = q + r
        local ycell = r
        state = g.getcell(xcell, ycell)
        if state == drawstate then
            -- kill cell
            g.setcell(xcell, ycell, 0)
        else
            g.setcell(xcell, ycell, drawstate)
        end
    
    elseif g.getcursor() == "Zoom In" then
        zoomin()
    
    elseif g.getcursor() == "Zoom Out" then
        zoomout()
    
    else
        -- ignore click with Move/Select cursor
    end
end

--------------------------------------------------------------------------------

local function main()
    checkrule()
    createoverlay()
    refresh()
    
    g.show("Hit escape key to abort script...")
    local generating = false
    while true do
        -- check if size of layer has changed
        local newwd, newht = g.getview(g.getlayer())
        if newwd ~= viewwd or newht ~= viewht then
            viewwd = newwd
            viewht = newht
            ov("resize "..viewwd.." "..viewht)      -- resize overlay
            refresh()
        end
        
        -- check for user input
        local event = g.getevent()
        if event == "key enter none" or event == "key return none" then
            generating = not generating
        elseif event == "key space none" then
            advance(true)
            generating = false
        elseif event == "key tab none" then
            advance(false)
            generating = false
        elseif event == "key r none" then
            reset()
            generating = false
        elseif event == "key ] none" then
            zoomin()
        elseif event == "key [ none" then
            zoomout()
        elseif event == "key h none" then
            showhelp()
        elseif event:find("^click") then
            doclick(event)
        elseif event:find("^ozoomin") then
            -- zoom in to hexagon at given x,y pixel???!!!
            zoomin()
        elseif event:find("^ozoomout") then
            -- zoom out from hexagon at given x,y pixel???!!!
            zoomout()
        else
            g.doevent(event)
        end
        
        -- check if pattern position has changed
        local x0, y0 = gp.getposint()
        if x0 ~= xpos or y0 ~= ypos then
            xpos = x0
            ypos = y0
            refresh()
            --!!! g.note("xpos="..xpos.." ypos="..ypos)
        end
        
        if generating then advance(false) end
    end
end

--------------------------------------------------------------------------------

-- allow layers to be tiled or stacked???!!!
-- local oldtile = g.setoption("tilelayers", 0)
-- local oldstack = g.setoption("stacklayers", 0)

local oldbuttons = g.setoption("showbuttons", 0) -- disable translucent buttons

local status, err = pcall(main)
if err then g.continue(err) end
-- the following code is always executed

ov("delete")
g.setoption("showbuttons", oldbuttons)
