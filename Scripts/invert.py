# Inverts all cell states in the current selection.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2006.

from glife import rect
from time import time
import golly as g

r = rect( g.getselrect() )

if r.empty:
   g.error("There is no selection.")

else:
   oldsecs = time()
   for row in xrange(r.top, r.top + r.height):
      newsecs = time()
      if newsecs - oldsecs >= 1.0:
         # when inverting large selections its nicer to
         # give some indication of progress
         oldsecs = newsecs
         if not r.visible(): g.fitsel()
         g.update()
      for col in xrange(r.left, r.left + r.width):
         g.setcell(col, row, 1 - g.getcell(col, row))

   if not r.visible(): g.fitsel()
