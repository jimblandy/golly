-- Run the current pattern for a given number of steps (using current
-- step size) and create a plot of population vs time in separate layer.
-- Author: Andrew Trevorrow (andrewtrevorrow.com), Apr 2016.

local g = gollylib()

-- size of plot
local xlen = 500    -- length of x axis
local ylen = 500    -- length of y axis

-- create a mono-spaced ASCII font
local mfont = {}
mfont[' '] = g.parse("")
mfont['!'] = g.parse("2bo$2bo$2bo$2bo$2bo2$2bo!")
mfont['"'] = g.parse("bobo$bobo$bobo!")
mfont['#'] = g.parse("bobo$bobo$5o$bobo$5o$bobo$bobo!")
mfont['$'] = g.parse("b3o$obobo$obo$b3o$2bobo$obobo$b3o!")
mfont['%'] = g.parse("2o2bo$2o2bo$3bo$2bo$bo$o2b2o$o2b2o!")
mfont['&'] = g.parse("b2o$o2bo$o2bo$b2o$o2bo$o2bo$b2obo!")
mfont["'"] = g.parse("2bo$2bo$2bo!")
mfont['('] = g.parse("3bo$2bo$2bo$2bo$2bo$2bo$3bo!")
mfont[')'] = g.parse("bo$2bo$2bo$2bo$2bo$2bo$bo!")
mfont['*'] = g.parse("$obobo$b3o$5o$b3o$obobo!")
mfont['+'] = g.parse("$2bo$2bo$5o$2bo$2bo!")
mfont[','] = g.parse("6$2bo$2bo$bo!")
mfont['-'] = g.parse("3$5o!")
mfont['.'] = g.parse("6$2bo!")
mfont['/'] = g.parse("3bo$3bo$2bo$2bo$2bo$bo$bo!")
mfont['0'] = g.parse("b3o$o3bo$o2b2o$obobo$2o2bo$o3bo$b3o!")
mfont['1'] = g.parse("2bo$b2o$2bo$2bo$2bo$2bo$b3o!")
mfont['2'] = g.parse("b3o$o3bo$4bo$3bo$2bo$bo$5o!")
mfont['3'] = g.parse("b3o$o3bo$4bo$2b2o$4bo$o3bo$b3o!")
mfont['4'] = g.parse("3bo$2b2o$bobo$o2bo$5o$3bo$3bo!")
mfont['5'] = g.parse("5o$o$o$b3o$4bo$o3bo$b3o!")
mfont['6'] = g.parse("b3o$o$o$4o$o3bo$o3bo$b3o!")
mfont['7'] = g.parse("5o$4bo$3bo$2bo$bo$o$o!")
mfont['8'] = g.parse("b3o$o3bo$o3bo$b3o$o3bo$o3bo$b3o!")
mfont['9'] = g.parse("b3o$o3bo$o3bo$b4o$4bo$4bo$b3o!")
mfont[':'] = g.parse("2$2bo4$2bo!")
mfont[';'] = g.parse("2$2bo4$2bo$2bo$bo!")
mfont['<'] = g.parse("$3bo$2bo$bo$2bo$3bo!")
mfont['='] = g.parse("2$5o2$5o!")
mfont['>'] = g.parse("$bo$2bo$3bo$2bo$bo!")
mfont['?'] = g.parse("b3o$o3bo$4bo$2b2o$2bo2$2bo!")
mfont['@'] = g.parse("b3o$o3bo$ob3o$obobo$ob2o$o$b3o!")
mfont['A'] = g.parse("b3o$o3bo$o3bo$5o$o3bo$o3bo$o3bo!")
mfont['B'] = g.parse("4o$o3bo$o3bo$4o$o3bo$o3bo$4o!")
mfont['C'] = g.parse("b3o$o3bo$o$o$o$o3bo$b3o!")
mfont['D'] = g.parse("4o$o3bo$o3bo$o3bo$o3bo$o3bo$4o!")
mfont['E'] = g.parse("5o$o$o$3o$o$o$5o!")
mfont['F'] = g.parse("5o$o$o$3o$o$o$o!")
mfont['G'] = g.parse("b3o$o3bo$o$o2b2o$o3bo$o3bo$b3o!")
mfont['H'] = g.parse("o3bo$o3bo$o3bo$5o$o3bo$o3bo$o3bo!")
mfont['I'] = g.parse("b3o$2bo$2bo$2bo$2bo$2bo$b3o!")
mfont['J'] = g.parse("2b3o$3bo$3bo$3bo$3bo$o2bo$b2o!")
mfont['K'] = g.parse("o3bo$o2bo$obo$2o$obo$o2bo$o3bo!")
mfont['L'] = g.parse("o$o$o$o$o$o$5o!")
mfont['M'] = g.parse("o3bo$2ob2o$obobo$obobo$o3bo$o3bo$o3bo!")
mfont['N'] = g.parse("o3bo$2o2bo$obobo$o2b2o$o3bo$o3bo$o3bo!")
mfont['O'] = g.parse("b3o$o3bo$o3bo$o3bo$o3bo$o3bo$b3o!")
mfont['P'] = g.parse("4o$o3bo$o3bo$4o$o$o$o!")
mfont['Q'] = g.parse("b3o$o3bo$o3bo$o3bo$obobo$o2bo$b2obo!")
mfont['R'] = g.parse("4o$o3bo$o3bo$4o$o2bo$o3bo$o3bo!")
mfont['S'] = g.parse("b3o$o3bo$o$b3o$4bo$o3bo$b3o!")
mfont['T'] = g.parse("5o$2bo$2bo$2bo$2bo$2bo$2bo!")
mfont['U'] = g.parse("o3bo$o3bo$o3bo$o3bo$o3bo$o3bo$b3o!")
mfont['V'] = g.parse("o3bo$o3bo$o3bo$o3bo$o3bo$bobo$2bo!")
mfont['W'] = g.parse("o3bo$o3bo$o3bo$obobo$obobo$2ob2o$o3bo!")
mfont['X'] = g.parse("o3bo$o3bo$bobo$2bo$bobo$o3bo$o3bo!")
mfont['Y'] = g.parse("o3bo$o3bo$bobo$2bo$2bo$2bo$2bo!")
mfont['Z'] = g.parse("5o$4bo$3bo$2bo$bo$o$5o!")
mfont['['] = g.parse("2b2o$2bo$2bo$2bo$2bo$2bo$2b2o!")
mfont['\\'] = g.parse("bo$bo$2bo$2bo$2bo$3bo$3bo!")
mfont[']'] = g.parse("b2o$2bo$2bo$2bo$2bo$2bo$b2o!")
mfont['^'] = g.parse("2bo$bobo$o3bo!")
mfont['_'] = g.parse("6$5o!")
mfont['`'] = g.parse("o$bo!")
mfont['a'] = g.parse("2$b4o$o3bo$o3bo$o3bo$b4o!")
mfont['b'] = g.parse("o$o$4o$o3bo$o3bo$o3bo$4o!")
mfont['c'] = g.parse("2$b4o$o$o$o$b4o!")
mfont['d'] = g.parse("4bo$4bo$b4o$o3bo$o3bo$o3bo$b4o!")
mfont['e'] = g.parse("2$b3o$o3bo$5o$o$b4o!")
mfont['f'] = g.parse("2b2o$bo2bo$bo$3o$bo$bo$bo!")
mfont['g'] = g.parse("2$b3o$o3bo$o3bo$o3bo$b4o$4bo$b3o!")
mfont['h'] = g.parse("o$o$ob2o$2o2bo$o3bo$o3bo$o3bo!")
mfont['i'] = g.parse("$2bo2$2bo$2bo$2bo$2b2o!")
mfont['j'] = g.parse("$3bo2$3bo$3bo$3bo$3bo$o2bo$b2o!")
mfont['k'] = g.parse("o$o$o2bo$obo$3o$o2bo$o3bo!")
mfont['l'] = g.parse("b2o$2bo$2bo$2bo$2bo$2bo$2b2o!")
mfont['m'] = g.parse("2$bobo$obobo$obobo$o3bo$o3bo!")
mfont['n'] = g.parse("2$4o$o3bo$o3bo$o3bo$o3bo!")
mfont['o'] = g.parse("2$b3o$o3bo$o3bo$o3bo$b3o!")
mfont['p'] = g.parse("2$4o$o3bo$o3bo$o3bo$4o$o$o!")
mfont['q'] = g.parse("2$b4o$o3bo$o3bo$o3bo$b4o$4bo$4bo!")
mfont['r'] = g.parse("2$ob2o$2o2bo$o$o$o!")
mfont['s'] = g.parse("2$b4o$o$b3o$4bo$4o!")
mfont['t'] = g.parse("$2bo$5o$2bo$2bo$2bo$3b2o!")
mfont['u'] = g.parse("2$o3bo$o3bo$o3bo$o3bo$b4o!")
mfont['v'] = g.parse("2$o3bo$o3bo$o3bo$bobo$2bo!")
mfont['w'] = g.parse("2$o3bo$o3bo$obobo$2ob2o$o3bo!")
mfont['x'] = g.parse("2$o3bo$bobo$2bo$bobo$o3bo!")
mfont['y'] = g.parse("2$o3bo$o3bo$o3bo$o3bo$b4o$4bo$b3o!")
mfont['z'] = g.parse("2$5o$3bo$2bo$bo$5o!")
mfont['{'] = g.parse("3bo$2bo$2bo$bo$2bo$2bo$3bo!")
mfont['|'] = g.parse("2bo$2bo$2bo$2bo$2bo$2bo$2bo!")
mfont['}'] = g.parse("bo$2bo$2bo$3bo$2bo$2bo$bo!")
mfont['~'] = g.parse("2$bo$obobo$3bo!")

--------------------------------------------------------------------------------

-- return a rect which is the minimal bounding box of the given pattern
-- note that the pattern is two-state so we don't have to worry about
-- getting a multi-state cell array here

function getminbox(cells)
    local len = #cells
    if len < 2 then return {} end
    
    local minx = cells[1]
    local miny = cells[2]
    local maxx = minx
    local maxy = miny
    for x = 1, len, 2 do
        if cells[x] < minx then minx = cells[x] end
        if cells[x] > maxx then maxx = cells[x] end
    end
    for y = 2, len, 2 do
        if cells[y] < miny then miny = cells[y] end
        if cells[y] > maxy then maxy = cells[y] end
    end
    
    return {minx, miny, maxx - minx + 1, maxy - miny + 1}
end

--------------------------------------------------------------------------------

-- convert given string to a cell array using above mono-spaced font
-- and also return the array's width and height

function make_text(s)
    local p = {}
    local x = 0
    for ch in string.gmatch(s, ".[\128-\191]*") do
        if not mfont[ch] then ch = '?' end
        local symbol = g.transform(mfont[ch], x, 0)
        p = g.join(p, symbol)
        x = x + 6
    end
    local tbox = getminbox(p)
    return p, tbox[3], tbox[4]
end

--------------------------------------------------------------------------------

-- draw a line of cells from x1,y1 to x2,y2 using Bresenham's algorithm

function draw_line(x1, y1, x2, y2)
    g.setcell(x1, y1, 1)
    if x1 == x2 and y1 == y2 then return end
    
    local dx = x2 - x1
    local ax = math.abs(dx) * 2
    local sx = 1
    if dx < 0 then sx = -1 end
    
    local dy = y2 - y1
    local ay = math.abs(dy) * 2
    local sy = 1
    if dy < 0 then sy = -1 end
    
    if ax > ay then
        local d = ay - (ax / 2)
        while x1 ~= x2 do
            g.setcell(x1, y1, 1)
            if d >= 0 then
                y1 = y1 + sy
                d = d - ax
            end
            x1 = x1 + sx
            d = d + ay
        end
    else
        local d = ax - (ay / 2)
        while y1 ~= y2 do
            g.setcell(x1, y1, 1)
            if d >= 0 then
                x1 = x1 + sx
                d = d - ay
            end
            y1 = y1 + sy
            d = d + ax
        end
    end
    g.setcell(x2, y2, 1)
end

--------------------------------------------------------------------------------

-- fit pattern in viewport if not empty and not completely visible

function fit_if_not_visible()
    local r = g.getrect()
    if #r > 0 and not g.visrect(r) then g.fit() end
end

--------------------------------------------------------------------------------

-- return minimum value in given array

function min(a)
    if #a == 0 then return nil end
    local m = a[1]
    for i = 2, #a do
        if a[i] < m then m = a[i] end
    end
    return m
end

--------------------------------------------------------------------------------

-- return maximum value in given array

function max(a)
    if #a == 0 then return nil end
    local m = a[1]
    for i = 2, #a do
        if a[i] > m then m = a[i] end
    end
    return m
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
local poplist = { tonumber(g.getpop()) }
local genlist = { tonumber(g.getgen()) }
local oldsecs = os.clock()
for i = 0, numsteps-1 do
    g.step()
    poplist[#poplist+1] = tonumber(g.getpop())
    genlist[#genlist+1] = tonumber(g.getgen())
    local newsecs = os.clock()
    if newsecs - oldsecs >= 1.0 then     -- show pattern every second
        oldsecs = newsecs
        fit_if_not_visible()
        g.update()
        g.show(string.format("Step %d of %d", i+1, numsteps))
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

local minpop = min(poplist)
local maxpop = max(poplist)
if minpop == maxpop then
    -- avoid division by zero
    minpop = minpop - 1
end
local popscale = (maxpop - minpop) / ylen

local mingen = min(genlist)
local maxgen = max(genlist)
local genscale = (maxgen - mingen) / xlen

-- draw axes with origin at 0,0
draw_line(0, 0, xlen, 0)
draw_line(0, 0, 0, -ylen)

-- add annotation using mono-spaced ASCII font
local t, wd, ht = make_text(string.upper(pattname))
g.putcells(t, math.floor((xlen - wd) / 2), -ylen - 10 - ht)

t, wd, ht = make_text("POPULATION")
g.putcells(t, -10 - ht, math.floor(-(ylen - wd) / 2), 0, 1, -1, 0)

t, wd, ht = make_text(""..minpop)
g.putcells(t, -wd - 10, math.floor(-ht / 2))

t, wd, ht = make_text(""..maxpop)
g.putcells(t, -wd - 10, -ylen - math.floor(ht / 2))

t, wd, ht = make_text("GENERATION (step="..stepsize..")")
g.putcells(t, math.floor((xlen - wd) / 2), 10)

t, wd, ht = make_text(""..mingen)
g.putcells(t, math.floor(-wd / 2), 10)

t, wd, ht = make_text(""..maxgen)
g.putcells(t, xlen - math.floor(wd / 2), 10)

-- display result at scale 1:1
g.fit()
g.setmag(0)
g.show("")

-- plot the data (do last because it could take a while if numsteps is huge)
local x = math.floor((genlist[1] - mingen) / genscale)
local y = math.floor((poplist[1] - minpop) / popscale)
oldsecs = os.clock()
for i = 1, numsteps-1 do
    local newx = math.floor((genlist[i+1] - mingen) / genscale)
    local newy = math.floor((poplist[i+1] - minpop) / popscale)
    draw_line(x, -y, newx, -newy)
    x = newx
    y = newy
    local newsecs = os.clock()
    if newsecs - oldsecs >= 1.0 then     -- show pattern every second
        oldsecs = newsecs
        g.update()
    end
end
