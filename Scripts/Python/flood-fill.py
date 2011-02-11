# Fill clicked region with current drawing state.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Jan 2011.

import golly as g
from time import time

# set edges of bounded grid for later use
if g.getwidth() > 0:
   gridl = -int(g.getwidth()/2)
   gridr = gridl + g.getwidth() - 1
if g.getheight() > 0:
   gridt = -int(g.getheight()/2)
   gridb = gridt + g.getheight() - 1

# ------------------------------------------------------------------------------

def checkneighbor(x, y, oldstate):
   # first check if x,y is outside bounded grid
   if g.getwidth() > 0 and (x < gridl or x > gridr): return False
   if g.getheight() > 0 and (y < gridt or y > gridb): return False
   return g.getcell(x, y) == oldstate

# ------------------------------------------------------------------------------

def floodfill():
   newstate = g.getoption("drawingstate")
   oldstate = newstate
   
   # wait for click
   g.show("Click the region you wish to fill... (hit escape to abort)")
   while oldstate == newstate:
      event = g.getevent()
      if event.startswith("click"):
         # event is a string like "click 10 20 left none"
         evt, xstr, ystr, butt, mods = event.split()
         x = int(xstr)
         y = int(ystr)
         # user might have changed drawing state
         newstate = g.getoption("drawingstate")
         oldstate = g.getcell(x, y)
         if oldstate == newstate:
            g.warn("The clicked cell must have a different state\n"+
                   "to the current drawing state.")
      else:
         g.doevent(event)

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
      if checkneighbor( x, y-1, oldstate):
         clist.append( (x, y-1) )
         g.setcell(     x, y-1, newstate)
      if checkneighbor( x, y+1, oldstate):
         clist.append( (x, y+1) )
         g.setcell(     x, y+1, newstate)
      if checkneighbor( x+1, y, oldstate):
         clist.append( (x+1, y) )
         g.setcell(     x+1, y, newstate)
      if checkneighbor( x-1, y, oldstate):
         clist.append( (x-1, y) )
         g.setcell(     x-1, y, newstate)
      
      # diagonal neighbors are more complicated because we don't
      # want to cross a diagonal line of live cells
      if checkneighbor( x+1, y+1, oldstate) and (g.getcell(x, y+1) == 0 or
                                                 g.getcell(x+1, y) == 0):
         clist.append( (x+1, y+1) )
         g.setcell(     x+1, y+1, newstate)
      if checkneighbor( x+1, y-1, oldstate) and (g.getcell(x, y-1) == 0 or
                                                 g.getcell(x+1, y) == 0):
         clist.append( (x+1, y-1) )
         g.setcell(     x+1, y-1, newstate)
      if checkneighbor( x-1, y+1, oldstate) and (g.getcell(x, y+1) == 0 or
                                                 g.getcell(x-1, y) == 0):
         clist.append( (x-1, y+1) )
         g.setcell(     x-1, y+1, newstate)
      if checkneighbor( x-1, y-1, oldstate) and (g.getcell(x, y-1) == 0 or
                                                 g.getcell(x-1, y) == 0):
         clist.append( (x-1, y-1) )
         g.setcell(     x-1, y-1, newstate)
      
      g.doevent(g.getevent())

# ------------------------------------------------------------------------------

oldcursor = g.getcursor()
g.setcursor("Draw")

try:
   floodfill()
finally:
   g.setcursor(oldcursor)
   g.show(" ")
