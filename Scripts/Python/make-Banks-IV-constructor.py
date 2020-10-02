# Make a pattern from states 3 and 0, select it and then
# run this script to make a Banks-IV CA pattern that will then
# construct the pattern.
#
# The machine will attempt to activate the pattern by injecting
# a signal at a special trigger point.
# See: Patterns/Self-Rep/Banks/Banks-IV-constructor.rle.gz
#
# You may need to change trigger_row for your pattern, or disable
# triggering.
#
# Tim Hutton <tim.hutton@gmail.com>

from glife import rect
from time import time
import golly as g

r = rect( g.getselrect() )
if r.empty: g.exit("There is no selection.")

oldsecs = time()
maxstate = g.numstates() - 1

top_tape = '00'
bottom_tape = '00'

# write a state 3 to the required position, relative to the centre of the face
def write_cell(x,y):
    global top_tape
    global bottom_tape
    # delay one of the tapes, to select the correct row
    if y<0: top_tape += '00'*abs(y)
    elif y>0: bottom_tape += '00'*y
    # send out a write arm to the right length
    arm_length=2
    while arm_length < x:
        top_tape += '120' + '00'*arm_length + '12000'
        bottom_tape += '120' + '00'*arm_length + '12000'
        arm_length += 1
    # send an erasing wing down the arm, to write a cell at the end
    top_tape += '120' + '00'*(arm_length-3) + '120000'
    bottom_tape += '120' + '00'*(arm_length-3) + '120000'
    # undo the delay on the tapes (could just add the difference but this is simpler)
    if y<0: bottom_tape += '00'*abs(y)
    elif y>0: top_tape += '00'*y

# we first write the commands to 2 strings, and then to the grid
H_OFFSET = 10   # how far from the construction face should the new pattern be?

half_height = (r.height-(r.height%2))//2

# write the cells that are in state 1 (can ignore the zeros)
# (still plenty of room for optimisation here)
for col in range(r.left + r.width-1, r.left-1,-1):
    # if large selection then give some indication of progress
    newsecs = time()
    if newsecs - oldsecs >= 1.0:
        oldsecs = newsecs
        g.update()
    for row in range(r.top, r.top + r.height):
        if g.getcell(col,row)==3:
            write_cell(H_OFFSET + col-r.left,row - r.top - half_height)

# trigger
trigger_row = 3 # from the vertical middle
write_cell(H_OFFSET + 2,trigger_row)

# (optional) restore the trigger input
write_cell(H_OFFSET-3, trigger_row-1)
write_cell(H_OFFSET-2, trigger_row+2)
write_cell(H_OFFSET-2, trigger_row+1) # actually just sends an erasing wing
write_cell(H_OFFSET, trigger_row-2)
write_cell(H_OFFSET, trigger_row+2)

fx = r.left-20 # constructing face
fy = r.top+r.height*2 # centre of constructing face
half_height += 10

# draw the constructing face
for cy in range(fy-(half_height-6),fy+(half_height-6)+1):
    g.setcell(fx,cy,3)
# draw the top shoulder
g.setcell(fx,fy-half_height,3)
g.setcell(fx,fy-half_height-3,3)
g.setcell(fx-1,fy-half_height-3,3)
g.setcell(fx+1,fy-half_height-3,3)
g.setcell(fx-1,fy-half_height-2,3)
g.setcell(fx+1,fy-half_height-2,3)
g.setcell(fx+1,fy-half_height-1,3)
g.setcell(fx+1,fy-half_height+1,3)
g.setcell(fx+1,fy-half_height+2,3)
g.setcell(fx+1,fy-half_height+3,3)
g.setcell(fx+1,fy-half_height+4,3)
g.setcell(fx+1,fy-half_height+5,3)
g.setcell(fx-1,fy-half_height+2,3)
g.setcell(fx-1,fy-half_height+3,3)
g.setcell(fx-1,fy-half_height+4,3)
g.setcell(fx-1,fy-half_height+5,3)
g.setcell(fx+2,fy-half_height-1,3)
g.setcell(fx+3,fy-half_height-1,3)
g.setcell(fx+3,fy-half_height,3)
g.setcell(fx+2,fy-half_height+1,3)
g.setcell(fx+3,fy-half_height+1,3)
g.setcell(fx+2,fy-half_height+4,3)
g.setcell(fx-2,fy-half_height+5,3)
# draw the bottom shoulder
g.setcell(fx,fy+half_height,3)
g.setcell(fx,fy+half_height+3,3)
g.setcell(fx-1,fy+half_height+3,3)
g.setcell(fx+1,fy+half_height+3,3)
g.setcell(fx-1,fy+half_height+2,3)
g.setcell(fx+1,fy+half_height+2,3)
g.setcell(fx+1,fy+half_height+1,3)
g.setcell(fx+1,fy+half_height-1,3)
g.setcell(fx+1,fy+half_height-2,3)
g.setcell(fx+1,fy+half_height-3,3)
g.setcell(fx+1,fy+half_height-4,3)
g.setcell(fx+1,fy+half_height-5,3)
g.setcell(fx-1,fy+half_height-2,3)
g.setcell(fx-1,fy+half_height-3,3)
g.setcell(fx-1,fy+half_height-4,3)
g.setcell(fx-1,fy+half_height-5,3)
g.setcell(fx+2,fy+half_height-1,3)
g.setcell(fx+3,fy+half_height-1,3)
g.setcell(fx+3,fy+half_height,3)
g.setcell(fx+2,fy+half_height+1,3)
g.setcell(fx+3,fy+half_height+1,3)
g.setcell(fx+2,fy+half_height-4,3)
g.setcell(fx-2,fy+half_height-5,3)
# draw the tapes
x = fx-1
for c in range(0,len(top_tape)):
    g.setcell(x,fy-half_height-1,3)
    g.setcell(x,fy-half_height,int(top_tape[c]))
    g.setcell(x,fy-half_height+1,3)
    x -= 1
x = fx-1
for c in range(0,len(bottom_tape)):
    g.setcell(x,fy+half_height-1,3)
    g.setcell(x,fy+half_height,int(bottom_tape[c]))
    g.setcell(x,fy+half_height+1,3)
    x -= 1
