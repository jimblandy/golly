# generator for Langton's Ant rules
# inspired by Aldoaldoz: http://www.youtube.com/watch?v=1X-gtr4pEBU
# contact: tim.hutton@gmail.com

import golly
import random
from glife.RuleTree import *

opposite_dirs=[2,3,0,1] # index of opposite direction

# encoding:
# (0-n_colors: empty square)
def encode(c,s,d):
    # turmite on color c in state s facing direction d
    return n_colors + n_dirs*(n_states*c+s) + d

prefix = 'LangtonsAnt'
# (We choose a different name to the inbuilt Langtons-Ant rule to avoid
#  name collision between the rules we output and the existing icons.)

spec = golly.getstring(
'''This script will create a Langton's Ant CA for a given string of actions.

The string specifies which way to turn when standing on a square of each state.

Examples: RL (Langton's Ant), RLR (Chaos), LLRR (Cardioid), LRRL (structure)

Permitted moves: 'L':Left, 'R':Right, 'U':U-turn, 'N':No turn

Enter string:''', 'RL', 'Enter string:')

n_colors = len(spec)

d={'R':'2','L':'8','U':'4','N':'1'} # 1=noturn, 2=right, 4=u-turn, 8=left
turmite_spec = "{{"+','.join(['{'+str((i+1)%n_colors)+','+d[spec[i]]+',0}' for i in range(n_colors)])+"}}"
rule_name = prefix+'_'+spec
action_table = eval(turmite_spec.replace('}',']').replace('{','['))
n_states = len(action_table)
n_dirs=4
# (N.B. The terminology 'state' here refers to the internal state of the finite
#       state machine that each Turmite is using, not the contents of each Golly
#       cell. We use the term 'color' to denote the symbol on the 2D 'tape'. The
#       actual 'Golly state' in this emulation of Turmites is given by the
#       "encoding" section below.)

total_states = n_colors+n_colors*n_states*n_dirs

# problem if we try to export more than 255 states
if total_states>255:
    golly.warn("Number of states required exceeds Golly's limit of 255\n\nMaximum 51 turns allowed.")
    golly.exit()

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

tree.write(golly.getdir('rules')+rule_name+'.tree')

# Create some multi-colour icons so we can see what the ant is doing
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

# now we can switch to the new rule
golly.new(rule_name+' demo')
golly.setalgo('RuleLoader')
golly.setrule(rule_name)
golly.setcell(0,0,n_colors+3) # start with an ant facing west
golly.show('Created '+rule_name+'.rule and selected that rule.')
