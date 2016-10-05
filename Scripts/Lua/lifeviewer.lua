-- LifeViewer for Golly
-- Author: Chris Rowett (rowett@yahoo.com), September 2016.

-- build number
local buildnumber = 7

local g = golly()

local ov = g.overlay

local gp = require "gplus"
local op = require "oplus"

local viewwd, viewht = g.getview(g.getlayer())

-- whether generating life
local generating = false

-- view constants
local minzoom = 0.0625
local maxzoom = 32
local viewwidth = 2048
local viewheight = 2048

-- camera defaults
local defcamx = viewwidth / 2
local defcamy = viewheight / 2
local defcamangle = 0
local defcamlayers = 1
local defcamlayerdepth = 0.05
local deflinearzoom  -- set by copygollyzoom()

-- camera
local camx = defcamx
local camy = defcamy
local camangle = defcamangle
local camlayers = defcamlayers
local camlayerdepth = defcamlayerdepth

-- zoom is held as a value 0..1 for smooth linear zooms
local linearzoom

-- theme
local theme = -1

-- mouse drag
local lastmousex = 0
local lastmousey = 0
local mousedrag = false

local clickx = 0
local clicky = 0

--------------------------------------------------------------------------------

local function showhelp()
    local helptext = "LifeViewer for Golly build "..buildnumber..
[[

Keyboard commands

Playback controls:
Enter	toggle play / pause
Space	pause / next generation
Tab		pause / next step
Esc		close
R		reset to generation 0

Camera controls:
Key		Function	Shift
[		zoom out	halve zoom
]		zoom in		double zoom
1		1x zoom	integer zoom
2		2x zoom	-2x zoom
4		4x zoom	-4x zoom
8		8x zoom	-8x zoom
6		16x zoom	-16x zoom
3		32x zoom
Left		pan left		pan north west
Right	pan right	pan south east
Up		pan up		pan north east
Down	pan down	pan south west
<		rotate left	rotate left 90
>		rotate right	rotate right 90
5		reset angle

View controls:
Q		increase number of layers
A		decrease number of layers
P		increase layer depth
L		decrease layer depth
C		cycle themes
]]

    -- display help
    g.note(helptext)
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
    local themestatus = theme
    if theme == -1 then
        themestatus = "off"
    end

    g.show("Hit escape to abort script.  Zoom "..string.format("%.1f", camzoom).."x  Angle "..camangle.."  X "..string.format("%.1f", x).."  Y "..string.format("%.1f", y).."  Layers "..camlayers.."  Depth "..string.format("%.2f",camlayerdepth).."  Theme "..themestatus)
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

    ov("camzoom "..camzoom)
    ov("camangle "..camangle)
    ov("camxy "..camx.." "..camy)
    ov("camlayers "..camlayers.." "..camlayerdepth)

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

local function createcellview()
    ov("cellview "..math.floor(-viewwidth / 2).." "..math.floor(-viewheight / 2).. " "..viewwidth.." "..viewheight)
end

--------------------------------------------------------------------------------

local function refresh()
    ov("drawcells")
    ov("update")
end

--------------------------------------------------------------------------------

local function advance(by1)
    if g.empty() then
        g.note("There are no live cells.")
        generating = false
        return
    end
    
    if by1 then
        g.run(1)
    else
        g.step()
    end
    
    ov("updatecells")
    refresh()

    g.update()
end

--------------------------------------------------------------------------------

local function rotate(amount)
    camangle = camangle + amount
    if (camangle > 359) then
        camangle = camangle - 360
    else
        if (camangle < 0) then
            camangle = camangle + 360
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
    adjustzoom(0.1, x, y)
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function zoomoutfrom(event)
    local _, x, y = gp.split(event)
    adjustzoom(-0.1, x, y)
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function zoomout()
    if linearzoom > 0 then
        linearzoom = linearzoom - 0.01
        if linearzoom < 0 then
            linearzoom = 0
        end
        updatecamera()
        refresh()
    end
end

--------------------------------------------------------------------------------

local function zoomin()
    if linearzoom < 1 then
        linearzoom = linearzoom + 0.01
        if linearzoom > 1 then
            linearzoom = 1
        end
        updatecamera()
        refresh()
    end
end

--------------------------------------------------------------------------------

local function resetangle()
    camangle = 0
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function setzoom(zoom)
    linearzoom = realtolinear(zoom)
    updatecamera()
    refresh()
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
    ov(op.themes[theme])
    updatestatus()
end

--------------------------------------------------------------------------------

local function cycletheme()
    theme = theme + 1
    if theme > op.maxtheme then
        theme = -1
    end
    settheme()
    ov("updatecells")
    refresh()
end

--------------------------------------------------------------------------------

local function decreaselayerdepth()
    if camlayerdepth > 0 then
        camlayerdepth = camlayerdepth - 0.01
        if camlayerdepth < 0 then
            camlayerdepth = 0
        end
        updatecamera()
        refresh()
    end
end

--------------------------------------------------------------------------------

local function increaselayerdepth()
    if camlayerdepth < 10 then
        camlayerdepth = camlayerdepth + 0.01
        if camlayerdepth > 10 then
            camlayerdepth = 10
        end
        updatecamera()
        refresh()
    end
end

--------------------------------------------------------------------------------

local function decreaselayers()
    if camlayers > 1 then
        camlayers = camlayers - 1
        updatecamera()
        refresh()
    end
end

--------------------------------------------------------------------------------

local function increaselayers()
    if camlayers < 10 then
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

local function reset()
    -- reset the camera if at generation 0
    local gen = tonumber(g.getgen())
    if gen == 0 then
        resetcamera()
    end

    -- reset golly
    g.reset()
    g.update()

    -- recreate the cell view if not using theme
    if theme ~= -1 then
        createcellview()
        settheme()
        updatecamera()
    end

    -- update cell view from reset universe
    ov("updatecells")

    refresh()
end

--------------------------------------------------------------------------------

local function copygollyzoom()
    local mag = g.getmag()
    local zoom

    -- convert magnification to zoom value
    if mag >= 0 then
        zoom = 1 << mag
    else
        zoom = 1 / (1 << -mag)
    end

    -- ensure in range
    if zoom > maxzoom then
        zoom = maxzoom
    end
    if zoom < minzoom then
        zoom = minzoom
    end

    -- save as default zoom
    deflinearzoom = realtolinear(zoom)

    -- set the zoom
    setzoom(zoom)
end

--------------------------------------------------------------------------------

local function main()
    -- create overlay and cell view
    createoverlay()
    createcellview()

    -- set initial zoom from Golly's current zoom
    copygollyzoom()

    -- reset the camera to default position
    resetcamera()
    
    -- set the default theme
    settheme()

    -- update the overlay
    refresh()
    
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
            reset()
            generating = false
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
        
        if generating then advance(false) end
    end
end

--------------------------------------------------------------------------------

local status, err = pcall(main)
if err then g.continue(err) end
-- the following code is always executed

ov("delete")
