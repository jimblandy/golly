-- Breakout for Golly
-- Author: Chris Rowett (crowett@gmail.com), November 2016
-- Use F12 to save a screenshot

local build = 71
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
local text = {
    alignleft   = 0,
    aligncenter = 1,
    alignright  = 2,
    aligntop    = 3,
    alignbottom = 4,
    fontscale   = 1
}

-- overlay width and height
local wd, ht   = g.getview(g.getlayer())
local minwd    = 400
local minht    = 400
local edgegapl = 0
local edgegapr = 0

-- background settings
local bgclip = "bg"

-- shadow settings
local shadow = {
    x     = floor(-wd / 100),
    y     = floor(ht / 100),
    col   = "rgba 0 0 0 128",
    txtx  = -2,
    txty  = 2
}

-- brick settings
local brick = {
    numrows     = 6,
    numcols     = 20,
    rows        = {},
    wd,
    ht          = floor(ht / 40),
    maxoffsety  = 20,
    offsety     = 0,
    startoffset = 0,
    movedown    = 0,
    movesteps   = 24,
    cols  = {
        [1] = op.red,
        [2] = op.yellow,
        [3] = op.magenta,
        [4] = op.green,
        [5] = op.cyan,
        [6] = op.blue
    },
    bricksleft,
    totalbricks,
    x,
    y
}
brick.wd = floor(wd / brick.numcols)
brick.bricksleft = brick.numrows * brick.numcols
brick.totalbricks = bricksleft

-- bat settings
local bat = {
    x,
    y,
    wd,
    ht,
    lastx
}

-- ball settings
local ball = {
    size      = wd / 80,
    x         = 0,
    y         = 0,
    numsteps  = 80
}

-- particle settings
local particle = {
    particles       = {},
    brickparticles  = floor(brick.wd * brick.ht / 10),
    ballparticles   = 1,
    ballpartchance  = 0.25,
    wallparticles   = 20,
    batparticles    = 20,
    highparticles   = 4,
    comboparticles  = 4,
    bonusparticles  = 4,
    lostparticles   = 1024,
    bonusparticlesg = 6,
    bonusparticlesy = 3
}

-- points settings
local points = {}

-- game settings
local game = {
    level      = 1,
    newball    = true,
    pause      = false,
    hiscore    = 0,
    score      = 0,
    combo      = 1,
    combomult  = 1,
    combofact  = 1.04,
    comboraw   = 0,
    comboextra = 0,
    gamecombo  = 1,
    maxcombo   = 2,
    balls      = 3,
    newhigh    = false,
    newcombo   = false,
    newbonus   = false,
    offoverlay = false,
    finished   = false,
    again      = true
}

-- timing settings
local timing = {
    times         = {},
    timenum       = 1,
    numtimes      = 8,
    framemult     = 1,
    framecap      = 100,
    sixtyhz       = 16.7
}

-- game options
local options = {
    brickscore    = 1,
    showtiming    = 0,
    showparticles = 1,
    autopause     = 0,
    autostart     = 0,
    showmouse     = 1,
    showshadows   = 1,
    confirmquit   = 0,
    showoptions   = false,
    confirming    = false,
    comboscore    = 1,
    soundvol      = 1,
    musicvol      = 1,
    fullscreen    = 0
}

-- settings are saved in this file
local settingsfile = g.getdir("data").."breakout.ini"

-- notifications
local notification = {
    duration = 300,
    trans    = 20,
    current  = 0,
    message  = ""
}

-- bonus level
local bonus = {
    bricks = {
        805203,
        699732,
        830290,
        698705,
        698705,
        805158
    },
    level    = false,
    interval = 3,
    time     = 60,
    current  = 0,
    green    = 10,
    yellow   = 20,
    best     = 0
}

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
    ["manfocus"]   = { text = "Move mouse onto overlay and", size = 10, color = op.white },
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

-- music
local music = {
    currenttrack = "",
    fade         = 1,
    faderate     = -0.01,
    gameovertime = 1000 * 64
}

--------------------------------------------------------------------------------

local function showcursor()
    if options.showmouse == 0 then
        ov("cursor hidden")
    else
        ov("cursor arrow")
    end
end

--------------------------------------------------------------------------------

local function setfullscreen()
    g.setoption("fullscreen", options.fullscreen)
    showcursor()
end

--------------------------------------------------------------------------------

local function readsettings()
    local f = io.open(settingsfile, "r")
    if f then
        game.hiscore          = tonumber(f:read("*l")) or 0
        options.fullscreen    = tonumber(f:read("*l")) or 0
        options.showtiming    = tonumber(f:read("*l")) or 0
        options.showparticles = tonumber(f:read("*l")) or 1
        options.autopause     = tonumber(f:read("*l")) or 0
        options.autostart     = tonumber(f:read("*l")) or 0
        options.showmouse     = tonumber(f:read("*l")) or 1
        options.showshadows   = tonumber(f:read("*l")) or 1
        game.maxcombo         = tonumber(f:read("*l")) or 2
        options.brickscore    = tonumber(f:read("*l")) or 1
        options.confirmquit   = tonumber(f:read("*l")) or 1
        bonus.best            = tonumber(f:read("*l")) or 0
        options.comboscore    = tonumber(f:read("*l")) or 1
        options.soundvol      = tonumber(f:read("*l")) or 100
        options.musicvol      = tonumber(f:read("*l")) or 70
        f:close()

        if options.soundvol == 1 then
           options.soundvol = 100
        end
        if options.musicvol == 1 then
           options.musicvol = 100
        end
    end
end

--------------------------------------------------------------------------------

local function writesettings()
    local f = io.open(settingsfile, "w")
    if f then
        f:write(tostring(game.hiscore).."\n")
        f:write(tostring(options.fullscreen).."\n")
        f:write(tostring(options.showtiming).."\n")
        f:write(tostring(options.showparticles).."\n")
        f:write(tostring(options.autopause).."\n")
        f:write(tostring(options.autostart).."\n")
        f:write(tostring(options.showmouse).."\n")
        f:write(tostring(options.showshadows).."\n")
        f:write(tostring(game.maxcombo).."\n")
        f:write(tostring(options.brickscore).."\n")
        f:write(tostring(options.confirmquit).."\n")
        f:write(tostring(bonus.best).."\n")
        f:write(tostring(options.comboscore).."\n")
        f:write(tostring(options.soundvol).."\n")
        f:write(tostring(options.musicvol).."\n")
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
    ov("font "..floor(message.size * text.fontscale))
    -- create the text message clips
    message.text = s
    local w, h = op.maketext(message.text, name, message.color, shadow.txtx, shadow.txty)
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

local function soundstate(sound)
    return ov("sound state oplus/sounds/breakout/"..sound..".ogg")
end

--------------------------------------------------------------------------------

local function setvolume(sound, vol)
    ov("sound volume oplus/sounds/breakout/"..sound..".ogg "..vol)
end

--------------------------------------------------------------------------------

local function setchannelvolume(channel, vol)
    if vol == 0 then
        updatemessage(channel.."vol", "Off", op.red)
    else
        updatemessage(channel.."vol", vol.."%", op.green)
    end
    -- update the music volume immediately since it may be playing
    if channel == "music" then
        if music.currenttrack ~= "" then
            setvolume(music.currenttrack, (options.musicvol * music.fade / 100))
        end
    end
end

--------------------------------------------------------------------------------

local function stopallsound()
    ov("sound stop")
end

--------------------------------------------------------------------------------

local function stopmusic()
    if music.currenttrack ~= "" then
        ov("sound stop oplus/sounds/breakout/"..music.currenttrack..".ogg")
    end
end
--------------------------------------------------------------------------------

local function playsound(name, loop)
    if options.soundvol > 0 then
        loop = loop or false
        if loop then
            ov("sound loop oplus/sounds/breakout/"..name..".ogg "..(options.soundvol / 100))
        else
            ov("sound play oplus/sounds/breakout/"..name..".ogg "..(options.soundvol / 100))
        end
    end
end

--------------------------------------------------------------------------------

local function playmusic(name, loop)
    loop = loop or false
    stopmusic()
    music.currenttrack = name
    if loop then
        ov("sound loop oplus/sounds/breakout/"..name..".ogg "..(options.musicvol / 100))
    else
        ov("sound play oplus/sounds/breakout/"..name..".ogg "..(options.musicvol / 100))
    end
end

--------------------------------------------------------------------------------

local function updatelevel(value)
    game.level = value
    updatemessage("level", "Level "..game.level)
    updatemessage("complete", "Level "..game.level.." complete!")
end

--------------------------------------------------------------------------------

local function updatescore(value)
    game.score = value
    updatemessage("score", "Score "..game.score)
end

--------------------------------------------------------------------------------

local function updatehighscore(value)
    local color = op.white
    if game.newhigh then
        color = op.green
    end
    game.hiscore = value
    updatemessage("high", "High Score "..game.hiscore, color)
end

--------------------------------------------------------------------------------

local function updateballs(value)
    game.balls = value
    local color = op.white
    if game.balls == 1 then
        updatemessage("left", "Last ball!", op.red)
        color = op.red
    elseif game.balls == 2 then
        color = op.yellow
        updatemessage("left", game.balls.." balls left", op.yellow)
    else
        updatemessage("left", game.balls.." balls left", op.green)
    end
    updatemessage("balls", "Balls "..game.balls, color)
end

--------------------------------------------------------------------------------

local function updatecombo(value)
    local color = op.white
    game.combo = value
    if game.combo == game.maxcombo then
        color = op.green
    end
    updatemessage("combo", "Combo x"..game.combo - 1, color)
end

--------------------------------------------------------------------------------

local function highlightkey(text, x, y, w, h, token, color)
    local t1, t2 = text:find(token)
    if t1 ~= nil then
        local charw = w / text:len()
        local x1 = x + (t1 - 1) * charw
        local oldblend = ov("blend 0")
        local oldrgba = ov(op.black)
        ov("fill "..floor(x1 + shadow.txtx).." "..floor(y + shadow.txty).." "..floor(charw * (t2 - t1 + 1) + 5).." "..floor(h - 4))
        ov(color)
        ov("fill "..floor(x1).." "..floor(y).." "..floor(charw * (t2 - t1 + 1) + 5).." "..floor(h - 4))
        ov("rgba "..oldrgba)
        ov("blend "..oldblend)
    end
end

--------------------------------------------------------------------------------

local function drawtextclip(name, x, y, xalign, yalign, highlight)
    xalign    = xalign or text.alignleft
    yalign    = yalign or text.aligntop
    highlight = highlight or false
    -- lookup the message
    local message = messages[name]
    local w = message.width
    local h = message.height
    local xoffset = 0
    local yoffset = 0
    if xalign == text.aligncenter then
        xoffset = (wd - w) / 2
    elseif xalign == text.alignright then
        xoffset = wd - w - edgegapr - edgegapl
    end
    if yalign == text.aligncenter then
        yoffset = (ht - h) / 2
    elseif yalign == text.alignbottom then
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
    if notification.message ~= "" then
        local y = 0
        -- check if notification finished
        if notification.current >= notification.duration then
            notification.message = ""
            notification.current = 0
        else
            -- check which phase
            if notification.current < notification.trans then
                -- appear
                y = (notification.current / notification.trans) * (8 * text.fontscale)
            elseif notification.current > notification.duration - notification.trans then
                -- disappear
                y = (notification.duration - notification.current) / notification.trans * (8 * text.fontscale)
            else
                -- hold
                y = (8 * text.fontscale)
            end
            -- draw notification
            drawtextclip("notify", 4, (8 * text.fontscale) - y, text.alignleft, text.alignbottom)
            notification.current = notification.current + timing.framemult
        end
    end
end

--------------------------------------------------------------------------------

local function notify(message, flag)
    flag = flag or -1
    notification.message = message
    if flag == 0 then
        notification.message = message.." off"
    elseif flag == 1 then
        notification.message = message.." on"
    end
    -- create the text clip
    updatemessage("notify", notification.message)
    if notification.current ~= 0 then
        notification.current = notification.trans
    end
end

--------------------------------------------------------------------------------

local function initparticles()
    particle.particles = {}
end

--------------------------------------------------------------------------------

local function createparticles(x, y, areawd, areaht, howmany, color)
    color = color or "rgba 255 255 255 255"
    -- find the first free slot
    local i = 1
    while i <= #particle.particles and particle.particles[i].alpha > 0 do
        i = i + 1
    end
    for j = 1, howmany do
        local item = { alpha = 255, x = x - rand(floor(areawd)), y = y + rand(floor(areaht)), dx = rand() - 0.5, dy = rand() - 0.5, color = color }
        particle.particles[i] = item
        i = i + 1
        -- find the next free slot
        while i <= #particle.particles and particle.particles[i].alpha > 0 do
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
    for i = 1, #particle.particles do
        local item  = particle.particles[i]
        local scale = ht / 1000
        -- check if particle is still alive
        if item.alpha > 0 then
            if options.showparticles ~= 0 then
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
            item.alpha = item.alpha - 4 * timing.framemult
            if item.alpha < 0 then
                item.alpha = 0
            end
            item.x = item.x + item.dx * timing.framemult * scale
            item.y = item.y + item.dy * timing.framemult * scale
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
    ov("font "..floor(7 * text.fontscale).." mono")
    local w, h = op.maketext(value, "point"..i, op.white, shadow.txtx, shadow.txty)

    -- save the item
    local item = { duration = 60, x = floor(x + brick.wd / 2 - w / 2), y = floor(y + brick.ht / 2 - h / 2) }
    points[i] = item
end

--------------------------------------------------------------------------------

local function drawpoints()
    ov("blend 1")
    for i = 1, #points do
        local item = points[i]
        -- check if item is still alive
        if item.duration > 0 then
            local y = floor(item.y + brick.offsety * brick.ht)
            if item.duration < 8 then
                -- fade out by replacing clip alpha
                ov("target point"..i)
                ov("replace *# *# *# *#-16")
                ov("target")
            end
            -- draw item
            ov("paste "..item.x.." "..y.." point"..i)
            item.duration = item.duration - 1 * timing.framemult
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
    local xoff = shadow.x
    local yoff = shadow.y
    local startpass = 1
    -- check whether to draw shadows
    if options.showshadows == 0 then
        xoff = 0
        yoff = 0
        startpass = 2
        ov("blend 0")
    else
        ov("blend 1")
        ov(shadow.col)
    end
    for pass = startpass, 2 do
        for y = 1, brick.numrows do
            local bricks = rows[y]
            brick.y = floor((y + brick.offsety) * brick.ht)
            if pass == 2 then
                ov(brick.cols[y])
            end
            for x = 1, brick.numcols do
                if bricks[x] then
                    brick.x = (x - 1) * brick.wd
                    ov("fill "..(brick.x + xoff + edgegapl).." "..(brick.y + yoff).." "..(brick.wd - 1).." "..(brick.ht - 1))
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
    local oldwidth = ov("lineoption width "..floor(ball.size / 2))
    ov("blend 1")
    if options.showshadows == 1 then
        ov(shadow.col)
        ov("ellipse "..(floor(ball.x - ball.size / 2) + shadow.x).." "..(floor(ball.y - ball.size / 2) + shadow.y).." "..floor(ball.size).." "..floor(ball.size))
    end
    ov(op.white)
    ov("ellipse "..floor(ball.x - ball.size / 2).." "..floor(ball.y - ball.size / 2).." "..floor(ball.size).." "..floor(ball.size))
    if rand() < particle.ballpartchance * timing.framemult then
        createparticles(ball.x + ball.size / 2, ball.y - ball.size / 2, ball.size, ball.size, particle.ballparticles)
    end
    ov("lineoption width "..oldwidth)
end

--------------------------------------------------------------------------------

local function drawbat()
    if options.showshadows == 1 then
        ov(shadow.col)
        ov("blend 1")
        ov("fill "..(floor(bat.x) + shadow.x).." "..(floor(bat.y) + shadow.y).." "..bat.wd.." "..bat.ht)
    end
    ov("blend 0")
    -- draw the bat in red if mouse is off the overlay
    if game.offoverlay then
        ov(op.red)
    else
        ov("rgba 192 192 192 255")
    end
    ov("fill "..floor(bat.x).." "..floor(bat.y).." "..bat.wd.." "..bat.ht)
end

--------------------------------------------------------------------------------

local function initbricks()
    rows              = {}
    brick.wd          = floor(wd / brick.numcols)
    brick.ht          = floor(ht / 40)
    brick.bricksleft  = 0
    brick.totalbricks = 0
    brick.offsety     = game.level + 1
    if brick.offsety > brick.maxoffsety then
        brick.offsety = brick.maxoffsety
    end
    brick.movedown    = 0

    -- check for bonus level
    bonus.current = bonus.time
    bonus.level   = false
    if (game.level % bonus.interval) == 0 then
       bonus.level = true
    end

    -- distribute any gap left and right
    local edgegap = wd - brick.wd * brick.numcols
    edgegapl = floor(edgegap / 2)
    edgegapr = edgegap - edgegapl

    -- set the required bricks alive
    local match = 1
    for y = 1, brick.numrows do
        local bricks = {}
        if bonus.level then
            local bonusrow = bonus.bricks[y]
            match = 1
            for x = brick.numcols - 1, 0, -1 do
                if (bonusrow & match) == match then
                    bricks[x + 1] = true
                    brick.bricksleft = brick.bricksleft + 1
                else
                    bricks[x + 1] = false
                end
                match = match + match
            end
        else
            for x = 1, brick.numcols do
                bricks[x] = true
                brick.bricksleft = brick.bricksleft + 1
            end
        end
        rows[y] = bricks
    end
    brick.totalbricks = brick.bricksleft
end

--------------------------------------------------------------------------------

local function initbat()
    bat.wd = floor(wd / 10)
    bat.x  = floor((wd - bat.wd) / 2)
    bat.ht = brick.ht
    bat.y  = ht - bat.ht * 4
end

--------------------------------------------------------------------------------

local function initball()
    ball.size = wd / 80
    ball.x    = (wd - ball.size) / 2
    ball.y    = bat.y - ball.size
end

--------------------------------------------------------------------------------

local function initshadow()
    shadow.x   = floor(-wd / 100)
    shadow.y   = floor(ht / 100)
    shadow.col = "rgba 0 0 0 128"
end

--------------------------------------------------------------------------------

local function togglefullscreen()
    options.fullscreen = 1 - options.fullscreen
    writesettings()
    setfullscreen()
    initpoints()
end

--------------------------------------------------------------------------------

local function toggletiming()
    options.showtiming = 1 - options.showtiming
    writesettings()
    notify("Timing", options.showtiming)
end

--------------------------------------------------------------------------------

local function toggleparticles()
    options.showparticles = 1 - options.showparticles
    writesettings()
    notify("Particles", options.showparticles)
end

--------------------------------------------------------------------------------

local function toggleautopause()
    options.autopause = 1 - options.autopause
    writesettings()
    notify("Autopause", options.autopause)
end

--------------------------------------------------------------------------------

local function toggleautostart()
    options.autostart = 1 - options.autostart
    writesettings()
    notify("Autostart", options.autostart)
end

--------------------------------------------------------------------------------

local function togglemouse()
    options.showmouse = 1 - options.showmouse
    writesettings()
    showcursor()
    notify("Mouse pointer", options.showmouse)
end

--------------------------------------------------------------------------------

local function toggleshadowdisplay()
    options.showshadows = 1 - options.showshadows
    writesettings()
    notify("Shadows", options.showshadows)
end

--------------------------------------------------------------------------------

local function togglebrickscore()
    options.brickscore = 1 - options.brickscore
    writesettings()
    notify("Brick Score", options.brickscore)
end

--------------------------------------------------------------------------------

local function savescreenshot()
    local filename = g.getdir("data").."shot"..os.date("%y%m%d%H%M%S", os.time())..".png"
    ov("save 0 0 0 0 "..filename)
    notify("Saved screenshot "..filename)
end

--------------------------------------------------------------------------------

local function toggleconfirmquit()
    options.confirmquit = 1 - options.confirmquit
    writesettings()
    notify("Confirm Quit", options.confirmquit)
end

--------------------------------------------------------------------------------

local function togglecomboscore()
    options.comboscore = 1 - options.comboscore
    writesettings()
    notify("Combo Score", options.comboscore)
end

--------------------------------------------------------------------------------

local function adjustsoundvol(delta)
    options.soundvol = options.soundvol + delta
    if options.soundvol > 100 then
        options.soundvol = 100
    end
    if options.soundvol < 0 then
        options.soundvol = 0
    end
    writesettings()
    notify("Sound Volume "..options.soundvol.."%")
    setchannelvolume("fx", options.soundvol)
end

--------------------------------------------------------------------------------

local function adjustmusicvol(delta)
    options.musicvol = options.musicvol + delta
    if options.musicvol > 100 then
        options.musicvol = 100
    end
    if options.musicvol < 0 then
        options.musicvol = 0
    end
    writesettings()
    notify("Music Volume "..options.musicvol.."%")
    setchannelvolume("music", options.musicvol)
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
            options.showoptions = not options.showoptions
        elseif event == "key f12 none" then
            -- save screenshot
            savescreenshot()
        end
    end
end

--------------------------------------------------------------------------------

local function pausegame(paused)
    if paused ~= game.pause then
        game.pause = paused
        if music.currenttrack ~= "" then
            if game.pause then
                music.fade = 1
                music.faderate  = -0.05
            else
                music.faderate = 0.05
                ov("sound resume")
            end
        end
    end
end

--------------------------------------------------------------------------------

local function updatemusic()
    if music.currenttrack ~= "" then
        if game.pause then
            if music.fade > 0 then
                music.fade = music.fade + music.faderate
                if music.fade <= 0 then
                    music.fade = 0
                    ov("sound pause")
                else
                    setvolume(music.currenttrack, (options.musicvol * music.fade / 100))
                end
            end
        else
            if music.fade < 1 then
                music.fade = music.fade + music.faderate
                if music.fade > 1 then
                    music.fade = 1
                end
                setvolume(music.currenttrack, (options.musicvol * music.fade / 100))
            end
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
            if options.confirmquit == 0 then
                updateballs(0)
                options.showoptions = false
            else
                options.confirming = not options.confirming
            end
        elseif button == "left" or event == "key enter none" or event == "key return none" or event == "key space none" then
            -- left click, enter or space starts game, toggles pause or dismisses settings
            if options.confirming then
                updateballs(0)
                options.showoptions = false
                options.confirming = false
            elseif options.showoptions then
                options.showoptions = false
            elseif game.newball then
                if bonus.level then
                    playmusic("bonusloop", true)
                else
                    playmusic("gameloop", true)
                end
                game.newball = false
                pausegame(false)
            else
                -- do not unpause if off overlay
                if not (game.pause and game.offoverlay and options.autopause ~= 0) then
                    pausegame(not game.pause)
                end
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
            game.again          = false
            game.finished       = true
            options.showoptions = false
        elseif button == "left" or event == "key enter none" or event == "key return none" or event == "key space none" then
            -- left click, enter or space restarts game or dismisses settings
            if options.showoptions then
                options.showoptions = false
            else
                game.finished = true
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
    text.fontscale = wd / minwd
    if (ht / minht) < text.fontscale then
        text.fontscale = ht / minht
    end

    -- scale bat, ball and bricks
    brick.wd                = floor(wd / brick.numcols)
    brick.ht                = floor(ht / 40)
    particle.brickparticles = floor(brick.wd * brick.ht / 10)
    bat.wd                  = floor(wd / 10)
    bat.ht                  = brick.ht
    ball.size               = wd / 80
    local edgegap           = wd - brick.wd * brick.numcols
    edgegapl                = floor(edgegap / 2)
    edgegapr                = edgegap - edgegapl

    -- reposition the bat and ball
    bat.x  = bat.x * xscale
    bat.y  = ht - bat.ht * 4
    ball.x = ball.x * xscale
    ball.y = ball.y * yscale

    -- reposition particles
    for i = 1, #particle.particles do
        local item = particle.particles[i]
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
    drawtextclip("score", 4, 4, text.alignleft)
    drawtextclip("balls", -4, 4, text.alignright)
    drawtextclip("high", 0, 4, text.aligncenter)
    if game.combo > 2 then
        drawtextclip("combo", 0, 0, text.aligncenter, text.alignbottom)
    end
    if not game.newball and not game.pause and not options.showoptions and bonus.level and bonus.current >= 0 then
        local color = op.green
        if bonus.current < 10 then
            color = op.red
        elseif bonus.current < 20 then
            color = op.yellow
        end
        updatemessage("time", "Time "..string.format("%.1f", bonus.current), color)
        drawtextclip("time", 0, ht / 2, text.aligncenter)
        color = op.green
        if brick.bricksleft > bonus.yellow then
            color = op.red
        elseif brick.bricksleft > bonus.green then
            color = op.yellow
        end
        updatemessage("remain", "Bricks left "..brick.bricksleft, color)
        drawtextclip("remain", 0, ht / 2 + 25 * text.fontscale, text.aligncenter)
    end
end

--------------------------------------------------------------------------------

local function drawgameover()
    ov("blend 1")
    if game.newhigh then
        local highscorew = drawtextclip("newhigh", 0, ht / 2 + 96 * text.fontscale, text.aligncenter)
        createparticles(edgegapl + floor(wd / 2 + highscorew / 2), floor(ht / 2 + 96 * text.fontscale), highscorew, 1, particle.highparticles)
    end
    updatecombo(game.gamecombo)
    if game.newcombo then
        local combow = drawtextclip("newcombo", 0, ht / 2 + 118 * text.fontscale, text.aligncenter)
        createparticles(edgegapl + floor(wd / 2 + combow / 2), floor(ht / 2 + 118 * text.fontscale), combow, 1, particle.comboparticles)
    end
    drawtextclip("gameover", 0, ht / 2 - 30 * text.fontscale, text.aligncenter)
    drawtextclip("restart", 0, ht / 2 + 30 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("quit", 0, ht / 2 + 52 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("option", 0, ht / 2 + 74 * text.fontscale, text.aligncenter, nil, true)
end

--------------------------------------------------------------------------------

local function drawlevelcomplete()
    ov("blend 1")
    drawtextclip("complete", 0, ht / 2 - 30 * text.fontscale, text.aligncenter)
    drawtextclip("continue", 0, ht / 2 + 30 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("quitgame", 0, ht / 2 + 52 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("option", 0, ht / 2 + 74 * text.fontscale, text.aligncenter, nil, true)
end

--------------------------------------------------------------------------------

local function drawbonuscomplete(remainingtime, bonusscore)
    ov("blend 1")
    drawtextclip("bcomplete", 0, ht / 2 - 30 * text.fontscale, text.aligncenter)

    local w = drawtextclip("awarded", 0, ht / 2, text.aligncenter)
    if brick.bricksleft <= bonus.green then
        createparticles(edgegapl + floor(wd / 2 + w / 2), floor(ht / 2), w, 1, particle.bonusparticlesg)
    elseif brick.bricksleft <= bonus.yellow then
        createparticles(edgegapl + floor(wd / 2 + w / 2), floor(ht / 2), w, 1, particle.bonusparticlesy)
    end
    drawtextclip("continue", 0, ht / 2 + 30 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("quitgame", 0, ht / 2 + 52 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("option", 0, ht / 2 + 74 * text.fontscale, text.aligncenter, nil, true)
    if game.newbonus then
        local bonusw = drawtextclip("newbonus", 0, ht / 2 + 96 * text.fontscale, text.aligncenter)
        createparticles(edgegapl + floor(wd / 2 + bonusw / 2), floor(ht / 2 + 96 * text.fontscale), bonusw, 1, particle.bonusparticles)
    end
end

--------------------------------------------------------------------------------

local function drawconfirm()
    ov("blend 1")
    drawtextclip("askquit", 0, ht / 2 - 15 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("askleft", 0, ht / 2 + 22 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("askright", 0, ht / 2 + 44 * text.fontscale, text.aligncenter, nil, true)
end

--------------------------------------------------------------------------------

local function drawpause()
    ov("blend 1")
    drawtextclip("pause", 0, ht / 2 - 15 * text.fontscale, text.aligncenter)
    if game.offoverlay and options.autopause ~= 0 then
        if options.autostart ~= 0 then
            drawtextclip("focus", 0, ht / 2 + 22 * text.fontscale, text.aligncenter)
        else
            drawtextclip("manfocus", 0, ht / 2 + 22 * text.fontscale, text.aligncenter)
            drawtextclip("resume", 0, ht / 2 + 44 * text.fontscale, text.aligncenter, nil, true)
            drawtextclip("quitgame", 0, ht / 2 + 66 * text.fontscale, text.aligncenter, nil, true)
            drawtextclip("option", 0, ht / 2 + 88 * text.fontscale, text.aligncenter, nil, true)
        end
    else
        drawtextclip("resume", 0, ht / 2 + 22 * text.fontscale, text.aligncenter, nil, true)
        drawtextclip("quitgame", 0, ht / 2 + 44 * text.fontscale, text.aligncenter, nil, true)
        drawtextclip("option", 0, ht / 2 + 66 * text.fontscale, text.aligncenter, nil, true)
    end
end

--------------------------------------------------------------------------------

local function drawnewball()
    ov("blend 1")
    drawtextclip("newball", 0, ht / 2 + 22 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("control", 0, ht / 2 + 44 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("quitgame", 0, ht / 2 + 66 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("option", 0,  ht / 2 + 88 * text.fontscale, text.aligncenter, nil, true)
    drawtextclip("left", 0, ht / 2 - 15 * text.fontscale, text.aligncenter)
    if bonus.level then
        drawtextclip("bonus", 0, ht / 2 - 52 * text.fontscale, text.aligncenter)
    else
        drawtextclip("level", 0, ht / 2 - 52 * text.fontscale, text.aligncenter)
    end
end

--------------------------------------------------------------------------------

local function drawtiming(t)
    timing.times[timing.timenum] = t
    timing.timenum = timing.timenum + 1
    if timing.timenum > timing.numtimes then
        timing.timenum = 1
    end
    local average = 0
    for i = 1, #timing.times do
        average = average + timing.times[i]
    end
    average = average / #timing.times
    local oldblend = ov("blend 1")
    updatemessage("ms", string.format("%.1fms", average))
    drawtextclip("ms", -4, 0, text.alignright, text.alignbottom)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

local function drawoption(key, setting, state, leftx, h, y)
    if key ~= "key" then
        ov(op.black)
        ov("fill "..(leftx + edgegapl + shadow.txtx).." "..(y + shadow.txty).." "..(messages[key].width + 3).." "..(messages[key].height - 4))
        ov(keycol)
        ov("fill "..(leftx + edgegapl).." "..y.." "..(messages[key].width + 3).." "..(messages[key].height - 4))
    end
    drawtextclip(key, leftx, y, text.alignleft)
    drawtextclip(setting, 0, y, text.aligncenter)
    drawtextclip(state, -leftx, y, text.alignright)
    return y + h
end

--------------------------------------------------------------------------------

local function drawpercent(downkey, upkey, setting, valname, leftx, h, y)
    local width  = messages[downkey].width
    local height = messages[downkey].height
    ov(op.black)
    ov("fill "..(leftx + edgegapl + shadow.txtx).." "..(y + shadow.txty).." "..(width + 3).." "..(height - 4))
    ov("fill "..(leftx + width * 2 + edgegapl + shadow.txtx).." "..(y + shadow.txty).." "..(width + 3).." "..(height - 4))
    ov(keycol)
    ov("fill "..(leftx + edgegapl).." "..y.." "..(width + 3).." "..(height - 4))
    ov("fill "..(leftx + width * 2 + edgegapl).." "..y.." "..(width + 3).." "..(height - 4))
    drawtextclip(downkey, leftx, y, text.alignleft)
    drawtextclip(upkey, leftx + width * 2, y, text.alignleft)
    drawtextclip(setting, 0, y, text.aligncenter)
    drawtextclip(valname, -leftx, y, text.alignright)
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
    y = drawoption("a", "autopause", state[options.autopause], leftx, h, y)
    y = drawoption("b", "brickscore", state[options.brickscore], leftx, h, y)
    y = drawoption("c", "comboscore", state[options.comboscore], leftx, h, y)
    y = drawoption("d", "shadows", state[options.showshadows], leftx, h, y)
    y = drawoption("m", "mouse", state[options.showmouse], leftx, h, y)
    y = drawoption("p", "particles", state[options.showparticles], leftx, h, y)
    y = drawoption("q", "confirm", state[options.confirmquit], leftx, h, y)
    y = drawoption("s", "autostart", state[options.autostart], leftx, h, y)
    y = drawoption("t", "timing", state[options.showtiming], leftx, h, y)
    y = drawoption("f11", "fullscreen", state[options.fullscreen], leftx, h, y)
    y = drawpercent("-", "=", "sound", "fxvol", leftx, h, y)
    y = drawpercent("[", "]", "music", "musicvol", leftx, h, y)

    -- draw close options
    drawtextclip("close", 0, y + h, text.aligncenter, nil, true)
    if game.balls == 0 then
        drawtextclip("quit", 0, y + h * 2.5, text.aligncenter, nil, true)
    else
        drawtextclip("quitgame", 0, y + h * 2.5, text.aligncenter, nil, true)
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
    if not game.pause then
        if g.getoption("showoverlay") == 0 then
            pausegame(true)
        end
    end
end

--------------------------------------------------------------------------------

local function updatebatposition()
    local mousepos = ov("xy")
    if mousepos ~= "" then
        local mousex, mousey = split(mousepos)
        if mousex ~= bat.lastx then
            bat.lastx = mousex
            bat.x = tonumber(mousex) - bat.wd / 2
            if bat.x < edgegapl then
                bat.x = edgegapl
            elseif bat.x > wd - edgegapr - bat.wd then
                bat.x = wd - edgegapr - bat.wd
            end
        end
        -- check if mouse was off overlay
        if game.offoverlay then
            -- check if paused
            if game.pause and options.autostart ~= 0 and options.autopause ~= 0 then
                pausegame(false)
            end
        end
        game.offoverlay = false
    else
        -- mouse off overlay
        game.offoverlay = true
        -- check for autopause if in game
        if options.autopause ~= 0 and not game.newball then
            pausegame(true)
        end
    end
end

--------------------------------------------------------------------------------

local function clearbonusbricks()
    local bricks = {}
    local clearparticles = particle.brickparticles / 4
    for y = 1, brick.numrows do
        bricks = rows[y]
        for x = 1, brick.numcols do
            if bricks[x] then
                bricks[x] = false
                createparticles(x * brick.wd + edgegapl, floor((y + brick.offsety) * brick.ht), brick.wd, brick.ht, clearparticles, brick.cols[y])
            end
        end
    end
end

--------------------------------------------------------------------------------

local function computebonus()
    local bonusscore = 0
    if brick.bricksleft <= bonus.green then
        bonusscore = (brick.totalbricks - brick.bricksleft) * (100 + (game.level - 1) * 10)
        updatemessage("awarded", "Bricks left "..brick.bricksleft.." = "..bonusscore, op.green)
    elseif brick.bricksleft <= bonus.yellow then
        bonusscore = (brick.totalbricks - brick.bricksleft) * (50 + (game.level - 1) * 10)
        updatemessage("awarded", "Bricks left "..brick.bricksleft.." = "..bonusscore, op.yellow)
    else
        updatemessage("awarded", "Bricks left "..brick.bricksleft.." = ".."No Bonus", op.red)
    end
    playmusic("levelcompleteloop", true)
    updatescore(game.score + bonusscore)
    if game.score > game.hiscore then
        game.newhigh = true
        updatehighscore(game.score)
    end
    if bonusscore > bonus.best then
        game.newbonus = true
        bonus.best = bonusscore
    end

    return bonusscore
end

--------------------------------------------------------------------------------

local function resetcombo()
    updatecombo(1)
    game.combomult  = 1
    game.comboraw   = 0
    game.comboextra = 0
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
    local fadestart = music.fade
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
        if options.showtiming == 1 then
            drawtiming(g.millisecs() - t)
        end
        -- fade the music
        music.fade = fadestart * ((100 - i) / 100)
        setvolume(music.currenttrack, (options.musicvol * music.fade / 100))
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
    setchannelvolume("fx", options.soundvol)
    setchannelvolume("music", options.musicvol)

    -- play games until finished
    game.balls    = 3
    game.score    = 0
    game.level    = 1
    game.again    = true
    game.newhigh  = false
    game.newcombo = false
    game.newbonus = false

    -- initialise the bat and ball
    initbat()
    initball()

    -- welcome message
    notify("Golly Breakout build "..build)

    -- create static text
    createstatictext()

    -- initialize dynamic text
    updatescore(game.score)
    updatehighscore(game.hiscore)
    updateballs(game.balls)
    updatelevel(game.level)

    -- main loop
    while game.again do
        music.fade = 1

        -- initialize the bricks
        initbricks()

        -- create the background
        createbackground()

        -- intiialize the bat
        local bathits = 0
        local maxhits = 7
        bat.lastx     = -1

        -- initialize the ball
        local balldx   = 0.5
        local balldy   = -1
        local maxspeed = 2.2 + (game.level - 1) * 0.1
        if maxspeed > 3 then
            maxspeed = 3
        end
        local speedinc  = 0.02
        local speeddef  = 1 + (game.level - 1) * speedinc * 4
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
        game.newball       = true
        options.confirming = false
        pausegame(false)

        -- whether mouse off overlay
        game.offoverlay = false

        -- reset combo
        resetcombo()

        -- game loop
        playmusic("gamestart")
        while game.balls > 0 and brick.bricksleft > 0 and bonus.current > 0 do
            -- time frame
            local frametime = g.millisecs()

            -- check for mouse click or key press
            processinput()

            -- check if size of overlay has changed or overlay is hidden
            checkforsystemevent()

            -- draw the background
            drawbackground()

            -- check if paused
            if not game.pause and not options.confirming and not options.showoptions and bonus.current > 0 then
                -- check for new ball
                if not game.newball then
                    -- update ball position incrementally
                    local framesteps = floor(ball.numsteps * timing.framemult)
                    local i = 1
                    while i <= framesteps and not game.newball do
                        i = i + 1
                        local stepx = ((balldx * ballspeed * ball.size) / speeddiv) / ball.numsteps
                        local stepy = ((balldy * ballspeed * ball.size) / speeddiv) / ball.numsteps
                        ball.x = ball.x + stepx
                        ball.y = ball.y + stepy

                        -- check for ball hitting left or right boundary
                        if ball.x < ball.size / 2 + edgegapl or ball.x >= wd - edgegapr - ball.size / 2 then
                            createparticles(ball.x, ball.y, 1, 1, particle.wallparticles)
                            -- invert x direction
                            balldx = -balldx
                            ball.x  = ball.x - stepx
                            playsound("edge")
                        end

                        -- check for ball hitting top boundary
                        if ball.y < ball.size / 2 then
                            createparticles(ball.x, floor(ball.y - ball.size / 2), 1, 1, particle.wallparticles)
                            -- ball hit top so speed up a little bit
                            balldy    = -balldy
                            ball.y     = ball.y - stepy
                            ballspeed = ballspeed + speedinc / 2
                            if ballspeed > maxspeed then
                                ballspeed = maxspeed
                            end
                            playsound("top")

                        -- check for ball hitting bottom boundary
                        elseif ball.y >= ht then
                            -- check for bonus level
                            if bonus.level then
                                -- end bonus level
                                bonus.current = 0
                            else
                                -- ball lost!
                                updateballs(game.balls - 1)
                                balldy       = -1
                                balldx       = 0.5
                                ballspeed    = speeddef
                                game.newball = true
                                -- reset combo
                                if game.comboextra - game.comboraw > 0 then
                                    if options.comboscore == 1 then
                                        notify("Combo x "..(game.combo - 1).." Score "..game.comboextra - game.comboraw.." (+"..(floor(100 * game.comboextra / game.comboraw) - 100).."%)")
                                    end
                                end
                                resetcombo()
                                -- destroy bat if no balls left
                                if game.balls == 0 then
                                    createparticles(bat.x + bat.wd, bat.y, bat.wd, bat.ht, particle.lostparticles)
                                end
                                playmusic("lostball")
                            end
                            -- exit loop
                            i = framesteps + 1

                        -- check for ball hitting bat
                        elseif ball.y >= bat.y and ball.y <= bat.y + bat.ht - 1 and ball.x >= bat.x and ball.x < bat.x + bat.wd then
                            -- set dx from where ball hit bat
                            balldx = (3 * (ball.x - bat.x) / bat.wd) - 1.5
                            if balldx >= 0 and balldx < 0.1 then
                                balldx = 0.1
                            end
                            if balldx > -0.1 and balldx <= 0 then
                                balldx = -0.1
                            end
                            balldy  = -balldy
                            ball.y  = bat.y
                            bathits = bathits + 1
                            -- move the bricks down after a number of bat hits
                            if bathits == maxhits then
                                bathits = 0
                                if brick.offsety < brick.maxoffsety then
                                    brick.movedown = brick.movesteps
                                    brick.startoffset = brick.offsety
                                end
                            end
                            createparticles(ball.x, ball.y - ball.size / 2, 1, 1, particle.batparticles)
                            -- reset combo
                            if game.comboextra - game.comboraw > 0 then
                                if options.comboscore == 1 then
                                    notify("Combo x "..(game.combo - 1).." Score "..game.comboextra - game.comboraw.." (+"..(floor(100 * game.comboextra / game.comboraw) - 100).."%)")
                                end
                            end
                            resetcombo()
                            playsound("bat")
                        end

                        -- check for ball hitting brick
                        brick.y = floor((ball.y - (brick.offsety * brick.ht)) / brick.ht)
                        if brick.y >= 1 and brick.y <= brick.numrows then
                            brick.x = floor((ball.x - edgegapl) / brick.wd) + 1
                            if rows[brick.y][brick.x] then
                                -- hit a brick!
                                rows[brick.y][brick.x] = false
                                -- adjust score
                                local points = floor((game.level + 9) * (brick.numrows - brick.y + 1) * game.combomult)
                                local rawpoints = floor((game.level + 9) * (brick.numrows - brick.y + 1))
                                if game.combo > 1 then
                                    game.comboraw = game.comboraw + rawpoints
                                    game.comboextra = game.comboextra + points
                                end
                                updatescore(game.score + points)
                                if game.score > game.hiscore then
                                    game.newhigh = true
                                    updatehighscore(game.score)
                                end
                                createpoints((brick.x - 1) * brick.wd + edgegapl, brick.y * brick.ht, points)
                                -- increment combo
                                game.combomult = game.combomult * game.combofact
                                if game.combo + 1 > game.maxcombo then
                                    game.maxcombo = game.combo + 1
                                    game.newcombo = true
                                end
                                updatecombo(game.combo + 1)
                                if game.combo > game.gamecombo then
                                    game.gamecombo = game.combo
                                end
                                -- work out which axis to invert
                                local lastbricky = floor(((ball.y - stepy) - (brick.offsety * brick.ht)) / brick.ht)
                                if lastbricky == brick.y then
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
                                createparticles(brick.x * brick.wd + edgegapl, floor((brick.y + brick.offsety) * brick.ht), brick.wd, brick.ht, particle.brickparticles, brick.cols[brick.y])
                                -- one less brick
                                brick.bricksleft = brick.bricksleft - 1
                                playsound("brick"..brick.y)
                            end
                        end
                    end
                end
            end

            -- update brick position
            if brick.movedown > 0 then
                brick.movedown = brick.movedown - 1
                brick.offsety = brick.offsety + (1 / brick.movesteps)
                if brick.movedown <= 0 then
                    brick.movedown = 0
                    brick.offsety = brick.startoffset + 1
                end
            end
            
            -- update bat position
            updatebatposition()

            -- if new ball then set ball to sit on bat
            if game.newball then
                ball.x = bat.x + bat.wd / 2
                ball.y = bat.y - ball.size
            end

            -- draw the particles
            drawparticles()

            -- draw the bricks
            drawbricks()

            -- draw the points
            if options.brickscore == 1 then
                drawpoints()
            end

            -- draw the ball
            if game.balls > 0 then
                drawball()
            end

            -- draw the bat
            drawbat()

            -- draw the score, high score and lives
            drawscoreline()

            -- check for text overlay
            if options.confirming then
                drawconfirm()
            elseif options.showoptions then
                drawoptions()
            elseif game.pause then
                drawpause()
            elseif game.newball and game.balls > 0 then
                drawnewball()
            end

            -- update music volume (used for pause fade/resume)
            updatemusic()

            -- draw timing if on
            if options.showtiming == 1 then
                drawtiming(g.millisecs() - frametime)
            end

            -- update notification
            updatenotification()

            -- update the display
            ov("update")

            -- pause until frame time reached
            while g.millisecs() - frametime < 16 do end

            -- check what the actual frame time was and scale speed accordingly
            timing.framemult = 1
            local finaltime = g.millisecs() - frametime
            if finaltime > timing.sixtyhz then
                -- cap to maximum frame time in case external event took control for a long time
                if finaltime > timing.framecap then
                    finaltime = timing.framecap
                end
                timing.framemult = finaltime / timing.sixtyhz
            end

            -- update bonus time if on bonus level
            if bonus.level and not game.pause and not game.newball and not options.showoptions then
                bonus.current = bonus.current - (finaltime / 1000)
            end
        end

        -- check for bonus level complete
        local bonusfinal = bonus.current
        local bonusscore = 0
        if bonus.level then
            bonusscore = computebonus()
            clearbonusbricks()
        else
            if brick.bricksleft == 0 then
                playmusic("levelcompleteloop", true)
            end
        end

        -- save high score, max combo and best bonus
        if game.newhigh or game.newcombo or game.newbonus then
            writesettings()
        end

        -- draw best combo
        if game.balls == 0 then
            notify("Best Combo x"..game.maxcombo - 1)
        end

        -- loop until mouse button clicked or enter pressed
        bonus.current       = -1
        game.finished       = false
        local fading        = false
        local musicplaytime = g.millisecs()

        while not game.finished do
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

            -- draw brick score
            if options.brickscore == 1 then
                drawpoints()
            end

            -- check why game finished
            if options.showoptions then
                drawoptions()
            else
                if game.balls == 0 then
                    -- game over
                    drawgameover()
                else
                    -- draw bat
                    drawbat()
                    if bonus.level then
                        -- end of bonus level
                        drawbonuscomplete(bonusfinal, bonusscore)
                    else
                        -- level complete
                        drawlevelcomplete()
                    end
                end
            end

            -- handle music during game over
            if game.balls == 0 then
                if fading then
                    -- fade music after one play through
                    if frametime - musicplaytime > music.gameovertime then
                        music.fade = music.fade + music.faderate
                        if music.fade < 0 then
                            music.fade = 0
                        end
                        setvolume("gamelostloop", (options.musicvol * music.fade / 100))
                    end
                else
                    -- wait for ball lost music to finish
                    if soundstate("lostball") ~= "playing" then
                        fading         = true
                        musicplaytime  = g.millisecs()
                        music.fade     = 1
                        music.faderate = -0.001
                        playmusic("gamelostloop", true)
                    else
                        updatemusic()
                    end
                end
            end

            -- draw score line
            drawscoreline()

            -- get key or mouse event
            processendinput()

            -- draw timing if on
            if options.showtiming == 1 then
                drawtiming(g.millisecs() - frametime)
            end

            -- update notification
            updatenotification()

            -- update the display
            ov("update")

            -- pause until frame time reached
            while g.millisecs() - frametime < 16 do end

            -- check what the actual frame time was and scale speed accordingly
            timing.framemult = 1
            local finaltime = g.millisecs() - frametime
            if finaltime > timing.sixtyhz then
                -- cap to maximum frame time in case external event took control for a long time
                if finaltime > timing.framecap then
                    finaltime = timing.framecap
                end
                timing.framemult = finaltime / timing.sixtyhz
            end
        end

        -- check why game finished
        if game.balls == 0 then
            -- reset
            updatescore(0)
            updateballs(3)
            updatelevel(1)
            game.newhigh   = false
            updatehighscore(game.hiscore)
            game.newcombo  = false
            game.gamecombo = 1
        else
            -- level complete
            updatelevel(game.level + 1)
        end
        game.newbonus = false
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
    text.fontscale = wd / minwd
    if (ht /minht) < text.fontscale then
        text.fontscale = ht / minht
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
