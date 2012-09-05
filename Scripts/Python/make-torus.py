# Use the current selection to create a toroidal universe.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Sept 2010.

from glife import inside, outside
import golly as g

selrect = g.getselrect()
if len(selrect) == 0: g.exit("There is no selection.")
x =  selrect[0]
y =  selrect[1]
wd = selrect[2]
ht = selrect[3]

selcells = g.getcells(selrect)
if not g.empty():
    g.clear(inside)
    g.clear(outside)

# get current rule, remove any existing suffix, then add new suffix
rule = g.getrule().split(":")[0]
g.setrule(rule + ":T" + str(wd) + "," + str(ht))

newx = -int(wd/2)
newy = -int(ht/2)
selrect[0] = newx
selrect[1] = newy
g.select(selrect)
if len(selcells) > 0: g.putcells(selcells, newx - x, newy - y)
g.fitsel()
