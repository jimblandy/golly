--[[
If the current rule uses a hexagonal neighborhood then create
an overlay that shows the current pattern in a hexagonal grid.
Much thanks to http://www.redblobgames.com/grids/hexagons/.
Author: Andrew Trevorrow (andrew@trevorrow.com), August 2016.
--]]

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local op = require "oplus"
local int = gp.int
local round = gp.round
local split = gp.split
local draw_line = op.draw_line

local ov = g.overlay
local ovt = g.ovtable

-- minor optimizations
local abs = math.abs

-- the following are set in create_overlay()
local state_rgba = {}   -- table of RGBA strings for each state
local show_grid_rgba    -- RGBA string for showing grid lines
local hide_grid_rgba    -- RGBA string for hiding grid lines
local grid_rgba         -- set to one of the above

-- this array remembers which hexagons are alive
local livehexagons = {}

-- get pixel dimensions of current layer (used to create/resize overlay)
local viewwd, viewht = g.getview(g.getlayer())

-- keep track of the layer's middle pixel for drawing the central hexagon
local midx, midy = int(viewwd/2), int(viewht/2)

-- keep track of the position of the current layer (the central cell)
local xpos, ypos = gp.getposint()

-- keep track of the magnification  of the current layer
local currmag = g.getmag()

local edgelen       -- current length of a hexagon edge (see set_hex_size)
local minedge = 1   -- minimum edgelen
local maxedge = 22  -- maximum edgelen
local mingrid = 3   -- minimum edgelen at which to draw hexagonal grid

-- this array is used to set edgelen when currmag is 1 to 5 (scale = 1:2 to 1:32)
local hexsize = { 2, 3, 6, 11, maxedge }

-- the following depend on edgelen and are set in create_hextile()
local hextilewd     -- width of tile used to draw hex grid
local hextileht     -- height of tile used to draw hex grid
local hexwd         -- width of 1 hexagon
local hexht         -- height of 1 hexagon
local xrad          -- x distances to each vertex
local yrad          -- y distances to each vertex
local hextox        -- used to find central x coord of a hexagon
local hextoy        -- used to find central y coord of a hexagon

-- get width and height of the universe (in cells, bounded if either > 0)
local gridwd = g.getwidth()
local gridht = g.getheight()

-- these controls are created in create_overlay
local ssbutton      -- Start/Stop button
local sbutton       -- Step button
local s1button      -- +1 button
local rbutton       -- Reset button
local fbutton       -- Fit button
local hbutton       -- ? button
local ebutton       -- X button
local zslider       -- slider for zooming in/out

local toolbarht     -- height of tool bar

local generating = false

--------------------------------------------------------------------------------

local function check_rule()
    -- check if the current rule uses a hexagonal neighborhood
    if not op.hexrule() then
        if g.getalgo() == "RuleLoader" then
            g.warn(
[[If the current rule does not use
a hexagonal neighborhood then the
results will look a bit strange!]])
            -- let user continue
        else
            g.exit("The current rule does not use a hexagonal neighborhood.")
        end
    end
end

--------------------------------------------------------------------------------

local function get_vertex(xc, yc, i)
    return int(xc + xrad[i] + 0.5), int(yc + yrad[i] + 0.5)
end

--------------------------------------------------------------------------------

local function create_hextile()
    --[[
    initialize values that depend on the current edgelen and
    create a clip for tiling the background in draw_hex_grid()
    that contains one flat-topped hexagon (minus bottom edge)
    with a horizontal edge sticking out from the east vertex:
             ___
            /4 5\
           {3   0}---
            \2 1/
    --]]

    -- these arrays are used in get_vertex
    xrad = {}
    yrad = {}
    for i = 0, 5 do
        local angle_rad = math.pi / 180 * 60 * i
        xrad[i] = edgelen * math.cos(angle_rad)
        yrad[i] = edgelen * math.sin(angle_rad)
    end

    local x0, y0 = get_vertex(midx, midy, 0) -- E
    local x1, y1 = get_vertex(midx, midy, 1) -- SE
    local x2, y2 = get_vertex(midx, midy, 2) -- SW
    local x3, y3 = get_vertex(midx, midy, 3) -- W
    local x4, y4 = get_vertex(midx, midy, 4) -- NW
    local x5, y5 = get_vertex(midx, midy, 5) -- NE

    hexwd = x0 - x3
    hexht = y1 - y5

    -- these values are used to find the x,y coords of a hexagon's center
    hextox = edgelen * 3/2
    hextoy = hexht

    -- create the clip
    hextilewd = hexwd + edgelen
    hextileht = hexht
    ov("rgba "..state_rgba[0])
    ovt{"fill", x3, y4, hextilewd, hextileht}
    ov("rgba "..grid_rgba)
    draw_line(x0, y0, x1, y1)
    -- skip bottom edge
    -- draw_line(x1, y1, x2, y2)
    draw_line(x2, y2, x3, y3)
    draw_line(x3, y3, x4, y4)
    draw_line(x4, y4, x5, y5)
    draw_line(x5, y5, x0, y0)
    -- draw horizontal edge from right-most vertex
    draw_line(x0, y0, x0 + edgelen, y0)
    ov("copy "..x3.." "..y4.." "..hextilewd.." "..hextileht.." hextile")
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

local function pixel_to_hex(x, y)
    local q = (x - midx) / hextox
    local r = (y - midy) / hextoy - q / 2

    return cube_round(q, -q-r, r)
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
    x = int(hextox * q + midx + 0.5)
    y = int(hextoy * (r + q/2) + midy + 0.5)

    return x, y
end

--------------------------------------------------------------------------------

local function fill_hexagon(xc, yc, state)
    ov("rgba "..state_rgba[state])

    if edgelen < mingrid then
        if edgelen == 1 then
            ovt{"set", xc, yc}
        else
            local x = int(xc - edgelen/2 + 0.5)
            local y = int(yc - edgelen/2 + 0.5)
            ovt{"fill", x, y, edgelen, edgelen}
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
            if xc - edgelen < viewwd then
                xc = viewwd-1
                check_pixel = true
            else
                return
            end
        end
        if yc < toolbarht then
            if yc + hexht/2 >= toolbarht then
                yc = toolbarht
                check_pixel = true
            else
                return
            end
        elseif yc >= viewht then
            if yc - hexht/2 < viewht then
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

    -- get live cells that will be visible in current hex grid
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

        livehexagons[#livehexagons + 1] = {xc, yc}
    end
end

--------------------------------------------------------------------------------

local function kill_live_cells()
    for _, hexcell in ipairs(livehexagons) do
        local xc, yc = table.unpack(hexcell)
        fill_hexagon(xc, yc, 0)
    end
    livehexagons = nil
end

--------------------------------------------------------------------------------

local function draw_hex_grid()
    -- draw middle hexagon
    local x = int(midx - edgelen + 0.5)
    local y = int(midy - hextileht/2 + 0.5)

    local xy = {"paste"}
    local xyi = 2

    xy[xyi] = x
    xy[xyi + 1] = y
    xyi = xyi + 2

    -- draw middle row of hexagons
    while x > 0 do
        x = x - hextilewd
        xy[xyi] = x
        xy[xyi + 1] = y
        xyi = xyi + 2
    end
    x = int(midx - edgelen + 0.5) + hextilewd
    while x < viewwd do
        xy[xyi] = x
        xy[xyi + 1] = y
        xyi = xyi + 2
        x = x + hextilewd
    end
    xy[xyi] = "hextile"
    ovt(xy)

    -- copy middle row and paste above and below until overlay is tiled
    ov("copy 0 "..y.." "..viewwd.." "..hextileht.." row")
    xy = {"paste"}
    xyi = 2
    while y > 0 do
        y = y - hextileht
        xy[xyi] = 0
        xy[xyi + 1] = y
        xyi = xyi + 2
    end
    y = int(midy - hextileht/2 + 0.5) + hextileht
    while y < viewht do
        xy[xyi] = 0
        xy[xyi + 1] = y
        xyi = xyi + 2
        y = y + hextileht
    end
    xy[xyi] = "row"
    ovt(xy)
end

--------------------------------------------------------------------------------

local function draw_boundary_line(x1, y1, x2, y2)
    x1, y1 = cell_to_hexagon(x1, y1)
    x2, y2 = cell_to_hexagon(x2, y2)
    ovt{"line", x1, y1, x2, y2}
end

--------------------------------------------------------------------------------

local function draw_outer_hexagon(xc, yc, toprow, bottomrow, leftcol, rightcol)
    local x0, y0 = get_vertex(xc, yc, 0)
    local x1, y1 = get_vertex(xc, yc, 1)
    local x2, y2 = get_vertex(xc, yc, 2)
    local x3, y3 = get_vertex(xc, yc, 3)
    local x4, y4 = get_vertex(xc, yc, 4)
    local x5, y5 = get_vertex(xc, yc, 5)
    if toprow then
        -- draw top edge and NE edge
        draw_line(x4, y4, x5, y5)
        draw_line(x5, y5, x0, y0)
    end
    if bottomrow then
        -- draw bottom edge and SW edge
        draw_line(x1, y1, x2, y2)
        draw_line(x2, y2, x3, y3)
    end
    if leftcol then
        -- draw NW edge
        draw_line(x3, y3, x4, y4)
        if not toprow then
            -- draw top edge
            draw_line(x4, y4, x5, y5)
        end
    end
    if rightcol then
        -- draw SE edge
        draw_line(x0, y0, x1, y1)
        if not bottomrow then
            -- draw bottom edge
            draw_line(x1, y1, x2, y2)
        end
    end
end

--------------------------------------------------------------------------------

local function draw_inner_hexagon(xc, yc, toprow, leftcol)
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

local function border_visible()
    -- return true if any corner hexagon is outside the bounded grid
    if outside_grid( pixel_to_hex(0,0) ) then return true end
    if outside_grid( pixel_to_hex(0,viewht-1) ) then return true end
    if outside_grid( pixel_to_hex(viewwd-1,0) ) then return true end
    if outside_grid( pixel_to_hex(viewwd-1,viewht-1) ) then return true end
    return false
end

--------------------------------------------------------------------------------

local function pixel_in_overlay(x, y)
    return x >= 0 and x < viewwd and y >= 0 and y < viewht
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
            local xc, yc = cell_to_hexagon(minx + int(wd/2), miny + int(ht/2))
            ov("flood "..xc.." "..yc)
        end
    else
        -- draw the outer edges of the outer hexagons
        ov("rgba "..grid_rgba)
        for y = miny, maxy do
            for x = minx, maxx do
                if x == minx or x == maxx or y == miny or y == maxy then
                    local xc, yc = cell_to_hexagon(x, y)
                    draw_outer_hexagon(xc, yc, y == miny, y == maxy, x == minx, x == maxx)
                end
            end
        end

        -- flood the interior with state 0 color
        ov("rgba "..state_rgba[0])
        local xc, yc = cell_to_hexagon(minx + int(wd/2), miny + int(ht/2))
        if pixel_in_overlay(xc, yc) then
            ov("flood "..xc.." "..yc)
        else
            -- central hexagon is not visible so search border hexagons
            for y = miny, maxy do
                for x = minx, maxx do
                    if x == minx or x == maxx or y == miny or y == maxy then
                        local xc, yc = cell_to_hexagon(x, y)
                        if pixel_in_overlay(xc, yc) then
                            ov("flood "..xc.." "..yc)
                            goto done
                        end
                    end
                end
            end
            ::done::
        end

        -- draw the inner hexagon edges
        ov("rgba "..grid_rgba)
        for y = miny, maxy do
            for x = minx, maxx do
                local xc, yc = cell_to_hexagon(x, y)
                draw_inner_hexagon(xc, yc, y == miny, x == minx)
            end
        end
    end
end

--------------------------------------------------------------------------------

local function draw_tool_bar()
    ov(op.white)
    ovt{"fill", 0, 0, viewwd, toolbarht}

    -- must draw line at bottom edge of tool bar in case a cell color is white
    ov("rgba "..show_grid_rgba)
    draw_line(0, toolbarht-1, viewwd-1, toolbarht-1)

    local gap = 10
    local x = gap
    local y = int((toolbarht - ssbutton.ht) / 2)

    if y < 10 then return end

    if x >= viewwd then return end
    ssbutton.show(x, y)
    x = x + ssbutton.wd + gap
    if x >= viewwd then return end
    sbutton.show(x, y)
    x = x + sbutton.wd + gap
    if x >= viewwd then return end
    s1button.show(x, y)
    x = x + s1button.wd + gap
    if x >= viewwd then return end
    rbutton.show(x, y)
    x = x + rbutton.wd + gap
    if x >= viewwd then return end
    zslider.show(x, y, edgelen)
    x = x + zslider.wd + gap
    if x >= viewwd then return end
    fbutton.show(x, y)

    x = viewwd - gap - ebutton.wd
    if x >= viewwd then return end
    ebutton.show(x, y)
    x = x - gap - hbutton.wd
    if x >= viewwd then return end
    hbutton.show(x, y)
end

--------------------------------------------------------------------------------

local function refresh()
    create_hextile()

    if (gridwd > 0 or gridht > 0) and border_visible() then
        -- fill overlay with border color
        local br, bg, bb = g.getcolor("border")
        ovt{"rgba", br, bg, bb, 255}
        ovt{"fill"}
        draw_bounded_grid()
    else
        -- fill overlay with state 0 color
        ov("rgba "..state_rgba[0])
        ovt{"fill"}
        if edgelen >= mingrid then
            draw_hex_grid()
        end
    end

    draw_live_cells()

    -- if selection exists then draw translucent selection color over selected cells???!!!

    draw_tool_bar()
    ov("update")
end

--------------------------------------------------------------------------------

local function set_hex_size()
    -- get current layer's magnification
    currmag = g.getmag()

    -- set edgelen so hexagons are roughly the same size as cells
    if currmag <= 0 then
        edgelen = 1
    else
        -- currmag is 1 to 5 (scale is 2^mag)
        edgelen = hexsize[currmag]
    end
end

--------------------------------------------------------------------------------

local function sync_layer_scale()
    -- edgelen has been changed so set layer magnification accordingly
    for i = #hexsize, 1, -1 do
        if edgelen >= hexsize[i] then
            currmag = i
            g.setmag(currmag)
            return
        end
    end
    currmag = 0
    g.setmag(currmag)
end

--------------------------------------------------------------------------------

local function fit_pattern()
    g.fit()

    -- update currmag and edgelen
    set_hex_size()

    if not g.empty() then
        -- may need to reduce edgelen further to see live cells at left/right edges
        local x, y, w, h = table.unpack(g.getrect())
        while edgelen > minedge do
            -- need to update values that depend on edgelen
            create_hextile()
            -- also need to update xpos and ypos
            xpos, ypos = gp.getposint()
            local x1, y1 = cell_to_hexagon(x, y+h-1)
            local x2, y2 = cell_to_hexagon(x+w-1, y)
            if pixel_in_overlay(x1, y1) and pixel_in_overlay(x2, y2) then
                break
            end
            edgelen = edgelen - 1
            sync_layer_scale()
        end
    end

    refresh()

    -- update the status bar to show scale
    g.update()
end

--------------------------------------------------------------------------------

local function zoom_in()
    if g.getoption("showoverlay") == 0 then
        -- zoom in to the current layer's central pixel
        -- (can't use midx and midy because they have been offset by toolbarht/2)
        g.doevent("zoomin "..int(viewwd/2).." "..int(viewht/2))
    elseif edgelen < maxedge then
        -- draw bigger hexagons
        edgelen = edgelen + 1
        refresh()
        sync_layer_scale()
    end
end

--------------------------------------------------------------------------------

local function zoom_out()
    if g.getoption("showoverlay") == 0 then
        -- zoom out from the current layer's central pixel
        -- (can't use midx and midy because they have been offset by toolbarht/2)
        g.doevent("zoomout "..int(viewwd/2).." "..int(viewht/2))
    elseif edgelen > minedge then
        -- draw smaller hexagons
        edgelen = edgelen - 1
        refresh()
        sync_layer_scale()
    end
end

--------------------------------------------------------------------------------

local function zoom_hexagons(event)
    local direction, x, y = split(event)
    x = tonumber(x)
    y = tonumber(y)

    if y < toolbarht then return end

    if direction == "ozoomin" then
        -- try to zoom in to hexagon at x,y???!!!
        zoom_in()
    else
        -- try to zoom out from hexagon at x,y???!!!
        zoom_out()
    end
end

--------------------------------------------------------------------------------

local function click_in_layer(event)
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
    local xc = int(hextox * q + midx + 0.5)
    local yc = int(hextoy * (r + q/2) + midy + 0.5)

    -- update hexagon
    if state == drawstate then
        -- erase hexagon
        fill_hexagon(xc, yc, 0)
    else
        fill_hexagon(xc, yc, drawstate)
        if drawstate > 0 then
            livehexagons[#livehexagons + 1] = {xc, yc}
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
    -- refresh overlay if current layer's position has changed
    local x0, y0 = gp.getposint()
    if x0 ~= xpos or y0 ~= ypos then
        xpos = x0
        ypos = y0
        refresh()
    end
end

--------------------------------------------------------------------------------

local function check_position_and_scale()
    -- refresh overlay if current layer's position or scale has changed
    local call_refresh = false

    local x0, y0 = gp.getposint()
    if x0 ~= xpos or y0 ~= ypos then
        xpos = x0
        ypos = y0
        call_refresh = true
    end

    local newmag = g.getmag()
    if newmag ~= currmag then
        set_hex_size()
        call_refresh = true
    end

    if call_refresh then refresh() end
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
        -- just pan the current layer (assuming the user hasn't changed
        -- the default actions for the arrow keys)
        g.doevent(event)
    else
        local _, key, mods = split(event)
        local amount = 1
        if edgelen < mingrid then
            amount = 10     -- pan further at this scale
        end
        if mods == "none" then
            -- to pan overlay orthogonally we need to pan layer diagonally
            if key == "left" then
                set_position(xpos-amount, ypos+amount)
            elseif key == "right" then
                set_position(xpos+amount, ypos-amount)
            elseif key == "up" then
                set_position(xpos-amount, ypos-amount)
            elseif key == "down" then
                set_position(xpos+amount, ypos+amount)
            end
        elseif mods == "shift" then
            -- to pan overlay diagonally we need to pan layer orthogonally
            if key == "left" then
                set_position(xpos-amount, ypos)
            elseif key == "right" then
                set_position(xpos+amount, ypos)
            elseif key == "up" then
                set_position(xpos, ypos-amount)
            elseif key == "down" then
                set_position(xpos, ypos+amount)
            end
        end
        -- main loop will call check_position_and_scale()
    end
end

--------------------------------------------------------------------------------

local function click_in_overlay(event)
    -- process a mouse click in overlay like "oclick 100 20 left none"
    local _, x, y, button, mods = split(event)
    x = tonumber(x)
    y = tonumber(y)

    if y < toolbarht then return end

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
        -- don't allow selections???!!!

    elseif g.getcursor() == "Zoom In" then
        -- zoom in to clicked hexagon
        zoom_hexagons("ozoomin "..x.." "..y)

    elseif g.getcursor() == "Zoom Out" then
        -- zoom out from clicked hexagon
        zoom_hexagons("ozoomout "..x.." "..y)
    end
end

--------------------------------------------------------------------------------

local function check_grid()
    if g.getoption("showgrid") == 1 then
        if grid_rgba ~= show_grid_rgba then
            grid_rgba = show_grid_rgba
            refresh()
            g.update()
        end
    else
        if grid_rgba ~= hide_grid_rgba then
            grid_rgba = hide_grid_rgba
            refresh()
            g.update()
        end
    end
end

--------------------------------------------------------------------------------

local function toggle_overlay()
    if g.getoption("showoverlay") == 1 then
        g.setoption("showoverlay", 0)
        g.update()
    else
        g.setoption("showoverlay", 1)
        -- force check_position_and_scale to call refresh
        currmag = 666
    end
end

--------------------------------------------------------------------------------

local function toggle_grid()
    g.setoption("showgrid", 1 - g.getoption("showgrid"))
    -- main loop will call check_grid() to do the update
end

--------------------------------------------------------------------------------

local function check_layer_size()
    local newwd, newht = g.getview(g.getlayer())
    if newwd ~= viewwd or newht ~= viewht then
        viewwd, viewht = newwd, newht
        if viewwd < 1 then viewwd = 1 end
        if viewht < 1 then viewht = 1 end
        midx, midy = int(viewwd/2), int(viewht/2 + toolbarht/2)
        ov("resize "..viewwd.." "..viewht)

        -- hide tool bar controls so show calls will use correct background
        ssbutton.hide()
        sbutton.hide()
        s1button.hide()
        rbutton.hide()
        fbutton.hide()
        hbutton.hide()
        ebutton.hide()
        zslider.hide()

        refresh()
    end
end

--------------------------------------------------------------------------------

local arrow_cursor = false

local function check_cursor()
    local xy = ov("xy")
    if #xy > 0 then
        local x, y = split(xy)
        x = tonumber(x)
        y = tonumber(y)
        if y < toolbarht then
            -- mouse is inside tool bar
            if not arrow_cursor then
                ov("cursor arrow")
                arrow_cursor = true
            end
        else
            -- mouse is outside tool bar
            if arrow_cursor then
                ov("cursor current")
                arrow_cursor = false
            end
        end
    end
end

--------------------------------------------------------------------------------

local function update_start_button()
    -- change label in ssbutton without changing the button's width
    if generating then
        ssbutton.setlabel("Stop", false)
    else
        ssbutton.setlabel("Start", false)
    end
    draw_tool_bar()
    ov("update")
end

--------------------------------------------------------------------------------

local function advance(by1)
    if g.empty() then
        g.note("There are no live cells.")
        generating = false
        update_start_button()
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

    draw_tool_bar()
    g.update()
end

--------------------------------------------------------------------------------

local function step()
    advance(false)
    generating = false
    update_start_button()
end

--------------------------------------------------------------------------------

local function step1()
    advance(true)
    generating = false
    update_start_button()
end

--------------------------------------------------------------------------------

local function start_stop()
    generating = not generating
    update_start_button()
end

--------------------------------------------------------------------------------

local function reset()
    g.reset()

    -- erase live hexagons and draw new ones
    kill_live_cells()
    draw_live_cells()

    generating = false
    update_start_button()
    draw_tool_bar()
    g.update()
end

--------------------------------------------------------------------------------

local function zoom_slider(newval)
    -- called if zslider position has changed
    edgelen = newval
    refresh()
    sync_layer_scale()
end

--------------------------------------------------------------------------------

local function show_help()
    if g.getoption("showoverlay") == 0 then
        g.setoption("showoverlay", 1)
        g.update()
    end

    local helptext =
[[
Features:

- Allows toggling between hex layer and current layer.
- Synchronizes hex layer's scale and position with current layer.
- Allows simple editing in hex layer or current layer.
- Allows pattern generation in hex layer or current layer.
- Hex layer can be zoomed in/out, but only from middle hexagon.
- Hex layer can be panned via arrow keys or via hand cursor.
- Hex layer has a tool bar with buttons and a slider for zooming.

Current limitations:

- Doesn't allow editing multiple cells/hexagons via click and drag.
- Doesn't show any selection, nor allow selection to be changed.
- Doesn't support zooming in/out from a particular hexagon.
- Hex layer can't be zoomed out beyond 1:1 scale.

Special keys and their actions:

enter/return  - start/stop generating
tab           - advance by step size
space         - advance by 1
arrows        - pan up/down/left/right
shift-arrows  - pan NE/SE/NW/SW
f             - fit pattern
h             - display this help
l             - show/hide grid lines
o             - show/hide overlay
r             - reset to starting pattern
[             - zoom out
]             - zoom in

               (click or hit any key to close help)]]

    ov("font 11 mono-bold")
    ov(op.black)
    local w, h = split(ov("text temp "..helptext))
    w = tonumber(w) + 20
    h = tonumber(h) + 20
    local x = int((viewwd - w) / 2)
    local y = int((viewht - h) / 2)
    ov(op.gray)
    ovt{"fill", x, y, w, h}
    ov("rgba 255 253 217 255") -- pale yellow (matches Help window)
    ovt{"fill", (x+2), (y+2), (w-4), (h-4)}
    local oldblend = ov("blend 1")
    ovt{"paste", (x+10), (y+10), "temp"}
    ov("blend "..oldblend)
    ov("update")

    while true do
        local event = g.getevent()
        if event:find("^key") or event:find("^oclick") then
            break
        end
    end
    refresh()
end

--------------------------------------------------------------------------------

local function exit_script()
    g.show("")  -- clear message
    g.exit()    -- don't beep
end

--------------------------------------------------------------------------------

local function create_overlay()
    -- overlay covers entire viewport
    ov("create "..viewwd.." "..viewht)

    ov("cursor current")    -- use current Golly cursor

    -- use smaller buttons in our tool bar
    op.buttonht = 20    -- height of buttons (also used for check boxes and sliders)
    op.sliderwd = 12    -- width of slider button (best if even)
    op.textgap = 8      -- gap between edge of button and its label
    op.textfont = "font 10 default-bold"    -- font for labels
    op.textshadowx = 2
    op.textshadowy = 2
    if g.os() == "Linux" then
        op.textfont = "font 10 default"
    end

    -- create tool bar buttons
    ssbutton = op.button("Start", start_stop)
    sbutton = op.button("Step", step)
    s1button = op.button("+1", step1)
    rbutton = op.button("Reset", reset)
    fbutton = op.button("Fit", fit_pattern)
    hbutton = op.button("?", show_help)
    ebutton = op.button("X", exit_script)

    -- create a slider for zooming in/out
    zslider = op.slider("", op.black, (maxedge-minedge+1)*3, minedge, maxedge, zoom_slider)

    toolbarht = 20 + ssbutton.ht
    midy = int(viewht/2 + toolbarht/2)

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
    -- show_grid_rgba and hide_grid_rgba must not match any state colors, so we give them
    -- an alpha value of 254 (this allows fill_hexagon to draw a hexagon with a flood command)
    show_grid_rgba = gridr.." "..gridg.." "..gridb.." 254"
    hide_grid_rgba = currcolors[2].." "..currcolors[3].." "..currcolors[4].." 254"
    if g.getoption("showgrid") == 1 then
        grid_rgba = show_grid_rgba
    else
        grid_rgba = hide_grid_rgba
    end

    -- initialize edgelen
    set_hex_size()
end

--------------------------------------------------------------------------------

local function do_key(event)
    local key = event:sub(5)
    if key == "return none" then
        start_stop()
    elseif key == "space none" then
        step1()
    elseif key == "tab none" then
        step()
    elseif key:find("^up ") or key:find("^down ") or
           key:find("^left ") or key:find("^right ") then
        pan(event)
    elseif key == "f none" then
        fit_pattern()
    elseif key == "h none" then
        show_help()
    elseif key == "l none" then
        toggle_grid()
    elseif key == "o none" then
        toggle_overlay()
    elseif key == "r none" then
        reset()
    elseif key == "] none" then
        zoom_in()
    elseif key == "[ none" then
        zoom_out()
    else
        -- might be a user keyboard shortcut, so do it
        g.doevent(event)
    end
end

--------------------------------------------------------------------------------

function main()
    check_rule()
    create_overlay()
    refresh()

    g.show("Hit escape key to abort script...")
    while true do
        -- check for user input
        local event = op.process( g.getevent() )
        -- event is empty if op.process handled the given event
        if #event > 0 then
            if event:find("^key") then
                do_key(event)
            elseif event:find("^click") then
                click_in_layer(event)
            elseif event:find("^oclick") then
                click_in_overlay(event)
            elseif event:find("^ozoomin") or event:find("^ozoomout") then
                zoom_hexagons(event)
            else
                -- eg. zoomin/zoomout event in current layer
                g.doevent(event)
            end
        else
            if not generating then
                g.sleep(5) -- don't hog the CPU when idle
            end
            -- if size of layer has changed then resize the overlay
            check_layer_size()
            -- update cursor if mouse moves in/out of tool bar
            check_cursor()
        end

        if g.getoption("showoverlay") == 1 then
            -- check if current layer's position or scale has changed
            check_position_and_scale()
        end

        -- check if user toggled grid lines (possibly via View menu)
        check_grid()

        if generating then advance(false) end
    end
end

--------------------------------------------------------------------------------

local oldstack = g.setoption("stacklayers", 0)
local oldbuttons = g.setoption("showbuttons", 0) -- disable translucent buttons

local status, err = xpcall(main, gp.trace)
if err then g.continue(err) end
-- the following code is always executed

ov("delete")
g.setoption("stacklayers", oldstack)
g.setoption("showbuttons", oldbuttons)
