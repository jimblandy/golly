-- Run the current pattern for a given number of steps (using current
-- step size) and create a plot of population vs time in an overlay.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Sep 2016.

local g = golly()
-- require "gplus.strict"

local gp = require "gplus"
local int = gp.int
local min = gp.min
local max = gp.max

local op = require "oplus"
local ov = g.overlay

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

local joindots = true

-- these controls are created in create_overlay
local sbutt, cbutt      -- Save and Cancel buttons
local jbox              -- check box for toggling joindots
local oslider           -- slider for adjusting opacity

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
    ov("paste "..(x+originx).." "..(y+originy).." textclip")
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
    ov("line "..(x1+originx).." "..(y1+originy).." "
              ..(x2+originx).." "..(y2+originy))
end

--------------------------------------------------------------------------------

local function run_pattern()
    if g.empty() then g.exit("There is no pattern.") end
    
    -- prompt user for number of steps
    local s = g.getstring("Enter the number of steps:", numsteps, "Population plotter")
    if string.len(s) == 0 then g.exit() end
    numsteps = tonumber(s)
    if numsteps <= 0 then g.exit() end
    
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
    -- fill overlay with background color
    ov(bgcolor..int(255*opacity/100+0.5))
    ov("fill") 
    
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
        if joindots then
            drawline(x, -y, newx, -newy)
        else
            drawline(newx, -newy, newx, -newy)
        end
        x = newx
        y = newy
        local newsecs = os.clock()
        if newsecs - oldsecs >= 1.0 then     -- update plot every second
            oldsecs = newsecs
            g.update()
        end
    end
    
    -- show the Save and Cancel buttons at bottom right corner of overlay
    sbutt.show(owd-cbutt.wd-sbutt.wd-20, oht-sbutt.ht-10)
    cbutt.show(owd-cbutt.wd-10, oht-cbutt.ht-10)
    
    -- show the check box at bottom left corner of overlay
    jbox.show(10, oht-jbox.ht-10, joindots)
    
    -- show slider at bottom middle of overlay
    oslider.show(int((owd-oslider.wd)/2)-40, oht-oslider.ht-10, opacity)
    
    -- show opacity % at right end of slider
    ov(textcolor)
    wd, ht = maketext(""..opacity.."%")
    pastetext(int((owd-oslider.wd)/2)-40 + oslider.wd + 2 - originx,
              oht-oslider.ht-10 + int((oslider.ht-ht)/2) - originy)
    
    g.update()
end

--------------------------------------------------------------------------------

local function do_save()
    -- called if Save button is clicked
    -- so prompt user to save overlay (minus bottom stuff) in a .png file
    g.note("Not yet implemented!!!")
end

--------------------------------------------------------------------------------

local function toggle_dots()
    -- called if check box is clicked
    joindots = not joindots
    draw_plot()
end

--------------------------------------------------------------------------------

local function do_slider(newval)
    -- called if oslider position has changed
    opacity = newval
    
    -- hide all controls so draw_plot will show them with the changed background
    sbutt.hide()
    cbutt.hide()
    jbox.hide()
    oslider.hide()
    
    draw_plot()
end

--------------------------------------------------------------------------------

local function create_overlay()
    -- create overlay in middle of current layer
    ov("create "..owd.." "..oht)
    ov("position middle")

    -- create the Save and Cancel buttons
    sbutt = op.button("Save as PNG", do_save)
    cbutt = op.button("Cancel", g.exit)
    
    -- create a check box for showing lines or dots
    jbox = op.checkbox("Join dots", op.black, toggle_dots)
    
    -- create a slider for adjusting opacity of background
    oslider = op.slider("Opacity:", op.black, 101, 0, 100, do_slider)
    
    g.setoption("showoverlay", 1)
end

--------------------------------------------------------------------------------

local function main()
    run_pattern()
    create_overlay()
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

local status, err = pcall(main)
if err then g.continue(err) end
-- the following code is always executed
ov("delete")
