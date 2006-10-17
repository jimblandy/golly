# Shift current selection by given x y amounts using optional mode.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2006.

from glife import getstring, validint, inside
import golly as g

# ------------------------------------------------------------------------------

def main ():
   selrect = g.getselrect()
   
   if len(selrect) == 0:
      g.error("There is no selection.")
      return
   
   answer = getstring("Enter x y amounts and optional mode (default is or):")
   xym = answer.split()
   
   if len(xym) == 0: return
   
   # extract x and y amounts
   if len(xym) == 1:
      g.error("You need to supply x and y amounts separated by a space.")
      return
   if not validint(xym[0]):
      g.error("Bad x value: " + xym[0])
      return
   if not validint(xym[1]):
      g.error("Bad y value: " + xym[1])
      return
   x = int(xym[0])
   y = int(xym[1])
   
   # extract optional mode
   if len(xym) > 2:
      mode = xym[2]
      if not (mode == "copy" or mode == "or" or mode == "xor"):
         g.error("Unknown mode: " + xym[2] + " (must be copy/or/xor)")
         return
   else:
      mode = "or"
   
   # this method cuts the current selection and pastes it into the
   # new position (without changing the current clipboard pattern)
   selcells = g.getcells(selrect)
   g.clear(inside)
   selrect[0] += x
   selrect[1] += y
   g.select(selrect)
   if mode == "or":
      g.putcells(selcells, x, y, 1, 0, 0, 1)
   elif mode == "copy":
      g.clear(inside)
      g.putcells(selcells, x, y, 1, 0, 0, 1)
   else:
      # xor is a bit trickier
      listlen = len(selcells)
      for i in xrange(0, listlen, 2):
         newx = selcells[i]   + x
         newy = selcells[i+1] + y
         g.setcell(newx, newy, 1 - g.getcell(newx, newy))
   
   if not g.visrect(selrect): g.fitsel()

# ------------------------------------------------------------------------------

main()
