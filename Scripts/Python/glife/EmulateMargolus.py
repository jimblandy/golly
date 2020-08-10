# We emulate Margolus and related partitioning neighborhoods by making a 2-layer CA, with a
# background layer (0,1) that alternates a sort-of-checker pattern, and the normal states on top.
#
# For the Margolus neighborhood there is only one 'phase'; one background pattern, that alternates
# between top-left and bottom-right of a 2x2 region.
#
# For the square4_* neighborhoods we need two phases since the background shifts in different
# directions to make a figure-8 or cyclic pattern.
#
# By looking at the nearby background states, each cell can work out where in the partition it is
# and what the current phase is. It can therefore update the cell states layer correctly, and also
# update the background layer for the next phase.

import golly
import os
from glife.RuleTree import *

# state s (0,N-1) on background state b (0,1) becomes 1+b+2s with 0 = off-grid
def encode(s,b): # (s is a list)
    return [ 1+b+2*se for se in s ]

# The BackgroundInputs array tells us where we are in the block:
#
# Phase 1:   0  1   (the lone 0 is the top-left of the block to be updated)
#            1  1
#
# Phase 2:   1  0   (the lone 1 is the top-left of the block to be updated)
#            0  0
#
# (imagine the blocks stacked together to see why this gives the values below)
#
BackgroundInputs = [ # each C,S,E,W,N,SE,SW,NE,NW
    [0,1,1,1,1,1,1,1,1], # this pattern of background states means that this is top-left, phase 1
    [1,1,0,0,1,1,1,1,1], # top-right, phase 1
    [1,0,1,1,0,1,1,1,1], # bottom-left, phase 1
    [1,1,1,1,1,0,0,0,0], # bottom-right, phase 1
    [1,0,0,0,0,0,0,0,0], # top-left, phase 2
    [0,0,1,1,0,0,0,0,0], # top-right, phase 2
    [0,1,0,0,1,0,0,0,0], # bottom-left, phase 2
    [0,0,0,0,0,1,1,1,1], # bottom-right, phase 2
]

# The BackgroundOutputs array tells us how the background pattern changes:
#
# Margolus:     0  1   become   1  1   (block moves diagonally)
#               1  1            1  0
#
# square4_figure8v:     0  1   becomes   0  0   (block moves diagonally)
#                       1  1             0  1
#
#                       1  0   becomes   1  0   (block moves horizontally)
#                       0  0             1  1
#
# square4_figure8h:     0  1   becomes   0  0   (block moves diagonally)
#                       1  1             0  1
#
#                       1  0   becomes   1  1   (block moves vertically)
#                       0  0             0  1
#
# square4_cyclic  :     0  1   becomes   0  0   (block moves vertically)
#                       1  1             1  0
#
#                       1  0   becomes   1  0   (block moves horizontally)
#                       0  0             1  1
#
# (Following Tim Tyler: http://www.cell-auto.com/neighbourhood/square4/index.html)
#
BackgroundOutputs = { # (top-left, top-right, bottom-left, bottom-right)*2 (or *1 for Margolus)
    "Margolus":        [1,1,1,0], # phase 1 -> phase 1
    "square4_figure8v":[0,0,0,1,1,0,1,1], # phase 1 <--> phase 2
    "square4_figure8h":[0,0,0,1,1,1,0,1], # phase 1 <--> phase 2
    "square4_cyclic":  [0,0,1,0,1,0,1,1], # phase 1 <--> phase 2
}

# The ForegroundInputs array tells us, for each of the 4 positions (0=top-left, 1=top-right,
# 2=bottom-left, 3=bottom-right), where the other positions are found in our Moore neighborhood
# e.g. if we're 0 (top-left) then the first row tells us that to our South is position 2 (bottom-left)
#      and to our South-East is position 3 (bottom-right).
# (N.B. these values don't depend on the phase or the background pattern)
ForegroundInputs = [
    [0,2,1,-1,-1,3,-1,-1,-1], # what surrounds this entry?
    [1,3,-1,0,-1,-1,2,-1,-1], # (C,S,E,W,N,SE,SW,NE,NW)
    [2,-1,3,-1,0,-1,-1,1,-1], # (-1 = anything)
    [3,-1,-1,2,1,-1,-1,-1,0]  # (0,1,2,3 = index of Margolus input)
]

def EmulateMargolus(neighborhood,n_states,transitions,input_filename):
    '''Emulate a Margolus or square4_* neighborhood rule table with a Moore neighborhood rule tree.'''
    rule_name = os.path.splitext(os.path.split(input_filename)[1])[0]
    total_states = 1+2*n_states
    tree = RuleTree(total_states,8)
    # now work through the transitions
    for tr in transitions:
        for iOutput,background_output in enumerate(BackgroundOutputs[neighborhood]):
            bg_inputs = BackgroundInputs[iOutput]
            iEntry = iOutput % 4  # (0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right)
            rule_inputs = []
            for i in range(9):
                if ForegroundInputs[iEntry][i]==-1:
                    rule_inputs.append(encode( list(range(n_states)), bg_inputs[i] ) + [0]) # wildcard
                else:
                    rule_inputs.append(encode( tr[ForegroundInputs[iEntry][i]], bg_inputs[i] ))
            tree.add_rule( rule_inputs, encode( tr[iEntry+4], background_output )[0] )
    # supply default behaviour: background still changes even if the state doesn't
    for iState in range(n_states):
        for iOutput,background_output in enumerate(BackgroundOutputs[neighborhood]):
            bg_inputs = BackgroundInputs[iOutput]
            tree.add_rule( [ encode( [iState], bg_inputs[0] ) ] +
                           [ encode( list(range(n_states)), bg_inputs[i] )+[0] for i in range(1,9) ], # wildcard
                           encode( [iState], background_output )[0] )

    # output the rule tree
    golly.show("Compressing rule tree and saving to file...")
    tree.write(golly.getdir('rules') + rule_name + '.tree')

    # also save a .colors file
    golly.show("Generating colors...")

    # read rule_name+'.colors' file if it exists
    cfn = os.path.split(input_filename)[0] + '/' + os.path.splitext(os.path.split(input_filename)[1])[0] + ".colors"
    try:
        cf = open(cfn,'r')
    except IOError:
        # use Golly's default random colours
        random_colors=[[90,90,90],[0,255,127],[127,0,255],[148,148,148],[128,255,0],[255,0,128],[0,128,255],[1,159,0],
            [159,0,1],[255,254,96],[0,1,159],[96,255,254],[254,96,255],[126,125,21],[21,126,125],[125,21,126],
            [255,116,116],[116,255,116],[116,116,255],[228,227,0],[28,255,27],[255,27,28],[0,228,227],
            [227,0,228],[27,28,255],[59,59,59],[234,195,176],[175,196,255],[171,194,68],[194,68,171],
            [68,171,194],[72,184,71],[184,71,72],[71,72,184],[169,255,188],[252,179,63],[63,252,179],
            [179,63,252],[80,9,0],[0,80,9],[9,0,80],[255,175,250],[199,134,213],[115,100,95],[188,163,0],
            [0,188,163],[163,0,188],[203,73,0],[0,203,73],[73,0,203],[94,189,0],[189,0,94]]
        colors = dict(list(zip(list(range(len(random_colors))),random_colors)))
    else:
        # read from the .colors file
        colors = {}
        for line in cf:
            if line[0:5]=='color':
                entries = list(map(int,line[5:].replace('=',' ').replace('\n',' ').split()))
                if len(entries)<4:
                    continue # too few entries, ignore
                colors.update({entries[0]:[entries[1],entries[2],entries[3]]})
        # (TODO: support gradients in .colors)

    # provide a deep blue background if none provided
    if not 0 in colors:
        colors.update({0:[0,0,120]})

    c = open(golly.getdir('rules')+rule_name+".colors",'w')
    for col in list(colors.items())[:n_states]:
        c.write('color='+str(col[0]*2+1)+' '+' '.join(map(str,col[1]))+'\n')
        c.write('color='+str(col[0]*2+2)+' '+' '.join([ str(int(x*0.7)) for x in col[1] ])+'\n')  # (darken slightly)
    c.flush()
    c.close()

    # use rule_name.tree and rule_name.colors to create rule_name.rule (no icon info)
    ConvertTreeToRule(rule_name, total_states, [])
    return rule_name
