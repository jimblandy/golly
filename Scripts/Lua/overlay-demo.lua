--[[
This script demonstrates the overlay's capabilities.
Authors: Andrew Trevorrow (andrew@trevorrow.com) and Chris Rowett (crowett@gmail.com).
--]]

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local split = gp.split
local int = gp.int
local op = require "oplus"
local maketext = op.maketext
local pastetext = op.pastetext
local draw_line = op.draw_line

local ov = g.overlay
local ovt = g.ovtable

math.randomseed(os.time())  -- init seed for math.random

-- minor optimizations
local rand = math.random
local floor = math.floor
local sin = math.sin
local cos = math.cos
local abs = math.abs

local wd, ht            -- overlay's current width and height
local toggle = 0        -- for toggling alpha blending
local align = "right"   -- for text alignment
local transbg = 0       -- for text transparent background
local shadow = 0        -- for text shadow

local demofont = "font 11 default-bold"

-- buttons for main menu (created in create_menu_buttons)
local batch_button
local blend_button
local cellview_button
local copy_button
local cursor_button
local line_button
local set_button
local fill_button
local mouse_button
local multiline_button
local pos_button
local render_button
local replace_button
local save_button
local scale_button
local text_button
local transition_button

local extra_layer = false
local return_to_main_menu = false

-- timing
local tstart = gp.timerstart
local tsave = gp.timersave
local tvalueall = gp.timervalueall

--------------------------------------------------------------------------------

local function create_overlay()
    ov("create 1000 1000")
    -- main_menu() will resize the overlay to just fit buttons and text
end

--------------------------------------------------------------------------------

local function demotext(x, y, text)
    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    maketext(text)
    pastetext(x, y)
    ov("blend "..oldblend)
    ov("font "..oldfont)
end

--------------------------------------------------------------------------------

local function repeat_test(extratext, palebg)
    extratext = extratext or ""
    palebg = palebg or false

    -- explain how to repeat test or return to the main menu
    local text = "Hit the space bar to repeat this test"..extratext..".\n"
    if g.os() == "Mac" then
        text = text.."Click or hit the return key to return to the main menu."
    else
        text = text.."Click or hit the enter key to return to the main menu."
    end
    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    if palebg then
        -- draw black text
        ov(op.black)
        local _, h = maketext(text)
        pastetext(10, ht - 10 - h)
    else
        -- draw white text with a black shadow
        ov(op.white)
        local _, h = maketext(text, nil, op.white, 2, 2)
        local x = 10
        local y = ht - 10 - h
        pastetext(x, y)
    end
    ov("blend "..oldblend)
    ov("font "..oldfont)

    g.update()

    while true do
        local event = g.getevent()
        if event:find("^oclick") or event == "key return none" then
            -- return to main menu rather than repeat test
            return_to_main_menu = true
            return false
        elseif event == "key space none" then
            -- repeat current test
            return true
        else
            -- might be a keyboard shortcut
            g.doevent(event)
        end
    end
end

--------------------------------------------------------------------------------

local function ms(t)
    return string.format("%.2fms", t)
end

--------------------------------------------------------------------------------

local day = 1

local function test_transitions()
    -- create a clip from the menu screen
    local oldblend = ov("blend 0")
    ov(op.black)
    ovt{"line", 0, 0, (wd - 1), 0, (wd - 1), (ht - 1), 0, (ht - 1), 0, 0}
    ov("copy 0 0 "..wd.." "..ht.." bg")

    -- create the background clip
    ov(op.blue)
    ovt{"fill"}
    local oldfont = ov("font 100 mono")
    ov(op.yellow)
    local w,h = maketext("Golly")
    ov("blend 1")
    pastetext(floor((wd - w) / 2), floor((ht - h) / 2))
    ov("copy 0 0 "..wd.." "..ht.." fg")
    ov("blend 0")
    local pause = 0

    ::restart::
    ovt{"paste", 0, 0, "bg"}
    ov("update")
    local t = g.millisecs()
    while g.millisecs() - t < pause do end

    -- monday: exit stage left
    if day == 1 then
        for x = 0, wd, 10 do
            t = g.millisecs()
            ovt{"paste", 0, 0, "fg"}
            ovt{"paste", -x, 0, "bg"}
            ov("update")
            while g.millisecs() - t < 15 do end
        end

    -- tuesday: duck and cover
    elseif day == 2 then
        ov(op.white)
        for y = 0, ht, 10 do
            t = g.millisecs()
            ovt{"paste", 0, 0, "fg"}
            ovt{"paste", 0, y, "bg"}
            ov("update")
            while g.millisecs() - t < 15 do end
        end

    -- wednesday: slide to the right
    elseif day == 3 then
        for y = 0, ht, 8 do
            ov("copy 0 "..y.." "..wd.." 8 bg"..y)
        end
        local d
        local p
        for x = 0, wd * 2, 20 do
            t = g.millisecs()
            ovt{"paste", 0, 0, "fg"}
            d = 0
            for y = 0, ht, 8 do
                 p = x + 10 * d - wd
                 if p < 0 then p = 0 end
                 ovt{"paste", p, y, "bg"..y}
                 d = d + 1
            end
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        for y = 0, ht, 8 do
            ov("delete bg"..y)
        end

    -- thursday: as if by magic
    elseif day == 4 then
        ovt{"paste", 0, 0, "fg"}
        ov("copy 0 0 "..wd.." "..ht.." blend")
        for a = 0, 255, 5 do
            t = g.millisecs()
            ov("blend 0")
            ovt{"paste", 0, 0, "bg"}
            ov("blend 1")
            ovt{"rgba", 0, 0, 0, a}
            ov("target blend")
            ov("replace *r *g *b *")
            ov("target")
            ovt{"paste", 0, 0, "blend"}
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        ov("delete blend")

    -- friday: you spin me round
    elseif day == 5 then
        local x, y, r
        local deg2rad = 57.3
        for a = 0, 360, 6 do
            t = g.millisecs()
            r = wd / 360 * a
            x = floor(r * sin(a / deg2rad))
            y = floor(r * cos(a / deg2rad))
            ovt{"paste", 0, 0, "fg"}
            ovt{"paste", x, y, "bg"}
            ov("update")
            while g.millisecs() - t < 15 do end
       end

    -- saturday: through the square window
    elseif day == 6 then
        for x = 1, wd / 2, 4 do
            t = g.millisecs()
            local y = x * (ht / wd)
            ov("blend 0")
            ovt{"paste", 0, 0, "bg"}
            ovt{"rgba", 0, 0, 0, 0}
            ovt{"fill", floor(wd / 2 - x), floor(ht / 2 - y), (x * 2), floor(y * 2)}
            ov("copy 0 0 "..wd.." "..ht.." trans")
            ovt{"paste", 0, 0, "fg"}
            ov("blend 1")
            ovt{"paste", 0, 0, "trans"}
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        ov("delete trans")

    -- sunday: people in glass houses
    elseif day == 7 then
        local box = {}
        local n = 1
        local tx, ty
        for y = 0, ht, 16 do
            for x = 0, wd, 16 do
                tx = x + rand(0, floor(wd / 8)) - wd / 16
                ty = ht + rand(0, floor(ht / 2))
                local entry = {}
                entry[1] = x
                entry[2] = y
                entry[3] = tx
                entry[4] = ty
                box[n] = entry
                ov("copy "..x.." "..y.." 16 16 sprite"..n)
                n = n + 1
            end
        end
        for i = 0, 100 do
            t = g.millisecs()
            local a = i / 100
            local x, y
            ovt{"paste", 0, 0, "fg"}
            for j = 1, #box do
                x = box[j][1]
                y = box[j][2]
                tx = box[j][3]
                ty = box[j][4]
                ovt{"paste", floor(x * (1 - a) + tx * a), floor(y * (1 - a) + ty * a), "sprite"..j}
            end
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        for i = 1, #box do
            ov("delete sprite"..i)
        end

    -- bonus day: objects in the mirror are closer than they appear
    elseif day == 8 then
        for x = 1, 100 do
            t = g.millisecs()
            ovt{"paste", 0, 0, "bg"}
            ov("scale best "..floor(wd / 2 - ((wd / 2) * x / 100)).." "..floor(ht / 2 - ((ht / 2) * x / 100)).." "..floor(wd * x / 100).." "..floor(ht * x / 100).." fg")
            ov("update")
            while g.millisecs() - t < 15 do end
        end
    end

    -- next day
    day = day + 1
    if day == 9 then
        day = 1
    end

    ovt{"paste", 0, 0, "fg"}
    ov("update")
    pause = 300
    if repeat_test(" using a different transition", false) then goto restart end

    -- restore settings
    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov("delete fg")
    ov("delete bg")
end

--------------------------------------------------------------------------------

local curs = 0

local function test_cursors()
    ::restart::

    local cmd
    curs = curs + 1
    if curs ==  1 then cmd = "cursor pencil" end
    if curs ==  2 then cmd = "cursor pick" end
    if curs ==  3 then cmd = "cursor cross" end
    if curs ==  4 then cmd = "cursor hand" end
    if curs ==  5 then cmd = "cursor zoomin" end
    if curs ==  6 then cmd = "cursor zoomout" end
    if curs ==  7 then cmd = "cursor arrow" end
    if curs ==  8 then cmd = "cursor current" end
    if curs ==  9 then cmd = "cursor wait" end
    if curs == 10 then cmd = "cursor hidden" curs = 0 end
    ov(cmd)

    ov(op.white)
    ovt{"fill"}
    -- create a transparent hole
    ovt{"rgba", 0, 0, 0, 0}
    ovt{"fill", 100, 100, 100, 100}

    if cmd == "cursor current" then
        cmd = cmd.."\n\n".."The overlay cursor matches Golly's current cursor."
    else
        cmd = cmd.."\n\n".."The overlay cursor will change to Golly's current cursor\n"..
                           "if it moves outside the overlay or over a transparent pixel:"
    end
    ov(op.black)
    demotext(10, 10, cmd)

    if repeat_test(" using a different cursor", true) then goto restart end
    curs = 0
end

--------------------------------------------------------------------------------

local pos = 0

local function test_positions()
    ::restart::

    pos = pos + 1
    if pos == 1 then ov("position topleft") end
    if pos == 2 then ov("position topright") end
    if pos == 3 then ov("position bottomright") end
    if pos == 4 then ov("position bottomleft") end
    if pos == 5 then ov("position middle") pos = 0 end

    ov(op.white)
    ovt{"fill"}
    ovt{"rgba", 0, 0, 255, 128}
    ovt{"fill", 1, 1, -2, -2}

    local text =
[[The overlay can be positioned in the middle
of the current layer, or at any corner.]]
    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    local w, h
    local fontsize = 30
    ov(op.white)
    -- reduce fontsize until text nearly fills overlay width
    repeat
        fontsize = fontsize - 1
        ov("font "..fontsize)
        w, h = maketext(text)
    until w <= wd - 10
    ov(op.black)
    maketext(text, "shadow")
    local x = int((wd - w) / 2)
    local y = int((ht - h) / 2)
    pastetext(x+2, y+2, op.identity, "shadow")
    pastetext(x, y)
    ov("blend "..oldblend)
    ov("font "..oldfont)

    if repeat_test(" using a different position") then goto restart end
    pos = 0
end

--------------------------------------------------------------------------------

local replace = 1
local replacements = {
    [1] = { col = {"rgba", 255, 255, 0, 255}, cmd = "replace 255 0 0 255", desc = "replace red pixels with yellow" },
    [2] = { col = {"rgba", 255, 255, 0, 128}, cmd = "replace 0 0 0 255", desc = "replace black with semi-transparent yellow" },
    [3] = { col = {"rgba", 255, 255, 0, 255}, cmd = "replace !255 0 0 255", desc = "replace non-red pixels with yellow" },
    [4] = { col = {}, cmd = "replace *g *r *# *#", desc = "swap red and green components" },
    [5] = { col = {"rgba", 0, 0, 0, 128}, cmd = "replace *# *# *# *", desc = "make all pixels semi-transparent" },
    [6] = { col = {}, cmd = "replace *r- *g- *b- *#", desc = "invert clip r g b components" },
    [7] = { col = {"rgba", 255, 255, 0, 255}, cmd = "replace 255 0 0 255-64", desc = "replace red pixels with rgba -64 alpha" },
    [8] = { col = {}, cmd = "replace *# *# *# *#-", desc = "make transparent pixels opaque and vice versa" },
    [9] = { col = {"rgba", 255, 255, 0, 255}, cmd = "replace * * * !255", desc = "replace non-opaque pixels with yellow" },
    [10] = { col = {"rgba", 255, 255, 255, 255}, cmd = "replace *a *a *a *", desc = "convert alpha to grayscale" },
    [11] = { col = {}, cmd = "replace *a *b *g *r", desc = "convert RGBA to ABGR" },
    [12] = { col = {"rgba", 255, 255, 0, 255}, cmd = "replace 0 255 0 !255", desc = "replace non-opaque green with yellow" },
    [13] = { col = {"rgba", 255, 255, 0, 255}, cmd = "replace * * * *", desc = "fill (replace any pixel with yellow)" },
    [14] = { col = {}, cmd = "replace *# *# *# *#", desc = "no-op (replace pixels with clip pixels)" },
    [15] = { col = {"rgba", 0, 0, 0, 128}, cmd = "replace *# *# *# *", desc = "make whole overlay semi-transparent", overlay = true },
    [16] = { col = {}, cmd = "replace *#+64 *#+64 *#+64 *#", desc = "make pixels brighter" },
    [17] = { col = {}, cmd = "replace *# *# *# *#-64", desc = "make pixels more transparent" },
    [18] = { col = {}, cmd = "replace *#++ *#++ *#++ *#", desc = "fade to white using increment", overlay = true, loop = true },
    [19] = { col = {}, cmd = "replace *#-4 *#-4 *#-4 *#", desc = "fast fade to black", overlay = true, loop = true }
}

local function test_replace()
    ::restart::

    -- create clip
    local _
    local oldblend = ov("blend 0")
    ovt{"rgba", 0, 0, 0, 0}
    ovt{"fill"}
    ov("blend 1")
    ov(op.black)
    ovt{"fill", 20, 20, 192, 256}
    ov(op.red)
    ovt{"fill", 84, 84, 128, 128}
    ov(op.blue)
    ovt{"fill", 148, 148, 64, 64}
    ovt{"rgba", 0, 255, 0, 128}
    ovt{"fill", 64, 64, 104, 104}
    ovt{"rgba", 255, 255, 255, 64}
    ovt{"fill", 212, 20, 64, 64}
    ovt{"rgba", 255, 255, 255, 128}
    ovt{"fill", 212, 84, 64, 64}
    ovt{"rgba", 255, 255, 255, 192}
    ovt{"fill", 212, 148, 64, 64}
    ovt{"rgba", 255, 255, 255, 255}
    ovt{"fill", 212, 212, 64, 64}
    ov("blend 0")
    ovt{"rgba", 255, 255, 0, 0}
    ovt{"fill", 84, 212, 64, 64}
    ovt{"rgba", 0, 255, 0, 128}
    ovt{"fill", 20, 212, 64, 64}
    ov("blend 1")
    ov(op.white)
    ovt{"line", 20, 20, 275, 20, 275, 275, 20, 275, 20, 20}
    ov("copy 20 20 256 256 clip")

    -- create the background with some text
    local oldfont = ov("font 24 mono")
    ovt{"rgba", 0, 0, 192, 255}
    ovt{"fill"}
    ovt{"rgba", 192, 192, 192, 255}
    local w, h = maketext("Golly")
    ov("blend 1")
    for y = 0, ht - 70, h do
        for x = 0, wd, w do
            pastetext(x, y)
        end
    end

    -- draw the clip
    ovt{"paste", 20, 20, "clip"}

    -- replace clip
    local drawcol = replacements[replace].col
    local replacecmd = replacements[replace].cmd
    if #drawcol ~= 0 then
        -- set RGBA color
        ovt(drawcol)
    end
    -- execute replace and draw clip
    local replaced
    local t1 = g.millisecs()
    if replacements[replace].overlay ~= true then
        ov("target clip")
    end
    if replacements[replace].loop == true then
        -- copy overlay to bigclip
        ov("copy 0 0 0 0 bigclip")
        replaced = 1
        local count = 0
        while replaced > 0 do
            local t = g.millisecs()
            count = count + 1
            -- replace pixels in bigclip
            ov("target bigclip")
            replaced = tonumber(ov(replacecmd))
            -- paste bigclip to the overlay
            ov("target")
            ovt{"paste", 0, 0, "bigclip"}
            -- draw test number over the bigclip
            ov("blend 1")
            ovt{"rgba", 0, 0, 0, 192}
            ovt{"fill", 0, 300, wd, 144}

            -- draw test name
            ov("font 14 mono")
            ov(op.white)
            local testname = "Test "..replace..": "..replacements[replace].desc
            w, _ = maketext(testname, nil, nil, 2, 2)
            pastetext(floor((wd - w) / 2), 310)
            ov("font 22 mono")
            if #drawcol ~= 0 then
                ov(op.yellow)
                w, _ = maketext(table.concat(drawcol, " "), nil, nil, 2, 2)
                pastetext(floor((wd - w) / 2), 340)
            end
            ov(op.yellow)
            w, _ = maketext(replacecmd, nil, nil, 2, 2)
            pastetext(floor((wd - w) / 2), 390)

            -- update display
            g.show("Test "..replace..": pixels replaced in step "..count..": "..replaced)
            ov("update")
            while g.millisecs() - t < 15 do end
        end
    else
        replaced = tonumber(ov(replacecmd))
    end
    t1 = g.millisecs() - t1
    ov("target ")
    if replacements[replace].overlay ~= true then
        ovt{"paste", (wd - 276), 20, "clip"}
    end

    -- draw replacement text background
    if replacements[replace].loop ~= true then
        ov("blend 1")
        ovt{"rgba", 0, 0, 0, 192}
        ovt{"fill", 0, 300, wd, 144}

        -- draw test name
        ov("font 14 mono")
        ov(op.white)
        local testname = "Test "..replace..": "..replacements[replace].desc
        w, _ = maketext(testname, nil, nil, 2, 2)
        pastetext(floor((wd - w) / 2), 310)

        -- draw test commands
        ov("font 22 mono")
        if #drawcol ~= 0 then
            ov(op.yellow)
            w, _ = maketext(table.concat(drawcol, " "), nil, nil, 2, 2)
            pastetext(floor((wd - w) / 2), 340)
        end
        ov(op.yellow)
        w, _ = maketext(replacecmd, nil, nil, 2, 2)
        pastetext(floor((wd - w) / 2), 390)
    end

    -- restore settings
    ov("blend "..oldblend)
    ov("font "..oldfont)

    -- display elapsed time
    if replacements[replace].loop ~= true then
        g.show("Time to replace: "..ms(t1).."  Pixels replaced: "..replaced)
    else
        g.show("Time to replace: "..ms(t1))
    end

    -- next replacement
    replace = replace + 1
    if replace > #replacements then
        replace = 1
    end

    if repeat_test(" with different options") then goto restart end
    ov("target")
end

--------------------------------------------------------------------------------

local function test_copy_paste()
    ::restart::

    local t1 = g.millisecs()

    -- tile the overlay with a checkerboard pattern
    local sqsize = rand(5, 300)
    local tilesize = sqsize * 2

    -- create the 1st tile (2x2 squares) in the top left corner
    ov(op.white)
    ovt{"fill", 0, 0, tilesize, tilesize}
    ov(op.red)
    ovt{"fill", 1, 1, (sqsize-1), (sqsize-1)}
    ovt{"fill", (sqsize+1), (sqsize+1), (sqsize-1), (sqsize-1)}
    ov(op.black)
    ovt{"fill", (sqsize+1), 1, (sqsize-1), (sqsize-1)}
    ovt{"fill", 1, (sqsize+1), (sqsize-1), (sqsize-1)}
    ov("copy 0 0 "..tilesize.." "..tilesize.." tile")

    -- tile the top row
    for x = tilesize, wd, tilesize do
        ovt{"paste", x, 0, "tile"}
    end

    -- copy the top row and use it to tile the remaining rows
    ov("copy 0 0 "..wd.." "..tilesize.." row")
    for y = tilesize, ht, tilesize do
        ovt{"paste", 0, y, "row"}
    end

    ov("delete tile")
    ov("delete row")

    g.show("Time to test copy and paste: "..ms(g.millisecs()-t1))

    if repeat_test(" with different sized tiles") then goto restart end
end

--------------------------------------------------------------------------------

local function bezierx(t, x0, x1, x2, x3)
    local cX = 3 * (x1 - x0)
    local bX = 3 * (x2 - x1) - cX
    local aX = x3 - x0 - cX - bX

    -- compute x position
    local x = (aX * (t ^ 3)) + (bX * (t ^ 2)) + (cX * t) + x0
    return x
end

--------------------------------------------------------------------------------

local themenum = 1

local function test_cellview()
    -- create a new layer
    g.addlayer()
    extra_layer = true
    g.setalgo("QuickLife")
    g.setrule("b3/s23")

    -- resize overlay to cover entire layer
    wd, ht = g.getview( g.getlayer() )
    ov("resize "..wd.." "..ht)
    ov("position topleft")

    -- create a new layer with 50% random fill
    local size = 512
    g.select( {0, 0, size, size} )
    g.randfill(50)
    g.select( {} )

    -- save settings
    local oldblend = ov("blend 0")
    local oldalign = ov("textoption align left")

    -- create text clips
    local exitclip = "exit"
    local oldfont = ov("font 16 roman")
    local exitw = maketext("Click or press any key to return to the main menu.", exitclip, op.white, 2, 2)
    ov("blend 1")

    -- create a cellview (width and height must be multiples of 16)
    local w16 = size & ~15
    local h16 = size & ~15
    ov("cellview 0 0 "..w16.." "..h16)

    -- set the theme
    ov(op.themes[themenum])
    themenum = themenum + 1
    if themenum > #op.themes then
        themenum = 0
    end

    -- initialize the camera
    local angle = 0
    local zoom = 6
    local x = size / 2
    local y = size / 2
    local depth = 0
    local startangle = angle
    local startzoom = zoom
    local startx = x
    local starty = y
    local startdepth = depth
    local targetangle = angle
    local targetzoom = zoom
    local targetx = x
    local targety = y
    local targetdepth = depth
    local anglesteps = 600
    local zoomsteps = 400
    local xysteps = 200
    local depthsteps = 800
    local anglestep = 0
    local zoomstep = 0
    local xystep = 0
    local depthstep = 0
    local firstdepth = true
    local firstxy = true
    local firstangle = true
    local firstzoom = true

    -- create the world view clip
    local worldsize = 256
    ov("create "..worldsize.." "..worldsize.." world")

    -- animate the cells
    local running = true
    while running do
        local t1 = g.millisecs()
        tstart("frame")

        -- check for resize
        local newwd, newht = g.getview(g.getlayer())
        if newwd ~= wd or newht ~= ht then
            -- resize overlay
            if newwd < 1 then newwd = 1 end
            if newht < 1 then newht = 1 end

            -- save new size
            wd = newwd
            ht = newht

            -- resize overlay
            ov("resize "..wd.." "..ht)
        end

        -- stop when key pressed or mouse button clicked
        local event = g.getevent()
        if event == "key f11 none" then
            g.doevent(event)
        else
            if event:find("^key") or event:find("^oclick") then
                running = false
            end
        end

        -- next generation
        tstart("run")
        g.run(1)
        tsave("run")

        -- update cell view from pattern
        tstart("cells")
        ov("updatecells")
        tsave("cells")

        -- set the camera
        ov("camera angle "..angle)
        ov("camera zoom "..zoom)
        ov("camera xy "..x.." "..y)

        -- set the layer depth and number of layers
        ov("celloption depth "..depth)
        ov("celloption layers 6")

        -- update the camera zoom
        if zoomstep == zoomsteps then
            zoomstep = 0
            zoomsteps = rand(600, 800)
            startzoom = zoom
            if rand() > 0.25 or firstzoom then
                targetzoom = rand(2, 10)
                firstzoom = false
            else
                targetzoom = startzoom
            end
        else
            zoom = startzoom + bezierx(zoomstep / zoomsteps, 0, 0, 1, 1) * (targetzoom - startzoom)
            zoomstep = zoomstep + 1
        end

        -- update the camera angle
        if anglestep == anglesteps then
            anglestep = 0
            anglesteps = rand(600, 800)
            startangle = angle
            if rand() > 0.25 or firstangle then
                targetangle = rand(0, 359)
                firstangle = false
            else
                targetangle = startangle
            end
        else
            angle = startangle + bezierx(anglestep / anglesteps, 0, 0, 1, 1) * (targetangle - startangle)
            anglestep = anglestep + 1
        end

        -- update the camera position
        if xystep == xysteps then
            xystep = 0
            xysteps = rand(1000, 1200)
            startx = x
            starty = y
            if rand() > 0.25 or firstxy then
                targetx = rand(0, w16 - 1)
                targety = rand(0, h16 - 1)
                firstxy = false
            else
                targetx = startx
                targety = starty
            end
        else
            x = startx + bezierx(xystep / xysteps, 0, 0, 1, 1) * (targetx - startx)
            y = starty + bezierx(xystep / xysteps, 0, 0, 1, 1) * (targety - starty)
            xystep = xystep + 1
        end

        -- update the layer depth
        if depthstep == depthsteps then
            depthstep = 0
            depthsteps = rand(600, 800)
            startdepth = depth
            if rand() > 0.5 or firstdepth then
                targetdepth = rand()
                firstdepth = false
            else
                targetdepth = 0
            end
        else
            depth = startdepth + bezierx(depthstep / depthsteps, 0, 0, 1, 1) * (targetdepth - startdepth)
            depthstep = depthstep + 1
        end

        -- draw the cell view
        tstart("drawcells")
        ov("blend 0")
        ov("drawcells")
        tsave("drawcells")

        -- draw the world view
        ov("target world")
        ov("camera angle 0")
        ov("camera zoom 1")
        ov("camera xy "..floor(size / 2).." "..floor(size / 2))
        ov("celloption layers 1")
        tstart("worldcells")
        ov("drawcells")
        tsave("worldcells")
        ovt{"rgba", 128, 128, 128, 255}
        ovt{"line", 0, 0, (worldsize - 1), 0}
        ovt{"line", 0, 0, 0, (worldsize - 1)}
        ov("target")
        ovt{"paste", (wd - worldsize), (ht - worldsize), "world"}

        -- draw exit message
        ov("blend 1")
        pastetext(floor((wd - exitw) / 2), 20, op.identity, exitclip)

        tsave("frame")
        g.show("Time to test cellview: "..tvalueall(2))

        -- update the overlay
        ov("update")
        while g.millisecs() - t1 < 15 do end
    end

    -- free clips
    ov("delete world")
    ov("delete "..exitclip)

    -- restore settings
    ov("textoption align "..oldalign)
    ov("font "..oldfont)
    ov("blend "..oldblend)

    -- delete the layer
    g.dellayer()
    extra_layer = false

    g.update()
    return_to_main_menu = true
end

--------------------------------------------------------------------------------

local loaddir = g.getdir("app").."Help/images/"

local function test_scale()
    local loaded = false
    local iw, ih
    local quality = "best"
    local minscale = 0.1
    local maxscale = 8.0
    local scale = 1.0         -- no scaling

    ::restart::

    ov(op.yellow)
    ovt{"fill"}

    local text = "Hit the space bar to load another image.\n"
    if g.os() == "Mac" then
        text = text.."Click or hit the return key to return to the main menu."
    else
        text = text.."Click or hit the enter key to return to the main menu."
    end
    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    ov(op.black)
    local _, h = maketext(text)
    pastetext(10, ht - 10 - h)
    maketext("Hit [ to scale down, ] to scale up, 1 to reset scale to 1.0, Q to toggle quality.")
    pastetext(10, 10)
    ov("font "..oldfont)

    if not loaded then
        -- prompt user to load a BMP/GIF/PNG/TIFF file
        g.update()
        local filetypes = "Image files (*.bmp;*.gif;*.png;*.tiff)|*.bmp;*.gif;*.png;*.tiff"
        local filepath = g.opendialog("Load an image file", filetypes, loaddir, "")
        if #filepath > 0 then
            -- update loaddir by stripping off the file name
            local pathsep = g.getdir("app"):sub(-1)
            loaddir = filepath:gsub("[^"..pathsep.."]+$","")

            -- center image in overlay by first loading the file completely outside it
            -- so we get the image dimensions without changing the overlay
            local imgsize = ov("load "..wd.." "..ht.." "..filepath)
            iw, ih = split(imgsize)
            iw = tonumber(iw)
            ih = tonumber(ih)

            -- now load image into a clip
            ov("create "..iw.." "..ih.." img")
            ov("target img")
            ov("load 0 0 "..filepath)
            ov("target")
            loaded = true
        end
    end

    if loaded then
        -- draw image at current scale
        local scaledw = int(iw*scale+0.5)
        local scaledh = int(ih*scale+0.5)
        local x = int((wd-scaledw)/2+0.5)
        local y = int((ht-scaledh)/2+0.5)
        local t1 = g.millisecs()
        ov("scale "..quality.." "..x.." "..y.." "..scaledw.." "..scaledh.." img")
        g.show("Image width and height: "..scaledw.." "..scaledh..
               "  scale: "..scale.."  quality: "..quality.."  time: "..ms(g.millisecs()-t1))
    end

    ov("blend "..oldblend)
    g.update()

    while true do
        local event = g.getevent()
        if event == "key space none" then
            loaded = false
            scale = 1.0
            goto restart
        elseif event == "key [ none" or event:find("^ozoomout") then
            if scale > minscale then
                scale = scale - 0.1
                if scale < minscale then scale = minscale end
                goto restart
            end
        elseif event == "key ] none" or event:find("^ozoomin") then
            if scale < maxscale then
                scale = scale + 0.1
                if scale > maxscale then scale = maxscale end
                goto restart
            end
        elseif event == "key 1 none" then
            scale = 1.0
            goto restart
        elseif event == "key q none" then
            if quality == "fast" then quality = "best" else quality = "fast" end
            goto restart
        elseif event:find("^oclick") or event == "key return none" then
            return_to_main_menu = true
            return
        elseif #event > 0 then
            g.doevent(event)
        end
    end
end

--------------------------------------------------------------------------------

local savedir = g.getdir("data")

local function test_save()
    ::restart::

    -- create gradient from one random pale color to another
    local r1, g1, b1, r2, g2, b2
    repeat
        r1 = rand(128,255)
        g1 = rand(128,255)
        b1 = rand(128,255)
        r2 = rand(128,255)
        g2 = rand(128,255)
        b2 = rand(128,255)
    until abs(r1-r2) + abs(g1-g2) + abs(b1-b2) > 128
    local rfrac = (r2 - r1) / ht
    local gfrac = (g2 - g1) / ht
    local bfrac = (b2 - b1) / ht
    for y = 0, ht-1 do
        local rval = int(r1 + y * rfrac + 0.5)
        local gval = int(g1 + y * gfrac + 0.5)
        local bval = int(b1 + y * bfrac + 0.5)
        ovt{"rgba", rval, gval, bval, 255}
        ovt{"line", 0, y, wd, y}
    end

    -- create a transparent hole in the middle
    ovt{"rgba", 0, 0, 0, 0}
    ovt{"fill", int((wd-100)/2), int((ht-100)/2), 100, 100}

    g.update()

    -- prompt for file name and location
    local pngpath = g.savedialog("Save overlay as PNG file", "PNG (*.png)|*.png",
                                 savedir, "overlay.png")
    if #pngpath > 0 then
        -- save overlay in given file
        ov("save 0 0 "..wd.." "..ht.." "..pngpath)
        g.show("Overlay was saved in "..pngpath)

        -- update savedir by stripping off the file name
        local pathsep = g.getdir("app"):sub(-1)
        savedir = pngpath:gsub("[^"..pathsep.."]+$","")
    end

    if repeat_test(" and save a different overlay", true) then goto restart end
end

--------------------------------------------------------------------------------

local function test_set()
    local maxx = wd - 1
    local maxy = ht - 1
    local flakes = 10000

    -- create the exit message
    local oldfont = ov(demofont)
    local text
    if g.os() == "Mac" then
        text = "Click or hit the return key to return to the main menu."
    else
        text = "Click or hit the enter key to return to the main menu."
    end
    local w, h = maketext(text, nil, op.white, 2, 2)

    -- create the golly text in yellow (which has no blue component for r g b)
    ov("font 100 mono")
    ov(op.yellow)
    local gw, gh = maketext("Golly", "gollyclip")

    -- fill the background with graduated blue to black
    local oldblend = ov("blend 0")
    local c
    for i = 0, ht - 1 do
        c = 128 * i // ht
        ovt{"rgba", 0, 0, c, 255}
        ovt{"line", 0, i, (wd - 1), i}
    end

    -- draw golly text
    ov("blend 1")
    local texty = ht - gh - h
    pastetext((wd - gw) // 2, texty, op.identity, "gollyclip")

    -- create the background clip
    ov("copy 0 0 "..wd.." "..ht.." bg")
    ov("update")

    -- read the screen
    local fullrow = {}
    for j = 0, wd -1 do
        fullrow[j] = true
    end

    local screen = {}
    for i = 0, ht - 1 do
        local row = {}
        if i < texty or i > texty + gh then
            -- ignore rows outside Golly text clip rows
            row = {table.unpack(fullrow)}
        else
            for j = 0, wd - 1 do
                -- ignore pixels that don't have a red component
                row[j] = (ovt{"get", j, i} == 0)
            end
        end
        screen[i] = row
    end

    -- initialize flake positions
    local yrange = 20 * maxy
    local maxpos = -yrange
    local x  = {}
    local y  = {}
    local dy = {}
    for i = 1, flakes do
        x[i] = rand(0, maxx)
        local yval = 0
        for _ = 1, 10 do
            yval = yval + rand(0, yrange)
        end
        y[i] = -(yval // 10)
        if y[i] > maxpos then
            maxpos = y[i]
        end
        dy[i] = rand() / 5 + 0.8
    end
    for i = 1, flakes do
        y[i] = y[i] - maxpos
    end

    -- initialize the drawing command (will have pixel coordinates added during update)
    local xy = {"set"}

    -- set white for flake colour
    ov(op.white)

    -- loop until key pressed or mouse clicked
    while not return_to_main_menu do
        tstart("frame")
        local event = g.getevent()
        if event:find("^oclick") or event == "key return none" then
            -- return to main menu
            return_to_main_menu = true
        end

        -- draw the background
        local t1 = g.millisecs()
        tstart("background")
        ov("blend 0")
        ovt{"paste", 0, 0, "bg"}
        tsave("background")

        -- update the pixel positions
        tstart("update")
        local lastx, lasty, newx, newy, diry

        -- index into drawing command for coordinates starts after "set" keyword
        local m = 2
        -- no need to clear the coordinate list each frame since pixels are
        -- only ever added so it can be safely reused

        for i = 1, flakes do
            lastx = x[i]
            lasty = y[i]
            diry = dy[i]
            newx = lastx
            newy = lasty
            if lasty >= 0 and lasty < ht - 1 then
                if lasty // 1 == (lasty + diry) // 1 then
                    newy = lasty + diry
                else
                    if screen[(lasty + diry) // 1][lastx] then
                        newy = lasty + diry
                    else
                        local r = rand()
                        if r < 0.05 then
                            local dx = 1
                            if r < 0.025 then
                                dx = -1
                            end
                            if lastx + dx >= 0 and lastx + dx < wd then
                                if screen[(lasty + diry) // 1][lastx + dx] then
                                   newx = lastx + dx
                                   newy = lasty + diry
                                end
                            end
                        end
                    end
                    screen[lasty // 1][lastx] = true
                    screen[newy // 1][newx] = false
                end
            elseif lasty < 0 then
                newy = lasty + diry
            end
            x[i] = newx
            y[i] = newy
            if newy >= 0 and newy < ht then
                -- add pixel coordinates to list for drawing
                xy[m] = newx
                xy[m + 1] = newy
                m = m + 2
            end
        end
        tsave("update")

        -- check if there are pixels to draw
        if m > 2 then
            tstart("draw")
            ovt(xy)
            tsave("draw")
        end

        -- draw the exit message
        ov("blend 1")
        pastetext((wd - w) // 2, 10)

        -- display elapsed time
        tsave("frame")
        local drawn = (m - 2) // 2
        g.show("Time for "..drawn.." pixels: "..tvalueall(2))

        -- wait for frame time
        while g.millisecs() - t1 < 15 do
        end

        -- update display
        ov("update")
    end

    -- free clips and restore settings
    ov("delete gollyclip")
    ov("delete bg")
    ov("blend "..oldblend)
    ov("font "..oldfont)
end

--------------------------------------------------------------------------------

local function dot(x, y)
    local oldrgba = ov(op.green)
    ovt{"set", x, y}
    ov("rgba "..oldrgba)
end

--------------------------------------------------------------------------------

local function draw_rect(x0, y0, x1, y1)
    local oldrgba = ov(op.red)
    local oldwidth = ov("lineoption width 1")
    ovt{"line", x0, y0, x1, y0, x1, y1, x0, y1, x0, y0}
    ov("rgba "..oldrgba)
    ov("lineoption width "..oldwidth)
end

--------------------------------------------------------------------------------

local function draw_ellipse(x, y, w, h)
    ov("ellipse "..x.." "..y.." "..w.." "..h)
    -- enable next call to check that ellipse is inside given rectangle
    --draw_rect(x, y, x+w-1, y+h-1)
end

--------------------------------------------------------------------------------

local function radial_lines(x0, y0, length)
    draw_line(x0, y0, x0+length, y0)
    draw_line(x0, y0, x0, y0+length)
    draw_line(x0, y0, x0, y0-length)
    draw_line(x0, y0, x0-length, y0)
    for angle = 15, 75, 15 do
        local rad = angle * math.pi/180
        local xd = int(length * cos(rad))
        local yd = int(length * sin(rad))
        draw_line(x0, y0, x0+xd, y0+yd)
        draw_line(x0, y0, x0+xd, y0-yd)
        draw_line(x0, y0, x0-xd, y0+yd)
        draw_line(x0, y0, x0-xd, y0-yd)
    end
end

--------------------------------------------------------------------------------

local function vertical_lines(x, y)
    local oldwidth = ov("lineoption width 1")
    local len = 30
    local gap = 15
    for w = 1, 7 do
        ov("lineoption width "..w)
        draw_line(x, y, x, y+len)  dot(x,y)  dot(x,y+len)  x = x+gap
    end
    ov("lineoption width "..oldwidth)
end

--------------------------------------------------------------------------------

local function diagonal_lines(x, y)
    local oldwidth = ov("lineoption width 1")
    local len = 30
    local gap = 15
    for w = 1, 7 do
        ov("lineoption width "..w)
        draw_line(x, y, x+len, y+len)  dot(x,y)  dot(x+len,y+len)  x = x+gap
    end
    ov("lineoption width "..oldwidth)
end

--------------------------------------------------------------------------------

local function nested_ellipses(x, y)
    -- start with a circle
    local w = 91
    local h = 91
    draw_ellipse(x, y, w, h)
    for i = 1, 3 do
        draw_ellipse(x-i*10, y-i*15, w+i*20, h+i*30)
        draw_ellipse(x+i*10, y+i*15, w-i*20, h-i*30)
    end
end

--------------------------------------------------------------------------------

local function show_magnified_pixels(x, y)
    local radius = 5
    local numcols = radius*2+1
    local numrows = numcols
    local magsize = 14
    local boxsize = (1+magsize)*numcols+1

    -- get pixel colors
    local color = {}
    for i = 1, numrows do
        color[i] = {}
        for j = 1, numcols do
            color[i][j] = ov("get "..(x-radius-1+j).." "..(y-radius-1+i))
        end
    end

    -- save area in top left corner big enough to draw the magnifying glass
    local outersize = int(math.sqrt(boxsize*boxsize+boxsize*boxsize) + 0.5)
    ov("copy 0 0 "..outersize.." "..outersize.." outer_bg")
    local oldrgba = ov("rgba 0 0 0 0")
    local oldblend = ov("blend 0")
    ov("fill 0 0 "..outersize.." "..outersize)

    -- draw gray background (ie. grid lines around pixels)
    ov(op.gray)
    local xpos = int((outersize-boxsize)/2)
    local ypos = int((outersize-boxsize)/2)
    ov("fill "..xpos.." "..ypos.." "..boxsize.." "..boxsize)

    -- draw magnified pixels
    for i = 1, numrows do
        for j = 1, numcols do
            if #color[i][j] > 0 then
                ov("rgba "..color[i][j])
                local xv = xpos+1+(j-1)*(magsize+1)
                local yv = ypos+1+(i-1)*(magsize+1)
                ov("fill "..xv.." "..yv.." "..magsize.." "..magsize)
            end
        end
    end

    -- erase outer ring
    local oldwidth = ov("lineoption width "..int((outersize-boxsize)/2))
    ov("rgba 0 0 0 0")
    draw_ellipse(0, 0, outersize, outersize)

    -- surround with a gray circle
    ov(op.gray)
    ov("lineoption width 4")
    ov("blend 1")
    draw_ellipse(xpos-2, ypos-2, boxsize+4, boxsize+4)
    ov("blend 0")

    ov("copy 0 0 "..outersize.." "..outersize.." mag_box")

    -- restore background saved above
    ov("paste 0 0 outer_bg")
    ov("delete outer_bg")

    -- draw magnified circle with center at x,y
    xpos = int(x-outersize/2)
    ypos = int(y-outersize/2)
    ov("blend 1")
    ov("paste "..xpos.." "..ypos.." mag_box")
    ov("delete mag_box")

    -- restore settings
    ov("rgba "..oldrgba)
    ov("blend "..oldblend)
    ov("lineoption width "..oldwidth)
end

--------------------------------------------------------------------------------

local function test_lines()
    -- this test requires a bigger overlay
    local owd = 800
    local oht = 600
    ov("resize "..owd.." "..oht)

    ov(op.white)
    ovt{"fill"}
    ov(op.black)

    local oldblend = ov("blend 0")
    local oldwidth = ov("lineoption width 1")
    local _

    -- non-antialiased lines (line width = 1)
    radial_lines(80, 90, 45)
    -- antialiased lines
    ov("blend 1")
    radial_lines(200, 90, 45)
    ov("blend 0")

    -- thick non-antialiased lines (line width = 2)
    ov("lineoption width 2")
    radial_lines(80, 190, 45)
    -- thick antialiased lines
    ov("blend 1")
    radial_lines(200, 190, 45)
    ov("blend 0")

    -- thick non-antialiased lines (line width = 3)
    ov("lineoption width 3")
    radial_lines(80, 290, 45)
    -- thick antialiased lines
    ov("blend 1")
    radial_lines(200, 290, 45)
    ov("blend 0")

    -- non-antialiased lines with increasing thickness
    vertical_lines(30, 350)
    diagonal_lines(30, 390)

    -- antialiased lines with increasing thickness
    ov("blend 1")
    vertical_lines(150, 350)
    diagonal_lines(150, 390)
    ov("blend 0")

    -- non-antialiased ellipses (line width = 1)
    ov("lineoption width 1")
    nested_ellipses(350, 90)

    -- antialiased ellipses (line width = 1)
    ov("blend 1")
    nested_ellipses(520, 90)
    ov("blend 0")

    -- thick non-antialiased ellipses
    ov("lineoption width 3")
    nested_ellipses(350, 290)

    -- thick antialiased ellipses
    ov("blend 1")
    nested_ellipses(520, 290)
    ov("blend 0")
    ov("lineoption width 1")

    -- test overlapping translucent colors
    ov("blend 1")
    ov("lineoption width 20")
    local oldrgba = ov("rgba 0 255 0 128")
    draw_ellipse(50, 450, 100, 100)
    ovt{"rgba", 255, 0, 0, 128}
    draw_line(50, 450, 150, 550)
    ovt{"rgba", 0, 0, 255, 128}
    draw_line(150, 450, 50, 550)
    ov("blend 0")
    ov("lineoption width 1")
    ov("rgba "..oldrgba)

    -- draw filled ellipses using fill_ellipse function from oplus
    -- (no border if given border width is 0)
    op.fill_ellipse(370, 450, 50, 100, 0, "rgba 0 0 255 128")
    ovt{"rgba", 200, 200, 200, 128} -- translucent gray border
    op.fill_ellipse(300, 450, 100, 80, 15, op.green)
    ov("rgba "..oldrgba)
    op.fill_ellipse(200, 450, 140, 99, 2, "rgba 255 255 0 128")

    -- draw rounded rectangles using round_rect function from oplus
    op.round_rect(670, 50, 60, 40,  10,  3, "")
    op.round_rect(670, 100, 60, 40, 20,  0, "rgba 0 0 255 128")
    op.round_rect(670, 150, 60, 21,  3,  0, op.blue)
    ovt{"rgba", 200, 200, 200, 128} -- translucent gray border
    op.round_rect(700, 30, 70, 200, 35, 10, "rgba 255 255 0 128")
    ov("rgba "..oldrgba)

    -- draw some non-rounded rectangles (radius is 0)
    op.round_rect(670, 200, 10, 8, 0, 3, "")
    op.round_rect(670, 210, 10, 8, 0, 1, op.red)
    op.round_rect(670, 220, 10, 8, 0, 0, op.green)
    op.round_rect(670, 230, 10, 8, 0, 0, "")        -- nothing is drawn

    -- draw solid circles (non-antialiased and antialiased)
    ov("lineoption width 10")
    draw_ellipse(450, 500, 20, 20)
    ov("blend 1")
    draw_ellipse(480, 500, 20, 20)
    ov("blend 0")
    ov("lineoption width 1")

    -- draw solid ellipses (non-antialiased and antialiased)
    ov("lineoption width 11")
    draw_ellipse(510, 500, 21, 40)
    ov("blend 1")
    draw_ellipse(540, 500, 21, 40)
    ov("blend 0")
    ov("lineoption width 1")

    -- create a circular hole with fuzzy edges
    ovt{"rgba", 255, 255, 255, 0}
    ov("blend 0")
    ov("lineoption width 25")
    local x, y, w, h = 670, 470, 50, 50
    draw_ellipse(x, y, w, h)
    ov("lineoption width 1")
    local a = 0
    for _ = 1, 63 do
        a = a + 4
        ovt{"rgba", 255, 255, 255, a}
        -- we need to draw 2 offset ellipses to ensure all pixels are altered
        x = x-1
        w = w+2
        draw_ellipse(x, y, w, h)
        y = y-1
        h = h+2
        draw_ellipse(x, y, w, h)
    end
    ov("lineoption width 1")
    ov("rgba "..oldrgba)

    -- create and draw the exit message
    local oldfont = ov(demofont)
    ov("blend 1")
    local text
    if g.os() == "Mac" then
        text = "Click or hit the return key to return to the main menu."
    else
        text = "Click or hit the enter key to return to the main menu."
    end
    ov(op.black)
    _, h = maketext(text)
    pastetext(10, oht - h - 10)
    maketext("Hit the M key to toggle the magnifying glass.")
    pastetext(10, 10)
    ov("font "..oldfont)

    g.update()

    ov("blend 0")
    ov("copy 0 0 0 0 bg")
    local showing_magnifier = false
    local display_magnifier = true
    local prevx, prevy

    -- loop until enter/return key pressed or mouse clicked
    while true do
        local event = g.getevent()
        if event:find("^oclick") or event == "key return none" then
            -- tidy up and return to main menu
            ov("blend "..oldblend)
            ov("lineoption width "..oldwidth)
            ov("delete bg")
            return_to_main_menu = true
            return
        elseif event == "key m none" then
            -- toggle magnifier
            display_magnifier = not display_magnifier
            if showing_magnifier and not display_magnifier then
                ovt{"paste", 0, 0, "bg"}
                g.show("")
                g.update()
                showing_magnifier = false
            elseif display_magnifier and not showing_magnifier then
                -- force it to appear if mouse is in overlay
                prevx = -1
            end
        else
            -- might be a keyboard shortcut
            g.doevent(event)
        end

        -- track mouse and magnify pixels under cursor
        local xy = ov("xy")
        if #xy > 0 then
            local xv, yv = split(xy)
            xv = tonumber(xv)
            yv = tonumber(yv)
            if xv ~= prevx or yv ~= prevy then
                prevx = xv
                prevy = yv
                ovt{"paste", 0, 0, "bg"}
                if display_magnifier then
                    -- show position and color of x,y pixel in status bar
                    local rgba = ov("get "..xv.." "..yv)
                    show_magnified_pixels(xv, yv)
                    g.show("xy: "..xv.." "..yv.."  rgba: "..rgba)
                    g.update()
                    showing_magnifier = true
                end
            end
        elseif showing_magnifier then
            ovt{"paste", 0, 0, "bg"}
            g.show("")
            g.update()
            showing_magnifier = false
        end
    end
end

--------------------------------------------------------------------------------

local function test_multiline_text()
    ::restart::

    -- resize overlay to cover entire layer
    wd, ht = g.getview( g.getlayer() )
    ov("resize "..wd.." "..ht)

    local oldfont = ov("font 10 mono-bold")   -- use a mono-spaced font

    -- draw solid background
    local oldblend = ov("blend 0")
    ov(op.blue)
    ovt{"fill"}

    local textstr =
[[
"To be or not to be, that is the question;
Whether 'tis nobler in the mind to suffer
The slings and arrows of outrageous fortune,
Or to take arms against a sea of troubles,
And by opposing, end them. To die, to sleep;
No more; and by a sleep to say we end
The heart-ache and the thousand natural shocks
That flesh is heir to — 'tis a consummation
Devoutly to be wish'd. To die, to sleep;
To sleep, perchance to dream. Ay, there's the rub,
For in that sleep of death what dreams may come,
When we have shuffled off this mortal coil,
Must give us pause. There's the respect
That makes calamity of so long life,
For who would bear the whips and scorns of time,
Th'oppressor's wrong, the proud man's contumely,
The pangs of despised love, the law's delay,
The insolence of office, and the spurns
That patient merit of th'unworthy takes,
When he himself might his quietus make
With a bare bodkin? who would fardels bear,
To grunt and sweat under a weary life,
But that the dread of something after death,
The undiscovered country from whose bourn
No traveller returns, puzzles the will,
And makes us rather bear those ills we have
Than fly to others that we know not of?
Thus conscience does make cowards of us all,
And thus the native hue of resolution
Is sicklied o'er with the pale cast of thought,
And enterprises of great pitch and moment
With this regard their currents turn awry,
And lose the name of action.
Soft you now! The fair Ophelia! Nymph,
In thy Orisons be all my sins remembered.

Test non-ASCII: áàâäãåçéèêëíìîïñóòôöõúùûüæøœÿ
                ÁÀÂÄÃÅÇÉÈÊËÍÌÎÏÑÓÒÔÖÕÚÙÛÜÆØŒŸ
]]

    -- toggle the column alignments and transparency
    if align == "left" then
       align = "center"
    else
        if align == "center" then
            align = "right"
        else
            if align == "right" then
                align = "left"
                transbg = 2 - transbg
                if transbg == 2 then
                    shadow = 1 - shadow
                end
            end
        end
    end

    -- set text alignment
    local oldalign = ov("textoption align "..align)

    -- set the text foreground color
    ov(op.white)

    -- set the text background color
    local oldbackground
    local transmsg
    if transbg == 2 then
        oldbackground = ov("textoption background 0 0 0 0")
        transmsg = "transparent background"
    else
        oldbackground = ov("textoption background 0 128 128 255")
        transmsg = "opaque background     "
    end

    local t1 = g.millisecs()

    ov("blend "..transbg)

    -- create the text clip
    if shadow == 0 then
        maketext(textstr)
    else
        maketext(textstr, nil, nil, 2, 2)
    end

    -- paste the clip onto the overlay
    local t2 = g.millisecs()
    t1 = t2 - t1
    pastetext(0, 0)

    -- output timing and drawing options
    local shadowmsg
    if shadow == 1 then
        shadowmsg = "on"
    else
        shadowmsg = "off"
    end
    g.show("Time to test multiline text: maketext "..ms(t1)..
           "  pastetext "..ms(g.millisecs() - t2)..
           "  align "..string.format("%-6s", align)..
           "  "..transmsg.."  shadow "..shadowmsg)

    -- restore old settings
    ov("textoption background "..oldbackground)
    ov("textoption align "..oldalign)
    ov("font "..oldfont)
    ov("blend "..oldblend)

    if repeat_test(" with different text options") then goto restart end
end

--------------------------------------------------------------------------------

local function test_text()
    ::restart::

    local t1 = g.millisecs()

    local oldfont, oldblend, w, h, descent, nextx, _

    oldblend = ov("blend 0")
    ov(op.white) -- white background
    ovt{"fill"}
    ov(op.black) -- black text

    ov("blend 1")
    maketext("FLIP Y")
    pastetext(20, 30)
    pastetext(20, 30, op.flip_y)

    maketext("FLIP X")
    pastetext(110, 30)
    pastetext(110, 30, op.flip_x)

    maketext("FLIP BOTH")
    pastetext(210, 30)
    pastetext(210, 30, op.flip)

    maketext("ROTATE CW")
    pastetext(20, 170)
    pastetext(20, 170, op.rcw)

    maketext("ROTATE ACW")
    pastetext(20, 140)
    pastetext(20, 140, op.racw)

    maketext("SWAP XY")
    pastetext(150, 170)
    pastetext(150, 170, op.swap_xy)

    maketext("SWAP XY FLIP")
    pastetext(150, 140)
    pastetext(150, 140, op.swap_xy_flip)

    oldfont = ov("font 7 default")
    w, h, descent = maketext("tiny")
    pastetext(300, 30 - h + descent)
    nextx = 300 + w + 5

    ov("font "..oldfont)    -- restore previous font
    w, h, descent = maketext("normal")
    pastetext(nextx, 30 - h + descent)
    nextx = nextx + w + 5

    ov("font 20 default-bold")
    _, h, descent = maketext("Big")
    pastetext(nextx, 30 - h + descent)

    ov("font 10 default-bold")
    w = maketext("bold")
    pastetext(300, 40)
    nextx = 300 + w + 5

    ov("font 10 default-italic")
    maketext("italic")
    pastetext(nextx, 40)

    ov("font 10 mono")
    w, h, descent = maketext("mono")
    pastetext(300, 80 - h + descent)
    nextx = 300 + w + 5

    ov("font 12")   -- just change font size
    _, h, descent = maketext("mono12")
    pastetext(nextx, 80 - h + descent)

    ov("font 10 mono-bold")
    maketext("mono-bold")
    pastetext(300, 90)

    ov("font 10 mono-italic")
    maketext("mono-italic")
    pastetext(300, 105)

    ov("font 10 roman")
    maketext("roman")
    pastetext(300, 130)

    ov("font 10 roman-bold")
    maketext("roman-bold")
    pastetext(300, 145)

    ov("font 10 roman-italic")
    maketext("roman-italic")
    pastetext(300, 160)

    ov("font "..oldfont)    -- restore previous font

    ov(op.red)
    w, h, descent = maketext("RED")
    pastetext(300, 200 - h + descent)
    nextx = 300 + w + 5

    ov(op.green)
    w, h, descent = maketext("GREEN")
    pastetext(nextx, 200 - h + descent)
    nextx = nextx + w + 5

    ov(op.blue)
    _, h, descent = maketext("BLUE")
    pastetext(nextx, 200 - h + descent)

    ov(op.yellow)
    w, h = maketext("Yellow on black [] gjpqy")
    ov(op.black)
    ovt{"fill", 300, 210, w, h}
    pastetext(300, 210)

    ov("blend 0")
    ov(op.yellow)             ovt{"fill", 0,   250, 100, 100}
    ov(op.cyan)               ovt{"fill", 100, 250, 100, 100}
    ov(op.magenta)            ovt{"fill", 200, 250, 100, 100}
    ovt{"rgba", 0, 0, 0, 0}  ovt{"fill", 300, 250, 100, 100}
    ov("blend 1")

    ov(op.black)
    maketext("The quick brown fox jumps over 123 dogs.")
    pastetext(10, 270)

    ov(op.white)
    maketext("SPOOKY")
    pastetext(310, 270)

    oldfont = ov("font "..rand(10,150).." default-bold")
    ovt{"rgba", 255, 0, 0, 40}   -- translucent red text
    w, h, descent = maketext("Golly")
    local gollyclip = pastetext(10, 10)

    -- draw box around text
    ovt{"line", 10, 10, (w-1+10), 10, (w+1+10), (h-1+10), 10, (h-1+10), 10, 10}
    -- show baseline
    ovt{"line", 10, (h-1+10-descent), (w-1+10), (h-1+10-descent)}

    -- draw minimal bounding rect over text
    local xoff, yoff, minwd, minht = op.minbox(gollyclip, w, h)
    ovt{"rgba", 0, 0, 255, 20}
    ovt{"fill", (xoff+10), (yoff+10), minwd, minht}

    -- restore blend state and font
    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov(op.black)

    g.show("Time to test text: "..ms(g.millisecs()-t1))

    if repeat_test(" with a different sized \"Golly\"", true) then goto restart end
end

--------------------------------------------------------------------------------

local function test_fill()
    ::restart::

    ov(op.white)
    ovt{"fill"}

    toggle = toggle + 1
    if toggle == 3 then toggle = 0 end
    ov("blend "..toggle)
    math.randomseed(531642)

    local maxx = wd-1
    local maxy = ht-1
    tstart()
    for _ = 1, 1000 do
        ovt{"rgba", rand(0,255), rand(0,255), rand(0,255), rand(0,255)}
        ovt{"fill", rand(0,maxx), rand(0,maxy), rand(100), rand(100)}
    end
    tsave()
    g.show("Time to fill one thousand rectangles: "..tvalueall(2).."  blend "..toggle)
    ovt{"rgba", 0, 0, 0, 0}
    ovt{"fill", 10, 10, 100, 100} -- does nothing when alpha blending is on

    if toggle > 0 then
        ov("blend 0") -- turn off alpha blending
    end

    if repeat_test(" with a different blend setting") then goto restart end
end

--------------------------------------------------------------------------------

local target = 1

local function test_target()
    -- set overlay as the rendering target
    local oldtarget = ov("target")
    local oldfont = ov("font 16 mono")
    local oldblend = ov("blend 0")

    ::restart::

    target = 1 - target

    -- fill the overlay with black
    ov("blend 0")
    ov(op.black)
    ovt{"fill"}

    -- create a 300x300 clip
    ov("create 300 300 clip")

    -- change the clip contents to yellow
    ov("target clip")
    ov(op.yellow)
    ovt{"fill"}

    -- either draw to the overlay or clip
    if target == 0 then
        ov("target clip")
    else
        ov("target")
    end

    -- draw red, green and blue squares
    ov(op.red)
    ovt{"fill", 0, 0, wd, 50}

    ov(op.green)
    ovt{"fill", 0, 50, wd, 50}

    ov(op.blue)
    ovt{"fill", 0, 100, wd, 50}

    -- draw circle
    ov("blend 1")
    ov(op.white)
    op.fill_ellipse(0, 0, 100, 100, 1, op.magenta)

    -- draw some lines
    ov(op.cyan)
    for x = 0, wd, 20 do
        ovt{"line", 0, 0 ,x, ht}
    end

    -- draw text label
    local textstring = "Clip"
    if target ~= 0 then
        textstring = "Overlay"
    end
    ov(op.black)
    maketext(textstring, nil, op.white, 2, 2)
    pastetext(0, 0)

    -- set overlay as the target
    ov("target")

    -- paste the clip
    ov("blend 0")
    ovt{"paste", 200, 0, "clip"}

    if repeat_test(" with a different target") then goto restart end

    -- free clip and restore previous target
    ov("delete clip")
    ov("target "..oldtarget)
    ov("font "..oldfont)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

local batchsize = 1
local maxbatch  = 16384

local function test_batch()
    ::restart::

    ov("blend 0")
    ov(op.black)
    ovt{"fill"}

    -- use fixed seed
    math.randomseed(1234)

    -- udpate the batch size
    batchsize = batchsize * 2
    if batchsize > maxbatch then
        batchsize = 4
    end

    -- random coordinates
    local xyset = { "set" }
    local xyline = { "line" }
    local xyfill = { "fill" }
    local xypaste = { "paste" }
    local items = batchsize
    local reps  = maxbatch // batchsize

    -- points, rectangles, lines and pastes use the same random positions
    local m = 2
    local n = 2
    for _ = 1, items do
        -- points
        xyset[m] = rand(0, wd - 1)
        xyset[m + 1] = rand(0, ht - 1)
        -- lines
        xyline[m] = xyset[m]
        xyline[m + 1] = xyset[m + 1]
        -- pastes
        xypaste[m] = xyset[m]
        xypaste[m + 1] = xyset[m + 1]
        -- rectangles
        xyfill[n] = xyset[m]
        xyfill[n + 1] = xyset[m + 1]
        xyfill[n + 2] = 8
        xyfill[n + 3] = 8
        m = m + 2
        n = n + 4
    end
    xyline[m] = xyset[2]
    xyline[m + 1] = xyset[3]

    -- create paste clip
    local clipname = "testclip"
    ov("create 32 32 "..clipname)
    ov("target "..clipname)
    ov(op.blue)
    ovt{"fill"}
    ov("optimize "..clipname)
    ov("target")
    xypaste[#xypaste + 1] = clipname

    -- time paste one at a time
    tstart("paste")
    for _ = 1, reps do
        m = 2
        for _ = 1, items do
            ov("paste "..xypaste[m].." "..xypaste[m + 1].." "..clipname)
            m = m + 2
        end
    end
    tsave("paste")

    -- time paste one at a time with array
    local xy1 = {"paste", 0, 0, clipname}
    tstart("paste1array")
    for _ = 1, reps do
        m = 2
        for _ = 1, items do
            xy1[2] = xypaste[m]
            xy1[3] = xypaste[m + 1]
            ovt(xy1)
            m = m + 2
        end
    end
    tsave("paste1array")

    -- time drawing all at once
    tstart("pastearray")
    for _ = 1, reps do
        ovt(xypaste)
    end
    tsave("pastearray")

    -- draw random lines
    ov(op.green)

    -- time draw one at a time
    tstart("line")
    for _ = 1, reps do
        m = 2
        for _ = 1, items do
            ov("line "..xyline[m].." "..xyline[m + 1].." "..xyline[m + 2].." "..xyline[m + 3])
            m = m + 2
        end
    end
    tsave("line")

    -- time draw one at a time with array
    xy1 = {"line", 0, 0, 0, 0}
    tstart("line1array")
    for _ = 1, reps do
        m = 2
        for _ = 1, items do
            xy1[2] = xyline[m]
            xy1[3] = xyline[m + 1]
            xy1[4] = xyline[m + 2]
            xy1[5] = xyline[m + 3]
            ovt(xy1)
            m = m + 2
        end
    end
    tsave("line1array")

    -- time drawing all at once
    tstart("linearray")
    for _ = 1, reps do
        ovt(xyline)
    end
    tsave("linearray")

    -- draw random rectangles
    ov(op.red)

    -- time draw one at a time
    tstart("rectangle")
    for _ = 1, reps do
        m = 2
        for _ = 1, items do
            ov("fill "..xyfill[m].." "..xyfill[m + 1].." "..xyfill[m + 2].." "..xyfill[m + 3])
            m = m + 4
        end
    end
    tsave("rectangle")

    -- time draw one at a time with array
    xy1 = {"fill", 0, 0, 0, 0}
    tstart("rectangle1array")
    for _ = 1, reps do
        m = 2
        for _ = 1, items do
            xy1[2] = xyfill[m]
            xy1[3] = xyfill[m + 1]
            xy1[4] = xyfill[m + 2]
            xy1[5] = xyfill[m + 3]
            ovt(xy1)
            m = m + 4
        end
    end
    tsave("rectangle1array")

    -- time drawing all at once
    tstart("rectanglearray")
    for _ = 1, reps do
        ovt(xyfill)
    end
    tsave("rectanglearray")

    -- draw random pixels
    ov(op.white)

    -- time drawing one at a time
    tstart("pixel")
    for _ = 1, reps do
        m = 2
        for _ = 1, items do
            ov("set "..xyset[m].." "..xyset[m + 1])
            m = m + 2
        end
    end
    tsave("pixel")

    -- time drawing one at a time with array
    xy1 = {"set", 0, 0}
    tstart("pixel1array")
    for _ = 1, reps do
        m = 2
        for _ = 1, items do
            xy1[2] = xyset[m]
            xy1[3] = xyset[m + 1]
            ovt(xy1)
            m = m + 2
        end
    end
    tsave("pixel1array")

    -- time drawing all at once
    tstart("pixelarray")
    for _ = 1, reps do
        ovt(xyset)
    end
    tsave("pixelarray")

    g.show("reps: "..reps.."  items: "..items.." "..tvalueall(2))

    -- delete the clip
    ov("delete "..clipname)

    -- create batch string
    if repeat_test(" with a different batch size") then goto restart end
end

--------------------------------------------------------------------------------

local function test_blending()
    ::restart::

    ov(op.white)
    ovt{"fill"}

    toggle = 1 - toggle
    if toggle > 0 then
        ov("blend 1")           -- turn on alpha blending
    end

    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    ov(op.black)
    maketext("Alpha blending is turned on or off using the blend command:")
    pastetext(10, 10)
    maketext("blend "..oldblend)
    pastetext(40, 50)
    maketext("The blend command also controls antialiasing of lines and ellipses:")
    pastetext(10, 300)
    ov("blend "..oldblend)
    ov("font "..oldfont)

    ovt{"rgba", 0, 255, 0, 128}      -- 50% translucent green
    ovt{"fill", 40, 70, 100, 100}

    ovt{"rgba", 255, 0, 0, 128}      -- 50% translucent red
    ovt{"fill", 80, 110, 100, 100}

    ovt{"rgba", 0, 0, 255, 128}      -- 50% translucent blue
    ovt{"fill", 120, 150, 100, 100}

    ov(op.black)
    radial_lines(100, 400, 50)
    draw_ellipse(200, 350, 200, 100)
    draw_ellipse(260, 360, 80, 80)
    draw_ellipse(290, 370, 20, 60)
    -- draw a solid circle by setting the line width to the radius
    local oldwidth = ov("lineoption width 50")
    draw_ellipse(450, 350, 100, 100)
    ov("lineoption width "..oldwidth)

    if toggle > 0 then
        ov("blend 0")           -- turn off alpha blending
    end

    if repeat_test(" with a different blend setting", true) then goto restart end
end

--------------------------------------------------------------------------------

local function test_mouse()
    ::restart::

    ov(op.black)
    ovt{"fill"}
    ov(op.white)

    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    local _, h
    if g.os() == "Mac" then
        maketext("Click and drag to draw.\n"..
                 "Option-click to flood.\n"..
                 "Control-click or right-click to change the color.")
        _, h = maketext("Hit the space bar to restart this test.\n"..
                        "Hit the return key to return to the main menu.", "botlines")
    else
        maketext("Click and drag to draw.\n"..
                 "Alt-click to flood. "..
                 "Control-click or right-click to change the color.")
        _, h = maketext("Hit the space bar to restart this test.\n"..
                        "Hit the enter key to return to the main menu.", "botlines")
    end
    pastetext(10, 10)
    pastetext(10, ht - 10 - h, op.identity, "botlines")
    ov("blend "..oldblend)
    ov("font "..oldfont)

    ov("cursor pencil")
    g.update()

    local mousedown = false
    local prevx, prevy
    while true do
        local event = g.getevent()
        if event == "key space none" then
            goto restart
        end
        if event == "key return none" then
            return_to_main_menu = true
            return
        end
        if event:find("^oclick") then
            local _, x, y, button, mods = split(event)
            if mods == "alt" then
                ov("flood "..x.." "..y)
            else
                if mods == "ctrl" or button == "right" then
                    ovt{"rgba", rand(0,255), rand(0,255), rand(0,255), 255}
                end
                ovt{"set", x, y}
                mousedown = true
                prevx = x
                prevy = y
            end
            g.update()
        elseif event:find("^mup") then
            mousedown = false
        elseif #event > 0 then
            g.doevent(event)
        end

        local xy = ov("xy")
        if #xy > 0 then
            local x, y = split(xy)
            g.show("pixel at "..x..","..y.." = "..ov("get "..x.." "..y))
            if mousedown and (x ~= prevx or y ~= prevy) then
                ovt{"line", prevx, prevy, x, y}
                prevx = x
                prevy = y
                g.update()
            end
        else
            g.show("mouse is outside overlay")
        end
    end
end

--------------------------------------------------------------------------------

local function create_menu_buttons()
    op.buttonht = 22
    op.radius = 12
    op.textgap = 15
    op.textfont = "font 10 default-bold"
    if g.os() == "Linux" then
        op.textfont = "font 10 default"
    end
    op.textshadowx = 2
    op.textshadowy = 2

    local longest = "Text and Transforms"
    blend_button = op.button(       longest, test_blending)
    batch_button = op.button(       longest, test_batch)
    cellview_button = op.button(    longest, test_cellview)
    copy_button = op.button(        longest, test_copy_paste)
    cursor_button = op.button(      longest, test_cursors)
    set_button = op.button(         longest, test_set)
    fill_button = op.button(        longest, test_fill)
    line_button = op.button(        longest, test_lines)
    mouse_button = op.button(       longest, test_mouse)
    multiline_button = op.button(   longest, test_multiline_text)
    pos_button = op.button(         longest, test_positions)
    render_button = op.button(      longest, test_target)
    replace_button = op.button(     longest, test_replace)
    save_button = op.button(        longest, test_save)
    scale_button = op.button(       longest, test_scale)
    text_button = op.button(        longest, test_text)
    transition_button = op.button(  longest, test_transitions)

    -- change labels without changing button widths
    blend_button.setlabel(       "Alpha Blending", false)
    batch_button.setlabel(       "Batch Draw", false)
    cellview_button.setlabel(    "Cell View", false)
    copy_button.setlabel(        "Copy and Paste", false)
    cursor_button.setlabel(      "Cursors", false)
    set_button.setlabel(         "Drawing Pixels", false)
    fill_button.setlabel(        "Filling Rectangles", false)
    line_button.setlabel(        "Lines and Ellipses", false)
    mouse_button.setlabel(       "Mouse Tracking", false)
    multiline_button.setlabel(   "Multi-line Text", false)
    pos_button.setlabel(         "Overlay Positions", false)
    render_button.setlabel(      "Render Target", false)
    replace_button.setlabel(     "Replacing Pixels", false)
    save_button.setlabel(        "Saving the Overlay", false)
    scale_button.setlabel(       "Scaling Images", false)
    text_button.setlabel(        "Text and Transforms", false)
    transition_button.setlabel(  "Transitions", false)
end

--------------------------------------------------------------------------------

local function main_menu()
    local numbutts = 17
    local buttwd = blend_button.wd
    local buttht = blend_button.ht
    local buttgap = 9
    local hgap = 20
    local textgap = 10

    local oldfont = ov("font 24 default-bold")
    ov(op.black)
    local w1, h1 = maketext("Welcome to the overlay!", "bigtext")
    ov(demofont)
    local w2, h2 = maketext("Click on a button to see what's possible.", "smalltext")
    local textht = h1 + textgap + h2

    -- resize overlay to fit buttons and text
    wd = hgap + buttwd + hgap + w1 + hgap
    ht = hgap + numbutts * buttht + (numbutts-1) * buttgap + hgap
    ov("resize "..wd.." "..ht)

    ov("position middle")
    ov("cursor arrow")
    ov(op.gray)
    ovt{"fill"}
    ov(op.white)
    ovt{"fill", 2, 2, -4, -4}

    local x = hgap
    local y = hgap

    blend_button.show(x, y)         y = y + buttgap + buttht
    batch_button.show(x, y)         y = y + buttgap + buttht
    cellview_button.show(x, y)      y = y + buttgap + buttht
    copy_button.show(x, y)          y = y + buttgap + buttht
    cursor_button.show(x, y)        y = y + buttgap + buttht
    set_button.show(x, y)           y = y + buttgap + buttht
    fill_button.show(x, y)          y = y + buttgap + buttht
    line_button.show(x, y)          y = y + buttgap + buttht
    mouse_button.show(x, y)         y = y + buttgap + buttht
    multiline_button.show(x, y)     y = y + buttgap + buttht
    pos_button.show(x, y)           y = y + buttgap + buttht
    render_button.show(x, y)        y = y + buttgap + buttht
    replace_button.show(x, y)       y = y + buttgap + buttht
    save_button.show(x, y)          y = y + buttgap + buttht
    scale_button.show(x, y)         y = y + buttgap + buttht
    text_button.show(x, y)          y = y + buttgap + buttht
    transition_button.show(x, y)

    local oldblend = ov("blend 1")

    x = hgap + buttwd + hgap
    y = int((ht - textht) / 2)
    pastetext(x, y, op.identity, "bigtext")
    x = x + int((w1 - w2) / 2)
    y = y + h1 + textgap
    pastetext(x, y, op.identity, "smalltext")

    ov("blend "..oldblend)
    ov("font "..oldfont)

    g.update()
    g.show(" ") -- clear any timing info

    -- wait for user to click a button
    return_to_main_menu = false
    while true do
        local event = op.process( g.getevent() )
        if return_to_main_menu then
            return
        end
        if #event > 0 then
            -- might be a keyboard shortcut
            g.doevent(event)
        else
            g.sleep(5)  -- don't hog the CPU when idle
        end
    end
end

--------------------------------------------------------------------------------

local function main()
    -- create overlay
    create_overlay()

    -- create menu
    create_menu_buttons()

    -- run main loop
    while true do
        main_menu()
    end
end

--------------------------------------------------------------------------------

local oldoverlay = g.setoption("showoverlay", 1)
local oldbuttons = g.setoption("showbuttons", 0) -- disable translucent buttons
local oldtile = g.setoption("tilelayers", 0)
local oldstack = g.setoption("stacklayers", 0)

local status, err = xpcall(main, gp.trace)
if err then g.continue(err) end
-- the following code is always executed

-- delete the overlay and restore settings saved above
g.check(false)
ov("delete")
g.setoption("showoverlay", oldoverlay)
g.setoption("showbuttons", oldbuttons)
g.setoption("tilelayers", oldtile)
g.setoption("stacklayers", oldstack)
if extra_layer then g.dellayer() end
