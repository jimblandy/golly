# Fill clicked region with current drawing state.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Jan 2011.

import golly as g
from time import time

# avoid an unbounded fill
if g.empty():
    if g.getwidth() == 0 or g.getheight() == 0:
        g.exit("You cannot fill an empty universe that is unbounded!")
else:
    # set fill limits to the pattern's bounding box
    # (these will be extended below if the grid is bounded)
    r = g.getrect()
    minx = r[0]
    miny = r[1]
    maxx = minx + r[2] - 1
    maxy = miny + r[3] - 1

# allow filling to extend to the edges of bounded grid
if g.getwidth() > 0:
    minx = -int(g.getwidth()/2)
    maxx = minx + g.getwidth() - 1
if g.getheight() > 0:
    miny = -int(g.getheight()/2)
    maxy = miny + g.getheight() - 1

# ------------------------------------------------------------------------------

def checkneighbor(x, y, oldstate):
    # first check if x,y is outside fill limits
    if x < minx or x > maxx: return False
    if y < miny or y > maxy: return False
    return g.getcell(x, y) == oldstate

# ------------------------------------------------------------------------------

def floodfill():
    newstate = g.getoption("drawingstate")
    oldstate = newstate

    # wait for user to click a cell
    g.show("Click the region you wish to fill... (hit escape to abort)")
    while oldstate == newstate:
        event = g.getevent()
        if event.startswith("click"):
            # event is a string like "click 10 20 left none"
            evt, xstr, ystr, butt, mods = event.split()
            x = int(xstr)
            y = int(ystr)
            if x < minx or x > maxx or y < miny or y > maxy:
                # click is outside pattern's bounding box in unbounded universe
                g.warn("Click within the pattern's bounding box\n"+
                       "otherwise the fill will be unbounded.")
            else:
                # note that user might have changed drawing state
                newstate = g.getoption("drawingstate")
                oldstate = g.getcell(x, y)
                if oldstate == newstate:
                    g.warn("The clicked cell must have a different state\n"+
                           "to the current drawing state.")
        else:
            g.doevent(event)

    # tell Golly to handle all further keyboard/mouse events
    g.getevent(False)

    # do flood fill starting with clicked cell
    g.show("Filling clicked region... (hit escape to stop)")
    clist = [ (x,y) ]
    g.setcell(x, y, newstate)
    oldsecs = time()
    while len(clist) > 0:
        # remove cell from start of clist
        x, y = clist.pop(0)
        newsecs = time()
        if newsecs - oldsecs >= 0.5:     # show changed pattern every half second
            oldsecs = newsecs
            g.update()

        # check if any orthogonal neighboring cells are in oldstate
        if checkneighbor(  x, y-1, oldstate):
            g.setcell(     x, y-1, newstate)
            clist.append( (x, y-1) )
        
        if checkneighbor(  x, y+1, oldstate):
            g.setcell(     x, y+1, newstate)
            clist.append( (x, y+1) )
        
        if checkneighbor(  x+1, y, oldstate):
            g.setcell(     x+1, y, newstate)
            clist.append( (x+1, y) )
        
        if checkneighbor(  x-1, y, oldstate):
            g.setcell(     x-1, y, newstate)
            clist.append( (x-1, y) )

        # diagonal neighbors are more complicated because we don't
        # want to cross a diagonal line of live cells
        if checkneighbor(  x+1, y+1, oldstate) and (g.getcell(x, y+1) == 0 or
                                                   g.getcell(x+1, y) == 0):
            g.setcell(     x+1, y+1, newstate)
            clist.append( (x+1, y+1) )
        
        if checkneighbor(  x+1, y-1, oldstate) and (g.getcell(x, y-1) == 0 or
                                                   g.getcell(x+1, y) == 0):
            g.setcell(     x+1, y-1, newstate)
            clist.append( (x+1, y-1) )
        
        if checkneighbor(  x-1, y+1, oldstate) and (g.getcell(x, y+1) == 0 or
                                                   g.getcell(x-1, y) == 0):
            g.setcell(     x-1, y+1, newstate)
            clist.append( (x-1, y+1) )
        
        if checkneighbor(  x-1, y-1, oldstate) and (g.getcell(x, y-1) == 0 or
                                                   g.getcell(x-1, y) == 0):
            g.setcell(     x-1, y-1, newstate)
            clist.append( (x-1, y-1) )

# ------------------------------------------------------------------------------

oldcursor = g.getcursor()
g.setcursor("Draw")

try:
    floodfill()
finally:
    g.setcursor(oldcursor)
    g.show(" ")
