# Generator for Triangular Turmite rules

import golly
import random
import string
from glife.EmulateTriangular import *
from glife.WriteRuleTable import *

prefix = 'TriTurmite'

# http://bytes.com/topic/python/answers/25176-list-subsets
get_subsets = lambda items: [[x for (pos,x) in zip(list(range(len(items))), items) if (2**pos) & switches] for switches in range(2**len(items))]

# Generate a random rule, while filtering out the dull ones.
# More to try:
# - if turmite can get stuck in period-2 cycles then rule is bad (or might it avoid them?)
# - (extending) if turmite has (c,2 (or 8),s) for state s and color c then will loop on the spot (unlikely to avoid?)
example_spec = '{{{1, 2, 0}, {0, 1, 0}}}'
import random
ns = 2
nc = 3
while True: # (we break out if ok)
    example_spec = '{'
    for state in range(ns):
        if state > 0:
            example_spec += ','
        example_spec += '{'
        for color in range(nc):
            if color > 0:
                example_spec += ','
            new_color = random.randint(0,nc-1)
            dir_to_turn = [1,2,4][random.randint(0,2)] # (we don't consider splitting and dying here)
            new_state = random.randint(0,ns-1)
            example_spec += '{' + str(new_color) + "," + str(dir_to_turn) + "," + str(new_state) + '}'
        example_spec += '}'
    example_spec += '}'
    is_rule_acceptable = True
    action_table = eval(example_spec.replace('}',']').replace('{','['))
    # does Turmite change at least one color?
    changes_one = False
    for state in range(ns):
        for color in range(nc):
            if not action_table[state][color][0] == color:
                changes_one = True
    if not changes_one:
        is_rule_acceptable = False
    # does Turmite write every non-zero color?
    colors_written = set([])
    for state in range(ns):
        for color in range(nc):
            colors_written.add(action_table[state][color][0])
    if not colors_written==set(range(1,nc)):
        is_rule_acceptable = False
    # does Turmite ever turn?
    turmite_turns = False
    for state in range(ns):
        for color in range(nc):
            if not action_table[state][color][0] in [4]: # u-turn
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
'''This script will create a TriTurmite CA for a given specification.

Enter a specification string: a curly-bracketed table of n_states rows
and n_colors columns, where each entry is a triple of integers.
The elements of each triple are:
a: the new color of the square
b: the direction(s) for the Turmite to turn (1=Left, 2=Right, 4=U-turn)
c: the new internal state of the Turmite

Example:
{{{1, 2, 0}, {0, 1, 0}}}
Has 1 state and 2 colors. The triple {1,2,0} says:
1. set the color of the square to 1
2. turn right (2)
3. adopt state 0 (no change) and move forward one square
This is the equivalent of Langton's Ant.

Enter string:
''', example_spec, 'Enter TriTurmite specification:')

'''Some interesting rule found with this script:
{{{2,4,0},{2,4,0},{1,2,1}},{{1,2,1},{2,1,0},{1,4,1}}} - lightning cloud
{{{1,1,1},{1,2,0},{2,1,1}},{{2,2,1},{2,2,1},{1,4,0}}} - makes a highway (seems to be rarer in tri grids compared to square grids?)
{{{2,2,1},{1,2,0},{1,1,1}},{{1,2,0},{2,1,0},{1,4,1}}} - data pyramid
{{{2,1,0},{1,4,1},{1,1,0}},{{2,4,0},{2,2,1},{1,1,1}}} - a filled version of the tri-grid Langton's ant
{{{1,1,0},{2,2,1},{1,1,0}},{{1,4,0},{2,2,0},{2,2,0}}} - hypnodisc
'''

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
n_dirs = 3

# TODO: check table is full and valid

total_states = n_colors+n_colors*n_states*3

# problem if we try to export more than 255 states
if total_states > 128: # max allowed using checkerboard emulation (see EmulateTriangular)
    golly.warn("Number of states required exceeds Golly's limit of 255.")
    golly.exit()

# encoding:
# (0-n_colors: empty square)
def encode(c,s,d):
    # turmite on color c in state s facing away from direction d
    return n_colors + 3*(n_states*c+s) + d

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
1:[2,0,1], # left
2:[1,2,0], # right
4:[0,1,2], # u-turn
}

not_arriving_from_here = [list(range(n_colors)) for i in range(n_dirs)] # (we're going to modify them)
for color in range(n_colors):
    for state in range(n_states):
        turnset = action_table[state][color][1]
        for turn in [1,2,4]:
            if not turn&turnset: # didn't turn this way
                for dir in range(n_dirs):
                    facing = would_have_been_facing[turn][dir]
                    not_arriving_from_here[dir] += [encode(color,state,facing)]

# What states leave output_color behind?
leaving_color_behind = {}
for output_color in range(n_colors):
    leaving_color_behind[output_color] = [output_color] # (no turmite present)
    for state in range(n_states):
        for color in range(n_colors):
            if action_table[state][color][0]==output_color:
                leaving_color_behind[output_color] += [encode(color,state,d) for d in range(n_dirs)] # any direction

# we can't build the rule tree directly so we collect the transitions ready for emulation
transitions = []

# A single turmite is entering this square:
for s in range(n_states):
    for dir in range(n_dirs):
        # collect all the possibilities for a turmite to arrive in state s from direction dir
        inputs = []
        for state in range(n_states):
            for color in range(n_colors):
                if action_table[state][color][2]==s:
                    turnset = action_table[state][color][1] # sum of all turns
                    inputs += [encode(color,state,would_have_been_facing[turn][dir]) \
                               for turn in [1,2,4] if turn&turnset]
        if len(inputs)>0:
            for central_color in range(n_colors):
                # output the required transition
                ### AKT: this code causes syntax error in Python 2.3:
                ### transition = [leaving_color_behind[central_color]] + \
                ###     [ inputs if i==dir else not_arriving_from_here[i] for i in range(n_dirs) ] + \
                ###     [ [encode(central_color,s,dir)] ]
                transition = [leaving_color_behind[central_color]]
                for i in range(n_dirs):
                    if i==dir:
                        transition.append(inputs)
                    else:
                        transition.append(not_arriving_from_here[i])
                transition += [ [encode(central_color,s,dir)] ]
                transitions += [transition]

# default: square is left with no turmite present
for output_color,inputs in list(leaving_color_behind.items()):
    transition = [inputs]+[list(range(total_states))]*n_dirs+[[output_color]]
    transitions += [transition]

rule_name = prefix+'_'+spec_string

# To see the intermediate output as a rule table (can use RuleTableToTree.py to load it):
#WriteRuleTable("triangularVonNeumann",total_states,transitions,golly.getdir('rules')+rule_name+'_asTable.table')

# -- make some icons --

palette=[[0,0,0],[0,155,67],[127,0,255],[128,128,128],[185,184,96],[0,100,255],[196,255,254],
    [254,96,255],[126,125,21],[21,126,125],[255,116,116],[116,255,116],[116,116,255],
    [228,227,0],[28,255,27],[255,27,28],[0,228,227],[227,0,228],[27,28,255],[59,59,59],
    [234,195,176],[175,196,255],[171,194,68],[194,68,171],[68,171,194],[72,184,71],[184,71,72],
    [71,72,184],[169,255,188],[252,179,63],[63,252,179],[179,63,252],[80,9,0],[0,80,9],[9,0,80],
    [255,175,250],[199,134,213],[115,100,95],[188,163,0],[0,188,163],[163,0,188],[203,73,0],
    [0,203,73],[73,0,203],[94,189,0],[189,0,94]]

if total_states<=16:

    TriangularTransitionsToRuleTree_SplittingMethod("triangularVonNeumann",total_states,transitions,rule_name)

    width = 15*(total_states*total_states-1) + 15 # we set the background color
    height = 22
    pixels = [[(0,0,0) for x in range(width)] for y in range(height)]

    # turmite icons: 0=black, 1=background color, 2=turmite color
    turmite_big =  [[[0,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
                     [1,0,1,1,1,1,1,1,1,1,1,1,1,1,1],
                     [1,1,0,1,1,1,1,1,1,2,2,2,2,1,1],
                     [1,1,1,0,1,1,1,1,1,1,1,2,2,1,1],
                     [1,1,1,1,0,1,1,1,1,1,2,1,2,1,1],
                     [1,1,1,1,1,0,1,1,1,2,1,1,2,1,1],
                     [1,1,1,1,1,1,0,1,2,1,1,1,1,1,1],
                     [1,1,1,1,1,1,1,0,1,1,1,1,1,1,1],
                     [1,1,1,1,1,1,2,1,0,1,1,1,1,1,1],
                     [1,1,2,1,1,2,1,1,1,0,1,1,1,1,1],
                     [1,1,2,1,2,1,1,1,1,1,0,1,1,1,1],
                     [1,1,2,2,1,1,1,1,1,1,1,0,1,1,1],
                     [1,1,2,2,2,2,1,1,1,1,1,1,0,1,1],
                     [1,1,1,1,1,1,1,1,1,1,1,1,1,0,1],
                     [1,1,1,1,1,1,1,1,1,1,1,1,1,1,0]],
                    [[0,1,1,1,1,1,1,1,2,1,1,1,1,1,1],
                     [1,0,1,1,1,1,1,1,2,1,1,1,1,1,1],
                     [1,1,0,1,1,1,1,1,1,2,1,1,1,1,1],
                     [1,1,1,0,1,1,1,1,1,2,1,1,1,1,1],
                     [1,1,1,1,0,1,1,1,1,1,2,1,1,1,1],
                     [1,1,2,1,1,0,1,1,1,1,2,1,2,1,1],
                     [1,1,2,2,1,1,0,1,1,1,1,2,2,1,1],
                     [1,1,2,2,2,1,1,0,1,1,2,2,2,1,1],
                     [1,1,2,2,1,1,1,1,0,1,1,2,2,1,1],
                     [1,1,2,1,2,1,1,1,1,0,1,1,2,1,1],
                     [1,1,1,1,2,1,1,1,1,1,0,1,1,1,1],
                     [1,1,1,1,1,2,1,1,1,1,1,0,1,1,1],
                     [1,1,1,1,1,2,1,1,1,1,1,1,0,1,1],
                     [1,1,1,1,1,1,2,1,1,1,1,1,1,0,1],
                     [1,1,1,1,1,1,2,1,1,1,1,1,1,1,0]],
                    [[0,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
                     [1,0,1,1,1,1,1,1,1,1,1,1,1,1,1],
                     [1,1,0,1,1,2,2,2,2,2,1,1,1,1,1],
                     [1,1,1,0,1,1,2,2,2,1,1,1,1,1,1],
                     [1,1,1,1,0,1,1,2,1,2,2,1,1,1,1],
                     [1,1,1,1,1,0,1,1,1,1,1,2,2,1,1],
                     [1,1,1,1,1,1,0,1,1,1,1,1,1,2,2],
                     [1,1,1,1,1,1,1,0,1,1,1,1,1,1,1],
                     [2,2,1,1,1,1,1,1,0,1,1,1,1,1,1],
                     [1,1,2,2,1,1,1,1,1,0,1,1,1,1,1],
                     [1,1,1,1,2,2,1,2,1,1,0,1,1,1,1],
                     [1,1,1,1,1,1,2,2,2,1,1,0,1,1,1],
                     [1,1,1,1,1,2,2,2,2,2,1,1,0,1,1],
                     [1,1,1,1,1,1,1,1,1,1,1,1,1,0,1],
                     [1,1,1,1,1,1,1,1,1,1,1,1,1,1,0]]]
    turmite_small = [[[0,1,1,1,1,1,1],
                      [1,0,1,2,2,2,1],
                      [1,1,0,1,1,2,1],
                      [1,2,1,0,1,2,1],
                      [1,2,1,1,0,1,1],
                      [1,2,2,2,1,0,1],
                      [1,1,1,1,1,1,0]],
                     [[0,1,1,1,1,1,1],
                      [1,0,1,1,1,2,1],
                      [1,1,0,1,2,2,1],
                      [1,2,1,0,1,2,1],
                      [1,2,2,1,0,1,1],
                      [1,2,1,1,1,0,1],
                      [1,1,1,1,1,1,0]],
                     [[0,1,1,1,1,1,1],
                      [1,0,1,2,2,2,1],
                      [1,1,0,1,2,1,1],
                      [1,1,1,0,1,1,1],
                      [1,1,2,1,0,1,1],
                      [1,2,2,2,1,0,1],
                      [1,1,1,1,1,1,0]]]

    # TODO: do something about this horrible code
    for lc in range(n_colors):
        for uc in range(n_colors):
            '''draw the cells with no turmites'''
            golly_state = uc * total_states + lc
            for row in range(15):
                for column in range(15):
                    if column>row:
                        # upper
                        pixels[row][(golly_state-1)*15+column] = palette[uc]
                    elif column<row:
                        # lower
                        pixels[row][(golly_state-1)*15+column] = palette[lc]
            for row in range(7):
                for column in range(7):
                    if column>row:
                        # upper
                        pixels[15+row][(golly_state-1)*15+column] = palette[uc]
                    elif column<row:
                        # lower
                        pixels[15+row][(golly_state-1)*15+column] = palette[lc]
            '''draw the cells with a turmite in the upper half'''
            for us in range(n_states):
                for ud in range(n_dirs):
                    upper = encode(uc,us,ud)
                    golly_state = upper * total_states + lc
                    for row in range(15):
                        for column in range(15):
                            if column>row:
                                # upper
                                bg_col = palette[uc]
                                fg_col = palette[n_colors+us]
                                pixels[row][(golly_state-1)*15+column] = [palette[0],bg_col,fg_col][turmite_big[ud][row][column]]
                            elif column<row:
                                # lower
                                pixels[row][(golly_state-1)*15+column] = palette[lc]
                    for row in range(7):
                        for column in range(7):
                            if column>row:
                                # upper
                                bg_col = palette[uc]
                                fg_col = palette[n_colors+us]
                                pixels[15+row][(golly_state-1)*15+column] = [palette[0],bg_col,fg_col][turmite_small[ud][row][column]]
                            elif column<row:
                                # lower
                                pixels[15+row][(golly_state-1)*15+column] = palette[lc]
        for ls in range(n_states):
            for ld in range(n_dirs):
                lower = encode(lc,ls,ld)
                for uc in range(n_colors):
                    '''draw the cells with a turmite in the lower half'''
                    golly_state = uc * total_states + lower
                    for row in range(15):
                        for column in range(15):
                            if row>column:
                                # lower
                                bg_col = palette[lc]
                                fg_col = palette[n_colors+ls]
                                pixels[row][(golly_state-1)*15+column] = [palette[0],bg_col,fg_col][turmite_big[ld][row][column]]
                            elif column>row:
                                # upper
                                pixels[row][(golly_state-1)*15+column] = palette[uc]
                    for row in range(7):
                        for column in range(7):
                            if row>column:
                                # lower
                                bg_col = palette[lc]
                                fg_col = palette[n_colors+ls]
                                pixels[15+row][(golly_state-1)*15+column] = [palette[0],bg_col,fg_col][turmite_small[ld][row][column]]
                            elif column>row:
                                # upper
                                pixels[15+row][(golly_state-1)*15+column] = palette[uc]
                    '''draw the cells with a turmite in both halves'''
                    for us in range(n_states):
                        for ud in range(n_dirs):
                            upper = encode(uc,us,ud)
                            golly_state = upper * total_states + lower
                            for row in range(15):
                                for column in range(15):
                                    if row>column:
                                        # lower
                                        bg_col = palette[lc]
                                        fg_col = palette[n_colors+ls]
                                        pixels[row][(golly_state-1)*15+column] = [palette[0],bg_col,fg_col][turmite_big[ld][row][column]]
                                    elif column>row:
                                        # upper
                                        bg_col = palette[uc]
                                        fg_col = palette[n_colors+us]
                                        pixels[row][(golly_state-1)*15+column] = [palette[0],bg_col,fg_col][turmite_big[ud][row][column]]
                            for row in range(7):
                                for column in range(7):
                                    if row>column:
                                        # lower
                                        bg_col = palette[lc]
                                        fg_col = palette[n_colors+ls]
                                        pixels[15+row][(golly_state-1)*15+column] = [palette[0],bg_col,fg_col][turmite_small[ld][row][column]]
                                    elif column>row:
                                        # upper
                                        bg_col = palette[uc]
                                        fg_col = palette[n_colors+us]
                                        pixels[15+row][(golly_state-1)*15+column] = [palette[0],bg_col,fg_col][turmite_small[ud][row][column]]

    ConvertTreeToRule(rule_name, total_states, pixels)

elif total_states<=128:

    TriangularTransitionsToRuleTree_CheckerboardMethod("triangularVonNeumann",total_states,transitions,rule_name)

    width = 15*(total_states*2-2) + 15 # we set the background color
    height = 22
    pixels = [[(0,0,0) for x in range(width)] for y in range(height)]

    # turmite icons: 0=black, 1=background color, 2=turmite color
    lower = [[[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,1,1,1,0,0,0,0,0,0],
              [0,0,0,0,0,1,1,1,1,1,0,0,0,0,0],
              [0,0,0,0,1,1,1,1,1,1,2,0,0,0,0],
              [0,0,0,1,1,1,1,1,2,2,1,1,0,0,0],
              [0,0,1,1,2,1,2,2,1,1,1,1,1,0,0],
              [0,1,1,2,2,2,1,1,1,1,1,1,1,1,0],
              [1,1,2,2,2,2,2,1,1,1,1,1,1,1,1],
              [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]],
             [[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,1,1,1,0,0,0,0,0,0],
              [0,0,0,0,0,1,1,1,1,1,0,0,0,0,0],
              [0,0,0,0,1,1,1,2,1,1,1,0,0,0,0],
              [0,0,0,1,1,1,2,2,2,1,1,1,0,0,0],
              [0,0,1,1,1,2,2,2,2,2,1,1,1,0,0],
              [0,1,1,1,1,1,1,2,1,1,1,1,1,1,0],
              [1,1,1,1,1,1,1,2,1,1,1,1,1,1,1],
              [1,1,1,1,1,1,1,2,1,1,1,1,1,1,1],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]],
             [[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,1,1,1,0,0,0,0,0,0],
              [0,0,0,0,0,1,1,1,1,1,0,0,0,0,0],
              [0,0,0,0,2,1,1,1,1,1,1,0,0,0,0],
              [0,0,0,1,1,2,2,1,1,1,1,1,0,0,0],
              [0,0,1,1,1,1,1,2,2,1,2,1,1,0,0],
              [0,1,1,1,1,1,1,1,1,2,2,2,1,1,0],
              [1,1,1,1,1,1,1,1,2,2,2,2,2,1,1],
              [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
              [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]]]
    lower7x7 = [[[0,0,0,0,0,0,0],
                 [0,0,0,0,0,0,0],
                 [0,0,0,1,0,0,0],
                 [0,0,1,1,1,0,0],
                 [0,1,2,2,1,1,0],
                 [1,1,1,1,1,1,1],
                 [0,0,0,0,0,0,0]],
                [[0,0,0,0,0,0,0],
                 [0,0,0,0,0,0,0],
                 [0,0,0,1,0,0,0],
                 [0,0,1,2,1,0,0],
                 [0,1,2,1,2,1,0],
                 [1,1,1,1,1,1,1],
                 [0,0,0,0,0,0,0]],
                [[0,0,0,0,0,0,0],
                 [0,0,0,0,0,0,0],
                 [0,0,0,1,0,0,0],
                 [0,0,1,1,1,0,0],
                 [0,1,1,2,2,1,0],
                 [1,1,1,1,1,1,1],
                 [0,0,0,0,0,0,0]]]
    # (we invert the lower triangle to get the upper triangle)

    for color in range(n_colors):
        bg_color = palette[color]
        # draw the 15x15 icon
        for row in range(15):
            for column in range(15):
                # lower triangle
                pixels[row][(color-1)*15+column] = \
                    [palette[0],bg_color,bg_color][lower[0][row][column]]
                # upper triangle
                pixels[row][(color+total_states-2)*15+column] = \
                    [palette[0],bg_color,bg_color][lower[0][13-row][column]]
        # draw the 7x7 icon
        for row in range(7):
            for column in range(7):
                # lower triangle
                pixels[15+row][(color-1)*15+column] = \
                    [palette[0],bg_color,bg_color][lower7x7[0][row][column]]
                # upper triangle
                pixels[15+row][(color+total_states-2)*15+column] = \
                    [palette[0],bg_color,bg_color][lower7x7[0][6-row][column]]
        for state in range(n_states):
            fg_color = palette[n_colors+state]
            for dir in range(n_dirs):
                # draw the 15x15 icon
                for row in range(15):
                    for column in range(15):
                        # lower triangle
                        pixels[row][(encode(color,state,dir)-1)*15+column] = \
                            [palette[0],bg_color,fg_color][lower[dir][row][column]]
                        # upper triangle
                        pixels[row][(encode(color,state,dir)+total_states-2)*15+column] = \
                            [palette[0],bg_color,fg_color][lower[2-dir][13-row][column]]
                # draw the 7x7 icon
                for row in range(7):
                    for column in range(7):
                        # lower triangle
                        pixels[15+row][(encode(color,state,dir)-1)*15+column] = \
                            [palette[0],bg_color,fg_color][lower7x7[dir][row][column]]
                        # upper triangle
                        pixels[15+row][(encode(color,state,dir)+total_states-2)*15+column] = \
                            [palette[0],bg_color,fg_color][lower7x7[2-dir][6-row][column]]

    ConvertTreeToRule(rule_name, total_states, pixels)

else:

    golly.warn('Too many states!')
    golly.exit()

# -- select the rule --

golly.new(rule_name+'-demo.rle')
golly.setalgo('RuleLoader')
golly.setrule(rule_name)
golly.setcell(0,0,encode(0,0,0)) # start with a single turmite
golly.show('Created '+rule_name+'.rule and selected that rule.')
