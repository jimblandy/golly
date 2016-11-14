-- Breakout for Golly
-- Author: Chris Rowett (crowett@gmail.com), November 2016
-- build 14

local g = golly()
require "gplus.strict"
local gp = require "gplus"
local split = gp.split
local int = gp.int
local op = require "oplus"
local ov = g.overlay
local floor = math.floor

-- text alignment
local alignleft   = 0
local aligncenter = 1
local alignright  = 2
local fontscale   = 1

-- overlay width and height
local wd, ht = g.getview(g.getlayer())
local minwd   = 400
local minht   = 400
local edgegap = 0

-- shadow settings
local shadowx   = floor(-wd / 100)
local shadowy   = floor(ht / 100)
local shadowcol = "rgba 0 0 0 128"

-- brick settings
local numrows    = 6
local numcols    = 20
local rows       = {}
local brickwd    = floor(wd / numcols)
local brickht    = floor(ht / 40)
local maxoffsety = 20
local offsety    = 0
local brickcols  = {
    [1] = op.red,
    [2] = op.yellow,
    [3] = op.magenta,
    [4] = op.green,
    [5] = op.cyan,
    [6] = op.blue
}
local bricksleft = numrows * numcols
local brickx
local bricky

-- bat settings
local batx
local baty
local batwd
local batht

-- ball settings
local ballsize = wd / 80
local ballx    = 0
local bally    = 0

-- game settings
local level      = 1
local newball    = true
local pause      = false
local fullscreen = 0
local hiscore    = 0
local score      = 0
local balls      = 3
local newhigh    = false
local offoverlay = false
local finished   = false
local again      = true

-- settings are saved in this file
local settingsfile = g.getdir("data").."overlay-bricks.ini"

-- messages
local gameoverstr = "Game Over"
local newballstr  = "Click or Enter to launch ball"
local controlstr  = "Mouse or Arrow keys to move bat"
local pausestr    = "Paused: Click or Enter to continue"
local fullstr     = "F11 to toggle full screen mode"
local restartstr  = "Click or Enter to start again"
local quitstr     = "Right Click or Esc to quit"
local continuestr = "Click or Enter for next level"
local newhighstr  = "New High Score!"

--------------------------------------------------------------------------------

local function setfullscreen()
    g.setoption("fullscreen", fullscreen)
    if fullscreen == 0 then
        ov("cursor arrow")
    else
        ov("cursor hidden")
    end
end

--------------------------------------------------------------------------------

local function readsettings()
    local score = 0
    local fullscreen = 0
    local f = io.open(settingsfile, "r")
    if f then
        score = tonumber(f:read("*l")) or 0
        fullscreen = tonumber(f:read("*l")) or 0
    end
    return score, fullscreen
end

--------------------------------------------------------------------------------

local function writesettings(score, fullscreen)
    local f = io.open(settingsfile, "w")
    if f then
        f:write(tostring(score).."\n")
        f:write(tostring(fullscreen).."\n")
        f:close()
    end
end

--------------------------------------------------------------------------------

local function ms(t)
    return string.format("%.2fms", t)
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

local function shadowtext(x, y, text, align, color)
    align = align or alignleft
    color = color or op.white
    ov("blend 1")
    ov(op.black)
    local w, h = maketext(text)
    local textx = 0
    if align == aligncenter then
        textx = (wd - w - edgegap) / 2
    elseif align == alignright then
        textx = wd - w - edgegap
    end
    pastetext(floor(textx + x + 2), floor(y + 2))
    ov(color)
    maketext(text)
    pastetext(floor(textx + x), floor(y))
end

--------------------------------------------------------------------------------

local function createbackground()
    ov("blend 0")
    ov(op.black)
    ov("fill")
    local y, c
    for y = 0, ht - 1 do
        c = floor((y / ht) * 128)
        ov("rgba 0 "..(128 - c).." "..c.." 255")
        ov("line 0 "..y.." "..(wd - 1).." "..y)
    end

    -- save the background clip
    ov("copy 0 0 "..wd.." "..ht.." bg")
end

--------------------------------------------------------------------------------

local function drawbackground()
    ov("blend 0")
    ov("paste 0 0 bg")
    if edgegap > 0 then
        ov(op.black)
        ov("fill "..(wd - edgegap).." 0 "..edgegap.." "..(ht - 1))
    end
end

--------------------------------------------------------------------------------

local function drawbricks()
    local xoff = shadowx
    local yoff = shadowy
    ov("blend 1")
    ov(shadowcol)
    for pass = 1, 2 do
        for y = 1, numrows do
            local bricks = rows[y]
            bricky = (y + offsety) * brickht
            if pass == 2 then
                ov(brickcols[y])
            end
            for x = 1, numcols do
                if bricks[x] == true then
                    brickx = (x - 1) * brickwd
                    ov("fill "..(brickx + xoff).." "..(bricky + yoff).." "..(brickwd - 1).." "..(brickht - 1))
                end
            end
        end
        ov("blend 0")
        xoff = 0
        yoff = 0
    end
end

--------------------------------------------------------------------------------

local function drawball()
    ov(shadowcol)
    ov("blend 1")
    ov("fill "..(floor(ballx - ballsize / 2) + shadowx).." "..(floor(bally - ballsize / 2) + shadowy).." "..floor(ballsize).." "..floor(ballsize))
    ov("blend 0")
    ov(op.white)
    ov("fill "..floor(ballx - ballsize / 2).." "..floor(bally - ballsize / 2).." "..floor(ballsize).." "..floor(ballsize))
end

--------------------------------------------------------------------------------

local function drawbat()
    ov(shadowcol)
    ov("blend 1")
    ov("fill "..(floor(batx) + shadowx).." "..(floor(baty) + shadowy).." "..batwd.." "..batht)
    ov("blend 0")
    -- draw the bat in red if mouse is off the overlay
    if offoverlay then
        ov(op.red)
    else
        ov("rgba 192 192 192 255")
    end
    ov("fill "..floor(batx).." "..floor(baty).." "..batwd.." "..batht)
end

--------------------------------------------------------------------------------

local function initbricks()
    rows       = {}
    brickwd    = floor(wd / numcols)
    brickht    = floor(ht / 40)
    bricksleft = numrows * numcols
    offsety    = level + 1
    if offsety > maxoffsety then
        offsety = maxoffsety
    end
    edgegap = wd - brickwd * numcols

    for y = 1, numrows do
        local bricks = {}
        for x = 1, numcols do
            bricks[x] = true
        end
        rows[y] = bricks
    end
end

--------------------------------------------------------------------------------

local function initbat()
    batwd = floor(wd / 10)
    batx  = floor((wd - batwd) / 2)
    batht = brickht
    baty  = ht - batht * 4
end

--------------------------------------------------------------------------------

local function initball()
    ballsize = wd / 80
    ballx    = (wd - ballsize) / 2
    bally    = baty - ballsize
end

--------------------------------------------------------------------------------

local function initshadow()
    shadowx   = floor(-wd / 100)
    shadowy   = floor(ht / 100)
    shadowcol = "rgba 0 0 0 128"
end

--------------------------------------------------------------------------------

local function processinput()
    -- check for click, enter or return
    local event = g.getevent()
    if #event then
        if event:find("^oclick") or event == "key enter none" or event == "key return none" then
            -- click or enter starts or toggles pause
            if newball then
                newball = false
                pause   = false
            else
                pause = not pause
            end
        elseif event == "key left none" then
            -- left arrow moves bat left
            batx = batx - (wd / 50)
            if batx < 0 then
                batx = 0
            end
        elseif event == "key right none" then
            -- right arrow moves bat right
            batx = batx + (wd / 50)
            if batx + batwd >= wd then
                batx = wd - batwd
            end
        elseif event == "key f11 none" then
            -- toggle fullscreen
            fullscreen = 1 - fullscreen
            writesettings(hiscore, fullscreen)
            setfullscreen()
        end
    end
end

--------------------------------------------------------------------------------

local function processendinput()
    local event = g.getevent()
    if #event > 0 then
        if event:find("^oclick") then
            local _, x, y, button, mods = split(event)
            -- quit if right button pressed
            if button == "right" then
                again = false
            end
            finished = true
        elseif event == "key enter none" or event == "key return none" then
            finished = true
        elseif event == "key f11 none" then
            -- toggle fullscreen
            fullscreen = 1 - fullscreen
            writesettings(hiscore, fullscreen)
            setfullscreen()
        end
    end
end

--------------------------------------------------------------------------------

local function resizegame(newwd, newht)
    -- check minimum size
    if newwd < minwd then
        newwd = minwd
    end
    if newht < minht then
        newht = minht
    end

    -- reposition the bat and ball
    batx = batx * (newwd / wd)
    ballx = ballx * (newwd / wd)
    bally = bally * (newht / ht)

    -- resize overlay
    wd = newwd
    ht = newht
    ov("resize "..wd.." "..ht)
    fontscale = wd / minwd
    if (ht /minht) < fontscale then
        fontscale = ht / minht
    end

    -- scale bat, ball and bricks
    brickwd  = floor(wd / numcols)
    brickht  = floor(ht / 40)
    batwd    = floor(wd / 10)
    batht    = brickht
    batx     = batx * (newwd / wd)
    baty     = ht - batht * 4
    ballsize = wd / 80
    edgegap  = wd - brickwd * numcols

    -- recreate background
    ov("freeclip bg")
    createbackground()
end

--------------------------------------------------------------------------------

local function drawscoreline()
    ov("blend 1")
    ov("font "..floor(10 * fontscale).." mono")
    shadowtext(5, 5, "Score "..score)
    shadowtext(-5, 5, "Balls "..balls, alignright)
    local highcol = op.white
    if newhigh == true then
        highcol = op.green
    end
    shadowtext(0, 5, "High Score "..hiscore, aligncenter, highcol)
end

--------------------------------------------------------------------------------

local function drawgameover()
    ov("font "..floor(30 * fontscale).." mono")
    shadowtext(0, ht / 2 - 30 * fontscale, gameoverstr, aligncenter, op.red)
    ov("font "..floor(10 * fontscale).." mono")
    shadowtext(0, ht / 2 + 30 * fontscale, restartstr, aligncenter)
    if newhigh then
        shadowtext(0, ht / 2 - 52 * fontscale, newhighstr, aligncenter, op.green)
    end
    shadowtext(0, ht / 2 + 52 * fontscale, quitstr, aligncenter)
end

--------------------------------------------------------------------------------

local function drawlevelcomplete()
    ov("font "..floor(20 * fontscale).." mono")
    shadowtext(0, ht / 2 - 30 * fontscale, "Level "..level.." complete!", aligncenter, op.green)
    ov("font "..floor(10 * fontscale).." mono")
    shadowtext(0, ht / 2 + 30 * fontscale, continuestr, aligncenter)
    shadowtext(0, ht / 2 + 52 * fontscale, quitstr, aligncenter)
end

--------------------------------------------------------------------------------

local function drawpause()
    ov("font "..floor(10 * fontscale).." mono")
    shadowtext(0, ht / 2, pausestr, aligncenter)
end

--------------------------------------------------------------------------------

local function breakout()
    -- set font
    local oldfont   = ov("font 16 mono")
    local oldbg     = ov("textoption background 0 0 0 0")
    local oldblend  = ov("blend 0")
    local oldcursor = ov("cursor arrow")

    -- read saved settings
    hiscore, fullscreen = readsettings()
    setfullscreen()

    -- create the background
    createbackground()

    -- play games until finished
    balls   = 3
    score   = 0
    level   = 1
    again   = true
    newhigh = false

    while again == true do
        -- initialize the bricks
        initbricks()

        -- intiialize the bat
        initbat()
        local bathits = 0
        local maxhits = 7
        local lastx   = -1

        -- initialize the ball
        initball()
        local balldx    = 0.5
        local balldy    = -1
        local maxspeed  = 2.25 + (level  - 1) * 0.25
        if maxspeed > 3 then
            maxspeed = 3
        end
        local speedinc  = 0.04
        local speeddef  = 1 + (level - 1) * speedinc * 2
        if speeddef > maxspeed then
            speeddef = maxspeed
        end
        local ballspeed = speeddef
        local speeddiv  = 3

        -- initialize shadow
        initshadow()

        -- whether alive
        newball = true
        pause   = false

        -- whether new high score
        newhigh = false

        -- whether mouse off overlay
        offoverlay = false

        -- main loop
        while balls > 0 and bricksleft > 0 do
            -- time frame
            local frametime = g.millisecs()

            -- check for mouse click or key press
            processinput()

            -- check if size of layer has changed
            local newwd, newht = g.getview(g.getlayer())
            if newwd ~= wd or newht ~= ht then
                resizegame(newwd, newht)
            end

            -- draw the background
            drawbackground()

            -- check if paused
            if pause then
                drawpause()
            else
                -- check for new ball
                if newball then
                    ov("font "..floor(10 * fontscale).." mono")
                    shadowtext(0, ht / 2 + 82 * fontscale, fullstr, aligncenter)
                    shadowtext(0, ht / 2 + 52 * fontscale, controlstr, aligncenter)
                    shadowtext(0, ht / 2 + 22 * fontscale, newballstr, aligncenter)
                    ov("font "..floor(15 * fontscale).." mono")
                    local remstr, remcol
                    if balls == 1 then
                        remstr = "Last ball!"
                        remcol = op.red
                    else
                        remstr = balls.." balls left"
                        if balls == 2 then
                            remcol = op.yellow
                        else
                            remcol = op.green
                        end
                    end
                    shadowtext(0, ht / 2 - 15 * fontscale, remstr, aligncenter, remcol)
                    shadowtext(0, ht / 2 - 52 * fontscale, "Level "..level, aligncenter)
                else
                    -- update ball position
g.show(string.format("%.2f", ((balldx * ballspeed * ballsize) / speeddiv)).." "..ballsize)
                    ballx = ballx + (balldx * ballspeed * ballsize) / speeddiv
                    bally = bally + (balldy * ballspeed * ballsize) / speeddiv

                    -- check for ball hitting boundary
                    if ballx < ballsize / 2 or ballx >= wd - edgegap - ballsize / 2 then
                        balldx = -balldx
                        ballx  = ballx + balldx * 2
                    end
                    if bally < ballsize / 2 then
                        -- ball hit top so speed up a little bit
                        balldy    = -balldy
                        bally     = bally + balldy * 2
                        ballspeed = ballspeed + speedinc / 4
                        if ballspeed > maxspeed then
                            ballspeed = maxspeed
                        end
                    elseif bally >= ht then
                        -- ball lost!
                        balls     = balls - 1
                        bally     = baty
                        balldy    = -1
                        balldx    = 0.5
                        ballspeed = speeddef
                        newball   = true
                    -- check for ball hitting bat
                    elseif bally >= baty and bally <= baty + batht - 1 and ballx >= batx and ballx < batx + batwd then
                        -- set dx from where ball hit bat
                        balldx = (3 * (ballx - batx) / batwd) - 1.5
                        if balldx >= 0 and balldx < 0.1 then
                            balldx = 0.1
                        end
                        if balldx > -0.1 and balldx <= 0 then
                            balldx = -0.1
                        end
                        balldy = -balldy
                        bally = baty
                        bathits = bathits + 1
                        -- move the bricks down after a number of bat hits
                        if bathits == maxhits then
                            bathits = 0
                            if offsety < maxoffsety then
                                offsety = offsety + 1
                            end
                       end
                    end
                    -- check for ball hitting brick
                    bricky = floor(bally / brickht) - offsety
                    brickx = floor(ballx / brickwd) + 1
                    if bricky >= 1 and bricky <= numrows then
                        if rows[bricky][brickx] == true then
                            -- hit a brick!
                            rows[bricky][brickx] = false
                            score = score + (level + 9) * (numrows - bricky + 1)
                            if score > hiscore then
                                hiscore = score
                                newhigh = true
                            end
                            -- work out which axis to invert
                            local lastbricky = floor((bally - (balldy * ballspeed * ballsize / speeddiv)) / brickht) - offsety
                            if lastbricky == bricky then
                                balldx = -balldx
                            else
                                balldy = -balldy
                            end
                            -- speed the ball up
                            ballspeed = ballspeed + speedinc
                            if ballspeed > maxspeed then
                                ballspeed = maxspeed
                            end
                            bricksleft = bricksleft - 1
                        end
                    end
                end
            end

            -- update bat position
            local mousepos = ov("xy")
            if mousepos ~= "" then
                local mousex, mousey = split(mousepos)
                if mousex ~= lastx then
                    lastx = mousex
                    batx = tonumber(mousex) - batwd / 2
                    if batx < 0 then
                        batx = 0
                    elseif batx > wd - edgegap - batwd then
                        batx = wd - edgegap - batwd
                    end
                end
                offoverlay = false
            else
                -- mouse off overlay
                offoverlay = true
            end

            -- if new ball then set ball to sit on bat
            if newball then
                ballx = batx + batwd / 2
                bally = baty - ballsize
            end

            -- draw the bricks
            drawbricks()

            -- draw the ball
            drawball()

            -- draw the bat
            drawbat()

            -- draw the score, high score and lives
            drawscoreline()

            -- update the display
            ov("update")

            -- pause until frame time reached
            while g.millisecs() - frametime < 16 do end
        end

        -- save high score
        if newhigh == true then
            writesettings(hiscore, fullscreen)
        end

        -- loop until mouse button clicked or enter pressed
        finished = false
        while not finished do
            -- time frame
            local frametime = g.millisecs()

            -- check if size of layer has changed
            local newwd, newht = g.getview(g.getlayer())
            if newwd ~= wd or newht ~= ht then
                resizegame(newwd, newht)
            end

            -- draw background
            drawbackground()

            -- check for mouse off overlay
            local mousepos = ov("xy")
            if mousepos ~= "" then
                offoverlay = false
            else
                offoverlay = true
            end

            -- draw bricks and bat but not ball
            drawbricks()
            drawbat()

            -- check why game finished
            if balls == 0 then
                -- game over
                drawgameover()
            else
                -- level complete
                drawlevelcomplete()
            end

            -- get key or mouse event
            processendinput()

            -- update the display
            ov("update")

            -- pause until frame time reached
            while g.millisecs() - frametime < 16 do end
        end

        -- check why game finished
        if balls == 0 then
            -- reset
            score = 0
            balls = 3
            level = 1
        else
            -- level complete
            level = level + 1
        end

    end

    -- free clips and restore settings
    ov("freeclip bg")
    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov("textoption background "..oldbg)
    ov("cursor "..oldcursor)
end

--------------------------------------------------------------------------------

local function main()
    -- get size of overlay
    wd, ht = g.getview(g.getlayer())
    if wd < minwd then
        wd = minwd
    end
    if ht < minht then
        ht = minht
    end

    -- create overlay
    ov("create "..wd.." "..ht)
    fontscale = wd / minwd
    if (ht /minht) < fontscale then
        fontscale = ht / minht
    end

    -- run breakout
    breakout()
end

--------------------------------------------------------------------------------

local oldoverlay = g.setoption("showoverlay", 1)
local oldbuttons = g.setoption("showbuttons", 0) -- disable translucent buttons
local oldfs      = g.getoption("fullscreen")

local status, err = pcall(main)
if err then g.continue(err) end
-- the following code is always executed

ov("delete")
g.setoption("showoverlay", oldoverlay)
g.setoption("showbuttons", oldbuttons)
g.setoption("fullscreen", oldfs)
