-- If the current rule uses a hexagonal neighborhood then create
-- an overlay that shows the current pattern in a hexagonal grid.
-- Much thanks to http://www.redblobgames.com/grids/hexagons/.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), August 2016.

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local op = require "oplus"
local int = gp.int

local ov = g.overlay

-- the following are set in create_overlay()
local gridr, gridg, gridb   -- RGB colors for grid lines
local state_rgba = {}       -- table of "r g b 255" strings for each state
local gridwd, gridht        -- width and height of grid (bounded if either > 0)

local minedge = 1   -- minimum length of a hexagon edge
local maxedge = 32  -- maximum length of a hexagon edge
local edgelen = 16  -- initial length
local mingrid = 4   -- minimum edgelen at which to draw hexagonal grid

-- the following depend on edgelen and are set in init_values()
local hexwd         -- width of 1 hexagon
local hexht         -- height of 1 hexagon
local hdist         -- horizontal distance between adjacent hexagons
local vdist         -- vertical distance between adjacent hexagons
local xrad          -- x distances to each vertex
local yrad          -- y distances to each vertex
local hextox        -- used to find x coord of hexagon's center
local hextoy        -- used to find y coord of hexagon's center

-- minor optimizations
local abs = math.abs
local ceil = math.ceil
local floor = math.floor

-- remember which hexagons were filled
local livehexagons = {}

local generating = false

-- get pixel dimensions of current layer
local viewwd, viewht = g.getview(g.getlayer())
local midx, midy = viewwd/2, viewht/2

-- initial cell position of current layer (central cell)
local xpos, ypos = gp.getposint()

--------------------------------------------------------------------------------

local function check_rule()
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

local function create_overlay()
    -- for checking if grid is bounded
    gridwd = g.getwidth()
    gridht = g.getheight()

    ov("create "..viewwd.." "..viewht)      -- overlay covers entire viewport
    
    ov("cursor current")    -- use current Golly cursor
    
    local currcolors = g.getcolors()
    for i = 0, g.numstates()-1 do
        state_rgba[i] = ""..currcolors[2+i*4].." "..currcolors[3+i*4].." "..currcolors[4+i*4].." 255"
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

local function init_values()
    -- initialize values that depend on edgelen
    -- (note that we use flat-topped hexagons)
    hexwd = edgelen * 2
    hexht = math.sqrt(3) / 2 * hexwd
    hdist = hexwd * 3/4
    vdist = hexht
    
    -- these arrays are used in get_vertex
    xrad = {}
    yrad = {}
    for i = 0, 5 do
        local angle_rad = math.pi / 180 * 60 * i
        xrad[i] = edgelen * math.cos(angle_rad)
        yrad[i] = edgelen * math.sin(angle_rad)
    end
    
    -- these values are used to find x,y coords of a hexagon's center
    hextox = edgelen * 3/2
    hextoy = edgelen * math.sqrt(3)
end

--------------------------------------------------------------------------------

local function round(x)
    if x < 0 then
        return ceil(x - 0.5)
    else
        return floor(x + 0.5)
    end
end

--------------------------------------------------------------------------------

local function cube_round(x, y, z)
    local rx = round(x)
    local ry = round(y)
    local rz = round(z)

    local x_diff = abs(rx - x)
    local y_diff = abs(ry - y)
    local z_diff = abs(rz - z)

    if x_diff > y_diff and x_diff > z_diff then
        rx = -ry-rz
    elseif y_diff > z_diff then
        ry = -rx-rz
    else
        rz = -rx-ry
    end

    return rx, rz
end

--------------------------------------------------------------------------------

local sqrt3on3 = math.sqrt(3)/3

local function pixel_to_hex(x, y)
    x = x - midx
    y = y - midy
    
    local q = x * 2/3 / edgelen
    local r = (-x/3 + sqrt3on3 * y) / edgelen
    
    q, r = cube_round(q, -q-r, r)
    
    return int(q), int(r)
end

--------------------------------------------------------------------------------

local function get_visible_rect()
    -- get axial coords of corner hexagons
    local qmin, _ = pixel_to_hex(0, 0)
    local qmax, _ = pixel_to_hex(viewwd-1, viewht-1)
    local _, rmin = pixel_to_hex(viewwd-1, 0)
    local _, rmax = pixel_to_hex(0, viewht-1)

    -- convert axial coords to cell coords
    local minx = qmin + rmin + xpos
    local maxx = qmax + rmax + xpos
    local miny = rmin + ypos
    local maxy = rmax + ypos
    
    -- adjust limits if current layer's grid is bounded
    if gridwd > 0 then
        local bminx = -int(gridwd / 2)
        local bmaxx = bminx + gridwd - 1
        if minx < bminx then minx = bminx end
        if maxx > bmaxx then maxx = bmaxx end
    end
    if gridht > 0 then
        local bminy = -int(gridht / 2)
        local bmaxy = bminy + gridht - 1
        if miny < bminy then miny = bminy end
        if maxy > bmaxy then maxy = bmaxy end
    end

    return { minx, miny, maxx-minx+1, maxy-miny+1 }
end

--------------------------------------------------------------------------------

local function fill_hexagon(xc, yc, oldstate, newstate)
    xc = xc + int(midx)
    yc = yc + int(midy)

    ov("rgba "..state_rgba[newstate])

    if edgelen < mingrid then
        if edgelen == 1 then
            ov("set "..xc.." "..yc)
        else
            ov("fill "..int(xc-edgelen/2).." "..int(yc-edgelen/2).." "..edgelen.." "..edgelen)
        end
    else
        -- adjust xc and/or yc if they are outside overlay but hexagon is partially visible
        if xc < 0 then
            if xc + edgelen >= 0 then xc = 0 end
        elseif xc >= viewwd then
            if xc - edgelen <= viewwd-1 then xc = viewwd-1 end
        end
        if yc < 0 then
            if yc + hexht/2 >= 0 then yc = 0 end
        elseif yc >= viewht then
            if yc - hexht/2 <= viewht-1 then yc = viewht-1 end
        end
        ov("flood "..xc.." "..yc)
    end
end

--------------------------------------------------------------------------------

local function draw_live_cells()
    livehexagons = {}
    
    local cells = {}
    if not g.empty() then
        -- get cells that will be visible in current hex grid
        cells = g.getcells( get_visible_rect() )
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
        convert cell coords x,y to axial coords q,r so that the pattern
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
        local r = y - ypos
        local q = x - xpos - r
        
        -- calculate pixel at center of hexagon and fill it with current state
        local xc = int(hextox * q)
        local yc = int(hextoy * (r + q/2))
        fill_hexagon(xc, yc, 0, state)
        
        livehexagons[#livehexagons + 1] = {xc, yc, state}
    end
end

--------------------------------------------------------------------------------

local function kill_live_cells()
    for _, hexcell in ipairs(livehexagons) do
        local xc, yc, state = table.unpack(hexcell)
        fill_hexagon(xc, yc, state, 0)
    end
    livehexagons = nil
end

--------------------------------------------------------------------------------

local function get_vertex(cx, cy, i)
    return int(cx + midx + xrad[i]), int(cy + midy + yrad[i])
end 

--------------------------------------------------------------------------------

local function draw_line(x1, y1, x2, y2)
    ov("line "..x1.." "..y1.." "..x2.." "..y2)
end

--------------------------------------------------------------------------------

local function draw_hexagon(cx, cy)
    local x0, y0 = get_vertex(cx, cy, 0)
    local x1, y1 = get_vertex(cx, cy, 1)
    local x2, y2 = get_vertex(cx, cy, 2)
    local x3, y3 = get_vertex(cx, cy, 3)
    local x4, y4 = get_vertex(cx, cy, 4)
    local x5, y5 = get_vertex(cx, cy, 5)
    draw_line(x5, y5, x0, y0)
    draw_line(x0, y0, x1, y1)
    draw_line(x1, y1, x2, y2)
    draw_line(x2, y2, x3, y3)
    draw_line(x3, y3, x4, y4)
    -- no need to draw top edge
    -- draw_line(x4, y4, x5, y5)
    
    -- draw horizontal line from right-most vertex
    draw_line(x0, y0, x0 + edgelen, y0)
end

--------------------------------------------------------------------------------

local function draw_hex_grid()
    ov("rgba "..gridr.." "..gridg.." "..gridb.." 255")
    
    local minx, maxx, miny, maxy = -midx, midx, -midy, midy
    local x, y = 0, 0
    
    repeat
        draw_hexagon(x, y)
        while y <= maxy do
            y = y + vdist
            draw_hexagon(x, y)
        end
        y = 0
        while y >= miny do
            y = y - vdist
            draw_hexagon(x, y)
        end
        x = x + hdist * 2
        y = 0
    until x - hdist > maxx
    
    x = -hdist * 2
    y = 0
    repeat
        draw_hexagon(x, y)
        while y <= maxy do
            y = y + vdist
            draw_hexagon(x, y)
        end
        y = 0
        while y >= miny do
            y = y - vdist
            draw_hexagon(x, y)
        end
        x = x - hdist * 2
        y = 0
    until x + edgelen * 2 < minx
end

--------------------------------------------------------------------------------

local function refresh()
    init_values()
    ov("rgba "..state_rgba[0])  -- background color
    ov("fill")
    if edgelen >= mingrid then
        draw_hex_grid()
    end
    draw_live_cells()
    ov("update")
end

--------------------------------------------------------------------------------

local function advance(by1)
    if g.empty() then
        g.note("There are no live cells.")
        generating = false
        return
    end
    
    if by1 then
        g.run(1)
    else
        g.step()
    end
    
    -- erase live hexagons and draw new ones
    kill_live_cells()
    draw_live_cells()
    g.update()
end

--------------------------------------------------------------------------------

local function reset()
    g.reset()
    
    -- erase live hexagons and draw new ones
    kill_live_cells()
    draw_live_cells()
    g.update()
end

--------------------------------------------------------------------------------

local function zoom_in()
    if edgelen < maxedge then
        -- draw bigger hexagons
        edgelen = edgelen + 1
        refresh()
    end
end

--------------------------------------------------------------------------------

local function zoom_out()
    if edgelen > minedge then
        -- draw smaller hexagons
        edgelen = edgelen - 1
        refresh()
    end
end

--------------------------------------------------------------------------------

local function outside_grid(q, r)
    if gridwd == 0 and gridht == 0 then
        -- grid is unbounded
        return false
    end

    -- convert axial coords to cell coords
    local x = q + r + xpos
    local y = r + ypos

    if gridwd > 0 then
        local bminx = -int(gridwd / 2)
        local bmaxx = bminx + gridwd - 1
        if x < bminx or x > bmaxx then
            return true
        end
    end
    if gridht > 0 then
        local bminy = -int(gridht / 2)
        local bmaxy = bminy + gridht - 1
        if y < bminy or y > bmaxy then
            return true
        end
    end
    
    return false
end

--------------------------------------------------------------------------------

local function do_click_in_layer(event)
    -- process a mouse click in current layer like "click 100 20 left none"
    local _, x, y, button, mods = gp.split(event)
    local state = g.getcell(x, y)
    
    if button ~= "left" then return end
    if mods ~= "none" and mods ~= "shift" then return end
    
    if g.getcursor() == "Draw" then
        local drawstate = g.getoption("drawingstate")
        if state == 0 and drawstate == 0 then
            -- no change
        else
            if state == drawstate then
                -- kill cell
                g.setcell(x, y, 0)
            else
                g.setcell(x, y, drawstate)
            end
            g.update()
            refresh()   -- updates overlay (currently not shown)
        end
    
    elseif g.getcursor() == "Pick" then
        g.setoption("drawingstate", state)
        g.update()      -- updates edit bar
    
    elseif g.getcursor() == "Select" then
        -- don't allow selections???!!!
        -- g.doevent(event)
    
    else
        -- g.getcursor() == "Move" or "Zoom In" or "Zoom Out"
        g.doevent(event)
    end
end

--------------------------------------------------------------------------------

local function get_state(color)
    -- search state_rgba array for color and return matching state
    for s, c in pairs(state_rgba) do
        if c == color then
            return s
        end
    end
    return -1   -- no match
end

--------------------------------------------------------------------------------

local function do_click_in_overlay(event)
    -- process a mouse click in overlay like "oclick 100 20 left none"
    local _, x, y, button, mods = gp.split(event)
    x = tonumber(x)
    y = tonumber(y)
    
    if button ~= "left" then return end
    if mods ~= "none" and mods ~= "shift" then return end

    if g.getcursor() == "Draw" then
        local state = get_state( ov("get "..x.." "..y) )
        if state < 0 then
            -- no match (click was in a grid line, so ignore it)
            return
        end
        
        -- get axial coords of hexagon containing clicked pixel
        local q, r = pixel_to_hex(x, y)
        
        -- check if cell is outside bounded grid
        if outside_grid(q, r) then
            -- no need for this msg if we draw gray border around bounded hexagons!!!
            g.note("Cell is outside grid boundary.")
            return
        end
        
        -- find central pixel of hexagon containing x,y
        local xc = int(hextox * q)
        local yc = int(hextoy * (r + q/2))
        
        -- update hexagon
        local drawstate = g.getoption("drawingstate")
        if state == drawstate then
            -- erase hexagon
            fill_hexagon(xc, yc, state, 0)
        else
            fill_hexagon(xc, yc, state, drawstate)
            if drawstate > 0 then
                livehexagons[#livehexagons + 1] = {xc, yc, drawstate}
            end
        end
        
        -- update corresponding cell in current layer
        local xcell = q + r + xpos
        local ycell = r + ypos
        state = g.getcell(xcell, ycell)
        if state == drawstate then
            -- kill cell
            g.setcell(xcell, ycell, 0)
        else
            g.setcell(xcell, ycell, drawstate)
        end
        
        -- display overlay and show new population count in status bar
        g.update()

    elseif g.getcursor() == "Pick" then
        local state = get_state( ov("get "..x.." "..y) )
        if state >= 0 then
            g.setoption("drawingstate", state)
            g.update()  -- updates edit bar
        end
    
    elseif g.getcursor() == "Move" then
        -- this sorta works but really need a better approach!!!
        local q, r = pixel_to_hex(x, y)
        if outside_grid(q, r) then
            return
        end
        local xcell = q + r + xpos
        local ycell = r + ypos
        g.doevent("click "..xcell.." "..ycell.." left none")
    
    elseif g.getcursor() == "Select" then
        --!!!???
        
    elseif g.getcursor() == "Zoom In" then
        -- zoom in to clicked hexagon!!!
        zoom_in()
        
    elseif g.getcursor() == "Zoom Out" then
        -- zoom out from clicked hexagon!!!
        zoom_out()
    end
end

--------------------------------------------------------------------------------

local function pan(event)
    -- user pressed an arrow key
    if g.getoption("showoverlay") == 0 then
        -- just pan the current layer
        g.doevent(event)
    else
        local _, key, mods = gp.split(event)
        if mods == "none" then
            -- to pan overlay orthogonally we need to pan layer diagonally
            if key == "left" then
                gp.setposint(xpos-1, ypos+1)
            elseif key == "right" then
                gp.setposint(xpos+1, ypos-1)
            elseif key == "up" then
                gp.setposint(xpos-1, ypos-1)
            elseif key == "down" then
                gp.setposint(xpos+1, ypos+1)
            end
        elseif mods == "shift" then
            -- to pan overlay diagonally we need to pan layer orthogonally
            if key == "left" then
                gp.setposint(xpos-1, ypos)
            elseif key == "right" then
                gp.setposint(xpos+1, ypos)
            elseif key == "up" then
                gp.setposint(xpos, ypos-1)
            elseif key == "down" then
                gp.setposint(xpos, ypos+1)
            end
        end
    end
end

--------------------------------------------------------------------------------

local function show_help()
    --!!!
end

--------------------------------------------------------------------------------

local function main()
    check_rule()
    create_overlay()
    refresh()
    
    g.show("Hit escape key to abort script...")
    while true do
        -- check if size of layer has changed
        local newwd, newht = g.getview(g.getlayer())
        if newwd ~= viewwd or newht ~= viewht then
            viewwd, viewht = newwd, newht
            midx, midy = viewwd/2, viewht/2
            ov("resize "..viewwd.." "..viewht)
            refresh()
        end
        
        -- check for user input
        local event = g.getevent()
        if event:find("^click") then
            do_click_in_layer(event)
        elseif event:find("^oclick") then
            do_click_in_overlay(event)
        elseif event:find("^ozoomin") then
            -- zoom in to hexagon at given x,y pixel???!!!
            zoom_in()
        elseif event:find("^ozoomout") then
            -- zoom out from hexagon at given x,y pixel???!!!
            zoom_out()
        elseif event == "key enter none" or event == "key return none" then
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
            zoom_in()
        elseif event == "key [ none" then
            zoom_out()
        elseif event:find("^key left ") or
               event:find("^key right ") or
               event:find("^key up ") or
               event:find("^key down ") then
            pan(event)
        elseif event == "key h none" then
            show_help()
        else
            g.doevent(event)
        end
        
        -- check if current layer's position has changed
        local x0, y0 = gp.getposint()
        if x0 ~= xpos or y0 ~= ypos then
            xpos = x0
            ypos = y0
            refresh()
        end
        
        if generating then advance(false) end
    end
end

--------------------------------------------------------------------------------

local oldstack = g.setoption("stacklayers", 0)
local oldbuttons = g.setoption("showbuttons", 0) -- disable translucent buttons

local status, err = pcall(main)
if err then g.continue(err) end
-- the following code is always executed

ov("delete")
g.setoption("stacklayers", oldstack)
g.setoption("showbuttons", oldbuttons)
