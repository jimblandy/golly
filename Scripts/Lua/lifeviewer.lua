-- LifeViewer for Golly
-- Author: Chris Rowett (rowett@yahoo.com), September 2016.
--
-- Lua version of the Javascript/HTML5 LifeViewer used
-- on the conwaylife.com forums and the LifeWiki.

-- build number
local buildnumber = 17

local g = golly()

local ov = g.overlay

local gp = require "gplus"
local op = require "oplus"

local viewwd, viewht = g.getview(g.getlayer())

-- whether generating life
local generating = false
local autostart = false
local loopgen = -1
local stopgen = -1
local currentgen = 0

-- view constants
local minzoom = 0.0625
local mininvzoom = -1 / minzoom
local maxzoom = 32
local minangle = 0
local maxangle = 360
local viewwidth = 2048
local viewheight = 2048
local mindepth = 0
local maxdepth = 1
local minlayers = 1
local maxlayers = 10
local zoomborder = 0.9  -- proportion of view to fill with fit zoom
local glidesteps = 40   -- steps to glide camera
local minstop = 1
local minloop = 1
local mintheme = 0
local maxtheme = #op.themes
local minpan = -4096
local maxpan = 4096
local mingps = 1
local maxgps = 60
local minstep = 1
local maxstep = 50
local decaysteps = 64  -- number of steps to decay after life finished
local decay = 0

-- playback speed
local defgps = 60
local defstep = 1
local gps = defgps
local step = defstep
local genstarttime = os.clock()

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
local defcamx = viewwidth / 2
local defcamy = viewheight / 2
local defcamangle = 0
local defcamlayers = 1
local defcamlayerdepth = 0.05
local deflinearzoom = 0.4

-- camera
local camx = defcamx
local camy = defcamy
local camangle = defcamangle
local camlayers = defcamlayers
local camlayerdepth = defcamlayerdepth
local autofit = false

-- tracking
local tracke = 0
local tracks = 0
local trackw = 0
local trackn = 0
local trackperiod = 0
local trackdefined = false

-- origin
local originx = 0
local originy = 0
local originz = 1

-- zoom is held as a value 0..1 for smooth linear zooms
local linearzoom

-- theme
local themeon = true
local theme = 1

-- mouse drag
local lastmousex = 0
local lastmousey = 0
local mousedrag = false
local clickx = 0
local clicky = 0

-- whether hex display is on
local hexon = false

-- grid
local grid = false
local gridmajoron = true
local gridmajor = 10
local mingridmajor = 0
local maxgridmajor = 16

-- stars
local stars = false

-- script tokens
local tokens = {}
local curtok = 1
local arguments
local scriptstartword   = "[["
local scriptendword     = "]]"
local angleword         = "ANGLE"
local autofitword       = "AUTOFIT"
local autostartword     = "AUTOSTART"
local depthword         = "DEPTH"
local gpsword           = "GPS"
local gridword          = "GRID"
local gridmajorword     = "GRIDMAJOR"
local hexdisplayword    = "HEXDISPLAY"
local layersword        = "LAYERS"
local loopword          = "LOOP"
local squaredisplayword = "SQUAREDISPLAY"
local starsword         = "STARS"
local stepword          = "STEP"
local stopword          = "STOP"
local themeword         = "THEME"
local trackword         = "TRACK"
local trackboxword      = "TRACKBOX"
local trackloopword     = "TRACKLOOP"
local xword             = "X"
local yword             = "Y"
local zoomword          = "ZOOM"
local zword             = "Z"

-- keyword decoding
-- each argument has a type followed by constraint values
-- "r" - float range
-- "R" - int range
-- "l" - float lower bound
-- "L" - int lower bound
local keywords = {
    [angleword] =         { "r", minangle, maxangle, "" },
    [autofitword] =       { "" },
    [autostartword] =     { "" },
    [depthword] =         { "r", mindepth, maxdepth, "" },
    [gpsword] =           { "r", mingps, maxgps, "" },
    [gridword] =          { "" },
    [gridmajorword] =     { "R", mingridmajor, maxgridmajor, "" },
    [hexdisplayword] =    { "" },
    [layersword] =        { "R", minlayers, maxlayers, "" },
    [loopword] =          { "L", minloop, "" },
    [squaredisplayword] = { "" },
    [starsword]         = { "" },
    [stepword] =          { "r", minstep, maxstep, "" },
    [stopword] =          { "L", minstop, "" },
    [themeword] =         { "R", mintheme, maxtheme, "" },
    [trackword] =         { "r", -1, 1, "r", -1, 1, "" },
    [trackboxword] =      { "r", -1, 1, "r", -1, 1, "r", -1, 1, "r", -1, 1, "" },
    [trackloopword] =     { "L", 1, "r", -1, 1, "r", -1, 1, "" },
    [xword] =             { "r", minpan, maxpan, "" },
    [yword] =             { "r", minpan, maxpan, "" },
    [zoomword] =          { "r", mininvzoom, maxzoom, "" },
    [zword] =             { "r", mininvzoom, maxzoom, "" }
}

--------------------------------------------------------------------------------

local function refresh()
    local newx, newy

    -- add the fractional part to the current generation
    local fractionalgen = (os.clock() - genstarttime) * gps

    if fractionalgen < 0 then
        fractionalgen = 0
    else
        if fractionalgen > 1 then
            fractionalgen = 1
        end
    end
    local gen = currentgen + fractionalgen
    originx = gen * (tracke + trackw) / 2
    originy = gen * (tracks + trackn) / 2

    if trackdefined then
        newx = camx + originx
        newy = camy + originy
        ov("camera xy "..newx.." "..newy)
    end

    ov("drawcells")
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
    g.open("lifeviewer.html")
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

local function realtolinear(zoom)
    return math.log(zoom / minzoom) / math.log(maxzoom / minzoom)
end

--------------------------------------------------------------------------------

local function lineartoreal(zoom)
    return minzoom * math.pow(maxzoom / minzoom, zoom)
end

--------------------------------------------------------------------------------

local function updatestatus()
    -- convert zoom to actual value
    local camzoom = lineartoreal(linearzoom)
    if camzoom < 0.999 then
       camzoom = -(1 / camzoom)
    end

    -- convert x and y to display coordinates
    local x = camx - viewwidth / 2
    local y = camy - viewheight / 2
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
    local status = "Hit escape to abort script.  Zoom "..string.format("%.1f", camzoom).."x  Angle "..displayangle.."  X "..string.format("%.1f", x).."  Y "..string.format("%.1f", y).."  Layers "..camlayers.."  Depth "..string.format("%.2f",camlayerdepth).."  GPS "..gps.."  Step "..step.."  Theme "..themestatus.."  Autofit "..autofitstatus.."  Mode "..hexstatus.."  Grid "..gridstatus
    if stopgen ~= -1 then
        status = status.."  Stop "..stopgen
    end
    if loopgen ~= -1 then
        status = status.."  Loop "..loopgen
    end
    g.show(status)
end

--------------------------------------------------------------------------------

local function createoverlay()
    -- create overlay over entire viewport
    ov("create "..viewwd.." "..viewht)
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
    updatestatus()
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

    updatecamera()
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
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function togglegrid()
    grid = not grid
    setgridlines()
    updatestatus()
    refresh()
end

--------------------------------------------------------------------------------

local function togglegridmajor()
    gridmajoron = not gridmajoron
    setgridlines()
    updatestatus()
    refresh()
end

--------------------------------------------------------------------------------

local function togglestars()
    stars = not stars
    setstars()
    refresh()
end

--------------------------------------------------------------------------------

local function bezierx(t, x0, x1, x2, x3)
    local cX = 3 * (x1 - x0)
    local bX = 3 * (x2 - x1) - cX
    local aX = x3 - x0 - cX - bX

    -- compute x position
    local x = (aX * math.pow(t, 3)) + (bX * math.pow(t, 2)) + (cX * t) + x0;
    return x
end

--------------------------------------------------------------------------------

local function glidecamera()
    local linearcomplete = camsteps / glidesteps
    local beziercomplete = bezierx(linearcomplete, 0, 0, 1, 1)
    
    camx = startx + beziercomplete * (endx - startx)
    camy = starty + beziercomplete * (endy - starty)
    linearzoom = startzoom + beziercomplete * (endzoom - startzoom)
    
    camsteps = camsteps + 1
    if camsteps > glidesteps then
        camsteps = -1
        camx = endx
        camy = endy
    end

    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function createcellview()
    ov("cellview "..math.floor(-viewwidth / 2).." "..math.floor(-viewheight / 2).. " "..viewwidth.." "..viewheight)
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
        zoom = zoom * zoomborder

        -- apply origin
        zoom = zoom / originz

        -- ensure zoom in range
        if zoom < minzoom then
            zoom = minzoom
        else
            if zoom > maxzoom then
                zoom = maxzoom
            end
        end

        -- get new pan position
        local newx = viewwidth / 2 + leftx + width / 2 + originy
        local newy = viewheight / 2 + bottomy + height / 2 + originx

        if hexon then
            newx = newx - newy / 2
        end

        -- check whether to fit immediately
        if immediate then
            camx = newx
            camy = newy
            linearzoom = realtolinear(zoom)
            updatecamera()
        else
            -- setup start point
            startx = camx
            starty = camy
            startangle = camangle
            startzoom = linearzoom
     
            -- setup destination point
            endx = newx
            endy = newy
            endangle = camangle
            endzoom = realtolinear(zoom)
    
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
            ov("updatecells")
            refresh()
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
    local deltatime = os.clock() - genstarttime
    if deltatime > 1 / gps then
        while remaining > 0 do
            g.run(1)
            ov("updatecells")
            if g.empty() then
                refresh()
                g.note("Life ended at generation "..tonumber(g.getgen()))
                remaining = 0
                decay = decaysteps
            else
                remaining = remaining - 1
            end
        end
        genstarttime = os.clock()
    end
    
    currentgen = tonumber(g.getgen())

    if autofit then
        fitzoom(true)
    end
    refresh()

    g.update()
end

--------------------------------------------------------------------------------

local function rotate(amount)
    camangle = camangle + amount
    if camangle >= maxangle then
        camangle = camangle - maxangle
    else
        if camangle < minangle then
            camangle = camangle + maxangle
        end
    end
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function adjustzoom(amount, x, y)
    local newx = 0
    local newy = 0

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
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function zoomoutfrom(event)
    local _, x, y = gp.split(event)
    adjustzoom(-0.05, x, y)
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function zoomout()
    local x, y = getmouseposition()
    adjustzoom(-0.01, x, y)
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function zoomin()
    local x, y = getmouseposition()
    adjustzoom(0.01, x, y)
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function resetangle()
    camangle = 0
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function setzoom(zoom)
    startx = camx
    starty = camy
    startzoom = linearzoom
    startangle = camangle
    endx = camx
    endy = camy
    endzoom = realtolinear(zoom)
    endangle = camangle
    camsteps = 0
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
    local camzoom = lineartoreal(linearzoom) / 2
    if camzoom < minzoom then
        camzoom = minzoom
    end
    setzoom(camzoom)
end

--------------------------------------------------------------------------------

local function doublezoom()
    local camzoom = lineartoreal(linearzoom) * 2
    if camzoom > maxzoom then
        camzoom = maxzoom
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
    ov("updatecells")
    updatestatus()
end

--------------------------------------------------------------------------------

local function cycletheme()
    if themeon then
        theme = theme + 1
        if theme > maxtheme then
            theme = mintheme
        end
    else
        themeon = true
    end
    settheme()
    ov("updatecells")
    refresh()
end

--------------------------------------------------------------------------------

local function toggletheme()
    themeon = not themeon
    settheme()
    ov("updatecells")
    refresh()
end

--------------------------------------------------------------------------------

local function decreaselayerdepth()
    if camlayerdepth > mindepth then
        camlayerdepth = camlayerdepth - 0.01
        if camlayerdepth < mindepth then
            camlayerdepth = mindepth
        end
        updatecamera()
        refresh()
    end
end

--------------------------------------------------------------------------------

local function increaselayerdepth()
    if camlayerdepth < maxdepth then
        camlayerdepth = camlayerdepth + 0.01
        if camlayerdepth > maxdepth then
            camlayerdepth = maxdepth
        end
        updatecamera()
        refresh()
    end
end

--------------------------------------------------------------------------------

local function decreaselayers()
    if camlayers > minlayers then
        camlayers = camlayers - 1
        updatecamera()
        refresh()
    end
end

--------------------------------------------------------------------------------

local function increaselayers()
    if camlayers < maxlayers then
        camlayers = camlayers + 1
        updatecamera()
        refresh()
    end
end

--------------------------------------------------------------------------------

local function panview(dx, dy)
    local camzoom = lineartoreal(linearzoom)
    dx = dx / camzoom;
    dy = dy / camzoom;

    local angle = camangle
    if hexon then
        angle = 0
    end
    local sinangle = math.sin(-angle / 180 * math.pi)
    local cosangle = math.cos(-angle / 180 * math.pi)
    camx = camx + (dx * cosangle + dy * -sinangle)
    camy = camy + (dx * sinangle + dy * cosangle)
    updatecamera()
    refresh()
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
    updatestatus()
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

local function reset(looping)
    -- reset the camera if at generation 0
    local gen = tonumber(g.getgen())
    if gen == 0 or looping then
        resetcamera()
        if looping then
            generating = true
        else
            generating = autostart 
        end
    else
        generating = false
    end

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
    ov("updatecells")
    refresh()
end

--------------------------------------------------------------------------------

local function toggleautofit()
    autofit = not autofit
    updatestatus()
end

--------------------------------------------------------------------------------

local function readtokens()
    local comments = g.getinfo()
    local inscript = false
    
    -- build token list
    for token in comments:gsub("\n", " "):gmatch("([^ ]*)") do
        if token ~= "" then
            if token == scriptendword then
                inscript = false
            end
            if inscript then
                table.insert(tokens, token)
            end
            if token == scriptstartword then
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

    if keyword == nil then
        invalid = "unknown script command"
    else
        -- check what sort of validation is required for this keyword
        arguments = {}
        validation = keyword[index]
        while validation ~= "" do
            -- need an argument
            if curtok >= #tokens then
                invalid = "missing argument"
            else
                -- get the argument
                curtok = curtok + 1
                argvalue = tokens[curtok]

                -- check which sort of validation to perform
                if validation == "l" then
                    -- float lower bound
                    argvalue = numberorfraction(argvalue)
                    if argvalue == nil then
                        invalid = "argument must be a number"
                    else
                        index = index + 1
                        lower = keyword[index]
                        if (argvalue < lower) then
                            invalid = "argument must be greater than "..lower
                        end
                    end
                elseif validation == "L" then
                    -- integer lower bound
                    argvalue = tonumber(argvalue)
                    if argvalue == nil then
                        invalid = "argument must be an integer"
                    else
                        if argvalue ~= math.floor(argvalue) then
                            invalid = "argument must be an integer"
                        else
                            index = index + 1
                            lower = keyword[index]
                            if (argvalue < lower) then
                                invalid = "argument must be greater than "..lower
                            end
                        end
                    end
                elseif validation == "r" then
                    -- float range
                    argvalue = numberorfraction(argvalue)
                    if argvalue == nil then
                        invalid = "argument must be a number"
                    else
                        index = index + 1
                        lower = keyword[index]
                        index = index + 1
                        upper = keyword[index]
                        if (argvalue < lower or argvalue > upper) then
                            invalid = "argument must be between "..lower.." and "..upper
                        end
                    end
                elseif validation == "R" then
                    -- integer range
                    argvalue = tonumber(argvalue)
                    if argvalue == nil then
                        invalid = "argument must be an integer"
                    else
                        if argvalue ~= math.floor(argvalue) then
                            invalid = "argument must be an integer"
                        else
                            index = index + 1
                            lower = keyword[index]
                            index = index + 1
                            upper = keyword[index]
                            if (argvalue < lower or argvalue > upper) then
                                invalid = "argument must be between "..lower.." and "..upper
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
                argnum = argnum + 1

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
                if token == angleword then
                    camangle = arguments[1]
                elseif token == autofitword then
                    autofit = true
                elseif token == autostartword then
                    autostart = true
                    generating = true
                elseif token == depthword then
                    camlayerdepth = arguments[1]
                elseif token == gpsword then
                    gps = arguments[1]
                elseif token == gridword then
                    grid = true
                elseif token == gridmajor then
                    gridmajor = arguments[1]
                elseif token == hexdisplayword then
                    hexon = true
                elseif token == layersword then
                    camlayers = arguments[1]
                elseif token == loopword then
                    loopgen = arguments[1]
                elseif token == squaredisplayword then
                    hexon = false
                elseif token == starsword then
                    stars = true
                elseif token == stepword then
                    step = arguments[1]
                elseif token == stopword then
                    stopgen = arguments[1]
                elseif token == themeword then
                    theme = arguments[1]
                elseif token == trackword then
                    tracke = arguments[1]
                    tracks = arguments[2]
                    trackw = tracke
                    trackn = tracks
                    trackdefined = true
                elseif token == trackboxword then
                    tracke = arguments[1]
                    tracks = arguments[2]
                    trackw = arguments[3]
                    trackn = arguments[4]
                    trackdefined = true
                elseif token == trackloopword then
                    trackperiod = arguments[1]
                    tracke = arguments[2]
                    tracks = arguments[3]
                    trackw = tracke
                    trackn = tracks
                    trackdefined = true
                elseif token == xword then
                    camx = arguments[1]
                elseif token == yword then
                    camy = arguments[1]
                elseif token == zoomword or token == zword then
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
        if trackdefined and trackperiod > 0 then
            if gridmajor > 0 then
                loopgen = trackperiod * gridmajor
            else
                loopgen = trackperiod
            end
        end
            
        setstars()
        sethex()
        if hexon then
            camx = camx - camy / 2
        end
        setgridlines()
        updatecamera()
        refresh()
        if invalid ~= "" then
            g.note(token..": "..invalid)
        end
    end
end

--------------------------------------------------------------------------------

local function decreasespeed()
    if step > minstep then
        step = step - 1
    else
        if gps > mingps then
            gps = gps - 1
        end
    end
    updatestatus()
end

--------------------------------------------------------------------------------

local function increasespeed()
    if gps < maxgps then
        gps = gps + 1
    else
        if step < maxstep then
            step = step + 1
        end
    end
    updatestatus()
end

--------------------------------------------------------------------------------

local function setmaxspeed()
    if gps < maxgps then
        gps = maxgps
    else
        if step < maxstep then
            step = maxstep
        end
    end
    updatestatus()
end

--------------------------------------------------------------------------------

local function setminspeed()
    if step > minstep then
        step = minstep
    else
        if gps > mingps then
            gps = mingps
        end
    end
    updatestatus()
end

--------------------------------------------------------------------------------

local function increasestep()
    if step < maxstep then
        step = step + 1
    end
    updatestatus()
end

--------------------------------------------------------------------------------

local function decreasestep()
    if step > minstep then
        step = step - 1
    end
    updatestatus()
end

--------------------------------------------------------------------------------

local function setmaxstep()
    step = maxstep
    updatestatus()
end

--------------------------------------------------------------------------------

local function setminstep()
    step = minstep
    updatestatus()
end

--------------------------------------------------------------------------------

local function resetspeed()
   gps = defgps
   step = defstep
   updatestatus()
end

--------------------------------------------------------------------------------

local function main()
    -- check if there is a pattern
    if g.empty() then
        g.exit("There is no pattern.")
    end

    -- reset pattern if required
    currentgen = tonumber(g.getgen())
    if (currentgen ~= 0) then
        g.reset();
    end

    -- create overlay and cell view
    createoverlay()
    createcellview()

    -- check for hex rules
    checkhex()

    -- reset the camera to default position
    fitzoom(true)
    
    -- use Golly's colors if multi-state pattern
    if g.numstates() > 2 then
        themeon = false
    end

    -- check for script commands
    checkscript()
    setdefaultcamera()
    settheme()
    refresh()

    -- start timing
    genstarttime = os.clock()

    -- loop until quit
    while true do
        -- check if size of layer has changed
        local newwd, newht = g.getview(g.getlayer())
        if newwd ~= viewwd or newht ~= viewht then
            viewwd = newwd
            viewht = newht
            ov("resize "..viewwd.." "..viewht)      -- resize overlay
            refresh()
        end
        
        -- check for user input
        local event = g.getevent()
        if event == "key enter none" or event == "key return none" then
            generating = not generating
        elseif event == "key space none" then
            advance(true)
            generating = false
        elseif event == "key tab none" then
            advance(false)
            generating = false
        elseif event == "key r none" then
            reset(false)
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
    end
end

--------------------------------------------------------------------------------

local status, err = pcall(main)
if err then g.continue(err) end
-- the following code is always executed

ov("delete")
