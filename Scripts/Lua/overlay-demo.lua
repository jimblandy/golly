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

local ov = g.overlay

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

local demofont = "font 11 default-bold"

-- buttons for main menu (created in create_menu_buttons)
local blend_button
local animation_button
local copy_button
local cursor_button
local line_button
local set_button
local fill_button
local load_button
local mouse_button
local multiline_button
local pos_button
local replace_button
local save_button
local text_button

local return_to_main_menu = false

--------------------------------------------------------------------------------

local day = 1

local function imhereallweek()
    local oldblend = ov("blend 0")
    ov(op.black)
    ov("line 0 0 "..(wd - 1).." 0")
    ov("line 0 0 0 "..(ht - 1))
    ov("line "..(wd - 1).." 0 "..(wd - 1).." "..(ht - 1))
    ov("line 0 "..(ht - 1).." "..(wd - 1).." "..(ht - 1))
    ov("copy 0 0 "..wd.." "..ht.." bg")

    -- monday: exit stage left
    if day == 1 then
        for x = 0, wd, 10 do
            local t = g.millisecs()
            ov("paste 0 0 menu")
            ov("paste "..-x.." 0 bg")
            ov("update")
            while g.millisecs() - t < 15 do end
        end
    -- tuesday: duck and cover
    elseif day == 2 then
        ov(op.white)
        for y = 0, ht, 10 do
            local t = g.millisecs()
            ov("paste 0 0 menu")
            ov("paste 0 "..y.." bg")
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
            local t = g.millisecs()
            ov("paste 0 0 menu")
            d = 0
            for y = 0, ht, 8 do
                 p = x + 10 * d - wd
                 if p < 0 then p = 0 end
                 ov("paste "..p.." "..y.." bg"..y)
                 d = d + 1
            end
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        for y = 0, ht, 8 do
            ov("freeclip bg"..y)
        end
    -- thursday: as if by magic
    elseif day == 4 then
        ov("paste 0 0 menu")
        ov("copy 0 0 "..wd.." "..ht.." blend")
        for a = 0, 255, 5 do
            local t = g.millisecs()
            ov("blend 0")
            ov("paste 0 0 bg")
            ov("blend 1")
            ov("rgba 0 0 0 "..a)
            ov("replace *r *g *b * blend")
            ov("paste 0 0 blend")
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        ov("freeclip blend")
    -- friday: you spin me round
    elseif day == 5 then
        local x, y, r
        local deg2rad = 57.3
        for a = 0, 360, 6 do
            local t = g.millisecs()
            r = wd / 360 * a
            x = floor(r * sin(a / deg2rad))
            y = floor(r * cos(a / deg2rad))
            ov("paste 0 0 menu")
            ov("paste "..x.." "..y.." bg")
            ov("update")
            while g.millisecs() - t < 15 do end
       end
    -- saturday: through the square window
    elseif day == 6 then
        for x = 1, wd / 2, 4 do
            local t = g.millisecs()           
            local y = x * (ht / wd)
            ov("blend 0")
            ov("paste 0 0 bg")
            ov("rgba 0 0 0 0")
            ov("fill "..floor(wd / 2 - x).." "..floor(ht / 2 - y).." "..(x * 2).." "..floor(y * 2))
            ov("copy 0 0 "..wd.." "..ht.." trans")
            ov("paste 0 0 menu")
            ov("blend 1")
            ov("paste 0 0 trans")
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        ov("freeclip trans")
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
            local t = g.millisecs()
            local a = i / 100
            local x, y
            ov("paste 0 0 menu")
            for n = 1, #box do
                x = box[n][1]
                y = box[n][2]
                tx = box[n][3]
                ty = box[n][4]
                ov("paste "..floor(x * (1 - a) + tx * a).." "..floor(y * (1 - a) + ty * a).." sprite"..n)
            end
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        n = 1
        for y = 0, ht, 16 do
            for x = 0, wd, 16 do
                ov("freeclip sprite"..n)
                n = n + 1
            end
        end
    end

    -- next day
    day = day + 1
    if day == 8 then
        day = 1
    end

    -- restore settings
    ov("blend "..oldblend)
    ov("freeclip bg")
end

--------------------------------------------------------------------------------

local function create_overlay()
    ov("create 1000 1000")
    -- main_menu() will resize the overlay to just fit buttons and text
end

--------------------------------------------------------------------------------

local textclip = "textclip"

local function maketext(s, clipname)
    clipname = clipname or textclip
    -- convert given string to text in current font and return
    -- its width and height etc for later use by pastetext
    local w, h, descent = split(ov("text "..clipname.." "..s))
    return tonumber(w), tonumber(h), tonumber(descent)
end

--------------------------------------------------------------------------------

local function pastetext(x, y, transform, clipname)
    transform = transform or op.identity
    clipname = clipname or textclip
    local oldtransform = ov(transform)
    ov("paste "..x.." "..y.." "..clipname)
    ov("transform "..oldtransform)
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
        local w, h = maketext(text)
        pastetext(10, ht - 10 - h)
    else
        -- draw white text with a black shadow
        ov(op.white)
        local w, h = maketext(text)
        ov(op.black)
        maketext(text, "shadow")
        local x = 10
        local y = ht - 10 - h
        pastetext(x+2, y+2, op.identity, "shadow")
        pastetext(x, y)
    end
    ov("blend "..oldblend)
    ov("font "..oldfont)

    g.update()
    
    -- note that if we call op.process below then we'll need to hide all the
    -- main menu buttons so they can't be clicked!!!
    
    while true do
        local event = g.getevent()
        if event:find("^oclick") or event == "key enter none" or event == "key return none" then
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
    
    imhereallweek()
end

--------------------------------------------------------------------------------

local function ms(t)
    return string.format("%.2fms", t)
end

--------------------------------------------------------------------------------

local curs = 0

local function test_cursors()
    ::restart::

    local cmd
    curs = curs + 1
    if curs == 1 then cmd = "cursor pencil" end
    if curs == 2 then cmd = "cursor pick" end
    if curs == 3 then cmd = "cursor cross" end
    if curs == 4 then cmd = "cursor hand" end
    if curs == 5 then cmd = "cursor zoomin" end
    if curs == 6 then cmd = "cursor zoomout" end
    if curs == 7 then cmd = "cursor arrow" end
    if curs == 8 then cmd = "cursor current" end
    if curs == 9 then cmd = "cursor hidden" curs = 0 end
    ov(cmd)

    ov(op.white)
    ov("fill")
    -- create a transparent hole
    ov("rgba 0 0 0 0")
    ov("fill 100 100 100 100")
    
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

    imhereallweek()
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
    ov("fill")
    ov("rgba 0 0 255 128")
    ov("fill 1 1 -2 -2")
    
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
    [1] = { op.yellow, "replace 255 0 0 255", "replace red pixels with yellow" },
    [2] = { "rgba 255 255 0 128", "replace 0 0 0 255", "replace black with semi-tranparent yellow" },
    [3] = { op.yellow, "replace !255 !0 !0 255", "replace non-red pixels with yellow" },
    [4] = { "", "replace *g *r *# *#", "swap red and green components" },
    [5] = { "rgba 0 0 0 128", "replace *# *# *# *", "make all pixels semi-transparent" },
    [6] = { "", "replace *r- *g- *b- *#", "invert r g b components" },
    [7] = { op.yellow, "replace * * * !255", "replace non-opaque pixels with yellow" },
    [8] = { "rgba 255 255 255 255", "replace *a *a *a *", "convert alpha to greyscale" },
    [9] = { op.yellow, "replace 0 255 0 !255", "replace non-opaque green with yellow" },
    [10] = { op.yellow, "replace * * * *", "fill (replace any pixel with yellow)" },
    [11] = { "", "replace *# *# *# *#", "no-op (replace pixels with clip pixels)" }
}

local function test_replace()
    ::restart::

    -- create clip
    local oldblend = ov("blend 0")
    ov("rgba 0 0 0 0")
    ov("fill")
    ov("blend 1")
    ov(op.black)
    ov("fill 20 20 192 256")
    ov(op.red)
    ov("fill 84 84 128 128")
    ov(op.blue)
    ov("fill 148 148 64 64")
    ov("rgba 0 255 0 128")
    ov("fill 64 64 104 104")
    ov("rgba 255 255 255 64")
    ov("fill 212 20 64 64")
    ov("rgba 255 255 255 128")
    ov("fill 212 84 64 64")
    ov("rgba 255 255 255 192")
    ov("fill 212 148 64 64")
    ov("rgba 255 255 255 255")
    ov("fill 212 212 64 64")
    ov("blend 0")
    ov("rgba 0 255 0 128")
    ov("fill 20 212 64 64")
    ov("blend 1")
    ov(op.white)
    ov("line 20 20 275 20")
    ov("line 20 275 275 275")
    ov("line 20 20 20 275")
    ov("line 275 20 275 275")
    ov("copy 20 20 256 256 clip")

    -- create the background with some text
    local oldfont = ov("font 24 mono")
    ov("rgba 0 0 192 255")
    ov("fill")
    ov("rgba 192 192 192 255")
    local w, h = maketext("Golly")
    ov("blend 1")
    for y = 0, ht - 70, h do
        for x = 0, wd, w do
            pastetext(x, y)
        end
    end

    -- draw the clip
    ov("paste 20 20 clip")

    -- replace clip
    local drawcol = replacements[replace][1]
    local replacecmd = replacements[replace][2]
    if drawcol ~= "" then
        -- set RGBA color
        ov(drawcol)
    end
    -- execute replace and draw clip
    local t1 = g.millisecs()
    ov(replacecmd.." clip")
    t1 = g.millisecs() - t1
    ov("paste "..(wd - 276).." 20 clip")

    -- draw replacement text background
    ov("blend 1")
    ov("rgba 0 0 0 192")
    ov("fill 20 300 "..(wd - 40).. " 144")

    -- draw test name
    ov("font 14 mono")
    ov(op.black)
    local testname = "Test "..replace..": "..replacements[replace][3]
    w, h = maketext(testname)
    pastetext(floor((wd - w) / 2) + 2, 310 + 2)
    ov(op.white)
    maketext(testname)
    pastetext(floor((wd - w) / 2), 310)

    -- draw test commands
    ov("font 24 mono")
    if drawcol ~= "" then
        ov(op.black)
        w, h = maketext(drawcol)
        pastetext(floor((wd - w) / 2) + 2, 340 + 2)
        ov(op.yellow)
        maketext(drawcol)
        pastetext(floor((wd - w) / 2), 340)
    end
    ov(op.black)
    w, h = maketext(replacecmd.." clip")
    pastetext(floor((wd - w) / 2) + 2, 390 + 2)
    ov(op.yellow)
    maketext(replacecmd.." clip")
    pastetext(floor((wd - w) / 2), 390)

    -- next replacement
    replace = replace + 1
    if replace > #replacements then
        replace = 1
    end

    -- restore settings
    ov("blend "..oldblend)
    ov("font "..oldfont)

    g.show("Time to replace: "..ms(t1))
    if repeat_test(" with different options") then goto restart end

    imhereallweek()
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
    ov("fill 0 0 "..tilesize.." "..tilesize)
    ov(op.red)
    ov("fill 1 1 "..(sqsize-1).." "..(sqsize-1))
    ov("fill "..(sqsize+1).." "..(sqsize+1).." "..(sqsize-1).." "..(sqsize-1))
    ov(op.black)
    ov("fill "..(sqsize+1).." 1 "..(sqsize-1).." "..(sqsize-1))
    ov("fill 1 "..(sqsize+1).." "..(sqsize-1).." "..(sqsize-1))
    ov("copy 0 0 "..tilesize.." "..tilesize.." tile")
    
    -- tile the top row
    for x = tilesize, wd, tilesize do
        ov("paste "..x.." 0 tile")
    end
    
    -- copy the top row and use it to tile the remaining rows
    ov("copy 0 0 "..wd.." "..tilesize.." row")
    for y = tilesize, ht, tilesize do
        ov("paste 0 "..y.." row")
    end
    
    ov("freeclip tile")
    ov("freeclip row")
    
    g.show("Time to test copy and paste: "..ms(g.millisecs()-t1))
    
    if repeat_test(" with different sized tiles") then goto restart end

    imhereallweek()
end

--------------------------------------------------------------------------------

local function test_animation()
    -- resize overlay to cover entire layer
    wd, ht = g.getview( g.getlayer() )
    ov("resize "..wd.." "..ht)

    local t1, t2

    -- grid size
    local tilewd = 48
    local tileht = 48

    -- glider
    local glider = {
        [1] = { 1, 1, 1, 0, 0, 1, 0, 1, 0 },
        [2] = { 0, 1, 0, 0, 1, 1, 1, 0, 1 },
        [3] = { 0, 1, 1, 1, 0, 1, 0, 0, 1 },
        [4] = { 1, 1, 0, 0, 1, 1, 1, 0, 0 }
    }
    local adjustx = { 0, 0, 0, 1 };
    local adjusty = { 0, -1, 0, 0 };

    -- save settings
    local oldblend = ov("blend 0")
    local oldalign = ov("textoption align left")

    -- create text clips
    local exitclip = "exit"
    local exitshadowclip = "exitshadow"
    local oldfont = ov("font 12 roman")
    ov(op.white)
    local exitw = maketext("Click or press any key to return to the main menu.", exitclip)
    ov(op.black)
    maketext("Click or press any key to return to the main menu.", exitshadowclip)
    local gollyopaqueclip = "clip1"
    local gollytranslucentclip = "clip2"
    ov("font 200 mono")
    local bannertext = "Golly 2.9"
    ov("rgba 255 192 32 144")
    local w, h = maketext(bannertext, gollyopaqueclip)
    ov("rgba 255 192 32 255")
    maketext(bannertext, gollytranslucentclip)

    local creditstext = [[
Golly 2.9


© 2016 The Golly Gang:

Tom Rokicki, Andrew Trevorrow, Tim Hutton, Dave Greene,
Jason Summers, Maks Verver, Robert Munafo, Chris Rowett.




CREDITS




The Pioneers


John Conway
for creating the Game of Life

Martin Gardner
for popularizing the topic in Scientific American




The Programmers


Tom Rokicki
for the complicated stuff

Andrew Trevorrow
for the cross-platform GUI and overlay

Chris Rowett
for rendering and overlay improvements

Tim Hutton
for the RuleTable algorithm

Dave Greene

Jason Summers

Maks Verver

Robert Munafo




The Beta Testers


Thanks to all the bug hunters for their
reports and suggestions, especially:

Dave Greene

Gabriel Nivasch

Dean Hickerson

Brice Due

David Eppstein

Tony Smith

Alan Hensel

Dennis Langdeau

Bill Gosper

Mark Jeronimus

Eric Goldstein




Thanks to


Bill Gosper
for the idea behind the HashLife algorithm

Alan Hensel
for QuickLife ideas and non-totalistic algorithm

David Eppstein
for the B0 rule emulation idea

Eugene Langvagen
for inspiring Golly's scripting capabilities

Stephen Silver
for the wonderful Life Lexicon

Nathaniel Johnston
for the brilliant LifeWiki and the online archive

Julian Smart and all wxWidgets developers
for wxWidgets

Guido van Rossum
for Python

Roberto Ierusalimschy and all Lua developers
for Lua




Pattern Collection


Dave Greene and Alan Hensel

Thanks to everybody who allowed us to distribute
their fantastic patterns, especially:

Nick Gotts

Gabriel Nivasch

David Eppstein

Jason Summers

Stephen Morley

Dean Hickerson

Brice Due

William R. Buckley

David Moore

Mark Owen

Tim Hutton

Renato Nobili

Adam P. Goucher

David Bell
]]
    local oldalign = ov("textoption align center")
    ov("font 14 roman")

    local creditsclip = "credits"
    local creditsshadowclip = "credshadow"

    ov("rgba 128 255 255 255")
    local credwidth, credheight = maketext(creditstext, creditsclip)
    ov(op.black)
    maketext(creditstext, creditsshadowclip)

    -- create graduated background
    local level

    for y = 0, ht / 2 do
        level = 32 + floor(176 * (y * 2 / ht))
        ov("rgba 0 0 "..level.." 255")
        ov("line 0 "..y.." "..wd.." "..y)
        ov("line 0 "..(ht - y).." "..wd.." "..(ht -y))
    end

    -- save background
    local bgclip = "bg"
    ov("copy 0 0 0 0 "..bgclip)

    -- create stars
    local starx = {}
    local stary = {}
    local stard = {}
    local numstars = 1000
    for i = 1, numstars do
        starx[i] = rand(0, wd - 1)
        stary[i] = rand(0, ht - 1)
        stard[i] = floor((i - 1) / 10) / 100
    end

    local textx = wd
    local texty
    local gridx = 0
    local offset
    local running = true
    local gliderx = 0
    local glidery = floor(ht / tileht)
    local gliderframe = 1
    local x, y
    local lastframe = -1
    local credity = ht
    local creditx = floor((wd - credwidth) / 2)
    local credpos

    -- main loop
    while running do
        t2 = g.millisecs()

        -- measure frame draw time
        t1 = g.millisecs()

        -- stop when key pressed or mouse button clicked
        local event = g.getevent()
        if event:find("^key") or event:find("^oclick") then
            running = false
        end

        local timeevent = g.millisecs() - t1

        -- draw background
        ov("blend 0")
        ov("paste 0 0 "..bgclip)

        local timebg = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw gridlines
        offset = floor(gridx)
        ov("rgba 64 64 64 255")
        for i = 0, wd, tilewd do
           ov("line "..(i + offset).." 0 "..(i + offset).." "..ht)
        end
        for i = 0, ht, tileht do
           ov("line 0 "..(i + offset).." "..wd.." "..(i + offset))
        end

        local timegrid = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw stars
        local level = 50
        local i = 1
        ov("rgba "..level.." "..level.." "..level.." 255")
        local lastd = stard[i]

        for i = 1, numstars do
            if (stard[i] ~= lastd) then
                ov("rgba "..level.." "..level.." "..level.." 255")
                level = level + 2
                lastd = stard[i]
            end
            starx[i] = starx[i] + lastd
            if starx[i] > wd then
                starx[i] = 0
                stary[i] = rand(0, ht - 1)
            end
            x = floor(starx[i])
            y = floor(stary[i])
            ov("set "..x.." "..y)
        end

        local timestars = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw glider
        ov(op.white)
        local gx, gy
        local frame = floor(gliderframe)
        if frame == 5 then
            gliderframe = 1
            frame = 1
        end
        if frame ~= lastframe then
            gliderx = gliderx + adjustx[frame]
            glidery = glidery + adjusty[frame]
            lastframe = frame
            if glidery < -3 then
                glidery = floor(ht / tileht)
            end
            if gliderx > wd / tilewd then
                gliderx = -3 
            end
        end
        gliderframe = gliderframe + 0.05
        
        for gy = 0, 2 do
            for gx = 0, 2 do
                if glider[frame][3 * gy + gx + 1] == 1 then
                    x = (gliderx + gx) * tilewd + offset
                    y = (glidery + gy) * tileht + offset
                    ov("fill "..(x + 1).." "..(y + 1).." "..(tilewd - 1).." "..(tileht - 1))
                end
            end
        end

        local timeglider = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw bouncing scrolling text
        ov("blend 1")
        texty = floor(((ht - h) / 2 + (100 * sin(textx / 100))))
        pastetext(textx, texty, op.identity, gollytranslucentclip)

        texty = floor(((ht - h) / 2 - (100 * sin(textx / 100))))
        pastetext(textx, texty, op.identity, gollyopaqueclip)

        local timegolly = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw credits
        credpos = floor(credity)
        pastetext(creditx + 2, credpos + 2, op.identity, creditsshadowclip)
        pastetext(creditx, credpos, op.identity, creditsclip)
        credity = credity - .5
        if credity < -credheight then
            credity = ht
        end

        local timecredits = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw exit message
        pastetext(floor((wd - exitw) / 2 + 2), 20 + 2, op.identity, exitshadowclip)
        pastetext(floor((wd - exitw) / 2), 20, op.identity, exitclip)

        -- update display
        ov("update")

        local timeupdate = g.millisecs() - t1

        -- move grid
        gridx = gridx + 0.2
        if gridx >= tilewd then
            gridx = 0
            gliderx = gliderx + 1
            glidery = glidery + 1
        end

        -- move text
        textx = textx - 2
        if textx <= -w then
            textx = wd
        end

        -- wait until at least 15ms have elapsed
        t1 = g.millisecs()
        local timewait = t1
        while (t1 - t2 < 15) do
            t1 = g.millisecs()
        end
        timewait = t1 - timewait

        -- display frame time
        local frametime = g.millisecs() - t2

        g.show("Time: frame "..ms(frametime).."  event "..ms(timeevent)..
               "  bg "..ms(timebg).."  stars "..ms(timestars)..
               "  glider "..ms(timeglider).."  grid "..ms(timegrid)..
               "  golly "..ms(timegolly).."  credits "..ms(timecredits)..
               "  update "..ms(timeupdate).."  wait "..ms(timewait))
    end

    -- free clips
    ov("freeclip "..exitclip)
    ov("freeclip "..exitshadowclip)
    ov("freeclip "..gollytranslucentclip)
    ov("freeclip "..gollyopaqueclip)
    ov("freeclip "..creditsclip)
    ov("freeclip "..creditsshadowclip)
    ov("freeclip "..bgclip)

    -- restore settings
    ov("textoption align "..oldalign)
    ov("font "..oldfont)
    ov("blend "..oldblend)
    
    -- no point calling repeat_test()
    return_to_main_menu = true
end

--------------------------------------------------------------------------------

local loaddir = g.getdir("app").."Help/images/"

local function test_load()
    ::restart::
    
    ov(op.yellow)
    ov("fill")
    g.update()
    
    -- prompt user to load a BMP/GIF/PNG/TIFF file
    local filetypes = "Image files (*.bmp;*.gif;*.png;*.tiff)|*.bmp;*.gif;*.png;*.tiff"
    local filepath = g.opendialog("Load an image file", filetypes, loaddir, "")
    if #filepath > 0 then
        -- center image in overlay by first loading the file completely outside it
        -- so we get the image dimensions without changing the overlay
        local imgsize = ov("load "..wd.." "..ht.." "..filepath)
        local iw, ih = split(imgsize)
        ov("load "..int((wd-iw)/2).." "..int((ht-ih)/2).." "..filepath)
        g.show("Image width and height: "..imgsize)
        
        -- update loaddir by stripping off the file name
        local pathsep = g.getdir("app"):sub(-1)
        loaddir = filepath:gsub("[^"..pathsep.."]+$","")
    end
    
    if repeat_test(" and load another image", true) then goto restart end

    imhereallweek()
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
    local rfrac = (r2 - r1) / ht;
    local gfrac = (g2 - g1) / ht;
    local bfrac = (b2 - b1) / ht;
    for y = 0, ht-1 do
        local rval = int(r1 + y * rfrac + 0.5)
        local gval = int(g1 + y * gfrac + 0.5)
        local bval = int(b1 + y * bfrac + 0.5)
        ov("rgba "..rval.." "..gval.." "..bval.." 255")
        ov("line 0 "..y.." "..wd.." "..y)
    end

    -- create a transparent hole in the middle
    ov("rgba 0 0 0 0")
    ov("fill "..int((wd-100)/2).." "..int((ht-100)/2).." 100 100")
    
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

    imhereallweek()
end

--------------------------------------------------------------------------------

local function test_set()
    local maxx = wd - 1
    local maxy = ht - 1
    local flakes = 7500

    -- create the exit message
    local oldfont = ov(demofont)
    local text
    if g.os() == "Mac" then
        text = "Click or hit the return key to return to the main menu."
    else
        text = "Click or hit the enter key to return to the main menu."
    end
    ov(op.white)
    local w, h = maketext(text)
    ov(op.black)
    maketext(text, "shadow")

    -- create the golly text
    ov("font 100 mono")
    ov(op.yellow)
    local gw, gh = maketext("Golly", "gollyclip")

    -- fill the background with graduated blue to black
    local oldblend = ov("blend 0")
    local c
    for i = 0, ht - 1 do
        c = floor(i / ht * 128)
        ov("rgba 0 0 "..c.." 255")
        ov("line 0 "..i.." "..(wd - 1).." "..i)
    end

    -- draw golly text
    ov("blend 1")
    local texty = ht - gh - h
    pastetext(floor((wd - gw) / 2), texty, op.identity, "gollyclip")

    -- create the background clip
    ov("copy 0 0 "..wd.." "..ht.." bg")
    ov("update")

    -- read the screen
    local rgba
    local screen = {}
    for i = 0, ht - 1 do
        local row = {}
        if i < texty or i > texty + gh then
            for j = 0, wd - 1 do
                row[j] = true
            end
        else
            for j = 0, wd - 1 do
                rgba = ov("get "..j.." "..i)
                local r, g, b = split(rgba)
                if tonumber(r) == 0 and tonumber(g) == 0 then
                    row[j] = true
                else
                    row[j] = false
                end
            end
        end
        screen[i] = row
    end

    -- initialize flake positions
    local maxpos = -20 * maxy
    local x = {}
    local y = {}
    local dy = {}
    for i = 1, flakes do
        x[i] = rand(0, maxx)
        local yval = 0
        for j = 1, 10 do
            yval = yval + rand(0, 20 * maxy)
        end
        y[i] = floor(-(yval / 10))
        if y[i] > maxpos then
            maxpos = y[i]
        end
        dy[i] = rand() / 5 + 0.8
    end
    for i = 1, flakes do
        y[i] = y[i] - maxpos
    end

    -- loop until key pressed or mouse clicked
    while not return_to_main_menu do
        local event = g.getevent()
        if event:find("^oclick") or event == "key enter none" or event == "key return none" then
            -- return to main menu
            return_to_main_menu = true
        end

        -- draw the background
        local t1 = g.millisecs()
        ov("blend 0")
        ov("paste 0 0 bg")

        -- time drawing the pixels
        ov(op.white)
        local drawn = 0
        local lastx, lasty, newx, newy, diry
        for i = 1, flakes do
            lastx = x[i]
            lasty = y[i]
            diry = dy[i]
            newx = lastx
            newy = lasty
            if lasty >= 0 and lasty < ht - 1 then
                if floor(lasty) == floor(lasty + diry) then
                    newy = lasty + diry
                else
                    if screen[floor(lasty + diry)][lastx] == true then
                        newy = lasty + diry
                    else
                        if rand() < 0.05 then
                            local dx = 1
                            if rand() < 0.5 then
                                dx = -1
                            end
                            if lastx + dx >= 0 and lastx + dx < wd then
                                if screen[floor(lasty + diry)][lastx + dx] == true then
                                   newx = lastx + dx
                                   newy = lasty + diry
                                end
                            end
                        end
                    end
                    screen[floor(lasty)][lastx] = true
                    screen[floor(newy)][newx] = false
                end
            elseif lasty < 0 then
                newy = lasty + diry
            end
            x[i] = newx
            y[i] = newy
            if newy >= 0 and newy < ht then
                ov("set "..newx.." "..floor(newy))
                drawn = drawn + 1
            end
        end

        -- display elapsed time
        g.show("Time to draw "..drawn.." pixels "..ms(g.millisecs() - t1))

        -- draw the exit message
        ov("blend 1")
        pastetext(floor((wd - w) / 2) + 2, 10 + 2, op.identity, "shadow")
        pastetext(floor((wd - w) / 2), 10)

        -- wait for frame time
        while g.millisecs() - t1 < 15 do
        end

        -- update display
        ov("update")
    end

    -- free clips and restore settings
    ov("freeclip bg")
    ov("blend "..oldblend)
    ov("font "..oldfont)

    imhereallweek()
end

--------------------------------------------------------------------------------

local function test_lines()
    local maxx = wd - 1
    local maxy = ht - 1
    local x1 = rand(0, maxx)
    local y1 = rand(0, maxy)
    local x2 = rand(0, maxx)
    local y2 = rand(0, maxy)
    local dx1 = 0.5 + rand(0, 50) / 50
    local dy1 = 0.5 + rand(0, 50) / 50
    local dx2 = 0.5 + rand(0, 50) / 50
    local dy2 = 0.5 + rand(0, 50) / 50
    local hx1 = {}
    local hx2 = {}
    local hy1 = {}
    local hy2 = {}
    local numlines = 0
    local index = 0
    local maxindex = 1000

    -- create the exit message
    local oldfont = ov(demofont)
    local text
    if g.os() == "Mac" then
        text = "Click or hit the return key to return to the main menu."
    else
        text = "Click or hit the enter key to return to the main menu."
    end
    ov(op.white)
    local w,h = maketext(text)
    ov(op.black)
    maketext(text, "shadow")

    -- loop until key pressed or mouse clicked
    while not return_to_main_menu do
        local event = g.getevent()
        if event:find("^oclick") or event == "key enter none" or event == "key return none" then
            -- return to main menu
            return_to_main_menu = true
        end

        -- fill the background with blue
        local oldblend = ov("blend 0")
        ov(op.blue)
        ov("fill")

        -- time drawing 1000 lines
        local t1 = g.millisecs()

        -- update the line endpoints
        x1 = x1 + dx1
        y1 = y1 + dy1
        x2 = x2 + dx2
        y2 = y2 + dy2
        if x1 < 0 or x1 > maxx then
            dx1 = -dx1
            x1 = x1 + dx1
        end
        if y1 < 0 or y1 > maxy then
            dy1 = -dy1
            y1 = y1 + dy1
        end
        if x2 < 0 or x2 > maxx then
            dx2 = -dx2
            x2 = x2 + dx2
        end
        if y2 < 0 or y2 > maxy then
            dy2 = -dy2
            y2 = y2 + dy2
        end

        -- store the new line position in the history buffer
        if numlines < maxindex then
            numlines = numlines + 1
        end
        index = index + 1
        if index > maxindex then
            index = 0
        end
        hx1[index] = x1
        hy1[index] = y1
        hx2[index] = x2
        hy2[index] = y2
        local temp = index - numlines + 1
        if temp < 0 then
           temp = temp + numlines
        end

        -- draw 1000 lines
        for i = 1, numlines do
            local c = floor((i / numlines) * 255)
            ov("rgba "..c.." "..c.." 255 255")
            ov("line "..floor(hx1[temp]).." "..floor(hy1[temp]).." "..floor(hx2[temp]).." "..floor(hy2[temp]))
            temp = temp + 1
            if temp > maxindex then
                temp = 0
            end
        end

        -- display elapsed time
        g.show("Time to draw 1000 lines "..ms(g.millisecs() - t1))

        -- draw the exit message
        ov("blend 1")
        pastetext(10 + 2, ht - h - 10 + 2, op.identity, "shadow")
        pastetext(10, ht - h - 10)
        ov("update")
        
        ov("blend "..oldblend)
    end
    ov("font "..oldfont)

    imhereallweek()
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
    ov("fill")

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
                transbg = 1 - transbg
            end
        end
    end

    -- set text alignment
    local oldalign = ov("textoption align "..align)

    -- set the text foreground color
    ov("rgba 255 255 255 255")

    -- set the text background color
    local oldbackground
    local transmsg
    if transbg == 1 then
        oldbackground = ov("textoption background 0 0 0 0")
        transmsg = "transparent background"
    else
        oldbackground = ov("textoption background 0 0 128 255")
        transmsg = "opaque background"
    end

    local t1 = g.millisecs()

    ov("blend "..transbg)

    -- create the text clip
    maketext(textstr)

    -- paste the clip onto the overlay
    local t2 = g.millisecs()
    t1 = t2 - t1
    pastetext(0, 0)

    -- output timing and drawing options
    g.show("Time to test multiline text: maketext "..ms(t1)..
           "  pastetext "..ms(g.millisecs() - t2)..
           "  align "..string.format("%-6s", align)..
           "  "..transmsg)

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

    local oldfont, oldblend, w, h, descent, nextx
    
    oldblend = ov("blend 0")
    ov(op.white) -- white background
    ov("fill")
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
    w, h, descent = maketext("Big")
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
    w, h, descent = maketext("mono12")
    pastetext(nextx, 80 - h + descent)
    
    ov("font 10 mono-bold")
    w = maketext("mono-bold")
    pastetext(300, 90)
    
    ov("font 10 mono-italic")
    maketext("mono-italic")
    pastetext(300, 105)
    
    ov("font 10 roman")
    maketext("roman")
    pastetext(300, 130)
    
    ov("font 10 roman-bold")
    w = maketext("roman-bold")
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
    w, h, descent = maketext("BLUE")
    pastetext(nextx, 200 - h + descent)

    ov(op.yellow)
    w, h = maketext("Yellow on black [] gjpqy")
    ov(op.black)
    ov("fill 300 210 "..w.." "..h)
    pastetext(300, 210)

    ov("blend 0")
    ov(op.yellow)       ov("fill 0   250 100 100")
    ov(op.cyan)         ov("fill 100 250 100 100")
    ov(op.magenta)      ov("fill 200 250 100 100")
    ov("rgba 0 0 0 0")  ov("fill 300 250 100 100")
    ov("blend 1")
    
    ov(op.black)
    maketext("The quick brown fox jumps over 123 dogs.")
    pastetext(10, 270)

    ov(op.white)
    maketext("SPOOKY")
    pastetext(310, 270)

    oldfont = ov("font "..rand(10,150).." default-bold")
    ov("rgba 255 0 0 40")   -- translucent red text
    w, h, descent = maketext("Golly")
    pastetext(10, 10)
    
    -- draw box around text
    ov("line 10 10 "..(w-1+10).." 10")
    ov("line 10 10 10 "..(h-1+10))
    ov("line "..(w-1+10).." 10 "..(w-1+10).." "..(h-1+10))
    ov("line 10 "..(h-1+10).." "..(w-1+10).." "..(h-1+10))
    -- show baseline
    ov("line 10 "..(h-1+10-descent).." "..(w-1+10).." "..(h-1+10-descent))
    
    -- draw minimal bounding rect over text
    local xoff, yoff, minwd, minht = op.minbox(textclip, w, h)
    ov("rgba 0 0 255 20")
    ov("fill "..(xoff+10).." "..(yoff+10).." "..minwd.." "..minht)
    
    -- restore blend state and font
    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov(op.black)

    g.show("Time to test text: "..ms(g.millisecs()-t1))
    
    if repeat_test(" with a different sized \"Golly\"", true) then goto restart end

    imhereallweek()
end

--------------------------------------------------------------------------------

local function test_fill()
    ::restart::

    ov(op.white)
    ov("fill")
    
    toggle = 1 - toggle
    if toggle > 0 then
        ov("blend 1") -- turn on alpha blending
    end
    
    local maxx = wd-1
    local maxy = ht-1
    local t1 = g.millisecs()
    for i = 1, 1000 do
        ov("rgba "..rand(0,255).." "..rand(0,255).." "..rand(0,255).." "..rand(0,255))
        ov("fill "..rand(0,maxx).." "..rand(0,maxy).." "..rand(100).." "..rand(100))
    end
    g.show("Time to fill one thousand rectangles: "..ms(g.millisecs()-t1).."  blend "..toggle)
    ov("rgba 0 0 0 0")
    ov("fill 10 10 100 100") -- does nothing when alpha blending is on

    if toggle > 0 then
        ov("blend 0") -- turn off alpha blending
    end
    
    if repeat_test(" with a different blend setting") then goto restart end

    imhereallweek()
end

--------------------------------------------------------------------------------

local function test_blending()
    ::restart::

    ov(op.white)
    ov("fill")
    
    toggle = 1 - toggle
    if toggle > 0 then
        ov("blend 1")           -- turn on alpha blending
    end
    
    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    ov(op.black)
    maketext("Alpha blending is turned on or off using the blend command.")
    pastetext(10, 10)
    maketext("blend "..oldblend)
    pastetext(40, 50)
    ov("blend "..oldblend)
    ov("font "..oldfont)
    
    ov("rgba 0 255 0 128")      -- 50% translucent green
    ov("fill 40 70 100 100")
    
    ov("rgba 255 0 0 128")      -- 50% translucent red
    ov("fill 80 110 100 100")
    
    ov("rgba 0 0 255 128")      -- 50% translucent blue
    ov("fill 120 150 100 100")
    
    if toggle > 0 then
        ov("blend 0")           -- turn off alpha blending
    end
    
    if repeat_test(" with a different blend setting", true) then goto restart end

    imhereallweek()
end

--------------------------------------------------------------------------------

local function test_mouse()
    ::restart::

    ov(op.black)
    ov("fill")
    ov(op.white)
    
    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    local w, h
    if g.os() == "Mac" then
        maketext("Click and drag to draw.\n"..
                 "Option-click to flood.\n"..
                 "Control-click or right-click to change the color.")
        w, h = maketext("Hit the space bar to restart this test.\n"..
                        "Hit the return key to return to the main menu.", "botlines")
    else
        maketext("Click and drag to draw.\n"..
                 "Alt-click to flood. "..
                 "Control-click or right-click to change the color.")
        w, h = maketext("Hit the space bar to restart this test.\n"..
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
        if event == "key enter none" or event == "key return none" then
            imhereallweek()
            return_to_main_menu = true
            return
        end
        if event:find("^oclick") then
            local _, x, y, button, mods = split(event)
            if mods == "alt" then
                ov("flood "..x.." "..y)
            else
                if mods == "ctrl" or button == "right" then
                    ov("rgba "..rand(0,255).." "..rand(0,255).." "..rand(0,255).." 255")
                end
                ov("set "..x.." "..y)
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
                ov("line "..prevx.." "..prevy.." "..x.." "..y)
                prevx = x
                prevy = y
                g.update()
            end
        else
            g.show("mouse is outside overlay")
        end
    end
    
    -- above loop checks for enter/return/space key
    -- if repeat_test() then goto restart end
end

--------------------------------------------------------------------------------

local function create_menu_buttons()
    local longest = "Text and Transforms"
    blend_button = op.button(       longest, test_blending)
    animation_button = op.button(   longest, test_animation)
    copy_button = op.button(        longest, test_copy_paste)
    cursor_button = op.button(      longest, test_cursors)
    line_button = op.button(        longest, test_lines)
    set_button = op.button(         longest, test_set)
    fill_button = op.button(        longest, test_fill)
    load_button = op.button(        longest, test_load)
    mouse_button = op.button(       longest, test_mouse)
    multiline_button = op.button(   longest, test_multiline_text)
    pos_button = op.button(         longest, test_positions)
    replace_button = op.button(     longest, test_replace)
    save_button = op.button(        longest, test_save)
    text_button = op.button(        longest, test_text)

    -- change labels without changing button widths
    blend_button.setlabel(      "Alpha Blending", false)
    animation_button.setlabel(  "Animation", false)
    copy_button.setlabel(       "Copy and Paste", false)
    cursor_button.setlabel(     "Cursors", false)
    line_button.setlabel(       "Drawing Lines", false)
    set_button.setlabel(        "Drawing Pixels", false)
    fill_button.setlabel(       "Filling Rectangles", false)
    load_button.setlabel(       "Loading Images", false)
    mouse_button.setlabel(      "Mouse Tracking", false)
    multiline_button.setlabel(  "Multi-line Text", false)
    pos_button.setlabel(        "Overlay Positions", false)
    replace_button.setlabel(    "Replacing Pixels", false)
    save_button.setlabel(       "Saving the Overlay", false)
    text_button.setlabel(       "Text and Transforms", false)
end

--------------------------------------------------------------------------------

local clip = false

local function main_menu()
    local numbutts = 14
    local buttwd = blend_button.wd
    local buttht = blend_button.ht
    local buttgap = 10
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
    ov("fill")
    ov(op.white)
    ov("fill 2 2 -4 -4")
    
    local x = hgap
    local y = hgap
    
    blend_button.show(x, y)         y = y + buttgap + buttht
    animation_button.show(x, y)     y = y + buttgap + buttht
    copy_button.show(x, y)          y = y + buttgap + buttht
    cursor_button.show(x, y)        y = y + buttgap + buttht
    line_button.show(x, y)          y = y + buttgap + buttht
    set_button.show(x, y)           y = y + buttgap + buttht
    fill_button.show(x, y)          y = y + buttgap + buttht
    load_button.show(x, y)          y = y + buttgap + buttht
    mouse_button.show(x, y)         y = y + buttgap + buttht
    multiline_button.show(x, y)     y = y + buttgap + buttht
    pos_button.show(x, y)           y = y + buttgap + buttht
    replace_button.show(x, y)       y = y + buttgap + buttht
    save_button.show(x, y)          y = y + buttgap + buttht
    text_button.show(x, y)
    
    local oldblend = ov("blend 1")
    
    x = hgap + buttwd + hgap
    y = int((ht - textht) / 2)
    pastetext(x, y, op.identity, "bigtext")
    x = x + int((w1 - w2) / 2)
    y = y + h1 + textgap
    pastetext(x, y, op.identity, "smalltext")
    
    ov("blend "..oldblend)
    ov("font "..oldfont)
    
    if not clip then
        ov("copy 0 0 "..wd.." "..ht.." menu")
        clip = true
    end

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
        end
    end
end

--------------------------------------------------------------------------------

local function main()
    create_overlay()
    create_menu_buttons()
    while true do
        main_menu()
    end
end

--------------------------------------------------------------------------------

local oldoverlay = g.setoption("showoverlay", 1)
local oldbuttons = g.setoption("showbuttons", 0) -- disable translucent buttons

local status, err = pcall(main)
if err then g.continue(err) end
-- the following code is always executed

ov("delete")
g.setoption("showoverlay", oldoverlay)
g.setoption("showbuttons", oldbuttons)
