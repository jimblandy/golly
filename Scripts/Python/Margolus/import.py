# Change the selected area from N states to emulated-Margolus states:
# s -> 1+2s (if in the top-left of the partition)
# s -> 2+2s (if not)

from glife import rect
import golly as g

r = rect( g.getselrect() )
if r.empty: g.exit("There is no selection.")

for row in range(r.top, r.top + r.height):
    for col in range(r.left, r.left + r.width):
        if (col%2) and (row%2):
            g.setcell(col, row, 1+g.getcell(col,row)*2)
        else:
            g.setcell(col, row, 2+g.getcell(col,row)*2)
