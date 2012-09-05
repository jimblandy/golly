# Calculates the density of live cells in the current pattern.
# Author: Andrew Trevorrow (andrew@trevorrow.com), March 2006.
# Updated to use exit command, Nov 2006.

from glife import rect
import golly as g

bbox = rect( g.getrect() )
if bbox.empty: g.exit("The pattern is empty.")

d = float( g.getpop() ) / ( float(bbox.wd) * float(bbox.ht) )
if d < 0.000001:
    g.show("Density = %.1e" % d)
else:
    g.show("Density = %.6f" % d)
