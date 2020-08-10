# Make a pattern from states 1 and 0, select it and then
# run this script to make a Codd CA pattern that will then
# construct the pattern. The constructor will then attempt
# to inject sheathing and triggering signals to a point one
# up from the pattern's bottom-left corner.
#
# See example: Patterns/Self-Rep/Codd/golly-constructor.rle
#
# Tim Hutton <tim.hutton@gmail.com>

from glife import rect
from time import time
import golly as g

r = rect( g.getselrect() )
if r.empty: g.exit("There is no selection.")

oldsecs = time()
maxstate = g.numstates() - 1

# these are the commands:
extend = '70116011'
extend_left = '4011401150116011'
extend_right = '5011501140116011'
retract = '4011501160116011'
retract_left = '5011601160116011'
retract_right = '4011601160116011'
mark = '701160114011501170116011'
erase = '601170114011501160116011'
sense = '70117011'
cap = '40116011'
inject_sheath = '701150116011'
inject_trigger = '60117011701160116011'
# (sometimes you don't need two blanks after each command but I haven't analysed this fully)

# we first write the commands to a string, and then to the grid
# (we start off facing right, at the bottom-left of the construction area)

# write the cells that are in state 1 (can ignore the zeros)
# (still plenty of room for optimisation here)
tape = '11'
for row in range(r.top, r.top + r.height):
    # if large selection then give some indication of progress
    newsecs = time()
    if newsecs - oldsecs >= 1.0:
        oldsecs = newsecs
        g.update()
    for col in range(r.left, r.left + r.width):
        if g.getcell(col, row)==1:
            tape += extend*(4+col-r.left) + extend_left + extend*(r.top+r.height-row)
            tape += mark
            tape += retract*(r.top+r.height-row) + retract_left + retract*(4+col-r.left)
        elif g.getcell(col,row)!=0:
            g.exit('Cells in the selected area must be in states 0 or 1 only.')

# finally we sheath and trigger and retract
tape += extend_left + extend*4 + extend_right + extend
tape += inject_sheath + '1'*50 + inject_trigger
tape += retract*2 + retract_right + retract*4 + retract_left

# now write the tape out
x = r.left+r.width+10
y = r.top+r.height+10
g.setcell(x+1,y,2)
for i in tape:
    g.setcell(x,y-1,2)
    g.setcell(x,y,int(i))
    g.setcell(x,y+1,2)
    x-=1
