# Generator for Turmite rules
#
# Following work of Ed Pegg Jr.
# specifically the Mathematica notebook linked-to from here:
# http://www.mathpuzzle.com/26Mar03.html
#
# contact: tim.hutton@gmail.com

import golly
import random
from glife.RuleTree import *

dirs=['N','E','S','W']
opposite_dirs=[2,3,0,1] # index of opposite direction
turn={ # index of new direction
1:[0,1,2,3], # no turn
2:[1,2,3,0], # right
4:[2,3,0,1], # u-turn
8:[3,0,1,2], # left
}

prefix = 'Turmite'

# N.B. Translating from Langtons-Ant-nColor to Turmite is easy:
# e.g. LLRR is (1 state, 4 color) {{{1,8,0},{2,8,0},{3,2,0},{0,2,0}}}
# but translating the other way is only possible if the Turmite has
# exactly 1 state and only moves left or right

EdPeggJrTurmiteLibrary = [
# source: http://demonstrations.wolfram.com/Turmites/
# Translated into his later notation: 1=noturn, 2=right, 4=u-turn, 8=left
# (old notation was: 0=noturn,1=right,-1=left)
# (all these are 2-color patterns)
"{{{1, 2, 0}, {0, 8, 0}}}",  # 1: Langton's ant
"{{{1, 2, 0}, {0, 1, 0}}}",  # 2: binary counter
"{{{0, 8, 1}, {1, 2, 1}}, {{1, 1, 0}, {1, 1, 1}}}", # 3: (filled triangle)
"{{{0, 1, 1}, {0, 8, 1}}, {{1, 2, 0}, {0, 1, 1}}}", # 4: spiral in a box
"{{{0, 2, 1}, {0, 8, 0}}, {{1, 8, 1}, {0, 2, 0}}}", # 5: stripe-filled spiral
"{{{0, 2, 1}, {0, 8, 0}}, {{1, 8, 1}, {1, 1, 0}}}", # 6: stepped pyramid
"{{{0, 2, 1}, {0, 1, 1}}, {{1, 2, 1}, {1, 8, 0}}}", # 7: contoured island
"{{{0, 2, 1}, {0, 2, 1}}, {{1, 1, 0}, {0, 2, 1}}}", # 8: woven placemat
"{{{0, 2, 1}, {1, 2, 1}}, {{1, 8, 1}, {1, 8, 0}}}", # 9: snowflake-ish (mimics Jordan's ice-skater)
"{{{1, 8, 0}, {0, 1, 1}}, {{0, 8, 0}, {0, 8, 1}}}", # 10: slow city builder
"{{{1, 8, 0}, {1, 2, 1}}, {{0, 2, 0}, {0, 8, 1}}}", # 11: framed computer art
"{{{1, 8, 0}, {1, 2, 1}}, {{0, 2, 1}, {1, 8, 0}}}", # 12: balloon bursting (makes a spreading highway)
"{{{1, 8, 1}, {0, 8, 0}}, {{1, 1, 0}, {0, 1, 0}}}", # 13: makes a horizontal highway
"{{{1, 8, 1}, {0, 8, 0}}, {{1, 2, 1}, {1, 2, 0}}}", # 14: makes a 45 degree highway
"{{{1, 8, 1}, {0, 8, 1}}, {{1, 2, 1}, {0, 8, 0}}}", # 15: makes a 45 degree highway
"{{{1, 8, 1}, {0, 1, 0}}, {{1, 1, 0}, {1, 2, 0}}}", # 16: spiral in a filled box
"{{{1, 8, 1}, {0, 2, 0}}, {{0, 8, 0}, {0, 8, 0}}}", # 17: glaciers
"{{{1, 8, 1}, {1, 8, 1}}, {{1, 2, 1}, {0, 1, 0}}}", # 18: golden rectangle!
"{{{1, 8, 1}, {1, 2, 0}}, {{0, 8, 0}, {0, 8, 0}}}", # 19: fizzy spill
"{{{1, 8, 1}, {1, 2, 1}}, {{1, 1, 0}, {0, 1, 1}}}", # 20: nested cabinets
"{{{1, 1, 1}, {0, 8, 1}}, {{1, 2, 0}, {1, 1, 1}}}", # 21: (cross)
"{{{1, 1, 1}, {0, 1, 0}}, {{0, 2, 0}, {1, 8, 0}}}", # 22: saw-tipped growth
"{{{1, 1, 1}, {0, 1, 1}}, {{1, 2, 1}, {0, 1, 0}}}", # 23: curves in blocks growth
"{{{1, 1, 1}, {0, 2, 0}}, {{0, 8, 0}, {0, 8, 0}}}", # 24: textured growth
"{{{1, 1, 1}, {0, 2, 1}}, {{1, 8, 0}, {1, 2, 0}}}", # 25: (diamond growth)
"{{{1, 1, 1}, {1, 8, 0}}, {{1, 2, 1}, {0, 1, 0}}}", # 26: coiled rope
"{{{1, 2, 0}, {0, 8, 1}}, {{1, 8, 0}, {0, 1, 1}}}", # 27: (growth)
"{{{1, 2, 0}, {0, 8, 1}}, {{1, 8, 0}, {0, 2, 1}}}", # 28: (square spiral)
"{{{1, 2, 0}, {1, 2, 1}}, {{0, 1, 0}, {0, 1, 1}}}", # 29: loopy growth with holes
"{{{1, 2, 1}, {0, 8, 1}}, {{1, 1, 0}, {0, 1, 0}}}", # 30: Langton's Ant drawn with squares
"{{{1, 2, 1}, {0, 2, 0}}, {{0, 8, 1}, {1, 8, 0}}}", # 31: growth with curves and blocks
"{{{1, 2, 1}, {0, 2, 0}}, {{0, 1, 0}, {1, 2, 1}}}", # 32: distracted spiral builder
"{{{1, 2, 1}, {0, 2, 1}}, {{1, 1, 0}, {1, 1, 1}}}", # 33: cauliflower stalk (45 deg highway)
"{{{1, 2, 1}, {1, 8, 1}}, {{1, 2, 1}, {0, 2, 0}}}", # 34: worm trails (eventually turns cyclic!)
"{{{1, 2, 1}, {1, 1, 0}}, {{1, 1, 0}, {0, 1, 1}}}", # 35: eventually makes a two-way highway!
"{{{1, 2, 1}, {1, 2, 0}}, {{0, 1, 0}, {0, 1, 0}}}", # 36: almost symmetric mould bloom
"{{{1, 2, 1}, {1, 2, 0}}, {{0, 2, 0}, {1, 1, 1}}}", # 37: makes a 1 in 2 gradient highway
"{{{1, 2, 1}, {1, 2, 1}}, {{1, 8, 1}, {0, 2, 0}}}", # 38: immediately makes a 1 in 3 highway
"{{{0, 2, 1}, {1, 2, 1}}, {{0, 8, 2}, {0, 8, 0}}, {{1, 2, 2}, {1, 8, 0}}}", # 39: squares and diagonals growth
"{{{1, 8, 1}, {0, 1, 0}}, {{0, 2, 2}, {1, 8, 0}}, {{1, 2, 1}, {1, 1, 0}}}", # 40: streak at approx. an 8.1 in 1 gradient
"{{{1, 8, 1}, {0, 1, 2}}, {{0, 2, 2}, {1, 1, 1}}, {{1, 2, 1}, {1, 1, 0}}}", # 41: streak at approx. a 1.14 in 1 gradient
"{{{1, 8, 1}, {1, 8, 1}}, {{1, 1, 0}, {0, 1, 2}}, {{0, 8, 1}, {1, 1, 1}}}", # 42: maze-like growth
"{{{1, 8, 2}, {0, 2, 0}}, {{1, 8, 0}, {0, 2, 0}}, {{0, 8, 0}, {0, 8, 1}}}", # 43: growth by cornices
"{{{1, 2, 0}, {0, 2, 2}}, {{0, 8, 0}, {0, 2, 0}}, {{0, 1, 1}, {1, 8, 0}}}", # 44: makes a 1 in 7 highway
"{{{1, 2, 1}, {0, 8, 0}}, {{1, 2, 2}, {0, 1, 0}}, {{1, 8, 0}, {0, 8, 0}}}", # 45: makes a 4 in 1 highway
# source: http://www.mathpuzzle.com/Turmite5.nb
# via: http://www.mathpuzzle.com/26Mar03.html
# "I wondered what would happen if a turmite could split as an action... say Left and Right.
#  In addition, I added the rule that when two turmites met, they annihilated each other.
#  Some interesting patterns came out from my initial study. My main interest is finding
#  turmites that will grow for a long time, then self-annihilate."
"{{{1, 8, 0}, {1, 2, 1}}, {{0, 10, 0}, {0, 8, 1}}}",  # 46: stops at 11 gens
"{{{1, 8, 0}, {1, 2, 1}}, {{1, 10, 0}, {1, 8, 1}}}",  # 47: stops at 12 gens
"{{{1, 15, 0}, {0, 2, 1}}, {{0, 10, 0}, {0, 8, 1}}}", # 48: snowflake-like growth
"{{{1, 2, 0}, {0, 15, 0}}}",                          # 49: blob growth
"{{{1, 3, 0}, {0, 3, 0}}}",                           # 50: (spiral) - like SDSR-Loop on a macro level
"{{{1, 3, 0}, {0, 1, 0}}}",                           # 51: (spiral)
"{{{1, 10, 0}, {0, 1, 0}}}",                          # 52: snowflake-like growth
"{{{1, 10, 1}, {0, 1, 1}}, {{0, 2, 0}, {0, 0, 0}}}",  # 53: interesting square growth
"{{{1, 10, 1}, {0, 5, 1}}, {{1, 2, 0}, {0, 8, 0}}}",  # 54: interesting square growth
"{{{1, 2, 0}, {0, 1, 1}}, {{1, 2, 0}, {0, 6, 0}}}",   # 55: growth
"{{{1, 2, 0}, {1, 1, 1}}, {{0, 2, 0}, {0, 11, 0}}}",  # 56: wedge growth with internal snakes
"{{{1, 2, 1}, {0, 2, 1}}, {{1, 15, 0}, {1, 8, 0}}}",  # 57: divided square growth
"{{{1, 2, 0}, {2, 8, 2}, {1, 1, 1}}, {{1, 1, 2}, {0, 2, 1}, {2, 8, 1}}, {{0, 11, 0}, {1, 1, 1}, {0, 2, 2}}}", # 58: semi-chaotic growth with spikes
"{{{1, 2, 0}, {2, 8, 2}, {2, 1, 1}}, {{1, 1, 2}, {1, 1, 1}, {1, 8, 1}}, {{2, 10, 0}, {2, 8, 1}, {2, 8, 2}}}"  # 59: halts after 204 gens (256 non-zero cells written)
]
# N.B. the images in the demonstration project http://demonstrations.wolfram.com/Turmites/
# are mirrored - e.g. Langton's ant turns left instead of right.
# (Also example #45 isn't easily accessed in the demonstration, you need to open both 'advanced' controls,
#  then type 45 in the top text edit box, then click in the bottom one.)

# just some discoveries of my own, discovered through random search
TimHuttonTurmiteLibrary = [

# One-turmite effects:
"{{{1,2,1},{0,4,1}},{{1,1,0},{0,8,0}}}", # makes a period-12578 gradient 23 in 29 speed c/340 highway (what is the highest period highway?)
"{{{0,2,1},{0,8,0}},{{1,8,1},{1,4,0}}}", # makes a period-68 (speed c/34) glider (what is the highest period glider?)
                                         # (or alternatively, what are the *slowest* highways and gliders?)
"{{{1,4,1},{1,2,1}},{{1,8,0},{1,1,1}}}", # another ice-skater rule, makes H-fractal-like shapes
"{{{1,1,1},{0,2,1}},{{0,2,1},{1,8,0}}}", # period-9 glider
"{{{1,2,1},{0,8,1}},{{1,4,0},{1,8,0}}}", # another ice-skater
"{{{1,4,1},{1,8,1}},{{1,4,0},{0,1,0}}}", # Langton's ant (rot 45) on a 1 background but maze-like on a 0 background
"{{{0,2,1},{0,8,1}},{{1,1,0},{1,8,1}}}", # slow ice-skater makes furry shape
"{{{1,8,0},{1,1,1}},{{1,8,0},{0,2,1}}}", # busy beaver (goes cyclic) <50k
"{{{0,8,1},{1,8,1}},{{1,2,1},{0,2,0}}}", # interesting texture
"{{{1,1,1},{0,1,0}},{{1,2,0},{1,2,1}}}", # period-17 1 in 3 gradient highway (but needs debris to get started...)
"{{{0,2,1},{1,8,1}},{{1,4,0},{0,2,1}}}", # period-56 speed-28 horizontal highway
"{{{1,8,0},{0,1,1}},{{1,2,0},{1,8,0}}}", # somewhat loose growth

# Some two-turmite rules below: (ones that are dull with 1 turmite but fun with 2)
# Guessing this behaviour may be quite common, because turmites alternate between black-and-white squares
# on a checkerboard. To discover these I use a turmite testbed which includes a set of turmite pairs as
# starting conditions.
"{{{1,4,0},{0,1,1}},{{1,4,1},{0,2,0}}}", # loose loopy growth (but only works with two ants together, e.g. 2,2)
"{{{0,1,1},{0,4,0}},{{1,2,0},{1,8,1}}}", # spiral growth on its own but changing density + structure with 2 turmites, e.g. 2,2
"{{{1,2,1},{1,1,0}},{{1,1,0},{0,2,1}}}", # extender on its own but mould v. aerials with 2, e.g. 2,3
"{{{1,1,1},{0,8,1}},{{1,8,0},{0,8,0}}}", # together they build a highway
]

'''
We can scale any tumite by a factor of N: (need n_states+N new states)
e.g. for the turmite:
"{{{1,2,0},{0,1,0}}}",  # normal binary counter
"{{{1,2,1},{0,1,1}},{{0,1,0},{0,0,0}}}",  # scale 2 binary counter (sparse version)
(Takes twice as long.)
"{{{1,2,1},{0,1,1}},{{0,1,2},{0,0,0}},{{0,1,0},{0,0,0}}}",  # scale 3 binary counter, etc.
(Takes three times as long.)
Note that the state 1+ color 1+ transitions are never used if started on a zero background -
we set them to {0,0,0} (die) simply to make this clear. Or you can step these cells. Or redraw them.
e.g. for Langton's ant:
"{{{1, 2, 0}, {0, 8, 0}}}",  # normal Langton's ant
"{{{1,2,1},{0,8,1}},{{0,1,0},{0,0,0}}}", # Langton's Ant at scale 2 (sparse version)
(See also Ed Pegg Jr.'s rule #30.)
This is a bit like Langton's original formulation of his ants, only using 2 colors instead of 3, and
with states to keep track of when to turn instead of using color for this.

We can rotate any turmites too: (need e.g. 2*n_states states for 45 degree rotation)
e.g. for the binary counter:
"{{{1,2,0},{0,1,0}}}",  # normal binary counter
"{{{1,1,1},{0,8,1}},{{0,2,0},{0,0,0}}}", # diagonal counter (turning right on off-steps)
"{{{1,4,1},{0,2,1}},{{0,8,0},{0,0,0}}}", # diagonal counter (turning left on off-steps)
e.g. for Langton's ant:
"{{{1,2,0},{0,8,0}}}",  # normal Langton's ant
"{{{1,1,1},{0,4,1}},{{0,2,0},{0,0,0}}}", # Langton's ant at 45 degrees (turning right on off-steps)
"{{{1,4,1},{0,1,1}},{{0,8,0},{0,0,0}}}", # Langton's ant at 45 degrees (turning left on off-steps)
(These take twice as long as the normal versions.)
"{{{1,1,1},{0,4,1}},{{0,2,2},{0,0,0}},{{0,1,0},{0,0,0}}}", # knight's move rotated
"{{{1,1,1},{0,4,1}},{{0,1,2},{0,0,0}},{{0,2,0},{0,0,0}}}", # knight's move rotated v2
(These take three times as long.)
We can increase the number of straight-ahead steps to achieve any angle of highway we want, e.g.
"{{{1,1,1},{0,4,1}},{{0,2,2},{0,0,0}},{{0,1,3},{0,0,0}},{{0,1,0},{0,0,0}}}", # long knight's move rotated
We can rotate twice, for fun:
"{{{1,8,1},{0,2,1}},{{0,2,2},{0,0,0}},{{0,1,3},{0,0,0}},{{0,2,0},{0,0,0}}}", # rotated twice (expanded version)
(same as scale 2 version)
(These take four times as long.)
"{{{1,8,1},{0,2,1}},{{0,2,2},{1,2,2}},{{0,2,0},{1,2,0}}}", # rotated twice (compact version)
Note that with the compact version we have to walk over 'live' cells without changing them - we can't
set those transitions to 'die'.
This makes exactly the same pattern as Langtons Ant but takes 3 times as long.

For turmites with more states, e.g.:
"{{{1,8,0},{1,2,1}},{{0,2,0},{0,8,1}}}", # 11: framed computer art
"{{{1,4,2},{1,1,3}},{{0,1,2},{0,4,3}},{{0,2,0},{0,0,0}},{{0,2,1},{0,0,0}}}", # framed computer art rotated
# 2 states become 4 states for 45 degree rotation
# (sparse version, turning right on off-steps: hence turns N becomes L,L->U,R->N,U->R)
# here the new states are used as off-step storage of the old states: state 2 says I will become state 0,
# state 3 says I will become state 1

Can we detect when a smaller version of a turmite is possible - can we
apply it in reverse?

1=noturn, 2=right, 4=u-turn, 8=left
'''

# http://bytes.com/topic/python/answers/25176-list-subsets
get_subsets = lambda items: [[x for (pos,x) in zip(list(range(len(items))), items) if (2**pos) & switches] for switches in range(2**len(items))]

example_spec = "{{{1, 8, 1}, {1, 8, 1}}, {{1, 2, 1}, {0, 1, 0}}}"

# Generate a random rule, filtering out the most boring ones.
# TODO:
# - if turmite can get stuck in period-2 cycles then rule is bad (or might it avoid them?)
# - (extending) if turmite has (c,2 (or 8),s) for state s and color c then will loop on the spot (unlikely to avoid?)
ns = 2
nc = 2
while True: # (we break out if ok)
    example_spec = '{'
    for state in range(ns):
        if state > 0:
            example_spec += ','
        example_spec += '{'
        for color in range(nc):
            if color > 0:
                example_spec += ','
            new_state = random.randint(0,ns-1)
            new_color = random.randint(0,nc-1)
            dir_to_turn = [1,2,4,8][random.randint(0,3)] # (we don't consider splitting and dying here)
            example_spec += '{' + str(new_color) + "," + str(dir_to_turn) + "," + str(new_state) + '}'
        example_spec += '}'
    example_spec += '}'
    is_rule_acceptable = True
    action_table = eval(example_spec.replace('}',']').replace('{','['))
    # will turmite zip off? if on color 0 with state s the turmite does (?,1,s) then rule is bad
    for state in range(ns):
        if action_table[state][0][1]==1 and action_table[state][0][2]==state:
            is_rule_acceptable = False
    # does Turmite change at least one color?
    changes_one = False
    for state in range(ns):
        for color in range(nc):
            if not action_table[state][color][0] == color:
                changes_one = True
    if not changes_one:
        is_rule_acceptable = False
    # does Turmite ever turn?
    turmite_turns = False
    for state in range(ns):
        for color in range(nc):
            if not action_table[state][color][0] in [1,4]: # forward or u-turn
                turmite_turns = True
    if not turmite_turns:
        is_rule_acceptable = False
    # does turmite get stuck in any subset of states?
    for subset in get_subsets(list(range(ns))):
        if len(subset)==0 or len(subset)==ns: # (just an optimisation)
            continue
        leaves_subset = False
        for state in subset:
            for color in range(nc):
                if not action_table[state][color][2] in subset:
                    leaves_subset = True
        if not leaves_subset:
            is_rule_acceptable = False
            break # (just an optimisation)
    # does turmite wobble on the spot? (u-turn that doesn't change color or state)
    for state in range(ns):
        for color in range(nc):
            if action_table[state][color][0]==color and action_table[state][color][1]==4 and action_table[state][color][2]==state:
                is_rule_acceptable = False
    # so was the rule acceptable, in the end?
    if is_rule_acceptable:
        break

spec = golly.getstring(
'''This script will create a Turmite CA for a given specification.

Enter a number (1-59) for an example Turmite from Ed Pegg Jr.'s (extended) library.
(see http://demonstrations.wolfram.com/Turmites/ or the code of this script)

Otherwise enter a specification string: a curly-bracketed table of n_states rows
and n_colors columns, where each entry is a triple of integers.
The elements of each triple are:
a: the new color of the square
b: the direction(s) for the Turmite to turn (1=noturn, 2=right, 4=u-turn, 8=left)
c: the new internal state of the Turmite

Example:
{{{1, 2, 0}, {0, 8, 0}}}
(example pattern #1)
Has 1 state and 2 colors. The triple {1,2,0} says:
1. set the color of the square to 1
2. turn right (2)
3. adopt state 0 (no change) and move forward one square
This is Langton's Ant.

Enter integer or string:
(default is a random example)''', example_spec, 'Enter Turmite specification:')

if spec.isdigit():
    spec = EdPeggJrTurmiteLibrary[int(spec)-1] # (his demonstration project UI is 1-based)

# convert the specification string into action_table[state][color]
# (convert Mathematica code to Python and run eval)
action_table = eval(spec.replace('}',']').replace('{','['))
n_states = len(action_table)
n_colors = len(action_table[0])
# (N.B. The terminology 'state' here refers to the internal state of the finite
#       state machine that each Turmite is using, not the contents of each Golly
#       cell. We use the term 'color' to denote the symbol on the 2D 'tape'. The
#       actual 'Golly state' in this emulation of Turmites is given by the
#       "encoding" section below.)
n_dirs=4

# TODO: check table is full and valid

total_states = n_colors+n_colors*n_states*n_dirs

# problem if we try to export more than 255 states
if total_states > 255:
    golly.warn("Number of states required exceeds Golly's limit of 255.")
    golly.exit()

# encoding:
# (0-n_colors: empty square)
def encode(c,s,d):
    # turmite on color c in state s facing direction d
    return n_colors + n_dirs*(n_states*c+s) + d

# http://rightfootin.blogspot.com/2006/09/more-on-python-flatten.html
def flatten(l, ltypes=(list, tuple)):
    ltype = type(l)
    l = list(l)
    i = 0
    while i < len(l):
        while isinstance(l[i], ltypes):
            if not l[i]:
                l.pop(i)
                i -= 1
                break
            else:
                l[i:i + 1] = l[i]
        i += 1
    return ltype(l)

# convert the string to a form we can embed in a filename
spec_string = ''.join(map(str,[hex(x)[2:] for x in flatten(action_table)]))
# (ambiguous but we have to try something)

# what direction would a turmite have been facing to end up here from direction
# d if it turned t: would_have_been_facing[t][d]
would_have_been_facing={
1:[2,3,0,1], # no turn
2:[1,2,3,0], # right
4:[0,1,2,3], # u-turn
8:[3,0,1,2], # left
}

remap = [2,1,3,0] # N,E,S,W -> S,E,W,N

not_arriving_from_here = [list(range(n_colors)) for i in range(n_dirs)] # (we're going to modify them)
for color in range(n_colors):
    for state in range(n_states):
        turnset = action_table[state][color][1]
        for turn in [1,2,4,8]:
            if not turn&turnset: # didn't turn this way
                for dir in range(n_dirs):
                    facing = would_have_been_facing[turn][dir]
                    not_arriving_from_here[dir] += [encode(color,state,facing)]

#golly.warn(str(not_arriving_from_here))

# What states leave output_color behind?
leaving_color_behind = {}
for output_color in range(n_colors):
    leaving_color_behind[output_color] = [output_color] # (no turmite present)
    for state in range(n_states):
        for color in range(n_colors):
            if action_table[state][color][0]==output_color:
                leaving_color_behind[output_color] += [encode(color,state,d) for d in range(n_dirs)] # any direction

tree = RuleTree(total_states,4)

# A single turmite is entering this square:
for s in range(n_states):
    # collect all the possibilities for a turmite to arrive in state s...
    inputs_sc = []
    for state in range(n_states):
        for color in range(n_colors):
            if action_table[state][color][2]==s:
                inputs_sc += [(state,color)]
    # ...from direction dir
    for dir in range(n_dirs):
        inputs = []
        for state,color in inputs_sc:
            turnset = action_table[state][color][1] # sum of all turns
            inputs += [encode(color,state,would_have_been_facing[turn][dir]) for turn in [1,2,4,8] if turn&turnset]
        if len(inputs)==0:
            continue
        for central_color in range(n_colors):
            # output the required transition
            ### AKT: this code causes syntax error in Python 2.3:
            ### transition_inputs = [leaving_color_behind[central_color]] + \
            ###     [ inputs if i==dir else not_arriving_from_here[i] for i in remap ]
            transition_inputs = [leaving_color_behind[central_color]]
            for i in remap:
                if i==dir:
                    transition_inputs.append(inputs)
                else:
                    transition_inputs.append(not_arriving_from_here[i])
            transition_output = encode(central_color,s,opposite_dirs[dir])
            tree.add_rule( transition_inputs, transition_output )

# default: square is left with no turmite present
for output_color,inputs in list(leaving_color_behind.items()):
    tree.add_rule([inputs]+[list(range(total_states))]*4,output_color)

rule_name = prefix+'_'+spec_string
tree.write(golly.getdir('rules')+rule_name+'.tree')

# Create some multi-colour icons so we can see what the turmite is doing
# Andrew's ant drawing: (with added eyes (2) and anti-aliasing (3))
ant31x31 = [[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,1,1,1,2,2,1,1,1,2,2,1,1,1,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,2,2,1,1,1,2,2,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,1,1,0,0,0,1,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,1,1,0,0,0,1,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,1,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,1,1,0,0,0,1,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,1,1,0,0,0,3,1,1,1,3,0,0,0,1,1,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,3,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]]
ant15x15 = [[0,0,0,0,1,0,0,0,0,0,1,0,0,0,0],
            [0,0,0,0,0,1,0,0,0,1,0,0,0,0,0],
            [0,0,0,0,0,0,2,1,2,0,0,0,0,0,0],
            [0,0,0,1,0,0,1,1,1,0,0,1,0,0,0],
            [0,0,0,0,1,0,0,1,0,0,1,0,0,0,0],
            [0,0,0,0,0,1,1,1,1,1,0,0,0,0,0],
            [0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
            [0,0,0,0,1,1,1,1,1,1,1,0,0,0,0],
            [0,0,0,1,0,0,0,1,0,0,0,1,0,0,0],
            [0,0,0,0,0,1,1,1,1,1,0,0,0,0,0],
            [0,0,0,0,1,0,0,1,0,0,1,0,0,0,0],
            [0,0,0,1,0,0,3,1,3,0,0,1,0,0,0],
            [0,0,0,0,0,0,1,1,1,0,0,0,0,0,0],
            [0,0,0,0,0,0,3,1,3,0,0,0,0,0,0],
            [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]]
ant7x7 = [  [0,1,0,0,0,1,0],
            [0,0,2,1,2,0,0],
            [0,0,0,1,0,0,0],
            [0,1,1,1,1,1,0],
            [0,0,0,1,0,0,0],
            [0,1,1,1,1,1,0],
            [0,0,0,1,0,0,0] ]
palette=[[0,0,0],[0,155,67],[127,0,255],[128,128,128],[185,184,96],[0,100,255],[196,255,254],
    [254,96,255],[126,125,21],[21,126,125],[255,116,116],[116,255,116],[116,116,255],
    [228,227,0],[28,255,27],[255,27,28],[0,228,227],[227,0,228],[27,28,255],[59,59,59],
    [234,195,176],[175,196,255],[171,194,68],[194,68,171],[68,171,194],[72,184,71],[184,71,72],
    [71,72,184],[169,255,188],[252,179,63],[63,252,179],[179,63,252],[80,9,0],[0,80,9],[9,0,80],
    [255,175,250],[199,134,213],[115,100,95],[188,163,0],[0,188,163],[163,0,188],[203,73,0],
    [0,203,73],[73,0,203],[94,189,0],[189,0,94]]
eyes = (255,255,255)
rotate4 = [ [[1,0],[0,1]], [[0,-1],[1,0]], [[-1,0],[0,-1]], [[0,1],[-1,0]] ]
offset4 = [ [0,0], [1,0], [1,1], [0,1] ]
pixels = [[palette[0] for column in list(range(total_states))*31] for row in range(53)]
for state in range(n_states):
    for color in range(n_colors):
        for dir in range(n_dirs):
            bg_col = palette[color]
            fg_col = palette[state+n_colors]
            mid = [(f+b)//2 for f,b in zip(fg_col,bg_col)]
            for x in range(31):
                for y in range(31):
                    column = (encode(color,state,dir)-1)*31 + rotate4[dir][0][0]*x + \
                        rotate4[dir][0][1]*y + offset4[dir][0]*30
                    row = rotate4[dir][1][0]*x + rotate4[dir][1][1]*y + offset4[dir][1]*30
                    pixels[row][column] = [bg_col,fg_col,eyes,mid][ant31x31[y][x]]
            for x in range(15):
                for y in range(15):
                    column = (encode(color,state,dir)-1)*31 + rotate4[dir][0][0]*x + \
                        rotate4[dir][0][1]*y + offset4[dir][0]*14
                    row = 31 + rotate4[dir][1][0]*x + rotate4[dir][1][1]*y + offset4[dir][1]*14
                    pixels[row][column] = [bg_col,fg_col,eyes,mid][ant15x15[y][x]]
            for x in range(7):
                for y in range(7):
                    column = (encode(color,state,dir)-1)*31 + rotate4[dir][0][0]*x + \
                        rotate4[dir][0][1]*y + offset4[dir][0]*6
                    row = 46 + rotate4[dir][1][0]*x + rotate4[dir][1][1]*y + offset4[dir][1]*6
                    pixels[row][column] = [bg_col,fg_col,eyes,mid][ant7x7[y][x]]
for color in range(n_colors):
    bg_col = palette[color]
    for row in range(31):
        for column in range(31):
            pixels[row][(color-1)*31+column] = bg_col
    for row in range(15):
        for column in range(15):
            pixels[31+row][(color-1)*31+column] = bg_col
    for row in range(7):
        for column in range(7):
            pixels[46+row][(color-1)*31+column] = bg_col

# use rule_name.tree and icon info to create rule_name.rule
ConvertTreeToRule(rule_name, total_states, pixels)

# switch to the new rule
golly.new(rule_name+'-demo.rle')
golly.setalgo('RuleLoader')
golly.setrule(rule_name)
golly.setcell(0,0,encode(0,0,0)) # start with a single turmite
golly.show('Created '+rule_name+'.rule and selected that rule.')

'''
# we make a turmite testbed so we don't miss interesting behaviour

# create an area of random initial configuration with a few turmites
for x in range(-300,-100):
    for y in range(-100,100):
        if x%50==0 and y%50==0:
            golly.setcell(x,y,n_colors) # start with a turmite facing N
        else:
            golly.setcell(x,y,random.randint(0,n_colors-1))

# create a totally random area (many turmites)
for x in range(-200,-100):
    for y in range(200,300):
        golly.setcell(x,y,random.randint(0,total_states-1))

# also start with some pairs of turmites
# (because of checkerboard rule, sometimes turmites work in pairs)
pair_separation = 30
x=0
y=100
for t1 in range(n_colors,total_states):
    for t2 in range(n_colors,total_states):
        golly.setcell(x,y,t1)
        golly.setcell(x+1,y,t2)
        x += pair_separation
    x = 0
    y += pair_separation

# also start with a filled-in area of a color
for color in range(n_colors):
    for x in range(color*200,color*200+100):
        for y in range(-50,50):
            if x==color*200+50 and y==0:
                golly.setcell(x,y,encode(color,0,0)) # start with a turmite facing N
            else:
                golly.setcell(x,y,color)
golly.fit()
'''
