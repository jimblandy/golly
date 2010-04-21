import golly
import os
from glife.RuleTree import *

# state s (0,N-1) on background state b (0,1) becomes 1+b+2s
def encode(s,b):
    return 1+b+2*s
    
TransitionInputs = [ # each C,S,E,W,N,SE,SW,NE,NW
    [1,0,1,1,0,1,1,1,1], # top-left, phase 1
    [1,1,1,1,1,0,0,0,0], # top-right, phase 1
    [0,1,1,1,1,1,1,1,1], # bottom-left, phase 1
    [1,1,0,0,1,1,1,1,1], # bottom-right, phase 1
    [1,0,0,0,0,0,0,0,0], # top-left, phase 2
    [0,0,1,1,0,0,0,0,0], # top-right, phase 2
    [0,1,0,0,1,0,0,0,0], # bottom-left, phase 2
    [0,0,0,0,0,1,1,1,1], # bottom-right, phase 2
]

TransitionOutputs = {
    "Margolus":        [1,0,1,1], # phase 1 -> phase 1
    "square4_figure8v":[0,0,0,1,1,1,1,0], # phase 1 <--> phase 2
    "square4_figure8h":[0,1,0,0,1,1,0,1], # phase 1 <--> phase 2 
    "square4_cyclic":  [0,0,0,1,1,1,0,1], # phase 1 <--> phase 2
}
    
Locale = [
    [0,2,1,-1,-1,3,-1,-1,-1], # what surrounds this entry?
    [1,3,-1,0,-1,-1,2,-1,-1], # (C,S,E,W,N,SE,SW,NE,NW)
    [2,-1,3,-1,0,-1,-1,1,-1], # (-1 = anything)
    [3,-1,-1,2,1,-1,-1,-1,0]  # (0,1,2,3 = index of Margolus input)
]

def EmulateMargolus(neighborhood,n_states,rt_transitions,input_filename,rule_name):
    m_transitions = [[e[0] for e in t] for t in rt_transitions]
    total_states = 1+2*n_states
    tree = RuleTree(total_states,8)
    any_state = range(total_states)
    # now work through the Margolus transitions
    for mt in m_transitions:
        for iOutput,background_output in enumerate(TransitionOutputs[neighborhood]):
            bg_inputs = TransitionInputs[iOutput]
            iEntry = iOutput % 4
            transition_inputs = [ [ encode( s, bg_inputs[i] ) for s in range(n_states) ]+[0] \
                                  if Locale[iEntry][i]==-1 else \
                                  [ encode( mt[Locale[iEntry][i]], bg_inputs[i] ) ] \
                                  for i in range(9) ]
            transition_output = encode(mt[iEntry+4],background_output)
            tree.add_rule(transition_inputs,transition_output)
    # supply default behaviour: background still changes even if state doesn't
    for iState in range(n_states):
        for iOutput,background_output in enumerate(TransitionOutputs[neighborhood]):
            transition_inputs = [[encode(iState,TransitionInputs[iOutput][0])]]
            transition_inputs += [ [encode(s,TransitionInputs[iOutput][i]) for s in range(n_states)]+[0] \
                                   for i in range(1,9) ]
            transition_output = encode(iState,background_output)
            tree.add_rule(transition_inputs,transition_output)
    # output the rule tree
    golly.show("Compressing rule tree and saving to file...")
    tree.write(golly.getdir('rules') + rule_name + '.tree')
    
    # also save a .colors file
    golly.show("Generating colors...")

    # read rule_name+'.colors' file if it exists
    force_background = False
    background_color = 0,0,0
    cfn = os.path.split(input_filename)[0]+"/"+os.path.splitext(os.path.split(input_filename)[1])[0]+".colors"
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
        colors = {0:[0,0,0]} # background is black
        for line in cf:
            if line[0:6]=='color ':
                entries = map(int,line[6:].replace('=',' ').replace('\n',' ').split())
                if len(entries)<4:
                    continue # too few entries, ignore
                if entries[0]==0:
                    force_background = True
                    background_color = [entries[1],entries[2],entries[3]]
                else:
                    colors.update({entries[0]:[entries[1],entries[2],entries[3]]})
        # (we don't support gradients in .colors)

    # TODO: output a suitable .colors file

