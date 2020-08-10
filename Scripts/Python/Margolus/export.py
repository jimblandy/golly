# Change the entire pattern from emulated-Margolus states to N-state:

from glife import rect
import golly as g

r = rect( g.getrect() )
if r.empty: g.exit("There is no pattern.")

for row in range(r.top, r.top + r.height):
    for col in range(r.left, r.left + r.width):
        s = g.getcell(col,row)
        g.setcell(col, row, (s+s%2)//2-1)
