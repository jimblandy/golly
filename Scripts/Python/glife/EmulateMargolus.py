# We emulate Margolus and related partitioning neighborhoods by making a 2-layer CA, with a 
# background layer (0,1) that alternates a checker pattern, and the normal states on top.
#
# For the Margolus neighborhood there is only one 'phase'; one background pattern, that alternates
# between top-left and bottom-right of a 2x2 region.
#
# For the square4_* neighborhoods we need two phases since the background shifts in different 
# directions to make a figure-8 or cyclic pattern.
#
# By looking at the nearby background states, each cell can work out where in the partition it is
# and what the current phase is. It can therefore update the cell states layer correctly, and also
# updates the background layer for the next pattern.

import golly
import os
from glife.RuleTree import *

# state s (0,N-1) on background state b (0,1) becomes 1+b+2s with 0 = off-grid
def encode(s,b): # (s is a list)
    return [ 1+b+2*se for se in s ]
    
BackgroundInputs = [ # each C,S,E,W,N,SE,SW,NE,NW
    [1,0,1,1,0,1,1,1,1], # top-left, phase 1
    [1,1,1,1,1,0,0,0,0], # top-right, phase 1
    [0,1,1,1,1,1,1,1,1], # bottom-left, phase 1
    [1,1,0,0,1,1,1,1,1], # bottom-right, phase 1
    [1,0,0,0,0,0,0,0,0], # top-left, phase 2
    [0,0,1,1,0,0,0,0,0], # top-right, phase 2
    [0,1,0,0,1,0,0,0,0], # bottom-left, phase 2
    [0,0,0,0,0,1,1,1,1], # bottom-right, phase 2
]

BackgroundOutputs = {
    "Margolus":        [1,0,1,1], # phase 1 -> phase 1
    "square4_figure8v":[0,0,0,1,1,1,1,0], # phase 1 <--> phase 2
    "square4_figure8h":[0,1,0,0,1,1,0,1], # phase 1 <--> phase 2 
    "square4_cyclic":  [0,0,0,1,1,1,0,1], # phase 1 <--> phase 2
}
    
ForegroundInputs = [
    [0,2,1,-1,-1,3,-1,-1,-1], # what surrounds this entry?
    [1,3,-1,0,-1,-1,2,-1,-1], # (C,S,E,W,N,SE,SW,NE,NW)
    [2,-1,3,-1,0,-1,-1,1,-1], # (-1 = anything)
    [3,-1,-1,2,1,-1,-1,-1,0]  # (0,1,2,3 = index of Margolus input)
]

def EmulateMargolus(neighborhood,n_states,transitions,input_filename):
    '''Emulate a Margolus or square4_* neighborhood rule table with a Moore neighborhood rule tree.'''
    rule_name = os.path.splitext(os.path.split(input_filename)[1])[0]+'_emulated'
    total_states = 1+2*n_states
    tree = RuleTree(total_states,8)
    # now work through the transitions
    for t in transitions:
        for iOutput,background_output in enumerate(BackgroundOutputs[neighborhood]):
            bg_inputs = BackgroundInputs[iOutput]
            iEntry = iOutput % 4
            ### AKT: this code causes syntax error in Python 2.3:
            ### tree.add_rule( [ encode( range(n_states), bg_inputs[i] ) + [0] \
            ###                  if ForegroundInputs[iEntry][i]==-1 \
            ###                  else encode( t[ForegroundInputs[iEntry][i]], bg_inputs[i] ) \
            ###                  for i in range(9) ],
            ###                encode( t[iEntry+4], background_output )[0] )
            temp = []
            for i in range(9):
                if ForegroundInputs[iEntry][i]==-1:
                    temp.append(encode( range(n_states), bg_inputs[i] ) + [0])
                else:
                    temp.append(encode( t[ForegroundInputs[iEntry][i]], bg_inputs[i] ))
            tree.add_rule( temp, encode( t[iEntry+4], background_output )[0] )
    # supply default behaviour: background still changes even if state doesn't
    for iState in range(n_states):
        for iOutput,background_output in enumerate(BackgroundOutputs[neighborhood]):
            bg_inputs = BackgroundInputs[iOutput]
            tree.add_rule( [ encode( [iState], bg_inputs[0] ) ] +
                           [ encode( range(n_states), bg_inputs[i] )+[0] for i in range(1,9) ],
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
        random_colors=[[0,0,0],[0,255,127],[127,0,255],[148,148,148],[128,255,0],[255,0,128],[0,128,255],[1,159,0],
            [159,0,1],[255,254,96],[0,1,159],[96,255,254],[254,96,255],[126,125,21],[21,126,125],[125,21,126],
            [255,116,116],[116,255,116],[116,116,255],[228,227,0],[28,255,27],[255,27,28],[0,228,227],
            [227,0,228],[27,28,255],[59,59,59],[234,195,176],[175,196,255],[171,194,68],[194,68,171],
            [68,171,194],[72,184,71],[184,71,72],[71,72,184],[169,255,188],[252,179,63],[63,252,179],
            [179,63,252],[80,9,0],[0,80,9],[9,0,80],[255,175,250],[199,134,213],[115,100,95],[188,163,0],
            [0,188,163],[163,0,188],[203,73,0],[0,203,73],[73,0,203],[94,189,0],[189,0,94]]
        colors = dict(zip(range(len(random_colors)),random_colors))
    else:
        # read from the .colors file
        colors = {}
        for line in cf:
            if line[0:5]=='color':
                entries = map(int,line[5:].replace('=',' ').replace('\n',' ').split())
                if len(entries)<4:
                    continue # too few entries, ignore
                colors.update({entries[0]:[entries[1],entries[2],entries[3]]})
        # (TODO: support gradients in .colors)
        
    # provide a deep blue background if none provided
    if not 0 in colors:
        colors.update({0:[0,0,120]})

    c = open(golly.getdir('rules')+rule_name+".colors",'w')
    for col in colors.items():
        c.write('color='+str(col[0]*2+1)+' '+' '.join(map(str,col[1]))+'\n')
        c.write('color='+str(col[0]*2+2)+' '+' '.join([ str(int(x*0.7)) for x in col[1] ])+'\n')  # (darken slightly)
    c.flush()
    c.close()
    
    return rule_name
