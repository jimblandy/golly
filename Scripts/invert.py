# Invert all cell states in the current selection.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2006.
# Updated to use exit command, Nov 2006.

from glife import rect
from time import time
import golly as g

r = rect( g.getselrect() )

if r.empty: g.exit("There is no selection.")

oldsecs = time()
for row in xrange(r.top, r.top + r.height):
   # when inverting large selections it's nicer to
   # give some indication of progress
   newsecs = time()
   if newsecs - oldsecs >= 1.0:
      oldsecs = newsecs
      g.update()

   # also allow keyboard interaction
   g.dokey( g.getkey() )

   for col in xrange(r.left, r.left + r.width):
      g.setcell(col, row, 1 - g.getcell(col, row))

if not r.visible(): g.fitsel()
