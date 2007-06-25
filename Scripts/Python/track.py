# Based on track.py from PLife (http://plife.sourceforge.net/).

from glife.herschel import *

text = """This file reproduces 'BTRACK' from Alan Hensel's collection
(without boxes and labels, though).
The conduits are passed in the following order:
77, 112, 158, 190, 119, 117, 156, 77, 64, 64 and 112.
What 'BTRACK' says is the following:

 All 8 of David Buckingham's Herschel conduits demonstrated.

 A Herschel is the 20th generation of the B-heptomino (see BHEPTO).
 Various combinations of still life can be made to assist the
 forward propagation of a Herschel, and with Buckingham's complete
 set, a glider gun of any period greater than 61 can be constructed.
 This period-1246 loop demonstrates all of them. Each conduit in
 this pattern is boxed off and labeled, and future Herschel
 positions are marked by ghost Herschels. See also STILLREFLECTOR, GUN256B.
 
 Gens Type      Tightest Compression; Notes
 ---- ----      ---------------------------
 64   90 degree 153 (also 73, 74, 77); 1 glider, aimed inward.
 77   move+flip  61; 1 glider (natural b-heptomino glider).
 112  90 degree  61; 1 glider, aimed outward.
 117  move       63; 1 glider. Can be perturbed by single-spark oscs.
 119  move+flip 231; 3 gliders.
 156  90 degree  62; 2 gliders: Normal B-hepto glider + one more.
 158  move+flip 176 (also 101, 102, 109, 111, 159); 1 glider.
 190  90 degree 107; 1 glider, aimed inward.
"""

rule()

path = ( hc77, hc112, hc158, hc190, hc119, hc117, hc156, hc77, hc64, hc64, hc112, )

A = herschel + eater (-20, 24, flip_x) + eater (1, 46, swap_xy_flip) + eater (8, 60, rcw)
T = (10, -24, swap_xy_flip)

all = A (*T)

for hc in path:
	all += hc (*T) + herschel_ghost (*T)
	T = compose (hc.transform, T)

### all.save ("track.lif", text)

all.display ("track")
