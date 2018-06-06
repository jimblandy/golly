-- LifeViewer for Golly (work in progress)
-- Author: Chris Rowett (crowett@gmail.com), September 2016.
--
-- LifeViewer is a scriptable pattern viewer featuring:
-- - Rotation and smooth non-integer zoom.
-- - Color themes with cell history and longevity.
-- - Automatic hex grid display.
-- - Pseudo 3D layers and stars.
--
-- This is the Lua version of the Javascript/HTML5 LifeViewer
-- used on the conwaylife.com forums and the LifeWiki.
--
-- Press H after running lifeviewer.lua to display the list of
--  supported keyboard shortcuts and script commands.
--
-- To see some example LifeViewer script commands in action, open
-- Patterns/Life/Rakes/c2-Cordership-rake.rle, then run lifeviewer.lua.

-- build number
local buildnumber = 30

local g = golly()
local ov = g.overlay

local gp = require "gplus"
local op = require "oplus"

local a = require "gplus.strict"

local viewwd, viewht = g.getview(g.getlayer())

-- update flags
local update = {
    docells   = false,
    docamera  = false,
    dorefresh = false,
    dostatus  = false
}

-- textalert
local textalert = {
    appear    = 0,
    hold      = 0,
    disappear = 0,
    color     = "32 255 255 255",
    fontsize  = 30,
    message   = "",
    starttime = 0
}

-- timing
local timing = {
    updatecells    = 0,
    drawcells      = 0,
    drawmenu       = 0,
    texttime       = 0,
    show           = false,
    extended       = false,
    genstarttime   = 0,
    framestarttime = 0,
    frameendtime   = 0
}

-- whether generating life
local generating = false
local autostart  = false
local loopgen    = -1
local stopgen    = -1
local currentgen = 0

-- view constants
local viewconstants = {
    minzoom         = 0.0625, -- 1/16
    maxzoom         = 32,
    minangle        = 0,
    maxangle        = 360,
    viewwidth       = 2048,
    viewheight      = 2048,
    mindepth        = 0,
    maxdepth        = 1,
    minlayers       = 1,
    maxlayers       = 10,
    zoomborder      = 0.9,  -- proportion of view to fill with fit zoom
    glidesteps      = 40,   -- steps to glide camera
    minstop         = 1,
    minloop         = 1,
    mintheme        = 0,
    maxtheme        = #op.themes,
    minpan          = -4096,
    maxpan          = 4096,
    mingps          = 1,
    maxgps          = 60,
    minstep         = 1,
    maxstep         = 50,
    decaysteps      = 64,  -- number of steps to decay after life finished
    mingridmajor    = 0,
    maxgridmajor    = 16,
    minviewerwidth  = 480,
    maxviewerwidth  = 4096,
    minviewerheight = 480,
    maxviewerheight = 4096
}
viewconstants.mininvzoom = -1 / viewconstants.minzoom

-- playback speed
local defgps  = 60
local defstep = 1
local gps     = defgps
local step    = defstep

-- smooth camera movement
local startx
local starty
local startangle
local startzoom
local endx
local endy
local endangle
local endzoom
local camsteps = -1 

-- camera defaults
local defcamx          = viewconstants.viewwidth / 2
local defcamy          = viewconstants.viewheight / 2
local defcamangle      = 0
local defcamlayers     = 1
local defcamlayerdepth = 0.05
local deflinearzoom    = 0.4

-- camera
local camx          = defcamx
local camy          = defcamy
local camangle      = defcamangle
local camlayers     = defcamlayers
local camlayerdepth = defcamlayerdepth
local autofit       = false
local historyfit    = false
local decay         = 0

-- tracking
local tracking = {
    east    = 0,
    south   = 0,
    west    = 0,
    north   = 0,
    period  = 0,
    defined = false
}
local initialrect        -- initial bounding box for pattern at gen 0
local initialzoom = 1    -- initial fit zoom value at gen 0
local histleftx          -- used for autofit history mode
local histrightx
local histbottomy
local histtopy

-- origin
local originx = 0
local originy = 0
local originz = 1

-- zoom is held as a value 0..1 for smooth linear zooms
local linearzoom

-- theme
local themeon = true
local theme   = 1

-- mouse drag
local lastmousex = 0
local lastmousey = 0
local mousedrag  = false
local clickx     = 0
local clicky     = 0

-- whether hex display is on
local hexon = false

-- grid
local grid        = false
local gridmajoron = true
local gridmajor   = 10

-- stars
local stars = false

-- reset
local hardreset = false

-- menu button images
local buttons = {
    ["play"]        = "button0",
    ["pause"]       = "button1",
    ["reset"]       = "button2",
    ["menu"]        = "button3",
    ["stepback"]    = "button4",
    ["stepforward"] = "button5",
    ["autofit"]     = "button6",
    ["fit"]         = "button7",
    ["squaregrid"]  = "button8",
    ["help"]        = "button9",
    ["shrink"]      = "button10",
    ["fps"]         = "button11",
    ["hexgrid"]     = "button12"
}

-- script tokens
local tokens = {}
local curtok = 1
local arguments
local rawarguments    -- the script text before conversion
local commands = {
    scriptstartword    = "[[",
    scriptendword      = "]]",
    angleword          = "ANGLE",
    autofitword        = "AUTOFIT",
    autostartword      = "AUTOSTART",
    depthword          = "DEPTH",
    extendedtimingword = "EXTENDEDTIMING",
    gpsword            = "GPS",
    gridword           = "GRID",
    gridmajorword      = "GRIDMAJOR",
    hardresetword      = "HARDRESET",
    heightword         = "HEIGHT",
    hexdisplayword     = "HEXDISPLAY",
    historyfitword     = "HISTORYFIT",
    layersword         = "LAYERS",
    loopword           = "LOOP",
    pauseword          = "PAUSE",
    showtimingword     = "SHOWTIMING",
    squaredisplayword  = "SQUAREDISPLAY",
    starsword          = "STARS",
    stepword           = "STEP",
    stopword           = "STOP",
    tword              = "T",
    themeword          = "THEME",
    trackword          = "TRACK",
    trackboxword       = "TRACKBOX",
    trackloopword      = "TRACKLOOP",
    widthword          = "WIDTH",
    xword              = "X",
    yword              = "Y",
    zoomword           = "ZOOM",
    zword              = "Z"
}

-- keyword decoding
-- each argument has a type followed by constraint values
-- "r" - float range
-- "R" - int range
-- "l" - float lower bound
-- "L" - int lower bound
local keywords = {
    [commands.angleword] =          { "r", viewconstants.minangle, viewconstants.maxangle, "" },
    [commands.autofitword] =        { "" },
    [commands.autostartword] =      { "" },
    [commands.depthword] =          { "r", viewconstants.mindepth, viewconstants.maxdepth, "" },
    [commands.extendedtimingword] = { "" },
    [commands.gpsword] =            { "r", viewconstants.mingps, viewconstants.maxgps, "" },
    [commands.gridword] =           { "" },
    [commands.gridmajorword] =      { "R", viewconstants.mingridmajor, viewconstants.maxgridmajor, "" },
    [commands.hardresetword] =      { "" },
    [commands.heightword] =         { "R", viewconstants.minviewerheight, viewconstants.maxviewerheight, "" },
    [commands.hexdisplayword] =     { "" },
    [commands.historyfitword] =     { "" },
    [commands.layersword] =         { "R", viewconstants.minlayers, viewconstants.maxlayers, "" },
    [commands.loopword] =           { "L", viewconstants.minloop, "" },
    [commands.pauseword] =          { "l", 0.0, "" },
    [commands.showtimingword] =     { "" },
    [commands.squaredisplayword] =  { "" },
    [commands.starsword]         =  { "" },
    [commands.stepword] =           { "r", viewconstants.minstep, viewconstants.maxstep, "" },
    [commands.stopword] =           { "L", viewconstants.minstop, "" },
    [commands.tword] =              { "L", 0, "" },
    [commands.themeword] =          { "R", viewconstants.mintheme, viewconstants.maxtheme, "" },
    [commands.trackword] =          { "r", -1, 1, "r", -1, 1, "" },
    [commands.trackboxword] =       { "r", -1, 1, "r", -1, 1, "r", -1, 1, "r", -1, 1, "" },
    [commands.trackloopword] =      { "L", 1, "r", -1, 1, "r", -1, 1, "" },
    [commands.widthword] =          { "R", viewconstants.minviewerwidth, viewconstants.maxviewerwidth, "" },
    [commands.xword] =              { "r", viewconstants.minpan, viewconstants.maxpan, "" },
    [commands.yword] =              { "r", viewconstants.minpan, viewconstants.maxpan, "" },
    [commands.zoomword] =           { "r", viewconstants.mininvzoom, viewconstants.maxzoom, "" },
    [commands.zword] =              { "r", viewconstants.mininvzoom, viewconstants.maxzoom, "" }
}

-- pre-rendered text clips
local clips = {
    timinglongfg = "tlongfg",
    timinglongshadow = "tlongshadow",
    shortht = 0,
    longht = 0
}

--------------------------------------------------------------------------------

local function maketext(s, name)
    name = name or "temp"
    -- convert string to text in current font
    local w, h, descent = gp.split(ov("text "..name.." "..s))
    return tonumber(w), tonumber(h), tonumber(descent)
end

--------------------------------------------------------------------------------

local function pastetext(x, y, name)
    name = name or "temp"
    ov("paste "..x.." "..y.." "..name)
end

--------------------------------------------------------------------------------

local function makeclips()
    local oldalign = ov("textoption align left")
    local oldtextbg = ov("textoption background 0 0 0 0")

    local label = "fps"
    local longlabel = "updatecells\ndrawcells\nmenu"

    ov(op.black)
    local w, h = maketext(longlabel, clips.timinglongshadow)
    clips.longht = h
    
    ov(op.white)
    w, h = maketext(label)
    clips.shortht = h
    maketext(longlabel, clips.timinglongfg)
    
    -- restore settings
    ov("textoption background "..oldtextbg)
    ov("textoption align "..oldalign)
end
    
--------------------------------------------------------------------------------

local function outputtiming(xoff, yoff, labelclip)
    -- draw labels
    ov("paste "..(viewwd - 140 + xoff).." "..(20 + yoff + clips.shortht).." "..labelclip)

    -- draw values
    local output = string.format("%.1fms", timing.updatecells)
    output = output.."\n"..string.format("%.1fms", timing.drawcells)
    output = output.."\n"..string.format("%.1fms", timing.drawmenu)
    ov("textoption align right")
    local w = maketext(output)
    pastetext(viewwd - w - 20 + xoff, 20 + yoff + clips.shortht)
end

--------------------------------------------------------------------------------

local function drawmenu()
    local y = 10

    local i = 0
    ov("blend 1")
    while i < 13 do
        -- TBD ov("paste "..(i * 50).." "..y.. "button"..i)
        i = i + 1
    end
    ov("blend 0")
end

--------------------------------------------------------------------------------

local function drawtiming()
    local start = g.millisecs()
    local oldblend = ov("blend 1")
    local oldalign = ov("textoption align left")
    local oldtextbg = ov("textoption background 0 0 0 0")

    -- draw translucent rectangle
    local height = clips.shortht
    if timing.extended then
        height = height + clips.longht
    end
    ov("rgba 0 0 0 128")
    ov("fill "..(viewwd - 140).." 20 122 "..height)

    -- compute load
    local frametime = timing.frameendtime - timing.framestarttime
    local loadamount = frametime / 16
    if loadamount > 1 then
        loadamount = 1
    end
    if frametime < 16.666 then
        frametime = 16.666
    end

    -- draw load background
    if (loadamount < 0.5) then
        -- fade from green to yellow
        ov("rgba "..math.floor(255 * loadamount * 2).." 255 0 144")
    else
        -- fade from yellow to red
        ov("rgba 255 "..math.floor(255 * (1 - (loadamount - 0.5) * 2)).." 0 144")
    end
    ov("fill "..(viewwd - 140).." 20 "..math.floor(122 * loadamount).." "..clips.shortht)

    -- draw fps and load
    ov(op.black)
    maketext(math.floor(1000 / frametime).."fps")
    pastetext(viewwd - 138, 22)
    local w = maketext(math.floor(100 * loadamount).."%")
    pastetext(viewwd - 18 - w, 22)
    ov(op.white)
    maketext(math.floor(1000 / frametime).."fps")
    pastetext(viewwd - 140, 20)
    maketext(math.floor(100 * loadamount).."%")
    pastetext(viewwd - w - 20, 20)

    -- draw extended timing if enabled
    local shadowclip, fgclip
    if timing.extended then
       shadowclip = clips.timinglongshadow
       fgclip = clips.timinglongfg
       ov(op.black)
       outputtiming(2, 2, shadowclip)
       ov(op.white)
       outputtiming(0, 0, fgclip)
    end

    -- restore settings
    ov("textoption background "..oldtextbg)
    ov("textoption align "..oldalign)
    ov("blend "..oldblend)
    timing.texttime = g.millisecs() - start
end

--------------------------------------------------------------------------------

local function alert(message, appear, hold, disappear)
    textalert.message = message
    textalert.appear = appear
    textalert.hold = hold
    textalert.disappear = disappear
end

--------------------------------------------------------------------------------

local function updatealert()
    if textalert.message ~= "" then

    end
end

--------------------------------------------------------------------------------

local function realtolinear(zoom)
    return math.log(zoom / viewconstants.minzoom) / math.log(viewconstants.maxzoom / viewconstants.minzoom)
end

--------------------------------------------------------------------------------

local function lineartoreal(zoom)
    return viewconstants.minzoom * math.pow(viewconstants.maxzoom / viewconstants.minzoom, zoom)
end

--------------------------------------------------------------------------------

local function updatestatus()
    -- convert zoom to actual value
    local zoom = lineartoreal(linearzoom) * originz
    if zoom < 0.99999 then
       zoom = -(1 / zoom)
    end

    -- convert x and y to display coordinates
    local x = camx - viewconstants.viewwidth / 2 + originx
    local y = camy - viewconstants.viewheight / 2 + originy
    if hexon then
        x = x + camy / 2
    end

    -- convert theme to status
    local themestatus = "off"
    if themeon then
        themestatus = theme
    end

    -- convert autofit to status
    local autofitstatus = "off"
    if autofit then
        autofitstatus = "on"
    end

    -- convert hex mode to status
    local hexstatus = "square"
    if hexon then
        hexstatus = "hex"
    end

    -- convert grid to status
    local gridstatus = "off"
    if grid then
        gridstatus = "on"
    end

    -- convert angle to status
    local displayangle = camangle
    if hexon then
        displayangle = 0
    end

    -- update status bar
    local status = "Hit escape to close."
    status = status.."  Zoom "..string.format("%.1f", zoom).."x  Angle "..displayangle
    status = status.."  X "..string.format("%.1f", x).."  Y "..string.format("%.1f", y)
    status = status.."  Layers "..camlayers.."  Depth "..string.format("%.2f",camlayerdepth)
    status = status.."  GPS "..gps.."  Step "..step.."  Theme "..themestatus
    status = status.."  Autofit "..autofitstatus.."  Mode "..hexstatus.."  Grid "..gridstatus
    if stopgen ~= -1 then
        status = status.."  Stop "..stopgen
    end
    if loopgen ~= -1 then
        status = status.."  Loop "..loopgen
    end
    g.show(status)
end

--------------------------------------------------------------------------------

local function refresh()
    local start = g.millisecs()
    local newx, newy, newz

    -- add the fractional part to the current generation
    if tracking.defined then
        local fractionalgen = (g.millisecs() - timing.genstarttime) / 1000 * gps
        if fractionalgen < 0 then
            fractionalgen = 0
        else
            if fractionalgen > 1 then
                fractionalgen = 1
            end
        end
        local gen = currentgen + fractionalgen

        -- compute the new origin
        originx = gen * (tracking.east + tracking.west) / 2
        originy = gen * (tracking.south + tracking.north) / 2

        -- compute the trackbox
        local leftx = initialrect[1]
        local bottomy = initialrect[2]
        local width = initialrect[3]
        local height = initialrect[4]
        local rightx = leftx + width
        local topy = bottomy + height
        leftx = leftx + gen * tracking.west
        rightx = rightx + gen * tracking.east
        bottomy = bottomy + gen * tracking.north
        topy = topy + gen * tracking.south
        width = rightx - leftx + 1
        height = topy - bottomy + 1

        -- check for hex
        if hexon then
            leftx = leftx - bottomy / 2
            rightx = rightx - topy / 2
            width = rightx - leftx + 1
        end

        -- get the smallest zoom needed
        local zoom = viewwd / width
        if zoom > viewht / height then
            zoom = viewht / height
        end
    
        -- leave a border around the pattern
        zoom = zoom * viewconstants.zoomborder

        -- apply the origin to the camera
        originz = zoom / initialzoom
        newx = camx + originx
        newy = camy + originy
        newz = lineartoreal(linearzoom) * originz
        if newz > viewconstants.maxzoom then
            newz = viewconstants.maxzoom
        else
            if newz < viewconstants.minzoom then
                newz = viewconstants.minzoom
            end
        end

        -- set the new camera settings
        ov("camera xy "..newx.." "..newy)
        ov("camera zoom "..newz)

        -- update status because camera changed
        update.dostatus = true
    else
        originx = 0
        originy = 0
        originz = 1
    end

    -- draw the cells
    ov("drawcells")
    timing.drawcells = g.millisecs() - start

    -- draw the menu
    local t1 = g.millisecs()
    drawmenu()
    timing.drawmenu = g.millisecs() - t1

    -- end of frame time
    timing.frameendtime = g.millisecs()

    -- draw timing if on
    if timing.show then
        drawtiming()
    end

    -- update the overlay
    ov("update")
end

--------------------------------------------------------------------------------

local function setgridlines()
    if grid then
        ov("celloption grid 1")
    else
        ov("celloption grid 0")
    end
    if gridmajoron then
        ov("celloption gridmajor "..gridmajor)
    else
        ov("celloption gridmajor 0")
    end
end

--------------------------------------------------------------------------------

local function setstars()
    if stars then
        ov("celloption stars 1")
    else
        ov("celloption stars 0")
    end
end

--------------------------------------------------------------------------------

local function sethex()
    if hexon then
        ov("celloption hex 1")
    else
        ov("celloption hex 0")
    end
    ov("camera xy "..camx.." "..camy)
end

--------------------------------------------------------------------------------

local function checkhex()
    hexon = op.hexrule()
    sethex()
end

--------------------------------------------------------------------------------

local function showhelp()
    -- open help window
    g.open(g.getdir("app").."Help/lifeviewer.html")
end

--------------------------------------------------------------------------------

local function getmouseposition()
    local x = viewwd / 2
    local y = viewht / 2
    local mousepos = ov("xy")
    if mousepos ~= "" then
        x, y = gp.split(mousepos)
        x = tonumber(x)
        y = tonumber(y)
    end

    return x, y
end

--------------------------------------------------------------------------------

local function createoverlay()
    -- create overlay over entire viewport
    ov("create "..viewwd.." "..viewht)
end

--------------------------------------------------------------------------------

local function updatecells()
    local start = g.millisecs()
    ov("updatecells")
    timing.updatecells = g.millisecs() - start
end

--------------------------------------------------------------------------------

local function updatecamera()
    -- convert linear zoom to real zoom
    local camzoom = lineartoreal(linearzoom)
    ov("camera zoom "..camzoom)
    ov("camera angle "..camangle)
    ov("camera xy "..camx.." "..camy)
    ov("celloption layers "..camlayers)
    ov("celloption depth "..camlayerdepth)

    -- update status
    update.dostatus = true
end

--------------------------------------------------------------------------------

local function resetcamera()
    -- restore default camera settings
    camx = defcamx
    camy = defcamy
    camangle = defcamangle
    camlayers = defcamlayers
    camlayerdepth = defcamlayerdepth
    linearzoom = deflinearzoom

    update.docamera = true
end

--------------------------------------------------------------------------------

local function setdefaultcamera()
    defcamx = camx
    defcamy = camy
    defcamangle = camangle
    deflinearzoom = linearzoom
    defcamlayers = camlayers
    defcamlayerdepth = camlayerdepth
end

--------------------------------------------------------------------------------

local function togglehex()
    hexon = not hexon
    sethex()
    if hexon then
        camx = camx - camy / 2
        defcamx = defcamx - camy / 2
    else
        camx = camx + camy / 2
        defcamx = defcamx + camy / 2
    end
    update.docamera = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function togglegrid()
    grid = not grid
    setgridlines()
    update.dostatus = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function togglegridmajor()
    gridmajoron = not gridmajoron
    setgridlines()
    update.dostatus = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function togglestars()
    stars = not stars
    setstars()
    update.dorefresh = true
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

local function glidecamera()
    local linearcomplete = camsteps / viewconstants.glidesteps
    local beziercomplete = bezierx(linearcomplete, 0, 0, 1, 1)
    
    camx = startx + beziercomplete * (endx - startx)
    camy = starty + beziercomplete * (endy - starty)
    local camzoom = startzoom + beziercomplete * (endzoom - startzoom)
    linearzoom = realtolinear(camzoom)

    camsteps = camsteps + 1
    if camsteps > viewconstants.glidesteps then
        camsteps = -1
        camx = endx
        camy = endy
        linearzoom = realtolinear(endzoom)
    end
    update.docamera = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function createcellview()
    ov("cellview "..math.floor(-viewconstants.viewwidth / 2).." "..math.floor(-viewconstants.viewheight / 2).. " "..viewconstants.viewwidth.." "..viewconstants.viewheight)
end

--------------------------------------------------------------------------------

local function fitzoom(immediate)
    local rect = g.getrect()
    if #rect > 0 then
        local leftx = rect[1]
        local bottomy = rect[2]
        local width = rect[3]
        local height = rect[4]
        local rightx = leftx + width
        local topy = bottomy + height

        -- check for historyfit
        if historyfit then
            -- update history bounding box
            if histleftx > leftx then
                histleftx = leftx
            end
            if histrightx < rightx then
                histrightx = rightx
            end
            if histbottomy > bottomy then
                histbottomy = bottomy
            end
            if histtopy < topy then
                histtopy = topy
            end
            
            leftx = histleftx
            rightx = histrightx
            bottomy = histbottomy
            topy = histtopy
            width = rightx - leftx + 1
            height = topy - bottomy + 1
        end

        -- check for hex
        if hexon then
            leftx = leftx - bottomy / 2
            rightx = rightx - topy / 2
            width = rightx - leftx + 1
        end

        -- get the smallest zoom needed
        local zoom = viewwd / width
        if zoom > viewht / height then
            zoom = viewht / height
        end

        -- leave a border around the pattern
        zoom = zoom * viewconstants.zoomborder

        -- apply origin
        if tracking.defined then
            zoom = zoom / originz
        end

        -- ensure zoom in range
        if zoom < viewconstants.minzoom then
            zoom = viewconstants.minzoom
        else
            if zoom > viewconstants.maxzoom then
                zoom = viewconstants.maxzoom
            end
        end

        -- get new pan position
        local newx = viewconstants.viewwidth / 2 + leftx + width / 2 + originy
        local newy = viewconstants.viewheight / 2 + bottomy + height / 2 + originx

        if hexon then
            newx = newx - newy / 2
        end

        -- check whether to fit immediately
        if immediate then
            camx = newx
            camy = newy
            linearzoom = realtolinear(zoom)
            update.docamera = true
        else
            -- setup start point
            startx = camx
            starty = camy
            startangle = camangle
            startzoom = lineartoreal(linearzoom)
     
            -- setup destination point
            endx = newx
            endy = newy
            endangle = camangle
            endzoom = zoom
    
            -- trigger start
            camsteps = 0
        end
    end
end

--------------------------------------------------------------------------------

local function advance(singlestep)
    -- check if all cells are dead
    if g.empty() then
        generating = false
        if decay > 0 then
            update.docells = true
            update.dorefresh = true
            decay = decay - 1
        end
        return
    end
    
    -- check for single step
    local remaining = step
    if singlestep then
        remaining = 1
    end

    -- check if enough time has elapsed to advance
    local deltatime = (g.millisecs() - timing.genstarttime) / 1000
    if deltatime > 1 / gps then
        while remaining > 0 do
            g.run(1)
            updatecells()
            if g.empty() then
                refresh()
                g.note("Life ended at generation "..tonumber(g.getgen()))
                remaining = 0
                decay = viewconstants.decaysteps
            else
                remaining = remaining - 1
            end
        end
        timing.genstarttime = g.millisecs()
    end
    
    currentgen = tonumber(g.getgen())

    if autofit then
        fitzoom(true)
    end
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function rotate(amount)
    camangle = camangle + amount
    if camangle >= viewconstants.maxangle then
        camangle = camangle - viewconstants.maxangle
    else
        if camangle < viewconstants.minangle then
            camangle = camangle + viewconstants.maxangle
        end
    end
    update.docamera = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function adjustzoom(amount, x, y)
    local newx = 0
    local newy = 0
    local newzoom

    -- get the current zoom as actual zoom
    local currentzoom = lineartoreal(linearzoom)

    -- compute new zoom and ensure it is in range
    linearzoom = linearzoom + amount
    if linearzoom < 0 then
        linearzoom = 0
    else
        if linearzoom > 1 then
            linearzoom = 1
        end
    end

    local sinangle = math.sin(-camangle / 180 * math.pi)
    local cosangle = math.cos(-camangle / 180 * math.pi)
    local dx = 0
    local dy = 0

    -- convert new zoom to actual zoom
    newzoom = lineartoreal(linearzoom)

    -- compute as an offset from the centre
    x = x - viewwd / 2
    y = y - viewht / 2

    -- compute position based on new zoom
    newx = x * (newzoom / currentzoom)
    newy = y * (newzoom / currentzoom)
    dx = (x - newx) / newzoom
    dy = -(y - newy) / newzoom

    -- apply pan
    camx = camx - dx * cosangle + dy * -sinangle
    camy = camy - dx * sinangle + dy * cosangle
end

--------------------------------------------------------------------------------

local function zoominto(event)
    local _, x, y = gp.split(event)
    adjustzoom(0.05, x, y)
    update.docamera = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function zoomoutfrom(event)
    local _, x, y = gp.split(event)
    adjustzoom(-0.05, x, y)
    update.docamera = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function zoomout()
    local x, y = getmouseposition()
    adjustzoom(-0.01, x, y)
    update.docamera = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function zoomin()
    local x, y = getmouseposition()
    adjustzoom(0.01, x, y)
    update.docamera = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function resetangle()
    camangle = 0
    update.docamera = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function setzoom(zoom)
    startx = camx
    starty = camy
    startzoom = lineartoreal(linearzoom)
    startangle = camangle
    endx = camx
    endy = camy
    endzoom = zoom
    endangle = camangle
    camsteps = 0
    if tracking.defined then
        startzoom = startzoom / originz
        endzoom = endzoom / originz
    end
end

--------------------------------------------------------------------------------

local function integerzoom()
    local camzoom = lineartoreal(linearzoom)
    if camzoom >= 1 then
        camzoom = math.floor(camzoom + 0.5)
    else
        camzoom = 1 / math.floor(1 / camzoom + 0.5)
    end
    setzoom(camzoom)
end

--------------------------------------------------------------------------------

local function halvezoom()
    local camzoom = lineartoreal(linearzoom)
    camzoom = camzoom / 2
    if camzoom < viewconstants.minzoom then
        camzoom = viewconstants.minzoom
    end
    setzoom(camzoom)
end

--------------------------------------------------------------------------------

local function doublezoom()
    local camzoom = lineartoreal(linearzoom)
    camzoom = camzoom * 2
    if camzoom > viewconstants.maxzoom then
        camzoom = viewconstants.maxzoom
    end
    setzoom(camzoom)
end

--------------------------------------------------------------------------------

local function settheme()
    local index = -1
    if themeon then
        index = theme
    end
    ov(op.themes[index])
    update.docells = true
    update.dostatus = true
end

--------------------------------------------------------------------------------

local function cycletheme()
    if themeon then
        theme = theme + 1
        if theme > viewconstants.maxtheme then
            theme = viewconstants.mintheme
        end
    else
        themeon = true
    end
    settheme()
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function toggletheme()
    themeon = not themeon
    settheme()
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function decreaselayerdepth()
    if camlayerdepth > viewconstants.mindepth then
        camlayerdepth = camlayerdepth - 0.01
        if camlayerdepth < viewconstants.mindepth then
            camlayerdepth = viewconstants.mindepth
        end
        update.docamera = true
        update.dorefresh = true
    end
end

--------------------------------------------------------------------------------

local function increaselayerdepth()
    if camlayerdepth < viewconstants.maxdepth then
        camlayerdepth = camlayerdepth + 0.01
        if camlayerdepth > viewconstants.maxdepth then
            camlayerdepth = viewconstants.maxdepth
        end
        update.docamera = true
        update.dorefresh = true
    end
end

--------------------------------------------------------------------------------

local function decreaselayers()
    if camlayers > viewconstants.minlayers then
        camlayers = camlayers - 1
        update.docamera = true
        update.dorefresh = true
    end
end

--------------------------------------------------------------------------------

local function increaselayers()
    if camlayers < viewconstants.maxlayers then
        camlayers = camlayers + 1
        update.docamera = true
        update.dorefresh = true
    end
end

--------------------------------------------------------------------------------

local function panview(dx, dy)
    local camzoom = lineartoreal(linearzoom)
    dx = dx / camzoom
    dy = dy / camzoom

    local angle = camangle
    if hexon then
        angle = 0
    end
    local sinangle = math.sin(-angle / 180 * math.pi)
    local cosangle = math.cos(-angle / 180 * math.pi)
    camx = camx + (dx * cosangle + dy * -sinangle)
    camy = camy + (dx * sinangle + dy * cosangle)
    update.docamera = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function doclick(event)
    local _, x, y, button, mode = gp.split(event)

    -- start of drag
    lastmousex = tonumber(x)
    lastmousey = tonumber(y)
    mousedrag = true
end

--------------------------------------------------------------------------------

local function stopdrag()
    mousedrag = false
end

--------------------------------------------------------------------------------

local function checkdrag()
    local x, y

    -- check if a drag is in progress
    if mousedrag then
        -- get the mouse position
        local mousepos = ov("xy")
        if #mousepos > 0 then
            x, y = gp.split(mousepos)
            panview(lastmousex - tonumber(x), lastmousey - tonumber(y))
            lastmousex = x
            lastmousey = y
        end
    end
end

--------------------------------------------------------------------------------

local function resethistory()
    histleftx = initialrect[1]
    histbottomy = initialrect[2]
    histrightx = histleftx + initialrect[3] - 1
    histtopy = histbottomy + initialrect[4] - 1
end

--------------------------------------------------------------------------------

local function reset(looping)
    -- reset the camera if at generation 0
    local gen = tonumber(g.getgen())
    if gen == 0 or looping or hardreset then
        resetcamera()
        if looping then
            generating = true
        else
            generating = autostart 
        end
    else
        generating = false
    end

    -- reset historyfit
    resethistory()

    -- reset origin
    originx = 0
    originy = 0
    originz = 1

    -- reset golly
    g.reset()
    g.update()
    currentgen = 0

    -- if using theme then need to recreate the cell view to clear history
    if theme ~= -1 then
        createcellview()
        setstars()
        settheme()
        updatecamera()
        sethex()
        setgridlines()
    end

    -- update cell view from reset universe
    update.docells = true
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function toggleautofit()
    autofit = not autofit
    update.dostatus = true
end

--------------------------------------------------------------------------------

local function togglehistoryfit()
    historyfit = not historyfit
    update.dostatus = true
end

--------------------------------------------------------------------------------

local function readtokens()
    local comments = g.getinfo()
    local inscript = false
    
    -- build token list
    for token in comments:gsub("\n", " "):gmatch("([^ ]*)") do
        if token ~= "" then
            if token == commands.scriptendword then
                inscript = false
            end
            if inscript then
                table.insert(tokens, token)
            end
            if token == commands.scriptstartword then
                inscript = true
            end
        end
    end
end

--------------------------------------------------------------------------------

local function numberorfraction(argument)
    local result = nil

    -- search for a fraction
    local index = argument:find("/")
    if index == nil then
        -- simple number
        result = tonumber(argument)
    else
        -- possible fraction
        local left = argument:sub(1, index - 1)
        local right = argument:sub(index + 1)
        if tonumber(left) ~= nil and tonumber(right) ~= nil then
            result = left / right
        end
    end

    return result
end

--------------------------------------------------------------------------------

local function validatekeyword(token)
    local invalid = ""
    local keyword = keywords[token]
    local validation
    local index = 1
    local lower, upper
    local argvalue
    local argnum = 1
    local whicharg
    local arglist = ""

    if keyword == nil then
        invalid = "unknown script command"
    else
        -- check what sort of validation is required for this keyword
        arguments = {}
        rawarguments = {}
        validation = keyword[index]
        while validation ~= "" do
            -- need an argument
            whicharg = arglist
            if curtok >= #tokens then
                invalid = whicharg.."{{}}\nArgument "..argnum.." is missing"
            else
                -- get the argument
                curtok = curtok + 1
                argvalue = tokens[curtok]
                whicharg = whicharg.." {{"..argvalue.."}}\nArgument "..argnum

                -- check which sort of validation to perform
                if validation == "l" then
                    -- float lower bound
                    argvalue = numberorfraction(argvalue)
                    if argvalue == nil then
                        invalid = whicharg.." must be a number"
                    else
                        index = index + 1
                        lower = keyword[index]
                        if argvalue < lower then
                            invalid = whicharg.." must be at least "..lower
                        end
                    end
                elseif validation == "L" then
                    -- integer lower bound
                    argvalue = tonumber(argvalue)
                    if argvalue == nil then
                        invalid = whicharg.." must be an integer"
                    else
                        if argvalue ~= math.floor(argvalue) then
                            invalid = whicharg.." must be an integer"
                        else
                            index = index + 1
                            lower = keyword[index]
                            if argvalue < lower then
                                invalid = whicharg.." must be at least "..lower
                            end
                        end
                    end
                elseif validation == "r" then
                    -- float range
                    argvalue = numberorfraction(argvalue)
                    if argvalue == nil then
                        invalid = whicharg.." must be a number"
                    else
                        index = index + 1
                        lower = keyword[index]
                        index = index + 1
                        upper = keyword[index]
                        if argvalue < lower or argvalue > upper then
                            invalid = whicharg.." must be between "..lower.." and "..upper
                        end
                    end
                elseif validation == "R" then
                    -- integer range
                    argvalue = tonumber(argvalue)
                    if argvalue == nil then
                        invalid = whicharg.." must be an integer"
                    else
                        if argvalue ~= math.floor(argvalue) then
                            invalid = whicharg.." must be an integer"
                        else
                            index = index + 1
                            lower = keyword[index]
                            index = index + 1
                            upper = keyword[index]
                            if argvalue < lower or argvalue > upper then
                                invalid = whicharg.." must be between "..lower.." and "..upper
                            end
                        end
                    end
                end
            end

            -- check if validation failed
            if invalid ~= "" then
                -- exit loop
                validation = ""
            else
                -- save the argument
                arguments[argnum] = argvalue
                rawarguments[argnum] = tokens[curtok]
                argnum = argnum + 1
                arglist = arglist..tokens[curtok].." "

                -- get next argument to validation
                index = index + 1
                validation = keyword[index]
            end
        end
    end

    return invalid
end

--------------------------------------------------------------------------------

local function checkscript()
    local invalid = ""
    local token

    -- read tokens
    readtokens()

    -- process any script comments
    if #tokens > 0 then
        curtok = 1
        while curtok <= #tokens do
            -- get the next token
            token = tokens[curtok]

            -- check if it is a valid keyword
            invalid = validatekeyword(token)

            -- process command if valid
            if invalid == "" then
                if token == commands.angleword then
                    camangle = arguments[1]
                elseif token == commands.autofitword then
                    autofit = true
                elseif token == commands.autostartword then
                    autostart = true
                    generating = true
                elseif token == commands.depthword then
                    camlayerdepth = arguments[1]
                elseif token == commands.extendedtimingword then
                    timing.extended = true
                elseif token == commands.gpsword then
                    gps = arguments[1]
                elseif token == commands.gridword then
                    grid = true
                elseif token == gridmajor then
                    gridmajor = arguments[1]
                elseif token == commands.hardresetword then
                    hardreset = true
                elseif token == commands.heightword then
                    -- ignored
                elseif token == commands.hexdisplayword then
                    hexon = true
                elseif token == commands.layersword then
                    camlayers = arguments[1]
                elseif token == commands.historyfitword then
                    historyfit = true
                elseif token == commands.loopword then
                    loopgen = arguments[1]
                elseif token == commands.showtimingword then
                    timing.show = true
                elseif token == commands.squaredisplayword then
                    hexon = false
                elseif token == commands.starsword then
                    stars = true
                elseif token == commands.stepword then
                    step = arguments[1]
                elseif token == commands.stopword then
                    stopgen = arguments[1]
                elseif token == commands.themeword then
                    theme = arguments[1]
                elseif token == commands.trackword then
                    tracking.east = arguments[1]
                    tracking.south = arguments[2]
                    tracking.west = tracking.east
                    tracking.north = tracking.south
                    tracking.defined = true
                elseif token == commands.trackboxword then
                    tracking.east = arguments[1]
                    tracking.south = arguments[2]
                    tracking.west = arguments[3]
                    tracking.north = arguments[4]
                    if tracking.west > tracking.east then
                        invalid = "[["..rawarguments[1].."]] "..rawarguments[2].." [["
                        invalid = invalid..rawarguments[3].."]] "..rawarguments[4]
                        invalid = invalid.."\nW is greater than E"
                    else
                        if tracking.north > tracking.south then
                            invalid = rawarguments[1].." [["..rawarguments[2].."]] "
                            invalid = invalid..rawarguments[3].." [["..rawarguments[4].."]]"
                            invalid = invalid.."\nN is greater than S"
                        else
                            tracking.defined = true
                        end
                    end
                elseif token == commands.trackloopword then
                    tracking.period = arguments[1]
                    tracking.east = arguments[2]
                    tracking.south = arguments[3]
                    tracking.west = tracking.east
                    tracking.north = tracking.south
                    tracking.defined = true
                elseif token == commands.widthword then
                    -- ignored
                elseif token == commands.xword then
                    camx = arguments[1]
                elseif token == commands.yword then
                    camy = arguments[1]
                elseif token == commands.zoomword or token == commands.zword then
                    if arguments[1] < 0 then
                        arguments[1] = -1 / arguments[1]
                    end
                    linearzoom = realtolinear(arguments[1])
                end
            end

            -- abort script processing on error
            if invalid ~= "" then
                curtok = #tokens
            end
            curtok = curtok + 1
        end

        -- check for trackloop
        if tracking.defined and tracking.period > 0 then
            if gridmajor > 0 then
                loopgen = tracking.period * gridmajor
            else
                loopgen = tracking.period
            end
        end
            
        setstars()
        sethex()
        if hexon then
            camx = camx - camy / 2
        end
        setgridlines()
        update.docamera = true
        update.dorefresh = true
        if invalid ~= "" then
            g.note(token.." "..invalid)
        end
    end
end

--------------------------------------------------------------------------------

local function decreasespeed()
    if step > viewconstants.minstep then
        step = step - 1
    else
        if gps > viewconstants.mingps then
            gps = gps - 1
        end
    end
    update.dostatus = true
end

--------------------------------------------------------------------------------

local function increasespeed()
    if gps < viewconstants.maxgps then
        gps = gps + 1
    else
        if step < viewconstants.maxstep then
            step = step + 1
        end
    end
    update.dostatus = true
end

--------------------------------------------------------------------------------

local function setmaxspeed()
    if gps < viewconstants.maxgps then
        gps = viewconstants.maxgps
    else
        if step < viewconstants.maxstep then
            step = viewconstants.maxstep
        end
    end
    update.dostatus = true
end

--------------------------------------------------------------------------------

local function setminspeed()
    if step > viewconstants.minstep then
        step = viewconstants.minstep
    else
        if gps > viewconstants.mingps then
            gps = viewconstants.mingps
        end
    end
    update.dostatus = true
end

--------------------------------------------------------------------------------

local function increasestep()
    if step < viewconstants.maxstep then
        step = step + 1
        update.dostatus = true
    end
end

--------------------------------------------------------------------------------

local function decreasestep()
    if step > viewconstants.minstep then
        step = step - 1
        update.dostatus = true
    end
end

--------------------------------------------------------------------------------

local function setmaxstep()
    step = viewconstants.maxstep
    update.dostatus = true
end

--------------------------------------------------------------------------------

local function setminstep()
    step = viewconstants.minstep
    update.dostatus = true
end

--------------------------------------------------------------------------------

local function resetspeed()
   gps = defgps
   step = defstep
   update.dostatus = true
end

--------------------------------------------------------------------------------

local function togglehardreset()
    hardreset = not hardreset
end

--------------------------------------------------------------------------------

local function toggletiming()
    timing.show = not timing.show
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function toggleextendedtiming()
    timing.extended = not timing.extended
    update.dorefresh = true
end

--------------------------------------------------------------------------------

local function cbauto()
    toggleautofit()
end

--------------------------------------------------------------------------------

local function cbfit()
    if not autofit then
        fitzoom(false)
    end
end

--------------------------------------------------------------------------------

local function cbgrid()
    togglegrid()
end

--------------------------------------------------------------------------------

local function cbhelp()
    showhelp()
end

--------------------------------------------------------------------------------

local function cbreset()
    reset(false)
end

--------------------------------------------------------------------------------

local function cbplay()
    generating = not generating
end

--------------------------------------------------------------------------------

local function cbfps()
    toggletiming()
end

--------------------------------------------------------------------------------

local function createmenu()
    ov("blend 0")
    ov("create 1 1 menuclip")
    ov("target menuclip")

    -- load button images to get width and height
    local w, h = gp.split(ov("load 1 0 oplus/images/lifeviewer.png"))
    ov("resize "..w.." "..h.." menuclip")

    -- load buttons over the rectangle
    ov("load 0 0 oplus/images/lifeviewer.png")

    -- replace transparent background with semi-opaque
    ov("rgba 0 0 0 128")
    ov("replace *# *# *# 0")

    -- create clips from the images
    ov("rgba 32 255 255 255")
    local x = 0
    while x < 13 do
        -- draw box around button
        ov("line "..(40 * x).." 0 "..(40 * x + 39).." 0")
        ov("line "..(40 * x).." 39 "..(40 * x + 39).." 39")
        ov("line "..(40 * x).." 0 "..(40 * x).." 39")
        ov("line "..(40 * x + 39).." 0 "..(40 * x + 39).." 39")

        -- create clip from button
        ov("copy "..(40 * x).." 0 40 40 button"..x)
        x = x + 1
    end

    -- delete menuclip
    ov("target")
    ov("delete menuclip")
end

--------------------------------------------------------------------------------

local function setupoverlay()
    -- create overlay and cell view
    createoverlay()
    createcellview()

    -- create menu
    createmenu()

    -- make prerendered text clips
    makeclips()
end

--------------------------------------------------------------------------------

function main()
    -- check if there is a pattern
    if g.empty() then
        g.exit("There is no pattern.")
    end

    -- reset pattern if required
    currentgen = tonumber(g.getgen())
    if currentgen ~= 0 then
        g.reset()
    end

    -- setup overlay
    setupoverlay()
    
    -- check for hex rules
    checkhex()

    -- reset the camera to default position
    fitzoom(true)

    -- get the initial bounding box and zoom
    initialrect = g.getrect()
    initialzoom = lineartoreal(linearzoom)

    -- reset history box
    resethistory()
    
    -- use Golly's colors if multi-state pattern
    if g.numstates() > 2 then
        themeon = false
    end

    -- check for script commands
    checkscript()

    -- save camera settings to use when reset
    setdefaultcamera()

    -- apply any script settings
    settheme()
    updatecamera()
    updatecells()
    refresh()
    updatestatus()

    -- start timing
    timing.genstarttime = g.millisecs()

    -- loop until quit
    while true do
        -- clear update flags
        update.dorefresh = false
        update.dostatus = false
        update.docamera = false
        update.docells = false

        -- check if size of layer has changed
        local newwd, newht = g.getview(g.getlayer())
        if newwd ~= viewwd or newht ~= viewht then
            viewwd = newwd
            viewht = newht

            -- resize overlay
            ov("resize "..viewwd.." "..viewht)
            update.dorefresh = true
        end

        -- check for user input
        local event = op.process(g.getevent())
        if event == "key return none" then
            generating = not generating
            if generating then
                timing.genstarttime = g.millisecs()
            end
        elseif event == "key space none" then
            advance(true)
            generating = false
        elseif event == "key tab none" then
            advance(false)
            generating = false
        elseif event == "key r none" then
            reset(false)
        elseif event == "key r shift" then
            togglehardreset()
        elseif event == "key - none" then
            decreasespeed()
        elseif event == "key = none" then
            increasespeed()
        elseif event == "key _ none" then
            setminspeed()
        elseif event == "key + none" then
            setmaxspeed()
        elseif event == "key d none" then
            decreasestep()
        elseif event == "key e none" then
            increasestep()
        elseif event == "key d shift" then
            setminstep()
        elseif event == "key e shift" then
            setmaxstep()
        elseif event == "key 0 none" then
            resetspeed()
        elseif event == "key f none" then
            if not autofit then
                fitzoom(false)
            end
        elseif event == "key f shift" then
            toggleautofit()
        elseif event == "key h shift" then
            togglehistoryfit()
        elseif event == "key [ none" then
            zoomout()
        elseif event == "key ] none" then
            zoomin()
        elseif event == "key { none" then
            halvezoom()
        elseif event == "key } none" then
            doublezoom()
        elseif event == "key , none" then
            rotate(-1)
        elseif event == "key < none" then
            rotate(-90)
        elseif event == "key . none" then
            rotate(1)
        elseif event == "key > none" then
            rotate(90)
        elseif event == "key 5 none" then
            resetangle()
        elseif event == "key 1 none" then
            setzoom(1)
        elseif event == "key ! none" then
            integerzoom()
        elseif event == "key 2 none" then
            setzoom(2)
        elseif event == "key \" none" then
            setzoom(1/2)
        elseif event == "key 4 none" then
            setzoom(4)
        elseif event == "key $ none" then
            setzoom(1/4)
        elseif event == "key 8 none" then
            setzoom(8)
        elseif event == "key * none" then
            setzoom(1/8)
        elseif event == "key 6 none" then
            setzoom(16)
        elseif event == "key ^ none" then
            setzoom(1/16)
        elseif event == "key 3 none" then
            setzoom(32)
        elseif event == "key c none" then
            cycletheme()
        elseif event == "key c shift" then
            toggletheme()
        elseif event == "key q none" then
            increaselayers()
        elseif event == "key a none" then
            decreaselayers()
        elseif event == "key p none" then
            increaselayerdepth()
        elseif event == "key l none" then
            decreaselayerdepth()
        elseif event == "key s none" then
            togglestars()
        elseif event == "key left none" then
            panview(-1, 0)
        elseif event == "key right none" then
            panview(1, 0)
        elseif event == "key up none" then
            panview(0, -1)
        elseif event == "key down none" then
            panview(0, 1)
        elseif event == "key left shift" then
            panview(-1, -1)
        elseif event == "key right shift" then
            panview(1, 1)
        elseif event == "key up shift" then
            panview(1, -1)
        elseif event == "key down shift" then
            panview(-1, 1)
        elseif event == "key h none" then
            showhelp()
        elseif event == "key / none" then
            togglehex()
        elseif event == "key x none" then
            togglegrid()
        elseif event == "key x shift" then
            togglegridmajor()
        elseif event == "key t none" then
            toggletiming()
        elseif event == "key t shift" then
            toggleextendedtiming()
        elseif event:find("^ozoomin") then
            zoominto(event)
        elseif event:find("^ozoomout") then
            zoomoutfrom(event)
        elseif event:find("^oclick") then
            doclick(event)
        elseif event:find("^mup") then
            stopdrag()
        else
            g.doevent(event)
            checkdrag()
        end
        
        -- start frame timer
        timing.framestarttime = g.millisecs()

        -- check if playing
        if generating then
            advance(false)

            -- check for stop or loop
            if stopgen == currentgen then
                generating = false
                g.note("STOP reached, Play to continue")
            end              
            if loopgen == currentgen then
                reset(true)
            end
        else
            -- check if life ended but cells still decaying
            if decay > 0 then
                advance(false)
            end
        end

        -- check if camera still gliding to target
        if camsteps ~= -1 then
             glidecamera()
        end

        -- perform required updates
        if update.docamera then
            updatecamera()
        end
        if update.docells then
            updatecells()
        end
        if update.dorefresh then
            refresh()
        end
        if update.dostatus then
            updatestatus()
        end
    end
end

--------------------------------------------------------------------------------

local status, err = xpcall(main, gp.trace)
if err then g.continue(err) end
-- the following code is always executed

g.check(false)
ov("delete")
