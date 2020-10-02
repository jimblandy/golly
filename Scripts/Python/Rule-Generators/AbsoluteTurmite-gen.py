# Generator for AbsoluteTurmites rules. Also known as 2D Turing machines.
#
# contact: tim.hutton@gmail.com

import golly
import random
from glife.RuleTree import *

dirs=['N','E','S','W']
opposite_dirs = [ 2, 3, 0, 1 ]

prefix = 'AbsoluteTurmite'

# N.B. All 'relative' Turmites (including Langton's ant and the n-Color family)
# can be expressed as Absolute Turmites but require 4n states instead of n.
# e.g. {{{1,'E',1},{0,'W',3}},{{1,'S',2},{0,'N',0}},{{1,'W',3},{0,'E',1}},{{1,'N',0},{0,'S',2}}}
#      is a 4-state 2-color Absolute Turmite that matches Langton's ant
# Likewise all normal 1D Turing machines can be expressed as Absolute Turmites in a
# straightforward fashion.
# e.g. {{{1,'E',1},{1,'W',1}},{{1,'W',0},{1,'',0}}}
#      is a 2-state 2-color busy beaver
# In both cases the opposite transform is usually not possible. Thus Absolute Turmites
# are a deeper generalization.

# http://bytes.com/topic/python/answers/25176-list-subsets
get_subsets = lambda items: [[x for (pos,x) in zip(list(range(len(items))), items) if (2**pos) & switches] for switches in range(2**len(items))]

example_spec = "{{{1,'E',1},{1,'W',1}},{{1,'W',0},{1,'',0}}}"

# Generate a random rule, filtering out the most boring
import random
ns = 3
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
            dir_to_move = dirs[random.randint(0,3)]
            example_spec += '{' + str(new_color) + ",'" + dir_to_move + "'," + str(new_state) + '}'
        example_spec += '}'
    example_spec += '}'
    # is rule acceptable?
    is_rule_acceptable = True
    action_table = eval(example_spec.replace('{','[').replace('}',']'))
    # does Turmite change at least one color?
    changes_one = False
    for state in range(ns):
        for color in range(nc):
            if not action_table[state][color][0] == color:
                changes_one = True
    if not changes_one:
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
    # 1-move lookahead: will turmite zip off when placed on 0?
    for state in range(ns):
        if action_table[state][0][2] == state:
            is_rule_acceptable = False
    # turmite must write each colour at least once
    for color in range(nc):
        if not "{"+str(color)+"," in example_spec:
            is_rule_acceptable = False
    # does turmite move in all directions?
    for dir in dirs:
        if not "'"+dir+"'" in example_spec:
            is_rule_acceptable = False

    if is_rule_acceptable:
        break

spec = golly.getstring(
'''This script will create an AbsoluteTurmite CA for a given specification.

Enter a specification string: a curly-bracketed table of n_states rows
and n_colors columns, where each entry is a triple of integers.
The elements of each triple are:
a: the new color of the square
b: the direction(s) for the turmite to move: 'NESW'
c: the new internal state of the turmite

Example:
{{{1,'E',1},{1,'W',1}},{{1,'W',0},{1,'',0}}}
(example pattern #1)
Has 2 states and 2 colors. The triple {1,'W',0} says:
1. set the color of the square to 1
2. move West
3. adopt state 0 and move forward one square
This is a 1D busy beaver

Enter specification string:
(default is a random example)''', example_spec, 'Enter Absolute Turmite specification:')

# convert the specification string into action_table[state][color]
# (convert Mathematica code to Python and run eval)
action_table = eval(spec.replace('{','[').replace('}',']'))
n_states = len(action_table)
n_colors = len(action_table[0])
# (N.B. The terminology 'state' here refers to the internal state of the finite
#       state machine that each turmite is using, not the contents of each Golly
#       cell. We use the term 'color' to denote the symbol on the 2D 'tape'. The
#       actual 'Golly state' in this emulation of turmites is given by the
#       "encoding" section below.)
n_dirs = 4

# TODO: check table is full and valid

total_states = n_colors + n_colors*n_states

# problem if we try to export more than 255 states
if total_states > 255:
    golly.warn("Number of states required exceeds Golly's limit of 255.")
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
spec_string = ''.join(map(str,list(map(str,flatten(action_table)))))
# (ambiguous but we have to try something)
rule_name = prefix+'_'+spec_string

remap = [2,1,3,0] # N,E,S,W -> S,E,W,N

not_arriving_from_here = [list(range(n_colors)) for i in range(n_dirs)] # (we're going to modify them)
for color in range(n_colors):
    for state in range(n_states):
        moveset = action_table[state][color][1]
        for iMove,move in enumerate(dirs):
            if not move in moveset:
                not_arriving_from_here[opposite_dirs[iMove]] += [encode(color,state)]

# What states leave output_color behind?
leaving_color_behind = {}
for output_color in range(n_colors):
    leaving_color_behind[output_color] = [output_color] # (no turmite present)
    for state in range(n_states):
        for color in range(n_colors):
            if action_table[state][color][0]==output_color:
                leaving_color_behind[output_color] += [encode(color,state)]

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
            moveset = action_table[state][color][1]
            if dirs[opposite_dirs[dir]] in moveset: # e.g. is there one to the S about to move N
                inputs += [encode(color,state)]
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
            transition_output = encode(central_color,s)
            tree.add_rule( transition_inputs, transition_output )

# default: square is left with no turmite present
for output_color,inputs in list(leaving_color_behind.items()):
    tree.add_rule([inputs]+[list(range(total_states))]*4,output_color)

tree.write(golly.getdir('rules')+rule_name+'.tree')

# Write some colour icons so we can see what the turmite is doing
# A simple ball drawing, with specular highlights (2) and anti-aliasing (3):
icon31x31 = [[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0],
             [0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0],
             [0,0,0,0,3,1,1,1,1,1,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0],
             [0,0,0,0,1,1,1,1,1,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0],
             [0,0,0,3,1,1,1,1,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0],
             [0,0,0,1,1,1,1,1,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0],
             [0,0,0,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0],
             [0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0],
             [0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0],
             [0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0],
             [0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0],
             [0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0],
             [0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0],
             [0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0],
             [0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0],
             [0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0],
             [0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0],
             [0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0],
             [0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]]
icon15x15 = [[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
             [0,0,0,0,0,0,3,3,3,0,0,0,0,0,0],
             [0,0,0,0,3,1,1,1,1,1,3,0,0,0,0],
             [0,0,0,3,1,1,1,1,1,1,1,3,0,0,0],
             [0,0,3,1,1,2,1,1,1,1,1,1,3,0,0],
             [0,0,1,1,2,1,1,1,1,1,1,1,1,0,0],
             [0,3,1,1,2,1,1,1,1,1,1,1,1,3,0],
             [0,3,1,1,1,1,1,1,1,1,1,1,1,3,0],
             [0,3,1,1,1,1,1,1,1,1,1,1,1,3,0],
             [0,0,1,1,1,1,1,1,1,1,1,1,1,0,0],
             [0,0,3,1,1,1,1,1,1,1,1,1,3,0,0],
             [0,0,0,3,1,1,1,1,1,1,1,3,0,0,0],
             [0,0,0,0,3,1,1,1,1,1,3,0,0,0,0],
             [0,0,0,0,0,0,3,3,3,0,0,0,0,0,0],
             [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]]
icon7x7 = [ [0,0,3,3,3,0,0],
            [0,3,1,1,1,3,0],
            [3,1,2,1,1,1,3],
            [3,1,1,1,1,1,3],
            [3,1,1,1,1,1,3],
            [0,3,1,1,1,3,0],
            [0,0,3,3,3,0,0] ]
palette=[[0,0,0],[0,155,67],[127,0,255],[128,128,128],[185,184,96],[0,100,255],[196,255,254],
    [254,96,255],[126,125,21],[21,126,125],[255,116,116],[116,255,116],[116,116,255],
    [228,227,0],[28,255,27],[255,27,28],[0,228,227],[227,0,228],[27,28,255],[59,59,59],
    [234,195,176],[175,196,255],[171,194,68],[194,68,171],[68,171,194],[72,184,71],[184,71,72],
    [71,72,184],[169,255,188],[252,179,63],[63,252,179],[179,63,252],[80,9,0],[0,80,9],[9,0,80],
    [255,175,250],[199,134,213],[115,100,95],[188,163,0],[0,188,163],[163,0,188],[203,73,0],
    [0,203,73],[73,0,203],[94,189,0],[189,0,94]]
highlight=(255,255,255)
pixels = [[palette[0] for column in list(range(total_states))*31] for row in range(53)]
for state in range(n_states):
    for color in range(n_colors):
        bg_col = palette[color]
        fg_col = palette[state+n_colors]
        mid = [(f+b)//2 for f,b in zip(fg_col,bg_col)]
        for row in range(31):
            for column in range(31):
                pixels[row][(encode(color,state)-1)*31+column] = [bg_col,fg_col,highlight,mid][icon31x31[row][column]]
        for row in range(15):
            for column in range(15):
                pixels[31+row][(encode(color,state)-1)*31+column] = [bg_col,fg_col,highlight,mid][icon15x15[row][column]]
        for row in range(7):
            for column in range(7):
                pixels[46+row][(encode(color,state)-1)*31+column] = [bg_col,fg_col,highlight,mid][icon7x7[row][column]]
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

# now we can switch to the new rule
golly.new(rule_name+'-demo.rle')
golly.setalgo('RuleLoader')
golly.setrule(rule_name)
golly.setcell(0,0,encode(0,0)) # start with a single turmite
golly.show('Created '+rule_name+'.rule and selected that rule.')
