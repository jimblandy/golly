# Generator for Absolute-movement Hexagonal Turmite rules

import golly
import random
from glife.EmulateHexagonal import *
from glife.WriteRuleTable import *

# AKT: Python 2.3 doesn't have "set" built-in
try:
    set
except NameError:
    from sets import Set as set

prefix = 'AbsoluteHexTurmite'

dirs = ['A','B','C','D','E','F']
opposite_dirs = [3,4,5,0,1,2]

# http://bytes.com/topic/python/answers/25176-list-subsets
get_subsets = lambda items: [[x for (pos,x) in zip(list(range(len(items))), items) if (2**pos) & switches] for switches in range(2**len(items))]

# Generate a random rule, while filtering out the dull ones.
# More to try:
# - if turmite can get stuck in period-2 cycles then rule is bad (or might it avoid them?)
example_spec = "{{{1, 'A', 0}, {0, 'B', 0}}}"
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
            dir_to_turn = dirs[random.randint(0,len(dirs)-1)] # (we don't consider splitting and dying here)
            new_state = random.randint(0,ns-1)
            example_spec += '{' + str(new_color) + ",'" + dir_to_turn + "'," + str(new_state) + '}'
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
    # so was the rule acceptable, in the end?
    if is_rule_acceptable:
        break

spec = golly.getstring(
'''This script will create an Absolute-movement HexTurmite CA for a given specification.

Enter a specification string: a curly-bracketed table of n_states rows
and n_colors columns, where each entry is a triple of integers.
The elements of each triple are:
a: the new color of the square
b: the direction(s) for the Turmite to move ('A', 'B', .. , 'F')
c: the new internal state of the Turmite

Example:
{{{1, 'A', 0}, {0, 'B', 0}}}
Has 1 state and 2 colors. The triple {1,'A',0} says:
1. set the color of the square to 1
2. move in direction 'A'
3. adopt state 0

Enter string:
''', example_spec, 'Enter AbsoluteHexTurmite specification:')

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
n_dirs = 6

# TODO: check table is full and valid

total_states = n_colors + n_colors*n_states
if total_states > 256:
    golly.warn("Number of states required exceeds Golly's limit of 256.")
    golly.exit()

# encoding:
# (0-n_colors: empty square)
def encode(c,s):
    # turmite on color c in state s
    return n_colors + n_states*c + s

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
spec_string = '_'.join(map(str,flatten(action_table)))
# (ambiguous but we have to try something)

not_arriving_from_here = [list(range(n_colors)) for i in range(n_dirs)] # (we're going to modify them)
for color in range(n_colors):
    for state in range(n_states):
        moveset = action_table[state][color][1]
        for iMove,move in enumerate(dirs):
            if not move in moveset: # didn't turn this way
                not_arriving_from_here[opposite_dirs[iMove]] += [encode(color,state)]

# What states leave output_color behind?
leaving_color_behind = {}
for output_color in range(n_colors):
    leaving_color_behind[output_color] = [output_color] # (no turmite present)
    for state in range(n_states):
        for color in range(n_colors):
            if action_table[state][color][0]==output_color:
                leaving_color_behind[output_color] += [encode(color,state)]

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
                    if dirs[opposite_dirs[dir]] in action_table[state][color][1]:
                        inputs += [encode(color,state)]
        if len(inputs)>0:
            for central_color in range(n_colors):
                # output the required transition
                ### AKT: this code causes syntax error in Python 2.3:
                ### transition = [leaving_color_behind[central_color]] + \
                ###     [ inputs if i==dir else not_arriving_from_here[i] for i in range(n_dirs) ] + \
                ###     [ [encode(central_color,s)] ]
                transition = [leaving_color_behind[central_color]]
                for i in range(n_dirs):
                    if i==dir:
                        transition.append(inputs)
                    else:
                        transition.append(not_arriving_from_here[i])
                transition += [ [encode(central_color,s)] ]
                transitions += [transition]

# default: square is left with no turmite present
for output_color,inputs in list(leaving_color_behind.items()):
    transition = [inputs]+[list(range(total_states))]*n_dirs+[[output_color]]
    transitions += [transition]

rule_name = prefix+'_'+spec_string

# To see the intermediate output as a rule table:
#WriteRuleTable('hexagonal',total_states,transitions,golly.getdir('rules')+rule_name+'_asTable.table')

HexagonalTransitionsToRuleTree('hexagonal',total_states,transitions,rule_name)

# -- make some icons --

palette=[[0,0,0],[0,155,67],[127,0,255],[128,128,128],[185,184,96],[0,100,255],[196,255,254],
    [254,96,255],[126,125,21],[21,126,125],[255,116,116],[116,255,116],[116,116,255],
    [228,227,0],[28,255,27],[255,27,28],[0,228,227],[227,0,228],[27,28,255],[59,59,59],
    [234,195,176],[175,196,255],[171,194,68],[194,68,171],[68,171,194],[72,184,71],[184,71,72],
    [71,72,184],[169,255,188],[252,179,63],[63,252,179],[179,63,252],[80,9,0],[0,80,9],[9,0,80],
    [255,175,250],[199,134,213],[115,100,95],[188,163,0],[0,188,163],[163,0,188],[203,73,0],
    [0,203,73],[73,0,203],[94,189,0],[189,0,94]]

width = 31*(total_states-1)
height = 53
pixels = [[(0,0,0) for x in range(width)] for y in range(height)]

huge = [[0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
        [0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
        [0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
        [0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
        [0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
        [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
        [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0],
        [0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0],
        [0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0],
        [0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0],
        [0,0,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,1,1,1,1,1,1,1,0,0,0,0,0,0,0],
        [0,0,0,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,1,1,1,1,1,0,0,0,0,0,0,0],
        [0,0,0,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,1,1,1,1,1,0,0,0,0,0,0],
        [0,0,0,0,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,1,1,1,1,1,0,0,0,0,0,0],
        [0,0,0,0,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,1,0,0,0,0,0],
        [0,0,0,0,0,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,1,0,0,0,0,0],
        [0,0,0,0,0,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,0,0,0,0],
        [0,0,0,0,0,0,1,1,1,1,1,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,0,0,0,0],
        [0,0,0,0,0,0,1,1,1,1,1,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,0,0,0],
        [0,0,0,0,0,0,0,1,1,1,1,1,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,0,0,0],
        [0,0,0,0,0,0,0,1,1,1,1,1,1,1,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,0,0],
        [0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0],
        [0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0],
        [0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0],
        [0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
        [0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
        [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0],
        [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0],
        [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0],
        [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0],
        [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0]]
big = [[0,0,0,1,1,0,0,0,0,0,0,0,0,0,0],
       [0,0,1,1,1,1,1,0,0,0,0,0,0,0,0],
       [0,1,1,1,1,1,1,1,1,0,0,0,0,0,0],
       [1,1,1,1,1,1,1,1,1,1,1,0,0,0,0],
       [1,1,1,1,1,1,2,2,2,1,1,1,0,0,0],
       [0,1,1,1,1,2,2,2,2,2,1,1,0,0,0],
       [0,1,1,1,2,2,2,2,2,2,2,1,1,0,0],
       [0,0,1,1,2,2,2,2,2,2,2,1,1,0,0],
       [0,0,1,1,2,2,2,2,2,2,2,1,1,1,0],
       [0,0,0,1,1,2,2,2,2,2,1,1,1,1,0],
       [0,0,0,1,1,1,2,2,2,1,1,1,1,1,1],
       [0,0,0,0,1,1,1,1,1,1,1,1,1,1,1],
       [0,0,0,0,0,0,1,1,1,1,1,1,1,1,0],
       [0,0,0,0,0,0,0,0,1,1,1,1,1,0,0],
       [0,0,0,0,0,0,0,0,0,0,1,1,0,0,0]]
small = [[0,1,1,0,0,0,0],
         [1,1,1,1,1,0,0],
         [1,1,2,2,2,1,0],
         [0,1,2,2,2,1,0],
         [0,1,2,2,2,1,1],
         [0,0,1,1,1,1,1],
         [0,0,0,0,1,1,0]]

for color in range(n_colors):
    bg = palette[color]
    for row in range(31):
        for column in range(31):
            pixels[row][(color-1)*31+column] = [palette[0],bg,bg][huge[row][column]]
    for row in range(15):
        for column in range(15):
            pixels[31+row][(color-1)*31+column] = [palette[0],bg,bg][big[row][column]]
    for row in range(7):
        for column in range(7):
            pixels[46+row][(color-1)*31+column] = [palette[0],bg,bg][small[row][column]]
    for state in range(n_states):
        fg = palette[n_colors+state]
        # draw the 31x31 icon
        for row in range(31):
            for column in range(31):
                pixels[row][(encode(color,state)-1)*31+column] = [palette[0],bg,fg][huge[row][column]]
        # draw the 15x15 icon
        for row in range(15):
            for column in range(15):
                pixels[31+row][(encode(color,state)-1)*31+column] = [palette[0],bg,fg][big[row][column]]
        # draw the 7x7 icon
        for row in range(7):
            for column in range(7):
                pixels[46+row][(encode(color,state)-1)*31+column] = [palette[0],bg,fg][small[row][column]]

# use rule_name.tree and icon info to create rule_name.rule
ConvertTreeToRule(rule_name, total_states, pixels)

# -- select the new rule --

golly.new(rule_name+'-demo.rle')
golly.setalgo('RuleLoader')
golly.setrule(rule_name)
golly.setcell(0,0,encode(0,0)) # start with a single turmite
golly.show('Created '+rule_name+'.rule and selected that rule.')
