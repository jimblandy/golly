--[[
This script displays animated credits for Golly
Author: Chris Rowett (crowett@gmail.com).
--]]

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local op = require "oplus"
local maketext = op.maketext
local pastetext = op.pastetext

local ov = g.overlay
local ovt = g.ovtable

math.randomseed(os.time())  -- init seed for math.random

-- minor optimizations
local rand = math.random
local floor = math.floor
local sin = math.sin

local wd, ht  = g.getview(g.getlayer())  -- viewport width and height

local sound_enabled = false  -- flag if sound available

local extra_layer = false    -- whether the extra layer was created

local minzoom = 1/16
local maxzoom = 32

--------------------------------------------------------------------------------

local function lineartoreal(zoom)
    return minzoom * math.pow(maxzoom / minzoom, zoom)
end

--------------------------------------------------------------------------------

local function bezierx(t, x0, x1, x2, x3)
    local cX = 3 * (x1 - x0)
    local bX = 3 * (x2 - x1) - cX
    local aX = x3 - x0 - cX - bX

    -- compute x position
    local x = (aX * math.pow(t, 3)) + (bX * math.pow(t, 2)) + (cX * t) + x0
    return x
end

--------------------------------------------------------------------------------

local function create_anim_bg(clipname)
    local level

    -- create a clip the size of the overlay and make it the render target
    ov("create "..wd.." "..ht.." "..clipname)
    ov("target "..clipname)

    -- create the graduated background
    for y = 0, ht / 2 do
        level = 32 + floor(128 * (y * 2 / ht))
        ovt{"rgba", 0, 0, level, 255}
        ovt{"line", 0, y, wd, y}
        ovt{"line", 0 , (ht - y), wd, (ht - y)}
    end

    -- make the overlay the render target
    ov("target")
end

--------------------------------------------------------------------------------

local function animate_credits()
    -- create the overlay
    ov("create "..wd.." "..ht)

    -- check if sound is available
    if (ov("sound") == "2") then sound_enabled = true end

    -- create a new layer
    g.addlayer()
    extra_layer = true
    g.setalgo("QuickLife")

    -- add the pattern
    g.open("../../Patterns/Life/Guns/golly-ticker.rle")
    g.setname("Credits")
    g.run(1024)

    -- create the cellview
    ov("cellview 1640 -138 1024 1024")
    ov("celloption grid 0")
    -- theme with opaque alive cells and transparent dead and unoccupied cells and border
    ov("theme 192 192 192 192 192 192 0 0 0 0 0 0 0 0 0 255 0 0 0")
    ov("create "..wd.." "..ht.." pattern")
    local zoomdelta = 0.0002
    local camminzoom = 5/9
    local cammaxzoom = 8/9
    local camzoom = cammaxzoom
    local camhold = 1000
    local camcount = camhold
    local smoothzoom

    -- update the pattern every n frames
    local patternupdateframe = 4
    local gliderframe = patternupdateframe

    -- save settings
    local oldblend = ov("blend 0")
    local oldalign = ov("textoption align left")

    -- create text clips
    local exitclip = "exit"
    local oldfont = ov("font 12 roman")
    local exitw = maketext("Click or press any key to close.", exitclip, op.white, 2, 2)
    local gollyopaqueclip = "clip1"
    local gollytranslucentclip = "clip2"
    ov("font 200 mono")
    local bannertext = "Golly"
    ovt{"rgba", 255, 192, 32, 255}
    local w, h = maketext(bannertext, gollyopaqueclip)
    ovt{"rgba", 255, 192, 32, 144}
    maketext(bannertext, gollytranslucentclip)

    local creditstext = [[
© The Golly Gang:

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
for rule, rendering and overlay improvements

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

Arie Paap




Thanks to


Bill Gosper
for the idea behind the HashLife algorithm

Alan Hensel
for QuickLife ideas and non-totalistic algorithm

David Eppstein
for the B0 rule emulation idea

Adam P. Goucher and Dean Hickerson
for clever ideas to speed up Larger than Life

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
]]

    if sound_enabled then
        creditstext = creditstext.."\nNikolaus Gebhardt @ Ambiera\nfor irrKlang\n"
    end

    creditstext = creditstext..
[[




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

Kenichi Morita
]]

    -- start music if sound enabled
    if sound_enabled then
        ov("sound loop oplus/sounds/overlay-demo/animation.ogg")
        creditstext = creditstext.."\n\n\n\nMusic\n\n\n'Contentment'\nWritten and performed by Chris Rowett"
    end

    -- create credits clip
    ov("textoption align center")
    ov("font 14 roman")

    local creditsclip = "credits"
    local credwidth, credheight = maketext(creditstext, creditsclip, "rgba 128 255 255 255", 2, 2)

    -- create graduated background
    local bgclip = "bg"
    create_anim_bg(bgclip)

    -- create stars
    local starx = {}
    local stary = {}
    local stard = {}
    local numstars = 800
    for i = 1, numstars do
        starx[i] = rand(0, wd - 1)
        stary[i] = rand(0, ht - 1)
        stard[i] = floor((i - 1) / 10) / 100
    end

    local textx = wd
    local texty
    local running = true
    local credity = ht
    local creditx = floor((wd - credwidth) / 2)
    local credpos
    local firsttime = true
    local starttime, endtime

    -- main loop
    while running do
        starttime = g.millisecs()

        -- check for resize
        local newwd, newht = g.getview(g.getlayer())
        if newwd ~= wd or newht ~= ht then
            -- resize overlay
            if newwd < 1 then newwd = 1 end
            if newht < 1 then newht = 1 end

            -- scale stars
            for i = 1, numstars do
                starx[i] = floor(starx[i] * newwd / wd)
                stary[i] = floor(stary[i] * newht / ht)
            end

            -- save new size
            wd = newwd
            ht = newht

            -- resize overlay
            ov("resize "..wd.." "..ht)

            -- resize cellview pattern clip
            ov("resize "..wd.." "..ht.." pattern")

            -- recreate background
            create_anim_bg(bgclip)

            -- recenter credits text
            creditx = floor((wd - credwidth) / 2)
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

        -- draw background
        ov("blend 0")
        ovt{"paste", 0, 0, bgclip}

        -- draw stars
        local level = 50
        ovt{"rgba", level, level, level, 255}
        local lastd = stard[1]
        local coords = { "set" }
        local ci = 2

        for i = 1, numstars do
            if (stard[i] ~= lastd) then
                if ci > 2 then
                    ovt(coords)
                    ci = 2
                    coords = { "set" }
                end
                ovt{"rgba", level, level, level, 255}
                level = level + 2
                lastd = stard[i]
            end
            starx[i] = starx[i] + lastd
            if starx[i] > wd then
                starx[i] = 0
                stary[i] = rand(0, ht - 1)
            end
            coords[ci] = starx[i]
            coords[ci + 1] = stary[i]
            ci = ci + 2
        end
        if ci > 2 then
            ovt(coords)
        end

        -- update pattern every few frames
        if gliderframe == patternupdateframe then
            gliderframe = 1
            g.run(1)
            ov("updatecells")
        end
        gliderframe = gliderframe + 1

        -- update camera zoom
        if camcount < camhold then
            camcount = camcount + 1
        else
            if zoomdelta < 0 then
                if camzoom > camminzoom then
                    camzoom = camzoom + zoomdelta
                    if camzoom < camminzoom then
                        camzoom = camminzoom
                    end
                else
                    camcount = 1
                    zoomdelta = -zoomdelta
                end
            else
                if camzoom < cammaxzoom then
                    camzoom = camzoom + zoomdelta
                    if camzoom > cammaxzoom then
                        camzoom = cammaxzoom
                    end
                else
                    camcount = 1
                    zoomdelta = -zoomdelta
                end
            end
        end
        smoothzoom = bezierx((camzoom - camminzoom) / (cammaxzoom - camminzoom), 0, 0, 1, 1) * (cammaxzoom - camminzoom) + camminzoom

        -- update cell view
        ov("target pattern")
        ov("camera zoom "..lineartoreal(smoothzoom))
        ov("drawcells")

        -- paste the pattern onto the background
        ov("target")
        ov("blend 1")
        ovt{"paste", 0, 0, "pattern"}

        -- draw bouncing scrolling text
        ov("blend 1")
        texty = floor(((ht - h) / 2 - (100 * sin(textx / 100))))
        pastetext(textx, texty, op.identity, gollyopaqueclip)

        texty = floor(((ht - h) / 2 + (100 * sin(textx / 100))))
        pastetext(textx, texty, op.identity, gollytranslucentclip)

        -- draw credits
        credpos = floor(credity)
        pastetext(creditx, credpos, op.identity, creditsclip)
        credity = credity - .5
        if credity < -credheight then
            credity = ht
        end

        -- draw exit message
        pastetext(floor((wd - exitw) / 2), 20, op.identity, exitclip)

        -- update display
        ov("update")
        if firsttime then
            firsttime = false
            g.update()
        end

        -- move text
        textx = textx - 2
        if textx <= -w then
            textx = wd
        end

        -- wait until at least 15ms have elapsed
        endtime = g.millisecs()
        while (endtime - starttime < 15) do
            endtime = g.millisecs()
        end
    end

    -- free clips
    ov("delete pattern")
    ov("delete "..exitclip)
    ov("delete "..gollytranslucentclip)
    ov("delete "..gollyopaqueclip)
    ov("delete "..creditsclip)
    ov("delete "..bgclip)

    -- restore settings
    ov("textoption align "..oldalign)
    ov("font "..oldfont)
    ov("blend "..oldblend)

    -- stop music
    if sound_enabled then
        ov("sound stop")
    end

    -- delete the layer
    g.dellayer()
    extra_layer = false
end

--------------------------------------------------------------------------------

local function main()
    -- draw animated credits
    animate_credits()
end

--------------------------------------------------------------------------------

local oldoverlay = g.setoption("showoverlay", 1)
local oldbuttons = g.setoption("showbuttons", 0) -- disable translucent buttons
local oldscroll = g.setoption("showscrollbars", 0)
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
g.setoption("showscrollbars", oldscroll)
g.setoption("tilelayers", oldtile)
g.setoption("stacklayers", oldstack)
if extra_layer then g.dellayer() end
