# Allow user to move the current selection.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Jan 2011.

import golly as g

# ------------------------------------------------------------------------------

def cellinrect(x, y, r):
   # return True if given cell position is inside given rectangle
   if x < r[0] or x >= r[0] + r[2]: return False
   if y < r[1] or y >= r[1] + r[3]: return False
   return True

# ------------------------------------------------------------------------------

def moveselection():
   global oldcells, selrect, selpatt
   
   # wait for 1st click in selection
   while True:
      event = g.getevent()
      if event.startswith("click"):
         # event is a string like "click 10 20 left none"
         evt, x, y, butt, mods = event.split()
         if cellinrect(int(x), int(y), selrect):
            oldmouse = x+' '+y
            firstx = int(x)
            firsty = int(y)
            xoffset = firstx - selrect[0]
            yoffset = firsty - selrect[1]
            break
      else:
         if len(event) > 0: g.doevent(event)
   
   # wait for 2nd click while moving selection
   g.show("Move mouse and click again...")
   gotclick = False
   while not gotclick:
      event = g.getevent()
      if event.startswith("click"):
         evt, x, y, butt, mods = event.split()
         mousepos = x+' '+y
         gotclick = True
      else:
         if len(event) > 0: g.doevent(event)
         mousepos = g.getxy()
      if len(mousepos) > 0 and mousepos != oldmouse:
         # mouse has moved, so move selection rect and pattern
         g.clear(0)
         if len(oldcells) > 0: g.putcells(oldcells)
         xstr, ystr = mousepos.split()
         x = int(xstr)
         y = int(ystr)
         selrect[0] = x - xoffset
         selrect[1] = y - yoffset
         
         if g.getwidth() > 0:
            # ensure selrect doesn't move beyond left/right edge of grid
            gridl = -int(g.getwidth()/2)
            gridr = gridl + g.getwidth() - 1
            while selrect[0] < gridl:
               selrect[0] += 1
               x += 1
            while selrect[0] + selrect[2] - 1 > gridr:
               selrect[0] -= 1
               x -= 1
         if g.getheight() > 0:
            # ensure selrect doesn't move beyond top/bottom edge of grid
            gridt = -int(g.getheight()/2)
            gridb = gridt + g.getheight() - 1
            while selrect[1] < gridt:
               selrect[1] += 1
               y += 1
            while selrect[1] + selrect[3] - 1 > gridb:
               selrect[1] -= 1
               y -= 1
         
         g.select(selrect)
         oldcells = g.getcells(selrect)
         g.putcells(selpatt, x - firstx, y - firsty)
         oldmouse = mousepos
         g.update()

# ------------------------------------------------------------------------------

selrect = g.getselrect()
if len(selrect) == 0: g.exit("There is no selection.")
selpatt = g.getcells(selrect)

# remember initial selection in case user aborts script
firstrect = g.getselrect()

g.show("Click anywhere in selection, move mouse and click again...")
oldcursor = g.getcursor()
g.setcursor("Move")
oldcells = []

try:
   aborted = True
   moveselection()
   aborted = False
finally:
   g.setcursor(oldcursor)
   if aborted:
      g.clear(0)
      if len(oldcells) > 0: g.putcells(oldcells)
      g.putcells(selpatt)
      g.select(firstrect)
   else:
      g.show(" ")
