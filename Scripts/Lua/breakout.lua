-- Breakout for Golly
-- Author: Chris Rowett (crowett@gmail.com), November 2016

local build = 32
local g = golly()
-- require "gplus.strict"
local gp    = require "gplus"
local split = gp.split
local int   = gp.int
local op    = require "oplus"
local ov    = g.overlay
local floor = math.floor
local rand  = math.random
local abs   = math.abs

-- text alignment
local alignleft   = 0
local aligncenter = 1
local alignright  = 2
local alignraw    = 3
local fontscale   = 1

-- overlay width and height
local wd, ht   = g.getview(g.getlayer())
local minwd    = 400
local minht    = 400
local edgegapl = 0
local edgegapr = 0

-- shadow settings
local shadowx   = floor(-wd / 100)
local shadowy   = floor(ht / 100)
local shadowcol = "rgba 0 0 0 128"
local shadtxtx  = -2
local shadtxty  = 2

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
local bricksleft  = numrows * numcols
local totalbricks = numrows * numcols
local brickx
local bricky

-- bat settings
local batx
local baty
local batwd
local batht
local batkeydir = 0
local lastx

-- ball settings
local ballsize  = wd / 80
local ballx     = 0
local bally     = 0
local numsteps  = 80

-- particle settings
local particles       = {}
local brickparticles  = 192
local ballparticles   = 1
local ballpartchance  = 0.25
local wallparticles   = 20
local batparticles    = 20
local highparticles   = 4
local comboparticles  = 4
local lostparticles   = 1024

-- game settings
local level      = 1
local newball    = true
local pause      = false
local fullscreen = 0
local hiscore    = 0
local score      = 0
local combo      = 1
local combomult  = 1
local combofact  = 1.04
local gamecombo  = 1
local maxcombo   = 2
local balls      = 3
local newhigh    = false
local newcombo   = false
local offoverlay = false
local finished   = false
local again      = true

-- timing settings
local times         = {}
local timenum       = 1
local numtimes      = 10
local framemult     = 1

-- game options
local showtiming    = 0
local showparticles = 1
local autopause     = 0
local autostart     = 0
local showmouse     = 1
local showshadows   = 1
local showoptions   = false

-- settings are saved in this file
local settingsfile = g.getdir("data").."breakout.ini"

-- notifications
local notifyduration = 300
local notifytrans    = 20
local notifycurrent  = 0
local notifymessage  = ""

-- bonus level
local bonus = {
    805203,
    699732,
    830290,
    698705,
    698705,
    805158
}
local bonuslevel    = false
local bonusinterval = 3
local bonustime     = 60
local bonuscurrent  = 0
local bonusgreen    = 10
local bonusyellow   = 20

-- static messages and clip names
local optcol = "rgba 192 192 192 255"
local messages = {
    ["gameover"]   = { "Game Over", 30, op.red },
    ["newball"]    = { "Click or Enter to launch ball", 10 },
    ["control"]    = { "Mouse or Arrow keys to move bat", 10 },
    ["pause"]      = { "Paused", 15, op.yellow },
    ["resume"]     = { "Click or Enter to continue", 10 },
    ["focus"]      = { "Move mouse onto overlay to continue", 10 },
    ["quitgame"]   = { "Right Click to quit game", 10 },
    ["option"]     = { "Tab for Game Settings", 10 },
    ["restart"]    = { "Click or Enter to start again", 10 },
    ["quit"]       = { "Right Click or Esc to exit", 10 },
    ["continue"]   = { "Click or Enter for next level", 10 },
    ["newhigh"]    = { "New High Score!", 10, op.green },
    ["newcombo"]   = { "New Best Combo!", 10, op.green },
    ["close"]      = { "Click or Tab to close Game Settings", 10 },
    ["autopause"]  = { "Autopause", 10, optcol },
    ["shadows"]    = { "Shadows", 10, optcol },
    ["mouse"]      = { "Mouse Pointer", 10, optcol },
    ["particles"]  = { "Particles", 10, optcol },
    ["autostart"]  = { "Autostart", 10, optcol },
    ["timing"]     = { "Timing", 10, optcol },
    ["fullscreen"] = { "Fullscreen", 10, optcol },
    ["function"]   = { "Function", 10 },
    ["on"]         = { "On", 10, op.green },
    ["off"]        = { "Off", 10, op.red },
    ["state"]      = { "State", 10 },
    ["key"]        = { "Key", 10 },
    ["a"]          = { "A", 10, optcol },
    ["d"]          = { "D", 10, optcol },
    ["m"]          = { "M", 10, optcol },
    ["p"]          = { "P", 10, optcol },
    ["s"]          = { "S", 10, optcol },
    ["t"]          = { "T", 10, optcol },
    ["f11"]        = { "F11", 10, optcol },
    ["score"]      = { "Score ", 10 },
    ["balls"]      = { "Balls ", 10 },
    ["combo"]      = { "Combo x", 10 },
    ["combog"]     = { "Combo x", 10, op.green },
    ["high"]       = { "High Score ", 10 },
    ["highg"]      = { "High Score ", 10, op.green },
    ["level"]      = { "Level ", 15 },
    ["bonus"]      = { "Bonus Level", 15 },
    ["bcomplete"]  = { "Bonus Level Complete", 15 },
    ["remaing"]    = { "Bricks left ", 10, op.green },
    ["remainy"]    = { "Bricks left ", 10, op.yellow },
    ["remainr"]    = { "Bricks left ", 10, op.red },
    ["timeg"]      = { "Time ", 15, op.green },
    ["timey"]      = { "Time ", 15, op.yellow },
    ["timer"]      = { "Time ", 15, op.red  },
    ["3left"]      = { "3 balls left", 15, op.green },
    ["2left"]      = { "2 balls left", 15, op.yellow },
    ["1left"]      = { "Last ball! ", 15, op.red },
    ["10"]         = { "M", 10 },
    ["15"]         = { "M", 15 },
    ["30"]         = { "M", 30 }
}

--------------------------------------------------------------------------------

local function showcursor()
    if showmouse == 0 or fullscreen == 1 then
        ov("cursor hidden")
    else
        ov("cursor arrow")
    end
end

--------------------------------------------------------------------------------

local function setfullscreen()
    g.setoption("fullscreen", fullscreen)
    showcursor()
end

--------------------------------------------------------------------------------

local function readsettings()
    local f = io.open(settingsfile, "r")
    if f then
        hiscore       = tonumber(f:read("*l")) or 0
        fullscreen    = tonumber(f:read("*l")) or 0
        showtiming    = tonumber(f:read("*l")) or 0
        showparticles = tonumber(f:read("*l")) or 1
        autopause     = tonumber(f:read("*l")) or 0
        autostart     = tonumber(f:read("*l")) or 0
        showmouse     = tonumber(f:read("*l")) or 1
        showshadows   = tonumber(f:read("*l")) or 1
        maxcombo      = tonumber(f:read("*l")) or 2
        f:close()
    end
end

--------------------------------------------------------------------------------

local function writesettings()
    local f = io.open(settingsfile, "w")
    if f then
        f:write(tostring(hiscore).."\n")
        f:write(tostring(fullscreen).."\n")
        f:write(tostring(showtiming).."\n")
        f:write(tostring(showparticles).."\n")
        f:write(tostring(autopause).."\n")
        f:write(tostring(autostart).."\n")
        f:write(tostring(showmouse).."\n")
        f:write(tostring(showshadows).."\n")
        f:write(tostring(maxcombo).."\n")
        f:close()
    end
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
    -- save settings
    local oldblend = ov("blend 1")
    local oldrgba = ov(op.black)
    local w, h = maketext(text)
    local textx = edgegapl
    -- process alignment
    if align == aligncenter then
        textx = (wd - w) / 2 + edgegapl
    elseif align == alignright then
        textx = wd - w - edgegapr
    elseif align == alignraw then
        textx = 0
    end
    -- adjust position for shadow
    if shadtxtx < 0 then
        x = x - shadtxtx
    end
    if shadtxty < 0 then
        y = y - shadtxty
    end
    -- draw shadow
    pastetext(floor(textx + x + shadtxtx), floor(y + shadtxty))
    -- draw text
    ov(color)
    maketext(text)
    pastetext(floor(textx + x), floor(y))
    -- adjust width and height for shadow
    w = w + abs(shadtxtx)
    h = h + abs(shadtxty)
    -- restore settings
    ov("blend "..oldblend)
    ov("rgba "..oldrgba)
    return w, h
end

--------------------------------------------------------------------------------

local function createstatictext()
    -- clear background to transparent black
    ov("rgba 0 0 0 0")
    ov("fill")

    -- create each static text clip
    local y = 0
    local fontsize
    local lastsize = -1
    for clipname, message in pairs(messages) do
        -- get the font size for this message
        local fontsize = floor(message[2] * fontscale)
        -- if it is different than the last message then set the new font
        if fontsize ~= lastsize then
            ov("font "..fontsize.." mono")
            lastsize = fontsize
        end
        -- create the text message
        local w, h = shadowtext(0, y, message[1], alignraw, message[3])
        -- copy to the named clip
        ov("copy 0 "..y.." "..w.." "..h.." "..clipname)
        -- save the clip width and height
        message[4] = w
        message[5] = h
        -- next screen position
        y = y + h
        if y > ht * 7 / 8 then
            y = 0
            ov("fill")
        end
    end
end

--------------------------------------------------------------------------------

local function updatenotification()
    -- check if there is a message to display
    if notifymessage ~= "" then
        local y = 0
        -- check if notification finished
        if notifycurrent >= notifyduration then
            notifymessage = ""
            notifycurrent = 0
        else
            -- select font
            ov("font "..floor(8 * fontscale).." mono")
            -- check for appear phase
            if notifycurrent < notifytrans then
                y = (notifycurrent / notifytrans) * (8 * fontscale)
            -- check for disappear phase
            elseif notifycurrent > notifyduration - notifytrans then
                y = (notifyduration - notifycurrent) / notifytrans * (8 * fontscale)
            -- hold phase
            else
                y = (8 * fontscale)
            end
            -- draw notification
            shadowtext(5, floor(ht - y - 4 * fontscale), notifymessage)
            notifycurrent = notifycurrent + framemult
        end
    end
end

--------------------------------------------------------------------------------

local function notify(message, flag)
    flag = flag or -1
    notifymessage = message
    if flag == 0 then
        notifymessage = message.." off"
    elseif flag == 1 then
        notifymessage = message.." on"
    end
    if notifycurrent ~= 0 then
        notifycurrent = notifytrans
    end
end

--------------------------------------------------------------------------------

local function initparticles()
    particles = {}
end

--------------------------------------------------------------------------------

local function createparticles(x, y, areawd, areaht, howmany)
    -- find the first free slot
    local i = 1
    while i <= #particles and particles[i][1] > 0 do
        i = i + 1
    end
    for j = 1, howmany do
        local item = { 255, x - rand(floor(areawd)), y + rand(floor(areaht)), rand() - 0.5, rand() - 0.5 }
        particles[i] = item
        i = i + 1
        -- find the next free slot
        while i <= #particles and particles[i][1] > 0 do
            i = i + 1
        end
    end
end

--------------------------------------------------------------------------------

local function drawparticles()
    ov("blend 1")
    for i = 1, #particles do
        local item = particles[i]
        local alpha = item[1]
        -- check if particle is still alive
        if alpha > 0 then
            local x = item[2]
            local y = item[3]
            local dx = item[4]
            local dy = item[5]
            if showparticles ~= 0 then
                ov("rgba 255 255 255 "..floor(alpha))
                ov("set "..floor(x).." "..floor(y))
            end
            -- fade item
            alpha = alpha - 3 * framemult
            if alpha < 0 then
                alpha = 0
            end
            item[1] = alpha
            item[2] = x + dx * framemult
            item[3] = y + dy * framemult
            item[4] = dx * 0.99
            if dy < 0 then
                dy = dy + 0.05
            else
                dy = (dy + 0.02) * 1.02
            end
            item[5] = dy
        end
    end
end

--------------------------------------------------------------------------------

local function createbackground()
    ov("blend 0")
    local y, c
    local level = 96
    for y = 0, ht - 1 do
        c = floor((y / ht) * level)
        ov("rgba 0 "..(level - c).." "..c.." 255")
        ov("line 0 "..y.." "..(wd - 1).." "..y)
    end
    if edgegapl > 0 then
        ov(op.black)
        ov("fill 0 0 "..edgegapl.." "..(ht - 1))
    end
    if edgegapr > 0 then
        ov(op.black)
        ov("fill "..(wd - edgegapr).." 0 "..edgegapr.." "..(ht - 1))
    end

    -- save the background clip
    ov("copy 0 0 "..wd.." "..ht.." bg")
end

--------------------------------------------------------------------------------

local function drawbackground()
    ov("blend 0")
    ov("paste 0 0 bg")
end

--------------------------------------------------------------------------------

local function drawbricks()
    local xoff = shadowx
    local yoff = shadowy
    local startpass = 1
    -- check whether the draw shadows
    if showshadows == 0 then
        xoff = 0
        yoff = 0
        startpass = 2
        ov("blend 0")
    else
        ov("blend 1")
        ov(shadowcol)
    end
    for pass = startpass, 2 do
        for y = 1, numrows do
            local bricks = rows[y]
            bricky = (y + offsety) * brickht
            if pass == 2 then
                ov(brickcols[y])
            end
            for x = 1, numcols do
                if bricks[x] then
                    brickx = (x - 1) * brickwd
                    ov("fill "..(brickx + xoff + edgegapl).." "..(bricky + yoff).." "..(brickwd - 1).." "..(brickht - 1))
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
    local oldwidth = ov("lineoption width "..floor(ballsize / 2))
    if showshadows == 1 then
        ov(shadowcol)
        ov("blend 1")
        ov("ellipse "..(floor(ballx - ballsize / 2) + shadowx).." "..(floor(bally - ballsize / 2) + shadowy).." "..floor(ballsize).." "..floor(ballsize))
    end
    ov(op.white)
    ov("ellipse "..floor(ballx - ballsize / 2).." "..floor(bally - ballsize / 2).." "..floor(ballsize).." "..floor(ballsize))
    if rand() < ballpartchance * framemult then
        createparticles(ballx + ballsize / 2, bally - ballsize / 2, ballsize, ballsize, ballparticles)
    end
    ov("lineoption width "..oldwidth)
end

--------------------------------------------------------------------------------

local function drawbat()
    if showshadows == 1 then
        ov(shadowcol)
        ov("blend 1")
        ov("fill "..(floor(batx) + shadowx).." "..(floor(baty) + shadowy).." "..batwd.." "..batht)
    end
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
    rows        = {}
    brickwd     = floor(wd / numcols)
    brickht     = floor(ht / 40)
    bricksleft  = 0
    totalbricks = 0
    offsety     = level + 1
    if offsety > maxoffsety then
        offsety = maxoffsety
    end

    -- check for bonus level
    bonuscurrent = bonustime
    bonuslevel = false
    if (level  % bonusinterval) == 0 then
       bonuslevel   = true
    end

    -- distribute any gap left and right
    local edgegap = wd - brickwd * numcols
    edgegapl = floor(edgegap / 2)
    edgegapr = edgegap - edgegapl

    -- set the required bricks alive
    local match = 1
    for y = 1, numrows do
        local bricks = {}
        if bonuslevel then
            local bonusrow = bonus[y]
            match = 1
            for x = numcols - 1, 0, -1 do
                if (bonusrow & match) == match then
                    bricks[x + 1] = true
                    bricksleft = bricksleft + 1
                else
                    bricks[x + 1] = false
                end
                match = match + match
            end
        else
            for x = 1, numcols do
                bricks[x] = true
                bricksleft = bricksleft + 1
            end
        end
        rows[y] = bricks
    end
    totalbricks = bricksleft
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

local function togglefullscreen()
    fullscreen = 1 - fullscreen
    writesettings()
    setfullscreen()
end

--------------------------------------------------------------------------------

local function toggletiming()
    showtiming = 1 - showtiming
    writesettings()
    notify("Timing", showtiming)
end

--------------------------------------------------------------------------------

local function toggleparticles()
    showparticles = 1 - showparticles
    writesettings()
    notify("Particles", showparticles)
end

--------------------------------------------------------------------------------

local function toggleautopause()
    autopause = 1 - autopause
    writesettings()
    notify("Autopause", autopause)
end

--------------------------------------------------------------------------------

local function toggleautostart()
    autostart = 1 - autostart
    writesettings()
    notify("Autostart", autostart)
end

--------------------------------------------------------------------------------

local function togglemouse()
    showmouse = 1 - showmouse
    writesettings()
    showcursor()
    notify("Mouse pointer", showmouse)
end

--------------------------------------------------------------------------------

local function toggleshadowdisplay()
    showshadows = 1 - showshadows
    writesettings()
    notify("Shadows", showshadows)
end

--------------------------------------------------------------------------------

local function processstandardkeys(event)
    if event == "key f11 none" then
        -- toggle fullscreen
        togglefullscreen()
    elseif event == "key a none" then
        -- toggle autopause when mouse moves off overlay
        toggleautopause()
    elseif event == "key d none" then
        -- toggle shadow display
        toggleshadowdisplay()
    elseif event == "key m none" then
        -- toggle mouse cursor display when not fullscreen
        togglemouse()
    elseif event == "key p none" then
        -- toggle particle display
        toggleparticles()
    elseif event == "key s none" then
        -- toggle autostart when mouse moves onto overlay
        toggleautostart()
    elseif event == "key t none" then
        -- toggle timing display
        toggletiming()
    elseif event == "key tab none" then
        -- show options
        showoptions = not showoptions
    end
end

--------------------------------------------------------------------------------

local function processinput()
    -- check for click, enter or return
    local event = g.getevent()
    if #event > 0 then
        local _, x, y, button, mods
        button = ""
        if event:find("^oclick") then
            _, x, y, button, mods = split(event)
        end
        -- right click quits game if paused
        if button == "right" then
            balls = 0
            showoptions = false
        elseif button == "left" or event == "key enter none" or event == "key return none" or event == "key space none" then
            -- left click, enter or space starts game, toggles pause or dismisses settings
            if showoptions then
                showoptions = false
            elseif newball then
                newball = false
                pause   = false
            else
                pause = not pause
            end
        elseif event == "key left none" then
            -- left arrow moves bat left
            batkeydir = -1
        elseif event == "key right none" then
            -- right arrow moves bat right
            batkeydir = 1
        elseif event == "kup left" then
            -- left key up stops left movement
            if batkeydir == -1 then
                batkeydir = 0
            end
        elseif event == "kup right" then
            -- right key up stops right movement
            if batkeydir == 1 then
                batkeydir = 0
            end
        else
            processstandardkeys(event)
        end
    end
end

--------------------------------------------------------------------------------

local function processendinput()
    local event = g.getevent()
    if #event > 0 then
        local _, x, y, button, mods
        button = ""
        if event:find("^oclick") then
            _, x, y, button, mods = split(event)
        end
        -- right click quits application or dismisses settings
        if button == "right" then
            -- quit application if game over
            if balls == 0 then
                again    = false
                finished = true
            else
                -- otherwise quit game
                balls = 0
            end
            showoptions = false
        elseif button == "left" or event == "key enter none" or event == "key return none" or event == "key space none" then
            -- left click, enter or space restarts game or dismisses settings
            if showoptions then
                showoptions = false
            else
                finished = true
            end
        else
            processstandardkeys(event)
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
    local xscale = newwd / wd
    local yscale = newht / ht

    -- resize overlay
    wd = newwd
    ht = newht
    ov("resize "..wd.." "..ht)
    fontscale = wd / minwd
    if (ht / minht) < fontscale then
        fontscale = ht / minht
    end

    -- scale bat, ball and bricks
    brickwd  = floor(wd / numcols)
    brickht  = floor(ht / 40)
    batwd    = floor(wd / 10)
    batht    = brickht
    ballsize = wd / 80
    local edgegap  = wd - brickwd * numcols
    edgegapl = floor(edgegap / 2)
    edgegapr = edgegap - edgegapl

    -- reposition the bat and ball
    batx  = batx * xscale
    baty  = ht - batht * 4
    ballx = ballx * xscale
    bally = bally * yscale

    -- reposition particles
    for i = 1, #particles do
        local item = particles[i]
        item[2] = item[2] * xscale
        item[3] = item[3] * yscale
    end

    -- resize shadow
    initshadow()

    -- recreate background
    ov("freeclip bg")
    createbackground()

    -- recreate static text
    createstatictext()
end

--------------------------------------------------------------------------------

local function drawtextclip(name, y, align, x)
    align = align or aligncenter
    x = x or 0
    local message = messages[name]
    local w = message[4]
    local h = message[5]
    local textx = edgegapl
    -- process alignment
    if align == aligncenter then
        textx = (wd - w) / 2 + edgegapl
    elseif align == alignright then
        textx = wd - w - edgegapr
    elseif align == alignraw then
        textx = 0
    end
    -- paste clip
    ov("paste "..floor(x + textx).." "..y.." "..name)
    return w, h
end

--------------------------------------------------------------------------------

local function centertextclip(name, value, y, col)
    col = col or op.white

    -- get the clip
    local message = messages[name]
    -- get the clip string
    local leftval = message[1]
    -- get the clip font size
    local size    = tostring(message[2])
    -- find the font size clip
    local charmsg = messages[size]
    -- get the width of the font size clip
    local charw   = charmsg[4] - abs(shadtxtx)
    -- get the length of the full message
    local length  = (leftval..tostring(value)):len()
    -- compute the center position
    local x = floor((wd - length * charw) / 2)
    -- draw the clip
    drawtextclip(name, y, alignleft, x)
    -- draw the value
    ov("font "..floor(tonumber(size) * fontscale).." mono")
    shadowtext(x + charw * leftval:len(), y, value, alignleft, col)
end

--------------------------------------------------------------------------------

local function drawscoreline()
    ov("blend 1")
    ov("font "..floor(10 * fontscale).." mono")
    local w, h = drawtextclip("score", 5, alignleft, 3)
    shadowtext(w + 5, 5, score)
    local w = shadowtext(-5, 5, balls, alignright)
    drawtextclip("balls", 5, alignright, -w)
    if newhigh then
        centertextclip("highg", hiscore, 5, op.green)
    else
        centertextclip("high", hiscore, 5)
    end
    if combo > 2 then
        if combo == maxcombo then
            centertextclip("combog", combo - 1, ht - h, op.green)
        else
            centertextclip("combo", combo - 1, ht - h)
        end
    end
    if not newball and not pause and not showoptions and bonuslevel and bonuscurrent >= 0 then
        if bonuscurrent < 10 then
            centertextclip("timer", string.format("%.1f", bonuscurrent), floor(ht / 2), op.red)
        elseif bonuscurrent < 20 then
            centertextclip("timey", string.format("%.1f", bonuscurrent), floor(ht / 2), op.yellow)
        else
            centertextclip("timeg", string.format("%.1f", bonuscurrent), floor(ht / 2), op.green)
        end
        if bricksleft <= bonusgreen then
            centertextclip("remaing", bricksleft, floor(ht / 2 + 25 * fontscale), op.green)
        elseif bricksleft <= bonusyellow then
            centertextclip("remainy", bricksleft, floor(ht / 2 + 25 * fontscale), op.yellow)
        else
            centertextclip("remainr", bricksleft, floor(ht / 2 + 25 * fontscale), op.red)
        end
    end
end

--------------------------------------------------------------------------------

local function drawgameover()
    ov("blend 1")
    if newhigh then
        local highscorew = drawtextclip("newhigh", floor(ht / 2 + 96 * fontscale))
        createparticles(edgegapl + floor(wd / 2) + highscorew / 2, floor(ht / 2 + 96 * fontscale), highscorew, 1, highparticles)
    end
    combo = gamecombo
    if newcombo then
        local combow = drawtextclip("newcombo", floor(ht / 2 + 118 * fontscale))
        createparticles(edgegapl + floor(wd / 2) + combow / 2, floor(ht / 2 + 118 * fontscale), combow, 1, comboparticles)
    end
    drawtextclip("gameover", floor(ht / 2 - 30 * fontscale))
    drawtextclip("restart", floor(ht / 2 + 30 * fontscale))
    drawtextclip("quit", floor(ht / 2 + 52 * fontscale))
    drawtextclip("option", floor(ht / 2 + 74 * fontscale))
end

--------------------------------------------------------------------------------

local function drawlevelcomplete()
    ov("blend 1")
    ov("font "..floor(20 * fontscale).." mono")
    shadowtext(0, ht / 2 - 30 * fontscale, "Level "..level.." complete!", aligncenter, op.green)
    drawtextclip("continue", floor(ht / 2 + 30 * fontscale))
    drawtextclip("quitgame", floor(ht / 2 + 52 * fontscale))
    drawtextclip("option", floor(ht / 2 + 74 * fontscale))
end

--------------------------------------------------------------------------------

local function drawbonuscomplete(remainingtime, bonusscore)
    ov("blend 1")
    ov("font "..floor(15 * fontscale).." mono")
    drawtextclip("bcomplete", floor(ht / 2 - 30 * fontscale))
    if bricksleft <= bonusgreen  then
        shadowtext(0, ht / 2 - 0 * fontscale, "Bricks left "..bricksleft.." = "..bonusscore, aligncenter, op.green)
    elseif bricksleft < bonusyellow then
        shadowtext(0, ht / 2 - 0 * fontscale, "Bricks left "..bricksleft.." = "..bonusscore, aligncenter, op.yellow)
    else
        shadowtext(0, ht / 2 - 0 * fontscale, "Bricks left "..bricksleft.." = No Bonus", aligncenter, op.red)
    end
    drawtextclip("continue", floor(ht / 2 + 30 * fontscale))
    drawtextclip("quitgame", floor(ht / 2 + 52 * fontscale))
    drawtextclip("option", floor(ht / 2 + 74 * fontscale))
end

--------------------------------------------------------------------------------

local function drawpause()
    ov("blend 1")
    drawtextclip("pause", floor(ht / 2 - 15 * fontscale))
    if offoverlay and autopause ~= 0 and autostart ~= 0 then
       drawtextclip("focus", floor(ht / 2 + 22 * fontscale))
    else
       drawtextclip("resume", floor(ht / 2 + 22 * fontscale))
       drawtextclip("quitgame", floor(ht / 2 + 44 * fontscale))
       drawtextclip("option", floor(ht / 2 + 66 * fontscale))
    end
end

--------------------------------------------------------------------------------

local function drawnewball()
    ov("blend 1")
    drawtextclip("newball", floor(ht / 2 + 22 * fontscale))
    drawtextclip("control", floor(ht / 2 + 44 * fontscale))
    drawtextclip("quitgame", floor(ht / 2 + 66 * fontscale))
    drawtextclip("option", floor(ht / 2 + 88 * fontscale))
    drawtextclip(balls.."left", floor(ht / 2 - 15 * fontscale))
    if bonuslevel then
        drawtextclip("bonus", floor(ht / 2 - 52 * fontscale))
    else
        centertextclip("level", level, floor(ht / 2 - 52 * fontscale))
    end
end

--------------------------------------------------------------------------------

local function drawtiming(t)
    times[timenum] = t
    timenum = timenum + 1
    if timenum > numtimes then
        timenum = 1
    end
    local average = 0
    for i = 1, #times do
        average = average + times[i]
    end
    average = average / #times
    ov("font "..floor(8 * fontscale).." mono")
    shadowtext(-5, ht - (8 * fontscale) - (4 * fontscale), string.format("%.1fms", average), alignright)
end

--------------------------------------------------------------------------------

local function drawoption(key, setting, state, leftx, h, y, color)
    drawtextclip(key, y, alignleft, leftx)
    drawtextclip(setting, y)
    drawtextclip(state, y, alignright, -leftx)
    return y + h
end

--------------------------------------------------------------------------------

local function drawoptions()
    local leftx = floor(wd / 6)
    local state = {[0] = "off", [1] = "on"}

    -- draw header
    ov("blend 1")
    ov("font "..floor(10 * fontscale).." mono")
    local w, h = maketext(0, 0, "M")
    local y = floor((ht - 8 * h) / 2)
    y = drawoption("key", "function", "state", leftx, h, y, op.white)

    -- draw options
    y = drawoption("a", "autopause", state[autopause], leftx, h, y)
    y = drawoption("d", "shadows", state[showshadows], leftx, h, y)
    y = drawoption("m", "mouse", state[showmouse], leftx, h, y)
    y = drawoption("p", "particles", state[showparticles], leftx, h, y)
    y = drawoption("s", "autostart", state[autostart], leftx, h, y)
    y = drawoption("t", "timing", state[showtiming], leftx, h, y)
    y = drawoption("f11", "fullscreen", state[fullscreen], leftx, h, y)

    -- draw close options
    drawtextclip("close", y + h)
    if balls == 0 then
        drawtextclip("quit", floor(y + h * 2.5))
    else
        drawtextclip("quitgame", floor(y + h * 2.5))
    end
end

--------------------------------------------------------------------------------

local function checkforresize()
    local newwd, newht = g.getview(g.getlayer())
    if newwd ~= wd or newht ~= ht then
        resizegame(newwd, newht)
    end
end

--------------------------------------------------------------------------------

local function updatebatposition()
    local mousepos = ov("xy")
    if mousepos ~= "" then
        local mousex, mousey = split(mousepos)
        if mousex ~= lastx then
            lastx = mousex
            batx = tonumber(mousex) - batwd / 2
            if batx < edgegapl then
                batx = edgegapl
            elseif batx > wd - edgegapr - batwd then
                batx = wd - edgegapr - batwd
            end
        end
        -- check if mouse was off overlay
        if offoverlay then
            -- check if paused
            if pause and autostart ~= 0 and autopause ~= 0 then
                pause = false
            end
        end
        offoverlay = false
    else
        -- mouse off overlay
        offoverlay = true
        -- check for autopause
        if autopause ~= 0 and fullscreen == 0 then
            pause = true
        end
    end

    -- check for keyboard move
    if batkeydir == -1 then
        -- move bat left
        batx = batx - (wd / 60)
        if batx < edgegapl then
            batx = edgegapl
        end
    elseif batkeydir == 1 then
        -- move bat right
        batx = batx + (wd / 60)
        if batx > wd - edgegapr - batwd then
            batx = wd - edgegapr - batwd
        end
    end
end

--------------------------------------------------------------------------------

local function clearbonusbricks()
    local bricks = {}
    for y = 1, numrows do
        bricks = rows[y]
        for x = 1, numcols do
            if bricks[x] then
                bricks[x] = false
                createparticles(x * brickwd + edgegapl, (y + offsety) * brickht, brickwd, brickht, brickparticles)
            end
        end
    end
end

--------------------------------------------------------------------------------

local function computebonus()
    local bonusscore = 0
    if bricksleft <= bonusgreen then
        bonusscore = (totalbricks - bricksleft) * (100 + (level - 1) * 10)
    elseif bricksleft <= bonusyellow then
        bonusscore = (totalbricks - bricksleft) * (50 + (level - 1) * 10)
    end
    score = score + bonusscore

    return bonusscore
end

--------------------------------------------------------------------------------

local function breakout()
    -- set font
    local oldfont   = ov("font 16 mono")
    local oldbg     = ov("textoption background 0 0 0 0")
    local oldblend  = ov("blend 0")
    local oldcursor = ov("cursor arrow")

    -- read saved settings
    readsettings()
    setfullscreen()

    -- create the background
    createbackground()

    -- play games until finished
    balls    = 3
    score    = 0
    level    = 1
    again    = true
    newhigh  = false
    newcombo = false

    -- initialise the bat and ball
    initbat()
    initball()

    -- welcome message
    notify("Golly Breakout build "..build)

    -- create static text
    createstatictext()

    -- main loop
    while again do
        -- initialize the bricks
        initbricks()

        -- intiialize the bat
        local bathits = 0
        local maxhits = 7
        lastx         = -1

        -- initialize the ball
        local balldx    = 0.5
        local balldy    = -1
        local maxspeed  = 2.2 + (level - 1) * 0.1
        if maxspeed > 3 then
            maxspeed = 3
        end
        local speedinc  = 0.02
        local speeddef  = 1 + (level - 1) * speedinc * 4
        if speeddef > maxspeed then
            speeddef = maxspeed
        end
        local ballspeed = speeddef
        local speeddiv  = 3

        -- initialize shadow
        initshadow()

        -- initialize particles
        initparticles()

        -- whether alive
        newball = true
        pause   = false

        -- whether mouse off overlay
        offoverlay = false

        -- reset combo
        combo     = 1
        combomult = 1

        -- game loop
        while balls > 0 and bricksleft > 0 and bonuscurrent > 0 do
            -- time frame
            local frametime = g.millisecs()

            -- check for mouse click or key press
            processinput()

            -- check if size of layer has changed
            checkforresize()

            -- draw the background
            drawbackground()

            -- check if paused
            if not pause and not showoptions and bonuscurrent > 0 then
                -- check for new ball
                if not newball then
                    -- update ball position incrementally
                    local framesteps = floor(numsteps * framemult)
                    local i = 1
                    while i <= framesteps and not newball do
                        i = i + 1
                        local stepx = ((balldx * ballspeed * ballsize) / speeddiv) / numsteps
                        local stepy = ((balldy * ballspeed * ballsize) / speeddiv) / numsteps
                        ballx = ballx + stepx
                        bally = bally + stepy

                        -- check for ball hitting left or right boundary
                        if ballx < ballsize / 2 + edgegapl or ballx >= wd - edgegapr - ballsize / 2 then
                            createparticles(ballx, bally, 1, 1, wallparticles)
                            -- invert x direction
                            balldx = -balldx
                            ballx  = ballx - stepx
                        end

                        -- check for ball hitting top boundary
                        if bally < ballsize / 2 then
                            createparticles(ballx, bally, 1, 1, wallparticles)
                            -- ball hit top so speed up a little bit
                            balldy    = -balldy
                            bally     = bally - stepy
                            ballspeed = ballspeed + speedinc / 2
                            if ballspeed > maxspeed then
                                ballspeed = maxspeed
                            end

                        -- check for ball hitting bottom boundary
                        elseif bally >= ht then
                            -- check for bonus level
                            if bonuslevel then
                                -- end bonus level
                                bonuscurrent = 0
                            else
                                -- ball lost!
                                balls     = balls - 1
                                balldy    = -1
                                balldx    = 0.5
                                ballspeed = speeddef
                                newball   = true
                                combo     = 1
                                combomult = 1
                                -- destroy bat if no balls left
                                if balls == 0 then
                                   createparticles(batx + batwd, baty, batwd, batht, lostparticles)
                                end
                            end
                            -- exit loop
                            i = framesteps + 1

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
                            balldy  = -balldy
                            bally   = baty
                            bathits = bathits + 1
                            -- move the bricks down after a number of bat hits
                            if bathits == maxhits then
                                bathits = 0
                                if offsety < maxoffsety then
                                    offsety = offsety + 1
                                end
                           end
                           createparticles(ballx, bally - ballsize / 2, 1, 1, batparticles)
                           -- reset combo
                           combo     = 1
                           combomult = 1
                        end

                        -- check for ball hitting brick
                        bricky = floor(bally / brickht) - offsety
                        if bricky >= 1 and bricky <= numrows then
                            brickx = floor((ballx - edgegapl) / brickwd) + 1
                            if rows[bricky][brickx] then
                                -- hit a brick!
                                rows[bricky][brickx] = false
                                -- adjust score
                                score     = score + floor((level + 9) * (numrows - bricky + 1) * combomult)
                                if score > hiscore then
                                    hiscore = score
                                    newhigh = true
                                end
                                -- increment combo
                                combo     = combo + 1
                                combomult = combomult * combofact
                                if combo > maxcombo then
                                    maxcombo = combo
                                    newcombo = true
                                end
                                if combo > gamecombo then
                                    gamecombo = combo
                                end
                                -- work out which axis to invert
                                local lastbricky = floor((bally - stepy) / brickht) - offsety
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
                                -- create particles
                                createparticles(brickx * brickwd + edgegapl, (bricky + offsety) * brickht, brickwd, brickht, brickparticles)
                                -- one less brick
                                bricksleft = bricksleft - 1
                            end
                        end
                    end
                end
            end

            -- update bat position
            updatebatposition()

            -- if new ball then set ball to sit on bat
            if newball then
                ballx = batx + batwd / 2
                bally = baty - ballsize
            end

            -- draw the particles
            drawparticles()

            -- draw the bricks
            drawbricks()

            -- draw the ball
            if balls > 0 then
                drawball()
            end

            -- draw the bat
            drawbat()

            -- draw the score, high score and lives
            drawscoreline()

            -- check for text overlay
            if showoptions then
                drawoptions()
            elseif pause then
                drawpause()
            elseif newball and balls > 0 then
                drawnewball()
            end

            -- draw timing if on
            if showtiming == 1 then
                drawtiming(g.millisecs() - frametime)
            end

            -- update notification
            updatenotification()

            -- update the display
            ov("update")

            -- pause until frame time reached
            while g.millisecs() - frametime < 16 do end

            -- check what the actual frame time was and scale speed accordingly
            framemult = 1
            local finaltime = g.millisecs() - frametime
            if finaltime > 16.7 then
                framemult = finaltime / 16.7
            end

            -- update bonus time if on bonus level
            if bonuslevel and not pause and not newball and not showoptions then
                bonuscurrent = bonuscurrent - (finaltime / 1000)
            end
        end

        -- check for bonus level complete
        local bonusfinal = bonuscurrent
        local bonusscore = 0
        if bonuslevel then
            bonusscore = computebonus()
            clearbonusbricks()
        end

        -- save high score and max combo
        if newhigh or newcombo then
            writesettings()
        end

        -- draw best combo
        notify("Best Combo x"..maxcombo - 1)

        -- loop until mouse button clicked or enter pressed
        bonuscurrent = -1
        finished = false
        while not finished do
            -- time frame
            local frametime = g.millisecs()

            -- check if size of layer has changed
            checkforresize()

            -- draw background
            drawbackground()

            -- update bat position
            updatebatposition()

            -- draw particles
            drawparticles()

            -- draw bricks
            drawbricks()

            -- check why game finished
            if showoptions then
                drawoptions()
            else
                if balls == 0 then
                    -- game over
                    drawgameover()
                else
                    -- draw bat
                    drawbat()
                    if bonuslevel then
                        -- end of bonus level
                        drawbonuscomplete(bonusfinal, bonusscore)
                    else
                        -- level complete
                        drawlevelcomplete()
                    end
                end
            end

            -- draw score line
            drawscoreline()

            -- get key or mouse event
            processendinput()

            -- draw timing if on
            if showtiming == 1 then
                drawtiming(g.millisecs() - frametime)
            end

            -- update notification
            updatenotification()

            -- update the display
            ov("update")

            -- pause until frame time reached
            while g.millisecs() - frametime < 16 do end

            -- check what the actual frame time was and scale speed accordingly
            framemult = 1
            local finaltime = g.millisecs() - frametime
            if finaltime > 16.7 then
                framemult = finaltime / 16.7
            end
        end

        -- check why game finished
        if balls == 0 then
            -- reset
            score     = 0
            balls     = 3
            level     = 1
            newhigh   = false
            newcombo  = false
            gamecombo = 1
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
