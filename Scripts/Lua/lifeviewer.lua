-- LifeViewer for Golly
-- Author: Chris Rowett (rowett@yahoo.com), September 2016.

-- build number
local buildnumber = 3

local g = golly()

local ov = g.overlay

local viewwd, viewht = g.getview(g.getlayer())

-- whether generating life
local generating = false

-- view constants
local minzoom = 0.0625
local maxzoom = 32
local viewwidth = 2048
local viewheight = 2048

-- camera defaults
local defcamx = 1024
local defcamy = 1024
local defcamangle = 0
local defcamzoom = 0.4
local defcamlayers = 1
local defcamlayerdepth = 0.05


-- camera
local camx = defcamx
local camy = defcamy
local camangle = defcamangle
local camzoom = defcamzoom
local camlayers = defcamlayers
local camlayerdepth = defcamlayerdepth

-- theme
local theme = -1

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
1		1x zoom
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

View controls:
Q		increase number of layers
A		decrease number of layers
P		increase layer depth
L		decrease layer depth
C		toggle pattern / theme colors
]]

    -- display help
    g.note(helptext)
end

--------------------------------------------------------------------------------

local function createoverlay()
    -- create overlay over entire viewport
    ov("create "..viewwd.." "..viewht)
end

--------------------------------------------------------------------------------

local function displaytointernalzoom(zoom)
    return math.log(zoom / minzoom) / math.log(maxzoom / minzoom)
end

--------------------------------------------------------------------------------

local function internaltodisplayzoom(zoom)
    return minzoom * math.pow(maxzoom / minzoom, zoom)
end

--------------------------------------------------------------------------------

local function updatecamera()
    ov("camzoom "..camzoom)
    ov("camangle "..camangle)
    ov("camxy "..camx.." "..camy)
    ov("camlayers "..camlayers.." "..camlayerdepth)

    -- convert zoom to actual value
    local zoom = internaltodisplayzoom(camzoom)
    if zoom < 0.999 then
       zoom = -(1 / zoom)
    end

    -- convert x and y to display coordinates
    local x = camx - viewwidth / 2
    local y = camy - viewheight / 2

    -- update status
    g.show("Hit escape to abort script.  Zoom "..string.format("%.1f", zoom).."x  Angle "..camangle.."  X "..string.format("%.1f", x).."  Y "..string.format("%.1f", y).."  Layers "..camlayers.."  Depth "..string.format("%.2f",camlayerdepth))
end

--------------------------------------------------------------------------------

local function resetcamera()
    camx = defcamx
    camy = defcamy
    camangle = defcamangle
    camzoom = defcamzoom
    updatecamera()
end

--------------------------------------------------------------------------------

local function createcellview()
    ov("cellview -1024 -1024 "..viewwidth.." "..viewheight)
end

--------------------------------------------------------------------------------

local function refresh()
    ov("update") --???!!!
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
    
    -- mark cell view as dirty
    ov("dirty")

    g.update()
end

--------------------------------------------------------------------------------

local function reset()
    -- reset the camera if at generation 0
    local gen = tonumber(g.getgen())
    if gen == 0 then
        resetcamera()
    end

    createcellview()
    updatecamera()
    g.reset()
    g.update()
    refresh()
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

local function zoomout()
    if camzoom > 0 then
        camzoom = camzoom - 0.01
        if camzoom < 0 then
            camzoom = 0
        end
        updatecamera()
        refresh()
    end
end

--------------------------------------------------------------------------------

local function zoomin()
    if camzoom < 1 then
        camzoom = camzoom + 0.01
        if camzoom > 1 then
            camzoom = 1
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
    camzoom = displaytointernalzoom(zoom)
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function halvezoom()
    local halfzoom = internaltodisplayzoom(camzoom) / 2
    if halfzoom < minzoom then
        halfzoom = minzoom
    end
    setzoom(halfzoom)
end

--------------------------------------------------------------------------------

local function doublezoom()
    local doublezoom = internaltodisplayzoom(camzoom) * 2
    if doublezoom > maxzoom then
        doublezoom = maxzoom
    end
    setzoom(doublezoom)
end

--------------------------------------------------------------------------------

local function settheme()
    ov("theme "..theme)
end

--------------------------------------------------------------------------------

local function toggletheme()
    if theme == -1 then
        theme = 1
    else
        theme = -1
    end
    settheme()
    ov("dirty")
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

local function main()
    createoverlay()
    createcellview()
    resetcamera()
    setzoom(1)
    settheme()
    refresh()
    
    while true do
        -- check if size of layer has changed
        local newwd, newht = g.getview(g.getlayer())
        if newwd ~= viewwd or newht ~= viewht then
            viewwd = newwd
            viewht = newht
            ov("create "..viewwd.." "..viewht)      -- resize overlay
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
        else
            g.doevent(event)
        end
        
        if generating then advance(false) end
    end
end

--------------------------------------------------------------------------------

local status, err = pcall(main)
if err then g.continue(err) end
-- the following code is always executed

ov("delete")
