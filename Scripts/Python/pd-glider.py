# Creates a large set of pentadecathlon + glider collisions.
# Based on pd_glider.py from PLife (http://plife.sourceforge.net/).

from glife.base import *

rule("B3/S23")

def collision (i, j):
    return pentadecathlon + glider[i + 11] (-8 + j, -10, flip)

all = pattern ()

for i in range (-7, 8):
    for j in range (-9, 10):
        all += collision (i, j) (100 * i, 100 * j)

all.display ("pd-glider")
