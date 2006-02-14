# A simple test script for Golly.

# execute Scripts/glife/__init__.py
from glife import *

import golly as g

g.new("test")
g.show("Hello Andy!")   # show message in Golly's status bar
g.setrule("b34/s34")

### blinker = g.parse("ooo!", 0, 0, 1, 0, 0, 1)
### g.putcells(blinker, 1, 2, 1, 0, 0, 1)
### g.putcells(blinker, 6, 7, 0, -1,  1,  0)   # rotate clockwise

# equivalent code using calls defined in life module
blinker = pattern("ooo!")
blinker.put(1, 2)
blinker.put(6, 7, rcw)   # rotate clockwise

# NOTE:
# - load simply returns pattern(g.load(filename))
# - g.load ensures the top left cell of the bounding box is at 0,0
# - paths are relative to the location of the script
rabbits = load("../Patterns/Methuselahs/rabbits.lif");
rabbits.put(10,10)
mctest = load("../Patterns/Hashing-Examples/wolfram22.mc");
mctest.put(20,20)

# test deliberate errors:
# xxx(3)
# g.setrule()

# g.warn("Seeya later.")     # show message in Golly's warning dialog
# import sys ; sys.exit(0)   # exits Golly too!

g.fit()

all = blinker(1,2) + blinker(6,7,rcw) + rabbits(10,10) + mctest(20,20)
all.save("test.rle")
g.show("Pattern saved in test.rle")
