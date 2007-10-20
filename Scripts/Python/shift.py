# Shift current selection by given x y amounts using optional mode.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2006.
# Updated to use exit command, Nov 2006.

from glife import validint, inside
from string import lower
import golly as g

selrect = g.getselrect()
if len(selrect) == 0: g.exit("There is no selection.")

answer = g.getstring("Enter x y shift amounts and an optional mode\n" +
                     "(valid modes are copy/or/xor, default is or):",
                     "0 0 or",
                     "Shift selection")
xym = answer.split()

# extract x and y amounts
if len(xym) == 0: g.exit()
if len(xym) == 1: g.exit("Supply x and y amounts separated by a space.")
if not validint(xym[0]): g.exit("Bad x value: " + xym[0])
if not validint(xym[1]): g.exit("Bad y value: " + xym[1])
x = int(xym[0])
y = int(xym[1])

# extract optional mode
if len(xym) > 2:
   mode = lower(xym[2])
   if mode=="c": mode="copy"
   if mode=="o": mode="or"
   if mode=="x": mode="xor"
   if not (mode == "copy" or mode == "or" or mode == "xor"):
      g.exit("Unknown mode: " + xym[2] + " (must be copy/or/xor)")
else:
   mode = "or"

# this method cuts the current selection and pastes it into the
# new position (without changing the current clipboard pattern)
selcells = g.getcells(selrect)
g.clear(inside)
selrect[0] += x
selrect[1] += y
g.select(selrect)
if mode == "copy":
   g.clear(inside)
g.putcells(selcells, x, y, 1, 0, 0, 1, mode)

if not g.visrect(selrect): g.fitsel()
