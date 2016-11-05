-- Bricks for Golly
-- Author: Chris Rowett (rowett@yahoo.com), November 2016

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local split = gp.split
local int = gp.int
local op = require "oplus"
local ov = g.overlay

-- overlay width and height
local wd, ht

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

local function bricks()
    -- set font
    local oldfont   = ov("font 16 mono")
    local oldbg     = ov("textoption background 0 0 0 0")
    local oldcursor = ov("cursor hidden")

    -- messages
    local gameoverstr = "Game Over"
    local newballstr  = "Click to launch"
    local pausestr    = "Paused: click to continue"

    -- create the background
    local oldblend = ov("blend 0")
    ov(op.black)
    ov("fill")
    local y, c
    for y = 0, ht - 1 do
        c = math.floor((y / ht) * 128)
        ov("rgba 0 "..(128 - c).." "..c.." 255")
        ov("line 0 "..y.." "..(wd - 1).." "..y)
    end

    -- save the background clip
    ov("copy 0 0 "..wd.." "..ht.." bg")

    local again = true

    -- play games until finished
    while again == true do
        -- initialize the bricks
        local rows       = {}
        local numrows    = 6
        local numcols    = 20
        local brickwd    = math.floor(wd / numcols)
        local brickht    = math.floor(ht / 40)
        local offsety    = 2
        local maxoffsety = 12
        local bricksleft = numrows * numcols
        local brickx, bricky
    
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
        local batwd   = math.floor(wd / 10)
        local batx    = math.floor((wd - batwd) / 2)
        local batht   = brickht
        local baty    = ht - batht * 4
        local bathits = 0
        local maxhits = 8 
    
        -- initialize the ball
        local ballsize  = wd / 80
        local ballx     = (wd - ballsize) / 2
        local bally     = baty - ballsize
        local balldx    = 1
        local balldy    = -1
        local speeddef  = 1
        local ballspeed = speeddef
        local speedinc  = 1.05
        local maxspeed  = 4
        local speeddiv  = 4
    
        -- shadow
        local shadowx = math.floor(-wd / 100)
        local shadowy = math.floor(ht / 100)
        local shadowcol = "rgba 0 0 0 128"
    
        -- whether alive
        local balls = 3
        local score = 0
        local newball = true
        local pause = false
    
        -- main loop
        while balls > 0 and bricksleft > 0 do
            -- time frame
            local frametime = g.millisecs()
    
            -- check for mouse click
            local event = g.getevent()
            if event:find("^oclick") then
                if newball then
                    newball = false
                    pause = false
                else
                    pause = not pause
                end
            end
           
            -- draw the background
            ov("paste 0 0 bg")
    
            -- draw the bricks
            local xoff, yoff
            for pass = 1, 2 do
                for y = 1, numrows do
                    bricks = rows[y]
                    bricky = (y + offsety) * brickht
                    if pass == 1 then
                        ov("blend 1")
                        ov(shadowcol)
                        xoff = shadowx
                        yoff = shadowy
                    else
                        ov("blend 0")
                        ov(brickcols[y])
                        xoff = 0
                        yoff = 0
                    end
                    for x = 1, numcols do
                        if bricks[x] == true then
                            brickx = (x - 1) * brickwd
                            ov("fill "..(brickx + xoff).." "..(bricky + yoff).." "..(brickwd - 1).." "..(brickht - 1))
                        end
                    end
                end
            end
    
            -- draw the ball
            ov(shadowcol)
            ov("blend 1")
            ov("fill "..(math.floor(ballx - ballsize / 2) + shadowx).." "..(math.floor(bally - ballsize / 2) + shadowy).." "..math.floor(ballsize).." "..math.floor(ballsize))
            ov("blend 0")
            ov("rgba 255 255 255 255")
            ov("fill "..math.floor(ballx - ballsize / 2).." "..math.floor(bally - ballsize / 2).." "..math.floor(ballsize).." "..math.floor(ballsize))
    
            -- draw the bat
            ov(shadowcol)
            ov("blend 1")
            ov("fill "..(batx + shadowx).." "..(baty + shadowy).." "..batwd.." "..batht)
            ov("blend 0")
            ov("rgba 192 192 192 255")
            ov("fill "..batx.." "..baty.." "..batwd.." "..batht)
    
            -- check if paused
            if pause then
                ov("font 16 mono")
                ov("blend 1")
                ov(op.black)
                local w, h = maketext(pausestr)
                pastetext(math.floor((wd - w) / 2) + 2, math.floor((ht - h) / 2) + 2)
                ov(op.white)
                maketext(pausestr)
                pastetext(math.floor((wd - w) / 2) + 2, math.floor((ht - h) / 2) + 2)
            else
                -- update ball position
                if newball then
                    ballx = batx + batwd / 2
                    bally = baty - ballsize
                    ov("font 24 mono")
                    ov("blend 1")
                    ov(op.black)
                    local w, h = maketext(newballstr)
                    pastetext(math.floor((wd - w) / 2) + 2, math.floor((ht - h) / 2) + 2 + 20)
                    ov(op.white)
                    maketext(newballstr)
                    pastetext(math.floor((wd - w) / 2), math.floor((ht - h) / 2) + 20)
                    ov(op.black)
                    local remstr
                    if balls == 1 then
                        remstr = "Last ball!"
                    else
                        remstr = balls.." balls left"
                    end
                    ov(op.black)
                    w, h = maketext(remstr)
                    pastetext(math.floor((wd - w) / 2) + 2, math.floor((ht - h) / 2) + 2 - 20)
                    ov(op.white)
                    maketext(remstr)
                    pastetext(math.floor((wd - w) / 2), math.floor((ht - h) / 2) - 20)
                else
                    ballx = ballx + (balldx * ballspeed * ballsize) / speeddiv
                    bally = bally + (balldy * ballspeed * ballsize) / speeddiv
                    -- check for ball hitting boundary
                    if ballx < 0 or ballx >= wd then
                        balldx = -balldx
                        ballx = ballx + balldx * 2
                    end
                    if bally <= 0 then
                        balldy = -balldy
                        bally = bally + balldy * 2
                    end
                    if bally >= ht then
                        -- ball lost!
                        balls = balls - 1
                        bally = baty
                        balldy = -1
                        ballspeed = speeddef
                        newball = true
                    end
                    -- check for ball hitting bat
                    if bally >= baty and bally <= baty + batht - 1 and ballx >= batx and ballx < batx + batwd then
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
                    bricky = math.floor(bally / brickht) - offsety
                    brickx = math.floor(ballx / brickwd) + 1
                    if bricky >= 1 and bricky <= numrows then
                        if rows[bricky][brickx] == true then
                            -- hit a brick!
                            rows[bricky][brickx] = false
                            score = score + 10 * (numrows - bricky + 1)
                            balldy = -balldy
                            ballspeed = ballspeed * speedinc
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
                batx = tonumber(mousex)
                if batx > wd - batwd then
                    batx = wd - batwd
                end
            end
    
            -- draw the score and lives
            ov("blend 1")
            ov("font 16 mono")
            for pass = 1, 2 do
                if pass == 1 then
                    ov(op.black)
                    xoff = 2
                    yoff = 2
                else
                    ov(op.white)
                    xoff = 0
                    yoff = 0
                end
                maketext("Score "..score)
                pastetext(5 + xoff, 5 + yoff)
                local w = maketext("Balls "..balls)
                pastetext(wd - w - 5 + xoff, 5 + yoff)
                w = maketext("Speed "..string.format("%.1f", ballspeed))
                pastetext(math.floor((wd - w) / 2) + xoff, 5 + yoff)
            end
              
            -- update the display
            ov("update")
    
            -- pause until frame time reached
            g.show("Frame time: "..ms(g.millisecs() - frametime))
            while g.millisecs() - frametime < 16 do
            end
        end

        -- game over
        ov("font 48 mono")
        ov("blend 1")
        ov(op.black)
        local w, h = maketext(gameoverstr)
        pastetext(math.floor((wd - w) / 2) + 2, math.floor((ht - h) / 2) + 2)
        ov(op.white)
        maketext(gameoverstr)
        pastetext(math.floor((wd - w) / 2), math.floor((ht - h) / 2))
        ov("update")
    
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
    wd, ht = g.getview(g.getlayer())
    ov("create "..wd.." "..ht)
    bricks()
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
