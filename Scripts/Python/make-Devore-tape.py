# =================================================
# A Golly script to write the program and data
# tapes for the Devore UCC. Load Devore-body.rle
# and run this script, then set the machine running.
#
# Tape length: 105k
# Rep period: 1.02 x 10^11 (102 billion) timesteps
#
# Script license: public domain
#
# Contact: tim.hutton@gmail.com
# Thanks to Ron Hightower and John Devore
# =================================================

import golly as g

program_tape_x = 322
program_tape_y = 233
data_tape_x = 330
data_tape_y = 8

# range from start to stop (either direction)
#   twoway_range(0,10,2)==[0,2,4,6,8]
#   twoway_range(0,-10,2)==[0,-2,-4,-6,-8]
def twoway_range(start,stop,step=1):
    return list(range(start,stop,(-1 if (stop-start)<0 else 1)*abs(step)))

# range from start to stop including both end points (either direction)
#   inclusive_range(0,10,2)==[0,2,4,6,8,10]
#   inclusive_range(10,0,2)==[10,8,6,4,2,0]
#   inclusive_range(0,-10,2)==[0,-2,-4,-6,-8,-10]
def inclusive_range(start,stop,step=1):
    return twoway_range(start,stop+(-1 if (stop-start)<0 else 1)*abs(step),step)

'''
Devore instruction set: (the program tape)
0000 goto (following bits give destination)
0001 move data head (D) left
0010 move data head right
0011 jump_if_one [from whichever path is open] (following bits give destination)
0100 m (mark_if_zero) [affects whichever path is open]
0101 e (erase_if_one) [affects whichever path is open]
0110 switch between construction path and data path [D open by default]
0111 x (extend) [all these below only affect C]
1000 xl
1001 xr
1010 r (retract)
1011 rl
1100 rr
1101 i (inject sheath) # legacy from Codd, don't use this! (see below)
1110 j (inject trigger)
1111 stop

Destination bits: first is sign (0=backwards, 1=forwards), followed by
unary count of marks and zero terminator. E.g. 111110=+4, 011110=-4

Marks lie 6 cells below the program tape, and appear on the column before
the start of the command they mark. Not every command needs a mark, just
those jumped-to.

Destination says how many marks forward or backwards to jump. Exception is
when a goto is immediately followed by a mark - in this case the goto
destination is decremented because the next mark is skipped over anyway.

For injection, use the pattern:

         1  1
            1  1  1  1  1
         1  1

Approach from the left into the receiver, then retract. This injects the
sheathing signal. Then call the inject_trigger command. You can then safely
retract away. The inject_sheath command was used by Codd but is now
redundant, it appears.
'''
goto = '0000'
toggle_C_or_D = '0110'
data_head_left = '0001'
data_head_right = '0010'
extend = '0111'
extend_left = '1000'
extend_right = '1001'
retract = '1010'
retract_left = '1011'
retract_right = '1100'
mark = '0100'
erase = '0101'
jump_if_one = '0011'
inject_sheath = '1101'
inject_trigger = '1110'
stop = '1111'

def write_string(s):
    global program_tape_x, program_tape_y
    for c in s:
        if c=='1':
            g.setcell(program_tape_x,program_tape_y,1)
        program_tape_x += 1

def write_data_string(s):
    global data_tape_x, data_tape_y
    for c in s:
        if c=='1':
            g.setcell(data_tape_x,data_tape_y,1)
        data_tape_x += 1

def write_program(program):
    global program_tape_x, program_tape_y

    # find all the labels
    labels = [c[:-1] for c in program if c[-1]==':']

    iCurrentLabel=0
    for i,c in enumerate(program):
        if 'jump_if_one:' in c:
            write_string(jump_if_one)
            jump = labels.index(c[c.index(':')+1:])+1-iCurrentLabel
            if jump<1:
                jump-=1
            if jump>1 and program[i+1][-1]==':':
                jump -= 1
                # (if next command is a label we don't need to jump so far)
            if jump<0:
                write_string('0')
            else:
                write_string('1')
            write_string('1'*abs(jump)+'0')
        elif 'goto:' in c:
            write_string(goto)
            jump = labels.index(c[c.index(':')+1:])+1-iCurrentLabel
            if jump<1:
                jump-=1
            if jump>1 and program[i+1][-1]==':':
                jump -= 1
                # (if next command is a label we don't need to jump so far)
            if jump<0:
                write_string('0')
            else:
                write_string('1')
            write_string('1'*abs(jump)+'0')
        elif c[-1]==':':
            # a label, make a mark on the marker tape
            g.setcell(program_tape_x-1,program_tape_y-6,1)
            iCurrentLabel+=1
        else:
            # a normal command
            write_string(c)

# ------------------------------------
# create the program:
# ------------------------------------

program = [
    # ---------- data tape copying phase: ------------------------
    'goto:extendup',
    'write1:',
    # == 1 ==
    extend_right,toggle_C_or_D,mark,toggle_C_or_D,retract_right,extend,
    'nextread:',
    data_head_left,
    'jump_if_one:write1',
    # 0:
    extend,
    data_head_left,
    'jump_if_one:write1',
    # == 00 ==
    extend,
    data_head_left,
    'jump_if_one:write1',
    # == 000 ==
    # -- end of copy --
    data_head_right, # we overshot (first data entry is 01)
    toggle_C_or_D, # open C
    'retractcopy:',
    retract,extend_left,extend,'jump_if_one:retractdown',
    retract,retract_left,'goto:retractcopy',
    # ---------- construction phase: ------------------------
    'rowstart:',
    # move into writing position
    extend_right,extend,extend,extend,
    # (make a mark at the start of the row for retraction)
    extend_left,extend,toggle_C_or_D,mark,toggle_C_or_D,retract,retract_left,
    'nextcolumn:',
    data_head_right,
    'jump_if_one:1',
    # 0:
    data_head_right,
    'jump_if_one:01',
    # == 00 ==
    data_head_right,
    'jump_if_one:001',
    # == 000 ==
    # (assume came immediately after start of a new row)
    extend_left,extend,toggle_C_or_D,erase,retract,retract_left,
    retract,retract,retract,retract_right,
    'retractcolumn:',
    extend_left,extend,'jump_if_one:doneretractcolumn',
    retract,retract_left,retract,'goto:retractcolumn',
    'doneretractcolumn:',erase,retract,retract_left,
    'goto:inject',
    '001:',
    # == 001 ==
    # -- retract row --
    toggle_C_or_D, # open C
    'retractrow:',
    retract,extend_left,extend,'jump_if_one:doneretractrow',
    retract,retract_left,'goto:retractrow',
    'doneretractrow:',
    erase,retract,retract_left,toggle_C_or_D,retract,retract,retract,retract_right,
    extend,
    'goto:rowstart',
    '01:',
    # == 01 ==
    # -- write N 0's --
    extend,extend,extend,
    'goto:nextcolumn',
    '1:',
    data_head_right,
    'jump_if_one:11',
    # == 10 ==
    # -- write a 0 --
    extend,
    'goto:nextcolumn',
    '11:',
    # == 11 ==
    # -- write a 1 --
    extend_right,
    toggle_C_or_D, # open C
    mark,
    toggle_C_or_D, # close C
    retract_right,
    extend,
    'goto:nextcolumn',
    # ------------------------ subroutines (here for speed): -----------------
    'extendup:',
    # move the construction arm into the starting position
    extend_right,extend,extend,extend,extend,extend,extend,extend,extend,
    # move up to the right height
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,
    # move into writing position
    extend_right,extend,extend,extend,
    # (make a mark at the start of the copy for retraction)
    extend_left,extend,toggle_C_or_D,mark,toggle_C_or_D,retract,retract_left,
    'goto:nextread',
    'retractdown:',
    erase,retract,retract_left,toggle_C_or_D,retract,retract,retract,retract_right,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,retract,retract,retract,retract,retract,retract,
    retract,retract,retract,retract,
    extend_left,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,
    extend_right,extend,extend,extend,extend,extend,extend,extend,extend,
    # (make a mark at the bottom-left for final retraction)
    extend_left,extend,toggle_C_or_D,mark,toggle_C_or_D,retract,retract_left,
    'goto:rowstart',
    # -- end of construction: retract and inject sheath+trigger --
    'inject:',
    retract,extend_right,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,extend,extend,extend,extend,extend,
    extend,extend,extend,extend,extend,
    retract,inject_trigger,
    retract,retract,retract,
    stop
]

# --------------------------------
# now write the tapes:
# --------------------------------

# Our data tape contains coded construction instructions:
# 11  : write a 1
# 10  : write a 0
# 01  : write n 0's (where n is the value below)
# 001 : end of row
# 000 : end of construction
#
# Each row is scanned from the left edge to the last 1. (A temporary marker
# is used at the beginning for retraction, so rows can be any length.) Because
# each row ends with a 1, we know that the sequence 000 cannot appear on the
# data tape until the very end. This allows us to detect (reading backwards)
# the 000 sequence, in order to know when to stop copying.
#
# For retracting vertically after construction another temporary marker is used.
#
# Following Ron Hightower's work (thanks Ron!) we copy the data tape first and
# then read it from the far end. The data read head is made slightly prominent
# to allow the 000 to be skipped (else it would be detected during tape-copy and
# would cause it to end), but read correctly from the blank space at the end of
# the data tape nearest the machine.
#
# The instruction set above was just one that I tried. I'm quite sure it is
# possible to do this more efficiently.

n=3 # found through experiment to give the shortest data tape
# must match the "write N 0's" section of the program above
# (I would have expected 10 or so to be better but anyway.)

# first we write the program and marker tapes
# then we encode the whole pattern onto the data tape
# (program length is mostly unrelated to machine size, so it all works)
write_program(program)

# write the data tape
data_tape = ''
rect = g.getrect()   # copy everything within the bounding rectangle
width = rect[2]
height = rect[3]
x = rect[0]
y = rect[1]
for row in inclusive_range(y+height-1,y):
    far_right = x+width-1
    while g.getcell(far_right,row)==0:
        far_right -= 1
        if far_right<x:
            g.exit('Empty rows forbidden: '+str(row))
    col = x
    while col <= far_right:
        if g.getcell(col,row)==1:
            data_tape+='11' # write 1
            col += 1
        else:
            if not g.getcells([col,row,n,1]): # next N cells zero?
                data_tape+='01'
                col += n
            else:
                data_tape+='10' # write 0
                col += 1
    data_tape+='001' # end of row
#data_tape+='000' # end of construction (actually we skip this command)
write_data_string(data_tape[::-1])
#g.warn(str(len(data_tape))) # tell the user how long the tape was

# activate the injection receiver
for i in range(50,65):
    g.setcell(i,230,2)
    g.setcell(i,231,1)
    g.setcell(i,232,2)
g.setcell(64,231,0)
g.setcell(65,231,6)
g.setcell(51,231,0)
g.setcell(52,231,7)
