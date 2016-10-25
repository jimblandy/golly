-- If the current rule uses a hexagonal neighborhood then create
-- an overlay that shows the current pattern in a hexagonal grid.
-- Much thanks to http://www.redblobgames.com/grids/hexagons/.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), August 2016.

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local op = require "oplus"
local int = gp.int
local split = gp.split

local ov = g.overlay

-- the following are set in create_overlay()
local grid_rgba         -- RGBA string for grid lines
local state_rgba = {}   -- table of RGBA strings for each state
local gridwd, gridht    -- width and height of grid (bounded if either > 0)

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
        state_rgba[i] = currcolors[2+i*4].." "..currcolors[3+i*4].." "..currcolors[4+i*4].." 255"
    end
    
    -- set grid colors based on current background color
    local gridr, gridg, gridb = currcolors[2], currcolors[3], currcolors[4]
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
    grid_rgba = gridr.." "..gridg.." "..gridb.." 255"
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

local function cell_to_hexagon(x, y)
    --[[
    convert cell position x,y to axial coords q,r so that the pattern
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
    
    -- return central pixel position of corresponding hexagon in overlay
    x = int(hextox * q + midx)
    y = int(hextoy * (r + q/2) + midy)
    
    return x, y
end

--------------------------------------------------------------------------------

local function fill_hexagon(xc, yc, state)
    ov("rgba "..state_rgba[state])

    if edgelen < mingrid then
        if edgelen == 1 then
            ov("set "..xc.." "..yc)
        else
            ov("fill "..int(xc-edgelen/2+0.5).." "..int(yc-edgelen/2+0.5).." "..edgelen.." "..edgelen)
        end
    else
        -- adjust xc and/or yc if they are outside overlay but hexagon is partially visible
        local check_pixel = false
        if xc < 0 then
            if xc + edgelen >= 0 then
                xc = 0
                check_pixel = true
            else
                return
            end
        elseif xc >= viewwd then
            if xc - edgelen <= viewwd-1 then
                xc = viewwd-1
                check_pixel = true
            else
                return
            end
        end
        if yc < 0 then
            if yc + hexht/2 >= 0 then
                yc = 0
                check_pixel = true
            else
                return
            end
        elseif yc >= viewht then
            if yc - hexht/2 <= viewht-1 then
                yc = viewht-1
                check_pixel = true
            else
                return
            end
        end
        if check_pixel then
            -- avoid filling grid line
            if ov("get "..xc.." "..yc) == grid_rgba then return end
        end
        ov("flood "..xc.." "..yc)
    end
end

--------------------------------------------------------------------------------

local function draw_live_cells()
    livehexagons = {}
    if g.empty() then return end
    
    -- get cells that will be visible in current hex grid
    local cells = g.getcells( get_visible_rect() )
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
        
        -- calculate pixel at center of hexagon and fill it with current state
        local xc, yc = cell_to_hexagon(x, y)
        fill_hexagon(xc, yc, state)
        
        livehexagons[#livehexagons + 1] = {xc, yc, state}
    end
end

--------------------------------------------------------------------------------

local function kill_live_cells()
    for _, hexcell in ipairs(livehexagons) do
        local xc, yc, state = table.unpack(hexcell)
        fill_hexagon(xc, yc, 0)
    end
    livehexagons = nil
end

--------------------------------------------------------------------------------

local function get_vertex(xc, yc, i)
    return int(xc + xrad[i]), int(yc + yrad[i])
end 

--------------------------------------------------------------------------------

local function draw_line(x1, y1, x2, y2)
    ov("line "..x1.." "..y1.." "..x2.." "..y2)
end

--------------------------------------------------------------------------------

local function draw_hexagon(xc, yc)
    xc = xc + midx
    yc = yc + midy
    local x0, y0 = get_vertex(xc, yc, 0)
    local x1, y1 = get_vertex(xc, yc, 1)
    local x2, y2 = get_vertex(xc, yc, 2)
    local x3, y3 = get_vertex(xc, yc, 3)
    local x4, y4 = get_vertex(xc, yc, 4)
    local x5, y5 = get_vertex(xc, yc, 5)
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
    ov("rgba "..grid_rgba)
    
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

local function draw_boundary_line(x1, y1, x2, y2)
    x1, y1 = cell_to_hexagon(x1, y1)
    x2, y2 = cell_to_hexagon(x2, y2)
    ov("line "..x1.." "..y1.." "..x2.." "..y2)
end

--------------------------------------------------------------------------------

local function draw_bounded_hexagon(xc, yc, toprow, leftcol)
    local x0, y0 = get_vertex(xc, yc, 0)
    local x1, y1 = get_vertex(xc, yc, 1)
    local x2, y2 = get_vertex(xc, yc, 2)
    local x3, y3 = get_vertex(xc, yc, 3)
    local x4, y4 = get_vertex(xc, yc, 4)
    local x5, y5 = get_vertex(xc, yc, 5)
    draw_line(x0, y0, x1, y1)
    draw_line(x1, y1, x2, y2)
    draw_line(x2, y2, x3, y3)
    if leftcol then
        -- need to draw NW edge
        draw_line(x3, y3, x4, y4)
    end
    if toprow then
        -- need to draw top edge and NE edge
        draw_line(x4, y4, x5, y5)
        draw_line(x5, y5, x0, y0)
    elseif leftcol then
        -- need to draw top edge
        draw_line(x4, y4, x5, y5)
    end
end

--------------------------------------------------------------------------------

local function draw_bounded_grid()
    local minx, miny, wd, ht = table.unpack( get_visible_rect() )
    local maxx = minx+wd-1
    local maxy = miny+ht-1
    
    if edgelen < mingrid then
        -- fill bounded diamond or strip with state 0 color
        ov("rgba "..state_rgba[0])
        local offset
        if edgelen == 2 and (gridht == 0 or gridwd == 0) then
            -- offset needs to be larger so strip is wider
            offset = 0.5
        else
            offset = (edgelen - 1) / 3
        end
        draw_boundary_line(minx-offset, miny-offset, maxx+offset, miny-offset)
        draw_boundary_line(maxx+offset, miny-offset, maxx+offset, maxy+offset)
        draw_boundary_line(maxx+offset, maxy+offset, minx-offset, maxy+offset)
        draw_boundary_line(minx-offset, maxy+offset, minx-offset, miny-offset)
        if edgelen > 1 or (edgelen == 1 and wd > 1 and ht > 1) then
            local x, y = cell_to_hexagon(minx + int(wd/2), miny + int(ht/2))
            ov("flood "..x.." "..y)
        end
    else
        ov("rgba "..grid_rgba)
        for y = miny, maxy do
            for x = minx, maxx do
                local xc, yc = cell_to_hexagon(x, y)
                draw_bounded_hexagon(xc, yc, y == miny, x == minx)
                if g.getcell(x, y) == 0 then
                    -- fill hexagon with state 0 color
                    fill_hexagon(xc, yc, 0)
                    ov("rgba "..grid_rgba)
                end
            end
        end
    end
end

--------------------------------------------------------------------------------

local function refresh()
    init_values()
    if gridwd > 0 or gridht > 0 then
        -- fill overlay with border color
        local br, bg, bb = g.getcolor("border")
        ov("rgba "..br.." "..bg.." "..bb.." 255")
        ov("fill")
        draw_bounded_grid()
    else
        -- fill overlay with state 0 color
        ov("rgba "..state_rgba[0])
        ov("fill")
        if edgelen >= mingrid then
            draw_hex_grid()
        end
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
    local _, x, y, button, mods = split(event)
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

local function edit_hexagons(x, y)
    -- get axial coords of hexagon containing clicked pixel
    local q, r = pixel_to_hex(x, y)
    
    -- check if hexagon is outside bounded grid
    if outside_grid(q, r) then return end

    -- update corresponding cell in current layer
    local xcell = q + r + xpos
    local ycell = r + ypos
    local state = g.getcell(xcell, ycell)
    local drawstate = g.getoption("drawingstate")
    if state == drawstate then
        -- kill cell
        g.setcell(xcell, ycell, 0)
    else
        g.setcell(xcell, ycell, drawstate)
    end
    
    -- get central pixel of hexagon
    local xc = int(hextox * q + midx)
    local yc = int(hextoy * (r + q/2) + midy)
    
    -- update hexagon
    if state == drawstate then
        -- erase hexagon
        fill_hexagon(xc, yc, 0)
    else
        fill_hexagon(xc, yc, drawstate)
        if drawstate > 0 then
            livehexagons[#livehexagons + 1] = {xc, yc, drawstate}
        end
    end
    
    -- display overlay and show new population count in status bar
    g.update()
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

local function set_position(x, y)
    -- avoid error message from gp.setposint if grid is bounded
    if gridwd > 0 then
        local bminx = -int(gridwd / 2)
        local bmaxx = bminx + gridwd - 1
        if x < bminx or x > bmaxx then return end
    end
    if gridht > 0 then
        local bminy = -int(gridht / 2)
        local bmaxy = bminy + gridht - 1
        if y < bminy or y > bmaxy then return end
    end
    gp.setposint(x, y)
end

--------------------------------------------------------------------------------

local function check_position()
    -- refresh if current layer's position has changed
    local x0, y0 = gp.getposint()
    if x0 ~= xpos or y0 ~= ypos then
        xpos = x0
        ypos = y0
        refresh()
    end
end

--------------------------------------------------------------------------------

local function move_hexagons(x, y)
    -- get axial coords of hexagon containing clicked pixel
    local q, r = pixel_to_hex(x, y)
    
    -- check if hexagon is outside bounded grid
    if outside_grid(q, r) then return end
    
    -- drag clicked hexagon to new position
    local prevq, prevr = q, r
    while true do
        local event = g.getevent()
        if event:find("^mup") then break end
        local xy = ov("xy")
        if #xy > 0 then
            x, y = split(xy)
            q, r = pixel_to_hex(tonumber(x), tonumber(y))
            if q ~= prevq or r ~= prevr then
                if not outside_grid(q, r) then
                    local deltaq = q - prevq
                    local deltar = r - prevr
                    --[[
                                    ^
                                    |  move up/down if deltaq == 0
                                    v
                                 }____{             _
                                /      \            /|
                         }-----{  q,r-1 }-----{   |/_  move NE/SW if deltaq == -deltar
                        /       \      /       \ 
                       { q-1,r   }----{ q+1,r-1 }
                        \       /      \       /
                         }-----{  q,r   }-----{
                        /       \      /       \
                       { q-1,r+1 }----{ q+1,r   }
                        \       /      \       /   _
                         }-----{  q,r+1 }-----{   |\   move NW/SE if deltar == 0
                                \      /           _\|
                                 }----{
                    --]]
                    if deltar ~= 0 and deltaq == 0 then
                        -- move current layer diagonally to move hexagons up/down
                        set_position(xpos - deltar, ypos - deltar)
                        check_position()
                    elseif deltaq ~= 0 and deltar == 0 then
                        -- move current layer left/right to move hexagons NW/SE
                        set_position(xpos - deltaq, ypos)
                        check_position()
                    elseif deltaq == -deltar then
                        -- move current layer up/down to move hexagons NE/SW
                        set_position(xpos, ypos + deltaq)
                        check_position()
                    else
                        -- move hexagons NE/SW until prevr = r
                        deltaq = prevr - r
                        ypos = ypos + deltaq
                        set_position(xpos, ypos)
                        prevq = prevq + deltaq
                        
                        -- now move hexagons NW/SE until prevq = q
                        deltaq = q - prevq
                        set_position(xpos - deltaq, ypos)
                        check_position()
                    end
                end
                prevq, prevr = q, r
            end
        end
    end
end

--------------------------------------------------------------------------------

local function pan(event)
    -- user pressed an arrow key
    if g.getoption("showoverlay") == 0 then
        -- just pan the current layer
        g.doevent(event)
    else
        local _, key, mods = split(event)
        if mods == "none" then
            -- to pan overlay orthogonally we need to pan layer diagonally
            if key == "left" then
                set_position(xpos-1, ypos+1)
            elseif key == "right" then
                set_position(xpos+1, ypos-1)
            elseif key == "up" then
                set_position(xpos-1, ypos-1)
            elseif key == "down" then
                set_position(xpos+1, ypos+1)
            end
        elseif mods == "shift" then
            -- to pan overlay diagonally we need to pan layer orthogonally
            if key == "left" then
                set_position(xpos-1, ypos)
            elseif key == "right" then
                set_position(xpos+1, ypos)
            elseif key == "up" then
                set_position(xpos, ypos-1)
            elseif key == "down" then
                set_position(xpos, ypos+1)
            end
        end
    end
end

--------------------------------------------------------------------------------

local function do_click_in_overlay(event)
    -- process a mouse click in overlay like "oclick 100 20 left none"
    local _, x, y, button, mods = split(event)
    x = tonumber(x)
    y = tonumber(y)
    
    if button ~= "left" then return end
    if mods ~= "none" and mods ~= "shift" then return end

    if g.getcursor() == "Draw" then
        edit_hexagons(x, y)

    elseif g.getcursor() == "Pick" then
        local state = get_state( ov("get "..x.." "..y) )
        if state >= 0 then
            g.setoption("drawingstate", state)
            g.update()  -- updates edit bar
        end

    elseif g.getcursor() == "Move" then
        move_hexagons(x, y)

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
        check_position()
        
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
