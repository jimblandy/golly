# Inverts all cell states in the current selection.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2006.

from glife import rect
import golly as g

r = rect( g.getselrect() )

if r.empty:
   g.error("There is no selection.")

else:
   for row in xrange(r.top, r.top + r.height):
      for col in xrange(r.left, r.left + r.width):
         g.setcell(col, row, 1 - g.getcell(col, row))

   if not r.visible(): g.fitsel()
