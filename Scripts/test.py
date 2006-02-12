# A simple test script for Golly.
# Note that Scripts/init.py has been executed.

import golly as g

g.new("test")
g.show("Hello Andy!")   # show message in Golly's status bar
g.setrule("b34/s34")

### blinker = g.parse("ooo!", 0, 0, 1, 0, 0, 1)
### g.putcells(blinker, 1, 2, 1, 0, 0, 1)
### g.putcells(blinker, 6, 7, 0, -1,  1,  0)   # rotate clockwise

# equivalent code using calls defined in init.py
blinker = pattern("ooo!")
blinker.put(1, 2)
blinker.put(6, 7, rcw)   # rotate clockwise

# test deliberate errors:
# g.xxx(3)
# g.setrule()

# g.warn("Seeya later.")     # show message in Golly's warning dialog
# import sys ; sys.exit(0)   # exits Golly too!

g.fit()
g.show("Seeya!")
