# Calculates the density of live cells in the current pattern.
# Author: Andrew Trevorrow (andrew@trevorrow.com), March 2006.

from glife import rect
import golly as g

def bigs2float(bigs):
   # convert a bigint string like "1,234,..." to a floating point number
   return float( bigs.replace(",","") )

g.setoption("showstatusbar", True)
bbox = rect( g.getrect() )
if bbox.empty:
   g.error("The pattern is empty.")
else:
   d = bigs2float( g.getpop() ) / ( float(bbox.wd) * float(bbox.ht) )
   if d < 0.000001:
      g.show("Density = %.1e" % d)
   else:
      g.show("Density = %.6f" % d)
