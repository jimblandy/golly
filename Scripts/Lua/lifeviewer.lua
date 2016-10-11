-- LifeViewer for Golly
-- Author: Chris Rowett (rowett@yahoo.com), September 2016.

-- build number
local buildnumber = 13

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
local glidesteps = 20   -- steps to glide camera
local minstop = 1
local minloop = 1
local mintheme = 0
local maxtheme = #op.themes

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

-- script tokens
local tokens = {}
local curtok = 1
local scriptstartword = "[["
local scriptendword   = "]]"
local angleword       = "ANGLE"
local autofitword     = "AUTOFIT"
local autostartword   = "AUTOSTART"
local depthword       = "DEPTH"
local layersword      = "LAYERS"
local loopword        = "LOOP"
local stopword        = "STOP"
local themeword       = "THEME"
local zoomword        = "ZOOM"

--------------------------------------------------------------------------------

local function sethex()
    if hexon then
        ov("celloption hex 1")
    else
        ov("celloption hex 0")
    end
end

--------------------------------------------------------------------------------

local function checkhex()
    hexon = op.hexrule()
    sethex()
end

--------------------------------------------------------------------------------

local function refresh()
    ov("drawcells")
    ov("update")
end

--------------------------------------------------------------------------------

local function showhelp()
    local helptext = "LifeViewer for Golly build "..buildnumber.."\n"..
[[

Keyboard commands:

Playback controls:
Enter   toggle play / pause
Space   pause / next generation
Tab     pause / next step
Esc     close
R       reset to generation 0
h       help

Camera controls:
Key     Function                Shift
[       zoom out                halve zoom
]       zoom in                 double zoom
F       fit pattern to display  toggle autofit
1       1x zoom                 integer zoom
2       2x zoom                 -2x zoom
4       4x zoom                 -4x zoom
8       8x zoom                 -8x zoom
6       16x zoom                -16x zoom
3       32x zoom
Left    pan left                pan north west
Right   pan right               pan south east
Up      pan up                  pan north east
Down    pan down                pan south west
<       rotate left             rotate left 90
>       rotate right            rotate right 90
5       reset angle

View controls:
Key     Function                Shift
Q       increase layers
A       decrease layers
P       increase layer depth
L       decrease layer depth
C       cycle themes            toggle theme
/       toggle hex mode
]]

    local scripttext =
[[


Scripts must be embedded in pattern comments.
Commands must be surrounded by whitespace.

Script commands:

[[                          start script section
]].."]]"..
[[                          end script secion
ANGLE       <0..360>        set camera angle
AUTOFIT                     fit pattern to display
AUTOSTART                   start play automatically
DEPTH       <0.0..1.0>      set layer depth
LAYERS      <1..10>         set number of layers
LOOP        <1..>           loop at generation
STOP        <1..>           stop at generation
THEME       <0..9>          set theme
ZOOM        <-16.0..32.0>   set camera zoom
]]

    -- display help using a mono-spaced font
    local oldfont = ov("font 10 mono")
    -- use a pale yellow background (matches help window)
    local oldrgba = ov("rgba 255 255 220 255")
    ov("fill")
    ov(op.black)
    local w, h = op.multiline("helpclip", helptext)
    local oldblend = ov("blend 1")
    ov("paste 10 10 helpclip")
    
    -- show script text to the right
    op.multiline("helpclip", scripttext)
    ov("paste "..(10+w+30).." 10 helpclip")
    
    -- restore blend state, font, color
    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov("rgba "..oldrgba)
    
    ov("freeclip helpclip")
    ov("update")

    -- wait for any key or click in overlay
    while true do
        local event = g.getevent()
        if event:find("^key") or event:find("^oclick") then break end
    end
    refresh()
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

    -- update status bar
    local status = "Hit escape to abort script.  Zoom "..string.format("%.1f", camzoom).."x  Angle "..camangle.."  X "..string.format("%.1f", x).."  Y "..string.format("%.1f", y).."  Layers "..camlayers.."  Depth "..string.format("%.2f",camlayerdepth).."  Theme "..themestatus.."  Autofit "..autofitstatus.."  Mode "..hexstatus
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

    -- hex mode does not support rotation
    if hexon then
        camangle = 0
    end

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
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function glidecamera()
    local linearcomplete = camsteps / glidesteps
    
    camx = startx + linearcomplete * (endx - startx)
    camy = starty + linearcomplete * (endy - starty)
    linearzoom = startzoom + linearcomplete * (endzoom - startzoom)
    
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

        -- get the smallest zoom needed
        local zoom = viewwd / width
        if zoom > viewht / height then
            zoom = viewht / height
        end

        -- leave a border around the pattern
        zoom = zoom * zoomborder

        -- ensure zoom in range
        if zoom < minzoom then
            zoom = minzoom
        else
            if zoom > maxzoom then
                zoom = maxzoom
            end
        end

        -- get new pan position
        local newx = viewwidth / 2 + leftx + width / 2
        local newy = viewheight / 2 + bottomy + height / 2

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

local function advance(by1)
    if g.empty() then
        generating = false
        return
    end
    
    if by1 then
        g.run(1)
    else
        g.step()
    end
    
    currentgen = tonumber(g.getgen())

    ov("updatecells")
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
    local sinangle = math.sin(-camangle / 180 * math.pi)
    local cosangle = math.cos(-camangle / 180 * math.pi)
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
        generating = autostart
    else
        generating = false
    end

    -- reset golly
    g.reset()
    g.update()

    -- if using theme then need to recreate the cell view to clear history
    if theme ~= -1 then
        createcellview()
        settheme()
        updatecamera()
        sethex()
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

local function scriptstop()
    local result = ""
    local newstop

    curtok = curtok + 1
    if curtok > #tokens then
        result = "missing argument"
    else
        newstop = tonumber(tokens[curtok])
        if newstop == nil then
            result = "argument must be a number"
        else
             if newstop < minstop then
                 result = "argument must be at least "..minstop
             else
                 stopgen = newstop
             end
        end
    end

    return result
end

--------------------------------------------------------------------------------

local function scriptloop()
    local result = ""
    local newloop

    curtok = curtok + 1
    if curtok > #tokens then
        result = "missing argument"
    else
        newloop = tonumber(tokens[curtok])
        if newloop == nil then
            result = "argument must be a number"
        else
             if newloop < minloop then
                 result = "argument must be at least "..minloop
             else
                 loopgen = newloop
             end
        end
    end

    return result
end

--------------------------------------------------------------------------------

local function scriptangle()
    local result = ""
    local newangle

    curtok = curtok + 1
    if curtok > #tokens then
        result = "missing argument"
    else
        newangle = tonumber(tokens[curtok])
        if newangle == nil then
            result = "argument must be a number"
        else
             if newangle < minangle or newangle > maxangle then
                 result = "argument must be from "..minangle.." to "..maxangle
             else
                 camangle = newangle
             end
        end
    end

    return result
end

--------------------------------------------------------------------------------

local function scriptzoom()
    local result = ""
    local newzoom

    curtok = curtok + 1
    if curtok > #tokens then
        result = "missing argument"
    else
        newzoom = tonumber(tokens[curtok])
        if newzoom == nil then
            result = "argument must be a number"
        else
             if newzoom < minzoom or newzoom > maxzoom then
                 result = "argument must be from "..minzoom.." to "..maxzoom
             else
                 linearzoom = realtolinear(newzoom)
             end
        end
    end

    return result
end

--------------------------------------------------------------------------------

local function scriptdepth()
    local result = ""
    local newdepth

    curtok = curtok + 1
    if curtok > #tokens then
        result = "missing argument"
    else
        newdepth = tonumber(tokens[curtok])
        if newdepth == nil then
            result = "argument must be a number"
        else
             if newdepth < mindepth or newdepth > maxdepth then
                 result = "argument must be from "..mindepth.." to "..maxdepth
             else
                 camlayerdepth = newdepth
             end
        end
    end

    return result
end

--------------------------------------------------------------------------------

local function scriptlayers()
    local result = ""
    local newlayers

    curtok = curtok + 1
    if curtok > #tokens then
        result = "missing argument"
    else
        newlayers = tonumber(tokens[curtok])
        if newlayers == nil then
            result = "argument must be a number"
        else
             if newlayers < minlayers or newlayers > maxlayers then
                 result = "argument must be from "..minlayers.." to "..maxlayers
             else
                 camlayers = newlayers
             end
        end
    end

    return result
end

--------------------------------------------------------------------------------

local function scripttheme()
    local result = ""
    local newtheme

    curtok = curtok + 1
    if curtok > #tokens then
        result = "missing argument"
    else
        newtheme = tonumber(tokens[curtok])
        if newtheme == nil then
            result = "argument must be a number"
        else
            if newtheme < mintheme or newtheme > maxtheme then
                result = "argument must be from "..mintheme.." to "..maxtheme
            else
                theme = newtheme
                settheme()
            end
        end
    end

    return result
end

--------------------------------------------------------------------------------

local function checkscript()
    local comments = g.getinfo()
    local inscript = false
    local invalid = ""

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

    -- process any script comments
    if #tokens > 0 then
        curtok = 1
        while curtok <= #tokens do
            -- process the next token
            token = tokens[curtok]
            if token == angleword then
                invalid = scriptangle()
            elseif token == autofitword then
                autofit = true
            elseif token == autostartword then
                autostart = true
                generating = true
            elseif token == depthword then
                invalid = scriptdepth()
            elseif token == layersword then
                invalid = scriptlayers()
            elseif token == loopword then
                invalid = scriptloop()
            elseif token == stopword then
                invalid = scriptstop()
            elseif token == themeword then
                invalid = scripttheme() 
            elseif token == zoomword then
                invalid = scriptzoom()
            else
                invalid = "unknown script commmand"
            end
            -- abort script processing on error
            if invalid ~= "" then
                curtok = #tokens
            end
            curtok = curtok + 1
        end
        updatecamera()
        refresh()
        if invalid ~= "" then
            g.note(token..": "..invalid)
        end
    end
end

--------------------------------------------------------------------------------

local function main()
    -- check if there is a pattern
    if g.empty() then
        g.exit("There is no pattern.")
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
    settheme()

    -- update the overlay
    refresh()

    -- read current generation
    currentgen = tonumber(g.getgen())

    -- check for script commands
    checkscript()
    setdefaultcamera()

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
        elseif event == "key f none" then
            fitzoom(false)
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
        
        if generating then
             advance(false)
             if stopgen == currentgen then
                 generating = false
                 g.note("STOP reached, Play to continue")
             end              
             if loopgen == currentgen then
                 reset(true)
             end
        end
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
