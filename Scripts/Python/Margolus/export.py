# Change the selected area from emulated-Margolus states to N-state:

from glife import rect
import golly as g

r = rect( g.getrect() )

for row in xrange(r.top, r.top + r.height):
   for col in xrange(r.left, r.left + r.width):
         s = g.getcell(col,row)
         g.setcell(col, row, (s+s%2)/2-1)

