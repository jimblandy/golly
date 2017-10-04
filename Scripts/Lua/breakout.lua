-- Breakout for Golly
-- Author: Chris Rowett (crowett@gmail.com), November 2016

local build = 65
local g = golly()
-- require "gplus.strict"
local gp    = require "gplus"
local split = gp.split
local op    = require "oplus"
local ov    = g.overlay
local floor = math.floor
local rand  = math.random
local abs   = math.abs

-- text alignment
local alignleft   = 0
local aligncenter = 1
local alignright  = 2
local aligntop    = 3
local alignbottom = 4
local fontscale   = 1

-- overlay width and height
local wd, ht   = g.getview(g.getlayer())
local minwd    = 400
local minht    = 400
local edgegapl = 0
local edgegapr = 0

-- background settings
local bgclip = "bg"

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
local lastx

-- ball settings
local ballsize  = wd / 80
local ballx     = 0
local bally     = 0
local numsteps  = 80

-- particle settings
local particles       = {}
local brickparticles  = floor(brickwd * brickht / 10)
local ballparticles   = 1
local ballpartchance  = 0.25
local wallparticles   = 20
local batparticles    = 20
local highparticles   = 4
local comboparticles  = 4
local bonusparticles  = 4
local lostparticles   = 1024
local bonusparticlesg = 6
local bonusparticlesy = 3

-- points settings
local points = {}

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
local comboraw   = 0
local comboextra = 0
local gamecombo  = 1
local maxcombo   = 2
local balls      = 3
local newhigh    = false
local newcombo   = false
local newbonus   = false
local offoverlay = false
local finished   = false
local again      = true

-- timing settings
local times         = {}
local timenum       = 1
local numtimes      = 8
local framemult     = 1
local framecap      = 100
local sixtyhz       = 16.7

-- game options
local brickscore    = 1
local showtiming    = 0
local showparticles = 1
local autopause     = 0
local autostart     = 0
local showmouse     = 1
local showshadows   = 1
local confirmquit   = 0
local showoptions   = false
local confirming    = false
local comboscore    = 1
local soundvol      = 1
local musicvol      = 1

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
local bestbonus     = 0

-- key highlight color and names
local keycol   = "rgba 32 32 32 255"
local mousecol = "rgba 48 0 0 255"
local keynames = {
     [keycol]   = { "Esc", "Tab", "Enter" },
     [mousecol] = { "Click", "Right Click", "Mouse" }
}

-- static messages and clip names
local optcol = "rgba 192 192 192 255"
local messages = {
    ["gameover"]   = { text = "Game Over", size = 30, color = op.red },
    ["newball"]    = { text = "Click or Enter to launch ball", size = 10, color = op.white },
    ["control"]    = { text = "Mouse to move bat", size = 10, color = op.white },
    ["askquit"]    = { text = "Quit Game?", size = 15, color = op.yellow },
    ["pause"]      = { text = "Paused", size = 15, color = op.yellow },
    ["askleft"]    = { text = "Click or Enter to Confirm", size = 10, color = op.white },
    ["askright"]   = { text = "Right Click to Cancel", size = 10, color = op.white },
    ["resume"]     = { text = "Click or Enter to continue", size = 10, color = op.white },
    ["focus"]      = { text = "Move mouse onto overlay to continue", size = 10, color = op.white },
    ["quitgame"]   = { text = "Right Click to quit game", size = 10, color = op.white },
    ["option"]     = { text = "Tab for Game Settings", size = 10, color = op.white },
    ["restart"]    = { text = "Click or Enter to start again", size = 10, color = op.white },
    ["quit"]       = { text = "Right Click or Esc to exit", size = 10, color = op.white },
    ["continue"]   = { text = "Click or Enter for next level", size = 10, color = op.white },
    ["newhigh"]    = { text = "New High Score!", size = 10, color = op.green },
    ["newcombo"]   = { text = "New Best Combo!", size = 10, color = op.green },
    ["newbonus"]   = { text = "New Best Bonus!", size = 10, color = op.green },
    ["close"]      = { text = "Click or Tab to close Game Settings", size = 10, color = op.white },
    ["autopause"]  = { text = "Autopause", size = 10, color = optcol },
    ["brickscore"] = { text = "Brick Score", size = 10, color = optcol },
    ["comboscore"] = { text = "Combo Score", size = 10, color = optcol },
    ["shadows"]    = { text = "Shadows", size = 10, color = optcol },
    ["mouse"]      = { text = "Mouse Pointer", size = 10, color = optcol },
    ["particles"]  = { text = "Particles", size = 10, color = optcol },
    ["confirm"]    = { text = "Confirm Quit", size = 10, color = optcol },
    ["autostart"]  = { text = "Autostart", size = 10, color = optcol },
    ["timing"]     = { text = "Timing", size = 10, color = optcol },
    ["fullscreen"] = { text = "Fullscreen", size = 10, color = optcol },
    ["function"]   = { text = "Function", size = 10, color = op.white },
    ["on"]         = { text = "On", size = 10, color = op.green },
    ["off"]        = { text = "Off", size = 10, color = op.red },
    ["state"]      = { text = "State", size = 10, color = op.white },
    ["key"]        = { text = "Key", size = 10, color = op.white },
    ["a"]          = { text = "A", size = 10, color = optcol },
    ["b"]          = { text = "B", size = 10, color = optcol },
    ["c"]          = { text = "C", size = 10, color = optcol },
    ["d"]          = { text = "D", size = 10, color = optcol },
    ["m"]          = { text = "M", size = 10, color = optcol },
    ["p"]          = { text = "P", size = 10, color = optcol },
    ["q"]          = { text = "Q", size = 10, color = optcol },
    ["s"]          = { text = "S", size = 10, color = optcol },
    ["t"]          = { text = "T", size = 10, color = optcol },
    ["f11"]        = { text = "F11", size = 10, color = optcol },
    ["-"]          = { text = "-", size = 10, color = optcol },
    ["="]          = { text = "+", size = 10, color = optcol },
    ["["]          = { text = "[", size = 10, color = optcol },
    ["]"]          = { text = "]", size = 10, color = optcol },
    ["sound"]      = { text = "Sound Volume", size = 10, color = optcol },
    ["music"]      = { text = "Music Volume", size = 10, color = optcol },
    ["fxvol"]      = { text = "100%", size = 10, color = op.green },
    ["musicvol"]   = { text = "100%", size = 10, color = op.green },
    ["level"]      = { text = "Level ", size = 15, color = op.white },
    ["bonus"]      = { text = "Bonus Level", size = 15, color = op.white },
    ["bcomplete"]  = { text = "Bonus Level Complete", size = 15, color = op.white },
    ["remain"]     = { text = "Bricks left", size = 10, color = op.green },
    ["time"]       = { text = "Time", size = 15, color = op.green },
    ["left"]       = { text = "3 balls left", size = 15, color = op.yellow },
    ["score"]      = { text = "Score", size = 10, color = op.white },
    ["high"]       = { text = "High Score", size = 10, color = op.white },
    ["balls"]      = { text = "Balls", size = 10, color = op.white },
    ["combo"]      = { text = "Combo", size = 10, color = op.white },
    ["notify"]     = { text = "Notify", size = 7, color = op.white },
    ["ms"]         = { text = "1 ms", size = 7, color = op.white },
    ["complete"]   = { text = "Level Complete", size = 20, color = op.green },
    ["awarded"]    = { text = "No Bonus", size = 15, color = op.red }
}

-- music tracks
local tracks = { "gamestart", "gameover", "gameloop", "lostball", "levelcompleteloop", "bonusloop" }

--------------------------------------------------------------------------------

local function showcursor()
    if showmouse == 0 then
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
        brickscore    = tonumber(f:read("*l")) or 1
        confirmquit   = tonumber(f:read("*l")) or 1
        bestbonus     = tonumber(f:read("*l")) or 0
        comboscore    = tonumber(f:read("*l")) or 1
        soundvol      = tonumber(f:read("*l")) or 100
        musicvol      = tonumber(f:read("*l")) or 70
        f:close()

        if soundvol == 1 then
           soundvol = 100
        end
        if musicvol == 1 then
           musicvol = 100
        end
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
        f:write(tostring(brickscore).."\n")
        f:write(tostring(confirmquit).."\n")
        f:write(tostring(bestbonus).."\n")
        f:write(tostring(comboscore).."\n")
        f:write(tostring(soundvol).."\n")
        f:write(tostring(musicvol).."\n")
        f:close()
    end
end

--------------------------------------------------------------------------------

local function updatemessage(name, s, color)
    -- lookup the message
    local message = messages[name]
    if color ~= nil then
        message.color = color
    end
    -- get the font size for this message
    ov("font "..floor(message.size * fontscale))
    -- create the text message clips
    message.text = s
    local w, h = op.maketext(message.text, name, message.color, shadtxtx, shadtxty)
    -- save the clip width and height
    message.width  = w
    message.height = h
end   

--------------------------------------------------------------------------------

local function createstatictext()
    -- create each static text clip
    for clipname, message in pairs(messages) do
        updatemessage(clipname, message.text)
    end
end

--------------------------------------------------------------------------------

local function setchannelvolume(channel, vol)
    if (vol == 0) then
        updatemessage(channel.."vol", "Off", op.red)
    else
        updatemessage(channel.."vol", vol.."%", op.green)
    end
    -- update the music volume immediately since it may be playing
    if (channel == "music") then
        for i = 1, #tracks do
            ov("sound volume "..(musicvol / 100).." oplus/sounds/breakout/"..tracks[i]..".ogg")
        end
    end
end

--------------------------------------------------------------------------------

local function stopallsound()
    ov("sound stop")
end

--------------------------------------------------------------------------------

local function stopmusic()
    for i = 1, #tracks do
        ov("sound stop oplus/sounds/breakout/"..tracks[i]..".ogg")
    end
end
--------------------------------------------------------------------------------

local function playsound(name, loop)
    if soundvol > 0 then
        loop = loop or false
        if loop then
            ov("sound loop oplus/sounds/breakout/"..name..".ogg "..(soundvol / 100))
        else
            ov("sound play oplus/sounds/breakout/"..name..".ogg "..(soundvol / 100))
        end
    end
end

--------------------------------------------------------------------------------

local function playmusic(name, loop)
    loop = loop or false
    stopmusic()
    if loop then
        ov("sound loop oplus/sounds/breakout/"..name..".ogg "..(musicvol / 100))
    else
        ov("sound play oplus/sounds/breakout/"..name..".ogg "..(musicvol / 100))
    end
end

--------------------------------------------------------------------------------

local function updatelevel(value)
    level = value
    updatemessage("level", "Level "..level)
    updatemessage("complete", "Level "..level.." complete!")
end

--------------------------------------------------------------------------------

local function updatescore(value)
    score = value
    updatemessage("score", "Score "..score)
end

--------------------------------------------------------------------------------

local function updatehighscore(value)
    local color = op.white
    if newhigh then
        color = op.green
    end
    hiscore = value
    updatemessage("high", "High Score "..hiscore, color)
end

--------------------------------------------------------------------------------

local function updateballs(value)
    balls = value
    updatemessage("balls", "Balls "..balls)
    if balls == 1 then
        updatemessage("left", "Last ball!", op.red)
    elseif balls == 2 then
        updatemessage("left", balls.." balls left", op.yellow)
    else
        updatemessage("left", balls.." balls left", op.green)
    end
end

--------------------------------------------------------------------------------

local function updatecombo(value)
    local color = op.white
    combo = value
    if combo == maxcombo then
        color = op.green
    end
    updatemessage("combo", "Combo x"..combo - 1, color)
end

--------------------------------------------------------------------------------

local function highlightkey(text, x, y, w, h, token, color)
    local t1, t2 = text:find(token)
    if t1 ~= nil then
        local charw = w / text:len()
        local x1 = x + (t1 - 1) * charw
        local oldblend = ov("blend 0")
        local oldrgba = ov(op.black)
        ov("fill "..floor(x1 + shadtxtx).." "..floor(y + shadtxty).." "..floor(charw * (t2 - t1 + 1) + 5).." "..floor(h - 4))
        ov(color)
        ov("fill "..floor(x1).." "..floor(y).." "..floor(charw * (t2 - t1 + 1) + 5).." "..floor(h - 4))
        ov("rgba "..oldrgba)
        ov("blend "..oldblend)
    end
end

--------------------------------------------------------------------------------

local function drawtextclip(name, x, y, xalign, yalign, highlight)
    xalign    = xalign or alignleft
    yalign    = yalign or aligntop
    highlight = highlight or false
    -- lookup the message
    local message = messages[name]
    local w = message.width
    local h = message.height
    local xoffset = 0
    local yoffset = 0
    if xalign == aligncenter then
        xoffset = (wd - w) / 2
    elseif xalign == alignright then
        xoffset = wd - w - edgegapr - edgegapl
    end
    if yalign == aligncenter then
        yoffset = (ht - h) / 2
    elseif yalign == alignbottom then
        yoffset = ht - h
    end
    -- check for highlight text
    if highlight == true then
        for color, list in pairs(keynames) do
            for i, name in pairs(list) do
                highlightkey(message.text, floor(x + xoffset) + edgegapl, floor(y + yoffset), w, h, name, color)
            end
        end
    end
    -- draw the text clip
    op.pastetext(floor(x + xoffset) + edgegapl, floor(y + yoffset), nil, name)
    -- return clip dimensions
    return w, h
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
            -- check which phase
            if notifycurrent < notifytrans then
                -- appear
                y = (notifycurrent / notifytrans) * (8 * fontscale)
            elseif notifycurrent > notifyduration - notifytrans then
                -- disappear
                y = (notifyduration - notifycurrent) / notifytrans * (8 * fontscale)
            else
                -- hold
                y = (8 * fontscale)
            end
            -- draw notification
            drawtextclip("notify", 4, (8 * fontscale) - y, alignleft, alignbottom)
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
    -- create the text clip
    updatemessage("notify", notifymessage)
    if notifycurrent ~= 0 then
        notifycurrent = notifytrans
    end
end

--------------------------------------------------------------------------------

local function initparticles()
    particles = {}
end

--------------------------------------------------------------------------------

local function createparticles(x, y, areawd, areaht, howmany, color)
    color = color or "rgba 255 255 255 255"
    -- find the first free slot
    local i = 1
    while i <= #particles and particles[i].alpha > 0 do
        i = i + 1
    end
    for j = 1, howmany do
        local item = { alpha = 255, x = x - rand(floor(areawd)), y = y + rand(floor(areaht)), dx = rand() - 0.5, dy = rand() - 0.5, color = color }
        particles[i] = item
        i = i + 1
        -- find the next free slot
        while i <= #particles and particles[i].alpha > 0 do
            i = i + 1
        end
    end
end

--------------------------------------------------------------------------------

local function drawparticles()
    ov("blend 1")
    local xy = {}
    local m  = 1
    local lastcol = ""
    local color
    for i = 1, #particles do
        local item  = particles[i]
        local scale = ht / 1000
        -- check if particle is still alive
        if item.alpha > 0 then
            if showparticles ~= 0 then
                color = item.color:sub(1, -4)..floor(item.alpha)
                if color ~= lastcol then
                    if m > 1 then
                        ov("fill "..table.concat(xy, " "))
                        m = 1
                        xy = {}
                    end
                    ov(color)
                    lastcol = color
                end
                xy[m] = floor(item.x)
                xy[m + 1] = floor(item.y)
                xy[m + 2] = "2 2"
                m = m + 3
            end
            -- fade item
            item.alpha = item.alpha - 4 * framemult
            if item.alpha < 0 then
                item.alpha = 0
            end
            item.x = item.x + item.dx * framemult * scale
            item.y = item.y + item.dy * framemult * scale
            item.dx = item.dx * 0.99
            if item.dy < 0 then
                item.dy = item.dy + 0.05
            else
                item.dy = (item.dy + 0.02) * 1.02
            end
        end
    end
    if m > 1 then
        ov("fill "..table.concat(xy, " "))
    end
end

--------------------------------------------------------------------------------

local function initpoints()
    points = {}
end

--------------------------------------------------------------------------------

local function createpoints(x, y, value)
    -- find the first free slot
    local i = 1
    while i <= #points and points[i].duration > 0 do
        i = i + 1
    end
    -- create the clip
    ov("font "..floor(7 * fontscale).." mono")
    local w, h = op.maketext(value, "point"..i, op.white, shadtxtx, shadtxty)

    -- save the item
    local item = { duration = 60, x = floor(x + brickwd / 2 - w / 2), y = floor(y + brickht / 2 - h / 2) }
    points[i] = item
end

--------------------------------------------------------------------------------

local function drawpoints()
    ov("blend 1")
    for i = 1, #points do
        local item = points[i]
        -- check if item is still alive
        if item.duration > 0 then
            local y = floor(item.y + offsety * brickht)
            if item.duration < 8 then
                -- fade out by replacing clip alpha
                ov("target point"..i)
                ov("replace *# *# *# *#-16")
                ov("target")
            end
            -- draw item
            ov("paste "..item.x.." "..y.." point"..i)
            item.duration = item.duration - 1 * framemult
            if item.duration < 0 then
                item.duration = 0
            end
        end
    end
end

--------------------------------------------------------------------------------

local function createbackground()
    -- create background clip
    ov("create "..wd.." "..ht.." "..bgclip)
    ov("target "..bgclip)
    -- create background gradient
    ov("blend 0")
    local y, c
    local level = 96
    for y = 0, ht - 1 do
        c = floor((y / ht) * level)
        ov("rgba 0 "..(level - c).." "..c.." 255")
        ov("line 0 "..y.." "..(wd - 1).." "..y)
    end

    -- add borders if required
    if edgegapl > 0 then
        ov(op.black)
        ov("fill 0 0 "..edgegapl.." "..(ht - 1))
    end
    if edgegapr > 0 then
        ov(op.black)
        ov("fill "..(wd - edgegapr).." 0 "..edgegapr.." "..(ht - 1))
    end
    
    -- reset target
    ov("target")
end

--------------------------------------------------------------------------------

local function drawbackground()
    ov("blend 0")
    ov("paste 0 0 "..bgclip)
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
    ov("blend 1")
    if showshadows == 1 then
        ov(shadowcol)
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
    bonuslevel   = false
    if (level  % bonusinterval) == 0 then
       bonuslevel = true
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
    initpoints()
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

local function togglebrickscore()
    brickscore = 1 - brickscore
    writesettings()
    notify("Brick Score", brickscore)
end

--------------------------------------------------------------------------------

local function savescreenshot()
    local filename = g.getdir("data").."shot"..os.date("%y%m%d%H%M%S", os.time())..".png"
    ov("save 0 0 0 0 "..filename)
    notify("Saved screenshot "..filename)
end

--------------------------------------------------------------------------------

local function toggleconfirmquit()
    confirmquit = 1 - confirmquit
    writesettings()
    notify("Confirm Quit", confirmquit)
end

--------------------------------------------------------------------------------

local function togglecomboscore()
    comboscore = 1 - comboscore
    writesettings()
    notify("Combo Score", comboscore)
end

--------------------------------------------------------------------------------

local function adjustsoundvol(delta)
    soundvol = soundvol + delta
    if soundvol > 100 then
        soundvol = 100
    end
    if soundvol < 0 then
        soundvol = 0
    end
    writesettings()
    notify("Sound Volume "..soundvol.."%")
    setchannelvolume("fx", soundvol)
end

--------------------------------------------------------------------------------

local function adjustmusicvol(delta)
    musicvol = musicvol + delta
    if musicvol > 100 then
        musicvol = 100
    end
    if musicvol < 0 then
        musicvol = 0
    end
    writesettings()
    notify("Music Volume "..musicvol.."%")
    setchannelvolume("music", musicvol)
end

--------------------------------------------------------------------------------

local function processstandardkeys(event)
    if event:find("^key") then
        if event == "key f11 none" then
            -- toggle fullscreen
            togglefullscreen()
        elseif event == "key a none" then
            -- toggle autopause when mouse moves off overlay
            toggleautopause()
        elseif event == "key b none" then
            -- toggle brick score display
            togglebrickscore()
        elseif event == "key c none" then
            -- toggle combo score display
            togglecomboscore()
        elseif event == "key d none" then
            -- toggle shadow display
            toggleshadowdisplay()
        elseif event == "key = none" then
            -- increase sound volume
            adjustsoundvol(10)
        elseif event == "key - none" then
            -- decrease sound volume
            adjustsoundvol(-10)
        elseif event == "key m none" then
            -- toggle mouse cursor display when not fullscreen
            togglemouse()
        elseif event == "key p none" then
            -- toggle particle display
            toggleparticles()
        elseif event == "key q none" then
            -- toggle confirm quit 
            toggleconfirmquit()
        elseif event == "key s none" then
            -- toggle autostart when mouse moves onto overlay
            toggleautostart()
        elseif event == "key t none" then
            -- toggle timing display
            toggletiming()
        elseif event == "key [ none" then
            -- decrease music volume
            adjustmusicvol(-10)
        elseif event == "key ] none" then
            -- increase music volume
            adjustmusicvol(10)
        elseif event == "key tab none" then
            -- show options
            showoptions = not showoptions
        elseif event == "key f12 none" then
            -- save screenshot
            savescreenshot()
        end
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
        -- right click quits game
        if button == "right" then
            if confirmquit == 0 then
                updateballs(0)
                showoptions = false
            else
                confirming = not confirming
            end
        elseif button == "left" or event == "key enter none" or event == "key return none" or event == "key space none" then
            -- left click, enter or space starts game, toggles pause or dismisses settings
            if confirming then
                updateballs(0)
                showoptions = false
                confirming = false
            elseif showoptions then
                showoptions = false
            elseif newball then
                if bonuslevel then
		    playmusic("bonusloop", true)
                else
		    playmusic("gameloop", true)
                end
                newball = false
                pause   = false
            else
                pause = not pause
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
        -- right click quits application
        if button == "right" then
            -- quit application
            again       = false
            finished    = true
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

    wd = newwd
    ht = newht
    fontscale = wd / minwd
    if (ht / minht) < fontscale then
        fontscale = ht / minht
    end

    -- scale bat, ball and bricks
    brickwd        = floor(wd / numcols)
    brickht        = floor(ht / 40)
    brickparticles = floor(brickwd * brickht / 10)
    batwd          = floor(wd / 10)
    batht          = brickht
    ballsize       = wd / 80
    local edgegap  = wd - brickwd * numcols
    edgegapl       = floor(edgegap / 2)
    edgegapr       = edgegap - edgegapl

    -- reposition the bat and ball
    batx  = batx * xscale
    baty  = ht - batht * 4
    ballx = ballx * xscale
    bally = bally * yscale

    -- reposition particles
    for i = 1, #particles do
        local item = particles[i]
        item.x = item.x * xscale
        item.y = item.y * yscale
    end

    -- resize shadow
    initshadow()

    -- recreate background
    createbackground()

    -- recreate static text
    createstatictext()

    -- resize overlay
    ov("resize "..wd.." "..ht)
end

--------------------------------------------------------------------------------

local function drawscoreline()
    ov("blend 1")
    drawtextclip("score", 4, 4, alignleft)
    drawtextclip("balls", -4, 4, alignright)
    drawtextclip("high", 0, 4, aligncenter)
    if combo > 2 then
        drawtextclip("combo", 0, 0, aligncenter, alignbottom)
    end
    if not newball and not pause and not showoptions and bonuslevel and bonuscurrent >= 0 then
        local color = op.green
        if bonuscurrent < 10 then
            color = op.red
        elseif bonuscurrent < 20 then
            color = op.yellow
        end
        updatemessage("time", "Time "..string.format("%.1f", bonuscurrent), color)
        drawtextclip("time", 0, ht / 2, aligncenter)
        color = op.green
        if bricksleft > bonusyellow then
            color = op.red
        elseif bricksleft > bonusgreen then
            color = op.yellow
        end
        updatemessage("remain", "Bricks left "..bricksleft, color)
        drawtextclip("remain", 0, ht / 2 + 25 * fontscale, aligncenter)
    end
end

--------------------------------------------------------------------------------

local function drawgameover()
    ov("blend 1")
    if newhigh then
        local highscorew = drawtextclip("newhigh", 0, ht / 2 + 96 * fontscale, aligncenter)
        createparticles(edgegapl + floor(wd / 2 + highscorew / 2), floor(ht / 2 + 96 * fontscale), highscorew, 1, highparticles)
    end
    updatecombo(gamecombo)
    if newcombo then
        local combow = drawtextclip("newcombo", 0, ht / 2 + 118 * fontscale, aligncenter)
        createparticles(edgegapl + floor(wd / 2 + combow / 2), floor(ht / 2 + 118 * fontscale), combow, 1, comboparticles)
    end
    drawtextclip("gameover", 0, ht / 2 - 30 * fontscale, aligncenter)
    drawtextclip("restart", 0, ht / 2 + 30 * fontscale, aligncenter, nil, true)
    drawtextclip("quit", 0, ht / 2 + 52 * fontscale, aligncenter, nil, true)
    drawtextclip("option", 0, ht / 2 + 74 * fontscale, aligncenter, nil, true)
end

--------------------------------------------------------------------------------

local function drawlevelcomplete()
    ov("blend 1")
    drawtextclip("complete", 0, ht / 2 - 30 * fontscale, aligncenter)
    drawtextclip("continue", 0, ht / 2 + 30 * fontscale, aligncenter, nil, true)
    drawtextclip("quitgame", 0, ht / 2 + 52 * fontscale, aligncenter, nil, true)
    drawtextclip("option", 0, ht / 2 + 74 * fontscale, aligncenter, nil, true)
end

--------------------------------------------------------------------------------

local function drawbonuscomplete(remainingtime, bonusscore)
    ov("blend 1")
    drawtextclip("bcomplete", 0, ht / 2 - 30 * fontscale, aligncenter)

    local w = drawtextclip("awarded", 0, ht / 2, aligncenter)
    if bricksleft <= bonusgreen then
        createparticles(edgegapl + floor(wd / 2 + w / 2), floor(ht / 2), w, 1, bonusparticlesg)
    elseif bricksleft <= bonusyellow then
        createparticles(edgegapl + floor(wd / 2 + w / 2), floor(ht / 2), w, 1, bonusparticlesy)
    end
    drawtextclip("continue", 0, ht / 2 + 30 * fontscale, aligncenter, nil, true)
    drawtextclip("quitgame", 0, ht / 2 + 52 * fontscale, aligncenter, nil, true)
    drawtextclip("option", 0, ht / 2 + 74 * fontscale, aligncenter, nil, true)
    if newbonus then
        local bonusw = drawtextclip("newbonus", 0, ht / 2 + 96 * fontscale, aligncenter)
        createparticles(edgegapl + floor(wd / 2 + bonusw / 2), floor(ht / 2 + 96 * fontscale), bonusw, 1, bonusparticles)
    end
end

--------------------------------------------------------------------------------

local function drawconfirm()
    ov("blend 1")
    drawtextclip("askquit", 0, ht / 2 - 15 * fontscale, aligncenter, nil, true)
    drawtextclip("askleft", 0, ht / 2 + 22 * fontscale, aligncenter, nil, true)
    drawtextclip("askright", 0, ht / 2 + 44 * fontscale, aligncenter, nil, true)
end

--------------------------------------------------------------------------------

local function drawpause()
    ov("blend 1")
    drawtextclip("pause", 0, ht / 2 - 15 * fontscale, aligncenter)
    if offoverlay and autopause ~= 0 and autostart ~= 0 then
        drawtextclip("focus", 0, ht / 2 + 22 * fontscale, aligncenter)
    else
        drawtextclip("resume", 0, ht / 2 + 22 * fontscale, aligncenter, nil, true)
        drawtextclip("quitgame", 0, ht / 2 + 44 * fontscale, aligncenter, nil, true)
        drawtextclip("option", 0, ht / 2 + 66 * fontscale, aligncenter, nil, true)
    end
end

--------------------------------------------------------------------------------

local function drawnewball()
    ov("blend 1")
    drawtextclip("newball", 0, ht / 2 + 22 * fontscale, aligncenter, nil, true)
    drawtextclip("control", 0, ht / 2 + 44 * fontscale, aligncenter, nil, true)
    drawtextclip("quitgame", 0, ht / 2 + 66 * fontscale, aligncenter, nil, true)
    drawtextclip("option", 0,  ht / 2 + 88 * fontscale, aligncenter, nil, true)
    drawtextclip("left", 0, ht / 2 - 15 * fontscale, aligncenter)
    if bonuslevel then
        drawtextclip("bonus", 0, ht / 2 - 52 * fontscale, aligncenter)
    else
        drawtextclip("level", 0, ht / 2 - 52 * fontscale, aligncenter)
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
    local oldblend = ov("blend 1")
    updatemessage("ms", string.format("%.1fms", average))
    drawtextclip("ms", -4, 0, alignright, alignbottom)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

local function drawoption(key, setting, state, leftx, h, y)
    if key ~= "key" then
        ov(op.black)
        ov("fill "..(leftx + edgegapl + shadtxtx).." "..(y + shadtxty).." "..(messages[key].width + 3).." "..(messages[key].height - 4))
        ov(keycol)
        ov("fill "..(leftx + edgegapl).." "..y.." "..(messages[key].width + 3).." "..(messages[key].height - 4))
    end
    drawtextclip(key, leftx, y, alignleft)
    drawtextclip(setting, 0, y, aligncenter)
    drawtextclip(state, -leftx, y, alignright)
    return y + h
end

--------------------------------------------------------------------------------

local function drawpercent(downkey, upkey, setting, valname, leftx, h, y)
    local width  = messages[downkey].width
    local height = messages[downkey].height
    ov(op.black)
    ov("fill "..(leftx + edgegapl + shadtxtx).." "..(y + shadtxty).." "..(width + 3).." "..(height - 4))
    ov("fill "..(leftx + width * 2 + edgegapl + shadtxtx).." "..(y + shadtxty).." "..(width + 3).." "..(height - 4))
    ov(keycol)
    ov("fill "..(leftx + edgegapl).." "..y.." "..(width + 3).." "..(height - 4))
    ov("fill "..(leftx + width * 2 + edgegapl).." "..y.." "..(width + 3).." "..(height - 4))
    drawtextclip(downkey, leftx, y, alignleft)
    drawtextclip(upkey, leftx + width * 2, y, alignleft)
    drawtextclip(setting, 0, y, aligncenter)
    drawtextclip(valname, -leftx, y, alignright)
    return y + h
end

--------------------------------------------------------------------------------

local function drawoptions()
    local leftx = floor(wd / 6)
    local state = {[0] = "off", [1] = "on"}

    -- draw header
    ov("blend 1")
    local h = messages["key"].height
    local y = floor((ht - 14 * h) / 2)
    y = drawoption("key", "function", "state", leftx, h, y)

    -- draw options
    y = drawoption("a", "autopause", state[autopause], leftx, h, y)
    y = drawoption("b", "brickscore", state[brickscore], leftx, h, y)
    y = drawoption("c", "comboscore", state[comboscore], leftx, h, y)
    y = drawoption("d", "shadows", state[showshadows], leftx, h, y)
    y = drawoption("m", "mouse", state[showmouse], leftx, h, y)
    y = drawoption("p", "particles", state[showparticles], leftx, h, y)
    y = drawoption("q", "confirm", state[confirmquit], leftx, h, y)
    y = drawoption("s", "autostart", state[autostart], leftx, h, y)
    y = drawoption("t", "timing", state[showtiming], leftx, h, y)
    y = drawoption("f11", "fullscreen", state[fullscreen], leftx, h, y)
    y = drawpercent("-", "=", "sound", "fxvol", leftx, h, y)
    y = drawpercent("[", "]", "music", "musicvol", leftx, h, y)

    -- draw close options
    drawtextclip("close", 0, y + h, aligncenter, nil, true)
    if balls == 0 then
        drawtextclip("quit", 0, y + h * 2.5, aligncenter, nil, true)
    else
        drawtextclip("quitgame", 0, y + h * 2.5, aligncenter, nil, true)
    end
end

--------------------------------------------------------------------------------

local function checkforsystemevent()
    -- check for resize
    local newwd, newht = g.getview(g.getlayer())
    if newwd ~= wd or newht ~= ht then
        resizegame(newwd, newht)
    end
    -- check for overlay hidden
    if not pause then
        if g.getoption("showoverlay") == 0 then
            pause = true
        end
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
        if autopause ~= 0 then
            pause = true
        end
    end
end

--------------------------------------------------------------------------------

local function clearbonusbricks()
    local bricks = {}
    local clearparticles = brickparticles / 4
    for y = 1, numrows do
        bricks = rows[y]
        for x = 1, numcols do
            if bricks[x] then
                bricks[x] = false
                createparticles(x * brickwd + edgegapl, (y + offsety) * brickht, brickwd, brickht, clearparticles, brickcols[y])
            end
        end
    end
end

--------------------------------------------------------------------------------

local function computebonus()
    local bonusscore = 0
    if bricksleft <= bonusgreen then
        bonusscore = (totalbricks - bricksleft) * (100 + (level - 1) * 10)
        updatemessage("awarded", "Bricks left "..bricksleft.." = "..bonusscore, op.green)
    elseif bricksleft <= bonusyellow then
        bonusscore = (totalbricks - bricksleft) * (50 + (level - 1) * 10)
        updatemessage("awarded", "Bricks left "..bricksleft.." = "..bonusscore, op.yellow)
    else
        updatemessage("awarded", "Bricks left "..bricksleft.." = ".."No Bonus", op.red)
    end
    playmusic("levelcompleteloop", true)
    updatescore(score + bonusscore)
    if score > hiscore then
        newhigh = true
        updatehighscore(score)
    end
    if bonusscore > bestbonus then
        newbonus = true
        bestbonus = bonusscore
    end

    return bonusscore
end

--------------------------------------------------------------------------------

local function resetcombo()
    updatecombo(1)
    combomult  = 1
    comboraw   = 0
    comboextra = 0
end

--------------------------------------------------------------------------------

local function playexit()
    local box = {}
    local n = 1
    local tx, ty
    local tilesize = math.floor(wd / 32)
    ov("blend 0")

    -- copy the screen into tiles
    for y = 0, ht, tilesize do
        for x = 0, wd, tilesize do
            tx = x + rand(0, floor(wd / 8)) - wd / 16
            ty = ht + rand(0, floor(ht / 2))
            local entry = {}
            entry[1] = x
            entry[2] = y
            entry[3] = tx
            entry[4] = ty
            box[n] = entry
            ov("copy "..x.." "..y.." "..tilesize.." "..tilesize.." sprite"..n)
            n = n + 1
        end
    end
    ov(op.black)
    for i = 0, 100 do
        t = g.millisecs()
        local a = i / 100
        local x, y
        ov("fill")
        -- update each tile
        for n = 1, #box do
            x = box[n][1]
            y = box[n][2]
            tx = box[n][3]
            ty = box[n][4]
            ov("paste "..floor(x * (1 - a) + tx * a).." "..floor(y * (1 - a) + ty * a).." sprite"..n)
        end
        -- draw timing if on
        if showtiming == 1 then
            drawtiming(g.millisecs() - t)
        end
        ov("update")
        while g.millisecs() - t < 15 do end
    end
    n = 1
    -- delete tiles
    for y = 0, ht, tilesize do
        for x = 0, wd, tilesize do
            ov("delete sprite"..n)
            n = n + 1
        end
    end
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

    -- set sound and music volume
    setchannelvolume("fx", soundvol)
    setchannelvolume("music", musicvol)

    -- play games until finished
    balls    = 3
    score    = 0
    level    = 1
    again    = true
    newhigh  = false
    newcombo = false
    newbonus = false

    -- initialise the bat and ball
    initbat()
    initball()

    -- welcome message
    notify("Golly Breakout build "..build)

    -- create static text
    createstatictext()

    -- initialize dynamic text
    updatescore(score)
    updatehighscore(hiscore)
    updateballs(balls)
    updatelevel(level)

    -- main loop
    while again do
        -- initialize the bricks
        initbricks()

        -- create the background
        createbackground()

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

        -- initialise points
        initpoints()

        -- whether alive
        newball    = true
        pause      = false
        confirming = false

        -- whether mouse off overlay
        offoverlay = false

        -- reset combo
        resetcombo()

        -- game loop
        playmusic("gamestart")
        while balls > 0 and bricksleft > 0 and bonuscurrent > 0 do
            -- time frame
            local frametime = g.millisecs()

            -- check for mouse click or key press
            processinput()

            -- check if size of overlay has changed or overlay is hidden
            checkforsystemevent()

            -- draw the background
            drawbackground()

            -- check if paused
            if not pause and not confirming and not showoptions and bonuscurrent > 0 then
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
                            playsound("edge")
                        end

                        -- check for ball hitting top boundary
                        if bally < ballsize / 2 then
                            createparticles(ballx, floor(bally - ballsize / 2), 1, 1, wallparticles)
                            -- ball hit top so speed up a little bit
                            balldy    = -balldy
                            bally     = bally - stepy
                            ballspeed = ballspeed + speedinc / 2
                            if ballspeed > maxspeed then
                                ballspeed = maxspeed
                            end
                            playsound("top")

                        -- check for ball hitting bottom boundary
                        elseif bally >= ht then
                            -- check for bonus level
                            if bonuslevel then
                                -- end bonus level
                                bonuscurrent = 0
                            else
                                -- ball lost!
                                updateballs(balls - 1)
                                balldy    = -1
                                balldx    = 0.5
                                ballspeed = speeddef
                                newball   = true
                                -- reset combo
                                if comboextra - comboraw > 0 then
                                    if comboscore == 1 then
                                        notify("Combo x "..(combo - 1).." Score "..comboextra - comboraw.." (+"..(floor(100 * comboextra / comboraw) - 100).."%)")
                                    end
                                end
                                resetcombo()
                                -- destroy bat if no balls left
                                if balls == 0 then
                                    createparticles(batx + batwd, baty, batwd, batht, lostparticles)
                                    playmusic("gameover")
                                else
                                    playmusic("lostball")
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
                            if comboextra - comboraw > 0 then
                                if comboscore == 1 then
                                    notify("Combo x "..(combo - 1).." Score "..comboextra - comboraw.." (+"..(floor(100 * comboextra / comboraw) - 100).."%)")
                                end
                            end
                            resetcombo()
                            playsound("bat")
                        end

                        -- check for ball hitting brick
                        bricky = floor(bally / brickht) - offsety
                        if bricky >= 1 and bricky <= numrows then
                            brickx = floor((ballx - edgegapl) / brickwd) + 1
                            if rows[bricky][brickx] then
                                -- hit a brick!
                                rows[bricky][brickx] = false
                                -- adjust score
                                local points = floor((level + 9) * (numrows - bricky + 1) * combomult)
                                local rawpoints = floor((level + 9) * (numrows - bricky + 1))
                                if combo > 1 then
                                    comboraw = comboraw + rawpoints
                                    comboextra = comboextra + points
                                end
                                updatescore(score + points)
                                if score > hiscore then
                                    newhigh = true
                                    updatehighscore(score)
                                end
                                createpoints((brickx - 1) * brickwd + edgegapl, bricky * brickht, points)
                                -- increment combo
                                combomult = combomult * combofact
                                if combo + 1 > maxcombo then
                                    maxcombo = combo + 1
                                    newcombo = true
                                end
                                updatecombo(combo + 1)
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
                                createparticles(brickx * brickwd + edgegapl, (bricky + offsety) * brickht, brickwd, brickht, brickparticles, brickcols[bricky])
                                -- one less brick
                                bricksleft = bricksleft - 1
                                playsound("brick"..bricky)
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

            -- draw the points
            if brickscore == 1 then
                drawpoints()
            end

            -- draw the ball
            if balls > 0 then
                drawball()
            end

            -- draw the bat
            drawbat()

            -- draw the score, high score and lives
            drawscoreline()

            -- check for text overlay
            if confirming then
                drawconfirm()
            elseif showoptions then
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
            if finaltime > sixtyhz then
                -- cap to maximum frame time in case external event took control for a long time
                if finaltime > framecap then
                    finaltime = framecap
                end
                framemult = finaltime / sixtyhz
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
        else
            if bricksleft == 0 then
                playmusic("levelcompleteloop", true)
            end
        end

        -- save high score, max combo and best bonus
        if newhigh or newcombo or newbonus then
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

            -- check if size of overlay has changed or overlay hidden
            checkforsystemevent()

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
            if finaltime > sixtyhz then
                -- cap to maximum frame time in case external event took control for a long time
                if finaltime > framecap then
                    finaltime = framecap
                end
                framemult = finaltime / sixtyhz
            end
        end

        -- check why game finished
        if balls == 0 then
            -- reset
            updatescore(0)
            updateballs(3)
            updatelevel(1)
            newhigh   = false
            updatehighscore(hiscore)
            newcombo  = false
            gamecombo = 1
        else
            -- level complete
            updatelevel(level + 1)
        end
        newbonus = false
    end

    -- exit animation
    playexit()

    -- free clips and restore settings
    ov("delete "..bgclip)
    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov("textoption background "..oldbg)
    ov("cursor "..oldcursor)
end

--------------------------------------------------------------------------------

function main()
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

local status, err = xpcall(main, gp.trace)
if err then g.continue(err) end
-- the following code is always executed

g.check(false)
stopallsound()
ov("delete")
g.setoption("showoverlay", oldoverlay)
g.setoption("showbuttons", oldbuttons)
g.setoption("fullscreen", oldfs)
