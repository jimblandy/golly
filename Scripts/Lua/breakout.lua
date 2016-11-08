-- Breakout for Golly
-- Author: Chris Rowett (rowett@yahoo.com), November 2016

local g = golly()
-- require "gplus.strict"
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

-- overlay width and height
local wd, ht
local minwd = 512
local minht = 512

-- high score is saved in this file
local scorefile = g.getdir("data").."overlay-bricks.ini"

--------------------------------------------------------------------------------

local function readscore()
    local score = 0
    local f = io.open(scorefile, "r")
    if f then
        score = tonumber(f:read("*l")) or 0
    end
    return score
end

--------------------------------------------------------------------------------

local function writescore(score)
    local f = io.open(scorefile, "w")
    if f then
        f:write(tostring(score).."\n")
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
        textx = (wd - w) / 2
    elseif align == alignright then
    	textx = wd - w
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

local function breakout()
    -- set font
    local oldfont   = ov("font 16 mono")
    local oldbg     = ov("textoption background 0 0 0 0")
    local oldcursor = ov("cursor hidden")
    local oldblend  = ov("blend 0")

    -- messages
    local gameoverstr = "Game Over"
    local newballstr  = "Click or Enter to launch ball"
    local controlstr  = "Mouse or Arrow keys to move bat"
    local pausestr    = "Paused: Click or Enter to continue"
    local restartstr  = "Click or Enter to start again"
    local quitstr     = "Right Click or Esc to quit"
    local continuestr = "Click or Enter for next level"
    local newhighstr  = "New High Score!"

    -- create the background
    createbackground()

    -- play games until finished
    local balls      = 3
    local score      = 0
    local level      = 1
    local again      = true
    local hiscore    = readscore()
    local numrows    = 6
    local numcols    = 20
    local maxoffsety = 20
    local newhigh

    while again == true do
        -- initialize the bricks
        local rows       = {}
        local brickwd    = floor(wd / numcols)
        local brickht    = floor(ht / 40)
        local offsety    = level + 1
        local bricksleft = numrows * numcols
        local brickx, bricky
        if offsety > maxoffsety then
            offsety = maxoffsety
        end

        for y = 1, numrows do
            local bricks = {}
            for x = 1, numcols do
                bricks[x] = true
            end
            rows[y] = bricks
        end
        local brickcols = {
            [1] = "rgba 255 0 0 255",
            [2] = "rgba 255 255 0 255",
            [3] = "rgba 255 0 255 255",
            [4] = "rgba 0 255 0 255",
            [5] = "rgba 0 255 255 255",
            [6] = "rgba 0 0 255 255",
        }

        -- intiialize the bat
        local batwd   = floor(wd / 10)
        local batx    = floor((wd - batwd) / 2)
        local batht   = brickht
        local baty    = ht - batht * 4
        local bathits = 0
        local maxhits = 8
        local lastx   = -1

        -- initialize the ball
        local ballsize  = wd / 80
        local ballx     = (wd - ballsize) / 2
        local bally     = baty - ballsize
        local balldx    = 1
        local balldy    = -1
        local maxspeed  = 3
        local speedinc  = 0.05
        local speeddef  = 1 + (level - 1) * speedinc
        if speeddef > maxspeed then
            speeddef = maxspeed
        end
        local ballspeed = speeddef
        local speeddiv  = 3

        -- shadow
        local shadowx = floor(-wd / 100)
        local shadowy = floor(ht / 100)
        local shadowcol = "rgba 0 0 0 128"

        -- whether alive
        local newball = true
        local pause   = false

        -- whether new high score
        newhigh = false

        -- main loop
        while balls > 0 and bricksleft > 0 do
            -- time frame
            local frametime = g.millisecs()

            -- check if size of layer has changed
            local newwd, newht = g.getview(g.getlayer())
            if (newwd ~= wd or new ~= viewht) and (newwd >= minwd and newht >= minht) then
                -- reposition the bat and ball
                batx = batx * (newwd / wd)
                ballx = ballx * (newwd / wd)
                bally = bally * (newht / ht)

                -- resize overlay
                wd = newwd
                ht = newht
                ov("resize "..wd.." "..ht)

                -- scale bat, ball and bricks
                brickwd  = floor(wd / numcols)
                brickht  = floor(ht / 40)
                batwd    = floor(wd / 10)
                batht    = brickht
                batx     = batx * (newwd / wd)
                baty     = ht - batht * 4
                ballsize = wd / 80

                -- recreate background
                ov("freeclip bg")
                createbackground()

                -- pause
                pause = true
            end

            -- check for mouse click or key press
            local event = g.getevent()
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
            end

            -- draw the background
            ov("blend 0")
            ov("paste 0 0 bg")

            -- draw the bricks
            local xoff = shadowx
            local yoff = shadowy
            ov("blend 1")
            ov(shadowcol)
            for pass = 1, 2 do
                for y = 1, numrows do
                    bricks = rows[y]
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

            -- draw the ball
            ov(shadowcol)
            ov("blend 1")
            ov("fill "..(floor(ballx - ballsize / 2) + shadowx).." "..(floor(bally - ballsize / 2) + shadowy).." "..floor(ballsize).." "..floor(ballsize))
            ov("blend 0")
            ov("rgba 255 255 255 255")
            ov("fill "..floor(ballx - ballsize / 2).." "..floor(bally - ballsize / 2).." "..floor(ballsize).." "..floor(ballsize))

            -- draw the bat
            ov(shadowcol)
            ov("blend 1")
            ov("fill "..(floor(batx) + shadowx).." "..(floor(baty) + shadowy).." "..batwd.." "..batht)
            ov("blend 0")
            ov("rgba 192 192 192 255")
            ov("fill "..floor(batx).." "..floor(baty).." "..batwd.." "..batht)

            -- if new ball then set ball to sit on bat
            if newball then
                ballx = batx + batwd / 2
                bally = baty - ballsize
            end

            -- check if paused
            if pause then
                ov("font 16 mono")
                shadowtext(0, ht / 2, pausestr, aligncenter)
            else
                -- check for new ball
                if newball then
                    ov("font 16 mono")
                    shadowtext(0, ht / 2 + 70, controlstr, aligncenter)
                    shadowtext(0, ht / 2 + 30, newballstr, aligncenter)
                    ov("font 24 mono")
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
                    shadowtext(0, ht / 2 - 20, remstr, aligncenter, remcol)
                    shadowtext(0, ht / 2 - 70, "Level "..level, aligncenter)
                else
                    -- update ball position
                    ballx = ballx + (balldx * ballspeed * ballsize) / speeddiv
                    bally = bally + (balldy * ballspeed * ballsize) / speeddiv

                    -- check for ball hitting boundary
                    if ballx < ballsize / 2 or ballx >= wd - ballsize / 2 then
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
                        balldx    = 1
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
                    elseif batx > wd - batwd then
                        batx = wd - batwd
                    end
                end
            else
                -- pause if mouse goes outside overlay
                pause = true
            end

            -- draw the score and lives
            ov("blend 1")
            ov("font 16 mono")
            shadowtext(5, 5, "Score "..score)
            shadowtext(-5, 5, "Balls "..balls, alignright)
            local highcol = op.white
            if newhigh == true then
                highcol = op.green
            end
            shadowtext(0, 5, "High Score "..hiscore, aligncenter, highcol)

            -- update the display
            ov("update")

            -- pause until frame time reached
            while g.millisecs() - frametime < 16 do
            end
        end

        -- save high score
        if newhigh == true then
            writescore(hiscore)
        end

        -- check why game finished
        ov("font 48 mono")
        if balls == 0 then
            -- game over
            shadowtext(0, ht / 2 - 40, gameoverstr, aligncenter, op.red)
            ov("font 16 mono")
            shadowtext(0, ht / 2 + 40, restartstr, aligncenter)
            if newhigh then
                shadowtext(0, ht / 2 - 70, newhighstr, aligncenter, op.green)
            end

            -- reset
            score = 0
            balls = 3
            level = 1
        else
            -- level complete
            ov("font 48 mono")
            shadowtext(0, ht / 2 - 40, "Level "..level.." complete!", aligncenter)
            ov("font 16 mono")
            shadowtext(0, ht / 2 + 40, continuestr, aligncenter)
            level = level + 1
        end
        shadowtext(0, ht / 2 + 70, quitstr, aligncenter)
        ov("update")

        -- wait until mouse button clicked or enter pressed
        local finished = false
        while not finished do
            local event = g.getevent()
            if event:find("^oclick") then
                local _, x, y, button, mods = split(event)
                -- quit if right button pressed
                if button == "right" then
                    again = false
                end
                finished = true
            elseif event == "key enter none" or event == "key return none" then
                finished = true
            end
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
    -- display status message
    g.show("Golly Breakout - press Esc to exit.")

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

    -- run breakout
    breakout()
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
