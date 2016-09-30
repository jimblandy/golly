-- LifeViewer for Golly
-- Author: Chris Rowett (rowett@yahoo.com), September 2016.

local g = golly()

local ov = g.overlay

local viewwd, viewht = g.getview(g.getlayer())

-- whether generating life
local generating = false

-- camera defaults
local defcamx = 1024
local defcamy = 1024
local defcamangle = 0
local defcamzoom = 0.4
local defcamlayers = 1
local defcamlayerdepth = 1.02

local minzoom = 0.0625
local maxzoom = 32

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

local function createoverlay()
    -- create overlay over entire viewport
    ov("create "..viewwd.." "..viewht)
end

--------------------------------------------------------------------------------

local function updatecamera()
    ov("camzoom "..camzoom)
    ov("camangle "..camangle)
    ov("camxy "..camx.." "..camy)
    ov("camlayers "..camlayers.." "..camlayerdepth)

    -- convert zoom to actual value
    local zoom = minzoom * math.pow(maxzoom / minzoom, camzoom)
    if zoom < 0.999 then
       zoom = -(1 / zoom)
    end

    -- update status
    g.show("Hit escape to abort script.  Zoom "..string.format("%.1f", zoom).."x  Angle "..camangle.."  X "..string.format("%.1f", camx).."  Y "..string.format("%.1f", camy).."  Layers "..camlayers.."  Depth "..camlayerdepth)
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
    ov("cellview -1024 -1024 2048 2048")
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

local function rotateleft()
    camangle = camangle - 1
    if camangle < 0 then
        camangle = 359
    end
    updatecamera()
    refresh()
end

--------------------------------------------------------------------------------

local function rotateright()
    camangle = camangle + 1
    if camangle > 359 then
        camangle = 0
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
    camzoom = math.log(zoom / minzoom) / math.log(maxzoom / minzoom)
    updatecamera()
    refresh()
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
        elseif event == "key , none" then
            rotateleft()
        elseif event == "key . none" then
            rotateright()
        elseif event == "key 5 none" then
            resetangle()
        elseif event == "key 1 none" then
            setzoom(1)
        elseif event == "key 2 none" then
            setzoom(2)
        elseif event == "key 4 none" then
            setzoom(4)
        elseif event == "key 8 none" then
            setzoom(8)
        elseif event == "key 6 none" then
            setzoom(16)
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
