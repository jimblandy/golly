-- Run the current pattern for a given number of steps (using current
-- step size) and create a plot of population vs time in separate layer.
-- Author: Andrew Trevorrow (andrewtrevorrow.com), Apr 2016.

local g = golly()

local gp = require "gplus"
local int = gp.int
local min = gp.min
local max = gp.max
local drawline = gp.drawline
local gpt = require "gplus.text"

-- size of plot
local xlen = 500    -- length of x axis
local ylen = 500    -- length of y axis

--------------------------------------------------------------------------------

local function monotext(s)
    -- convert given string to a pattern in a mono-spaced font and return
    -- its cell array and its minimal width and height
    local p = gpt.maketext(s, "mono")
    local r = gp.getminbox(p)
    return p.array, r.wd, r.ht
end

--------------------------------------------------------------------------------

local function fit_if_not_visible()
    -- fit pattern in viewport if not empty and not completely visible
    local r = g.getrect()
    if #r > 0 and not g.visrect(r) then g.fit() end
end

--------------------------------------------------------------------------------

if g.empty() then g.exit("There is no pattern.") end

-- check that a layer is available for population plot
local layername = "population plot"
local poplayer = -1
for i = 0, g.numlayers()-1 do
    if g.getname(i) == layername then
        poplayer = i
        break
    end
end
if poplayer == -1 and g.numlayers() == g.maxlayers() then
    g.exit("You need to delete a layer.")
end

-- prompt user for number of steps
local s = g.getstring("Enter the number of steps:", xlen, "Population plotter")
if string.len(s) == 0 then g.exit() end
local numsteps = tonumber(s)
if numsteps <= 0 then g.exit() end

-- generate pattern for given number of steps
local pops = { tonumber(g.getpop()) }
local gens = { tonumber(g.getgen()) }
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

-- save some info before we switch layers
local stepsize = string.format("%d^%d", g.getbase(), g.getstep())
local pattname = g.getname()

-- create population plot in separate layer
g.setoption("stacklayers", 0)
g.setoption("tilelayers", 0)
g.setoption("showlayerbar", 1)
if poplayer == -1 then
    poplayer = g.addlayer()
else
    g.setlayer(poplayer)
end
g.new(layername)

-- use same rule but without any suffix (we don't want a bounded grid)
local rule = g.getrule()
rule = rule:match("^(.+):") or rule
g.setrule(rule)

local deadr, deadg, deadb = g.getcolor("deadcells")
if (deadr + deadg + deadb) / 3 > 128 then
    -- use black if light background
    g.setcolors({1,0,0,0})
else
    -- use white if dark background
    g.setcolors({1,255,255,255})
end

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

-- draw axes with origin at 0,0
drawline(0, 0, xlen, 0)
drawline(0, 0, 0, -ylen)

-- add annotation using mono-spaced ASCII font
local t, wd, ht = monotext(string.upper(pattname))
g.putcells(t, int((xlen - wd) / 2), -ylen - 10 - ht)

t, wd, ht = monotext("POPULATION")
g.putcells(t, -10 - ht, int(-(ylen - wd) / 2), 0, 1, -1, 0)

t, wd, ht = monotext(""..minpop)
g.putcells(t, -wd - 10, int(-ht / 2))

t, wd, ht = monotext(""..maxpop)
g.putcells(t, -wd - 10, -ylen - int(ht / 2))

t, wd, ht = monotext("GENERATION (step="..stepsize..")")
g.putcells(t, int((xlen - wd) / 2), 10)

t, wd, ht = monotext(""..mingen)
g.putcells(t, int(-wd / 2), 10)

t, wd, ht = monotext(""..maxgen)
g.putcells(t, xlen - int(wd / 2), 10)

-- display result at scale 1:1
g.fit()
g.setmag(0)
g.show("")

-- plot the data (do last because it could take a while if numsteps is huge)
local x = int((gens[1] - mingen) / genscale)
local y = int((pops[1] - minpop) / popscale)
oldsecs = os.clock()
for i = 1, numsteps do
    local newx = int((gens[i+1] - mingen) / genscale)
    local newy = int((pops[i+1] - minpop) / popscale)
    drawline(x, -y, newx, -newy)
    x = newx
    y = newy
    local newsecs = os.clock()
    if newsecs - oldsecs >= 1.0 then     -- show pattern every second
        oldsecs = newsecs
        g.update()
    end
end
