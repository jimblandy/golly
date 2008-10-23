# Change the selected area from simulated-Margolus states to two-state:
# 0 - outside the area
# 1 - cell present, not currently the top-left corner of a partition
# 2 - empty cell, not currently the top-left corner of a partition
# 3 - cell present, currently the top-left corner of a partition
# 4 - empty cell, currently the top-left corner of a partition

from glife import rect
import golly as g

r = rect( g.getselrect() )
if r.empty: g.exit("There is no selection.")

for row in xrange(r.top, r.top + r.height):
   for col in xrange(r.left, r.left + r.width):
         g.setcell(col, row, g.getcell(col,row)%2)

if not r.visible(): g.fitsel()
