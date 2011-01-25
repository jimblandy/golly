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

def rectingrid(r):
   # return True if all of given rectangle is inside grid
   if g.getwidth() > 0:
      # check if r is outside left/right edge of grid
      gridl = -int(g.getwidth()/2)
      gridr = gridl + g.getwidth() - 1
      if r[0] < gridl or r[0] + r[2] - 1 > gridr: return False
   if g.getheight() > 0:
      # check if r is outside top/bottom edge of grid
      gridt = -int(g.getheight()/2)
      gridb = gridt + g.getheight() - 1
      if r[1] < gridt or r[1] + r[3] - 1 > gridb: return False
   return True

# ------------------------------------------------------------------------------

def lookforkeys(event, deltax, deltay):
   global oldcells, selrect, selpatt
   
   # look for keys used to flip/rotate selection
   if event == "key x none" or event == "key y none":
      # flip floating selection left-right or top-bottom
      if len(oldcells) > 0:
         g.clear(0)
         g.putcells(selpatt, deltax, deltay)
      if " x " in event:
         g.flip(0)
      else:
         g.flip(1)
      selpatt = g.transform(g.getcells(selrect), -deltax, -deltay)
      if len(oldcells) > 0:
         g.clear(0)
         g.putcells(oldcells)
         g.putcells(selpatt, deltax, deltay)
      g.update()
      return
   
   if event == "key > none" or event == "key < none":
      # rotate floating selection clockwise or anticlockwise
      midx = selrect[0] + int(selrect[2]/2)
      midy = selrect[1] + int(selrect[3]/2)
      newleft = midx + selrect[1] - midy
      newtop = midy + selrect[0] - midx
      rotrect = [ newleft, newtop, selrect[3], selrect[2] ]
      if not rectingrid(rotrect):
         g.warn("Rotation is not allowed if selection would be outside grid.")
         return
      g.clear(0)
      if len(oldcells) > 0: g.putcells(oldcells)
      oldcells = oldcells + g.getcells(rotrect)
      g.clear(0)
      g.select(rotrect)
      g.clear(0)
      g.select(selrect)
      g.putcells(selpatt, deltax, deltay)
      if " > " in event:
         g.rotate(0)
      else:
         g.rotate(1)
      selrect = g.getselrect()
      selpatt = g.transform(g.getcells(selrect), -deltax, -deltay)
      if len(oldcells) > 0:
         g.clear(0)
         g.putcells(oldcells)
         g.putcells(selpatt, deltax, deltay)
      g.update()
      return
   
   g.doevent(event)

# ------------------------------------------------------------------------------

def moveselection():
   global oldcells, selrect, selpatt
   
   # wait for 1st click in selection
   while True:
      event = g.getevent()
      if event.startswith("click"):
         # event is a string like "click 10 20 left none"
         evt, xstr, ystr, butt, mods = event.split()
         x = int(xstr)
         y = int(ystr)
         if cellinrect(x, y, selrect):
            oldmouse = xstr + ' ' + ystr
            firstx = x
            firsty = y
            xoffset = firstx - selrect[0]
            yoffset = firsty - selrect[1]
            if mods == "alt":
               # don't delete pattern in selection
               oldcells = g.getcells(selrect)
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
         if len(event) > 0:
            lookforkeys(event, x - firstx, y - firsty)
            # update xoffset,yoffset in case selection was rotated
            xoffset = x - selrect[0]
            yoffset = y - selrect[1]
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
            if selrect[0] < gridl:
               selrect[0] = gridl
               x = selrect[0] + xoffset
            elif selrect[0] + selrect[2] - 1 > gridr:
               selrect[0] = gridr + 1 - selrect[2]
               x = selrect[0] + xoffset
         if g.getheight() > 0:
            # ensure selrect doesn't move beyond top/bottom edge of grid
            gridt = -int(g.getheight()/2)
            gridb = gridt + g.getheight() - 1
            if selrect[1] < gridt:
               selrect[1] = gridt
               y = selrect[1] + yoffset
            elif selrect[1] + selrect[3] - 1 > gridb:
               selrect[1] = gridb + 1 - selrect[3]
               y = selrect[1] + yoffset
         
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
firstpatt = g.getcells(selrect)

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
      g.putcells(firstpatt)
      g.select(firstrect)
   else:
      g.show(" ")
