# Invert all cell states in the current selection.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Jun 2006.
# Updated to use exit command, Nov 2006.
# Updated to use numstates command, Jun 2008.

from glife import rect
from time import time
import golly as g

r = rect( g.getselrect() )
if r.empty: g.exit("There is no selection.")

oldsecs = time()
maxstate = g.numstates() - 1

for row in range(r.top, r.top + r.height):
    # if large selection then give some indication of progress
    newsecs = time()
    if newsecs - oldsecs >= 1.0:
        oldsecs = newsecs
        g.update()
    for col in range(r.left, r.left + r.width):
        g.setcell(col, row, maxstate - g.getcell(col, row))

if not r.visible(): g.fitsel()
