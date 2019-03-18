-- Run the current pattern for a given number of steps (using current
-- step size) and create a plot of population vs time in an overlay
-- that can be saved in a PNG file.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Oct 2016.

local g = golly()
-- require "gplus.strict"

local gp = require "gplus"
local int = gp.int
local min = gp.min
local max = gp.max

local op = require "oplus"
local ov = g.overlay
local ovt = g.ovtable

local opacity = 80                      -- initial opacity for bgcolor (as a percentage)
local bgcolor = "rgba 255 255 255 "     -- white background (alpha will be appended)
local axiscolor = "rgba 0 0 0 255"      -- black axes
local textcolor = "rgba 0 0 0 255"      -- black text
local plotcolor = "rgba 0 0 255 255"    -- blue plot lines

local xlen = 500    -- length of x axis
local ylen = 500    -- length of y axis
local tborder = 40  -- border above plot
local bborder = 80  -- border below plot
local lborder = 80  -- border left of plot
local rborder = 80  -- border right of plot

-- width and height of overlay
local owd = xlen + lborder + rborder
local oht = ylen + tborder + bborder

local pops = {}     -- store population counts
local gens = {}     -- store generation counts

local numsteps = xlen
local stepsize = string.format("%d^%d", g.getbase(), g.getstep())
local pattname = g.getname()

-- offsets to origin of axes
local originx = lborder
local originy = tborder + ylen

local lines = true      -- draw connected lines

-- these controls are created in create_overlay
local sbutt, cbutt      -- Save and Cancel buttons
local lbox              -- check box for toggling lines
local oslider           -- slider for adjusting opacity

local controlht         -- height of area containing the controls

-- initial directory for the save dialog
local initdir = g.getdir("data")

-- user settings are stored in this file
local settingsfile = g.getdir("data").."pop-plot.ini"

--------------------------------------------------------------------------------

local function read_settings()
    local f = io.open(settingsfile, "r")
    if f then
        -- must match order in write_settings
        numsteps = tonumber(f:read("*l")) or xlen
        opacity = tonumber(f:read("*l")) or 80
        lines = (f:read("*l") or "true") == "true"
        initdir = f:read("*l") or g.getdir("data")
        f:close()
    end
end

--------------------------------------------------------------------------------

local function write_settings()
    local f = io.open(settingsfile, "w")
    if f then
        -- must match order in read_settings
        f:write(tostring(numsteps).."\n")
        f:write(tostring(opacity).."\n")
        f:write(tostring(lines).."\n")
        f:write(initdir.."\n")
        f:close()
    end
end

--------------------------------------------------------------------------------

local function maketext(s)
    -- convert given string to text in default font and return
    -- its width and height for later use by pastetext
    local wd, ht = gp.split(ov("text textclip "..s))
    return tonumber(wd), tonumber(ht)
end

--------------------------------------------------------------------------------

local function pastetext(x, y, transform)
    transform = transform or op.identity
    -- text background is transparent so paste needs to use alpha blending
    ov("blend 1")
    ov(transform)
    ovt{"paste", x+originx, y+originy, "textclip"}
    ov(op.identity)
    ov("blend 0")
end

--------------------------------------------------------------------------------

local function fit_if_not_visible()
    -- fit pattern in viewport if not empty and not completely visible
    local r = g.getrect()
    if #r > 0 and not g.visrect(r) then g.fit() end
end

--------------------------------------------------------------------------------

local function drawline(x1, y1, x2, y2)
    ovt{"line", x1+originx, y1+originy, x2+originx, y2+originy}
end

--------------------------------------------------------------------------------

local function drawdot(x, y)
    -- draw a small "+" mark
    x = x + originx
    y = y + originy
    ovt{"set", x, y, x-1, y, x+1, y, x, y-1, x, y+1}
end

--------------------------------------------------------------------------------

local function run_pattern()
    if g.empty() then g.exit("There is no pattern.") end

    -- prompt user for number of steps
    local s = g.getstring("Enter the number of steps:", numsteps, "Population plotter")
    if #s == 0 then g.exit() end
    s = tonumber(s)
    if s and s > 0 then
        numsteps = int(s)
    else
        g.exit("Number of steps must be > zero.")
    end

    -- generate pattern for given number of steps
    pops[#pops+1] = tonumber(g.getpop())
    gens[#gens+1] = tonumber(g.getgen())
    local oldsecs = os.clock()
    for i = 1, numsteps do
        g.step()
        pops[#pops+1] = tonumber(g.getpop())
        gens[#gens+1] = tonumber(g.getgen())
        local newsecs = os.clock()
        if newsecs - oldsecs >= 1.0 then     -- show pattern every second
            oldsecs = newsecs
            fit_if_not_visible()
            g.update()
            g.show(string.format("Step %d of %d", i, numsteps))
        end
    end

    fit_if_not_visible()
    g.show(" ")
end

--------------------------------------------------------------------------------

local function draw_plot()
    -- fill area above control bar with background color
    ov(bgcolor..int(255*opacity/100+0.5))
    ovt{"fill", 0, 0, owd, (oht-controlht)}

    local minpop = min(pops)
    local maxpop = max(pops)
    if minpop == maxpop then
        -- avoid division by zero
        minpop = minpop - 1
    end
    local popscale = (maxpop - minpop) / ylen

    local mingen = min(gens)
    local maxgen = max(gens)
    local genscale = (maxgen - mingen) / xlen

    -- draw axes
    ov(axiscolor)
    drawline(0, 0, xlen, 0)
    drawline(0, 0, 0, -ylen)

    -- add annotation using the overlay's default font
    ov(textcolor)
    local wd, ht = maketext(string.upper(pattname))
    pastetext(int((xlen - wd) / 2), -ylen - 10 - ht)

    wd, ht = maketext("POPULATION")
    -- rotate this text 90 degrees anticlockwise
    pastetext(-10 - ht, int(-(ylen - wd) / 2), op.racw)

    wd, ht = maketext(""..minpop)
    pastetext(-wd - 10, int(-ht / 2))

    wd, ht = maketext(""..maxpop)
    pastetext(-wd - 10, -ylen - int(ht / 2))

    wd, ht = maketext("GENERATION (step="..stepsize..")")
    pastetext(int((xlen - wd) / 2), 10)

    wd, ht = maketext(""..mingen)
    pastetext(int(-wd / 2), 10)

    wd, ht = maketext(""..maxgen)
    pastetext(xlen - int(wd / 2), 10)

    -- plot the data (it could take a while if numsteps is huge)
    ov(plotcolor)
    local x = int((gens[1] - mingen) / genscale)
    local y = int((pops[1] - minpop) / popscale)
    local oldsecs = os.clock()
    for i = 1, numsteps do
        local newx = int((gens[i+1] - mingen) / genscale)
        local newy = int((pops[i+1] - minpop) / popscale)
        if lines then
            drawline(x, -y, newx, -newy)
        else
            drawdot(newx, -newy)
        end
        x = newx
        y = newy
        local newsecs = os.clock()
        if newsecs - oldsecs >= 1.0 then     -- update plot every second
            oldsecs = newsecs
            g.update()
        end
    end

    g.update()
end

--------------------------------------------------------------------------------

local function do_save()
    -- called if Save button is clicked

    -- remove any existing extension from pattern name and append .png
    local initfile = gp.split(pattname,"%.")..".png"

    -- prompt for file name and location
    local pngpath = g.savedialog("Save as PNG file", "PNG (*.png)|*.png",
                                 initdir, initfile)
    if #pngpath > 0 then
        -- save overlay (minus controls) in given file
        ov("save 0 0 "..owd.." "..(oht-controlht).." "..pngpath)
        g.show("Population plot was saved in "..pngpath)

        -- update initdir by stripping off the file name
        local pathsep = g.getdir("app"):sub(-1)
        initdir = pngpath:gsub("[^"..pathsep.."]+$","")
    end
end

--------------------------------------------------------------------------------

local function toggle_lines()
    -- called if check box is clicked
    lines = not lines
    draw_plot()
end

--------------------------------------------------------------------------------

local function show_opacity()
    -- show opacity % at right end of slider
    ov(textcolor)
    local wd, ht = maketext(""..opacity.."%")
    ov(bgcolor..255)
    local x = oslider.x + oslider.wd + 2
    local y = oht-oslider.ht-10 + int((oslider.ht-ht)/2)
    ovt{"fill", x, y, 50, ht}
    pastetext(x - originx, y - originy)
end

--------------------------------------------------------------------------------

local function do_slider(newval)
    -- called if oslider position has changed
    opacity = newval
    show_opacity()
    draw_plot()
end

--------------------------------------------------------------------------------

local function create_overlay()
    -- create overlay in middle of current layer
    ov("create "..owd.." "..oht)
    ov("position middle")

    -- create the Save and Cancel buttons
    op.textshadowx = 2
    op.textshadowy = 2
    sbutt = op.button("Save as PNG", do_save)
    cbutt = op.button("Cancel", g.exit)

    -- create a check box for showing lines or dots
    op.textshadowx = 0
    op.textshadowy = 0
    lbox = op.checkbox("Lines", op.black, toggle_lines)

    -- create a slider for adjusting opacity of background
    oslider = op.slider("Opacity:", op.black, 101, 0, 100, do_slider)

    controlht = 20 + sbutt.ht
end

--------------------------------------------------------------------------------

local function draw_controls()
    ov(bgcolor..255)
    ovt{"fill", 0, (oht-controlht), owd, controlht}

    -- show the Save and Cancel buttons at bottom right corner of overlay
    sbutt.show(owd-cbutt.wd-sbutt.wd-20, oht-sbutt.ht-10)
    cbutt.show(owd-cbutt.wd-10, oht-cbutt.ht-10)

    -- show the check box at bottom left corner of overlay
    lbox.show(10, oht-lbox.ht-10, lines)

    -- show slider to right of check box
    oslider.show(10+lbox.wd+70, oht-oslider.ht-10, opacity)

    show_opacity()
end

--------------------------------------------------------------------------------

function main()
    read_settings()
    run_pattern()
    create_overlay()
    draw_controls()
    draw_plot()
    -- wait for user to hit escape or click Cancel button
    while true do
        local event = op.process( g.getevent() )
        -- event is empty if op.process handled the given event (eg. button click)
        if #event > 0 then
            g.doevent(event)
        end
    end
end

--------------------------------------------------------------------------------

local status, err = xpcall(main, gp.trace)
if err then g.continue(err) end
-- the following code is always executed
ov("delete")
write_settings()
