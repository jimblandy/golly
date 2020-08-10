# Run the current pattern for a given number of steps (using current
# step size) and create a plot of population vs time in separate layer.
# Author: Andrew Trevorrow (andrew@trevorrow.com), May 2007.

import golly as g
from glife import getminbox, rect, rccw
from glife.text import make_text
from time import time

# --------------------------------------------------------------------

# size of plot
xlen = 500        # length of x axis
ylen = 500        # length of y axis

# --------------------------------------------------------------------

# draw a line of cells from x1,y1 to x2,y2 using Bresenham's algorithm
def draw_line(x1, y1, x2, y2):
    g.setcell(x1, y1, 1)
    if x1 == x2 and y1 == y2: return

    dx = x2 - x1
    ax = abs(dx) * 2
    sx = 1
    if dx < 0: sx = -1
    dy = y2 - y1
    ay = abs(dy) * 2
    sy = 1
    if dy < 0: sy = -1

    if ax > ay:
        d = ay - (ax // 2)
        while x1 != x2:
            g.setcell(x1, y1, 1)
            if d >= 0:
                y1 += sy
                d -= ax
            x1 += sx
            d += ay
    else:
        d = ax - (ay // 2)
        while y1 != y2:
            g.setcell(x1, y1, 1)
            if d >= 0:
                x1 += sx
                d -= ay
            y1 += sy
            d += ax

    g.setcell(x2, y2, 1)

# --------------------------------------------------------------------

# fit pattern in viewport if not empty and not completely visible
def fit_if_not_visible():
    try:
        r = rect(g.getrect())
        if (not r.empty) and (not r.visible()): g.fit()
    except:
        # getrect failed because pattern is too big
        g.fit()

# --------------------------------------------------------------------

if g.empty(): g.exit("There is no pattern.")

# check that a layer is available for population plot
layername = "population plot"
poplayer = -1
for i in range(g.numlayers()):
    if g.getname(i) == layername:
        poplayer = i
        break
if poplayer == -1 and g.numlayers() == g.maxlayers():
    g.exit("You need to delete a layer.")

# prompt user for number of steps
numsteps = xlen
s = g.getstring("Enter the number of steps:",
                str(numsteps), "Population plotter")
if len(s) > 0: numsteps = int(s)
if numsteps <= 0: g.exit()

# generate pattern for given number of steps
poplist = [ int(g.getpop()) ]
genlist = [ int(g.getgen()) ]
oldsecs = time()
for i in range(numsteps):
    g.step()
    poplist.append( int(g.getpop()) )
    genlist.append( int(g.getgen()) )
    newsecs = time()
    if newsecs - oldsecs >= 1.0:     # show pattern every second
        oldsecs = newsecs
        fit_if_not_visible()
        g.update()
        g.show("Step %i of %i" % (i+1, numsteps))

fit_if_not_visible()

# save some info before we switch layers
stepsize = "%i^%i" % (g.getbase(), g.getstep())
pattname = g.getname()

# create population plot in separate layer
g.setoption("stacklayers", 0)
g.setoption("tilelayers", 0)
g.setoption("showlayerbar", 1)
if poplayer == -1:
    poplayer = g.addlayer()
else:
    g.setlayer(poplayer)
g.new(layername)

# use QuickLife with an unbounded grid
g.setalgo("QuickLife")
g.setrule("B3/S23")

deadr, deadg, deadb = g.getcolor("deadcells")
if (deadr + deadg + deadb) // 3 > 128:
    # use black if light background
    g.setcolors([1,0,0,0])
else:
    # use white if dark background
    g.setcolors([1,255,255,255])

minpop = min(poplist)
maxpop = max(poplist)
if minpop == maxpop:
    # avoid division by zero
    minpop -= 1
popscale = float(maxpop - minpop) / float(ylen)

mingen = min(genlist)
maxgen = max(genlist)
genscale = float(maxgen - mingen) / float(xlen)

# draw axes with origin at 0,0
draw_line(0, 0, xlen, 0)
draw_line(0, 0, 0, -ylen)

# add annotation using mono-spaced ASCII font
t = make_text(pattname.upper(), "mono")
bbox = getminbox(t)
t.put((xlen - bbox.wd) // 2, -ylen - 10 - bbox.ht)

t = make_text("POPULATION", "mono")
bbox = getminbox(t)
t.put(-10 - bbox.ht, -(ylen - bbox.wd) // 2, rccw)

t = make_text(str(minpop), "mono")
bbox = getminbox(t)
t.put(-bbox.wd - 10, -bbox.ht // 2)

t = make_text(str(maxpop), "mono")
bbox = getminbox(t)
t.put(-bbox.wd - 10, -ylen - bbox.ht // 2)

t = make_text("GENERATION (step=%s)" % stepsize, "mono")
bbox = getminbox(t)
t.put((xlen - bbox.wd) // 2, 10)

t = make_text(str(mingen), "mono")
bbox = getminbox(t)
t.put(-bbox.wd // 2, 10)

t = make_text(str(maxgen), "mono")
bbox = getminbox(t)
t.put(xlen - bbox.wd // 2, 10)

# display result at scale 1:1
g.fit()
g.setmag(0)
g.show("")

# plot the data (do last because it could take a while if numsteps is huge)
x = int(float(genlist[0] - mingen) / genscale)
y = int(float(poplist[0] - minpop) / popscale)
oldsecs = time()
for i in range(numsteps):
    newx = int(float(genlist[i+1] - mingen) / genscale)
    newy = int(float(poplist[i+1] - minpop) / popscale)
    draw_line(x, -y, newx, -newy)
    x = newx
    y = newy
    newsecs = time()
    if newsecs - oldsecs >= 1.0:     # update plot every second
        oldsecs = newsecs
        g.update()
